/*
===========================================================================
ADVANCED FUZZY LOGIC SYSTEM
Multi-dimensional fuzzy reasoning for tactical decisions
===========================================================================
*/

#include "g_local.h"
#include "ai_main.h"

#define MAX_FUZZY_VARS 32
#define MAX_FUZZY_SETS 8
#define MAX_FUZZY_RULES 256

typedef struct {
    float low;
    float mid;
    float high;
    float value;
} fuzzySet_t;

typedef struct {
    fuzzySet_t sets[MAX_FUZZY_SETS];
    int numSets;
    char name[32];
} fuzzyVariable_t;

typedef struct {
    int inputVar[4];
    int inputSet[4];
    int outputVar;
    int outputSet;
    float weight;
} fuzzyRule_t;

static fuzzyVariable_t fuzzyVars[MAX_FUZZY_VARS];
static fuzzyRule_t fuzzyRules[MAX_FUZZY_RULES];
static int numFuzzyVars = 0;
static int numFuzzyRules = 0;

/*
====================
AI_FuzzyInit
====================
*/
void AI_FuzzyInit(void) {
    numFuzzyVars = 0;
    numFuzzyRules = 0;
    
    // Health variable
    fuzzyVariable_t *health = &fuzzyVars[numFuzzyVars++];
    Q_strncpyz(health->name, "health", sizeof(health->name));
    health->numSets = 3;
    health->sets[0].low = 0.0f; health->sets[0].mid = 0.0f; health->sets[0].high = 50.0f;
    health->sets[1].low = 25.0f; health->sets[1].mid = 50.0f; health->sets[1].high = 75.0f;
    health->sets[2].low = 50.0f; health->sets[2].mid = 100.0f; health->sets[2].high = 200.0f;
    
    // Distance variable
    fuzzyVariable_t *distance = &fuzzyVars[numFuzzyVars++];
    Q_strncpyz(distance->name, "distance", sizeof(distance->name));
    distance->numSets = 3;
    distance->sets[0].low = 0.0f; distance->sets[0].mid = 0.0f; distance->sets[0].high = 500.0f;
    distance->sets[1].low = 300.0f; distance->sets[1].mid = 800.0f; distance->sets[1].high = 1500.0f;
    distance->sets[2].low = 1000.0f; distance->sets[2].mid = 2000.0f; distance->sets[2].high = 4000.0f;
    
    // Aggression variable
    fuzzyVariable_t *aggression = &fuzzyVars[numFuzzyVars++];
    Q_strncpyz(aggression->name, "aggression", sizeof(aggression->name));
    aggression->numSets = 5;
    aggression->sets[0].low = 0.0f; aggression->sets[0].mid = 0.0f; aggression->sets[0].high = 0.3f;
    aggression->sets[1].low = 0.1f; aggression->sets[1].mid = 0.3f; aggression->sets[1].high = 0.5f;
    aggression->sets[2].low = 0.3f; aggression->sets[2].mid = 0.5f; aggression->sets[2].high = 0.7f;
    aggression->sets[3].low = 0.5f; aggression->sets[3].mid = 0.7f; aggression->sets[3].high = 0.9f;
    aggression->sets[4].low = 0.7f; aggression->sets[4].mid = 1.0f; aggression->sets[4].high = 1.0f;
}

/*
====================
AI_FuzzyMembership
Calculate membership value using triangular function
====================
*/
float AI_FuzzyMembership(float value, fuzzySet_t *set) {
    if (value <= set->low || value >= set->high) {
        return 0.0f;
    }
    
    if (value <= set->mid) {
        return (value - set->low) / (set->mid - set->low + 0.001f);
    } else {
        return (set->high - value) / (set->high - set->mid + 0.001f);
    }
}

/*
====================
AI_FuzzyFuzzify
Convert crisp input to fuzzy values
====================
*/
void AI_FuzzyFuzzify(int varIndex, float crispValue) {
    if (varIndex < 0 || varIndex >= numFuzzyVars) return;
    
    fuzzyVariable_t *var = &fuzzyVars[varIndex];
    int i;
    
    for (i = 0; i < var->numSets; i++) {
        var->sets[i].value = AI_FuzzyMembership(crispValue, &var->sets[i]);
    }
}

