/*
===========================================================================
QUAKE 3 BRAINWORKS - ULTIMATE ENHANCED EDITION
Neural Network AI Implementation - FULL PRODUCTION VERSION
Advanced Machine Learning Decision Making System
===========================================================================
*/

#include "g_local.h"
#include "ai_main.h"
#include "ai_dmq3.h"
#include "ai_chat.h"
#include "ai_cmd.h"
#include "ai_dmnet.h"
#include "ai_team.h"
#include "ai_neural.h"
#include <math.h>

// Global neural networks for all bots
static neural_network_t bot_neural_networks[MAX_CLIENTS];
static int nn_initialized[MAX_CLIENTS] = {0};

// Math utilities
static float NN_Sigmoid(float x) {
    return 1.0f / (1.0f + exp(-x));
}

static float NN_SigmoidDerivative(float x) {
    float sig = NN_Sigmoid(x);
    return sig * (1.0f - sig);
}

static float NN_ReLU(float x) {
    return (x > 0.0f) ? x : 0.0f;
}

static float NN_ReLUDerivative(float x) {
    return (x > 0.0f) ? 1.0f : 0.0f;
}

static float NN_LeakyReLU(float x) {
    return (x > 0.0f) ? x : (0.01f * x);
}

static float NN_LeakyReLUDerivative(float x) {
    return (x > 0.0f) ? 1.0f : 0.01f;
}

static float NN_Tanh(float x) {
    return tanh(x);
}

static float NN_TanhDerivative(float x) {
    float t = tanh(x);
    return 1.0f - t * t;
}

static float NN_ApplyActivation(float x, activation_func_t func) {
    switch(func) {
        case ACTIVATION_SIGMOID:
            return NN_Sigmoid(x);
        case ACTIVATION_RELU:
            return NN_ReLU(x);
        case ACTIVATION_LEAKY_RELU:
            return NN_LeakyReLU(x);
        case ACTIVATION_TANH:
            return NN_Tanh(x);
        case ACTIVATION_SOFTMAX:
            return x; // Softmax is applied at layer level
        default:
            return x;
    }
}

static float NN_ApplyActivationDerivative(float x, activation_func_t func) {
    switch(func) {
        case ACTIVATION_SIGMOID:
            return NN_SigmoidDerivative(x);
        case ACTIVATION_RELU:
            return NN_ReLUDerivative(x);
        case ACTIVATION_LEAKY_RELU:
            return NN_LeakyReLUDerivative(x);
        case ACTIVATION_TANH:
            return NN_TanhDerivative(x);
        default:
            return 1.0f;
    }
}

/*
==================
NN_Initialize
Initialize neural network with proper architecture
==================
*/
void NN_Initialize(neural_network_t *nn) {
    int i, j, k;
    
    if (!nn) return;
    
    memset(nn, 0, sizeof(neural_network_t));
    
    nn->learning_rate = NN_LEARNING_RATE;
    nn->momentum = NN_MOMENTUM;
    nn->training_epochs = 0;
    nn->total_error = 0.0f;
    nn->layer_count = 5;
    
    // Input layer: 64 neurons
    nn->layers[0].neuron_count = NN_INPUT_NEURONS;
    nn->layers[0].activation = ACTIVATION_RELU;
    nn->layers[0].type = LAYER_INPUT;
    
    // Hidden layer 1: 128 neurons
    nn->layers[1].neuron_count = 128;
    nn->layers[1].activation = ACTIVATION_LEAKY_RELU;
    nn->layers[1].type = LAYER_HIDDEN;
    
    // Hidden layer 2: 64 neurons
    nn->layers[2].neuron_count = 64;
    nn->layers[2].activation = ACTIVATION_RELU;
    nn->layers[2].type = LAYER_HIDDEN;
    
    // Hidden layer 3: 48 neurons
    nn->layers[3].neuron_count = 48;
    nn->layers[3].activation = ACTIVATION_TANH;
    nn->layers[3].type = LAYER_HIDDEN;
    
    // Output layer: 32 neurons
    nn->layers[4].neuron_count = NN_OUTPUT_NEURONS;
    nn->layers[4].activation = ACTIVATION_SOFTMAX;
    nn->layers[4].type = LAYER_OUTPUT;
    
    // Initialize all weights and biases using Xavier initialization
    for (i = 0; i < nn->layer_count; i++) {
        layer_t *layer = &nn->layers[i];
        
        for (j = 0; j < layer->neuron_count; j++) {
            neuron_t *neuron = &layer->neurons[j];
            
            neuron->value = 0.0f;
            neuron->bias = ((float)rand() / RAND_MAX - 0.5f) * 0.1f;
            neuron->delta = 0.0f;
            
            if (i < nn->layer_count - 1) {
                int next_layer_count = nn->layers[i + 1].neuron_count;
                float xavier_std = sqrt(2.0f / (layer->neuron_count + next_layer_count));
                
                for (k = 0; k < next_layer_count; k++) {
                    neuron->weights[k] = ((float)rand() / RAND_MAX - 0.5f) * 2.0f * xavier_std;
                    neuron->weight_gradients[k] = 0.0f;
                    neuron->prev_weight_changes[k] = 0.0f;
                }
            }
        }
    }
}

