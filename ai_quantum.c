/*
===========================================================================
QUANTUM DECISION ENGINE
Superposition-based decision making for ultimate AI performance
===========================================================================
*/

#include "g_local.h"
#include "ai_main.h"

#define QUANTUM_STATES 64
#define QUANTUM_DECISIONS 16

typedef struct {
    float amplitude[QUANTUM_STATES];
    float phase[QUANTUM_STATES];
    float probability[QUANTUM_STATES];
    float entanglement[QUANTUM_STATES][QUANTUM_STATES];
    int collapsed_state;
    float coherence;
} quantumState_t;

static quantumState_t botQuantumStates[MAX_CLIENTS];

/*
====================
AI_InitQuantumState
====================
*/
void AI_InitQuantumState(int botnum) {
    int i, j;
    quantumState_t *qs = &botQuantumStates[botnum];
    
    // Initialize in superposition
    float norm = 1.0f / sqrt(QUANTUM_STATES);
    for (i = 0; i < QUANTUM_STATES; i++) {
        qs->amplitude[i] = norm;
        qs->phase[i] = ((float)rand() / RAND_MAX) * 2.0f * M_PI;
        qs->probability[i] = norm * norm;
        
        for (j = 0; j < QUANTUM_STATES; j++) {
            qs->entanglement[i][j] = (i == j) ? 1.0f : 0.0f;
        }
    }
    
    qs->collapsed_state = -1;
    qs->coherence = 1.0f;
}

/*
====================
AI_QuantumRotation
Apply quantum rotation to state
====================
*/
void AI_QuantumRotation(quantumState_t *qs, float theta, float phi) {
    int i;
    float cosTheta = cos(theta);
    float sinTheta = sin(theta);
    
    for (i = 0; i < QUANTUM_STATES; i++) {
        float oldAmp = qs->amplitude[i];
        float oldPhase = qs->phase[i];
        
        qs->amplitude[i] = oldAmp * cosTheta;
        qs->phase[i] = oldPhase + phi;
        
        // Normalize phase
        while (qs->phase[i] > M_PI) qs->phase[i] -= 2.0f * M_PI;
        while (qs->phase[i] < -M_PI) qs->phase[i] += 2.0f * M_PI;
    }
}

/*
====================
AI_QuantumEntangle
Create quantum entanglement between states
====================
*/
void AI_QuantumEntangle(int botnum1, int botnum2, float strength) {
    quantumState_t *qs1 = &botQuantumStates[botnum1];
    quantumState_t *qs2 = &botQuantumStates[botnum2];
    int i, j;
    
    for (i = 0; i < QUANTUM_STATES; i++) {
        for (j = 0; j < QUANTUM_STATES; j++) {
            float entangle = strength * qs1->amplitude[i] * qs2->amplitude[j];
            qs1->entanglement[i][j] += entangle;
            qs2->entanglement[i][j] += entangle;
        }
    }
}

/*
====================
AI_QuantumMeasure
Collapse quantum state through measurement
====================
*/
int AI_QuantumMeasure(int botnum) {
    quantumState_t *qs = &botQuantumStates[botnum];
    int i;
    float cumulative = 0.0f;
    float random = (float)rand() / RAND_MAX;
    
    // Update probabilities
    float norm = 0.0f;
    for (i = 0; i < QUANTUM_STATES; i++) {
        qs->probability[i] = qs->amplitude[i] * qs->amplitude[i];
        norm += qs->probability[i];
    }
    
    // Normalize
    for (i = 0; i < QUANTUM_STATES; i++) {
        qs->probability[i] /= norm;
    }
    
    // Collapse through measurement
    for (i = 0; i < QUANTUM_STATES; i++) {
        cumulative += qs->probability[i];
        if (random < cumulative) {
            qs->collapsed_state = i;
            qs->coherence *= 0.95f; // Decoherence
            return i;
        }
    }
    
    return QUANTUM_STATES - 1;
}

/*
====================
AI_QuantumSuperposition
Evolve quantum state based on game situation
====================
*/
void AI_QuantumSuperposition(int botnum, bot_state_t *bs) {
    quantumState_t *qs = &botQuantumStates[botnum];
    int i;
    
    // Hamiltonian evolution
    float dt = 0.01f;
    float energy[QUANTUM_STATES];
    
    // Calculate energy for each state
    for (i = 0; i < QUANTUM_STATES; i++) {
        energy[i] = 0.0f;
        
        // Health energy
        energy[i] += (200.0f - bs->inventory[INVENTORY_HEALTH]) * 0.01f;
        
        // Weapon energy
        energy[i] += bs->weaponnum * 0.1f;
        
        // Enemy proximity energy
        if (bs->enemy >= 0) {
            gentity_t *enemy = &g_entities[bs->enemy];
            vec3_t dir;
            VectorSubtract(enemy->r.currentOrigin, bs->origin, dir);
            float dist = VectorLength(dir);
            energy[i] += 1000.0f / (dist + 1.0f);
        }
        
        // Position energy
        energy[i] += (bs->origin[2] / 1000.0f);
    }
    
    // Time evolution
    for (i = 0; i < QUANTUM_STATES; i++) {
        qs->phase[i] += energy[i] * dt;
        qs->amplitude[i] *= (1.0f - 0.001f * dt); // Small damping
    }
    
    // Renormalize
    float norm = 0.0f;
    for (i = 0; i < QUANTUM_STATES; i++) {
        norm += qs->amplitude[i] * qs->amplitude[i];
    }
    norm = sqrt(norm);
    for (i = 0; i < QUANTUM_STATES; i++) {
        qs->amplitude[i] /= norm;
    }
    
    // Restore coherence
    qs->coherence += 0.01f * (1.0f - qs->coherence);
}

/*
====================
AI_QuantumDecision
Make decision based on quantum measurement
====================
*/
int AI_QuantumDecision(int botnum, bot_state_t *bs) {
    AI_QuantumSuperposition(botnum, bs);
    int state = AI_QuantumMeasure(botnum);
    
    // Map quantum state to decision
    return state % QUANTUM_DECISIONS;
}

/*
====================
AI_GetQuantumCoherence
Get current quantum coherence level
====================
*/
float AI_GetQuantumCoherence(int botnum) {
    return botQuantumStates[botnum].coherence;
}
