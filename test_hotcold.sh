#!/bin/bash

# Configuration
MOUNT_POINT="/mnt/nilfs"      # CHANGE THIS
DIR_NAME="lfs_hot_cold"
FILE_SIZE_KB=32
NUM_FILES=1024                # same as before
HOT_RATIO=80                  # % of writes that land in hot region
HOT_RANGE=128                 # hot region size (12.5% of 1024)
TARGET_DIR="$MOUNT_POINT/$DIR_NAME"

# Ensure mount point exists
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
    dd if=/dev/urandom of="$TARGET_DIR/file_$i" bs=1K count=$FILE_SIZE_KB status=none

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
echo "Press CTRL+C to stop."
echo "=============================================="

count=0
while true; do
    r=$((RANDOM % 100))    # 0–99

    if (( r < HOT_RATIO )); then
        # Hot write
        RAND_ID=$((1 + RANDOM % HOT_RANGE))
    else
        # Cold write
        RAND_ID=$((1 + RANDOM % NUM_FILES))
    fi

    dd if=/dev/urandom of="$TARGET_DIR/file_$RAND_ID" \
        bs=1K count=$FILE_SIZE_KB status=none conv=notrunc

    ((count++))

    if (( count % 50 == 0 )); then
        sync
        nilfs-clean -p 0
    fi
done