/*
====================
AI_FuzzyAddRule
====================
*/
void AI_FuzzyAddRule(int inVar1, int inSet1, int inVar2, int inSet2, int outVar, int outSet, float weight) {
    if (numFuzzyRules >= MAX_FUZZY_RULES) return;
    
    fuzzyRule_t *rule = &fuzzyRules[numFuzzyRules++];
    rule->inputVar[0] = inVar1;
    rule->inputSet[0] = inSet1;
    rule->inputVar[1] = inVar2;
    rule->inputSet[1] = inSet2;
    rule->inputVar[2] = -1;
    rule->inputVar[3] = -1;
    rule->outputVar = outVar;
    rule->outputSet = outSet;
    rule->weight = weight;
}

/*
====================
AI_FuzzyEvaluateRule
====================
*/
float AI_FuzzyEvaluateRule(fuzzyRule_t *rule) {
    float minValue = 1.0f;
    int i;
    
    for (i = 0; i < 4 && rule->inputVar[i] >= 0; i++) {
        int varIdx = rule->inputVar[i];
        int setIdx = rule->inputSet[i];
        
        if (varIdx >= numFuzzyVars) continue;
        if (setIdx >= fuzzyVars[varIdx].numSets) continue;
        
        float value = fuzzyVars[varIdx].sets[setIdx].value;
        if (value < minValue) {
            minValue = value;
        }
    }
    
    return minValue * rule->weight;
}

/*
====================
AI_FuzzyInference
Apply all rules and get fuzzy output
====================
*/
void AI_FuzzyInference(void) {
    int i;
    
    // Clear output values
    for (i = 0; i < numFuzzyVars; i++) {
        int j;
        for (j = 0; j < fuzzyVars[i].numSets; j++) {
            fuzzyVars[i].sets[j].value = 0.0f;
        }
    }
    
    // Apply all rules
    for (i = 0; i < numFuzzyRules; i++) {
        fuzzyRule_t *rule = &fuzzyRules[i];
        float ruleStrength = AI_FuzzyEvaluateRule(rule);
        
        if (rule->outputVar >= 0 && rule->outputVar < numFuzzyVars) {
            if (rule->outputSet >= 0 && rule->outputSet < fuzzyVars[rule->outputVar].numSets) {
                fuzzyVariable_t *outVar = &fuzzyVars[rule->outputVar];
                if (ruleStrength > outVar->sets[rule->outputSet].value) {
                    outVar->sets[rule->outputSet].value = ruleStrength;
                }
            }
        }
    }
}

/*
====================
AI_FuzzyDefuzzify
Convert fuzzy output to crisp value using centroid method
====================
*/
float AI_FuzzyDefuzzify(int varIndex) {
    if (varIndex < 0 || varIndex >= numFuzzyVars) return 0.0f;
    
    fuzzyVariable_t *var = &fuzzyVars[varIndex];
    float numerator = 0.0f;
    float denominator = 0.0f;
    int i;
    
    for (i = 0; i < var->numSets; i++) {
        fuzzySet_t *set = &var->sets[i];
        numerator += set->mid * set->value;
        denominator += set->value;
    }
    
    if (denominator < 0.001f) {
        return 0.0f;
    }
    
    return numerator / denominator;
}

/*
====================
AI_FuzzyDecision
Make tactical decision using fuzzy logic
====================
*/
float AI_FuzzyDecision(bot_state_t *bs) {
    float health = (float)bs->inventory[INVENTORY_HEALTH];
    float distance = 1000.0f; // Default
    
    if (bs->enemy >= 0) {
        gentity_t *enemy = &g_entities[bs->enemy];
        vec3_t dir;
        VectorSubtract(enemy->r.currentOrigin, bs->origin, dir);
        distance = VectorLength(dir);
    }
    
    // Fuzzify inputs
    AI_FuzzyFuzzify(0, health); // Health
    AI_FuzzyFuzzify(1, distance); // Distance
    
    // Inference
    AI_FuzzyInference();
    
    // Defuzzify output
    return AI_FuzzyDefuzzify(2); // Aggression
}
