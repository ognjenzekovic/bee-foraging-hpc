#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <string.h>
#include <time.h>
#include <omp.h>
#include "types.h"
#include "config.h"

float distance(Vector2D a, Vector2D b)
{
    float dx = a.x - b.x;
    float dy = a.y - b.y;
    return sqrtf(dx * dx + dy * dy);
}

float random_float_r(unsigned int *seed, float min, float max)
{
    return min + (max - min) * ((float)rand_r(seed) / RAND_MAX);
}

Vector2D random_position_r(unsigned int *seed)
{
    Vector2D pos;
    pos.x = random_float_r(seed, 0, WORLD_SIZE);
    pos.y = random_float_r(seed, 0, WORLD_SIZE);
    return pos;
}

Vector2D normalize(Vector2D v)
{
    float len = sqrtf(v.x * v.x + v.y * v.y);
    if (len > 0.0001f)
    {
        v.x /= len;
        v.y /= len;
    }
    return v;
}

void init_bees(Bee *bees, int num_bees)
{
    unsigned int seed = 42;
    for (int i = 0; i < num_bees; i++)
    {
        bees[i].id = i;
        bees[i].position.x = HIVE_X + random_float_r(&seed, -HIVE_RADIUS, HIVE_RADIUS);
        bees[i].position.y = HIVE_Y + random_float_r(&seed, -HIVE_RADIUS, HIVE_RADIUS);
        bees[i].velocity.x = 0;
        bees[i].velocity.y = 0;

        if (i < (int)(num_bees * SCOUT_RATIO))
        {
            bees[i].state = SCOUT;
        }
        else
        {
            bees[i].state = IDLE;
        }

        bees[i].energy = MAX_ENERGY;
        bees[i].target_flower = -1;
        bees[i].following_dance = -1;
        bees[i].nectar_found = 0;
        bees[i].dance_followers = 0;
        bees[i].dance_timer = 0;
    }
}

void init_flowers(Flower *flowers, int num_flowers)
{
    unsigned int seed = 123;
    for (int i = 0; i < num_flowers; i++)
    {
        flowers[i].position = random_position_r(&seed);
        flowers[i].nectar_total = FLOWER_NECTAR_MAX;
        flowers[i].nectar_available = FLOWER_NECTAR_MAX;
        flowers[i].bees_feeding = 0;
        flowers[i].capacity = FLOWER_CAPACITY;
        omp_init_lock(&flowers[i].lock);
    }
}

Simulation *create_simulation()
{
    Simulation *sim = (Simulation *)malloc(sizeof(Simulation));

    sim->bees = (Bee *)malloc(NUM_BEES * sizeof(Bee));
    sim->flowers = (Flower *)malloc(NUM_FLOWERS * sizeof(Flower));
    sim->dances = (WaggleDance *)malloc(NUM_BEES * sizeof(WaggleDance));
    sim->num_dances = 0;
    sim->total_nectar_collected = 0;
    sim->timestep = 0;

    init_bees(sim->bees, NUM_BEES);
    init_flowers(sim->flowers, NUM_FLOWERS);

    return sim;
}

void destroy_simulation(Simulation *sim)
{
    for (int i = 0; i < NUM_FLOWERS; i++)
    {
        omp_destroy_lock(&sim->flowers[i].lock);
    }
    free(sim->bees);
    free(sim->flowers);
    free(sim->dances);
    free(sim);
}

void move_towards(Bee *bee, Vector2D target)
{
    Vector2D direction;
    direction.x = target.x - bee->position.x;
    direction.y = target.y - bee->position.y;

    direction = normalize(direction);

    bee->velocity.x = direction.x * BEE_SPEED;
    bee->velocity.y = direction.y * BEE_SPEED;

    bee->position.x += bee->velocity.x;
    bee->position.y += bee->velocity.y;

    bee->energy -= ENERGY_COST;
}

