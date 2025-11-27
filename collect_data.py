#!/usr/bin/env python3
import subprocess
import time
import csv
import re
import argparse
from datetime import datetime

ROW_RE = re.compile(
    r"^\s*(\d+)\s+"               # segnum
    r"(\d{4}-\d{2}-\d{2})\s+"     # date
    r"(\d{2}:\d{2}:\d{2})\s+"     # time
    r"(\S+)\s+"                   # STAT (e.g. -d--)
    r"(\d+)\s+"                   # NBLOCKS
    r"(\d+)\s*"                   # NLIVEBLOCKS
    r"\(\s*\d+%\s*\)"             # (  0%)  — ignore value, just match it
)


def run_cmd(cmd):
    out = subprocess.run(
        cmd, shell=True, capture_output=True, text=True
    )
    if out.returncode != 0:
        print(f"Command failed: {cmd}")
        print(out.stderr)
    return out.stdout

def parse_lssu_output(text):
    entries = []

    for line in text.splitlines():
        m = ROW_RE.match(line)
        if not m:
            continue

        segnum = int(m.group(1))
        date = m.group(2)
        time_s = m.group(3)
        nblocks = int(m.group(5))
        lblocks = int(m.group(6))

        # Skip segments with NBLOCKS = 0
        if nblocks == 0:
            continue

        util = lblocks / nblocks

        entries.append({
            "segnum": segnum,
            "date": date,
            "time": time_s,
            "nblocks": nblocks,
            "nliveblocks": lblocks,
            "util": util,
        })

    return entries



def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--dev", default="/dev/sda4",
                        help="NILFS2 device (default /dev/sda4)")
    parser.add_argument("--out", default="segment_util.csv",
                        help="Output CSV")
    parser.add_argument("--interval", type=int, default=5,
                        help="Polling interval (seconds)")
    args = parser.parse_args()

    # set up csv
    with open(args.out, "a", newline="") as csvfile:
        writer = csv.writer(csvfile)

        # write header if file empty
        if csvfile.tell() == 0:
            writer.writerow([
                "timestamp",
                "segnum",
                "nblocks",
                "nliveblocks",
                "util"
            ])

        while True:
            timestamp = datetime.now().isoformat()

            run_cmd("nilfs-clean")  # may require sudo
            time.sleep(0.2)

            out = run_cmd(f"lssu {args.dev} -l")
            rows = parse_lssu_output(out)

            for r in rows:
                writer.writerow([
                    timestamp,
                    r["segnum"],
                    r["nblocks"],
                    r["nliveblocks"],
                    f"{r['util']:.6f}",
                ])

            csvfile.flush()
            time.sleep(args.interval)

if __name__ == "__main__":
    main()
