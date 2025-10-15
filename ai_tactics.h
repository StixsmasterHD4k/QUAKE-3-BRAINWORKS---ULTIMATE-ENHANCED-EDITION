/*
===========================================================================
AI ADVANCED TACTICS - ULTIMATE ENHANCED EDITION
Strategic positioning, cover usage, flanking, and combat tactics
===========================================================================
*/

#ifndef __AI_TACTICS_H__
#define __AI_TACTICS_H__

#define MAX_TACTICAL_POSITIONS 64
#define COVER_SEARCH_RADIUS 1500.0f
#define FLANK_DISTANCE 800.0f

// Tactical position types
typedef enum {
    TPOS_COVER,
    TPOS_HIGHGROUND,
    TPOS_AMBUSH,
    TPOS_RETREAT,
    TPOS_FLANK,
    TPOS_CHOKEPOINT
} tactical_pos_type_t;

// Cover quality
typedef struct {
    vec3_t origin;
    float quality;
    int last_used_time;
    tactical_pos_type_t type;
    qboolean occupied;
} tactical_position_t;

// API Functions
void AI_InitTacticalSystem(void);
qboolean AI_FindCoverPosition(bot_state_t *bs, vec3_t coverPos);
qboolean AI_FindFlankPosition(bot_state_t *bs, vec3_t enemy_pos, vec3_t flankPos);
float AI_EvaluateTacticalPosition(bot_state_t *bs, vec3_t pos);
qboolean AI_FindHighGround(bot_state_t *bs, vec3_t highPos);
void AI_ExecuteSuppressiveFire(bot_state_t *bs);
qboolean AI_ShouldRetreat(bot_state_t *bs);
void AI_PerformTacticalRetreat(bot_state_t *bs);
qboolean AI_CanFlankEnemy(bot_state_t *bs);
void AI_UpdateTacticalSituation(bot_state_t *bs);

#endif