void scout_behavior(Bee *bee, Flower *flowers, int num_flowers, unsigned int *seed)
{
    if (bee->target_flower == -1)
    {
        // Levy flight
        if (random_float_r(seed, 0, 1) < 0.5f)
        {
            bee->position.x += random_float_r(seed, -BEE_SPEED, BEE_SPEED);
            bee->position.y += random_float_r(seed, -BEE_SPEED, BEE_SPEED);
        }
        else
        {
            bee->position.x += random_float_r(seed, -BEE_SPEED * 10, BEE_SPEED * 10);
            bee->position.y += random_float_r(seed, -BEE_SPEED * 10, BEE_SPEED * 10);
        }

        for (int i = 0; i < num_flowers; i++)
        {
            float dist = distance(bee->position, flowers[i].position);
            if (dist < BEE_VISION_RANGE && flowers[i].nectar_available > 0)
            {
                bee->target_flower = i;
                bee->nectar_found = flowers[i].nectar_available;
                bee->state = RETURNING;
                break;
            }
        }
    }

    bee->energy -= ENERGY_COST;
}

void returning_behavior(Bee *bee)
{
    Vector2D hive_pos = {HIVE_X, HIVE_Y};
    move_towards(bee, hive_pos);

    float dist_to_hive = distance(bee->position, hive_pos);
    if (dist_to_hive < HIVE_RADIUS)
    {
        bee->state = DANCING;
        bee->dance_timer = DANCE_DURATION;
    }
}

void create_dance(Simulation *sim, Bee *bee, omp_lock_t *dance_lock)
{
    WaggleDance dance;
    dance.bee_id = bee->id;
    dance.flower_location = sim->flowers[bee->target_flower].position;
    dance.nectar_quality = bee->nectar_found / FLOWER_NECTAR_MAX;

    Vector2D hive_pos = {HIVE_X, HIVE_Y};
    dance.distance_from_hive = distance(hive_pos, dance.flower_location);
    dance.followers = 0;

    omp_set_lock(dance_lock);
    sim->dances[sim->num_dances] = dance;
    sim->num_dances++;
    omp_unset_lock(dance_lock);
}

float calculate_dance_attractiveness(WaggleDance *dance)
{
    float quality_factor = dance->nectar_quality * 2.0f;
    float distance_penalty = 1.0f / (1.0f + dance->distance_from_hive / 100.0f);
    float follower_bonus = 1.0f + (dance->followers * 0.1f);

    return quality_factor * distance_penalty * follower_bonus;
}

int choose_dance(Simulation *sim, unsigned int *seed)
{
    if (sim->num_dances == 0)
        return -1;

    float total_score = 0.0f;
    for (int i = 0; i < sim->num_dances; i++)
    {
        total_score += calculate_dance_attractiveness(&sim->dances[i]);
    }

    if (total_score < 0.0001f)
        return -1;

    float random_val = random_float_r(seed, 0, total_score);
    float cumulative = 0.0f;

    for (int i = 0; i < sim->num_dances; i++)
    {
        cumulative += calculate_dance_attractiveness(&sim->dances[i]);
        if (random_val <= cumulative)
        {
            return i;
        }
    }

    return sim->num_dances - 1;
}

void idle_bees_watch_dances(Simulation *sim, unsigned int *seeds)
{
#pragma omp parallel for schedule(dynamic, 100)
    for (int i = 0; i < NUM_BEES; i++)
    {
        Bee *bee = &sim->bees[i];
        unsigned int *seed = &seeds[omp_get_thread_num()];

        if (bee->state == IDLE && sim->num_dances > 0)
        {
            if (random_float_r(seed, 0, 1) < DECISION_PROBABILITY)
            {
                int chosen_dance = choose_dance(sim, seed);

                if (chosen_dance >= 0)
                {
                    bee->following_dance = chosen_dance;
                    bee->state = FOLLOWER;

#pragma omp atomic
                    sim->dances[chosen_dance].followers++;

                    bee->target_location = sim->dances[chosen_dance].flower_location;

                    int dancer_id = sim->dances[chosen_dance].bee_id;
#pragma omp atomic
                    sim->bees[dancer_id].dance_followers++;
                }
            }
        }
    }
}

