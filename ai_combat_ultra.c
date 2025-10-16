/*
===========================================================================
Ultra Advanced Combat System for Brainworks
===========================================================================
*/

#include "g_local.h"
#include "ai_main.h"
#include "ai_combat_ultra.h"

#define COMBAT_THINK_INTERVAL 50
#define WEAPON_SWITCH_DELAY 500
#define DODGE_REACTION_TIME 150
#define AIM_PREDICTION_STEPS 10

typedef struct {
    int last_think_time;
    int last_weapon_switch;
    int last_dodge_time;
    vec3_t predicted_enemy_pos;
    float aim_accuracy;
    int combat_style; // 0=aggressive, 1=defensive, 2=tactical
    int kill_streak;
    int death_streak;
    float danger_level;
    qboolean in_combat;
} combat_state_t;

static combat_state_t combat_states[MAX_CLIENTS];

/*
================
AI_CombatPredictEnemyPosition
Advanced enemy position prediction using velocity and acceleration
================
*/
void AI_CombatPredictEnemyPosition(gentity_t *enemy, vec3_t predicted_pos, int steps_ahead) {
    vec3_t velocity;
    vec3_t gravity_effect = {0, 0, -800}; // Gravity acceleration
    int i;
    
    VectorCopy(enemy->r.currentOrigin, predicted_pos);
    VectorCopy(enemy->s.pos.trDelta, velocity);
    
    for (i = 0; i < steps_ahead; i++) {
        float time_step = 0.05f; // 50ms per step
        
        // Apply velocity
        predicted_pos[0] += velocity[0] * time_step;
        predicted_pos[1] += velocity[1] * time_step;
        predicted_pos[2] += velocity[2] * time_step;
        
        // Apply gravity
        velocity[2] += gravity_effect[2] * time_step;
        
        // Simple ground collision detection
        if (predicted_pos[2] < enemy->r.currentOrigin[2] - 200) {
            predicted_pos[2] = enemy->r.currentOrigin[2];
            velocity[2] = 0;
        }
    }
}

/*
================
AI_CombatAnalyzeThreat
Analyze threat level from enemy
================
*/
float AI_CombatAnalyzeThreat(gentity_t *self, gentity_t *enemy) {
    float threat = 0;
    float distance = Distance(self->r.currentOrigin, enemy->r.currentOrigin);
    
    // Distance factor (closer = more threat)
    threat += (2000.0f - distance) / 2000.0f * 30.0f;
    
    // Enemy weapon threat
    if (enemy->client) {
        switch (enemy->client->ps.weapon) {
            case WP_RAILGUN: threat += 25.0f; break;
            case WP_ROCKET_LAUNCHER: threat += 20.0f; break;
            case WP_LIGHTNING: threat += 18.0f; break;
            case WP_PLASMAGUN: threat += 15.0f; break;
            case WP_SHOTGUN: threat += 12.0f; break;
            case WP_GRENADE_LAUNCHER: threat += 10.0f; break;
            default: threat += 5.0f; break;
        }
    }
    
    // Enemy health/armor
    if (enemy->client) {
        threat += (enemy->client->ps.stats[STAT_HEALTH] / 100.0f) * 10.0f;
        threat += (enemy->client->ps.stats[STAT_ARMOR] / 100.0f) * 5.0f;
    }
    
    // Enemy powerups
    if (enemy->client) {
        if (enemy->client->ps.powerups[PW_QUAD] > level.time) threat += 40.0f;
        if (enemy->client->ps.powerups[PW_HASTE] > level.time) threat += 15.0f;
        if (enemy->client->ps.powerups[PW_INVIS] > level.time) threat += 20.0f;
    }
    
    // Enemy is aiming at us
    vec3_t enemy_aim_dir, to_us;
    if (enemy->client) {
        AngleVectors(enemy->client->ps.viewangles, enemy_aim_dir, NULL, NULL);
        VectorSubtract(self->r.currentOrigin, enemy->r.currentOrigin, to_us);
        VectorNormalize(to_us);
        
        float aim_dot = DotProduct(enemy_aim_dir, to_us);
        if (aim_dot > 0.9f) threat += 25.0f; // Enemy is aiming at us!
    }
    
    return threat;
}