/*
==================
NN_ForwardPass
Execute forward propagation through network
==================
*/
void NN_ForwardPass(neural_network_t *nn, float *inputs, float *outputs) {
    int i, j, k;
    
    if (!nn || !inputs || !outputs) return;
    
    // Set input layer values
    for (i = 0; i < nn->layers[0].neuron_count && i < NN_INPUT_NEURONS; i++) {
        nn->layers[0].neurons[i].value = inputs[i];
    }
    
    // Propagate through hidden and output layers
    for (i = 1; i < nn->layer_count; i++) {
        layer_t *current_layer = &nn->layers[i];
        layer_t *prev_layer = &nn->layers[i - 1];
        
        for (j = 0; j < current_layer->neuron_count; j++) {
            float sum = current_layer->neurons[j].bias;
            
            // Weighted sum from previous layer
            for (k = 0; k < prev_layer->neuron_count; k++) {
                sum += prev_layer->neurons[k].value * prev_layer->neurons[k].weights[j];
            }
            
            // Apply activation function
            if (current_layer->activation == ACTIVATION_SOFTMAX && i == nn->layer_count - 1) {
                current_layer->neurons[j].value = sum; // Will apply softmax after all neurons computed
            } else {
                current_layer->neurons[j].value = NN_ApplyActivation(sum, current_layer->activation);
            }
        }
        
        // Apply softmax for output layer
        if (current_layer->activation == ACTIVATION_SOFTMAX && i == nn->layer_count - 1) {
            float max_val = current_layer->neurons[0].value;
            float sum_exp = 0.0f;
            
            // Find max for numerical stability
            for (j = 1; j < current_layer->neuron_count; j++) {
                if (current_layer->neurons[j].value > max_val) {
                    max_val = current_layer->neurons[j].value;
                }
            }
            
            // Compute exp and sum
            for (j = 0; j < current_layer->neuron_count; j++) {
                float exp_val = exp(current_layer->neurons[j].value - max_val);
                current_layer->neurons[j].value = exp_val;
                sum_exp += exp_val;
            }
            
            // Normalize
            for (j = 0; j < current_layer->neuron_count; j++) {
                current_layer->neurons[j].value /= sum_exp;
            }
        }
    }
    
    // Copy output values
    for (i = 0; i < nn->layers[nn->layer_count - 1].neuron_count && i < NN_OUTPUT_NEURONS; i++) {
        outputs[i] = nn->layers[nn->layer_count - 1].neurons[i].value;
    }
}

