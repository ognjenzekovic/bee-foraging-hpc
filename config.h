// config.h
#ifndef CONFIG_H
#define CONFIG_H

// ========== PARAMETRI ZA BRZO TESTIRANJE ==========
#define NUM_BEES 50
#define NUM_FLOWERS 10
#define MAX_TIMESTEPS 200
#define WORLD_SIZE 500.0f

// ========== PARAMETRI ZA REALISTIČKO TESTIRANJE ==========
// #define NUM_BEES 5000
// #define NUM_FLOWERS 200
// #define MAX_TIMESTEPS 2000
// #define WORLD_SIZE 2000.0f

// ========== PARAMETRI ZA HPC BENCHMARK ==========
// #define NUM_BEES 50000
// #define NUM_FLOWERS 1000
// #define MAX_TIMESTEPS 5000
// #define WORLD_SIZE 5000.0f

// Parametri pčela
#define BEE_SPEED 5.0f          // Brzina kretanja
#define BEE_VISION_RANGE 100.0f // Koliko daleko vide
#define SCOUT_RATIO 0.2f        // 20% su scouts
#define MAX_ENERGY 100.0f
#define ENERGY_COST 0.2f // Po koraku

// Parametri cvetova
#define FLOWER_NECTAR_MAX 100.0f
#define FLOWER_CAPACITY 5      // Max pčela istovremeno
#define NECTAR_REGEN_RATE 0.5f // Regeneracija

// Waggle dance parametri
#define DANCE_DURATION 5          // Timesteps koliko traje ples
#define DECISION_PROBABILITY 0.4f // Verovatnoća pridruživanja

// Pozicija košnice
#define HIVE_X (WORLD_SIZE / 2.0f)
#define HIVE_Y (WORLD_SIZE / 2.0f)
#define HIVE_RADIUS 30.0f

#endif