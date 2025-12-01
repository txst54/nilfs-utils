#!/bin/bash

# =============================================
# Configuration
# =============================================
MOUNT_POINT="/mnt/nilfs"
DIR_NAME="lfs_hot_cold"
FILE_SIZE_KB=32
NUM_FILES=4096
HOT_RATIO=90
HOT_RANGE=512
TARGET_DIR="$MOUNT_POINT/$DIR_NAME"
NUM_WRITES=30000        # default

# =============================================
# Parse Arguments
# =============================================
usage() {
    echo "Usage: $0 [-w NUM_WRITES]"
    echo "  -w NUM_WRITES   Number of overwrite operations (default: 30000)"
    exit 1
}

while getopts "w:" opt; do
    case $opt in
        w)
            NUM_WRITES="$OPTARG"
            ;;
        *)
            usage
            ;;
    esac
done

# Validate NUM_WRITES
if ! [[ "$NUM_WRITES" =~ ^[0-9]+$ ]]; then
    echo "Error: -w must be an integer"
    exit 1
fi

if (( NUM_WRITES <= 0 )); then
    echo "Error: -w must be positive"
    exit 1
fi

# =============================================
# Ensure mount point exists
# =============================================
if [ ! -d "$MOUNT_POINT" ]; then
    echo "Error: Mount point $MOUNT_POINT does not exist."
    exit 1
fi

mkdir -p "$TARGET_DIR"

echo "=============================================="
echo "Phase 1: Initial Fill"
echo "Creating $NUM_FILES files of ${FILE_SIZE_KB}KB..."
echo "=============================================="

for i in $(seq 1 $NUM_FILES); do
    dd if=/dev/urandom of="$TARGET_DIR/file_$i" \
       bs=1K count=$FILE_SIZE_KB status=none

    if (( i % 1000 == 0 )); then
        echo "Created $i / $NUM_FILES files..."
    fi
done

sync
echo "Initial fill done."
sleep 5

echo "=============================================="
echo "Phase 2: Hot–Cold Overwrites"
echo "Hot region: files 1..$HOT_RANGE"
echo "Hot ratio:  $HOT_RATIO%"
echo "Total writes: $NUM_WRITES"
echo "=============================================="

count=0
clean_interval=1000     # keep your old value unless you change it explicitly

CHECK_INTERVAL=200     # check every 200 writes
HIGH_WM=40             # start GC if >= 40 used segments
LOW_WM=30              # stop GC once below 30 used segments

while (( count < NUM_WRITES )); do
    r=$((RANDOM % 100))

    if (( r < HOT_RATIO )); then
        RAND_ID=$((1 + RANDOM % HOT_RANGE))
    else
        RAND_ID=$((1 + RANDOM % NUM_FILES))
    fi

    dd if=/dev/urandom of="$TARGET_DIR/file_$RAND_ID" \
       bs=1K count=$FILE_SIZE_KB status=none conv=notrunc

    ((count++))

    if (( count % CHECK_INTERVAL == 0 )); then
        sync  # force checkpoint, ensures cleaner can act

        # Count in-use segments (segments with NBLOCKS > 0)
        USED_SEGS=$(lssu /dev/sda4 | awk 'NR>1 && $5 > 0 {c++} END {print c+0}')

#        echo "[debug] after $count writes: used segments = $USED_SEGS"

        # High watermark check
        if (( USED_SEGS >= HIGH_WM )); then
            echo "[GC] Trigger: used segments = $USED_SEGS >= $HIGH_WM"

            # Keep cleaning until below LOW_WM
            while (( USED_SEGS >= LOW_WM )); do
                echo "[GC] Running nilfs-clean..."
                nilfs-clean -p 0 /dev/sda4
                sync

                # Recompute used segments after each clean
                USED_SEGS=$(lssu /dev/sda4 | awk 'NR>1 && $5 > 0 {c++} END {print c+0}')
                echo "[GC] After cleaning: used segments = $USED_SEGS"
            done

            echo "[GC] Cleaning complete: used segments < $LOW_WM"
        fi
    fi
done

echo "=============================================="
echo "Workload complete ($count writes)."
echo "=============================================="

