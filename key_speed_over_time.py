import pandas as pd
import matplotlib.pyplot as plt
import seaborn as sns
import argparse

# --- Parse command-line arguments ---
parser = argparse.ArgumentParser(description="Plot typing speed over time for a specific key.")
parser.add_argument("-p", "--player", type=str, required=True, help="Player to show stats for")
parser.add_argument("-k", "--key", type=str, required=True, help="The key to plot")
parser.add_argument("-s", "--smoothness", type=int, default=1, help="Add rolling average of the graph")
args = parser.parse_args()
player = args.player
key_to_plot = args.key
smoothness = args.smoothness

# --- Load CSV ---
df = pd.read_csv(f"stats/{player}.key-history.csv")

# Filter for the specified key
df_key = df[df['key'] == key_to_plot].copy()

if df_key.empty:
    print(f"No data found for key '{key_to_plot}'")
    exit()

# Reset index to use as instance number
df_key = df_key.reset_index(drop=True)
df_key['instance'] = df_key.index + 1  # start counting from 1

# --- Optional: Smoothed WPM with rolling average ---
df_key['wpm_smooth'] = df_key['wpm'].rolling(window=smoothness, min_periods=1).mean()

plt.figure(figsize=(14, 6))
sns.lineplot(
    data=df_key,
    x='instance',
    y='wpm_smooth',
    marker='o',
    color='orange',
    label='Smoothed WPM'
)
plt.title(f"Typing Speed Over Instances for Key '{key_to_plot}', smoothness={smoothness}", fontsize=16)
plt.xlabel("Instance", fontsize=12)
plt.ylabel("WPM (Smoothed)", fontsize=12)
plt.legend()
plt.tight_layout()
plt.show()
