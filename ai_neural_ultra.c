/*
===========================================================================
ULTRA-ADVANCED NEURAL DECISION NETWORK
Enhanced Brainworks Ultimate Edition - Neural Core v3.0
===========================================================================
*/

#include "g_local.h"
#include "ai_main.h"
#include "ai_neural_ultra.h"

#define NEURAL_LAYERS 7
#define NEURONS_PER_LAYER 256
#define LEARNING_RATE 0.001f
#define MOMENTUM 0.9f
#define DECAY_RATE 0.99f

typedef struct {
    float weights[NEURONS_PER_LAYER][NEURONS_PER_LAYER];
    float biases[NEURONS_PER_LAYER];
    float activations[NEURONS_PER_LAYER];
    float gradients[NEURONS_PER_LAYER];
    float momentum[NEURONS_PER_LAYER][NEURONS_PER_LAYER];
} neuralLayer_t;

typedef struct {
    neuralLayer_t layers[NEURAL_LAYERS];
    float learningRate;
    float momentum;
    int trainingEpochs;
    float totalError;
    float accuracy;
} neuralNetwork_t;

static neuralNetwork_t botBrains[MAX_CLIENTS];

// Activation functions
float Sigmoid(float x) {
    return 1.0f / (1.0f + exp(-x));
}

float SigmoidDerivative(float x) {
    float sig = Sigmoid(x);
    return sig * (1.0f - sig);
}

float ReLU(float x) {
    return (x > 0.0f) ? x : 0.01f * x; // Leaky ReLU
}

float ReLUDerivative(float x) {
    return (x > 0.0f) ? 1.0f : 0.01f;
}

float Tanh(float x) {
    return tanh(x);
}

float TanhDerivative(float x) {
    float t = tanh(x);
    return 1.0f - t * t;
}

/*
====================
AI_InitNeuralNetwork
====================
*/
void AI_InitNeuralNetwork(int botnum) {
    int i, j, k;
    neuralNetwork_t *nn = &botBrains[botnum];
    
    nn->learningRate = LEARNING_RATE;
    nn->momentum = MOMENTUM;
    nn->trainingEpochs = 0;
    nn->totalError = 0.0f;
    nn->accuracy = 0.5f;
    
    // Initialize weights with Xavier initialization
    for (i = 0; i < NEURAL_LAYERS; i++) {
        float scale = sqrt(2.0f / NEURONS_PER_LAYER);
        for (j = 0; j < NEURONS_PER_LAYER; j++) {
            nn->layers[i].biases[j] = ((float)rand() / RAND_MAX - 0.5f) * 0.1f;
            nn->layers[i].activations[j] = 0.0f;
            nn->layers[i].gradients[j] = 0.0f;
            
            for (k = 0; k < NEURONS_PER_LAYER; k++) {
                nn->layers[i].weights[j][k] = ((float)rand() / RAND_MAX - 0.5f) * scale;
                nn->layers[i].momentum[j][k] = 0.0f;
            }
        }
    }
}

/*
====================
AI_ForwardPropagate
====================
*/
void AI_ForwardPropagate(int botnum, float *inputs, int inputSize) {
    int i, j, k;
    neuralNetwork_t *nn = &botBrains[botnum];
    
    // Set input layer
    for (i = 0; i < inputSize && i < NEURONS_PER_LAYER; i++) {
        nn->layers[0].activations[i] = inputs[i];
    }
    
    // Forward propagate through hidden layers
    for (i = 1; i < NEURAL_LAYERS; i++) {
        for (j = 0; j < NEURONS_PER_LAYER; j++) {
            float sum = nn->layers[i].biases[j];
            
            for (k = 0; k < NEURONS_PER_LAYER; k++) {
                sum += nn->layers[i-1].activations[k] * nn->layers[i].weights[k][j];
            }
            
            // Use different activation functions for different layers
            if (i < NEURAL_LAYERS - 1) {
                nn->layers[i].activations[j] = ReLU(sum); // Hidden layers use ReLU
            } else {
                nn->layers[i].activations[j] = Sigmoid(sum); // Output layer uses Sigmoid
            }
        }
    }
}

/*
====================
AI_BackPropagate
====================
*/
void AI_BackPropagate(int botnum, float *targets, int targetSize) {
    int i, j, k;
    neuralNetwork_t *nn = &botBrains[botnum];
    
    // Calculate output layer gradients
    int outputLayer = NEURAL_LAYERS - 1;
    for (i = 0; i < targetSize && i < NEURONS_PER_LAYER; i++) {
        float error = targets[i] - nn->layers[outputLayer].activations[i];
        nn->layers[outputLayer].gradients[i] = error * SigmoidDerivative(nn->layers[outputLayer].activations[i]);
        nn->totalError += error * error;
    }
    
    // Backpropagate through hidden layers
    for (i = outputLayer - 1; i >= 0; i--) {
        for (j = 0; j < NEURONS_PER_LAYER; j++) {
            float error = 0.0f;
            
            for (k = 0; k < NEURONS_PER_LAYER; k++) {
                error += nn->layers[i+1].gradients[k] * nn->layers[i+1].weights[j][k];
            }
            
            nn->layers[i].gradients[j] = error * ReLUDerivative(nn->layers[i].activations[j]);
        }
    }
    
    // Update weights and biases
    for (i = 1; i < NEURAL_LAYERS; i++) {
        for (j = 0; j < NEURONS_PER_LAYER; j++) {
            nn->layers[i].biases[j] += nn->learningRate * nn->layers[i].gradients[j];
            
            for (k = 0; k < NEURONS_PER_LAYER; k++) {
                float delta = nn->learningRate * nn->layers[i].gradients[j] * nn->layers[i-1].activations[k];
                delta += nn->momentum * nn->layers[i].momentum[k][j];
                
                nn->layers[i].weights[k][j] += delta;
                nn->layers[i].momentum[k][j] = delta;
            }
        }
    }
    
    nn->trainingEpochs++;
    
    // Decay learning rate
    if (nn->trainingEpochs % 1000 == 0) {
        nn->learningRate *= DECAY_RATE;
    }
}

