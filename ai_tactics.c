/*
===========================================================================
AI ADVANCED TACTICS - COMPLETE IMPLEMENTATION
===========================================================================
*/

#include "g_local.h"
#include "ai_main.h"
#include "ai_dmq3.h"
#include "ai_tactics.h"
#include <math.h>

static tactical_position_t tactical_positions[MAX_TACTICAL_POSITIONS];
static int num_tactical_positions = 0;

/*
==================
AI_InitTacticalSystem
==================
*/
void AI_InitTacticalSystem(void) {
    memset(tactical_positions, 0, sizeof(tactical_positions));
    num_tactical_positions = 0;
}

/*
==================
AI_FindCoverPosition
Find cover from enemy fire
==================
*/
qboolean AI_FindCoverPosition(bot_state_t *bs, vec3_t coverPos) {
    vec3_t forward, right, testPos, enemyDir;
    trace_t trace;
    int i, best_spot = -1;
    float best_quality = 0.0f;
    gentity_t *ent;
    
    if (!bs || bs->enemy < 0) return qfalse;
    
    // Get enemy direction
    ent = &g_entities[bs->enemy];
    VectorSubtract(ent->r.currentOrigin, bs->origin, enemyDir);
    VectorNormalize(enemyDir);
    
    // Test 16 directions around bot
    for (i = 0; i < 16; i++) {
        float angle = (i * 22.5f) * M_PI / 180.0f;
        float searchDist = 200.0f + (rand() % 300);
        
        forward[0] = cos(angle) * searchDist;
        forward[1] = sin(angle) * searchDist;
        forward[2] = 0;
        
        VectorAdd(bs->origin, forward, testPos);
        
        // Trace to test position
        trap_Trace(&trace, bs->origin, NULL, NULL, testPos, bs->entitynum, MASK_SOLID);
        
        if (trace.fraction > 0.3f) {
            float quality = 0.0f;
            vec3_t toEnemy;
            
            // Check if position has cover from enemy
            VectorSubtract(ent->r.currentOrigin, testPos, toEnemy);
            trap_Trace(&trace, testPos, NULL, NULL, ent->r.currentOrigin, bs->entitynum, MASK_SOLID);
            
            // Full cover is best
            if (trace.fraction < 0.9f) {
                quality += 50.0f;
            }
            
            // Prefer positions away from enemy line of sight
            float dot = DotProduct(forward, enemyDir);
            if (dot < 0) { // Moving away from enemy
                quality += 30.0f * (-dot);
            }
            
            // Check for nearby health/armor
            gentity_t *item = NULL;
            while ((item = G_Find(item, FOFS(classname), "item_health")) != NULL) {
                float dist = Distance(testPos, item->r.currentOrigin);
                if (dist < 300.0f) {
                    quality += (300.0f - dist) / 10.0f;
                }
            }
            
            // Prefer higher ground
            if (testPos[2] > bs->origin[2] + 32) {
                quality += 20.0f;
            }
            
            if (quality > best_quality) {
                best_quality = quality;
                VectorCopy(testPos, coverPos);
                best_spot = i;
            }
        }
    }
    
    return (best_spot >= 0);
}