void follower_behavior(Bee *bee, Simulation *sim)
{
    Vector2D target = bee->target_location;
    float dist = distance(bee->position, target);

    move_towards(bee, target);

    if (dist < 10.0f)
    {
        bee->state = FORAGING;

        for (int i = 0; i < NUM_FLOWERS; i++)
        {
            float flower_dist = distance(sim->flowers[i].position, target);
            if (flower_dist < 20.0f)
            {
                bee->target_flower = i;
                break;
            }
        }

        if (bee->target_flower == -1)
        {
            bee->state = RETURNING;
        }
    }

    if (bee->energy < MAX_ENERGY * 0.2f)
    {
        bee->state = RETURNING;
        bee->target_flower = -1;
    }
}

void foraging_behavior(Bee *bee, Simulation *sim, float *local_nectar)
{
    if (bee->target_flower < 0)
    {
        bee->state = IDLE;
        return;
    }

    Flower *flower = &sim->flowers[bee->target_flower];

    omp_set_lock(&flower->lock);

    if (flower->bees_feeding < flower->capacity && flower->nectar_available > 0)
    {
        flower->bees_feeding++;

        float collected = fminf(10.0f, flower->nectar_available);
        flower->nectar_available -= collected;

        *local_nectar += collected;
        bee->energy = fminf(MAX_ENERGY, bee->energy + collected * 0.5f);

        flower->bees_feeding--;

        omp_unset_lock(&flower->lock);

        bee->state = RETURNING;
        bee->target_flower = -1;
        bee->following_dance = -1;
    }
    else
    {
        omp_unset_lock(&flower->lock);

        if (bee->following_dance >= 0)
        {
            bee->state = RETURNING;
            bee->target_flower = -1;
            bee->following_dance = -1;
        }
        else
        {
            bee->state = SCOUT;
            bee->target_flower = -1;
        }
    }
}

void update_bees(Simulation *sim, unsigned int *seeds, omp_lock_t *dance_lock)
{
    float total_nectar_local = 0.0f;

#pragma omp parallel reduction(+ : total_nectar_local)
    {
        int tid = omp_get_thread_num();
        unsigned int *seed = &seeds[tid];
        float my_nectar = 0.0f;

#pragma omp for schedule(dynamic, 100)
        for (int i = 0; i < NUM_BEES; i++)
        {
            Bee *bee = &sim->bees[i];

            switch (bee->state)
            {
            case SCOUT:
                scout_behavior(bee, sim->flowers, NUM_FLOWERS, seed);
                break;

            case RETURNING:
                returning_behavior(bee);

                if (bee->state == DANCING && bee->dance_timer == DANCE_DURATION)
                {
                    create_dance(sim, bee, dance_lock);
                }
                break;

            case DANCING:
                bee->dance_timer--;
                if (bee->dance_timer <= 0)
                {
                    if (bee->dance_followers > 0)
                    {
                        bee->state = FORAGING;
                    }
                    else
                    {
                        bee->state = IDLE;
                        bee->target_flower = -1;
                    }
                    bee->dance_followers = 0;
                }
                break;

            case IDLE:
                break;

            case FOLLOWER:
                follower_behavior(bee, sim);
                break;

            case FORAGING:
                foraging_behavior(bee, sim, &my_nectar);
                break;
            }

            if (bee->energy <= 0)
            {
                Vector2D hive_pos = {HIVE_X, HIVE_Y};
                float dist = distance(bee->position, hive_pos);

                if (dist < HIVE_RADIUS)
                {
                    bee->energy = MAX_ENERGY;
                    bee->state = IDLE;
                }
                else
                {
                    bee->state = RETURNING;
                    bee->target_flower = -1;
                }
            }

            if (bee->position.x < 0)
                bee->position.x = 0;
            if (bee->position.x > WORLD_SIZE)
                bee->position.x = WORLD_SIZE;
            if (bee->position.y < 0)
                bee->position.y = 0;
            if (bee->position.y > WORLD_SIZE)
                bee->position.y = WORLD_SIZE;
        }

        total_nectar_local += my_nectar;
    }

    sim->total_nectar_collected += total_nectar_local;

    idle_bees_watch_dances(sim, seeds);
}

