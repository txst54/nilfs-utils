#!/bin/bash

# ======================
# Configuration
# ======================
MOUNT_POINT="/mnt/nilfs"   # change as needed
DATA_LOG="/var/log/nilfs/data.log"
WAIT_TIME=120              # workload runtime in seconds

usage() {
    echo "Usage: $0 -w <workload_script> -d <destination_csv>"
    echo ""
    echo "  -w    Workload script to run (must be executable)"
    echo "  -d    Destination output file (CSV)"
    echo "  -h    Show this help message"
    exit 1
}

# ======================
# Parse Arguments
# ======================
while getopts "w:d:h" opt; do
    case ${opt} in
        w) WORKLOAD="$OPTARG" ;;
        d) DEST="$OPTARG" ;;
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

echo "[+] Starting workload: $WORKLOAD"
"$WORKLOAD" &
WID=$!

echo "[+] Workload PID = $WID"
echo "[+] Letting workload run for $WAIT_TIME seconds..."
sleep "$WAIT_TIME"

echo "[+] Killing workload PID $WID"
kill "$WID" 2>/dev/null
sleep 1
kill -9 "$WID" 2>/dev/null

# ======================
# Save results
# ======================

if [ ! -f "$DATA_LOG" ]; then
    echo "Error: data log '$DATA_LOG' not found."
    exit 1
fi

echo "[+] Copying log to $DEST"
# Add CSV header and append actual data
{
    echo "seg,blk,util"
    cat "$DATA_LOG"
} > "$DEST"

echo "[+] Done. Output written to: $DEST"
