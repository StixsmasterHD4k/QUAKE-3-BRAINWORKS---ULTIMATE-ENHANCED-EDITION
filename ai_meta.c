/*
===========================================================================
META-LEARNING SYSTEM
Learning to learn - adaptive strategy selection
===========================================================================
*/

#include "g_local.h"
#include "ai_main.h"

#define MAX_STRATEGIES 16
#define STRATEGY_MEMORY 1000

typedef struct {
    int strategyID;
    float performance;
    int timeUsed;
    int kills;
    int deaths;
    int context[8];
} strategyMemory_t;

typedef struct {
    int id;
    float successRate;
    float avgReward;
    int timesUsed;
    int timesSuccessful;
    float explorationBonus;
} strategy_t;

static strategy_t strategies[MAX_STRATEGIES];
static strategyMemory_t memory[MAX_CLIENTS][STRATEGY_MEMORY];
static int memoryIndex[MAX_CLIENTS];

/*
====================
AI_MetaInit
====================
*/
void AI_MetaInit(void) {
    int i;
    
    for (i = 0; i < MAX_STRATEGIES; i++) {
        strategies[i].id = i;
        strategies[i].successRate = 0.5f;
        strategies[i].avgReward = 0.0f;
        strategies[i].timesUsed = 0;
        strategies[i].timesSuccessful = 0;
        strategies[i].explorationBonus = 1.0f;
    }
}

/*
====================
AI_MetaGetContext
Extract context features from game state
====================
*/
void AI_MetaGetContext(bot_state_t *bs, int *context) {
    context[0] = bs->inventory[INVENTORY_HEALTH] / 25;
    context[1] = bs->inventory[INVENTORY_ARMOR] / 25;
    context[2] = bs->weaponnum;
    context[3] = (bs->enemy >= 0) ? 1 : 0;
    context[4] = g_gametype.integer;
    context[5] = (bs->ltgtype != 0) ? 1 : 0;
    context[6] = level.time / 10000;
    context[7] = bs->inventory[INVENTORY_WEAPON0 + WP_RAILGUN] > 0 ? 1 : 0;
}

/*
====================
AI_MetaSelectStrategy
Select strategy using UCB1 algorithm
====================
*/
int AI_MetaSelectStrategy(int botnum, bot_state_t *bs) {
    int context[8];
    int totalUses = 0;
    int i;
    float bestScore = -999999.0f;
    int bestStrategy = 0;
    
    AI_MetaGetContext(bs, context);
    
    // Calculate total uses
    for (i = 0; i < MAX_STRATEGIES; i++) {
        totalUses += strategies[i].timesUsed;
    }
    
    if (totalUses == 0) totalUses = 1;
    
    // UCB1 selection
    for (i = 0; i < MAX_STRATEGIES; i++) {
        float exploitation = strategies[i].avgReward;
        float exploration = sqrt(2.0f * log(totalUses) / (strategies[i].timesUsed + 1));
        float score = exploitation + exploration * strategies[i].explorationBonus;
        
        // Context matching bonus
        int j;
        for (j = 0; j < memoryIndex[botnum]; j++) {
            strategyMemory_t *mem = &memory[botnum][j];
            if (mem->strategyID != i) continue;
            
            int contextMatch = 0;
            int k;
            for (k = 0; k < 8; k++) {
                if (mem->context[k] == context[k]) {
                    contextMatch++;
                }
            }
            
            if (contextMatch >= 5) {
                score += mem->performance * 0.1f;
            }
        }
        
        if (score > bestScore) {
            bestScore = score;
            bestStrategy = i;
        }
    }
    
    strategies[bestStrategy].timesUsed++;
    
    return bestStrategy;
}

/*
====================
AI_MetaRecordOutcome
====================
*/
void AI_MetaRecordOutcome(int botnum, int strategyID, float performance, int kills, int deaths) {
    strategy_t *strat = &strategies[strategyID];
    strategyMemory_t *mem;
    
    // Update strategy statistics
    strat->avgReward = (strat->avgReward * strat->timesUsed + performance) / (strat->timesUsed + 1.0f);
    
    if (performance > 0.0f) {
        strat->timesSuccessful++;
    }
    
    strat->successRate = (float)strat->timesSuccessful / (strat->timesUsed + 1.0f);
    
    // Reduce exploration bonus over time
    strat->explorationBonus *= 0.99f;
    
    // Store in memory
    if (memoryIndex[botnum] >= STRATEGY_MEMORY) {
        memoryIndex[botnum] = 0;
    }
    
    mem = &memory[botnum][memoryIndex[botnum]++];
    mem->strategyID = strategyID;
    mem->performance = performance;
    mem->timeUsed = level.time;
    mem->kills = kills;
    mem->deaths = deaths;
    
    // Store context
    bot_state_t *bs;
    if (botnum < MAX_CLIENTS && (bs = BotAIGetBotState(botnum)) != NULL) {
        AI_MetaGetContext(bs, mem->context);
    }
}

/*
====================
AI_MetaAdapt
Adapt strategies based on performance
====================
*/
void AI_MetaAdapt(int botnum) {
    int i, j;
    float totalPerformance = 0.0f;
    int recentMemory = 50;
    
    // Analyze recent memory
    int startIdx = (memoryIndex[botnum] - recentMemory + STRATEGY_MEMORY) % STRATEGY_MEMORY;
    
    for (i = 0; i < recentMemory; i++) {
        int idx = (startIdx + i) % STRATEGY_MEMORY;
        strategyMemory_t *mem = &memory[botnum][idx];
        
        if (mem->strategyID >= 0 && mem->strategyID < MAX_STRATEGIES) {
            strategy_t *strat = &strategies[mem->strategyID];
            
            // Adjust based on recent performance
            if (mem->performance < -0.5f) {
                strat->explorationBonus += 0.1f; // Encourage trying other strategies
            } else if (mem->performance > 0.5f) {
                strat->explorationBonus = fmax(0.1f, strat->explorationBonus - 0.05f);
            }
        }
        
        totalPerformance += mem->performance;
    }
    
    // If overall performance is poor, increase exploration
    if (totalPerformance / recentMemory < -0.2f) {
        for (i = 0; i < MAX_STRATEGIES; i++) {
            strategies[i].explorationBonus += 0.2f;
        }
    }
}

/*
====================
AI_MetaGetBestStrategy
====================
*/
int AI_MetaGetBestStrategy(void) {
    int i;
    int best = 0;
    float bestRate = strategies[0].successRate;
    
    for (i = 1; i < MAX_STRATEGIES; i++) {
        if (strategies[i].successRate > bestRate) {
            best = i;
            bestRate = strategies[i].successRate;
        }
    }
    
    return best;
}

/*
====================
AI_MetaGetStrategyStats
====================
*/
void AI_MetaGetStrategyStats(int strategyID, float *successRate, float *avgReward, int *timesUsed) {
    if (strategyID < 0 || strategyID >= MAX_STRATEGIES) {
        *successRate = 0.0f;
        *avgReward = 0.0f;
        *timesUsed = 0;
        return;
    }
    
    *successRate = strategies[strategyID].successRate;
    *avgReward = strategies[strategyID].avgReward;
    *timesUsed = strategies[strategyID].timesUsed;
}
