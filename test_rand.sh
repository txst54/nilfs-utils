#!/bin/bash

# Configuration
MOUNT_POINT="/mnt/nilfs"   # CHANGE THIS to your actual mount point
DIR_NAME="lfs_simulation"
FILE_SIZE_KB=32
NUM_FILES=1024            # Calculated for 75% of 512MB
TARGET_DIR="$MOUNT_POINT/$DIR_NAME"

# Ensure mount point exists
if [ ! -d "$MOUNT_POINT" ]; then
    echo "Error: Mount point $MOUNT_POINT does not exist."
    exit 1
fi

# Create simulation directory
mkdir -p "$TARGET_DIR"

echo "=============================================="
echo "Phase 1: Filling Disk to 75% Capacity"
echo "Creating $NUM_FILES files of ${FILE_SIZE_KB}KB..."
echo "=============================================="

# Loop 1: Sequential Fill
# We use /dev/urandom to ensure data isn't compressed or treated as sparse
for i in $(seq 1 $NUM_FILES); do
    # Create file_1, file_2, etc.
    dd if=/dev/urandom of="$TARGET_DIR/file_$i" bs=1K count=$FILE_SIZE_KB status=none

    # Print progress every 1000 files
    if (( i % 1000 == 0 )); then
        echo "Created $i / $NUM_FILES files..."
    fi
done

# Force data to disk so we have a clean '100% utilized' starting state for these segments
sync
echo "Fill complete. Disk should be ~75% full."
echo "Sleeping for 5 seconds to let cleanerd settle..."
sleep 5

echo "=============================================="
echo "Phase 2: Random Overwrites (The Churn)"
echo "Press [CTRL+C] to stop."
echo "=============================================="

# Loop 2: Random Overwrite
# This simulates the "Uniform" access pattern from the paper
count=0
while true; do
    # 1. Pick a random file ID between 1 and NUM_FILES
    RAND_ID=$((1 + RANDOM % NUM_FILES))

    # 2. Overwrite that file with new random data
    # In NILFS, this writes to a NEW segment and invalidates the OLD block
    dd if=/dev/urandom of="$TARGET_DIR/file_$RAND_ID" bs=1K count=$FILE_SIZE_KB status=none conv=notrunc

    ((count++))

    # Every 100 writes, sync to force a checkpoint/segment flush
    # This ensures the cleaner sees the changes
    if (( count % 50 == 0 )); then
        sync
        nilfs-clean -p 0
    fi
done