/*
==================
NN_BackwardPass
Execute backpropagation for training
==================
*/
void NN_BackwardPass(neural_network_t *nn, float *expected_outputs) {
    int i, j, k;
    float error = 0.0f;
    
    if (!nn || !expected_outputs) return;
    
    // Calculate output layer error
    layer_t *output_layer = &nn->layers[nn->layer_count - 1];
    for (i = 0; i < output_layer->neuron_count; i++) {
        float out = output_layer->neurons[i].value;
        float target = expected_outputs[i];
        float diff = target - out;
        
        error += diff * diff;
        
        // For softmax + cross-entropy, derivative is simply (output - target)
        if (output_layer->activation == ACTIVATION_SOFTMAX) {
            output_layer->neurons[i].delta = out - target;
        } else {
            output_layer->neurons[i].delta = diff * NN_ApplyActivationDerivative(out, output_layer->activation);
        }
    }
    
    nn->total_error += error;
    
    // Backpropagate through hidden layers
    for (i = nn->layer_count - 2; i >= 0; i--) {
        layer_t *current_layer = &nn->layers[i];
        layer_t *next_layer = &nn->layers[i + 1];
        
        for (j = 0; j < current_layer->neuron_count; j++) {
            float sum = 0.0f;
            
            // Sum weighted deltas from next layer
            for (k = 0; k < next_layer->neuron_count; k++) {
                sum += next_layer->neurons[k].delta * current_layer->neurons[j].weights[k];
            }
            
            current_layer->neurons[j].delta = sum * NN_ApplyActivationDerivative(
                current_layer->neurons[j].value, 
                current_layer->activation
            );
        }
    }
    
    // Update weights and biases
    for (i = 0; i < nn->layer_count - 1; i++) {
        layer_t *current_layer = &nn->layers[i];
        layer_t *next_layer = &nn->layers[i + 1];
        
        for (j = 0; j < current_layer->neuron_count; j++) {
            for (k = 0; k < next_layer->neuron_count; k++) {
                float gradient = nn->learning_rate * next_layer->neurons[k].delta * current_layer->neurons[j].value;
                float delta_weight = gradient + nn->momentum * current_layer->neurons[j].prev_weight_changes[k];
                
                current_layer->neurons[j].weights[k] += delta_weight;
                current_layer->neurons[j].prev_weight_changes[k] = delta_weight;
            }
        }
        
        // Update biases
        for (j = 0; j < next_layer->neuron_count; j++) {
            next_layer->neurons[j].bias += nn->learning_rate * next_layer->neurons[j].delta;
        }
    }
    
    nn->training_epochs++;
}

/*
==================
AI_InitNeuralNetwork
Public API to initialize bot neural network
==================
*/
void AI_InitNeuralNetwork(int botNum) {
    if (botNum < 0 || botNum >= MAX_CLIENTS) return;
    
    NN_Initialize(&bot_neural_networks[botNum]);
    nn_initialized[botNum] = 1;
}

