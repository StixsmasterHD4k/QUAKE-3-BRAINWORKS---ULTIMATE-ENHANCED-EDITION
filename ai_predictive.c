/*
===========================================================================
PREDICTIVE COMBAT SYSTEM
Advanced trajectory prediction and interception
===========================================================================
*/

#include "g_local.h"
#include "ai_main.h"

#define PREDICTION_STEPS 100
#define TIME_STEP 0.05f

typedef struct {
    vec3_t position;
    vec3_t velocity;
    float time;
    float probability;
} prediction_t;

typedef struct {
    prediction_t steps[PREDICTION_STEPS];
    int numSteps;
    vec3_t intercept_point;
    float intercept_time;
} trajectory_t;

/*
====================
AI_PredictProjectile
Predict projectile trajectory with gravity and air resistance
====================
*/
void AI_PredictProjectile(vec3_t start, vec3_t velocity, float gravity, float drag, trajectory_t *traj) {
    vec3_t pos, vel;
    int i;
    float t = 0.0f;
    
    VectorCopy(start, pos);
    VectorCopy(velocity, vel);
    
    traj->numSteps = 0;
    
    for (i = 0; i < PREDICTION_STEPS; i++) {
        prediction_t *step = &traj->steps[traj->numSteps++];
        
        VectorCopy(pos, step->position);
        VectorCopy(vel, step->velocity);
        step->time = t;
        step->probability = 1.0f - (t / 10.0f); // Uncertainty increases with time
        
        // Update velocity
        vel[2] -= gravity * TIME_STEP;
        VectorScale(vel, 1.0f - drag * TIME_STEP, vel);
        
        // Update position
        VectorMA(pos, TIME_STEP, vel, pos);
        
        t += TIME_STEP;
        
        // Check for ground collision (simplified)
        if (pos[2] < 0.0f) {
            break;
        }
    }
}

/*
====================
AI_PredictEntityMovement
Predict enemy movement with behavioral modeling
====================
*/
void AI_PredictEntityMovement(gentity_t *ent, trajectory_t *traj) {
    vec3_t pos, vel, accel;
    int i;
    float t = 0.0f;
    
    VectorCopy(ent->r.currentOrigin, pos);
    VectorCopy(ent->s.pos.trDelta, vel);
    VectorClear(accel);
    
    // Estimate acceleration from recent behavior
    if (ent->client) {
        vec3_t forward, right;
        AngleVectors(ent->client->ps.viewangles, forward, right, NULL);
        
        // Predict movement based on input
        if (ent->client->pers.cmd.forwardmove != 0) {
            VectorMA(accel, ent->client->pers.cmd.forwardmove, forward, accel);
        }
        if (ent->client->pers.cmd.rightmove != 0) {
            VectorMA(accel, ent->client->pers.cmd.rightmove, right, accel);
        }
    }
    
    traj->numSteps = 0;
    
    for (i = 0; i < PREDICTION_STEPS; i++) {
        prediction_t *step = &traj->steps[traj->numSteps++];
        
        VectorCopy(pos, step->position);
        VectorCopy(vel, step->velocity);
        step->time = t;
        
        // Uncertainty model
        float noise = 0.01f * t * t;
        step->probability = exp(-noise);
        
        // Update velocity with acceleration and damping
        VectorMA(vel, TIME_STEP, accel, vel);
        VectorScale(vel, 0.95f, vel); // Damping
        
        // Update position
        VectorMA(pos, TIME_STEP, vel, pos);
        
        // Gravity
        pos[2] -= 400.0f * TIME_STEP * TIME_STEP;
        
        t += TIME_STEP;
    }
}

/*
====================
AI_CalculateInterceptPoint
Calculate optimal interception point
====================
*/
qboolean AI_CalculateInterceptPoint(vec3_t shooterPos, float projectileSpeed, trajectory_t *targetTraj, vec3_t intercept) {
    int i;
    float bestTime = -1.0f;
    vec3_t bestPoint;
    
    for (i = 0; i < targetTraj->numSteps; i++) {
        prediction_t *step = &targetTraj->steps[i];
        vec3_t delta;
        float distance, travelTime;
        
        VectorSubtract(step->position, shooterPos, delta);
        distance = VectorLength(delta);
        travelTime = distance / projectileSpeed;
        
        // Check if projectile can reach target at this time
        if (fabs(travelTime - step->time) < TIME_STEP * 2.0f) {
            if (bestTime < 0.0f || step->probability > targetTraj->steps[(int)(bestTime/TIME_STEP)].probability) {
                bestTime = step->time;
                VectorCopy(step->position, bestPoint);
            }
        }
    }
    
    if (bestTime >= 0.0f) {
        VectorCopy(bestPoint, intercept);
        return qtrue;
    }
    
    return qfalse;
}

