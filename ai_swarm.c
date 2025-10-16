/*
===========================================================================
SWARM INTELLIGENCE SYSTEM
Collective behavior and emergent intelligence for team coordination
===========================================================================
*/

#include "g_local.h"
#include "ai_main.h"

#define MAX_SWARM_SIZE 16
#define SWARM_RANGE 2000.0f
#define COHESION_WEIGHT 0.3f
#define SEPARATION_WEIGHT 0.5f
#define ALIGNMENT_WEIGHT 0.2f
#define PHEROMONE_DECAY 0.95f

typedef struct {
    vec3_t position;
    vec3_t velocity;
    int botnum;
    float fitness;
    int role;
} swarmAgent_t;

typedef struct {
    vec3_t position;
    float strength;
    int type;
    int timestamp;
} pheromone_t;

static swarmAgent_t swarmAgents[MAX_CLIENTS];
static pheromone_t pheromones[1024];
static int numPheromones = 0;

/*
====================
AI_SwarmInit
====================
*/
void AI_SwarmInit(int botnum) {
    swarmAgent_t *agent = &swarmAgents[botnum];
    agent->botnum = botnum;
    agent->fitness = 1.0f;
    agent->role = rand() % 4;
    VectorClear(agent->position);
    VectorClear(agent->velocity);
}

/*
====================
AI_SwarmGetNeighbors
====================
*/
int AI_SwarmGetNeighbors(int botnum, int *neighbors, int maxNeighbors) {
    swarmAgent_t *agent = &swarmAgents[botnum];
    int count = 0;
    int i;
    
    for (i = 0; i < MAX_CLIENTS && count < maxNeighbors; i++) {
        if (i == botnum) continue;
        if (!g_entities[i].inuse) continue;
        if (!g_entities[i].client) continue;
        
        swarmAgent_t *other = &swarmAgents[i];
        vec3_t diff;
        VectorSubtract(agent->position, other->position, diff);
        float dist = VectorLength(diff);
        
        if (dist < SWARM_RANGE) {
            neighbors[count++] = i;
        }
    }
    
    return count;
}

/*
====================
AI_SwarmCohesion
Calculate cohesion force (move toward center of neighbors)
====================
*/
void AI_SwarmCohesion(int botnum, int *neighbors, int numNeighbors, vec3_t force) {
    if (numNeighbors == 0) {
        VectorClear(force);
        return;
    }
    
    swarmAgent_t *agent = &swarmAgents[botnum];
    vec3_t center;
    int i;
    
    VectorClear(center);
    for (i = 0; i < numNeighbors; i++) {
        VectorAdd(center, swarmAgents[neighbors[i]].position, center);
    }
    VectorScale(center, 1.0f / numNeighbors, center);
    
    VectorSubtract(center, agent->position, force);
    VectorScale(force, COHESION_WEIGHT, force);
}

/*
====================
AI_SwarmSeparation
Calculate separation force (avoid crowding neighbors)
====================
*/
void AI_SwarmSeparation(int botnum, int *neighbors, int numNeighbors, vec3_t force) {
    swarmAgent_t *agent = &swarmAgents[botnum];
    int i;
    
    VectorClear(force);
    for (i = 0; i < numNeighbors; i++) {
        swarmAgent_t *other = &swarmAgents[neighbors[i]];
        vec3_t diff;
        VectorSubtract(agent->position, other->position, diff);
        float dist = VectorLength(diff);
        
        if (dist > 0.1f) {
            VectorNormalize(diff);
            VectorScale(diff, 1.0f / dist, diff);
            VectorAdd(force, diff, force);
        }
    }
    
    VectorScale(force, SEPARATION_WEIGHT, force);
}

/*
====================
AI_SwarmAlignment
Calculate alignment force (match velocity of neighbors)
====================
*/
void AI_SwarmAlignment(int botnum, int *neighbors, int numNeighbors, vec3_t force) {
    if (numNeighbors == 0) {
        VectorClear(force);
        return;
    }
    
    swarmAgent_t *agent = &swarmAgents[botnum];
    vec3_t avgVelocity;
    int i;
    
    VectorClear(avgVelocity);
    for (i = 0; i < numNeighbors; i++) {
        VectorAdd(avgVelocity, swarmAgents[neighbors[i]].velocity, avgVelocity);
    }
    VectorScale(avgVelocity, 1.0f / numNeighbors, avgVelocity);
    
    VectorSubtract(avgVelocity, agent->velocity, force);
    VectorScale(force, ALIGNMENT_WEIGHT, force);
}

