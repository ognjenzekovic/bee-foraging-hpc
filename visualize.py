"""
Bee Foraging Simulation - Visualization
Reads positions.csv and creates animation
"""

import pandas as pd
import matplotlib.pyplot as plt
import matplotlib.animation as animation
from matplotlib.patches import Circle
import numpy as np

print("Loading data...")
df = pd.read_csv('positions.csv')

# Parametri (mora da odgovara config.h)
WORLD_SIZE = 800.0
HIVE_RADIUS = 10.0
HIVE_X = WORLD_SIZE / 2
HIVE_Y = WORLD_SIZE / 2

# Boje za različita stanja pčela
STATE_COLORS = {
    0: 'gray',      # IDLE
    1: 'blue',      # SCOUT
    2: 'green',     # RETURNING
    3: 'red',       # DANCING
    4: 'cyan',      # FOLLOWER
    5: 'orange'     # FORAGING
}

STATE_NAMES = {
    0: 'Idle',
    1: 'Scout',
    2: 'Returning',
    3: 'Dancing',
    4: 'Follower',
    5: 'Foraging'
}

# Uzmi sve timestep-ove
timesteps = sorted(df['timestep'].unique())
print(f"Found {len(timesteps)} timesteps")

# Setup figure
fig, ax = plt.subplots(figsize=(12, 10))
ax.set_xlim(0, WORLD_SIZE)
ax.set_ylim(0, WORLD_SIZE)
ax.set_aspect('equal')
ax.set_facecolor('#f0f0f0')

def animate(frame_idx):
    ax.clear()
    ax.set_xlim(0, WORLD_SIZE)
    ax.set_ylim(0, WORLD_SIZE)
    ax.set_aspect('equal')
    ax.set_facecolor('#f0f0f0')
    
    t = timesteps[frame_idx]
    ax.set_title(f'Bee Foraging Simulation - Timestep: {t}', 
                 fontsize=16, fontweight='bold')
    
    # Filtriraj podatke za ovaj timestep
    frame_data = df[df['timestep'] == t]
    
    # Crtaj košnicu
    hive = Circle((HIVE_X, HIVE_Y), HIVE_RADIUS, 
                  facecolor='brown', alpha=0.4, linewidth=2, 
                  edgecolor='saddlebrown', label='Hive')
    ax.add_patch(hive)
    
    # Crtaj cvetove
    flowers = frame_data[frame_data['type'] == 'flower']
    if len(flowers) > 0:
        # Veličina zavisi od dostupnog nektara
        sizes = flowers['nectar'] * 3  # Scale factor
        ax.scatter(flowers['x'], flowers['y'], 
                   c='pink', s=sizes, marker='*', 
                   edgecolors='red', linewidths=2, 
                   label='Flowers', zorder=2, alpha=0.8)
    
    # Crtaj pčele po stanju
    bees = frame_data[frame_data['type'] == 'bee']
    
    legend_handles = []
    for state, color in STATE_COLORS.items():
        state_bees = bees[bees['state'] == state]
        if len(state_bees) > 0:
            scatter = ax.scatter(state_bees['x'], state_bees['y'],
                                c=color, s=30, alpha=0.7,
                                edgecolors='black', linewidths=0.5,
                                label=f'{STATE_NAMES[state]} ({len(state_bees)})',
                                zorder=3)
            legend_handles.append(scatter)
    
    # Grid
    ax.grid(True, alpha=0.2, linestyle='--')
    
    # Legend
    ax.legend(loc='upper right', fontsize=10, framealpha=0.9)
    
    # Statistika
    stats_text = f"Bees: {len(bees)} | Flowers: {len(flowers)}"
    ax.text(0.02, 0.98, stats_text, 
            transform=ax.transAxes, fontsize=10,
            verticalalignment='top',
            bbox=dict(boxstyle='round', facecolor='white', alpha=0.8))

print("Creating animation...")
ani = animation.FuncAnimation(fig, animate, frames=len(timesteps), 
                             interval=100, repeat=True, blit=False)

print("Saving animation as bee_simulation.gif...")
try:
    ani.save('bee_simulation.gif', writer='pillow', fps=10, dpi=80)
    print("✓ Saved as bee_simulation.gif")
except Exception as e:
    print(f"Could not save GIF: {e}")

print("Displaying animation...")
plt.show()