/*
==================
AI_FindFlankPosition
Find position to flank enemy
==================
*/
qboolean AI_FindFlankPosition(bot_state_t *bs, vec3_t enemy_pos, vec3_t flankPos) {
    vec3_t toEnemy, right, testPos;
    trace_t trace;
    float best_score = 0.0f;
    qboolean found = qfalse;
    int i;
    
    VectorSubtract(enemy_pos, bs->origin, toEnemy);
    float dist = VectorNormalize(toEnemy);
    
    // Calculate perpendicular vector
    right[0] = -toEnemy[1];
    right[1] = toEnemy[0];
    right[2] = 0;
    
    // Test left and right flank positions
    for (i = 0; i < 2; i++) {
        vec3_t flankDir;
        
        if (i == 0) {
            VectorCopy(right, flankDir);
        } else {
            VectorNegate(right, flankDir);
        }
        
        // Try multiple distances
        for (int d = 400; d <= 800; d += 200) {
            VectorMA(bs->origin, d, flankDir, testPos);
            
            // Check if path is clear
            trap_Trace(&trace, bs->origin, NULL, NULL, testPos, bs->entitynum, MASK_SOLID);
            
            if (trace.fraction > 0.7f) {
                // Check if we can see enemy from flank position
                trap_Trace(&trace, testPos, NULL, NULL, enemy_pos, bs->entitynum, MASK_SOLID);
                
                float score = trace.fraction * 100.0f;
                
                if (score > best_score) {
                    best_score = score;
                    VectorCopy(testPos, flankPos);
                    found = qtrue;
                }
            }
        }
    }
    
    return found;
}

/*
==================
AI_EvaluateTacticalPosition
==================
*/
float AI_EvaluateTacticalPosition(bot_state_t *bs, vec3_t pos) {
    float score = 0.0f;
    trace_t trace;
    gentity_t *ent;
    
    // Check cover
    if (bs->enemy >= 0) {
        ent = &g_entities[bs->enemy];
        trap_Trace(&trace, pos, NULL, NULL, ent->r.currentOrigin, bs->entitynum, MASK_SOLID);
        if (trace.fraction < 1.0f) {
            score += 30.0f;
        }
    }
    
    // Check height advantage
    if (pos[2] > bs->origin[2] + 64) {
        score += 25.0f;
    }
    
    // Check nearby items
    ent = NULL;
    while ((ent = G_Find(ent, FOFS(classname), "item_")) != NULL) {
        if (!ent->r.linked) continue;
        
        float dist = Distance(pos, ent->r.currentOrigin);
        if (dist < 500.0f) {
            score += (500.0f - dist) / 25.0f;
        }
    }
    
    // Check teammate proximity (team games)
    if (TeamPlayIsOn()) {
        int i;
        for (i = 0; i < level.maxclients; i++) {
            if (i == bs->client) continue;
            
            ent = &g_entities[i];
            if (!ent->inuse || !ent->client) continue;
            if (ent->client->sess.sessionTeam != bs->sess.sessionTeam) continue;
            
            float dist = Distance(pos, ent->r.currentOrigin);
            if (dist < 600.0f && dist > 150.0f) {
                score += 15.0f; // Good spacing from teammates
            }
        }
    }
    
    return score;
}

/*
==================
AI_FindHighGround
==================
*/
qboolean AI_FindHighGround(bot_state_t *bs, vec3_t highPos) {
    vec3_t testPos, forward;
    trace_t trace;
    float best_height = bs->origin[2];
    qboolean found = qfalse;
    int i;
    
    for (i = 0; i < 12; i++) {
        float angle = (i * 30.0f) * M_PI / 180.0f;
        float dist = 500.0f;
        
        forward[0] = cos(angle) * dist;
        forward[1] = sin(angle) * dist;
        forward[2] = 200.0f; // Search upward
        
        VectorAdd(bs->origin, forward, testPos);
        
        trap_Trace(&trace, bs->origin, NULL, NULL, testPos, bs->entitynum, MASK_SOLID);
        
        if (trace.fraction > 0.1f && trace.endpos[2] > best_height) {
            // Check if position is reachable
            vec3_t groundPos;
            VectorCopy(trace.endpos, groundPos);
            groundPos[2] -= 128;
            
            trap_Trace(&trace, trace.endpos, NULL, NULL, groundPos, bs->entitynum, MASK_SOLID);
            
            if (trace.fraction < 1.0f) { // Found ground
                best_height = trace.endpos[2];
                VectorCopy(trace.endpos, highPos);
                found = qtrue;
            }
        }
    }
    
    return found;
}

