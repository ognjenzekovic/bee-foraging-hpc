#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <string.h>
#include <time.h>
#include <mpi.h>
#include "config.h"
#include "types.h"

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

void init_all_bees(Bee *bees, int num_bees, unsigned int *seed)
{
    for (int i = 0; i < num_bees; i++)
    {
        bees[i].id = i;
        bees[i].position.x = HIVE_X + random_float_r(seed, -HIVE_RADIUS, HIVE_RADIUS);
        bees[i].position.y = HIVE_Y + random_float_r(seed, -HIVE_RADIUS, HIVE_RADIUS);
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

void init_flowers(Flower *flowers, int num_flowers, unsigned int *seed)
{
    for (int i = 0; i < num_flowers; i++)
    {
        flowers[i].position = random_position_r(seed);
        flowers[i].nectar_total = FLOWER_NECTAR_MAX;
        flowers[i].nectar_available = FLOWER_NECTAR_MAX;
        flowers[i].bees_feeding = 0;
        flowers[i].capacity = FLOWER_CAPACITY;
    }
}

Simulation *create_simulation(int rank, int size)
{
    Simulation *sim = (Simulation *)malloc(sizeof(Simulation));

    sim->bees = (Bee *)malloc(NUM_BEES * sizeof(Bee));
    sim->flowers = (Flower *)malloc(NUM_FLOWERS * sizeof(Flower));
    sim->dances = (WaggleDance *)malloc(NUM_BEES * sizeof(WaggleDance));
    sim->num_dances = 0;
    sim->total_nectar_collected = 0;
    sim->timestep = 0;

    int bees_per_proc = NUM_BEES / size;
    int remainder = NUM_BEES % size;

    sim->bee_offset = rank * bees_per_proc + (rank < remainder ? rank : remainder);
    sim->num_local_bees = bees_per_proc + (rank < remainder ? 1 : 0);

    unsigned int seed = 42;
    init_all_bees(sim->bees, NUM_BEES, &seed);

    seed = 123;
    init_flowers(sim->flowers, NUM_FLOWERS, &seed);

    return sim;
}

void destroy_simulation(Simulation *sim)
{
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

void create_dance(Simulation *sim, Bee *bee)
{
    WaggleDance dance;
    dance.bee_id = bee->id;
    dance.flower_location = sim->flowers[bee->target_flower].position;
    dance.nectar_quality = bee->nectar_found / FLOWER_NECTAR_MAX;

    Vector2D hive_pos = {HIVE_X, HIVE_Y};
    dance.distance_from_hive = distance(hive_pos, dance.flower_location);
    dance.followers = 0;

    sim->dances[sim->num_dances] = dance;
    sim->num_dances++;
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

void idle_bees_watch_dances(Simulation *sim, unsigned int *seed)
{
    for (int i = sim->bee_offset; i < sim->bee_offset + sim->num_local_bees; i++)
    {
        Bee *bee = &sim->bees[i];

        if (bee->state == IDLE && sim->num_dances > 0)
        {
            if (random_float_r(seed, 0, 1) < DECISION_PROBABILITY)
            {
                int chosen_dance = choose_dance(sim, seed);

                if (chosen_dance >= 0)
                {
                    bee->following_dance = chosen_dance;
                    bee->state = FOLLOWER;
                    sim->dances[chosen_dance].followers++;
                    bee->target_location = sim->dances[chosen_dance].flower_location;

                    int dancer_id = sim->dances[chosen_dance].bee_id;
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

    if (flower->bees_feeding < flower->capacity && flower->nectar_available > 0)
    {
        flower->bees_feeding++;

        float collected = fminf(10.0f, flower->nectar_available);
        flower->nectar_available -= collected;

        *local_nectar += collected;
        bee->energy = fminf(MAX_ENERGY, bee->energy + collected * 0.5f);

        flower->bees_feeding--;

        bee->state = RETURNING;
        bee->target_flower = -1;
        bee->following_dance = -1;
    }
    else
    {
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

MPI_Datatype create_bee_type()
{
    MPI_Datatype bee_type;

    MPI_Type_contiguous(sizeof(Bee), MPI_BYTE, &bee_type);
    MPI_Type_commit(&bee_type);

    return bee_type;
}

void sync_bees(Simulation *sim, int rank, int size, MPI_Datatype bee_type)
{
    int *recvcounts = (int *)malloc(size * sizeof(int));
    int *displacements = (int *)malloc(size * sizeof(int));

    int bees_per_proc = NUM_BEES / size;
    int remainder = NUM_BEES % size;

    int offset = 0;
    for (int i = 0; i < size; i++)
    {
        recvcounts[i] = bees_per_proc + (i < remainder ? 1 : 0);
        displacements[i] = offset;
        offset += recvcounts[i];
    }

    MPI_Allgatherv(
        sim->bees + sim->bee_offset,
        sim->num_local_bees,
        bee_type,
        sim->bees,
        recvcounts,
        displacements,
        bee_type,
        MPI_COMM_WORLD);

    free(recvcounts);
    free(displacements);
}

void sync_flowers(Simulation *sim, int rank, int size)
{
    float *local_nectar = (float *)malloc(NUM_FLOWERS * sizeof(float));
    float *global_nectar = (float *)malloc(NUM_FLOWERS * sizeof(float));

    for (int i = 0; i < NUM_FLOWERS; i++)
    {
        local_nectar[i] = sim->flowers[i].nectar_available;
    }

    MPI_Allreduce(local_nectar, global_nectar, NUM_FLOWERS, MPI_FLOAT, MPI_MIN, MPI_COMM_WORLD);

    for (int i = 0; i < NUM_FLOWERS; i++)
    {
        sim->flowers[i].nectar_available = global_nectar[i];
    }

    free(local_nectar);
    free(global_nectar);
}

void sync_dances(Simulation *sim, int rank, int size)
{
    int *dance_counts = (int *)malloc(size * sizeof(int));
    MPI_Allgather(&sim->num_dances, 1, MPI_INT, dance_counts, 1, MPI_INT, MPI_COMM_WORLD);

    int *displacements = (int *)malloc(size * sizeof(int));
    int total_dances = 0;
    for (int i = 0; i < size; i++)
    {
        displacements[i] = total_dances;
        total_dances += dance_counts[i];
    }

    if (total_dances == 0)
    {
        sim->num_dances = 0;
        free(dance_counts);
        free(displacements);
        return;
    }

    WaggleDance *all_dances = (WaggleDance *)malloc(total_dances * sizeof(WaggleDance));

    int *byte_counts = (int *)malloc(size * sizeof(int));
    int *byte_displacements = (int *)malloc(size * sizeof(int));
    for (int i = 0; i < size; i++)
    {
        byte_counts[i] = dance_counts[i] * sizeof(WaggleDance);
        byte_displacements[i] = displacements[i] * sizeof(WaggleDance);
    }

    MPI_Allgatherv(
        sim->dances,
        sim->num_dances * sizeof(WaggleDance),
        MPI_BYTE,
        all_dances,
        byte_counts,
        byte_displacements,
        MPI_BYTE,
        MPI_COMM_WORLD);

    memcpy(sim->dances, all_dances, total_dances * sizeof(WaggleDance));
    sim->num_dances = total_dances;

    free(dance_counts);
    free(displacements);
    free(byte_counts);
    free(byte_displacements);
    free(all_dances);
}

void update_local_bees(Simulation *sim, unsigned int *seed, float *local_nectar)
{
    for (int i = sim->bee_offset; i < sim->bee_offset + sim->num_local_bees; i++)
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
                create_dance(sim, bee);
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
            foraging_behavior(bee, sim, local_nectar);
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
}

void update_flowers(Simulation *sim)
{
    for (int i = 0; i < NUM_FLOWERS; i++)
    {
        if (sim->flowers[i].nectar_available < sim->flowers[i].nectar_total)
        {
            sim->flowers[i].nectar_available += NECTAR_REGEN_RATE;
            if (sim->flowers[i].nectar_available > sim->flowers[i].nectar_total)
            {
                sim->flowers[i].nectar_available = sim->flowers[i].nectar_total;
            }
        }
    }
}

void simulation_step(Simulation *sim, int rank, int size, MPI_Datatype bee_type, unsigned int *seed)
{
    float local_nectar = 0.0f;

    update_local_bees(sim, seed, &local_nectar);

    sync_bees(sim, rank, size, bee_type);

    sync_dances(sim, rank, size);

    idle_bees_watch_dances(sim, seed);

    sync_bees(sim, rank, size, bee_type);

    sync_flowers(sim, rank, size);

    if (rank == 0)
    {
        update_flowers(sim);
    }
    MPI_Bcast(sim->flowers, NUM_FLOWERS * sizeof(Flower), MPI_BYTE, 0, MPI_COMM_WORLD);

    float global_nectar;
    MPI_Allreduce(&local_nectar, &global_nectar, 1, MPI_FLOAT, MPI_SUM, MPI_COMM_WORLD);
    sim->total_nectar_collected += global_nectar;

    sim->num_dances = 0;
    sim->timestep++;
}

void print_statistics(Simulation *sim, int rank)
{
    if (rank != 0)
        return;

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

int main(int argc, char **argv)
{
    MPI_Init(&argc, &argv);

    int rank, size;
    MPI_Comm_rank(MPI_COMM_WORLD, &rank);
    MPI_Comm_size(MPI_COMM_WORLD, &size);

    if (rank == 0)
    {
        printf("=== Bee Foraging Simulation (MPI) ===\n");
        printf("Configuration:\n");
        printf("  MPI Processes: %d\n", size);
        printf("  World size: %.0fx%.0f\n", WORLD_SIZE, WORLD_SIZE);
        printf("  Bees: %d (%.0f%% scouts)\n", NUM_BEES, SCOUT_RATIO * 100);
        printf("  Flowers: %d\n", NUM_FLOWERS);
        printf("  Timesteps: %d\n\n", MAX_TIMESTEPS);
    }

    Simulation *sim = create_simulation(rank, size);

    unsigned int seed = rank * 1000;

    MPI_Datatype bee_type = create_bee_type();

    MPI_Barrier(MPI_COMM_WORLD);
    double start = MPI_Wtime();

    for (int t = 0; t < MAX_TIMESTEPS; t++)
    {
        simulation_step(sim, rank, size, bee_type, &seed);

        // if (t % 1000 == 0)
        // {
        //     print_statistics(sim, rank);
        // }
    }

    MPI_Barrier(MPI_COMM_WORLD);
    double end = MPI_Wtime();
    double elapsed = end - start;

    if (rank == 0)
    {
        printf("\n=== Final Results ===\n");
        printf("Total nectar collected: %.2f\n", sim->total_nectar_collected);
        printf("Execution time: %.3f seconds\n", elapsed);
        printf("Throughput: %.2f timesteps/sec\n", MAX_TIMESTEPS / elapsed);
    }

    MPI_Type_free(&bee_type);
    destroy_simulation(sim);
    MPI_Finalize();

    return 0;
}