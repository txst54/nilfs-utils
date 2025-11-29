import csv
import numpy as np
import matplotlib
import matplotlib.pyplot as plt
import pandas as pd

matplotlib.use("Agg")

csv_files = {
    # "Timestamp (Hot and Cold)": "data_hc.log",
    # "Timestamp (Uniform)": "data_uni.log",
    "Timestamp (Uniform)": "timestamp_test_rand.csv",
    "Greedy (Uniform)": "greedy_test_rand.csv",
    "Cost-Benefit (Uniform)": "cost-benefit_test_rand.csv",

}
bins = 50
title = "Segment Utilization Distribution (Line Plot)"

plt.figure(figsize=(8, 5))

for label, path in csv_files.items():
    df = pd.read_csv(path)
    if 'util' not in df.columns:
        raise ValueError(f"{path} missing 'util' column")

    # Exclude zero utilization rows
    df = df[df['util'] > 0]

    # Compute histogram (density=True gives fraction of total segments)
    counts, edges = np.histogram(df['util'], bins=bins, density=True)
    centers = (edges[:-1] + edges[1:]) / 2  # bin centers for smooth x-axis

    plt.plot(centers, counts, label=label, linewidth=2)

plt.title(title)
plt.xlabel("Segment Utilization")
plt.ylabel("Fraction of Segments")
plt.legend()
plt.grid(True, linestyle='--', alpha=0.5)
plt.tight_layout()
plt.savefig("util_dist.png", dpi=200)