/*
====================
AI_LeadTarget
Calculate lead angle for moving target
====================
*/
qboolean AI_LeadTarget(bot_state_t *bs, gentity_t *target, int weapon, vec3_t aimPoint) {
    trajectory_t targetTraj;
    float projectileSpeed;
    vec3_t muzzle;
    
    // Get projectile speed for weapon
    switch (weapon) {
        case WP_ROCKET_LAUNCHER: projectileSpeed = 900.0f; break;
        case WP_PLASMAGUN: projectileSpeed = 2000.0f; break;
        case WP_BFG: projectileSpeed = 2000.0f; break;
        case WP_GRENADE_LAUNCHER: projectileSpeed = 700.0f; break;
        default: projectileSpeed = 8000.0f; break;
    }
    
    // Predict target movement
    AI_PredictEntityMovement(target, &targetTraj);
    
    // Calculate muzzle position
    VectorCopy(bs->origin, muzzle);
    muzzle[2] += bs->cur_ps.viewheight;
    
    // Calculate intercept
    if (AI_CalculateInterceptPoint(muzzle, projectileSpeed, &targetTraj, aimPoint)) {
        return qtrue;
    }
    
    // Fallback to current position
    VectorCopy(target->r.currentOrigin, aimPoint);
    return qfalse;
}

/*
====================
AI_PredictDodge
Predict incoming projectiles and calculate dodge vector
====================
*/
qboolean AI_PredictDodge(bot_state_t *bs, vec3_t dodgeDir) {
    int i;
    gentity_t *ent;
    vec3_t closest;
    float closestDist = 999999.0f;
    qboolean threat = qfalse;
    
    VectorClear(dodgeDir);
    
    // Scan for incoming projectiles
    for (i = MAX_CLIENTS; i < level.num_entities; i++) {
        ent = &g_entities[i];
        
        if (!ent->inuse) continue;
        if (ent->s.eType != ET_MISSILE) continue;
        
        // Predict projectile trajectory
        trajectory_t proj;
        AI_PredictProjectile(ent->r.currentOrigin, ent->s.pos.trDelta, 400.0f, 0.0f, &proj);
        
        // Find closest approach to bot
        int j;
        for (j = 0; j < proj.numSteps; j++) {
            vec3_t delta;
            VectorSubtract(proj.steps[j].position, bs->origin, delta);
            float dist = VectorLength(delta);
            
            if (dist < closestDist) {
                closestDist = dist;
                VectorCopy(proj.steps[j].position, closest);
            }
        }
        
        // If projectile will pass close, calculate dodge direction
        if (closestDist < 200.0f) {
            vec3_t perpendicular;
            VectorSubtract(bs->origin, closest, perpendicular);
            VectorNormalize(perpendicular);
            VectorAdd(dodgeDir, perpendicular, dodgeDir);
            threat = qtrue;
        }
    }
    
    if (threat) {
        VectorNormalize(dodgeDir);
        return qtrue;
    }
    
    return qfalse;
}

/*
====================
AI_PredictCombatOutcome
Simulate combat outcome
====================
*/
float AI_PredictCombatOutcome(bot_state_t *bs, int enemy) {
    gentity_t *bot = &g_entities[bs->client];
    gentity_t *ent = &g_entities[enemy];
    
    float botHealth = bot->health;
    float botArmor = bs->inventory[INVENTORY_ARMOR];
    float botWeaponPower = bs->weaponnum * 10.0f;
    
    float enemyHealth = ent->health;
    float enemyArmor = ent->client ? ent->client->ps.stats[STAT_ARMOR] : 0.0f;
    float enemyWeaponPower = ent->client ? ent->client->ps.weapon * 10.0f : 50.0f;
    
    // Calculate effective health
    float botEffective = botHealth + botArmor * 0.66f;
    float enemyEffective = enemyHealth + enemyArmor * 0.66f;
    
    // Calculate damage rates
    float botDPS = botWeaponPower * bs->accuracy;
    float enemyDPS = enemyWeaponPower * 0.5f; // Estimate
    
    // Simulate time to kill
    float botTTK = enemyEffective / (botDPS + 0.1f);
    float enemyTTK = botEffective / (enemyDPS + 0.1f);
    
    // Return advantage (-1 to 1)
    return (enemyTTK - botTTK) / (enemyTTK + botTTK + 0.1f);
}