/*
================
AI_CombatSelectBestWeapon
Select optimal weapon based on situation
================
*/
int AI_CombatSelectBestWeapon(gentity_t *self, gentity_t *enemy) {
    gclient_t *client = self->client;
    float distance = Distance(self->r.currentOrigin, enemy->r.currentOrigin);
    int best_weapon = WP_MACHINEGUN;
    float best_score = 0;
    int weapon;
    
    for (weapon = WP_MACHINEGUN; weapon < WP_NUM_WEAPONS; weapon++) {
        float score = 0;
        
        // Check if we have the weapon and ammo
        if (!(client->ps.stats[STAT_WEAPONS] & (1 << weapon))) continue;
        if (client->ps.ammo[weapon] <= 0 && weapon != WP_GAUNTLET) continue;
        
        // Score based on distance
        switch (weapon) {
            case WP_RAILGUN:
                if (distance > 1000) score = 100;
                else if (distance > 500) score = 80;
                else score = 40;
                break;
                
            case WP_ROCKET_LAUNCHER:
                if (distance > 300 && distance < 1500) score = 90;
                else if (distance > 150) score = 70;
                else score = 20; // Too close for rockets
                break;
                
            case WP_LIGHTNING:
                if (distance < 800) score = 95;
                else score = 30;
                break;
                
            case WP_PLASMAGUN:
                if (distance < 1000) score = 85;
                else score = 40;
                break;
                
            case WP_SHOTGUN:
                if (distance < 400) score = 90;
                else if (distance < 800) score = 50;
                else score = 20;
                break;
                
            case WP_GRENADE_LAUNCHER:
                if (distance > 400 && distance < 1200) score = 75;
                else score = 35;
                break;
                
            case WP_MACHINEGUN:
                score = 50; // Default fallback
                break;
        }
        
        // Bonus for having quad damage
        if (client->ps.powerups[PW_QUAD] > level.time) {
            score *= 1.3f;
        }
        
        // Penalty if low ammo
        if (client->ps.ammo[weapon] < 10) {
            score *= 0.7f;
        }
        
        if (score > best_score) {
            best_score = score;
            best_weapon = weapon;
        }
    }
    
    return best_weapon;
}

/*
================
AI_CombatDodgeProjectile
Dodge incoming projectiles
================
*/
qboolean AI_CombatDodgeProjectile(gentity_t *self, usercmd_t *ucmd) {
    gentity_t *projectile;
    vec3_t to_projectile, projectile_vel;
    float distance, projectile_speed;
    int i;
    
    // Look for nearby projectiles
    for (i = MAX_CLIENTS; i < level.num_entities; i++) {
        projectile = &g_entities[i];
        
        if (!projectile->inuse) continue;
        if (projectile->s.eType != ET_MISSILE) continue;
        
        VectorSubtract(projectile->r.currentOrigin, self->r.currentOrigin, to_projectile);
        distance = VectorLength(to_projectile);
        
        if (distance > 1000) continue; // Too far
        
        // Check if projectile is heading towards us
        VectorCopy(projectile->s.pos.trDelta, projectile_vel);
        projectile_speed = VectorLength(projectile_vel);
        
        if (projectile_speed < 0.1f) continue;
        
        VectorNormalize(projectile_vel);
        vec3_t to_us;
        VectorSubtract(self->r.currentOrigin, projectile->r.currentOrigin, to_us);
        VectorNormalize(to_us);
        
        float heading_dot = DotProduct(projectile_vel, to_us);
        
        if (heading_dot > 0.7f) { // Projectile heading towards us
            // Calculate dodge direction (perpendicular to projectile path)
            vec3_t dodge_dir;
            vec3_t up = {0, 0, 1};
            CrossProduct(projectile_vel, up, dodge_dir);
            VectorNormalize(dodge_dir);
            
            // Random left or right
            if (rand() % 2) {
                VectorNegate(dodge_dir, dodge_dir);
            }
            
            // Apply dodge movement
            vec3_t forward, right;
            AngleVectors(self->client->ps.viewangles, forward, right, NULL);
            
            float right_dot = DotProduct(dodge_dir, right);
            float forward_dot = DotProduct(dodge_dir, forward);
            
            ucmd->rightmove = (signed char)(right_dot * 127);
            ucmd->forwardmove = (signed char)(forward_dot * 127);
            
            // Jump if rocket is close
            if (projectile->s.weapon == WP_ROCKET_LAUNCHER && distance < 400) {
                ucmd->upmove = 127;
            }
            
            return qtrue;
        }
    }
    
    return qfalse;
}

