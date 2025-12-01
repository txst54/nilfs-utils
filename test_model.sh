#!/bin/bash

# ======================
# Configuration
# ======================
MOUNT_POINT="/mnt/nilfs"        # change as needed
DATA_LOG="/var/log/nilfs/data.log"
NUM_WRITES=30000                # default number of writes, replace WAIT_TIME

usage() {
    echo "Usage: $0 -w <workload_script> -d <destination_csv> [-n NUM_WRITES]"
    echo ""
    echo "  -w    Workload script to run (must be executable)"
    echo "  -d    Destination output file (CSV)"
    echo "  -n    Number of writes to perform (default: 30000)"
    echo "  -h    Show this help message"
    exit 1
}

# ======================
# Parse Arguments
# ======================
while getopts "w:d:n:h" opt; do
    case ${opt} in
        w) WORKLOAD="$OPTARG" ;;
        d) DEST="$OPTARG" ;;
        n) NUM_WRITES="$OPTARG" ;;
        h) usage ;;
        *) usage ;;
    esac
done

if [ -z "$WORKLOAD" ] || [ -z "$DEST" ]; then
    usage
fi

if [ ! -x "$WORKLOAD" ]; then
    echo "Error: workload script '$WORKLOAD' is not executable."
    exit 1
fi

# Validate NUM_WRITES
if ! [[ "$NUM_WRITES" =~ ^[0-9]+$ ]] || (( NUM_WRITES <= 0 )); then
    echo "Error: -n must be a positive integer"
    exit 1
fi

# ======================
# Reset the environment
# ======================

echo "[+] Removing old NILFS log: $DATA_LOG"
rm -f "$DATA_LOG"

echo "[+] Clearing mount point: $MOUNT_POINT"
rm -rf "${MOUNT_POINT:?}/"*

echo "[+] Forcing NILFS cleaner sync ×3"
for i in 1 2 3; do
    nilfs-clean -p 0
    sleep 1
done

# ======================
# Run workload
# ======================

echo "[+] Starting workload: $WORKLOAD (writes = $NUM_WRITES)"
"$WORKLOAD" -w "$NUM_WRITES" &
WID=$!

echo "[+] Workload PID = $WID"

# Wait for workload to finish on its own
wait $WID

echo "[+] Workload finished."

# ======================
# Save results
# ======================

if [ ! -f "$DATA_LOG" ]; then
    echo "Error: data log '$DATA_LOG' not found."
    exit 1
fi

echo "[+] Copying log to $DEST"
{
    echo "seg,blk,util"
    cat "$DATA_LOG"
} > "$DEST"

echo "[+] Done. Output written to: $DEST"

