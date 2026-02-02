#ifndef CONFIG_H
#define CONFIG_H

// for visualisation.py
#define WORLD_SIZE 1000.0f
#define HIVE_RADIUS 10.0f

// fast comparison
// #define NUM_BEES 50
// #define NUM_FLOWERS 3
// #define MAX_TIMESTEPS 500

// real comp
#define NUM_BEES 20000
#define NUM_FLOWERS 2000
#define MAX_TIMESTEPS 10000

#define BEE_SPEED 5.0f
#define BEE_VISION_RANGE 20.0f
#define SCOUT_RATIO 0.2f
#define MAX_ENERGY 100.0f
#define ENERGY_COST 0.1f

#define FLOWER_NECTAR_MAX 5.0f
#define FLOWER_CAPACITY 5
#define NECTAR_REGEN_RATE 0.1f

#define DANCE_DURATION 5
#define DECISION_PROBABILITY 0.3f

#define HIVE_X (WORLD_SIZE / 2.0f)
#define HIVE_Y (WORLD_SIZE / 2.0f)

#endif