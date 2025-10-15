#include "g_local.h"
#include "ai_main.h"
#include "ai_dmq3.h"
#include "ai_personality.h"

static bot_personality_t bot_personalities[MAX_CLIENTS];

void AI_InitPersonality(int botNum, int personality_type) {
    bot_personality_t *pers;
    
    if (botNum < 0 || botNum >= MAX_CLIENTS) return;
    
    pers = &bot_personalities[botNum];
    memset(pers, 0, sizeof(bot_personality_t));
    
    pers->type = personality_type;
    
    switch(personality_type) {
        case PERSONALITY_AGGRESSIVE:
            pers->aggression = 0.9f;
            pers->cautiousness = 0.2f;
            pers->teamwork = 0.4f;
            pers->camping_tendency = 0.1f;
            pers->accuracy_modifier = 0.8f;
            pers->reaction_time_modifier = 0.7f;
            break;
        case PERSONALITY_DEFENSIVE:
            pers->aggression = 0.3f;
            pers->cautiousness = 0.9f;
            pers->teamwork = 0.7f;
            pers->camping_tendency = 0.6f;
            pers->accuracy_modifier = 0.9f;
            pers->reaction_time_modifier = 0.8f;
            break;
        case PERSONALITY_TACTICAL:
            pers->aggression = 0.6f;
            pers->cautiousness = 0.7f;
            pers->teamwork = 0.8f;
            pers->camping_tendency = 0.3f;
            pers->accuracy_modifier = 0.95f;
            pers->reaction_time_modifier = 0.9f;
            break;
        case PERSONALITY_RUSHER:
            pers->aggression = 1.0f;
            pers->cautiousness = 0.1f;
            pers->teamwork = 0.3f;
            pers->camping_tendency = 0.05f;
            pers->accuracy_modifier = 0.7f;
            pers->reaction_time_modifier = 0.6f;
            break;
        case PERSONALITY_SNIPER:
            pers->aggression = 0.4f;
            pers->cautiousness = 0.8f;
            pers->teamwork = 0.5f;
            pers->camping_tendency = 0.9f;
            pers->accuracy_modifier = 1.0f;
            pers->reaction_time_modifier = 0.95f;
            break;
        case PERSONALITY_BALANCED:
        default:
            pers->aggression = 0.5f;
            pers->cautiousness = 0.5f;
            pers->teamwork = 0.5f;
            pers->camping_tendency = 0.3f;
            pers->accuracy_modifier = 0.85f;
            pers->reaction_time_modifier = 0.8f;
            break;
    }
    
    pers->item_priority = 0.5f + crandom() * 0.2f;
    pers->powerup_priority = 0.7f + crandom() * 0.2f;
}

void AI_ApplyPersonality(bot_state_t *bs) {
    bot_personality_t *pers = &bot_personalities[bs->client];
    
    if (bs->enemy >= 0) {
        // Aggressive personalities chase
        if (pers->aggression > 0.7f) {
            bs->flags |= BFL_CHASE;
        }
        
        // Cautious personalities keep distance
        if (pers->cautiousness > 0.7f) {
            vec3_t to_enemy;
            VectorSubtract(bs->lastenemyorigin, bs->origin, to_enemy);
            float dist = VectorLength(to_enemy);
            
            if (dist < 500.0f) {
                bs->flags |= BFL_RETREAT;
            }
        }
    }
    
    // Apply accuracy modifier
    bs->settings.accuracy *= pers->accuracy_modifier;
    
    // Camping behavior
    if (pers->camping_tendency > 0.6f && bs->enemy < 0) {
        if (random() < 0.3f) {
            bs->ltgtype = LTG_CAMP;
        }
    }
}

bot_personality_t *AI_GetPersonality(int botNum) {
    if (botNum < 0 || botNum >= MAX_CLIENTS) return NULL;
    return &bot_personalities[botNum];
}

qboolean AI_PersonalityDecision(bot_state_t *bs, int decision_type) {
    bot_personality_t *pers = AI_GetPersonality(bs->client);
    if (!pers) return qfalse;
    
    float roll = random();
    
    switch(decision_type) {
        case 0: // Attack decision
            return (roll < pers->aggression);
        case 1: // Retreat decision
            return (roll < pers->cautiousness);
        case 2: // Help teammate
            return (roll < pers->teamwork);
        case 3: // Camp
            return (roll < pers->camping_tendency);
    }
    
    return qfalse;
}
