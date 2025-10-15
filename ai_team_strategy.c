#include "g_local.h"
#include "ai_main.h"
#include "ai_dmq3.h"
#include "ai_team.h"
#include "ai_team_strategy.h"
#include <math.h>

static team_strategy_t team_strategies[TEAM_NUM_TEAMS];

void AI_InitTeamStrategy(void) {
    memset(team_strategies, 0, sizeof(team_strategies));
}

void AI_UpdateTeamStrategy(int team) {
    team_strategy_t *strat;
    int i, count;
    
    if (team != TEAM_RED && team != TEAM_BLUE) return;
    
    strat = &team_strategies[team];
    
    // Find all team members
    count = 0;
    for (i = 0; i < level.maxclients; i++) {
        gentity_t *ent = &g_entities[i];
        if (!ent->inuse || !ent->client) continue;
        if (ent->client->sess.sessionTeam != team) continue;
        
        if (count < MAX_CLIENTS) {
            strat->members[count++] = i;
        }
    }
    strat->numMembers = count;
    
    // Elect leader (highest score)
    if (count > 0) {
        int best_score = -999999;
        int leader_idx = 0;
        
        for (i = 0; i < count; i++) {
            gentity_t *ent = &g_entities[strat->members[i]];
            int score = ent->client->ps.persistant[PERS_SCORE];
            
            if (score > best_score) {
                best_score = score;
                leader_idx = i;
            }
        }
        
        strat->leader = strat->members[leader_idx];
    }
    
    // Determine strategy based on score difference
    int our_score = level.teamScores[team];
    int their_score = level.teamScores[team == TEAM_RED ? TEAM_BLUE : TEAM_RED];
    int score_diff = our_score - their_score;
    
    if (score_diff > 3) {
        strat->strategy = STRATEGY_DEFENSIVE;
        strat->formation = FORMATION_SPREAD;
    } else if (score_diff < -3) {
        strat->strategy = STRATEGY_AGGRESSIVE;
        strat->formation = FORMATION_WEDGE;
    } else {
        strat->strategy = STRATEGY_BALANCED;
        strat->formation = FORMATION_LINE;
    }
    
    strat->lastUpdate = level.time;
}

void AI_AssignTeamRoles(int team) {
    team_strategy_t *strat;
    int i;
    bot_state_t *bs;
    
    if (team != TEAM_RED && team != TEAM_BLUE) return;
    
    strat = &team_strategies[team];
    
    for (i = 0; i < strat->numMembers; i++) {
        bs = BotFromClient(strat->members[i]);
        if (!bs) continue;
        
        if (strat->members[i] == strat->leader) {
            bs->teamrole = ROLE_LEADER;
        } else if (i < strat->numMembers / 2) {
            bs->teamrole = ROLE_ATTACKER;
        } else if (i < 3 * strat->numMembers / 4) {
            bs->teamrole = ROLE_DEFENDER;
        } else {
            bs->teamrole = ROLE_SUPPORT;
        }
    }
}

void AI_ExecuteFormation(bot_state_t *bs) {
    team_strategy_t *strat;
    int team, myIndex, i;
    vec3_t formationPos;
    
    team = bs->sess.sessionTeam;
    if (team != TEAM_RED && team != TEAM_BLUE) return;
    
    strat = &team_strategies[team];
    
    // Find our index
    myIndex = -1;
    for (i = 0; i < strat->numMembers; i++) {
        if (strat->members[i] == bs->client) {
            myIndex = i;
            break;
        }
    }
    
    if (myIndex < 0) return;
    
    // Calculate formation position relative to leader
    VectorCopy(strat->rallyPoint, formationPos);
    
    switch(strat->formation) {
        case FORMATION_LINE:
            formationPos[0] += (myIndex - strat->numMembers/2) * 150.0f;
            break;
            
        case FORMATION_WEDGE:
            {
                int row = myIndex / 2;
                int col = myIndex % 2;
                formationPos[1] += row * 200.0f;
                formationPos[0] += (col ? 1 : -1) * row * 75.0f;
            }
            break;
            
        case FORMATION_CIRCLE:
            {
                float angle = (float)myIndex / strat->numMembers * 2.0f * M_PI;
                float radius = 300.0f;
                formationPos[0] += cos(angle) * radius;
                formationPos[1] += sin(angle) * radius;
            }
            break;
            
        case FORMATION_SPREAD:
            {
                float angle = (float)myIndex / strat->numMembers * 2.0f * M_PI;
                float radius = 400.0f + (rand() % 200);
                formationPos[0] += cos(angle) * radius;
                formationPos[1] += sin(angle) * radius;
            }
            break;
    }
    
    // Move toward formation position if not there
    float dist = Distance(bs->origin, formationPos);
    if (dist > 100.0f) {
        vec3_t dir;
        VectorSubtract(formationPos, bs->origin, dir);
        vectoangles(dir, bs->ideal_viewangles);
    }
}

