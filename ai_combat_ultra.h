#ifndef __AI_COMBAT_ULTRA_H
#define __AI_COMBAT_ULTRA_H

void AI_CombatThink(gentity_t *self, gentity_t *enemy, usercmd_t *ucmd);
void AI_CombatInit(int clientNum);
void AI_CombatUpdateStats(int clientNum, qboolean killed_enemy);

#endif
