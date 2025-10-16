/*
===========================================================================
GENETIC ALGORITHM SYSTEM
Evolution of bot behavior through natural selection
===========================================================================
*/

#include "g_local.h"
#include "ai_main.h"

#define GENE_COUNT 128
#define POPULATION_SIZE 32
#define MUTATION_RATE 0.05f
#define CROSSOVER_RATE 0.7f
#define ELITE_COUNT 4

typedef struct {
    float genes[GENE_COUNT];
    float fitness;
    int generation;
    int wins;
    int losses;
} genome_t;

static genome_t population[POPULATION_SIZE];
static genome_t nextGeneration[POPULATION_SIZE];
static int currentGeneration = 0;

/*
====================
AI_GeneticInit
====================
*/
void AI_GeneticInit(void) {
    int i, j;
    
    for (i = 0; i < POPULATION_SIZE; i++) {
        genome_t *g = &population[i];
        
        for (j = 0; j < GENE_COUNT; j++) {
            g->genes[j] = ((float)rand() / RAND_MAX) * 2.0f - 1.0f;
        }
        
        g->fitness = 0.0f;
        g->generation = 0;
        g->wins = 0;
        g->losses = 0;
    }
}

/*
====================
AI_GeneticEvaluate
====================
*/
void AI_GeneticEvaluate(int genomeIndex, float performance) {
    genome_t *g = &population[genomeIndex % POPULATION_SIZE];
    
    g->fitness += performance;
    
    if (performance > 0.0f) {
        g->wins++;
    } else {
        g->losses++;
    }
}

/*
====================
AI_GeneticSelect
Tournament selection
====================
*/
int AI_GeneticSelect(void) {
    int tournament[4];
    int i, best;
    float bestFitness;
    
    // Random tournament
    for (i = 0; i < 4; i++) {
        tournament[i] = rand() % POPULATION_SIZE;
    }
    
    // Find best in tournament
    best = tournament[0];
    bestFitness = population[best].fitness;
    
    for (i = 1; i < 4; i++) {
        if (population[tournament[i]].fitness > bestFitness) {
            best = tournament[i];
            bestFitness = population[best].fitness;
        }
    }
    
    return best;
}

/*
====================
AI_GeneticCrossover
Two-point crossover
====================
*/
void AI_GeneticCrossover(genome_t *parent1, genome_t *parent2, genome_t *child) {
    int point1 = rand() % GENE_COUNT;
    int point2 = rand() % GENE_COUNT;
    int i;
    
    if (point1 > point2) {
        int temp = point1;
        point1 = point2;
        point2 = temp;
    }
    
    for (i = 0; i < GENE_COUNT; i++) {
        if (i < point1 || i >= point2) {
            child->genes[i] = parent1->genes[i];
        } else {
            child->genes[i] = parent2->genes[i];
        }
    }
    
    child->fitness = 0.0f;
    child->generation = currentGeneration + 1;
    child->wins = 0;
    child->losses = 0;
}

/*
====================
AI_GeneticMutate
====================
*/
void AI_GeneticMutate(genome_t *genome) {
    int i;
    
    for (i = 0; i < GENE_COUNT; i++) {
        if (((float)rand() / RAND_MAX) < MUTATION_RATE) {
            // Gaussian mutation
            float mutation = ((float)rand() / RAND_MAX - 0.5f) * 0.2f;
            genome->genes[i] += mutation;
            
            // Clamp to [-1, 1]
            if (genome->genes[i] > 1.0f) genome->genes[i] = 1.0f;
            if (genome->genes[i] < -1.0f) genome->genes[i] = -1.0f;
        }
    }
}

/*
====================
AI_GeneticEvolve
Create next generation
====================
*/
void AI_GeneticEvolve(void) {
    int i, j;
    genome_t *sorted[POPULATION_SIZE];
    
    // Sort by fitness
    for (i = 0; i < POPULATION_SIZE; i++) {
        sorted[i] = &population[i];
    }
    
    for (i = 0; i < POPULATION_SIZE - 1; i++) {
        for (j = i + 1; j < POPULATION_SIZE; j++) {
            if (sorted[i]->fitness < sorted[j]->fitness) {
                genome_t *temp = sorted[i];
                sorted[i] = sorted[j];
                sorted[j] = temp;
            }
        }
    }
    
    // Elitism - keep best performers
    for (i = 0; i < ELITE_COUNT; i++) {
        memcpy(&nextGeneration[i], sorted[i], sizeof(genome_t));
    }
    
    // Generate rest through crossover and mutation
    for (i = ELITE_COUNT; i < POPULATION_SIZE; i++) {
        if (((float)rand() / RAND_MAX) < CROSSOVER_RATE) {
            int parent1 = AI_GeneticSelect();
            int parent2 = AI_GeneticSelect();
            AI_GeneticCrossover(&population[parent1], &population[parent2], &nextGeneration[i]);
        } else {
            memcpy(&nextGeneration[i], &population[AI_GeneticSelect()], sizeof(genome_t));
        }
        
        AI_GeneticMutate(&nextGeneration[i]);
    }
    
    // Replace population
    memcpy(population, nextGeneration, sizeof(population));
    currentGeneration++;
}

/*
====================
AI_GeneticGetGene
====================
*/
float AI_GeneticGetGene(int genomeIndex, int geneIndex) {
    if (genomeIndex < 0 || genomeIndex >= POPULATION_SIZE) return 0.0f;
    if (geneIndex < 0 || geneIndex >= GENE_COUNT) return 0.0f;
    
    return population[genomeIndex].genes[geneIndex];
}

/*
====================
AI_GeneticGetBestGenome
====================
*/
int AI_GeneticGetBestGenome(void) {
    int best = 0;
    float bestFitness = population[0].fitness;
    int i;
    
    for (i = 1; i < POPULATION_SIZE; i++) {
        if (population[i].fitness > bestFitness) {
            best = i;
            bestFitness = population[i].fitness;
        }
    }
    
    return best;
}

/*
====================
AI_GeneticGetGeneration
====================
*/
int AI_GeneticGetGeneration(void) {
    return currentGeneration;
}

/*
====================
AI_GeneticSave
====================
*/
void AI_GeneticSave(const char *filename) {
    fileHandle_t f;
    
    trap_FS_FOpenFile(filename, &f, FS_WRITE);
    if (!f) return;
    
    trap_FS_Write(&currentGeneration, sizeof(int), f);
    trap_FS_Write(population, sizeof(population), f);
    trap_FS_FCloseFile(f);
}

/*
====================
AI_GeneticLoad
====================
*/
void AI_GeneticLoad(const char *filename) {
    fileHandle_t f;
    
    trap_FS_FOpenFile(filename, &f, FS_READ);
    if (!f) {
        AI_GeneticInit();
        return;
    }
    
    trap_FS_Read(&currentGeneration, sizeof(int), f);
    trap_FS_Read(population, sizeof(population), f);
    trap_FS_FCloseFile(f);
}