/*
==================
AI_ExecuteSuppressiveFire
==================
*/
void AI_ExecuteSuppressiveFire(bot_state_t *bs) {
    if (bs->enemy < 0) return;
    
    gentity_t *enemy = &g_entities[bs->enemy];
    vec3_t target, lead;
    
    // Aim near enemy to suppress
    VectorCopy(enemy->r.currentOrigin, target);
    
    // Add random offset for suppression
    target[0] += crandom() * 100.0f;
    target[1] += crandom() * 100.0f;
    target[2] += crandom() * 50.0f;
    
    // Aim and fire
    BotAI_Trace(&bs->eye, bs->viewangles, NULL, target, bs->entitynum, MASK_SHOT);
    
    if (bs->weaponnum == WP_MACHINEGUN || bs->weaponnum == WP_PLASMAGUN) {
        trap_EA_Attack(bs->client);
    }
}

/*
==================
AI_ShouldRetreat
==================
*/
qboolean AI_ShouldRetreat(bot_state_t *bs) {
    // Check health
    if (bs->inventory[INVENTORY_HEALTH] < 30) {
        return qtrue;
    }
    
    // Check if heavily outgunned
    if (bs->enemy >= 0) {
        gentity_t *enemy = &g_entities[bs->enemy];
        
        // Enemy has powerup
        if (enemy->client && enemy->client->ps.powerups[PW_QUAD]) {
            if (bs->inventory[INVENTORY_HEALTH] < 75) {
                return qtrue;
            }
        }
        
        // Low on ammo
        if (bs->cur_ps.ammo[bs->cur_ps.weapon] < 10) {
            return qtrue;
        }
        
        // Multiple enemies nearby
        int nearby_enemies = 0;
        int i;
        for (i = 0; i < level.maxclients; i++) {
            if (i == bs->client) continue;
            
            gentity_t *ent = &g_entities[i];
            if (!ent->inuse || !ent->client) continue;
            if (!BotSameTeam(bs, i)) {
                float dist = Distance(bs->origin, ent->r.currentOrigin);
                if (dist < 700.0f) {
                    nearby_enemies++;
                }
            }
        }
        
        if (nearby_enemies >= 2) {
            return qtrue;
        }
    }
    
    return qfalse;
}

/*
==================
AI_PerformTacticalRetreat
==================
*/
void AI_PerformTacticalRetreat(bot_state_t *bs) {
    vec3_t retreatPos;
    
    if (AI_FindCoverPosition(bs, retreatPos)) {
        // Move to cover
        AIEnter_Battle_Retreat(bs, "tactical retreat");
    } else {
        // Find health/armor
        int health_goal = BotGetItemGoal(bs, "item_health", NULL);
        if (health_goal) {
            bs->ltgtype = LTG_GETITEM;
        }
    }
}

/*
==================
AI_CanFlankEnemy
==================
*/
qboolean AI_CanFlankEnemy(bot_state_t *bs) {
    if (bs->enemy < 0) return qfalse;
    
    gentity_t *enemy = &g_entities[bs->enemy];
    vec3_t flankPos;
    
    return AI_FindFlankPosition(bs, enemy->r.currentOrigin, flankPos);
}

/*
==================
AI_UpdateTacticalSituation
Update bot's understanding of tactical situation
==================
*/
void AI_UpdateTacticalSituation(bot_state_t *bs) {
    // Evaluate current position
    float current_pos_quality = AI_EvaluateTacticalPosition(bs, bs->origin);
    
    // If position is poor, look for better one
    if (current_pos_quality < 20.0f && bs->enemy >= 0) {
        vec3_t betterPos;
        
        // Try to find cover first
        if (AI_FindCoverPosition(bs, betterPos)) {
            // Move toward better position
            VectorCopy(betterPos, bs->ideal_viewangles);
        }
        // Otherwise try flanking
        else if (AI_CanFlankEnemy(bs)) {
            // Flank logic handled separately
        }
    }
    
    // Check if should retreat
    if (AI_ShouldRetreat(bs)) {
        AI_PerformTacticalRetreat(bs);
    }
}