/*
==================
AI_PrepareNeuralInputs
Prepare input vector from game state
==================
*/
void AI_PrepareNeuralInputs(bot_state_t *bs, float *inputs) {
    int i = 0;
    float norm_factor;
    
    if (!bs || !inputs) return;
    
    memset(inputs, 0, sizeof(float) * NN_INPUT_NEURONS);
    
    // Health and armor (normalized)
    inputs[i++] = bs->inventory[INVENTORY_HEALTH] / 200.0f;
    inputs[i++] = bs->cur_ps.stats[STAT_ARMOR] / 200.0f;
    
    // Weapon status (8 weapons)
    inputs[i++] = bs->inventory[INVENTORY_GAUNTLET] ? 1.0f : 0.0f;
    inputs[i++] = bs->inventory[INVENTORY_SHOTGUN] ? 1.0f : 0.0f;
    inputs[i++] = bs->inventory[INVENTORY_MACHINEGUN] ? 1.0f : 0.0f;
    inputs[i++] = bs->inventory[INVENTORY_GRENADELAUNCHER] ? 1.0f : 0.0f;
    inputs[i++] = bs->inventory[INVENTORY_ROCKETLAUNCHER] ? 1.0f : 0.0f;
    inputs[i++] = bs->inventory[INVENTORY_LIGHTNING] ? 1.0f : 0.0f;
    inputs[i++] = bs->inventory[INVENTORY_RAILGUN] ? 1.0f : 0.0f;
    inputs[i++] = bs->inventory[INVENTORY_PLASMAGUN] ? 1.0f : 0.0f;
    
    // Ammo levels (normalized)
    inputs[i++] = bs->cur_ps.ammo[AMMO_SHELLS] / 100.0f;
    inputs[i++] = bs->cur_ps.ammo[AMMO_BULLETS] / 200.0f;
    inputs[i++] = bs->cur_ps.ammo[AMMO_GRENADES] / 50.0f;
    inputs[i++] = bs->cur_ps.ammo[AMMO_CELLS] / 200.0f;
    inputs[i++] = bs->cur_ps.ammo[AMMO_LIGHTNING] / 200.0f;
    inputs[i++] = bs->cur_ps.ammo[AMMO_ROCKETS] / 50.0f;
    inputs[i++] = bs->cur_ps.ammo[AMMO_SLUGS] / 50.0f;
    inputs[i++] = bs->cur_ps.ammo[AMMO_BFG] / 100.0f;
    
    // Powerups
    inputs[i++] = bs->inventory[INVENTORY_QUAD] ? 1.0f : 0.0f;
    inputs[i++] = bs->inventory[INVENTORY_BATTLESUIT] ? 1.0f : 0.0f;
    inputs[i++] = bs->inventory[INVENTORY_HASTE] ? 1.0f : 0.0f;
    inputs[i++] = bs->inventory[INVENTORY_INVISIBILITY] ? 1.0f : 0.0f;
    inputs[i++] = bs->inventory[INVENTORY_REGEN] ? 1.0f : 0.0f;
    inputs[i++] = bs->inventory[INVENTORY_FLIGHT] ? 1.0f : 0.0f;
    
    // Enemy information
    if (bs->enemy >= 0 && bs->enemy < MAX_CLIENTS) {
        inputs[i++] = 1.0f; // Enemy present
        inputs[i++] = bs->enemysight_time > 0 ? 1.0f : 0.0f; // Can see enemy
        
        vec3_t dir;
        VectorSubtract(bs->lastenemyorigin, bs->origin, dir);
        float dist = VectorLength(dir);
        inputs[i++] = (dist > 0.0f) ? (1.0f - (dist / 4000.0f)) : 0.0f; // Normalized distance
        
        // Enemy direction (normalized)
        if (dist > 0.0f) {
            VectorNormalize(dir);
            inputs[i++] = dir[0];
            inputs[i++] = dir[1];
            inputs[i++] = dir[2];
        } else {
            inputs[i++] = 0.0f;
            inputs[i++] = 0.0f;
            inputs[i++] = 0.0f;
        }
    } else {
        inputs[i++] = 0.0f; // No enemy
        i += 6; // Skip enemy data
    }
    
    // Tactical situation
    inputs[i++] = bs->lastkilledplayer >= 0 ? 1.0f : 0.0f;
    inputs[i++] = bs->lastkilledby >= 0 ? 1.0f : 0.0f;
    inputs[i++] = bs->weaponnum / 10.0f; // Current weapon
    
    // Team information
    inputs[i++] = (float)bs->cur_ps.persistant[PERS_SCORE] / 100.0f;
    inputs[i++] = TeamPlayIsOn() ? 1.0f : 0.0f;
    
    // Map control factors
    inputs[i++] = (level.time - bs->ltgtime) / 60000.0f; // Time since last long term goal
    inputs[i++] = (level.time - bs->teammessage_time) / 30000.0f;
    inputs[i++] = (level.time - bs->teamgoal_time) / 60000.0f;
    
    // Movement state
    inputs[i++] = bs->cur_ps.velocity[0] / 320.0f;
    inputs[i++] = bs->cur_ps.velocity[1] / 320.0f;
    inputs[i++] = bs->cur_ps.velocity[2] / 320.0f;
    
    // Position factors
    inputs[i++] = bs->origin[2] / 1024.0f; // Height normalized
    
    // Combat history (simplified)
    inputs[i++] = bs->num_deaths > 0 ? (float)bs->num_kills / (float)bs->num_deaths : (float)bs->num_kills;
    
    // Behavioral flags
    inputs[i++] = (bs->ltgtype == LTG_TEAMHELP) ? 1.0f : 0.0f;
    inputs[i++] = (bs->ltgtype == LTG_TEAMACCOMPANY) ? 1.0f : 0.0f;
    inputs[i++] = (bs->ltgtype == LTG_DEFENDKEYAREA) ? 1.0f : 0.0f;
    inputs[i++] = (bs->ltgtype == LTG_GETITEM) ? 1.0f : 0.0f;
    inputs[i++] = (bs->ltgtype == LTG_KILL) ? 1.0f : 0.0f;
    inputs[i++] = (bs->ltgtype == LTG_CAMP) ? 1.0f : 0.0f;
    inputs[i++] = (bs->ltgtype == LTG_PATROL) ? 1.0f : 0.0f;
    
    // Time factors
    inputs[i++] = (float)(level.time % 60000) / 60000.0f;
    
    // Pad remaining inputs with zeros if necessary
    while (i < NN_INPUT_NEURONS) {
        inputs[i++] = 0.0f;
    }
}

