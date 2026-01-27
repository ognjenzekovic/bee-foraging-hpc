#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <string.h>
#include <time.h>
#include "types.h"
#include "config.h"

// ============ UTILITY FUNCTIONS ============

float distance(Vector2D a, Vector2D b)
{
    float dx = a.x - b.x;
    float dy = a.y - b.y;
    return sqrtf(dx * dx + dy * dy);
}

float random_float(float min, float max)
{
    return min + (max - min) * ((float)rand() / RAND_MAX);
}

Vector2D random_position()
{
    Vector2D pos;
    pos.x = random_float(0, WORLD_SIZE);
    pos.y = random_float(0, WORLD_SIZE);
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

// ============ INITIALIZATION ============

void init_bees(Bee *bees, int num_bees)
{
    for (int i = 0; i < num_bees; i++)
    {
        bees[i].id = i;
        bees[i].position.x = HIVE_X + random_float(-HIVE_RADIUS, HIVE_RADIUS);
        bees[i].position.y = HIVE_Y + random_float(-HIVE_RADIUS, HIVE_RADIUS);
        bees[i].velocity.x = 0;
        bees[i].velocity.y = 0;

        // 20% scout, ostali idle
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
    for (int i = 0; i < num_flowers; i++)
    {
        flowers[i].position = random_position();
        flowers[i].nectar_total = FLOWER_NECTAR_MAX;
        flowers[i].nectar_available = FLOWER_NECTAR_MAX;
        flowers[i].bees_feeding = 0;
        flowers[i].capacity = FLOWER_CAPACITY;
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
    free(sim->bees);
    free(sim->flowers);
    free(sim->dances);
    free(sim);
}

// ============ MOVEMENT & BEHAVIOR ============

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

void scout_behavior(Bee *bee, Flower *flowers, int num_flowers)
{
    // Random walk dok ne nađe cvet
    if (bee->target_flower == -1)
    {
        if (random_float(0, 1) < 0.5f)
        {
            // 80% vremena: mali koraci (exploration)
            bee->position.x += random_float(-BEE_SPEED, BEE_SPEED);
            bee->position.y += random_float(-BEE_SPEED, BEE_SPEED);
        }
        else
        {
            // 20% vremena: veliki skok (avoid local minima)
            bee->position.x += random_float(-BEE_SPEED * 10, BEE_SPEED * 10);
            bee->position.y += random_float(-BEE_SPEED * 10, BEE_SPEED * 10);
        }

        // printf("Bee %d searching\n", bee->id);

        // Proveri da li je našao cvet
        for (int i = 0; i < num_flowers; i++)
        {
            float dist = distance(bee->position, flowers[i].position);
            if (dist < BEE_VISION_RANGE && flowers[i].nectar_available > 0)
            {
                bee->target_flower = i;
                bee->nectar_found = flowers[i].nectar_available;
                bee->state = RETURNING;
                // if(i<20){
                //     printf("Bee %d found a flower\n", bee->id);
                // }
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

// ============ WAGGLE DANCE ============

void create_dance(Simulation *sim, Bee *bee)
{
    WaggleDance dance;
    dance.bee_id = bee->id;
    dance.flower_location = sim->flowers[bee->target_flower].position;
    dance.nectar_quality = bee->nectar_found / FLOWER_NECTAR_MAX;

    Vector2D hive_pos = {HIVE_X, HIVE_Y};
    dance.distance_from_hive = distance(hive_pos, dance.flower_location);
    dance.followers = 0;

    // Dodaj u listu plesova
    sim->dances[sim->num_dances] = dance;
    sim->num_dances++;
}

float calculate_dance_attractiveness(WaggleDance *dance)
{
    // Što je bliže i kvalitetnije, to bolje
    float quality_factor = dance->nectar_quality * 2.0f;
    float distance_penalty = 1.0f / (1.0f + dance->distance_from_hive / 100.0f);
    float follower_bonus = 1.0f + (dance->followers * 0.1f);

    return quality_factor * distance_penalty * follower_bonus;
}

int choose_dance(Simulation *sim)
{
    if (sim->num_dances == 0)
        return -1;

    // Izračunaj ukupan score
    float total_score = 0.0f;
    for (int i = 0; i < sim->num_dances; i++)
    {
        total_score += calculate_dance_attractiveness(&sim->dances[i]);
    }

    if (total_score < 0.0001f)
        return -1;

    // Probabilistički izbor (roulette wheel selection)
    float random_val = random_float(0, total_score);
    float cumulative = 0.0f;

    for (int i = 0; i < sim->num_dances; i++)
    {
        cumulative += calculate_dance_attractiveness(&sim->dances[i]);
        if (random_val <= cumulative)
        {
            return i;
        }
    }

    return sim->num_dances - 1; // Fallback
}

void idle_bees_watch_dances(Simulation *sim)
{
    for (int i = 0; i < NUM_BEES; i++)
    {
        Bee *bee = &sim->bees[i];

        if (bee->state == IDLE && sim->num_dances > 0)
        {
            // Odluči da li da se pridruži plesu
            if (random_float(0, 1) < DECISION_PROBABILITY)
            {
                int chosen_dance = choose_dance(sim);

                if (chosen_dance >= 0)
                {
                    bee->following_dance = chosen_dance;
                    bee->state = FOLLOWER;

                    // Povećaj broj followers za taj ples
                    sim->dances[chosen_dance].followers++;
                    // cuva lokaciju cveta
                    bee->target_location = sim->dances[chosen_dance].flower_location;

                    // Pronađi pčelu koja pleše i ažuriraj je
                    int dancer_id = sim->dances[chosen_dance].bee_id;
                    sim->bees[dancer_id].dance_followers++;
                }
            }
        }
    }
}

// ============ FOLLOWER BEHAVIOR ============

void follower_behavior(Bee *bee, Simulation *sim)
{
    Vector2D target = bee->target_location;

    float dist = distance(bee->position, target);

    // Kreni ka cilju
    move_towards(bee, target);

    // Proveri da li je stigao
    if (dist < 10.0f)
    {
        bee->state = FORAGING;

        // Pronađi cvet
        for (int i = 0; i < NUM_FLOWERS; i++)
        {
            float flower_dist = distance(sim->flowers[i].position, target);
            if (flower_dist < 20.0f)
            {
                bee->target_flower = i;
                break;
            }
        }

        // Ako nije našao cvet, vrati se
        if (bee->target_flower == -1)
        {
            bee->state = RETURNING;
        }
    }

    // Provera energije - ako je niska, vrati se
    if (bee->energy < MAX_ENERGY * 0.2f)
    {
        bee->state = RETURNING;
        bee->target_flower = -1;
    }
}

// ============ FORAGING BEHAVIOR ============

void foraging_behavior(Bee *bee, Simulation *sim)
{
    if (bee->target_flower < 0)
    {
        bee->state = IDLE;
        return;
    }

    Flower *flower = &sim->flowers[bee->target_flower];

    // Proveri da li može da pristupa cvetu
    if (flower->bees_feeding < flower->capacity && flower->nectar_available > 0)
    {
        // Uspešno pristupa
        flower->bees_feeding++;

        // Skuplja nektar (instant za sada, može se dodati timer)
        float collected = fminf(10.0f, flower->nectar_available);
        flower->nectar_available -= collected;

        sim->total_nectar_collected += collected;
        bee->energy = fminf(MAX_ENERGY, bee->energy + collected * 0.5f);

        // Završio je, vraća se kući
        flower->bees_feeding--;
        bee->state = RETURNING;
        bee->target_flower = -1;
        bee->following_dance = -1;
    }
    else
    {
        // Cvet je pun ili prazan

        // Ako je follower, vraća se
        if (bee->following_dance >= 0)
        {
            bee->state = RETURNING;
            bee->target_flower = -1;
            bee->following_dance = -1;
        }
        else
        {
            // Scout nastavlja da traži
            bee->state = SCOUT;
            bee->target_flower = -1;
        }
    }
}

// ============ MAIN UPDATE LOOP ============

void update_bees(Simulation *sim)
{
    // Prvo ažuriraj sve pčele osim onih koje plešu
    for (int i = 0; i < NUM_BEES; i++)
    {
        Bee *bee = &sim->bees[i];

        switch (bee->state)
        {
        case SCOUT:
            scout_behavior(bee, sim->flowers, NUM_FLOWERS);
            break;

        case RETURNING:
            returning_behavior(bee);

            // Ako je upravo stigla u košnicu, kreiraj ples
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
                    // Ima followers, postaje leader i ide nazad
                    bee->state = FORAGING;
                }
                else
                {
                    // Niko ga ne prati, postaje idle
                    bee->state = IDLE;
                    bee->target_flower = -1;
                }
                bee->dance_followers = 0; // Reset
            }
            break;

        case IDLE:
            // Ostaje u košnici
            break;

        case FOLLOWER:
            follower_behavior(bee, sim);
            break;

        case FORAGING:
            foraging_behavior(bee, sim);
            break;
        }

        // Provera energije
        if (bee->energy <= 0)
        {
            Vector2D hive_pos = {HIVE_X, HIVE_Y};
            float dist = distance(bee->position, hive_pos);

            if (dist < HIVE_RADIUS)
            {
                // U košnici, regeneracija
                bee->energy = MAX_ENERGY;
                bee->state = IDLE;
            }
            else
            {
                // Mora da se vrati
                bee->state = RETURNING;
                bee->target_flower = -1;
            }
        }

        // Granice prostora
        if (bee->position.x < 0)
            bee->position.x = 0;
        if (bee->position.x > WORLD_SIZE)
            bee->position.x = WORLD_SIZE;
        if (bee->position.y < 0)
            bee->position.y = 0;
        if (bee->position.y > WORLD_SIZE)
            bee->position.y = WORLD_SIZE;
    }

    // Idle pčele gledaju plesove
    idle_bees_watch_dances(sim);
}

void update_flowers(Simulation *sim)
{
    for (int i = 0; i < NUM_FLOWERS; i++)
    {
        // Regeneracija nektara
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

void simulation_step(Simulation *sim)
{
    update_bees(sim);
    update_flowers(sim);

    // Očisti listu plesova na kraju svakog koraka
    // (novi dance phase počinje)
    sim->num_dances = 0;

    sim->timestep++;
}

// ============ STATISTICS ============

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

    fprintf(f, "# Bee Foraging Simulation Results\n");
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

// ============ CSV EXPORT FOR VISUALIZATION ============

void save_positions_csv(Simulation *sim, int timestep)
{
    static FILE *f = NULL;

    // Otvori fajl samo prvi put
    if (f == NULL)
    {
        f = fopen("positions.csv", "w");
        if (!f)
        {
            printf("Error: Cannot create positions.csv\n");
            return;
        }
        // Header
        fprintf(f, "timestep,type,id,x,y,state,nectar\n");
    }

    // Sačuvaj cvetove
    for (int i = 0; i < NUM_FLOWERS; i++)
    {
        fprintf(f, "%d,flower,%d,%.2f,%.2f,0,%.2f\n",
                timestep, i,
                sim->flowers[i].position.x,
                sim->flowers[i].position.y,
                sim->flowers[i].nectar_available);
    }

    // Sačuvaj pčele
    for (int i = 0; i < NUM_BEES; i++)
    {
        fprintf(f, "%d,bee,%d,%.2f,%.2f,%d,0\n",
                timestep, i,
                sim->bees[i].position.x,
                sim->bees[i].position.y,
                (int)sim->bees[i].state);
    }

    // Zatvori fajl na kraju simulacije
    if (timestep == MAX_TIMESTEPS - 1)
    {
        fclose(f);
        f = NULL;
        printf("Positions saved to positions.csv\n");
    }
}

// ============ MAIN ============

int main(int argc, char **argv)
{
    srand(42); // Fixed seed

    printf("=== Bee Foraging Simulation (Sequential) ===\n");
    printf("Configuration:\n");
    printf("  World size: %.0fx%.0f\n", WORLD_SIZE, WORLD_SIZE);
    printf("  Bees: %d (%.0f%% scouts)\n", NUM_BEES, SCOUT_RATIO * 100);
    printf("  Flowers: %d\n", NUM_FLOWERS);
    printf("  Timesteps: %d\n\n", MAX_TIMESTEPS);

    Simulation *sim = create_simulation();

    // Meri vreme
    clock_t start = clock();

    for (int t = 0; t < MAX_TIMESTEPS; t++)
    {
        simulation_step(sim);

        if (t % 3 == 0 && t > 0 && t < 500)
        {
            save_positions_csv(sim, t);
        }

        // Ispis svakih 100 koraka
        if (t > 200 && t < 300)
        {
            print_statistics(sim);
        }
    }
    // poslednji frejm
    //  save_positions_csv(sim, MAX_TIMESTEPS - 1);

    clock_t end = clock();
    double elapsed = (double)(end - start) / CLOCKS_PER_SEC;

    printf("\n=== Final Results ===\n");
    printf("Total nectar collected: %.2f\n", sim->total_nectar_collected);
    printf("Execution time: %.3f seconds\n", elapsed);
    // printf("Throughput: %.2f timesteps/sec\n", MAX_TIMESTEPS / elapsed);

    save_results(sim, "results_sequential.txt");

    destroy_simulation(sim);
    return 0;
}