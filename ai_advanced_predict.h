#ifndef __AI_ADVANCED_PREDICT_H__
#define __AI_ADVANCED_PREDICT_H__

vec3_t AI_PredictEnemyPosition(bot_state_t *bs, float time_ahead);
qboolean AI_CalculateLeadTarget(bot_state_t *bs, vec3_t enemy_pos, vec3_t enemy_vel, vec3_t lead_target);
float AI_CalculateHitProbability(bot_state_t *bs, vec3_t target);
void AI_PredictProjectilePath(vec3_t start, vec3_t velocity, float time, vec3_t result);
qboolean AI_PredictEnemyMovement(bot_state_t *bs, vec3_t predicted_pos);
float AI_EstimateTimeToTarget(vec3_t start, vec3_t end, float projectile_speed);

#endif
