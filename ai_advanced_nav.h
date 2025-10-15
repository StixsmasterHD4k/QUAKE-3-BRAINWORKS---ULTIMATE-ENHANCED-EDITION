#ifndef __AI_ADVANCED_NAV_H__
#define __AI_ADVANCED_NAV_H__

qboolean AI_CanRocketJump(bot_state_t *bs);
void AI_ExecuteRocketJump(bot_state_t *bs, vec3_t target);
void AI_StrafeJump(bot_state_t *bs, vec3_t moveDir);
qboolean AI_CanWallClimb(bot_state_t *bs, vec3_t wallNormal);
qboolean AI_FindAlternateRoute(bot_state_t *bs, vec3_t goal, vec3_t waypoint);
void AI_AvoidObstacles(bot_state_t *bs, vec3_t moveDir);
qboolean AI_PerformParkourMove(bot_state_t *bs);
void AI_OptimizeMovement(bot_state_t *bs);

#endif
