#include "g_local.h"
#include "ai_main.h"
#include "ai_dmq3.h"
#include "ai_advanced_nav.h"
#include <math.h>

qboolean AI_CanRocketJump(bot_state_t *bs) {
    return (bs->inventory[INVENTORY_ROCKETLAUNCHER] &&
            bs->cur_ps.ammo[AMMO_ROCKETS] > 3 &&
            bs->inventory[INVENTORY_HEALTH] > 60);
}

void AI_ExecuteRocketJump(bot_state_t *bs, vec3_t target) {
    vec3_t dir, aimPoint;
    
    VectorSubtract(target, bs->origin, dir);
    dir[2] = 0;
    VectorNormalize(dir);
    
    // Aim behind and down
    VectorMA(bs->origin, -80.0f, dir, aimPoint);
    aimPoint[2] = bs->origin[2] - 80.0f;
    
    // Switch to rocket launcher
    trap_EA_SelectWeapon(bs->client, WP_ROCKET_LAUNCHER);
    
    // Wait a frame for weapon switch
    if (bs->weaponnum == WP_ROCKET_LAUNCHER) {
        // Look down
        vec3_t angles;
        VectorSubtract(aimPoint, bs->eye, dir);
        vectoangles(dir, angles);
        
        VectorCopy(angles, bs->ideal_viewangles);
        
        // Jump and shoot
        trap_EA_Jump(bs->client);
        trap_EA_Attack(bs->client);
    }
}

void AI_StrafeJump(bot_state_t *bs, vec3_t moveDir) {
    vec3_t velocity;
    float speed;
    
    VectorCopy(bs->cur_ps.velocity, velocity);
    velocity[2] = 0;
    speed = VectorLength(velocity);
    
    // Jump when on ground
    if (bs->cur_ps.groundEntityNum != ENTITYNUM_NONE) {
        trap_EA_Jump(bs->client);
    }
    
    // Strafe in optimal direction
    if (speed > 100.0f) {
        vec3_t forward, right;
        
        VectorNormalize2(velocity, forward);
        right[0] = -forward[1];
        right[1] = forward[0];
        right[2] = 0;
        
        // Alternate strafe direction
        int strafe_dir = (level.time / 100) % 2;
        float strafe_amount = 127.0f * (strafe_dir ? 1.0f : -1.0f);
        
        trap_EA_Move(bs->client, moveDir, 400);
        trap_EA_MoveRight(bs->client, strafe_amount);
    }
}

qboolean AI_CanWallClimb(bot_state_t *bs, vec3_t wallNormal) {
    vec3_t forward, testPoint;
    trace_t trace;
    
    AngleVectors(bs->viewangles, forward, NULL, NULL);
    VectorMA(bs->origin, 64, forward, testPoint);
    
    trap_Trace(&trace, bs->origin, NULL, NULL, testPoint, bs->entitynum, MASK_SOLID);
    
    if (trace.fraction < 1.0f && trace.plane.normal[2] < 0.7f) {
        VectorCopy(trace.plane.normal, wallNormal);
        return qtrue;
    }
    
    return qfalse;
}

qboolean AI_FindAlternateRoute(bot_state_t *bs, vec3_t goal, vec3_t waypoint) {
    int i;
    vec3_t testPoints[8];
    float bestDist = 999999.0f;
    int bestPoint = -1;
    trace_t trace;
    
    for (i = 0; i < 8; i++) {
        float angle = (float)i / 8.0f * 2.0f * M_PI;
        float radius = 400.0f;
        
        testPoints[i][0] = bs->origin[0] + cos(angle) * radius;
        testPoints[i][1] = bs->origin[1] + sin(angle) * radius;
        testPoints[i][2] = bs->origin[2];
        
        // Check if path is clear
        trap_Trace(&trace, bs->origin, NULL, NULL, testPoints[i], bs->entitynum, MASK_SOLID);
        
        if (trace.fraction > 0.5f) {
            float dist_to_goal = Distance(testPoints[i], goal);
            
            if (dist_to_goal < bestDist) {
                bestDist = dist_to_goal;
                bestPoint = i;
            }
        }
    }
    
    if (bestPoint >= 0) {
        VectorCopy(testPoints[bestPoint], waypoint);
        return qtrue;
    }
    
    return qfalse;
}

void AI_AvoidObstacles(bot_state_t *bs, vec3_t moveDir) {
    vec3_t forward, right, testPos;
    trace_t trace;
    
    AngleVectors(bs->viewangles, forward, right, NULL);
    VectorMA(bs->origin, 150.0f, forward, testPos);
    
    trap_Trace(&trace, bs->origin, NULL, NULL, testPos, bs->entitynum, MASK_SOLID);
    
    if (trace.fraction < 0.8f) {
        // Obstacle ahead, try left/right
        vec3_t leftPos, rightPos;
        trace_t leftTrace, rightTrace;
        
        VectorMA(bs->origin, 150.0f, right, rightPos);
        VectorMA(bs->origin, -150.0f, right, leftPos);
        
        trap_Trace(&leftTrace, bs->origin, NULL, NULL, leftPos, bs->entitynum, MASK_SOLID);
        trap_Trace(&rightTrace, bs->origin, NULL, NULL, rightPos, bs->entitynum, MASK_SOLID);
        
        if (rightTrace.fraction > leftTrace.fraction) {
            trap_EA_MoveRight(bs->client, 127);
        } else {
            trap_EA_MoveRight(bs->client, -127);
        }
    }
}

qboolean AI_PerformParkourMove(bot_state_t *bs) {
    vec3_t wallNormal;
    
    // Try wall climb
    if (AI_CanWallClimb(bs, wallNormal)) {
        trap_EA_Jump(bs->client);
        trap_EA_Move(bs->client, wallNormal, 400);
        return qtrue;
    }
    
    // Try rocket jump if available
    if (AI_CanRocketJump(bs)) {
        vec3_t target;
        VectorCopy(bs->teamgoal.origin, target);
        if (VectorLength(target) > 0) {
            AI_ExecuteRocketJump(bs, target);
            return qtrue;
        }
    }
    
    return qfalse;
}

void AI_OptimizeMovement(bot_state_t *bs) {
    float speed = VectorLength(bs->cur_ps.velocity);
    
    // Use strafe jumping for speed
    if (speed < 320.0f && bs->cur_ps.groundEntityNum != ENTITYNUM_NONE) {
        vec3_t moveDir;
        AngleVectors(bs->viewangles, moveDir, NULL, NULL);
        AI_StrafeJump(bs, moveDir);
    }
    
    // Avoid obstacles
    vec3_t forward;
    AngleVectors(bs->viewangles, forward, NULL, NULL);
    AI_AvoidObstacles(bs, forward);
}