/*
====================
AI_NeuralDecision
====================
*/
int AI_NeuralDecision(int botnum, bot_state_t *bs) {
    float inputs[NEURONS_PER_LAYER];
    int i, j;
    gentity_t *bot = &g_entities[bs->client];
    
    // Prepare input vector from game state
    i = 0;
    
    // Position and velocity
    inputs[i++] = bs->origin[0] / 4096.0f;
    inputs[i++] = bs->origin[1] / 4096.0f;
    inputs[i++] = bs->origin[2] / 4096.0f;
    inputs[i++] = bs->velocity[0] / 1000.0f;
    inputs[i++] = bs->velocity[1] / 1000.0f;
    inputs[i++] = bs->velocity[2] / 1000.0f;
    
    // Health and armor
    inputs[i++] = (float)bs->inventory[INVENTORY_HEALTH] / 200.0f;
    inputs[i++] = (float)bs->inventory[INVENTORY_ARMOR] / 200.0f;
    
    // Weapons and ammo (normalized)
    for (j = WP_MACHINEGUN; j < WP_NUM_WEAPONS && i < NEURONS_PER_LAYER - 50; j++) {
        inputs[i++] = (float)bs->inventory[INVENTORY_WEAPON0 + j] / 100.0f;
    }
    
    // Enemy information
    if (bs->enemy >= 0) {
        gentity_t *enemy = &g_entities[bs->enemy];
        vec3_t dir;
        VectorSubtract(enemy->r.currentOrigin, bs->origin, dir);
        float dist = VectorNormalize(dir);
        
        inputs[i++] = dist / 4096.0f;
        inputs[i++] = dir[0];
        inputs[i++] = dir[1];
        inputs[i++] = dir[2];
        inputs[i++] = (float)enemy->health / 200.0f;
    } else {
        inputs[i++] = 1.0f;
        inputs[i++] = 0.0f;
        inputs[i++] = 0.0f;
        inputs[i++] = 0.0f;
        inputs[i++] = 0.5f;
    }
    
    // Tactical situation
    inputs[i++] = (float)bs->teamgoal_time / 60000.0f;
    inputs[i++] = (float)bs->ctfroam_time / 60000.0f;
    inputs[i++] = (float)bs->formation_dist / 1000.0f;
    
    // Fill remaining inputs with noise or zero
    while (i < NEURONS_PER_LAYER) {
        inputs[i++] = 0.0f;
    }
    
    // Forward propagate
    AI_ForwardPropagate(botnum, inputs, NEURONS_PER_LAYER);
    
    // Get output decisions
    neuralNetwork_t *nn = &botBrains[botnum];
    int outputLayer = NEURAL_LAYERS - 1;
    
    // Find highest activation output
    int bestDecision = 0;
    float bestValue = nn->layers[outputLayer].activations[0];
    
    for (i = 1; i < 32 && i < NEURONS_PER_LAYER; i++) {
        if (nn->layers[outputLayer].activations[i] > bestValue) {
            bestValue = nn->layers[outputLayer].activations[i];
            bestDecision = i;
        }
    }
    
    return bestDecision;
}

/*
====================
AI_TrainFromCombat
====================
*/
void AI_TrainFromCombat(int botnum, int killed, int died, float damageDealt, float damageTaken) {
    float targets[NEURONS_PER_LAYER];
    int i;
    
    // Clear targets
    for (i = 0; i < NEURONS_PER_LAYER; i++) {
        targets[i] = 0.5f;
    }
    
    // Reward/punish based on combat outcome
    if (killed > 0) {
        targets[0] = 1.0f; // Attack was good
        targets[1] = 0.0f; // Retreat was bad
    }
    if (died > 0) {
        targets[0] = 0.0f; // Attack was bad
        targets[1] = 1.0f; // Should have retreated
    }
    
    // Damage efficiency
    if (damageDealt > damageTaken * 2) {
        targets[2] = 1.0f; // Good tactics
    } else if (damageTaken > damageDealt * 2) {
        targets[2] = 0.0f; // Bad tactics
    }
    
    // Train the network
    AI_BackPropagate(botnum, targets, NEURONS_PER_LAYER);
}

/*
====================
AI_GetNeuralAccuracy
====================
*/
float AI_GetNeuralAccuracy(int botnum) {
    return botBrains[botnum].accuracy;
}

/*
====================
AI_SaveNeuralNetwork
====================
*/
void AI_SaveNeuralNetwork(int botnum, const char *filename) {
    fileHandle_t f;
    neuralNetwork_t *nn = &botBrains[botnum];
    
    trap_FS_FOpenFile(filename, &f, FS_WRITE);
    if (!f) {
        return;
    }
    
    trap_FS_Write(nn, sizeof(neuralNetwork_t), f);
    trap_FS_FCloseFile(f);
}

/*
====================
AI_LoadNeuralNetwork
====================
*/
void AI_LoadNeuralNetwork(int botnum, const char *filename) {
    fileHandle_t f;
    neuralNetwork_t *nn = &botBrains[botnum];
    
    trap_FS_FOpenFile(filename, &f, FS_READ);
    if (!f) {
        AI_InitNeuralNetwork(botnum);
        return;
    }
    
    trap_FS_Read(nn, sizeof(neuralNetwork_t), f);
    trap_FS_FCloseFile(f);
}
