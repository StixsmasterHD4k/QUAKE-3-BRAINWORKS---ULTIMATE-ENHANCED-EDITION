#include "g_local.h"
#include "ai_main.h"
#include "ai_dmq3.h"
#include "ai_advanced_predict.h"
#include <math.h>

vec3_t AI_PredictEnemyPosition(bot_state_t *bs, float time_ahead) {
    vec3_t predicted;
    gentity_t *enemy;
    
    VectorCopy(vec3_origin, predicted);
    
    if (bs->enemy < 0 || bs->enemy >= MAX_CLIENTS) return predicted;
    
    enemy = &g_entities[bs->enemy];
    if (!enemy->inuse) return predicted;
    
    // Current position + velocity * time
    VectorMA(enemy->r.currentOrigin, time_ahead, enemy->client->ps.velocity, predicted);
    
    // Account for gravity
    predicted[2] -= 0.5f * g_gravity.value * time_ahead * time_ahead;
    
    return predicted;
}

qboolean AI_CalculateLeadTarget(bot_state_t *bs, vec3_t enemy_pos, vec3_t enemy_vel, vec3_t lead_target) {
    vec3_t to_enemy;
    float dist, projectile_speed;
    float time_to_target;
    
    VectorSubtract(enemy_pos, bs->eye, to_enemy);
    dist = VectorLength(to_enemy);
    
    // Get projectile speed based on weapon
    switch(bs->weaponnum) {
        case WP_ROCKET_LAUNCHER:
            projectile_speed = 900.0f;
            break;
        case WP_PLASMAGUN:
            projectile_speed = 2000.0f;
            break;
        case WP_BFG:
            projectile_speed = 2000.0f;
            break;
        case WP_GRENADE_LAUNCHER:
            projectile_speed = 700.0f;
            break;
        default:
            // Hitscan weapons don't need prediction
            VectorCopy(enemy_pos, lead_target);
            return qtrue;
    }
    
    // Calculate time to target
    time_to_target = dist / projectile_speed;
    
    // Predict enemy position
    VectorMA(enemy_pos, time_to_target, enemy_vel, lead_target);
    
    // Account for gravity on projectile
    lead_target[2] -= 0.5f * g_gravity.value * time_to_target * time_to_target;
    
    return qtrue;
}

float AI_CalculateHitProbability(bot_state_t *bs, vec3_t target) {
    vec3_t to_target;
    float dist, accuracy;
    
    VectorSubtract(target, bs->eye, to_target);
    dist = VectorLength(to_target);
    
    // Base accuracy from bot skill
    accuracy = bs->settings.skill / 5.0f;
    
    // Distance factor
    if (dist > 1500.0f) {
        accuracy *= 0.3f;
    } else if (dist > 800.0f) {
        accuracy *= 0.6f;
    } else if (dist > 400.0f) {
        accuracy *= 0.85f;
    }
    
    // Movement penalty
    float speed = VectorLength(bs->cur_ps.velocity);
    if (speed > 200.0f) {
        accuracy *= 0.7f;
    }
    
    // Target movement penalty
    if (bs->enemy >= 0) {
        gentity_t *enemy = &g_entities[bs->enemy];
        float enemy_speed = VectorLength(enemy->client->ps.velocity);
        if (enemy_speed > 300.0f) {
            accuracy *= 0.8f;
        }
    }
    
    return Com_Clamp(0.0f, 1.0f, accuracy);
}

void AI_PredictProjectilePath(vec3_t start, vec3_t velocity, float time, vec3_t result) {
    VectorMA(start, time, velocity, result);
    result[2] -= 0.5f * g_gravity.value * time * time;
}

qboolean AI_PredictEnemyMovement(bot_state_t *bs, vec3_t predicted_pos) {
    if (bs->enemy < 0) return qfalse;
    
    gentity_t *enemy = &g_entities[bs->enemy];
    vec3_t enemy_vel;
    
    VectorCopy(enemy->client->ps.velocity, enemy_vel);
    
    // Predict 0.5 seconds ahead
    VectorMA(enemy->r.currentOrigin, 0.5f, enemy_vel, predicted_pos);
    
    return qtrue;
}

float AI_EstimateTimeToTarget(vec3_t start, vec3_t end, float projectile_speed) {
    vec3_t diff;
    VectorSubtract(end, start, diff);
    float dist = VectorLength(diff);
    return dist / projectile_speed;
}