/*
================
AI_CombatStrafe
Implement advanced strafing patterns
================
*/
void AI_CombatStrafe(gentity_t *self, gentity_t *enemy, usercmd_t *ucmd) {
    static int strafe_pattern[MAX_CLIENTS] = {0};
    static int pattern_change_time[MAX_CLIENTS] = {0};
    int clientNum = self - g_entities;
    
    // Change strafe pattern every 1-2 seconds
    if (level.time - pattern_change_time[clientNum] > 1000 + (rand() % 1000)) {
        strafe_pattern[clientNum] = rand() % 4;
        pattern_change_time[clientNum] = level.time;
    }
    
    vec3_t to_enemy, forward, right;
    VectorSubtract(enemy->r.currentOrigin, self->r.currentOrigin, to_enemy);
    VectorNormalize(to_enemy);
    
    AngleVectors(self->client->ps.viewangles, forward, right, NULL);
    
    switch (strafe_pattern[clientNum]) {
        case 0: // Circle strafe right
            ucmd->rightmove = 127;
            ucmd->forwardmove = 64;
            break;
            
        case 1: // Circle strafe left
            ucmd->rightmove = -127;
            ucmd->forwardmove = 64;
            break;
            
        case 2: // Zigzag
            if ((level.time / 300) % 2) {
                ucmd->rightmove = 127;
            } else {
                ucmd->rightmove = -127;
            }
            ucmd->forwardmove = 127;
            break;
            
        case 3: // Aggressive advance
            ucmd->forwardmove = 127;
            if ((level.time / 200) % 2) {
                ucmd->rightmove = 50;
            } else {
                ucmd->rightmove = -50;
            }
            break;
    }
}

/*
================
AI_CombatAim
Advanced aiming with prediction
================
*/
void AI_CombatAim(gentity_t *self, gentity_t *enemy, usercmd_t *ucmd) {
    combat_state_t *cs = &combat_states[self - g_entities];
    vec3_t aim_target, aim_dir;
    vec3_t angles;
    
    // Predict enemy position
    AI_CombatPredictEnemyPosition(enemy, aim_target, AIM_PREDICTION_STEPS);
    
    // Add some random error based on skill and distance
    float distance = Distance(self->r.currentOrigin, enemy->r.currentOrigin);
    float error_factor = (1.0f - cs->aim_accuracy) * (distance / 1000.0f);
    
    aim_target[0] += (rand() % 100 - 50) * error_factor;
    aim_target[1] += (rand() % 100 - 50) * error_factor;
    aim_target[2] += (rand() % 100 - 50) * error_factor;
    
    // Calculate aim direction
    VectorSubtract(aim_target, self->r.currentOrigin, aim_dir);
    VectorNormalize(aim_dir);
    vectoangles(aim_dir, angles);
    
    // Smooth aim adjustment
    float yaw_diff = AngleSubtract(angles[YAW], self->client->ps.viewangles[YAW]);
    float pitch_diff = AngleSubtract(angles[PITCH], self->client->ps.viewangles[PITCH]);
    
    // Limit aim speed for realism
    float max_turn_speed = 5.0f + cs->aim_accuracy * 10.0f;
    
    if (fabs(yaw_diff) > max_turn_speed) {
        yaw_diff = (yaw_diff > 0) ? max_turn_speed : -max_turn_speed;
    }
    if (fabs(pitch_diff) > max_turn_speed) {
        pitch_diff = (pitch_diff > 0) ? max_turn_speed : -max_turn_speed;
    }
    
    // Apply aim
    ucmd->angles[YAW] = ANGLE2SHORT(self->client->ps.viewangles[YAW] + yaw_diff);
    ucmd->angles[PITCH] = ANGLE2SHORT(self->client->ps.viewangles[PITCH] + pitch_diff);
}