/*
====================
AI_SwarmUpdate
====================
*/
void AI_SwarmUpdate(int botnum, bot_state_t *bs) {
    swarmAgent_t *agent = &swarmAgents[botnum];
    int neighbors[MAX_SWARM_SIZE];
    int numNeighbors;
    vec3_t cohesion, separation, alignment, totalForce;
    
    // Update position and velocity from bot state
    VectorCopy(bs->origin, agent->position);
    VectorCopy(bs->velocity, agent->velocity);
    
    // Get neighbors
    numNeighbors = AI_SwarmGetNeighbors(botnum, neighbors, MAX_SWARM_SIZE);
    
    // Calculate forces
    AI_SwarmCohesion(botnum, neighbors, numNeighbors, cohesion);
    AI_SwarmSeparation(botnum, neighbors, numNeighbors, separation);
    AI_SwarmAlignment(botnum, neighbors, numNeighbors, alignment);
    
    // Combine forces
    VectorClear(totalForce);
    VectorAdd(totalForce, cohesion, totalForce);
    VectorAdd(totalForce, separation, totalForce);
    VectorAdd(totalForce, alignment, totalForce);
    
    // Apply force to desired velocity
    VectorAdd(bs->velocity, totalForce, bs->velocity);
    
    // Update fitness based on performance
    agent->fitness *= 0.999f;
    if (bs->enemy >= 0) {
        agent->fitness += 0.01f;
    }
}

/*
====================
AI_SwarmDepositPheromone
====================
*/
void AI_SwarmDepositPheromone(vec3_t position, int type, float strength) {
    if (numPheromones >= 1024) {
        // Remove oldest pheromone
        int oldest = 0;
        int oldestTime = pheromones[0].timestamp;
        int i;
        
        for (i = 1; i < 1024; i++) {
            if (pheromones[i].timestamp < oldestTime) {
                oldestTime = pheromones[i].timestamp;
                oldest = i;
            }
        }
        numPheromones = oldest;
    }
    
    pheromone_t *p = &pheromones[numPheromones++];
    VectorCopy(position, p->position);
    p->type = type;
    p->strength = strength;
    p->timestamp = level.time;
}

/*
====================
AI_SwarmFollowPheromones
====================
*/
void AI_SwarmFollowPheromones(int botnum, vec3_t position, int type, vec3_t direction) {
    float maxStrength = 0.0f;
    vec3_t bestDir;
    int i;
    
    VectorClear(bestDir);
    
    for (i = 0; i < numPheromones; i++) {
        pheromone_t *p = &pheromones[i];
        
        if (p->type != type) continue;
        
        vec3_t diff;
        VectorSubtract(p->position, position, diff);
        float dist = VectorLength(diff);
        
        if (dist < 1.0f) continue;
        
        // Strength decreases with distance
        float strength = p->strength / (dist * 0.01f);
        
        if (strength > maxStrength) {
            maxStrength = strength;
            VectorCopy(diff, bestDir);
        }
    }
    
    if (maxStrength > 0.0f) {
        VectorNormalize(bestDir);
        VectorCopy(bestDir, direction);
    } else {
        VectorClear(direction);
    }
    
    // Decay pheromones
    for (i = 0; i < numPheromones; i++) {
        pheromones[i].strength *= PHEROMONE_DECAY;
    }
}

/*
====================
AI_SwarmGetRole
====================
*/
int AI_SwarmGetRole(int botnum) {
    return swarmAgents[botnum].role;
}

/*
====================
AI_SwarmSetRole
====================
*/
void AI_SwarmSetRole(int botnum, int role) {
    swarmAgents[botnum].role = role;
}

/*
====================
AI_SwarmGetFitness
====================
*/
float AI_SwarmGetFitness(int botnum) {
    return swarmAgents[botnum].fitness;
}
