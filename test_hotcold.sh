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

    if (( count % clean_interval == 0 )); then
        sync
        nilfs-clean -p 0
    fi
done

echo "=============================================="
echo "Workload complete ($count writes)."
echo "=============================================="