void update_flowers(Simulation *sim)
{
#pragma omp parallel for schedule(static)
    for (int i = 0; i < NUM_FLOWERS; i++)
    {
        if (sim->flowers[i].nectar_available < sim->flowers[i].nectar_total)
        {
            sim->flowers[i].nectar_available += NECTAR_REGEN_RATE;
        }
    }
}

void simulation_step(Simulation *sim, unsigned int *seeds, omp_lock_t *dance_lock)
{
    update_bees(sim, seeds, dance_lock);
    update_flowers(sim);

    sim->num_dances = 0;
    sim->timestep++;
}

void print_statistics(Simulation *sim)
{
    int scouts = 0, idle = 0, dancing = 0, followers = 0, foraging = 0, returning = 0;

    for (int i = 0; i < NUM_BEES; i++)
    {
        switch (sim->bees[i].state)
        {
        case SCOUT:
            scouts++;
            break;
        case IDLE:
            idle++;
            break;
        case DANCING:
            dancing++;
            break;
        case FOLLOWER:
            followers++;
            break;
        case FORAGING:
            foraging++;
            break;
        case RETURNING:
            returning++;
            break;
        }
    }

    printf("Step %4d | Nectar: %7.2f | Scout: %3d | Idle: %3d | Dance: %3d | Follow: %3d | Forage: %3d | Return: %3d\n",
           sim->timestep, sim->total_nectar_collected, scouts, idle, dancing, followers, foraging, returning);
}

void save_results(Simulation *sim, const char *filename)
{
    FILE *f = fopen(filename, "w");
    if (!f)
    {
        printf("Error opening file %s\n", filename);
        return;
    }

    fprintf(f, "# Bee Foraging Simulation Results (OpenMP)\n");
    fprintf(f, "# Total nectar collected: %.2f\n", sim->total_nectar_collected);
    fprintf(f, "# Timesteps: %d\n\n", sim->timestep);

    fprintf(f, "# Flower positions and remaining nectar:\n");
    for (int i = 0; i < NUM_FLOWERS; i++)
    {
        fprintf(f, "Flower %d: (%.2f, %.2f) nectar=%.2f/%.2f\n",
                i, sim->flowers[i].position.x, sim->flowers[i].position.y,
                sim->flowers[i].nectar_available, sim->flowers[i].nectar_total);
    }

    fclose(f);
    printf("Results saved to %s\n", filename);
}

int main(int argc, char **argv)
{
    int num_threads = 4;
    if (argc > 1)
    {
        num_threads = atoi(argv[1]);
    }
    omp_set_num_threads(num_threads);

    printf("=== Bee Foraging Simulation (OpenMP) ===\n");
    printf("Configuration:\n");
    printf("  Threads: %d\n", num_threads);
    printf("  World size: %.0fx%.0f\n", WORLD_SIZE, WORLD_SIZE);
    printf("  Bees: %d (%.0f%% scouts)\n", NUM_BEES, SCOUT_RATIO * 100);
    printf("  Flowers: %d\n", NUM_FLOWERS);
    printf("  Timesteps: %d\n\n", MAX_TIMESTEPS);

    Simulation *sim = create_simulation();

    unsigned int *seeds = (unsigned int *)malloc(num_threads * sizeof(unsigned int));
    for (int i = 0; i < num_threads; i++)
    {
        seeds[i] = i * 1000;
    }

    omp_lock_t dance_lock;
    omp_init_lock(&dance_lock);

    double start = omp_get_wtime();

    for (int t = 0; t < MAX_TIMESTEPS; t++)
    {
        simulation_step(sim, seeds, &dance_lock);

        // if (t % 1000 == 0)
        // {
        //     print_statistics(sim);
        // }
    }

    double end = omp_get_wtime();
    double elapsed = end - start;

    printf("\n=== Final Results ===\n");
    printf("Total nectar collected: %.2f\n", sim->total_nectar_collected);
    printf("Execution time: %.3f seconds\n", elapsed);
    printf("Throughput: %.2f timesteps/sec\n", MAX_TIMESTEPS / elapsed);

    // save_results(sim, "results_openmp.txt");

    omp_destroy_lock(&dance_lock);
    free(seeds);
    destroy_simulation(sim);

    return 0;
}