void AI_CoordinateAttack(int team, vec3_t target) {
    team_strategy_t *strat;
    int i;
    bot_state_t *bs;
    
    if (team != TEAM_RED && team != TEAM_BLUE) return;
    
    strat = &team_strategies[team];
    VectorCopy(target, strat->rallyPoint);
    
    // Send attackers to target
    for (i = 0; i < strat->numMembers; i++) {
        bs = BotFromClient(strat->members[i]);
        if (!bs) continue;
        
        if (bs->teamrole == ROLE_ATTACKER || bs->teamrole == ROLE_LEADER) {
            VectorCopy(target, bs->teamgoal.origin);
            bs->teamgoal_time = level.time + 30000;
        }
    }
}

void AI_CoordinateDefense(int team, vec3_t position) {
    team_strategy_t *strat;
    int i;
    bot_state_t *bs;
    
    if (team != TEAM_RED && team != TEAM_BLUE) return;
    
    strat = &team_strategies[team];
    VectorCopy(position, strat->rallyPoint);
    
    // Send defenders to position
    for (i = 0; i < strat->numMembers; i++) {
        bs = BotFromClient(strat->members[i]);
        if (!bs) continue;
        
        if (bs->teamrole == ROLE_DEFENDER) {
            VectorCopy(position, bs->teamgoal.origin);
            bs->ltgtype = LTG_DEFENDKEYAREA;
        }
    }
}

qboolean AI_ShouldHelpTeammate(bot_state_t *bs, int teammate) {
    gentity_t *mate;
    float dist;
    
    if (!TeamPlayIsOn()) return qfalse;
    if (teammate < 0 || teammate >= MAX_CLIENTS) return qfalse;
    if (teammate == bs->client) return qfalse;
    
    mate = &g_entities[teammate];
    if (!mate->inuse || !mate->client) return qfalse;
    if (mate->client->sess.sessionTeam != bs->sess.sessionTeam) return qfalse;
    
    // Check distance
    dist = Distance(bs->origin, mate->r.currentOrigin);
    if (dist > 1500.0f) return qfalse;
    
    // Check if teammate is in combat
    if (mate->client->ps.stats[STAT_HEALTH] < 50) {
        return qtrue;
    }
    
    // Check if teammate needs backup
    if (mate->enemy && mate->enemy->client) {
        return qtrue;
    }
    
    return qfalse;
}

void AI_TeamCommunication(bot_state_t *bs) {
    team_strategy_t *strat;
    int team;
    
    team = bs->sess.sessionTeam;
    if (team != TEAM_RED && team != TEAM_BLUE) return;
    
    strat = &team_strategies[team];
    
    // Leader sends orders
    if (bs->client == strat->leader) {
        if (level.time - bs->teammessage_time > 15000) {
            // Send team message based on strategy
            switch(strat->strategy) {
                case STRATEGY_AGGRESSIVE:
                    BotVoiceChatOnly(bs, -1, VOICECHAT_ONOFFENSE);
                    break;
                case STRATEGY_DEFENSIVE:
                    BotVoiceChatOnly(bs, -1, VOICECHAT_ONDEFENSE);
                    break;
            }
            bs->teammessage_time = level.time;
        }
    }
    
    // Request help when needed
    if (bs->inventory[INVENTORY_HEALTH] < 40 && bs->enemy >= 0) {
        if (level.time - bs->teammessage_time > 10000) {
            BotVoiceChatOnly(bs, -1, VOICECHAT_HELP);
            bs->teammessage_time = level.time;
        }
    }
}
