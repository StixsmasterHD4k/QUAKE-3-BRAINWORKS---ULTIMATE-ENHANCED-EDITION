#ifndef __AI_PERSONALITY_H__
#define __AI_PERSONALITY_H__

#define PERSONALITY_AGGRESSIVE 0
#define PERSONALITY_DEFENSIVE 1
#define PERSONALITY_TACTICAL 2
#define PERSONALITY_RUSHER 3
#define PERSONALITY_SNIPER 4
#define PERSONALITY_BALANCED 5
#define NUM_PERSONALITIES 6

typedef struct {
    int type;
    float aggression;
    float cautiousness;
    float teamwork;
    float camping_tendency;
    float accuracy_modifier;
    float reaction_time_modifier;
    float item_priority;
    float powerup_priority;
} bot_personality_t;

void AI_InitPersonality(int botNum, int personality_type);
void AI_ApplyPersonality(bot_state_t *bs);
bot_personality_t *AI_GetPersonality(int botNum);
qboolean AI_PersonalityDecision(bot_state_t *bs, int decision_type);

#endif
