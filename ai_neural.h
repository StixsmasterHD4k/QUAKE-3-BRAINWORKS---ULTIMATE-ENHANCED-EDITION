/*
===========================================================================
QUAKE 3 BRAINWORKS - ULTIMATE ENHANCED EDITION
Neural Network AI System - Header File
===========================================================================
*/

#ifndef __AI_NEURAL_H__
#define __AI_NEURAL_H__

// Configuration
#define NN_MAX_LAYERS 5
#define NN_MAX_NEURONS_PER_LAYER 128
#define NN_INPUT_NEURONS 64
#define NN_OUTPUT_NEURONS 32
#define NN_LEARNING_RATE 0.01f
#define NN_MOMENTUM 0.9f

// Layer types
typedef enum {
    LAYER_INPUT,
    LAYER_HIDDEN,
    LAYER_OUTPUT
} layer_type_t;

// Activation functions
typedef enum {
    ACTIVATION_SIGMOID,
    ACTIVATION_RELU,
    ACTIVATION_TANH,
    ACTIVATION_SOFTMAX,
    ACTIVATION_LEAKY_RELU
} activation_func_t;

// Neuron structure
typedef struct neuron_s {
    float value;
    float bias;
    float delta;
    float weights[NN_MAX_NEURONS_PER_LAYER];
    float weight_gradients[NN_MAX_NEURONS_PER_LAYER];
    float prev_weight_changes[NN_MAX_NEURONS_PER_LAYER];
} neuron_t;

// Layer structure
typedef struct layer_s {
    layer_type_t type;
    activation_func_t activation;
    int neuron_count;
    neuron_t neurons[NN_MAX_NEURONS_PER_LAYER];
} layer_t;

// Neural network structure
typedef struct neural_network_s {
    int layer_count;
    layer_t layers[NN_MAX_LAYERS];
    float learning_rate;
    float momentum;
    int training_epochs;
    float total_error;
} neural_network_t;

// Decision context
typedef struct nn_decision_context_s {
    // Decision weights
    float attack_weight;
    float defend_weight;
    float retreat_weight;
    float support_weight;
    float hunt_weight;
    float camp_weight;
    float rush_weight;
    float patrol_weight;
    
    // Weapon preferences
    float weapon_preferences[8];
    
    // Movement
    float aggressive_movement;
    float cautious_movement;
    float tactical_movement;
    
    // Item priorities
    float health_priority;
    float armor_priority;
    float weapon_priority;
    float powerup_priority;
    
    // Team behavior
    float follow_leader;
    float assist_teammate;
    float defend_flag;
    float capture_flag;
    
    // Combat style
    float close_range_engage;
    float mid_range_engage;
    float long_range_engage;
    float use_cover;
    float flank_enemy;
} nn_decision_context_t;

// API Functions
void AI_InitNeuralNetwork(int botNum);
void AI_EvaluateSituation(bot_state_t *bs, nn_decision_context_t *decision);
void AI_TrainNetwork(int botNum, float *inputs, float *expected_outputs);
void AI_UpdateNeuralWeights(int botNum, qboolean success);
void AI_PrepareNeuralInputs(bot_state_t *bs, float *inputs);
void AI_InterpretNeuralOutputs(bot_state_t *bs, float *outputs, nn_decision_context_t *decision);

#endif // __AI_NEURAL_H__
