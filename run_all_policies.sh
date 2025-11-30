#!/bin/bash

# ================================
# Configuration
# ================================

CLEANER_CONF="/etc/nilfs_cleanerd.conf"
CLEANER_BIN="/users/jeffxu/nilfs-utils/sbin/nilfs_cleanerd"
ANAL_DIR="/users/jeffxu/anal"
DEVICE="/dev/sda4"
MOUNT_POINT="/mnt/nilfs"   # Correct this to your actual mount

# List of policy categories
POLICIES=("timestamp" "greedy" "cost-benefit")

# Workload scripts
WORKLOADS=("test_hotcold.sh")

# Test harness script
TEST_MODEL="./test_model.sh"

# Validate presence of test_model.sh
if [ ! -x "$TEST_MODEL" ]; then
    echo "Error: $TEST_MODEL not found or not executable."
    exit 1
fi

mkdir -p "$ANAL_DIR"

# ================================
# Function: modify cleaner policy
# ================================
set_policy() {
    local policy="$1"

    if [ ! -f "$CLEANER_CONF" ]; then
        echo "Error: cleaner config not found: $CLEANER_CONF"
        exit 1
    fi

    echo "[+] Setting cleaner policy to: $policy"
    sudo sed -Ei 's/^selection_policy.*/selection_policy '"$policy"'/' "$CLEANER_CONF"

}

# ================================
# Function: start cleaner
# ================================
start_cleaner() {
    echo "[+] Killing any existing cleaner daemons"
    sudo pkill -9 -f "$CLEANER_BIN" 2>/dev/null

    echo "[+] Starting cleaner daemon"
    sudo "$CLEANER_BIN" -p 0 -c "$CLEANER_CONF" "$DEVICE" "$MOUNT_POINT" >/dev/null 2>&1 &

    # Give the daemon time to fork twice and detach
    sleep 0.3

    # Get REAL daemon PID (with PPID=1)
    CLEANER_PID=$(pgrep -f "$CLEANER_BIN.*$DEVICE" | head -n1)
    echo "[+] Cleaner PID = $CLEANER_PID"
}

# ================================
# Function: stop cleaner
# ================================
stop_cleaner() {
    echo "[+] Killing cleaner daemons"
    sudo pkill -9 -f "$CLEANER_BIN" 2>/dev/null
}

# ================================
# Main loop
# ================================
for policy in "${POLICIES[@]}"; do
    echo "==============================================="
    echo "=== Testing cleaner policy: $policy"
    echo "==============================================="

    # 1. Modify configuration
    set_policy "$policy"
    echo "[+] New policy line:"
    grep "^selection_policy" "$CLEANER_CONF"


    # 2. Start cleaner daemon
    start_cleaner

    # 3. Run workloads
    for workload in "${WORKLOADS[@]}"; do
        if [ ! -x "$workload" ]; then
            echo "Error: workload script $workload not executable."
            stop_cleaner
            exit 1
        fi

        logname="${policy}_$(basename "$workload" .sh).csv"
        outfile="$ANAL_DIR/$logname"

        echo "[+] Running workload $workload → $outfile"
        "$TEST_MODEL" -w "./$workload" -d "$outfile"
    done

    # 4. Stop cleaner
    stop_cleaner

done

echo "[+] All policy categories tested."

