#ifndef TYPES_H
#define TYPES_H
#include <omp.h>

typedef struct
{
    float x, y;
} Vector2D;

typedef enum
{
    IDLE,
    SCOUT,
    RETURNING,
    DANCING,
    FOLLOWER,
    FORAGING
} BeeState;

typedef struct
{
    int id;
    Vector2D position;
    Vector2D velocity;
    BeeState state;

    float energy;
    int target_flower;   //-1 if empty
    int following_dance; //-1 if empty
    Vector2D target_location;

    float nectar_found;
    int dance_followers;
    int dance_timer;
} Bee;

typedef struct
{
    Vector2D position;
    float nectar_available;
    float nectar_total;
    int bees_feeding;
    int capacity;
    omp_lock_t lock;
} Flower;

typedef struct
{
    int bee_id;
    Vector2D flower_location;
    float nectar_quality;
    float distance_from_hive;
    int followers;
} WaggleDance;

typedef struct
{
    Bee *bees;
    Flower *flowers;
    WaggleDance *dances;
    int num_dances;

    float total_nectar_collected;
    int timestep;

    int num_local_bees;
    int bee_offset;
} Simulation;

#endif