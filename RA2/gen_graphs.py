import matplotlib
matplotlib.use('Agg')
import matplotlib.pyplot as plt
import numpy as np

out = "/Users/bry/Desktop/bry/college/third year/second semester/CMSC 180/RA2/data"

t_vals = [1, 2, 4, 8, 16, 32, 64]

# Lab Activity 1: Column Partitioning, n=25000
act1_avg = [17.630, 17.116, 17.631, 12.212, 11.621, 12.358, 10.823]

# Lab Activity 2: Row Partitioning, n=30000 and n=40000
act2_30k_avg = [29.853, 20.199, 9.378, 10.364, 8.183, 6.750, 7.342]
act2_40k_avg = [79.804, 62.744, 43.564, 48.650, 37.075, 30.846, 32.213]

# Lab Activity 3: Row Partitioning, n=25000
act3_avg = [10.525, 6.126, 4.493, 3.953, 3.676, 3.486, 3.391]

# Common style
plt.rcParams.update({
    'font.size': 11,
    'font.family': 'serif',
    'axes.grid': True,
    'grid.alpha': 0.3,
    'figure.figsize': (6, 4),
    'figure.dpi': 300,
})

# --- Graph 1: Lab Activity 1 ---
fig, ax = plt.subplots()
ax.plot(t_vals, act1_avg, 'o-', color='#2563eb', linewidth=2, markersize=6, label='n = 25,000')
ax.set_xlabel('Number of Threads (t)')
ax.set_ylabel('Average Runtime (seconds)')
ax.set_title('Lab Activity 1: Column Partitioning (n = 25,000)')
ax.set_xticks(t_vals)
ax.set_xticklabels([str(t) for t in t_vals])
ax.set_ylim(bottom=0)
ax.legend()
plt.tight_layout()
fig.savefig(f'{out}/act1_graph.png', dpi=300, bbox_inches='tight')
plt.close()

# --- Graph 2: Lab Activity 2 (both n=30k and n=40k) ---
fig, ax = plt.subplots()
ax.plot(t_vals, act2_30k_avg, 's-', color='#dc2626', linewidth=2, markersize=6, label='n = 30,000')
ax.plot(t_vals, act2_40k_avg, '^-', color='#9333ea', linewidth=2, markersize=6, label='n = 40,000')
ax.set_xlabel('Number of Threads (t)')
ax.set_ylabel('Average Runtime (seconds)')
ax.set_title('Lab Activity 2: Row Partitioning (Larger n)')
ax.set_xticks(t_vals)
ax.set_xticklabels([str(t) for t in t_vals])
ax.set_ylim(bottom=0)
ax.legend()
plt.tight_layout()
fig.savefig(f'{out}/act2_graph.png', dpi=300, bbox_inches='tight')
plt.close()

# --- Graph 3: Lab Activity 3 ---
fig, ax = plt.subplots()
ax.plot(t_vals, act3_avg, 'D-', color='#059669', linewidth=2, markersize=6, label='n = 25,000')
ax.set_xlabel('Number of Threads (t)')
ax.set_ylabel('Average Runtime (seconds)')
ax.set_title('Lab Activity 3: Row Partitioning (n = 25,000)')
ax.set_xticks(t_vals)
ax.set_xticklabels([str(t) for t in t_vals])
ax.set_ylim(bottom=0)
ax.legend()
plt.tight_layout()
fig.savefig(f'{out}/act3_graph.png', dpi=300, bbox_inches='tight')
plt.close()

print("Graphs saved to data/act1_graph.png, act2_graph.png, act3_graph.png")
