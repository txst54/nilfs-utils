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
        w) NUM_WRITES="$OPTARG" ;;
        *) usage ;;
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
echo "Hot–Cold Overwrite Workload (NO initial fill)"
echo "Hot region: files 1..$HOT_RANGE"
echo "Hot ratio:  $HOT_RATIO%"
echo "Total writes: $NUM_WRITES"
echo "=============================================="

# =============================================
# GC configuration
# =============================================
CHECK_INTERVAL=200
HIGH_WM=40
LOW_WM=30
MAX_GC_PER_TRIGGER=3

count=0

# =============================================
# Main loop: hot/cold writes
# =============================================
while (( count < NUM_WRITES )); do
    r=$((RANDOM % 100))

    if (( r < HOT_RATIO )); then
        # Hot write (1..HOT_RANGE)
        RAND_ID=$((1 + RANDOM % HOT_RANGE))
    else
        # Cold write (1..NUM_FILES)
        RAND_ID=$((1 + RANDOM % NUM_FILES))
    fi

    FILE="$TARGET_DIR/file_$RAND_ID"

    # Lazily create file if it doesn't exist yet
    if [ ! -f "$FILE" ]; then
        dd if=/dev/urandom of="$FILE" \
            bs=1K count=$FILE_SIZE_KB status=none
    else
        dd if=/dev/urandom of="$FILE" \
            bs=1K count=$FILE_SIZE_KB status=none conv=notrunc
    fi

    ((count++))

    # ---------------------------------------------
    # Periodic GC check
    # ---------------------------------------------
    if (( count % CHECK_INTERVAL == 0 )); then
        sync  # force checkpoint

        USED_SEGS=$(lssu /dev/sda4 | awk 'NR>1 && $5 > 0 {c++} END {print c+0}')

        if (( USED_SEGS >= HIGH_WM )); then
            gc_runs=0
            while (( USED_SEGS >= LOW_WM && gc_runs < MAX_GC_PER_TRIGGER )); do
                nilfs-clean -p 0 /dev/sda4
                sync
                ((gc_runs++))

                USED_SEGS=$(lssu /dev/sda4 | awk 'NR>1 && $5 > 0 {c++} END {print c+0}')
            done
        fi
    fi

done

echo "=============================================="
echo "Workload complete ($count writes)."
echo "=============================================="