/*
================
AI_CombatThink
Main combat thinking routine
================
*/
void AI_CombatThink(gentity_t *self, gentity_t *enemy, usercmd_t *ucmd) {
    combat_state_t *cs = &combat_states[self - g_entities];
    
    // Update combat state
    if (level.time - cs->last_think_time < COMBAT_THINK_INTERVAL) {
        return;
    }
    cs->last_think_time = level.time;
    
    if (!enemy || !enemy->client || enemy->client->ps.stats[STAT_HEALTH] <= 0) {
        cs->in_combat = qfalse;
        return;
    }
    
    cs->in_combat = qtrue;
    
    // Analyze threat
    cs->danger_level = AI_CombatAnalyzeThreat(self, enemy);
    
    // Adapt combat style based on situation
    float health_ratio = self->client->ps.stats[STAT_HEALTH] / 100.0f;
    
    if (health_ratio < 0.3f || cs->danger_level > 70.0f) {
        cs->combat_style = 1; // Defensive
    } else if (health_ratio > 0.7f && cs->danger_level < 40.0f) {
        cs->combat_style = 0; // Aggressive
    } else {
        cs->combat_style = 2; // Tactical
    }
    
    // Select best weapon
    if (level.time - cs->last_weapon_switch > WEAPON_SWITCH_DELAY) {
        int best_weapon = AI_CombatSelectBestWeapon(self, enemy);
        if (best_weapon != self->client->ps.weapon) {
            ucmd->weapon = best_weapon;
            cs->last_weapon_switch = level.time;
        }
    }
    
    // Dodge projectiles
    if (!AI_CombatDodgeProjectile(self, ucmd)) {
        // If not dodging, perform combat movement
        switch (cs->combat_style) {
            case 0: // Aggressive
                AI_CombatStrafe(self, enemy, ucmd);
                ucmd->forwardmove = 127;
                break;
                
            case 1: // Defensive
                // Retreat while strafing
                AI_CombatStrafe(self, enemy, ucmd);
                ucmd->forwardmove = -127;
                
                // Jump for evasion
                if ((level.time / 500) % 3 == 0) {
                    ucmd->upmove = 127;
                }
                break;
                
            case 2: // Tactical
                AI_CombatStrafe(self, enemy, ucmd);
                break;
        }
    }
    
    // Aim at enemy
    AI_CombatAim(self, enemy, ucmd);
    
    // Fire weapon
    float distance = Distance(self->r.currentOrigin, enemy->r.currentOrigin);
    vec3_t aim_dir, to_enemy;
    AngleVectors(self->client->ps.viewangles, aim_dir, NULL, NULL);
    VectorSubtract(enemy->r.currentOrigin, self->r.currentOrigin, to_enemy);
    VectorNormalize(to_enemy);
    
    float aim_dot = DotProduct(aim_dir, to_enemy);
    
    // Fire if aimed well enough
    float required_accuracy = 0.95f - cs->aim_accuracy * 0.2f;
    if (aim_dot > required_accuracy) {
        ucmd->buttons |= BUTTON_ATTACK;
    }
}

/*
================
AI_CombatInit
Initialize combat system for a bot
================
*/
void AI_CombatInit(int clientNum) {
    combat_state_t *cs = &combat_states[clientNum];
    
    memset(cs, 0, sizeof(combat_state_t));
    cs->aim_accuracy = 0.5f + (rand() % 50) / 100.0f; // 0.5 to 1.0
    cs->combat_style = 2; // Start with tactical
}

/*
================
AI_CombatUpdateStats
Update combat statistics
================
*/
void AI_CombatUpdateStats(int clientNum, qboolean killed_enemy) {
    combat_state_t *cs = &combat_states[clientNum];
    
    if (killed_enemy) {
        cs->kill_streak++;
        cs->death_streak = 0;
        
        // Improve aim slightly with kills
        cs->aim_accuracy += 0.01f;
        if (cs->aim_accuracy > 1.0f) cs->aim_accuracy = 1.0f;
    } else {
        cs->death_streak++;
        cs->kill_streak = 0;
        
        // Decrease aim slightly with deaths
        cs->aim_accuracy -= 0.005f;
        if (cs->aim_accuracy < 0.3f) cs->aim_accuracy = 0.3f;
    }
}