/*
==================
AI_InterpretNeuralOutputs
Convert network outputs to bot decisions
==================
*/
void AI_InterpretNeuralOutputs(bot_state_t *bs, float *outputs, nn_decision_context_t *decision) {
    int i;
    
    if (!bs || !outputs || !decision) return;
    
    // Clear decision context
    memset(decision, 0, sizeof(nn_decision_context_t));
    
    // Parse output neurons into decision weights
    decision->attack_weight = outputs[0];
    decision->defend_weight = outputs[1];
    decision->retreat_weight = outputs[2];
    decision->support_weight = outputs[3];
    decision->hunt_weight = outputs[4];
    decision->camp_weight = outputs[5];
    decision->rush_weight = outputs[6];
    decision->patrol_weight = outputs[7];
    
    // Weapon preferences
    for (i = 0; i < 8 && i + 8 < NN_OUTPUT_NEURONS; i++) {
        decision->weapon_preferences[i] = outputs[8 + i];
    }
    
    // Movement style
    decision->aggressive_movement = outputs[16];
    decision->cautious_movement = outputs[17];
    decision->tactical_movement = outputs[18];
    
    // Item priorities
    decision->health_priority = outputs[19];
    decision->armor_priority = outputs[20];
    decision->weapon_priority = outputs[21];
    decision->powerup_priority = outputs[22];
    
    // Team behavior
    decision->follow_leader = outputs[23];
    decision->assist_teammate = outputs[24];
    decision->defend_flag = outputs[25];
    decision->capture_flag = outputs[26];
    
    // Combat style
    decision->close_range_engage = outputs[27];
    decision->mid_range_engage = outputs[28];
    decision->long_range_engage = outputs[29];
    decision->use_cover = outputs[30];
    decision->flank_enemy = outputs[31];
}

/*
==================
AI_EvaluateSituation
Main neural network evaluation for bot AI
==================
*/
void AI_EvaluateSituation(bot_state_t *bs, nn_decision_context_t *decision) {
    float inputs[NN_INPUT_NEURONS];
    float outputs[NN_OUTPUT_NEURONS];
    
    if (!bs || !decision) return;
    
    // Initialize neural network if not done
    if (!nn_initialized[bs->client]) {
        AI_InitNeuralNetwork(bs->client);
    }
    
    // Prepare inputs from game state
    AI_PrepareNeuralInputs(bs, inputs);
    
    // Execute forward pass
    NN_ForwardPass(&bot_neural_networks[bs->client], inputs, outputs);
    
    // Interpret outputs into decisions
    AI_InterpretNeuralOutputs(bs, outputs, decision);
}

/*
==================
AI_TrainNetwork
Train network based on outcomes
==================
*/
void AI_TrainNetwork(int botNum, float *inputs, float *expected_outputs) {
    float outputs[NN_OUTPUT_NEURONS];
    
    if (botNum < 0 || botNum >= MAX_CLIENTS) return;
    if (!nn_initialized[botNum]) return;
    
    // Forward pass
    NN_ForwardPass(&bot_neural_networks[botNum], inputs, outputs);
    
    // Backward pass with expected outputs
    NN_BackwardPass(&bot_neural_networks[botNum], expected_outputs);
}

/*
==================
AI_UpdateNeuralWeights
Update network based on combat success/failure
==================
*/
void AI_UpdateNeuralWeights(int botNum, qboolean success) {
    if (botNum < 0 || botNum >= MAX_CLIENTS) return;
    if (!nn_initialized[botNum]) return;
    
    // Adjust learning rate based on success
    if (success) {
        // Reinforce current weights slightly
        bot_neural_networks[botNum].learning_rate *= 0.99f;
    } else {
        // Increase exploration
        bot_neural_networks[botNum].learning_rate = Q_min(NN_LEARNING_RATE * 1.5f, 0.1f);
    }
}
