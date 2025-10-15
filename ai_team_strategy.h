#ifndef __AI_TEAM_STRATEGY_H__
#define __AI_TEAM_STRATEGY_H__

#define ROLE_LEADER 0
#define ROLE_ATTACKER 1
#define ROLE_DEFENDER 2
#define ROLE_SUPPORT 3
#define NUM_ROLES 4

#define FORMATION_LINE 0
#define FORMATION_WEDGE 1
#define FORMATION_CIRCLE 2
#define FORMATION_SPREAD 3
#define NUM_FORMATIONS 4

#define STRATEGY_AGGRESSIVE 0
#define STRATEGY_DEFENSIVE 1
#define STRATEGY_BALANCED 2
#define NUM_STRATEGIES 3

typedef struct {
    int members[MAX_CLIENTS];
    int numMembers;
    int leader;
    int strategy;
    int formation;
    vec3_t rallyPoint;
    int objective;
    int lastUpdate;
} team_strategy_t;

void AI_InitTeamStrategy(void);
void AI_UpdateTeamStrategy(int team);
void AI_AssignTeamRoles(int team);
void AI_ExecuteFormation(bot_state_t *bs);
void AI_CoordinateAttack(int team, vec3_t target);
void AI_CoordinateDefense(int team, vec3_t position);
qboolean AI_ShouldHelpTeammate(bot_state_t *bs, int teammate);
void AI_TeamCommunication(bot_state_t *bs);

#endif
