import pandas as pd
import matplotlib.pyplot as plt
import seaborn as sns

# Load CSV
df = pd.read_csv("stats/isak.key-history.csv")

# --- Per key stats ---
per_key = df.groupby("key").agg(
    presses=("key", "count"),
    avg_wpm=("wpm", "mean"),
    accuracy=("acc", "mean")
).reset_index()

# --- Accuracy per key ---
plt.figure(figsize=(12, 5))
sns.barplot(x="key", y="accuracy", data=per_key, palette="viridis")
plt.title("Accuracy per Key")
plt.ylabel("Accuracy")
plt.xlabel("Key")
plt.ylim(0, 1)
plt.show()

# --- WPM per key ---
plt.figure(figsize=(12, 5))
sns.barplot(x="key", y="avg_wpm", data=per_key, palette="magma")
plt.title("Average WPM per Key")
plt.ylabel("WPM")
plt.xlabel("Key")
plt.show()

# --- Key usage ---
plt.figure(figsize=(12, 5))
sns.barplot(x="key", y="presses", data=per_key, palette="cubehelix")
plt.title("Key Usage (Number of Presses)")
plt.ylabel("Presses")
plt.xlabel("Key")
plt.show()

# --- Transition heatmap ---
transitions = df.groupby(["prevKey", "key"]).size().unstack(fill_value=0)

plt.figure(figsize=(14, 10))
sns.heatmap(transitions, cmap="Blues", cbar=True)
plt.title("Key Transition Heatmap (prevKey → key)")
plt.ylabel("Previous Key")
plt.xlabel("Current Key")
plt.show()

# --- Distribution of WPM ---
plt.figure(figsize=(10, 5))
sns.histplot(df["wpm"], bins=30, kde=True, color="skyblue")
plt.title("Distribution of WPM per Key Press")
plt.xlabel("WPM")
plt.ylabel("Frequency")
plt.show()

digraphs = df[df["prevKey"] != "NONE"].copy()

# --- Average WPM per key-to-key transition ---
digraph_speed = digraphs.groupby(["prevKey", "key"]).agg(
    avg_wpm=("wpm", "mean"),
    count=("wpm", "size"),
    accuracy=("acc", "mean")
).reset_index()

# --- Top fastest transitions ---
fastest = digraph_speed.sort_values("avg_wpm", ascending=False).head(15)
print("\n🔥 Fastest key-to-key transitions:")
print(fastest[["prevKey", "key", "avg_wpm", "count", "accuracy"]])

# --- Top slowest transitions ---
slowest = digraph_speed.sort_values("avg_wpm", ascending=True).head(15)
print("\n🐢 Slowest key-to-key transitions:")
print(slowest[["prevKey", "key", "avg_wpm", "count", "accuracy"]])

# --- Heatmap of average WPM for transitions ---
pivot_wpm = digraph_speed.pivot(index="prevKey", columns="key", values="avg_wpm").fillna(0)

plt.figure(figsize=(14, 10))
sns.heatmap(pivot_wpm, cmap="viridis", cbar_kws={'label': 'Avg WPM'})
plt.title("Average WPM by Key-to-Key Transition (prevKey → key)")
plt.ylabel("Previous Key")
plt.xlabel("Current Key")
plt.show()
