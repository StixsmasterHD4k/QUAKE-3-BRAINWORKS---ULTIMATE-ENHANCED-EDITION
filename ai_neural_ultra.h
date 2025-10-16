/*
===========================================================================
ULTRA-ADVANCED NEURAL DECISION NETWORK - HEADER
===========================================================================
*/

#ifndef __AI_NEURAL_ULTRA_H__
#define __AI_NEURAL_ULTRA_H__

void AI_InitNeuralNetwork(int botnum);
void AI_ForwardPropagate(int botnum, float *inputs, int inputSize);
void AI_BackPropagate(int botnum, float *targets, int targetSize);
int AI_NeuralDecision(int botnum, bot_state_t *bs);
void AI_TrainFromCombat(int botnum, int killed, int died, float damageDealt, float damageTaken);
float AI_GetNeuralAccuracy(int botnum);
void AI_SaveNeuralNetwork(int botnum, const char *filename);
void AI_LoadNeuralNetwork(int botnum, const char *filename);

#endif
