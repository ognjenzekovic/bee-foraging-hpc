// types.h
#ifndef TYPES_H
#define TYPES_H

typedef struct {
    float x, y;
} Vector2D;

typedef enum {
    IDLE,           // U košnici, čeka
    SCOUT,          // Traži cvetove
    RETURNING,      // Vraća se sa informacijom
    DANCING,        // Pleše u košnici
    FOLLOWER,       // Prati drugu pčelu
    FORAGING        // Skuplja nektar
} BeeState;

typedef struct {
    int id;
    Vector2D position;
    Vector2D velocity;
    BeeState state;
    
    float energy;
    int target_flower;      // ID cveta ka kojem ide (-1 ako nema)
    int following_dance;    // ID plesa koji prati (-1 ako ne prati)
    
    // Za dance
    float nectar_found;     // Količina nektara pronađenog
    int dance_followers;    // Broj followers
    int dance_timer;        // Koliko još pleše
} Bee;

typedef struct {
    Vector2D position;
    float nectar_available;
    float nectar_total;
    int bees_feeding;       // Trenutno broj pčela
    int capacity;           // Max kapacitet
} Flower;

typedef struct {
    int bee_id;
    Vector2D flower_location;
    float nectar_quality;
    float distance_from_hive;
    int followers;
} WaggleDance;

typedef struct {
    Bee* bees;
    Flower* flowers;
    WaggleDance* dances;
    int num_dances;
    
    // Statistika
    float total_nectar_collected;
    int timestep;
} Simulation;

#endif