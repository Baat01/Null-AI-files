#include "global.h"
#include "main.h"
#include "malloc.h"
#include "battle.h"
#include "battle_anim.h"
#include "battle_ai_util.h"
#include "battle_ai_main.h"
#include "battle_factory.h"
#include "battle_setup.h"
#include "battle_z_move.h"
#include "data.h"
#include "event_data.h"
#include "item.h"
#include "pokemon.h"
#include "random.h"
#include "recorded_battle.h"
#include "util.h"
#include "constants/abilities.h"
#include "constants/battle_ai.h"
#include "constants/battle_move_effects.h"
#include "constants/hold_effects.h"
#include "constants/moves.h"
#include "constants/items.h"

#define AI_ACTION_DONE          0x0001
#define AI_ACTION_FLEE          0x0002
#define AI_ACTION_WATCH         0x0004
#define AI_ACTION_DO_NOT_ATTACK 0x0008
#define AI_ACTION_UNK5          0x0010
#define AI_ACTION_UNK6          0x0020
#define AI_ACTION_UNK7          0x0040
#define AI_ACTION_UNK8          0x0080

static u32 ChooseMoveOrAction_Singles(u32 battlerAi);
static u32 ChooseMoveOrAction_Doubles(u32 battlerAi);
static inline void BattleAI_DoAIProcessing(struct AI_ThinkingStruct *aiThink, u32 battler, u32 battlerDef);
static bool32 IsPinchBerryItemEffect(u32 holdEffect);

// ewram
EWRAM_DATA const u8 *gAIScriptPtr = NULL;   // Still used in contests
EWRAM_DATA u8 sBattler_AI = 0;

// const rom data
static s16 AI_CheckBadMove(u8 battlerAtk, u8 battlerDef, u16 move, s16 score);
static s16 AI_TryToFaint(u8 battlerAtk, u8 battlerDef, u16 move, s16 score);
static s16 AI_CheckViability(u8 battlerAtk, u8 battlerDef, u16 move, s16 score);
static s16 AI_SetupFirstTurn(u8 battlerAtk, u8 battlerDef, u16 move, s16 score);
static s16 AI_Risky(u8 battlerAtk, u8 battlerDef, u16 move, s16 score);
static s16 AI_PreferStrongestMove(u8 battlerAtk, u8 battlerDef, u16 move, s16 score);
static s16 AI_PreferBatonPass(u8 battlerAtk, u8 battlerDef, u16 move, s16 score);
static s16 AI_HPAware(u8 battlerAtk, u8 battlerDef, u16 move, s16 score);
static s16 AI_Roaming(u8 battlerAtk, u8 battlerDef, u16 move, s16 score);
static s16 AI_Safari(u8 battlerAtk, u8 battlerDef, u16 move, s16 score);
static s16 AI_FirstBattle(u8 battlerAtk, u8 battlerDef, u16 move, s16 score);
static s16 AI_DoubleBattle(u8 battlerAtk, u8 battlerDef, u16 move, s16 score);

static s16 (*const sBattleAiFuncTable[])(u8, u8, u16, s16) =
{
    [0] = AI_CheckBadMove,           // AI_FLAG_CHECK_BAD_MOVE
    [1] = AI_TryToFaint,             // AI_FLAG_TRY_TO_FAINT
    [2] = AI_CheckViability,         // AI_FLAG_CHECK_VIABILITY
    [3] = AI_SetupFirstTurn,         // AI_FLAG_SETUP_FIRST_TURN
    [4] = AI_Risky,                  // AI_FLAG_RISKY
    [5] = AI_PreferStrongestMove,    // AI_FLAG_PREFER_STRONGEST_MOVE
    [6] = AI_PreferBatonPass,        // AI_FLAG_PREFER_BATON_PASS
    [7] = AI_DoubleBattle,           // AI_FLAG_DOUBLE_BATTLE
    [8] = AI_HPAware,                // AI_FLAG_HP_AWARE
    [9] = NULL,                      // AI_FLAG_NEGATE_UNAWARE
    [10] = NULL,                     // AI_FLAG_WILL_SUICIDE
    [11] = NULL,                     // AI_FLAG_HELP_PARTNER
    [12] = NULL,                     // Unused
    [13] = NULL,                     // Unused
    [14] = NULL,                     // Unused
    [15] = NULL,                     // Unused
    [16] = NULL,                     // Unused
    [17] = NULL,                     // Unused
    [18] = NULL,                     // Unused
    [19] = NULL,                     // Unused
    [20] = NULL,                     // Unused
    [21] = NULL,                     // Unused
    [22] = NULL,                     // Unused
    [23] = NULL,                     // Unused
    [24] = NULL,                     // Unused
    [25] = NULL,                     // Unused
    [26] = NULL,                     // Unused
    [27] = NULL,                     // Unused
    [28] = NULL,                     // Unused
    [29] = AI_Roaming,              // AI_FLAG_ROAMING
    [30] = AI_Safari,               // AI_FLAG_SAFARI
    [31] = AI_FirstBattle,          // AI_FLAG_FIRST_BATTLE
};

// Functions
void BattleAI_SetupItems(void)
{
    s32 i;
    u8 *data = (u8 *)BATTLE_HISTORY;

    for (i = 0; i < sizeof(struct BattleHistory); i++)
        data[i] = 0;

    // Items are allowed to use in ONLY trainer battles.
    if ((gBattleTypeFlags & BATTLE_TYPE_TRAINER)
        && !(gBattleTypeFlags & (BATTLE_TYPE_LINK | BATTLE_TYPE_SAFARI | BATTLE_TYPE_BATTLE_TOWER
                               | BATTLE_TYPE_EREADER_TRAINER | BATTLE_TYPE_SECRET_BASE | BATTLE_TYPE_FRONTIER
                               | BATTLE_TYPE_INGAME_PARTNER | BATTLE_TYPE_RECORDED_LINK)
            )
       )
    {
        for (i = 0; i < MAX_TRAINER_ITEMS; i++)
        {
            if (gTrainers[gTrainerBattleOpponent_A].items[i] != 0)
            {
                BATTLE_HISTORY->trainerItems[BATTLE_HISTORY->itemsNo] = gTrainers[gTrainerBattleOpponent_A].items[i];
                BATTLE_HISTORY->itemsNo++;
            }
        }
    }
}

static u32 GetWildAiFlags(void)
{
    u8 avgLevel = GetMonData(&gEnemyParty[0], MON_DATA_LEVEL);
    u32 flags;

    if (IsDoubleBattle())
        avgLevel = (GetMonData(&gEnemyParty[0], MON_DATA_LEVEL) + GetMonData(&gEnemyParty[1], MON_DATA_LEVEL)) / 2;

    flags |= AI_FLAG_CHECK_BAD_MOVE;
    if (avgLevel >= 20)
        flags |= AI_FLAG_CHECK_VIABILITY;
    if (avgLevel >= 60)
        flags |= AI_FLAG_PREFER_STRONGEST_MOVE;
    if (avgLevel >= 80)
        flags |= AI_FLAG_HP_AWARE;

#if B_VAR_WILD_AI_FLAGS != 0
    if (VarGet(B_VAR_WILD_AI_FLAGS) != 0)
        flags |= VarGet(B_VAR_WILD_AI_FLAGS);
#endif

    return flags;
}

void BattleAI_SetupFlags(void)
{
    if (gBattleTypeFlags & BATTLE_TYPE_RECORDED)
        AI_THINKING_STRUCT->aiFlags = GetAiScriptsInRecordedBattle();
    else if (gBattleTypeFlags & BATTLE_TYPE_SAFARI)
        AI_THINKING_STRUCT->aiFlags = AI_FLAG_SAFARI;
    else if (gBattleTypeFlags & BATTLE_TYPE_ROAMER)
        AI_THINKING_STRUCT->aiFlags = AI_FLAG_ROAMING;
    else if (gBattleTypeFlags & BATTLE_TYPE_FIRST_BATTLE)
        AI_THINKING_STRUCT->aiFlags = AI_FLAG_FIRST_BATTLE;
    else if (gBattleTypeFlags & BATTLE_TYPE_FACTORY)
        AI_THINKING_STRUCT->aiFlags = GetAiScriptsInBattleFactory();
    else if (gBattleTypeFlags & (BATTLE_TYPE_FRONTIER | BATTLE_TYPE_EREADER_TRAINER | BATTLE_TYPE_TRAINER_HILL | BATTLE_TYPE_SECRET_BASE))
        AI_THINKING_STRUCT->aiFlags = AI_FLAG_CHECK_BAD_MOVE | AI_FLAG_CHECK_VIABILITY | AI_FLAG_TRY_TO_FAINT;
    else if (gBattleTypeFlags & BATTLE_TYPE_TWO_OPPONENTS)
        AI_THINKING_STRUCT->aiFlags = gTrainers[gTrainerBattleOpponent_A].aiFlags | gTrainers[gTrainerBattleOpponent_B].aiFlags;
    else
        AI_THINKING_STRUCT->aiFlags = gTrainers[gTrainerBattleOpponent_A].aiFlags;

    // check smart wild AI
    if (!(gBattleTypeFlags & (BATTLE_TYPE_LINK | BATTLE_TYPE_TRAINER)) && IsWildMonSmart())
        AI_THINKING_STRUCT->aiFlags |= GetWildAiFlags();

    if (gBattleTypeFlags & (BATTLE_TYPE_DOUBLE | BATTLE_TYPE_TWO_OPPONENTS) || gTrainers[gTrainerBattleOpponent_A].doubleBattle)
        AI_THINKING_STRUCT->aiFlags |= AI_FLAG_DOUBLE_BATTLE; // Act smart in doubles and don't attack your partner.
}

// sBattler_AI set in ComputeBattleAiScores
void BattleAI_SetupAIData(u8 defaultScoreMoves)
{
    s32 i, move, dmg;
    u8 moveLimitations;

    // Clear AI data but preserve the flags.
    u32 flags = AI_THINKING_STRUCT->aiFlags;
    memset(AI_THINKING_STRUCT, 0, sizeof(struct AI_ThinkingStruct));
    AI_THINKING_STRUCT->aiFlags = flags;

    // Conditional score reset, unlike Ruby.
    for (i = 0; i < MAX_MON_MOVES; i++)
    {
        if (defaultScoreMoves & 1)
            AI_THINKING_STRUCT->score[i] = 100;
        else
            AI_THINKING_STRUCT->score[i] = 0;

        defaultScoreMoves >>= 1;
    }

    moveLimitations = AI_DATA->moveLimitations[gActiveBattler];

    // Ignore moves that aren't possible to use.
    for (i = 0; i < MAX_MON_MOVES; i++)
    {
        if (gBitTable[i] & moveLimitations)
            AI_THINKING_STRUCT->score[i] = 0;
    }

    //sBattler_AI = gActiveBattler;
    gBattlerTarget = SetRandomTarget(sBattler_AI);
    gBattleStruct->aiChosenTarget[sBattler_AI] = gBattlerTarget;
}

u32 BattleAI_ChooseMoveOrAction(void)
{
    u32 savedCurrentMove = gCurrentMove;
    u8 ret;

    if (!(gBattleTypeFlags & BATTLE_TYPE_DOUBLE))
        ret = ChooseMoveOrAction_Singles(sBattler_AI);
    else
        ret = ChooseMoveOrAction_Doubles(sBattler_AI);

    // Clear protect structures, some flags may be set during AI calcs
    // e.g. pranksterElevated from GetMovePriority
    memset(&gProtectStructs, 0, MAX_BATTLERS_COUNT * sizeof(struct ProtectStruct));

    gCurrentMove = savedCurrentMove;
    return ret;
}

// damages/other info computed in GetAIDataAndCalcDmg
u32 ComputeBattleAiScores(u32 battler)
{
    sBattler_AI = battler;
    BattleAI_SetupAIData(0xF);
    return BattleAI_ChooseMoveOrAction();
}

static void CopyBattlerDataToAIParty(u32 bPosition, u32 side)
{
    u32 battler = GetBattlerAtPosition(bPosition);
    struct AiPartyMon *aiMon = &AI_PARTY->mons[side][gBattlerPartyIndexes[battler]];
    struct BattlePokemon *bMon = &gBattleMons[battler];

    aiMon->species = bMon->species;
    aiMon->level = bMon->level;
    aiMon->status = bMon->status1;
    aiMon->gender = GetBattlerGender(battler);
    aiMon->isFainted = FALSE;
    aiMon->wasSentInBattle = TRUE;
    aiMon->switchInCount++;
}

void Ai_InitPartyStruct(void)
{
    u32 i;
    bool32 isOmniscient = (AI_THINKING_STRUCT->aiFlags & AI_FLAG_OMNISCIENT);
    struct Pokemon *mon;

    AI_PARTY->count[B_SIDE_PLAYER] = gPlayerPartyCount;
    AI_PARTY->count[B_SIDE_OPPONENT] = gEnemyPartyCount;

    // Save first 2 or 4(in doubles) mons
    CopyBattlerDataToAIParty(B_POSITION_PLAYER_LEFT, B_SIDE_PLAYER);
    if (gBattleTypeFlags & BATTLE_TYPE_DOUBLE)
        CopyBattlerDataToAIParty(B_POSITION_PLAYER_RIGHT, B_SIDE_PLAYER);

    // If player's partner is AI, save opponent mons
    if (gBattleTypeFlags & BATTLE_TYPE_INGAME_PARTNER)
    {
        CopyBattlerDataToAIParty(B_POSITION_OPPONENT_LEFT, B_SIDE_OPPONENT);
        CopyBattlerDataToAIParty(B_POSITION_OPPONENT_RIGHT, B_SIDE_OPPONENT);
    }

    // Find fainted mons
    for (i = 0; i < AI_PARTY->count[B_SIDE_PLAYER]; i++)
    {
        if (GetMonData(&gPlayerParty[i], MON_DATA_HP) == 0)
            AI_PARTY->mons[B_SIDE_PLAYER][i].isFainted = TRUE;

        if (isOmniscient)
        {
            u32 j;
            mon = &gPlayerParty[i];
            AI_PARTY->mons[B_SIDE_PLAYER][i].item = GetMonData(mon, MON_DATA_HELD_ITEM);
            AI_PARTY->mons[B_SIDE_PLAYER][i].heldEffect = ItemId_GetHoldEffect(AI_PARTY->mons[B_SIDE_PLAYER][i].item);
            AI_PARTY->mons[B_SIDE_PLAYER][i].ability = GetMonAbility(mon);
            for (j = 0; j < MAX_MON_MOVES; j++)
                AI_PARTY->mons[B_SIDE_PLAYER][i].moves[j] = GetMonData(mon, MON_DATA_MOVE1 + j);
        }
    }
}

void Ai_UpdateSwitchInData(u32 battler)
{
    SetBattlerAiData(battler);
}

void Ai_UpdateFaintData(u32 battler)
{
    struct AiPartyMon *aiMon = &AI_PARTY->mons[GET_BATTLER_SIDE(battler)][gBattlerPartyIndexes[battler]];
    ClearBattlerMoveHistory(battler);
    ClearBattlerAbilityHistory(battler);
    ClearBattlerItemEffectHistory(battler);
    aiMon->isFainted = TRUE;
}

void SetBattlerAiData(u32 battlerId)
{
    AI_DATA->abilities[battlerId] = AI_GetAbility(battlerId);
    AI_DATA->items[battlerId] = gBattleMons[battlerId].item;
    AI_DATA->holdEffects[battlerId] = AI_GetHoldEffect(battlerId);
    AI_DATA->holdEffectParams[battlerId] = GetBattlerHoldEffectParam(battlerId);
    AI_DATA->predictedMoves[battlerId] = gLastMoves[battlerId];
    AI_DATA->hpPercents[battlerId] = GetHealthPercentage(battlerId);
    AI_DATA->moveLimitations[battlerId] = CheckMoveLimitations(battlerId, 0, MOVE_LIMITATIONS_ALL);
}

void SetAiLogicDataForTurn(void)
{
    u32 battlerAtk, battlerDef, i, move;
    u8 effectiveness;
    s32 dmg, maxrollDmg;

    memset(AI_DATA, 0, sizeof(struct AiLogicData));

    if (!(gBattleTypeFlags & BATTLE_TYPE_HAS_AI) && !IsWildMonSmart())
        return;

    // Set delay timer to count how long it takes for AI to choose action/move
    gBattleStruct->aiDelayTimer = gMain.vblankCounter1;

    // get/assume all battler data
    for (i = 0; i < gBattlersCount; i++)
    {
        if (!IsBattlerAlive(i)) {
            continue;
        }
        SetBattlerAiData(i);
    }

    // simulate AI damage
    for (battlerAtk = 0; battlerAtk < gBattlersCount; battlerAtk++)
    {
        if (!IsBattlerAlive(battlerAtk)
          || !IsAiBattlerAware(battlerAtk)) {
            continue;
        }

        for (battlerDef = 0; battlerDef < gBattlersCount; battlerDef++)
        {
            if (battlerAtk == battlerDef)
                continue;

            //Intentional AI mechanic where it gets fooled by Illusion on first turn only
            SaveIllusionData(battlerAtk);
            SaveIllusionData(battlerDef);
            SetIllusionData(battlerAtk);
            SetIllusionData(battlerDef);

            RecordKnownMove(battlerDef, gLastMoves[battlerDef]);
            for (i = 0; i < MAX_MON_MOVES; i++)
            {
                dmg = 0;
                effectiveness = AI_EFFECTIVENESS_x0;
                move = gBattleMons[battlerAtk].moves[i];

                if (move != 0
                 && move != 0xFFFF
                 //&& gBattleMoves[move].power != 0  /* we want to get effectiveness of status moves */
                 && !(AI_DATA->moveLimitations[battlerAtk] & gBitTable[i])) {
                    dmg = AI_CalcDamage(move, battlerAtk, battlerDef, &effectiveness, TRUE, TRUE);
                    maxrollDmg = AI_CalcDamage(move, battlerAtk, battlerDef, &effectiveness, TRUE, FALSE);
                }

                AI_DATA->simulatedDmg[battlerAtk][battlerDef][i] = dmg;
                AI_DATA->maxrollDmg[battlerAtk][battlerDef][i] = maxrollDmg;
                AI_DATA->effectiveness[battlerAtk][battlerDef][i] = effectiveness;
            }
            RestoreIllusionData(battlerAtk);
            RestoreIllusionData(battlerDef);
        }
    }
}

static u32 ChooseMoveOrAction_Singles(u32 battlerAi)
{
    u8 currentMoveArray[MAX_MON_MOVES];
    u8 consideredMoveArray[MAX_MON_MOVES];
    u32 numOfBestMoves;
    s32 i, id;
    u32 flags = AI_THINKING_STRUCT->aiFlags;

    AI_DATA->partnerMove = 0;   // no ally
    while (flags != 0)
    {
        if (flags & 1)
        {
            BattleAI_DoAIProcessing(AI_THINKING_STRUCT, battlerAi, gBattlerTarget);
        }
        flags >>= 1;
        AI_THINKING_STRUCT->aiLogicId++;
    }

    for (i = 0; i < MAX_MON_MOVES; i++) {
        gBattleStruct->aiFinalScore[battlerAi][gBattlerTarget][i] = AI_THINKING_STRUCT->score[i];
    }

    // Check special AI actions.
    if (AI_THINKING_STRUCT->aiAction & AI_ACTION_FLEE)
        return AI_CHOICE_FLEE;
    if (AI_THINKING_STRUCT->aiAction & AI_ACTION_WATCH)
        return AI_CHOICE_WATCH;

    gActiveBattler = battlerAi;

    /*
    // If can switch.
    if (CountUsablePartyMons(battlerAi) > 0
        && !IsAbilityPreventingEscape(battlerAi)
        && !(gBattleMons[gActiveBattler].status2 & (STATUS2_WRAPPED | STATUS2_ESCAPE_PREVENTION))
        && !(gStatuses3[gActiveBattler] & STATUS3_ROOTED)
        && !(gBattleTypeFlags & (BATTLE_TYPE_ARENA | BATTLE_TYPE_PALACE))
        && AI_THINKING_STRUCT->aiFlags & (AI_FLAG_CHECK_VIABILITY | AI_FLAG_CHECK_BAD_MOVE | AI_FLAG_TRY_TO_FAINT | AI_FLAG_PREFER_BATON_PASS))
    {
        // Consider switching if all moves are worthless to use.
        if (gBattleMons[battlerAi].hp >= gBattleMons[battlerAi].maxHP / 2
            && !(gStatuses3[BATTLE_OPPOSITE(gActiveBattler)] & STATUS3_SEMI_INVULNERABLE))
        {
            s32 cap = AI_THINKING_STRUCT->aiFlags & (AI_FLAG_CHECK_VIABILITY) ? 95 : 93;
            for (i = 0; i < MAX_MON_MOVES; i++)
            {
                if (AI_THINKING_STRUCT->score[i] > cap)
                    break;
            }
        }
    }*/

    numOfBestMoves = 1;
    currentMoveArray[0] = AI_THINKING_STRUCT->score[0];
    consideredMoveArray[0] = 0;

    for (i = 1; i < MAX_MON_MOVES; i++)
    {
        if (gBattleMons[battlerAi].moves[i] != MOVE_NONE)
        {
            // In ruby, the order of these if statements is reversed.
            if (currentMoveArray[0] == AI_THINKING_STRUCT->score[i])
            {
                currentMoveArray[numOfBestMoves] = AI_THINKING_STRUCT->score[i];
                consideredMoveArray[numOfBestMoves++] = i;
            }
            if (currentMoveArray[0] < AI_THINKING_STRUCT->score[i])
            {
                numOfBestMoves = 1;
                currentMoveArray[0] = AI_THINKING_STRUCT->score[i];
                consideredMoveArray[0] = i;
            }
        }
    }
    return consideredMoveArray[Random() % numOfBestMoves];
}

static u32 ChooseMoveOrAction_Doubles(u32 battlerAi)
{
    s32 i, j;
    u32 flags;
    s16 bestMovePointsForTarget[MAX_BATTLERS_COUNT];
    s8 mostViableTargetsArray[MAX_BATTLERS_COUNT];
    u8 actionOrMoveIndex[MAX_BATTLERS_COUNT];
    u8 mostViableMovesScores[MAX_MON_MOVES];
    u8 mostViableMovesIndices[MAX_MON_MOVES];
    s32 mostViableTargetsNo;
    s32 mostViableMovesNo;
    s16 mostMovePoints;

    for (i = 0; i < MAX_BATTLERS_COUNT; i++)
    {
        if (i == battlerAi || gBattleMons[i].hp == 0)
        {
            actionOrMoveIndex[i] = 0xFF;
            bestMovePointsForTarget[i] = -1;
        }
        else
        {
            if (gBattleTypeFlags & BATTLE_TYPE_PALACE)
                BattleAI_SetupAIData(gBattleStruct->palaceFlags >> 4);
            else
                BattleAI_SetupAIData(0xF);

            gBattlerTarget = i;
            if ((i & BIT_SIDE) != (battlerAi & BIT_SIDE))
                RecordLastUsedMoveByTarget();

            AI_DATA->partnerMove = GetAllyChosenMove(battlerAi);
            AI_THINKING_STRUCT->aiLogicId = 0;
            AI_THINKING_STRUCT->movesetIndex = 0;
            flags = AI_THINKING_STRUCT->aiFlags;
            while (flags != 0)
            {
                if (flags & 1)
                {
                    BattleAI_DoAIProcessing(AI_THINKING_STRUCT, battlerAi, gBattlerTarget);
                }
                flags >>= 1;
                AI_THINKING_STRUCT->aiLogicId++;
            }

            if (AI_THINKING_STRUCT->aiAction & AI_ACTION_FLEE)
            {
                actionOrMoveIndex[i] = AI_CHOICE_FLEE;
            }
            else if (AI_THINKING_STRUCT->aiAction & AI_ACTION_WATCH)
            {
                actionOrMoveIndex[i] = AI_CHOICE_WATCH;
            }
            else
            {
                mostViableMovesScores[0] = AI_THINKING_STRUCT->score[0];
                mostViableMovesIndices[0] = 0;
                mostViableMovesNo = 1;
                for (j = 1; j < MAX_MON_MOVES; j++)
                {
                    if (gBattleMons[battlerAi].moves[j] != 0)
                    {
                        if (!CanTargetBattler(battlerAi, i, gBattleMons[battlerAi].moves[j]))
                            continue;

                        if (mostViableMovesScores[0] == AI_THINKING_STRUCT->score[j])
                        {
                            mostViableMovesScores[mostViableMovesNo] = AI_THINKING_STRUCT->score[j];
                            mostViableMovesIndices[mostViableMovesNo] = j;
                            mostViableMovesNo++;
                        }
                        if (mostViableMovesScores[0] < AI_THINKING_STRUCT->score[j])
                        {
                            mostViableMovesScores[0] = AI_THINKING_STRUCT->score[j];
                            mostViableMovesIndices[0] = j;
                            mostViableMovesNo = 1;
                        }
                    }
                }
                actionOrMoveIndex[i] = mostViableMovesIndices[Random() % mostViableMovesNo];
                bestMovePointsForTarget[i] = mostViableMovesScores[0];

                // Don't use a move against ally if it has less than 100 points.
                if (i == BATTLE_PARTNER(battlerAi) && bestMovePointsForTarget[i] < 100)
                {
                    bestMovePointsForTarget[i] = -1;
                    mostViableMovesScores[0] = mostViableMovesScores[0]; // Needed to match.
                }
            }

            for (j = 0; j < MAX_MON_MOVES; j++) {
                gBattleStruct->aiFinalScore[battlerAi][gBattlerTarget][j] = AI_THINKING_STRUCT->score[j];
            }
        }
    }

    mostMovePoints = bestMovePointsForTarget[0];
    mostViableTargetsArray[0] = 0;
    mostViableTargetsNo = 1;

    for (i = 1; i < MAX_BATTLERS_COUNT; i++)
    {
        if (mostMovePoints == bestMovePointsForTarget[i])
        {
            mostViableTargetsArray[mostViableTargetsNo] = i;
            mostViableTargetsNo++;
        }
        if (mostMovePoints < bestMovePointsForTarget[i])
        {
            mostMovePoints = bestMovePointsForTarget[i];
            mostViableTargetsArray[0] = i;
            mostViableTargetsNo = 1;
        }
    }

    gBattlerTarget = mostViableTargetsArray[Random() % mostViableTargetsNo];
    gBattleStruct->aiChosenTarget[battlerAi] = gBattlerTarget;
    return actionOrMoveIndex[gBattlerTarget];
}

static inline void BattleAI_DoAIProcessing(struct AI_ThinkingStruct *aiThink, u32 battler, u32 battlerDef)
{
    do
    {
        if (gBattleMons[battler].pp[aiThink->movesetIndex] == 0)
            aiThink->moveConsidered = MOVE_NONE;
        else
            aiThink->moveConsidered = gBattleMons[battler].moves[aiThink->movesetIndex];

        if (aiThink->moveConsidered != MOVE_NONE
          && aiThink->score[aiThink->movesetIndex] > 0)
        {
            if (aiThink->aiLogicId < ARRAY_COUNT(sBattleAiFuncTable)
              && sBattleAiFuncTable[aiThink->aiLogicId] != NULL)
            {
                // Call AI function
                aiThink->score[aiThink->movesetIndex] =
                    sBattleAiFuncTable[aiThink->aiLogicId](battler,
                      battlerDef,
                      aiThink->moveConsidered,
                      aiThink->score[aiThink->movesetIndex]);
            }
        }
        else
        {
            aiThink->score[aiThink->movesetIndex] = 0;
        }
        aiThink->movesetIndex++;
    } while (aiThink->movesetIndex < MAX_MON_MOVES && !(aiThink->aiAction & AI_ACTION_DO_NOT_ATTACK));

    aiThink->movesetIndex = 0;
}

// AI Score Functions
// AI_FLAG_CHECK_BAD_MOVE - decreases move scores
static s16 AI_CheckBadMove(u8 battlerAtk, u8 battlerDef, u16 move, s16 score)
{
    // move data
    s8 atkPriority = GetMovePriority(battlerAtk, move);
    u16 moveEffect = gBattleMoves[move].effect;
    s32 moveType;
    u16 moveTarget = AI_GetBattlerMoveTargetType(battlerAtk, move);
    u16 accuracy = AI_GetMoveAccuracy(battlerAtk, battlerDef, move);
    u32 effectiveness = AI_DATA->effectiveness[battlerAtk][battlerDef][AI_THINKING_STRUCT->movesetIndex];
    bool32 isDoubleBattle = IsValidDoubleBattle(battlerAtk);
    u32 i;
    u16 predictedMove = AI_DATA->predictedMoves[battlerDef];
    u16 moveEffectChance = gBattleMoves[move].secondaryEffectChance;

    SetTypeBeforeUsingMove(move, battlerAtk);
    GET_MOVE_TYPE(move, moveType);

    if (IsTargetingPartner(battlerAtk, battlerDef))
        return score;

    GET_MOVE_TYPE(move, moveType);

    if (GetBattleMoveSplit(move) == SPLIT_STATUS && AI_DATA->abilities[battlerDef] == ABILITY_GOOD_AS_GOLD
        && ((moveTarget == MOVE_TARGET_SELECTED) || (moveTarget & MOVE_TARGET_BOTH) || (moveTarget & MOVE_TARGET_FOES_AND_ALLY) || (moveTarget & MOVE_TARGET_FOES_AND_ALLY)))
        RETURN_SCORE_MINUS(20);

    if (gBattleMoves[move].powderMove && !IsAffectedByPowder(battlerDef, AI_DATA->abilities[battlerDef], AI_DATA->holdEffects[battlerDef]))
        RETURN_SCORE_MINUS(10);

    if (IsSemiInvulnerable(battlerDef, move) && moveEffect != EFFECT_SEMI_INVULNERABLE && AI_WhosFaster(battlerAtk, battlerDef) == AI_IS_FASTER)
        RETURN_SCORE_MINUS(20);

    if (gBattleStruct->battlerState[battlerDef].commandingDondozo)
        RETURN_SCORE_MINUS(20);
          
    // check if negates type
    switch (effectiveness)
    {
    case AI_EFFECTIVENESS_x0:
        RETURN_SCORE_MINUS(20);
        break;
    case AI_EFFECTIVENESS_x0_125:
    case AI_EFFECTIVENESS_x0_25:
        break;
    }

    // check non-user target
    if (!(moveTarget & MOVE_TARGET_USER))
    {
        // target ability checks
        if (!DoesBattlerIgnoreAbilityChecks(AI_DATA->abilities[battlerAtk], move))
        {
            switch (AI_DATA->abilities[battlerDef])
            {
            case ABILITY_MAGIC_GUARD:
                switch (moveEffect)
                {
                case EFFECT_POISON:
                case EFFECT_WILL_O_WISP:
                case EFFECT_TOXIC:
                case EFFECT_LEECH_SEED:
                    score -= 5;
                    break;
                case EFFECT_CURSE:
                    if (IS_BATTLER_OF_TYPE(battlerAtk, TYPE_GHOST)) // Don't use Curse if you're a ghost type vs a Magic Guard user, they'll take no damage.
                        score -= 5;
                    break;
                }
                break;
            case ABILITY_WONDER_GUARD:
                if (effectiveness < AI_EFFECTIVENESS_x2)
                    return 0;
                break;
            case ABILITY_DAZZLING:
            case ABILITY_QUEENLY_MAJESTY:
                if ((atkPriority > 0) && !(moveTarget & (MOVE_TARGET_USER | MOVE_TARGET_OPPONENTS_FIELD)))
                    RETURN_SCORE_MINUS(20);
                break;
            case ABILITY_AROMA_VEIL:
                if (IsAromaVeilProtectedMove(move))
                    RETURN_SCORE_MINUS(20);
                break;
            case ABILITY_SWEET_VEIL:
                if (moveEffect == EFFECT_SLEEP || moveEffect == EFFECT_YAWN)
                    RETURN_SCORE_MINUS(20);
                break;
            case ABILITY_FLOWER_VEIL:
                if (IS_BATTLER_OF_TYPE(battlerDef, TYPE_GRASS) && (IsNonVolatileStatusMoveEffect(moveEffect) || IsStatLoweringMoveEffect(moveEffect)))
                    RETURN_SCORE_MINUS(20);
                break;
            case ABILITY_MAGIC_BOUNCE:
                if (gBattleMoves[move].magicCoatAffected)
                    RETURN_SCORE_MINUS(20);
                break;
            case ABILITY_CONTRARY:
                if (IsStatLoweringMoveEffect(moveEffect))
                    RETURN_SCORE_MINUS(20);
                break;
            case ABILITY_CLEAR_BODY:
            case ABILITY_FULL_METAL_BODY:
            case ABILITY_WHITE_SMOKE:
                if (IsStatLoweringMoveEffect(moveEffect) || (moveEffect == EFFECT_PARTING_SHOT))
                    RETURN_SCORE_MINUS(20);
                break;
            case ABILITY_HYPER_CUTTER:
                if ((moveEffect == EFFECT_ATTACK_DOWN ||  moveEffect == EFFECT_ATTACK_DOWN_2)
                  && move != MOVE_PLAY_NICE && move != MOVE_NOBLE_ROAR && move != MOVE_TEARFUL_LOOK && move != MOVE_VENOM_DRENCH)
                    RETURN_SCORE_MINUS(20);
                break;
            case ABILITY_KEEN_EYE:
            case ABILITY_MINDS_EYE:
                if (moveEffect == EFFECT_ACCURACY_DOWN || moveEffect == EFFECT_ACCURACY_DOWN_2)
                    RETURN_SCORE_MINUS(20);
                break;
            case ABILITY_BIG_PECKS:
                if (moveEffect == EFFECT_DEFENSE_DOWN || moveEffect == EFFECT_DEFENSE_DOWN_2)
                    RETURN_SCORE_MINUS(20);
                break;
            case ABILITY_DEFIANT:
            case ABILITY_COMPETITIVE:
                if (IsStatLoweringMoveEffect(moveEffect) && !IsTargetingPartner(battlerAtk, battlerDef))
                    RETURN_SCORE_MINUS(20);
                break;
            case ABILITY_COMATOSE:
                if (IsNonVolatileStatusMoveEffect(moveEffect))
                    RETURN_SCORE_MINUS(20);
                break;
            case ABILITY_SHIELDS_DOWN:
                if (IsShieldsDownProtected(battlerAtk) && IsNonVolatileStatusMoveEffect(moveEffect))
                    RETURN_SCORE_MINUS(20);
                break;
            case ABILITY_LEAF_GUARD:
                if (AI_WeatherHasEffect() && (gBattleWeather & B_WEATHER_SUN)
                  && AI_DATA->holdEffects[battlerDef] != HOLD_EFFECT_UTILITY_UMBRELLA
                  && IsNonVolatileStatusMoveEffect(moveEffect))
                    RETURN_SCORE_MINUS(20);
                break;
            case ABILITY_SOUNDPROOF:
                if (gBattleMoves[move].soundMove)
                    RETURN_SCORE_MINUS(20);
                break;
            case ABILITY_VOLT_ABSORB:
            case ABILITY_MOTOR_DRIVE:
            case ABILITY_LIGHTNING_ROD:
                if (moveType == TYPE_ELECTRIC)
                    RETURN_SCORE_MINUS(20);
                break;
            case ABILITY_WATER_ABSORB:
            case ABILITY_DRY_SKIN:
            case ABILITY_STORM_DRAIN:
                if (moveType == TYPE_WATER)
                    RETURN_SCORE_MINUS(20);
                break;
            case ABILITY_FLASH_FIRE:
                if (moveType == TYPE_FIRE)
                    RETURN_SCORE_MINUS(20);
                break;
            case ABILITY_SAP_SIPPER:
                if (moveType == TYPE_GRASS)
                    RETURN_SCORE_MINUS(20);
                break;
            } // def ability checks

            // target partner ability checks & not attacking partner
            if (isDoubleBattle)
            {
                switch (AI_DATA->abilities[BATTLE_PARTNER(battlerDef)])
                {
                case ABILITY_LIGHTNING_ROD:
                    if (moveType == TYPE_ELECTRIC && !IsMoveRedirectionPrevented(move, AI_DATA->abilities[battlerAtk]) && !(moveTarget & (MOVE_TARGET_BOTH | MOVE_TARGET_FOES_AND_ALLY | MOVE_TARGET_ALL_BATTLERS)))
                        RETURN_SCORE_MINUS(20);
                    break;
                case ABILITY_STORM_DRAIN:
                    if (moveType == TYPE_WATER && !IsMoveRedirectionPrevented(move, AI_DATA->abilities[battlerAtk]) && !(moveTarget & (MOVE_TARGET_BOTH | MOVE_TARGET_FOES_AND_ALLY | MOVE_TARGET_ALL_BATTLERS)))
                        RETURN_SCORE_MINUS(20);
                    break;
                case ABILITY_MAGIC_BOUNCE:
                    if (gBattleMoves[move].magicCoatAffected && moveTarget & (MOVE_TARGET_BOTH | MOVE_TARGET_FOES_AND_ALLY | MOVE_TARGET_OPPONENTS_FIELD))
                        RETURN_SCORE_MINUS(20);
                    break;
                case ABILITY_SWEET_VEIL:
                    if (moveEffect == EFFECT_SLEEP || moveEffect == EFFECT_YAWN || moveEffect == EFFECT_DARK_VOID)
                        RETURN_SCORE_MINUS(20);
                    break;
                case ABILITY_FLOWER_VEIL:
                    if ((IS_BATTLER_OF_TYPE(battlerDef, TYPE_GRASS)) && (IsNonVolatileStatusMoveEffect(moveEffect) || IsStatLoweringMoveEffect(moveEffect)))
                        RETURN_SCORE_MINUS(10);
                    break;
                case ABILITY_AROMA_VEIL:
                    if (IsAromaVeilProtectedMove(move))
                        RETURN_SCORE_MINUS(10);
                    break;
                case ABILITY_DAZZLING:
                case ABILITY_QUEENLY_MAJESTY:
                    if ((atkPriority > 0) && !(moveTarget & (MOVE_TARGET_USER | MOVE_TARGET_OPPONENTS_FIELD)))
                        RETURN_SCORE_MINUS(10);
                    break;
                }
            } // def partner ability checks
        } // ignore def ability check

        // gen7+ dark type mons immune to priority->elevated moves from prankster
        #if B_PRANKSTER_DARK_TYPES >= GEN_7
        if (AI_DATA->abilities[battlerAtk] == ABILITY_PRANKSTER && IS_BATTLER_OF_TYPE(battlerDef, TYPE_DARK) && IS_MOVE_STATUS(move)
          && !(moveTarget & (MOVE_TARGET_OPPONENTS_FIELD | MOVE_TARGET_USER)))
            RETURN_SCORE_MINUS(10);
        #endif

        // terrain & effect checks
        if (AI_IsTerrainAffected(battlerDef, STATUS_FIELD_ELECTRIC_TERRAIN))
        {
            if (moveEffect == EFFECT_SLEEP || moveEffect == EFFECT_YAWN)
                RETURN_SCORE_MINUS(20);
        }

        if (AI_IsTerrainAffected(battlerDef, STATUS_FIELD_MISTY_TERRAIN))
        {
            if (IsNonVolatileStatusMoveEffect(moveEffect) || IsConfusionMoveEffect(moveEffect))
                RETURN_SCORE_MINUS(20);
        }

        if (AI_IsTerrainAffected(battlerAtk, STATUS_FIELD_PSYCHIC_TERRAIN) && atkPriority > 0)
        {
            RETURN_SCORE_MINUS(20);
        }
    } // end check MOVE_TARGET_USER

// the following checks apply to any target (including user)

    // throat chop check
    if (gDisableStructs[battlerAtk].throatChopTimer > gBattleTurnCounter && gBattleMoves[move].soundMove)
        return 0; // Can't even select move at all
    // heal block check
    if (gStatuses3[battlerAtk] & STATUS3_HEAL_BLOCK && IsHealBlockPreventingMove(battlerAtk, move))
        return 0; // Can't even select heal blocked move
    // primal weather check
    if (WEATHER_HAS_EFFECT)
    {
        if (gBattleWeather & B_WEATHER_PRIMAL_ANY)
        {
            switch (move)
            {
                case MOVE_SUNNY_DAY:
                case MOVE_RAIN_DANCE:
                case MOVE_HAIL:
                case MOVE_SANDSTORM:
                    RETURN_SCORE_MINUS(20);
            }
        }

        if (!IS_MOVE_STATUS(move))
        {
            if (gBattleWeather & B_WEATHER_SUN_PRIMAL)
            {
                if (moveType == TYPE_WATER && move != MOVE_HYDRO_STEAM)
                    RETURN_SCORE_MINUS(20);
            }
            else if (gBattleWeather & B_WEATHER_RAIN_PRIMAL)
            {
                if (moveType == TYPE_FIRE)
                    RETURN_SCORE_MINUS(20);
            }
        }
    }

    // check move effects
    switch (moveEffect)
    {
        case EFFECT_HIT:
        default:
            break;  // check move damage
        case EFFECT_DARK_VOID:
        case EFFECT_SLEEP:
            if (!AI_CanPutToSleep(battlerAtk, battlerDef, AI_DATA->abilities[battlerDef], move, AI_DATA->partnerMove))
                score -= 20;
            break;
    // stat raising effects
        case EFFECT_ATTACK_UP_HIT:
            if (GetMoveDamageResult(move) == MOVE_POWER_BEST || moveEffectChance != 100)
                break;
            //fallthrough
        case EFFECT_ATTACK_UP:
        case EFFECT_ATTACK_UP_2:
        case EFFECT_ATTACK_UP_USER_ALLY:
            if (!BattlerStatCanRise(battlerAtk, AI_DATA->abilities[battlerAtk], STAT_ATK) || !HasMoveWithSplit(battlerAtk, SPLIT_PHYSICAL))
                score -= 20;
            if (AI_DATA->abilities[battlerDef] == ABILITY_UNAWARE
                || HasMoveEffect(battlerDef, EFFECT_HAZE) 
                || HasMoveEffect(battlerDef, EFFECT_FREEZY_FROST)
                || HasMoveEffect(battlerDef, EFFECT_TOPSY_TURVY)
                || HasMoveEffect(battlerDef, EFFECT_CLEAR_SMOG))
                score -= 20;
            if (CanTargetFaintAi(battlerDef, battlerAtk))
            {
                if ((IsBattlerSashSturdyProtected(battlerAtk))
                || IsMimikyuDisguiseProtected(battlerAtk, battlerDef)
                || IsBattlerSubstituteProtected(battlerAtk, battlerDef))
                    break;
                score -= 20;
            }
            break;
        case EFFECT_SPEED_UP_HIT:
            if (GetMoveDamageResult(move) == MOVE_POWER_BEST || moveEffectChance != 100)
                break;
            //fallthrough
        case EFFECT_AUTOTOMIZE:
        case EFFECT_SPEED_UP:
        case EFFECT_SPEED_UP_2:
            if (AI_WhosFaster(battlerAtk, battlerDef) == AI_IS_FASTER)
                score -= 20;
            else if (!BattlerStatCanRise(battlerAtk, AI_DATA->abilities[battlerAtk], STAT_SPEED))
                score -= 20;
            else if (HasMoveEffect(battlerDef, EFFECT_HAZE) 
                || HasMoveEffect(battlerDef, EFFECT_FREEZY_FROST)
                || HasMoveEffect(battlerDef, EFFECT_TOPSY_TURVY)
                || HasMoveEffect(battlerDef, EFFECT_CLEAR_SMOG)
                || (HasMoveEffect(battlerDef, EFFECT_ROAR) && CountUsablePartyMons(battlerAtk) != 0)
                || (HasMoveEffect(battlerDef, EFFECT_HIT_SWITCH_TARGET) && CountUsablePartyMons(battlerAtk) != 0))
                score -= 20;
            break;
        case EFFECT_STUFF_CHEEKS:
            if (ItemId_GetPocket(AI_DATA->items[battlerAtk]) != POCKET_BERRIES)
                return 0;   // cannot even select
            //fallthrough
        case EFFECT_DEFENSE_UP_HIT:
            if (GetMoveDamageResult(move) == MOVE_POWER_BEST || moveEffectChance != 100)
                break;
            //fallthrough
        case EFFECT_DEFENSE_UP:
        case EFFECT_DEFENSE_UP_2:
        case EFFECT_DEFENSE_UP_3:
        case EFFECT_DEFENSE_CURL:
            if (!BattlerStatCanRise(battlerAtk, AI_DATA->abilities[battlerAtk], STAT_DEF))
                score -= 20;
            if (CanTargetFaintAi(battlerDef, battlerAtk))
            {
                if ((IsBattlerSashSturdyProtected(battlerAtk))
                || IsMimikyuDisguiseProtected(battlerAtk, battlerDef)
                || IsBattlerSubstituteProtected(battlerAtk, battlerDef))
                    break;
                score -= 20;
            }
            if (AI_DATA->abilities[battlerDef] == ABILITY_UNAWARE
                || HasMoveEffect(battlerDef, EFFECT_HAZE) 
                || HasMoveEffect(battlerDef, EFFECT_FREEZY_FROST)
                || HasMoveEffect(battlerDef, EFFECT_TOPSY_TURVY)
                || HasMoveEffect(battlerDef, EFFECT_CLEAR_SMOG))
                score -= 20;
            break;
        case EFFECT_SP_ATTACK_UP_HIT:
        case EFFECT_SPECIAL_ATTACK_UP_HIT:
            if (GetMoveDamageResult(move) == MOVE_POWER_BEST || moveEffectChance != 100)
                break;
            //fallthrough
        case EFFECT_SPECIAL_ATTACK_UP:
        case EFFECT_SPECIAL_ATTACK_UP_2:
        case EFFECT_SPECIAL_ATTACK_UP_3:
            if (!BattlerStatCanRise(battlerAtk, AI_DATA->abilities[battlerAtk], STAT_SPATK))
                score -= 20;
            if (CanTargetFaintAi(battlerDef, battlerAtk))
            {
                if (IsBattlerSashSturdyProtected(battlerAtk)
                    || IsMimikyuDisguiseProtected(battlerAtk, battlerDef)
                    || IsBattlerSubstituteProtected(battlerAtk, battlerDef))
                    break;
                score -= 20;
            }
            if (AI_DATA->abilities[battlerDef] == ABILITY_UNAWARE
                || HasMoveEffect(battlerDef, EFFECT_HAZE) 
                || HasMoveEffect(battlerDef, EFFECT_FREEZY_FROST)
                || HasMoveEffect(battlerDef, EFFECT_TOPSY_TURVY)
                || HasMoveEffect(battlerDef, EFFECT_CLEAR_SMOG))
                score -= 20;
            break;
        case EFFECT_SPECIAL_DEFENSE_UP:
        case EFFECT_SPECIAL_DEFENSE_UP_2:
            if (!BattlerStatCanRise(battlerAtk, AI_DATA->abilities[battlerAtk], STAT_SPDEF))
                score -= 20;
            if (CanTargetFaintAi(battlerDef, battlerAtk))
            {
                if ((IsBattlerSashSturdyProtected(battlerAtk))
                || IsMimikyuDisguiseProtected(battlerAtk, battlerDef)
                || IsBattlerSubstituteProtected(battlerAtk, battlerDef))
                    break;
                score -= 20;
            }
            if (AI_DATA->abilities[battlerDef] == ABILITY_UNAWARE
                || HasMoveEffect(battlerDef, EFFECT_HAZE) 
                || HasMoveEffect(battlerDef, EFFECT_FREEZY_FROST)
                || HasMoveEffect(battlerDef, EFFECT_TOPSY_TURVY)
                || HasMoveEffect(battlerDef, EFFECT_CLEAR_SMOG))
                score -= 20;
            break;
        case EFFECT_ACCURACY_UP:
        case EFFECT_ACCURACY_UP_2:
            if (!BattlerStatCanRise(battlerAtk, AI_DATA->abilities[battlerAtk], STAT_ACC))
                score -= 20;
            if (CanTargetFaintAi(battlerDef, battlerAtk))
            {
                if ((IsBattlerSashSturdyProtected(battlerAtk))
                || IsMimikyuDisguiseProtected(battlerAtk, battlerDef)
                || IsBattlerSubstituteProtected(battlerAtk, battlerDef))
                    break;
                score -= 20;
            }
            break;
        case EFFECT_EVASION_UP:
        case EFFECT_EVASION_UP_2:
        case EFFECT_MINIMIZE:
            if (AI_DATA->abilities[battlerDef] == ABILITY_UNAWARE
                || HasMoveEffect(battlerDef, EFFECT_HAZE) 
                || HasMoveEffect(battlerDef, EFFECT_FREEZY_FROST)
                || HasMoveEffect(battlerDef, EFFECT_TOPSY_TURVY)
                || HasMoveEffect(battlerDef, EFFECT_CLEAR_SMOG)
                || (HasMoveEffect(battlerDef, EFFECT_ROAR) && CountUsablePartyMons(battlerAtk) != 0)
                || (HasMoveEffect(battlerDef, EFFECT_HIT_SWITCH_TARGET) && CountUsablePartyMons(battlerAtk) != 0))
            {
                score -= 20;
                break;
            }
            if (!BattlerStatCanRise(battlerAtk, AI_DATA->abilities[battlerAtk], STAT_EVASION))
                score -= 20;
            break;
        case EFFECT_COSMIC_POWER:
            if (!BattlerStatCanRise(battlerAtk, AI_DATA->abilities[battlerAtk], STAT_DEF))
                score -= 20;
            if (!BattlerStatCanRise(battlerAtk, AI_DATA->abilities[battlerAtk], STAT_SPDEF))
                score -= 20;
            if (CanTargetFaintAi(battlerDef, battlerAtk))
            {
                if ((IsBattlerSashSturdyProtected(battlerAtk))
                || IsMimikyuDisguiseProtected(battlerAtk, battlerDef)
                || IsBattlerSubstituteProtected(battlerAtk, battlerDef))
                    break;
                score -= 20;
            }
            if (AI_DATA->abilities[battlerDef] == ABILITY_UNAWARE
                || HasMoveEffect(battlerDef, EFFECT_HAZE) 
                || HasMoveEffect(battlerDef, EFFECT_FREEZY_FROST)
                || HasMoveEffect(battlerDef, EFFECT_TOPSY_TURVY)
                || HasMoveEffect(battlerDef, EFFECT_CLEAR_SMOG))
                score -= 20;
            break;
        case EFFECT_BULK_UP:
            if (!BattlerStatCanRise(battlerAtk, AI_DATA->abilities[battlerAtk], STAT_ATK))
                score -= 20;
            if (CanTargetFaintAi(battlerDef, battlerAtk))
            {
                if ((IsBattlerSashSturdyProtected(battlerAtk))
                || IsMimikyuDisguiseProtected(battlerAtk, battlerDef)
                || IsBattlerSubstituteProtected(battlerAtk, battlerDef))
                    break;
                score -= 20;
            }
            if (AI_DATA->abilities[battlerDef] == ABILITY_UNAWARE
                || HasMoveEffect(battlerDef, EFFECT_HAZE) 
                || HasMoveEffect(battlerDef, EFFECT_FREEZY_FROST)
                || HasMoveEffect(battlerDef, EFFECT_TOPSY_TURVY)
                || HasMoveEffect(battlerDef, EFFECT_CLEAR_SMOG))
                score -= 20;
            break;
        case EFFECT_CALM_MIND:
            if (!BattlerStatCanRise(battlerAtk, AI_DATA->abilities[battlerAtk], STAT_SPATK))
                score -= 20;
            if (CanTargetFaintAi(battlerDef, battlerAtk))
            {
                if ((IsBattlerSashSturdyProtected(battlerAtk))
                || IsMimikyuDisguiseProtected(battlerAtk, battlerDef)
                || IsBattlerSubstituteProtected(battlerAtk, battlerDef))
                    break;
                score -= 20;
            }
            if (AI_DATA->abilities[battlerDef] == ABILITY_UNAWARE
                || HasMoveEffect(battlerDef, EFFECT_HAZE) 
                || HasMoveEffect(battlerDef, EFFECT_FREEZY_FROST)
                || HasMoveEffect(battlerDef, EFFECT_TOPSY_TURVY)
                || HasMoveEffect(battlerDef, EFFECT_CLEAR_SMOG))
                score -= 20;
            break;
        case EFFECT_TIDY_UP:          
        case EFFECT_DRAGON_DANCE:
        case EFFECT_COIL:
        case EFFECT_ATTACK_ACCURACY_UP: //hone claws
            if (!BattlerStatCanRise(battlerAtk, AI_DATA->abilities[battlerAtk], STAT_ATK))
                score -= 20;
            if (CanTargetFaintAi(battlerDef, battlerAtk))
            {
                if ((IsBattlerSashSturdyProtected(battlerAtk))
                || IsMimikyuDisguiseProtected(battlerAtk, battlerDef)
                || IsBattlerSubstituteProtected(battlerAtk, battlerDef))
                    break;
                score -= 20;
            }
            if (AI_DATA->abilities[battlerDef] == ABILITY_UNAWARE
                || HasMoveEffect(battlerDef, EFFECT_HAZE) 
                || HasMoveEffect(battlerDef, EFFECT_FREEZY_FROST)
                || HasMoveEffect(battlerDef, EFFECT_TOPSY_TURVY)
                || HasMoveEffect(battlerDef, EFFECT_CLEAR_SMOG))
                score -= 20;
            break;
        case EFFECT_CHARGE:
            if (gStatuses3[battlerAtk] & STATUS3_CHARGED_UP)
                score -= 20;
            if ((HasMoveEffect(battlerDef, EFFECT_ROAR) && CountUsablePartyMons(battlerAtk) != 0)
                || (HasMoveEffect(battlerDef, EFFECT_HIT_SWITCH_TARGET) && CountUsablePartyMons(battlerAtk) != 0))
            {
                score -= 20;
                break;
            }
            else if (!BattlerStatCanRise(battlerAtk, AI_DATA->abilities[battlerAtk], STAT_SPDEF))
                score -= 20;
            if (CanTargetFaintAi(battlerDef, battlerAtk))
            {
                if ((IsBattlerSashSturdyProtected(battlerAtk))
                || IsMimikyuDisguiseProtected(battlerAtk, battlerDef)
                || IsBattlerSubstituteProtected(battlerAtk, battlerDef))
                    break;
                score -= 20;
            }
            break;
        case EFFECT_QUIVER_DANCE:
            if (!BattlerStatCanRise(battlerAtk, AI_DATA->abilities[battlerAtk], STAT_SPATK))
                score -= 20;
            if (CanTargetFaintAi(battlerDef, battlerAtk))
            {
                if ((IsBattlerSashSturdyProtected(battlerAtk))
                || IsMimikyuDisguiseProtected(battlerAtk, battlerDef)
                || IsBattlerSubstituteProtected(battlerAtk, battlerDef))
                    break;
                score -= 20;
            }
            if (AI_DATA->abilities[battlerDef] == ABILITY_UNAWARE
                || HasMoveEffect(battlerDef, EFFECT_HAZE) 
                || HasMoveEffect(battlerDef, EFFECT_FREEZY_FROST)
                || HasMoveEffect(battlerDef, EFFECT_TOPSY_TURVY)
                || HasMoveEffect(battlerDef, EFFECT_CLEAR_SMOG))
                score -= 20;
            break;
        case EFFECT_GEOMANCY:
            if (AI_DATA->holdEffects[battlerAtk] != HOLD_EFFECT_POWER_HERB)
                score -= 20;
            else if (!BattlerStatCanRise(battlerAtk, AI_DATA->abilities[battlerAtk], STAT_SPATK))
                score -= 20;
            if (CanTargetFaintAi(battlerDef, battlerAtk))
            {
                if ((IsBattlerSashSturdyProtected(battlerAtk))
                || IsMimikyuDisguiseProtected(battlerAtk, battlerDef)
                || IsBattlerSubstituteProtected(battlerAtk, battlerDef))
                    break;
                score -= 20;
            }
            if (AI_DATA->abilities[battlerDef] == ABILITY_UNAWARE
                || HasMoveEffect(battlerDef, EFFECT_HAZE) 
                || HasMoveEffect(battlerDef, EFFECT_FREEZY_FROST)
                || HasMoveEffect(battlerDef, EFFECT_TOPSY_TURVY)
                || HasMoveEffect(battlerDef, EFFECT_CLEAR_SMOG)
                || (CountUsablePartyMons(battlerAtk) != 0 && (HasMoveEffect(battlerDef, EFFECT_ROAR) || HasMoveEffect(battlerDef, EFFECT_HIT_SWITCH_TARGET))))
                score -= 20;
            break;
        case EFFECT_VICTORY_DANCE:
            if (gBattleMons[battlerAtk].statStages[STAT_ATK] >= MAX_STAT_STAGE || !HasMoveWithSplit(battlerAtk, SPLIT_PHYSICAL))
                score -= 20;
            else if (!BattlerStatCanRise(battlerAtk, AI_DATA->abilities[battlerAtk], STAT_SPEED))
                score -= 20;
            else if (!BattlerStatCanRise(battlerAtk, AI_DATA->abilities[battlerAtk], STAT_DEF))
                score -= 20;
            if (CanTargetFaintAi(battlerDef, battlerAtk))
            {
                if ((IsBattlerSashSturdyProtected(battlerAtk))
                || IsMimikyuDisguiseProtected(battlerAtk, battlerDef)
                || IsBattlerSubstituteProtected(battlerAtk, battlerDef))
                    break;
                score -= 20;
            }
            if (AI_DATA->abilities[battlerDef] == ABILITY_UNAWARE
                || HasMoveEffect(battlerDef, EFFECT_HAZE) 
                || HasMoveEffect(battlerDef, EFFECT_FREEZY_FROST)
                || HasMoveEffect(battlerDef, EFFECT_TOPSY_TURVY)
                || HasMoveEffect(battlerDef, EFFECT_CLEAR_SMOG))
                score -= 20;
            break;
        case EFFECT_SHIFT_GEAR:
            if (!BattlerStatCanRise(battlerAtk, AI_DATA->abilities[battlerAtk], STAT_ATK))
                score -= 20;
            if (CanTargetFaintAi(battlerDef, battlerAtk))
            {
                if ((IsBattlerSashSturdyProtected(battlerAtk))
                || IsMimikyuDisguiseProtected(battlerAtk, battlerDef)
                || IsBattlerSubstituteProtected(battlerAtk, battlerDef))
                    break;
                score -= 20;
            }
            if (AI_DATA->abilities[battlerDef] == ABILITY_UNAWARE
                || HasMoveEffect(battlerDef, EFFECT_HAZE) 
                || HasMoveEffect(battlerDef, EFFECT_FREEZY_FROST)
                || HasMoveEffect(battlerDef, EFFECT_TOPSY_TURVY)
                || HasMoveEffect(battlerDef, EFFECT_CLEAR_SMOG))
                score -= 20;
            break;
        case EFFECT_SHELL_SMASH:
            if (AI_DATA->abilities[battlerDef] == ABILITY_UNAWARE
                || HasMoveEffect(battlerDef, EFFECT_HAZE) 
                || HasMoveEffect(battlerDef, EFFECT_FREEZY_FROST)
                || HasMoveEffect(battlerDef, EFFECT_TOPSY_TURVY)
                || HasMoveEffect(battlerDef, EFFECT_CLEAR_SMOG)
                || (HasMoveEffect(battlerDef, EFFECT_ROAR) && CountUsablePartyMons(battlerAtk) != 0)
                || (HasMoveEffect(battlerDef, EFFECT_HIT_SWITCH_TARGET) && CountUsablePartyMons(battlerAtk) != 0))
            {
                score -= 20;
                break;
            }
            if (AI_DATA->abilities[battlerAtk] == ABILITY_CONTRARY)
            {
                if (!BattlerStatCanRise(battlerAtk, AI_DATA->abilities[battlerAtk], STAT_DEF))
                    score -= 20;
                if (!BattlerStatCanRise(battlerAtk, AI_DATA->abilities[battlerAtk], STAT_SPDEF))
                    score -= 20;
            }
            else
            {
                if (!BattlerStatCanRise(battlerAtk, AI_DATA->abilities[battlerAtk], STAT_ATK))
                    score -= 20;
                else if (!BattlerStatCanRise(battlerAtk, AI_DATA->abilities[battlerAtk], STAT_SPATK))
                    score -= 20;
                break;
            }
            if (CanTargetFaintAi(battlerDef, battlerAtk))
            {
                if ((IsBattlerSashSturdyProtected(battlerAtk))
                || IsMimikyuDisguiseProtected(battlerAtk, battlerDef)
                || IsBattlerSubstituteProtected(battlerAtk, battlerDef))
                    break;
                score -= 20;
                break;
            }
            if (gBattleMons[battlerAtk].statStages[STAT_DEF] <= 5 || gBattleMons[battlerAtk].statStages[STAT_SPDEF] <= 5)
                score -= 20;
            break;
        case EFFECT_GROWTH:
        case EFFECT_ATTACK_SPATK_UP:    // work up
            if (!BattlerStatCanRise(battlerAtk, AI_DATA->abilities[battlerAtk], STAT_ATK))
                score -= 20;
            if (!BattlerStatCanRise(battlerAtk, AI_DATA->abilities[battlerAtk], STAT_SPATK))
                score -= 20;
            if (CanTargetFaintAi(battlerDef, battlerAtk))
            {
                if ((IsBattlerSashSturdyProtected(battlerAtk))
                || IsMimikyuDisguiseProtected(battlerAtk, battlerDef)
                || IsBattlerSubstituteProtected(battlerAtk, battlerDef))
                    break;
                score -= 20;
            }
            if (AI_DATA->abilities[battlerDef] == ABILITY_UNAWARE
                || HasMoveEffect(battlerDef, EFFECT_HAZE) 
                || HasMoveEffect(battlerDef, EFFECT_FREEZY_FROST)
                || HasMoveEffect(battlerDef, EFFECT_TOPSY_TURVY)
                || HasMoveEffect(battlerDef, EFFECT_CLEAR_SMOG))
                score -= 20;
            break;
        case EFFECT_ROTOTILLER:
            if (isDoubleBattle)
            {
                if (!(IS_BATTLER_OF_TYPE(battlerAtk, TYPE_GRASS)
                  && AI_IsBattlerGrounded(battlerAtk)
                  && (BattlerStatCanRise(battlerAtk, AI_DATA->abilities[battlerAtk], STAT_ATK) || BattlerStatCanRise(battlerAtk, AI_DATA->abilities[battlerAtk], STAT_SPATK)))
                  && !(IS_BATTLER_OF_TYPE(BATTLE_PARTNER(battlerAtk), TYPE_GRASS)
                  && AI_IsBattlerGrounded(BATTLE_PARTNER(battlerAtk))
                  && AI_DATA->abilities[BATTLE_PARTNER(battlerAtk)] != ABILITY_CONTRARY
                  && (BattlerStatCanRise(BATTLE_PARTNER(battlerAtk), AI_DATA->abilities[BATTLE_PARTNER(battlerAtk)], STAT_ATK)
                   || BattlerStatCanRise(BATTLE_PARTNER(battlerAtk), AI_DATA->abilities[BATTLE_PARTNER(battlerAtk)], STAT_SPATK))))
                {
                    score -= 20;
                }
            }
            else if (!(IS_BATTLER_OF_TYPE(battlerAtk, TYPE_GRASS)
              && AI_IsBattlerGrounded(battlerAtk)
              && (BattlerStatCanRise(battlerAtk, AI_DATA->abilities[battlerAtk], STAT_ATK) || BattlerStatCanRise(battlerAtk, AI_DATA->abilities[battlerAtk], STAT_SPATK))))
            {
                score -= 20;
            }
            break;
        case EFFECT_GEAR_UP:
            if (AI_DATA->abilities[battlerAtk] == ABILITY_PLUS || AI_DATA->abilities[battlerAtk] == ABILITY_MINUS)
            {
                // same as growth, work up
                if (!BattlerStatCanRise(battlerAtk, AI_DATA->abilities[battlerAtk], STAT_ATK) || !HasMoveWithSplit(battlerAtk, SPLIT_PHYSICAL))
                    score -= 10;
                else if (!BattlerStatCanRise(battlerAtk, AI_DATA->abilities[battlerAtk], STAT_SPATK) || !HasMoveWithSplit(battlerAtk, SPLIT_SPECIAL))
                    score -= 8;
                break;
            }
            else if (!isDoubleBattle)
            {
                score -= 10;    // no partner and our stats wont rise, so don't use
            }

            if (isDoubleBattle)
            {
                if (AI_DATA->abilities[BATTLE_PARTNER(battlerAtk)] == ABILITY_PLUS || AI_DATA->abilities[BATTLE_PARTNER(battlerAtk)] == ABILITY_MINUS)
                {
                    if ((!BattlerStatCanRise(BATTLE_PARTNER(battlerAtk), AI_DATA->abilities[BATTLE_PARTNER(battlerAtk)], STAT_ATK) || !HasMoveWithSplit(battlerAtk, SPLIT_PHYSICAL))
                      && (!BattlerStatCanRise(BATTLE_PARTNER(battlerAtk), AI_DATA->abilities[BATTLE_PARTNER(battlerAtk)], STAT_SPATK) || !HasMoveWithSplit(battlerAtk, SPLIT_SPECIAL)))
                        score -= 20;
                }
                else if (AI_DATA->abilities[battlerAtk] != ABILITY_PLUS && AI_DATA->abilities[battlerAtk] != ABILITY_MINUS)
                {
                    score -= 20;    // nor our or our partner's ability is plus/minus
                }
            }
            break;
        case EFFECT_ACUPRESSURE:
            if (DoesSubstituteBlockMove(battlerAtk, battlerDef, move) || AreBattlersStatsMaxed(battlerDef))
                score -= 20;
            else if (CanTargetFaintAi(battlerDef, battlerAtk))
            {
                if ((IsBattlerSashSturdyProtected(battlerAtk))
                || IsMimikyuDisguiseProtected(battlerAtk, battlerDef)
                || IsBattlerSubstituteProtected(battlerAtk, battlerDef))
                    break;
                score -= 20;
            }
            else if (AI_DATA->abilities[battlerDef] == ABILITY_UNAWARE
                || HasMoveEffect(battlerDef, EFFECT_HAZE) 
                || HasMoveEffect(battlerDef, EFFECT_FREEZY_FROST)
                || HasMoveEffect(battlerDef, EFFECT_TOPSY_TURVY)
                || HasMoveEffect(battlerDef, EFFECT_CLEAR_SMOG)
                || HasMoveEffect(battlerDef, EFFECT_ROAR) 
                || HasMoveEffect(battlerDef, EFFECT_HIT_SWITCH_TARGET))
                score -= 20;
            break;
        case EFFECT_MAGNETIC_FLUX:
            if (AI_DATA->abilities[battlerAtk] == ABILITY_PLUS || AI_DATA->abilities[battlerAtk] == ABILITY_MINUS)
            {
                if (!BattlerStatCanRise(battlerAtk, AI_DATA->abilities[battlerAtk], STAT_DEF))
                    score -= 20;
                else if (!BattlerStatCanRise(battlerAtk, AI_DATA->abilities[battlerAtk], STAT_SPDEF))
                    score -= 20;
            }
            else if (!isDoubleBattle)
            {
                score -= 20;    // our stats wont rise from this move
            }

            if (isDoubleBattle)
            {
                if (AI_DATA->abilities[BATTLE_PARTNER(battlerAtk)] == ABILITY_PLUS || AI_DATA->abilities[BATTLE_PARTNER(battlerAtk)] == ABILITY_MINUS)
                {
                    if (!BattlerStatCanRise(BATTLE_PARTNER(battlerAtk), AI_DATA->abilities[BATTLE_PARTNER(battlerAtk)], STAT_DEF))
                        score -= 10;
                    else if (!BattlerStatCanRise(BATTLE_PARTNER(battlerAtk), AI_DATA->abilities[BATTLE_PARTNER(battlerAtk)], STAT_SPDEF))
                        score -= 8;
                }
                else if (AI_DATA->abilities[battlerAtk] != ABILITY_PLUS && AI_DATA->abilities[battlerAtk] != ABILITY_MINUS)
                {
                    score -= 10;    // nor our or our partner's ability is plus/minus
                }
            }
            break;
    // stat lowering effects
        case EFFECT_ATTACK_DOWN:
        case EFFECT_ATTACK_DOWN_2:
            if (gBattleMons[battlerDef].statStages[STAT_ATK] == MIN_STAT_STAGE)
                score -= 20;
            else if (AI_DATA->abilities[battlerDef] == ABILITY_HYPER_CUTTER
            || AI_DATA->abilities[battlerDef] == ABILITY_CLEAR_BODY
            || AI_DATA->abilities[battlerDef] == ABILITY_WHITE_SMOKE
            || AI_DATA->abilities[battlerDef] == ABILITY_FULL_METAL_BODY
            || AI_DATA->abilities[battlerDef] == ABILITY_DEFIANT
            || AI_DATA->abilities[battlerDef] == ABILITY_COMPETITIVE
            || AI_DATA->abilities[battlerDef] == ABILITY_CONTRARY
            || AI_DATA->abilities[battlerDef] == ABILITY_MIRROR_ARMOR)
                score -= 20;
            break;
        case EFFECT_DEFENSE_DOWN:
        case EFFECT_DEFENSE_DOWN_2:
            if (gBattleMons[battlerDef].statStages[STAT_DEF] == MIN_STAT_STAGE)
                score -= 20;
            else if (AI_DATA->abilities[battlerDef] == ABILITY_CLEAR_BODY
            || AI_DATA->abilities[battlerDef] == ABILITY_WHITE_SMOKE
            || AI_DATA->abilities[battlerDef] == ABILITY_FULL_METAL_BODY
            || AI_DATA->abilities[battlerDef] == ABILITY_DEFIANT
            || AI_DATA->abilities[battlerDef] == ABILITY_COMPETITIVE
            || AI_DATA->abilities[battlerDef] == ABILITY_CONTRARY
            || AI_DATA->abilities[battlerDef] == ABILITY_MIRROR_ARMOR)
                score -= 20;
            break;
        case EFFECT_SPEED_DOWN:
        case EFFECT_SPEED_DOWN_2:
            if (gBattleMons[battlerDef].statStages[STAT_SPEED] == MIN_STAT_STAGE)
                score -= 20;
            else if (AI_DATA->abilities[battlerDef] == ABILITY_CLEAR_BODY
            || AI_DATA->abilities[battlerDef] == ABILITY_WHITE_SMOKE
            || AI_DATA->abilities[battlerDef] == ABILITY_FULL_METAL_BODY
            || AI_DATA->abilities[battlerDef] == ABILITY_DEFIANT
            || AI_DATA->abilities[battlerDef] == ABILITY_COMPETITIVE
            || AI_DATA->abilities[battlerDef] == ABILITY_CONTRARY
            || AI_DATA->abilities[battlerDef] == ABILITY_MIRROR_ARMOR)
                score -= 20;
            break;
        case EFFECT_SPECIAL_ATTACK_DOWN:
        case EFFECT_SPECIAL_ATTACK_DOWN_2:
            if (gBattleMons[battlerDef].statStages[STAT_SPATK] == MIN_STAT_STAGE)
                score -= 20;
            else if (AI_DATA->abilities[battlerDef] == ABILITY_CLEAR_BODY
            || AI_DATA->abilities[battlerDef] == ABILITY_WHITE_SMOKE
            || AI_DATA->abilities[battlerDef] == ABILITY_FULL_METAL_BODY
            || AI_DATA->abilities[battlerDef] == ABILITY_DEFIANT
            || AI_DATA->abilities[battlerDef] == ABILITY_COMPETITIVE
            || AI_DATA->abilities[battlerDef] == ABILITY_CONTRARY
            || AI_DATA->abilities[battlerDef] == ABILITY_MIRROR_ARMOR)
                score -= 20;
            break;
        case EFFECT_SPECIAL_DEFENSE_DOWN:
        case EFFECT_SPECIAL_DEFENSE_DOWN_2:
            if (gBattleMons[battlerDef].statStages[STAT_SPDEF] == MIN_STAT_STAGE)
                score -= 20;
            else if (AI_DATA->abilities[battlerDef] == ABILITY_CLEAR_BODY
            || AI_DATA->abilities[battlerDef] == ABILITY_WHITE_SMOKE
            || AI_DATA->abilities[battlerDef] == ABILITY_FULL_METAL_BODY
            || AI_DATA->abilities[battlerDef] == ABILITY_DEFIANT
            || AI_DATA->abilities[battlerDef] == ABILITY_COMPETITIVE
            || AI_DATA->abilities[battlerDef] == ABILITY_CONTRARY
            || AI_DATA->abilities[battlerDef] == ABILITY_MIRROR_ARMOR)
                score -= 20;
            break;
        case EFFECT_ACCURACY_DOWN:
        case EFFECT_ACCURACY_DOWN_2:
            if (gBattleMons[battlerDef].statStages[STAT_ACC] == MIN_STAT_STAGE)
                score -= 20;
            else if (AI_DATA->abilities[battlerDef] == ABILITY_KEEN_EYE
            || AI_DATA->abilities[battlerDef] == ABILITY_MINDS_EYE
            || AI_DATA->abilities[battlerDef] == ABILITY_CLEAR_BODY
            || AI_DATA->abilities[battlerDef] == ABILITY_WHITE_SMOKE
            || AI_DATA->abilities[battlerDef] == ABILITY_FULL_METAL_BODY
            || AI_DATA->abilities[battlerDef] == ABILITY_DEFIANT
            || AI_DATA->abilities[battlerDef] == ABILITY_COMPETITIVE
            || AI_DATA->abilities[battlerDef] == ABILITY_CONTRARY
            || AI_DATA->abilities[battlerDef] == ABILITY_MIRROR_ARMOR)
                score -= 20;
            break;
        case EFFECT_EVASION_DOWN:
        case EFFECT_EVASION_DOWN_2:
            if (gBattleMons[battlerDef].statStages[STAT_EVASION] == MIN_STAT_STAGE)
                score -= 20;
            else if (AI_DATA->abilities[battlerDef] == ABILITY_CLEAR_BODY
            || AI_DATA->abilities[battlerDef] == ABILITY_WHITE_SMOKE
            || AI_DATA->abilities[battlerDef] == ABILITY_FULL_METAL_BODY
            || AI_DATA->abilities[battlerDef] == ABILITY_DEFIANT
            || AI_DATA->abilities[battlerDef] == ABILITY_COMPETITIVE
            || AI_DATA->abilities[battlerDef] == ABILITY_CONTRARY
            || AI_DATA->abilities[battlerDef] == ABILITY_MIRROR_ARMOR)
                score -= 20;
            break;
        case EFFECT_TICKLE:
            if (gBattleMons[battlerDef].statStages[STAT_ATK] == MIN_STAT_STAGE)
                score -= 20;
            else if (AI_DATA->abilities[battlerDef] == ABILITY_HYPER_CUTTER
            || AI_DATA->abilities[battlerDef] == ABILITY_CLEAR_BODY
            || AI_DATA->abilities[battlerDef] == ABILITY_WHITE_SMOKE
            || AI_DATA->abilities[battlerDef] == ABILITY_FULL_METAL_BODY
            || AI_DATA->abilities[battlerDef] == ABILITY_DEFIANT
            || AI_DATA->abilities[battlerDef] == ABILITY_COMPETITIVE
            || AI_DATA->abilities[battlerDef] == ABILITY_CONTRARY
            || AI_DATA->abilities[battlerDef] == ABILITY_MIRROR_ARMOR)
                score -= 20;
            break;
        case EFFECT_VENOM_DRENCH:
            if (gBattleMons[battlerDef].statStages[STAT_ATK] == MIN_STAT_STAGE)
                score -= 20;
            if (!(gBattleMons[battlerDef].status1 & STATUS1_PSN_ANY))
            {
                score -= 20;
            }
            else
            {
                if (AI_DATA->abilities[battlerDef] == ABILITY_HYPER_CUTTER
                || AI_DATA->abilities[battlerDef] == ABILITY_CLEAR_BODY
                || AI_DATA->abilities[battlerDef] == ABILITY_WHITE_SMOKE
                || AI_DATA->abilities[battlerDef] == ABILITY_FULL_METAL_BODY
                || AI_DATA->abilities[battlerDef] == ABILITY_DEFIANT
                || AI_DATA->abilities[battlerDef] == ABILITY_COMPETITIVE
                || AI_DATA->abilities[battlerDef] == ABILITY_CONTRARY
                || AI_DATA->abilities[battlerDef] == ABILITY_MIRROR_ARMOR)
                    score -= 20;
            }
            break;
        case EFFECT_NOBLE_ROAR:
            if (gBattleMons[battlerDef].statStages[STAT_ATK] == MIN_STAT_STAGE)
                score -= 20;
            else if (AI_DATA->abilities[battlerDef] == ABILITY_HYPER_CUTTER
            || AI_DATA->abilities[battlerDef] == ABILITY_CLEAR_BODY
            || AI_DATA->abilities[battlerDef] == ABILITY_WHITE_SMOKE
            || AI_DATA->abilities[battlerDef] == ABILITY_FULL_METAL_BODY
            || AI_DATA->abilities[battlerDef] == ABILITY_DEFIANT
            || AI_DATA->abilities[battlerDef] == ABILITY_COMPETITIVE
            || AI_DATA->abilities[battlerDef] == ABILITY_CONTRARY
            || AI_DATA->abilities[battlerDef] == ABILITY_MIRROR_ARMOR)
                score -= 20;
            break;
        case EFFECT_CAPTIVATE:
            {
                u8 atkGender = GetGenderFromSpeciesAndPersonality(gBattleMons[battlerAtk].species, gBattleMons[battlerAtk].personality);
                u8 defGender = GetGenderFromSpeciesAndPersonality(gBattleMons[battlerDef].species, gBattleMons[battlerDef].personality);
                if (atkGender == MON_GENDERLESS || defGender == MON_GENDERLESS || atkGender == defGender)
                    score -= 10;
            }
            break;
    // other
        case EFFECT_HAZE:
            if (PartnerHasSameMoveEffectWithoutTarget(BATTLE_PARTNER(battlerAtk), move, AI_DATA->partnerMove))
            {
                score -= 20;    // partner already using haze
            }
            else
            {
                for (i = STAT_ATK; i < NUM_BATTLE_STATS; i++)
                {
                    if (gBattleMons[battlerAtk].statStages[i] > DEFAULT_STAT_STAGE || gBattleMons[BATTLE_PARTNER(battlerAtk)].statStages[i] > DEFAULT_STAT_STAGE)
                        score -= 10;  // Don't want to reset our boosted stats
                }
                for (i = STAT_ATK; i < NUM_BATTLE_STATS; i++)
                {
                    if (gBattleMons[battlerDef].statStages[i] < DEFAULT_STAT_STAGE || gBattleMons[BATTLE_PARTNER(battlerDef)].statStages[i] < DEFAULT_STAT_STAGE)
                        score -= 10; //Don't want to reset enemy lowered stats
                }
            }
            break;
        //case EFFECT_BIDE:
        //case EFFECT_SUPER_FANG:
        //case EFFECT_RECHARGE:
        //case EFFECT_COUNTER:
        //case EFFECT_FLAIL:
        case EFFECT_RETURN:
        case EFFECT_PRESENT:
        case EFFECT_FRUSTRATION:
        case EFFECT_ARG_FIXED_DAMAGE:
        //case EFFECT_MIRROR_COAT:
        case EFFECT_SKULL_BASH:
        case EFFECT_FOCUS_PUNCH:
        case EFFECT_SUPERPOWER:
        //case EFFECT_ENDEAVOR:
        case EFFECT_LOW_KICK:
            // AI_CBM_HighRiskForDamage
            if (AI_DATA->abilities[battlerDef] == ABILITY_WONDER_GUARD && effectiveness < AI_EFFECTIVENESS_x2)
                score -= 10;
            break;
        case EFFECT_COUNTER:
            if (IsBattlerIncapacitated(battlerDef, AI_DATA->abilities[battlerDef]) || gBattleMons[battlerDef].status2 & (STATUS2_INFATUATION | STATUS2_CONFUSION))
                score -= 20;
            if (HasOnlyMovesWithSplit(battlerDef, SPLIT_SPECIAL, TRUE))
                score -= 20;
            if (CanTargetFaintAi(battlerDef, battlerAtk))
            {
                if ((IsBattlerSashSturdyProtected(battlerAtk))
                || IsMimikyuDisguiseProtected(battlerAtk, battlerDef)
                || IsBattlerSubstituteProtected(battlerAtk, battlerDef))
                    break;
                score -= 20;
            }
            break;
        case EFFECT_MIRROR_COAT:
            if (IsBattlerIncapacitated(battlerDef, AI_DATA->abilities[battlerDef]) || gBattleMons[battlerDef].status2 & (STATUS2_INFATUATION | STATUS2_CONFUSION))
                score -= 20;
            if (HasOnlyMovesWithSplit(battlerDef, SPLIT_PHYSICAL, TRUE))
                score -= 20;
            if (CanTargetFaintAi(battlerDef, battlerAtk))
            {
                if ((IsBattlerSashSturdyProtected(battlerAtk))
                || IsMimikyuDisguiseProtected(battlerAtk, battlerDef)
                || IsBattlerSubstituteProtected(battlerAtk, battlerDef))
                    break;
                score -= 20;
            }
            break;
        case EFFECT_METAL_BURST:
            if (IsBattlerIncapacitated(battlerDef, AI_DATA->abilities[battlerDef]) || gBattleMons[battlerDef].status2 & (STATUS2_INFATUATION | STATUS2_CONFUSION))
                score -= 20;
            if (CanTargetFaintAi(battlerDef, battlerAtk))
            {
                if ((IsBattlerSashSturdyProtected(battlerAtk))
                || IsMimikyuDisguiseProtected(battlerAtk, battlerDef)
                || IsBattlerSubstituteProtected(battlerAtk, battlerDef))
                    break;
                score -= 20;
            }
            if (AI_WhosFaster(battlerAtk, battlerDef) == AI_IS_FASTER)
                score -= 20;
            break;
        case EFFECT_ROAR:
            if (CountUsablePartyMons(battlerDef) == 0)
                score -= 20;
            else if (AI_DATA->abilities[battlerDef] == ABILITY_SUCTION_CUPS)
                score -= 20;
            break;
        case EFFECT_TOXIC_THREAD:
            if (!ShouldLowerStat(battlerDef, AI_DATA->abilities[battlerDef], STAT_SPEED))
                score--;    // may still want to just poison
            //fallthrough
        case EFFECT_POISON:
        case EFFECT_TOXIC:
            if (!AI_CanPoison(battlerAtk, battlerDef, AI_DATA->abilities[battlerDef], move, AI_DATA->partnerMove))
                score -= 20;
            break;
        case EFFECT_LIGHT_SCREEN:
            if (gSideStatuses[GetBattlerSide(battlerAtk)] & SIDE_STATUS_LIGHTSCREEN
              || PartnerHasSameMoveEffectWithoutTarget(BATTLE_PARTNER(battlerAtk), move, AI_DATA->partnerMove))
                score -= 20;
            break;
        case EFFECT_REFLECT:
            if (gSideStatuses[GetBattlerSide(battlerAtk)] & SIDE_STATUS_REFLECT
              || PartnerHasSameMoveEffectWithoutTarget(BATTLE_PARTNER(battlerAtk), move, AI_DATA->partnerMove))
                score -= 20;
            break;
        case EFFECT_AURORA_VEIL:
            if (gSideStatuses[GetBattlerSide(battlerAtk)] & SIDE_STATUS_AURORA_VEIL
              || PartnerHasSameMoveEffectWithoutTarget(BATTLE_PARTNER(battlerAtk), move, AI_DATA->partnerMove)
              || !(gBattleWeather & (B_WEATHER_HAIL | B_WEATHER_SNOW)))
                score -= 20;
            break;
        case EFFECT_OHKO:
        #if B_SHEER_COLD_IMMUNITY >= GEN_7
            if (move == MOVE_SHEER_COLD && IS_BATTLER_OF_TYPE(battlerDef, TYPE_ICE))
                return 0;
        #endif
            if (!ShouldTryOHKO(battlerAtk, battlerDef, AI_DATA->abilities[battlerAtk], AI_DATA->abilities[battlerDef], move))
                score -= 20;
            if (GetNoOfHitsToKOBattlerDmg(GetHighestDmg(battlerAtk, battlerDef), battlerDef) < 3)
                score -= 1;
            break;
        case EFFECT_MIST:
            if (gSideStatuses[GetBattlerSide(battlerAtk)] & SIDE_STATUS_MIST
              || PartnerHasSameMoveEffectWithoutTarget(BATTLE_PARTNER(battlerAtk), move, AI_DATA->partnerMove))
                score -= 20;
            break;
        case EFFECT_LASER_FOCUS:
            if (gStatuses3[battlerAtk] & STATUS3_LASER_FOCUS)
                score -= 20;
            else if (AI_DATA->abilities[battlerDef] == ABILITY_SHELL_ARMOR 
                    || AI_DATA->abilities[battlerDef] == ABILITY_BATTLE_ARMOR 
                    || AI_DATA->abilities[battlerDef] == ABILITY_MAGMA_ARMOR
                    || AI_DATA->abilities[battlerDef] == ABILITY_LEAF_GUARD
                    || gSideStatuses[GetBattlerSide(battlerDef)] & SIDE_STATUS_LUCKY_CHANT)
                score -= 20;
            break;
        case EFFECT_FOCUS_ENERGY:
            if (gBattleMons[battlerAtk].status2 & STATUS2_FOCUS_ENERGY_ANY)
                score -= 20;
            else if (AI_DATA->abilities[battlerDef] == ABILITY_SHELL_ARMOR 
                    || AI_DATA->abilities[battlerDef] == ABILITY_BATTLE_ARMOR 
                    || AI_DATA->abilities[battlerDef] == ABILITY_MAGMA_ARMOR
                    || AI_DATA->abilities[battlerDef] == ABILITY_LEAF_GUARD
                    || gSideStatuses[GetBattlerSide(battlerDef)] & SIDE_STATUS_LUCKY_CHANT)
                score -= 20;
            break;
        case EFFECT_CONFUSE:
        case EFFECT_SWAGGER:
        case EFFECT_FLATTER:
            if (!AI_CanConfuse(battlerAtk, battlerDef, AI_DATA->abilities[battlerDef], BATTLE_PARTNER(battlerAtk), move, AI_DATA->partnerMove))
                score -= 20;
            break;
        case EFFECT_PARALYZE:
            if (!AI_CanParalyze(battlerAtk, battlerDef, AI_DATA->abilities[battlerDef], move, AI_DATA->partnerMove))
                score -= 20;
            break;
        case EFFECT_SHED_TAIL:
            if (CountUsablePartyMons(battlerAtk) == 0)
                score -= 20;
            else if (gBattleMons[battlerAtk].status2 & STATUS2_SUBSTITUTE || AI_DATA->abilities[battlerDef] == ABILITY_INFILTRATOR)
                score -= 20;
            else if (AI_DATA->hpPercents[battlerAtk] <= 50)
                score -= 20;
            else if (HasSoundMove(battlerDef))
                score -= 20;
            break;
        case EFFECT_SUBSTITUTE:
            if (gBattleMons[battlerAtk].status2 & STATUS2_SUBSTITUTE || AI_DATA->abilities[battlerDef] == ABILITY_INFILTRATOR)
                score -= 20;
            else if (AI_DATA->hpPercents[battlerAtk] <= 25)
                score -= 20;
            else if (HasSoundMove(battlerDef) && AI_DATA->abilities[battlerAtk] != ABILITY_SOUNDPROOF)
                score -= 20;
            break;
        case EFFECT_LEECH_SEED:
            if (gStatuses3[battlerDef] & STATUS3_LEECHSEED
             || IS_BATTLER_OF_TYPE(battlerDef, TYPE_GRASS)
             || DoesPartnerHaveSameMoveEffect(BATTLE_PARTNER(battlerAtk), battlerDef, move, AI_DATA->partnerMove))
                score -= 20;
            else if (AI_DATA->abilities[battlerDef] == ABILITY_LIQUID_OOZE || AI_DATA->abilities[battlerDef] == ABILITY_MAGIC_GUARD)
                score -= 20;
            else if (CanTargetFaintAi(battlerDef, battlerAtk))
            {
                if ((IsBattlerSashSturdyProtected(battlerAtk))
                || IsMimikyuDisguiseProtected(battlerAtk, battlerDef)
                || IsBattlerSubstituteProtected(battlerAtk, battlerDef))
                    break;
                score -= 20;
            }
            break;
        case EFFECT_DISABLE:
            if (gDisableStructs[battlerDef].disableTimer == 0
            #if B_MENTAL_HERB >= GEN_5
                && AI_DATA->holdEffects[battlerDef] != HOLD_EFFECT_MENTAL_HERB
            #endif
                && !PartnerHasSameMoveEffectWithoutTarget(BATTLE_PARTNER(battlerAtk), move, AI_DATA->partnerMove))
            {
                if (AI_WhosFaster(battlerAtk, battlerDef) == AI_IS_FASTER) // Attacker should go first
                {
                    if (gLastMoves[battlerDef] == MOVE_NONE || gLastMoves[battlerDef] == 0xFFFF)
                        score -= 20;    // no anticipated move to disable
                }
                else if (predictedMove == MOVE_NONE)
                {
                    score -= 20;
                }
            }
            else
            {
                score -= 20;
            }
            break;
        case EFFECT_ENCORE:
            if (gDisableStructs[battlerDef].encoreTimer == 0
                && !DoesPartnerHaveSameMoveEffect(BATTLE_PARTNER(battlerAtk), battlerDef, move, AI_DATA->partnerMove))
            {
                if (AI_WhosFaster(battlerAtk, battlerDef) == AI_IS_FASTER) // Attacker should go first
                {
                    if (gLastMoves[battlerDef] == MOVE_NONE || gLastMoves[battlerDef] == 0xFFFF)
                        score -= 20;    // no anticipated move to encore
                }
                else if (predictedMove == MOVE_NONE)
                {
                    score -= 20;
                }
            }
            else
            {
                score -= 20;
            }
            break;
        case EFFECT_SNORE:
        case EFFECT_SLEEP_TALK:
            if (IsWakeupTurn(battlerAtk) || !AI_IsBattlerAsleepOrComatose(battlerAtk))
                score -= 20;    // if mon will wake up, is not asleep, or is not comatose
            break;
        case EFFECT_OCTOLOCK:
            if (IsBattlerTrapped(battlerDef, TRUE) || (CountUsablePartyMons(battlerDef) == 0))
                score -= 20;
            break;
        case EFFECT_TRAP:
            if (GetMoveDamageResult(move) == MOVE_POWER_BEST)
                break;
        case EFFECT_MEAN_LOOK:
            if (IS_BATTLER_OF_TYPE(battlerDef, TYPE_GHOST))
            {
                score -= 20;
                break;
            }    
            if (IsBattlerTrapped(battlerDef, TRUE) 
                || DoesPartnerHaveSameMoveEffect(BATTLE_PARTNER(battlerAtk), battlerDef, move, AI_DATA->partnerMove)
                || AI_DATA->items[battlerDef] == ITEM_SHED_SHELL)
                score -= 20;
            break;
        case EFFECT_NIGHTMARE:
            if (gBattleMons[battlerDef].status2 & STATUS2_NIGHTMARE)
                score -= 20;
            else if (!(gBattleMons[battlerDef].status1 & STATUS1_SLEEP) || AI_DATA->abilities[battlerDef] == ABILITY_COMATOSE)
                score -= 20;
            else if (DoesPartnerHaveSameMoveEffect(BATTLE_PARTNER(battlerAtk), battlerDef, move, AI_DATA->partnerMove))
                score -= 20;
            break;
        case EFFECT_CURSE:
            if (IS_BATTLER_OF_TYPE(battlerAtk, TYPE_GHOST))
            {
                if (gBattleMons[battlerDef].status2 & STATUS2_CURSED
                  || DoesPartnerHaveSameMoveEffect(BATTLE_PARTNER(battlerAtk), battlerDef, move, AI_DATA->partnerMove))
                    score -= 20;
                else if (AI_DATA->hpPercents[battlerAtk] <= 50)
                    score -= 20;
            }
            else // regular curse
            {
                if (!BattlerStatCanRise(battlerAtk, AI_DATA->abilities[battlerAtk], STAT_ATK))
                score -= 20;
                if (CanTargetFaintAi(battlerDef, battlerAtk))
                {
                    if (IsBattlerSashSturdyProtected(battlerAtk)
                    || IsMimikyuDisguiseProtected(battlerAtk, battlerDef)
                    || IsBattlerSubstituteProtected(battlerAtk, battlerDef))
                        break;
                    score -= 20;
                }
                if (AI_DATA->abilities[battlerDef] == ABILITY_UNAWARE
                || HasMoveEffect(battlerDef, EFFECT_HAZE) 
                || HasMoveEffect(battlerDef, EFFECT_FREEZY_FROST)
                || HasMoveEffect(battlerDef, EFFECT_TOPSY_TURVY)
                || HasMoveEffect(battlerDef, EFFECT_CLEAR_SMOG))
                    score -= 20;
                break;
            }
            break;
        case EFFECT_SPIKES:
            if (AI_DATA->abilities[battlerDef] == ABILITY_MAGIC_BOUNCE)
                score -= 30;
            else if (gSideTimers[GetBattlerSide(battlerDef)].spikesAmount >= 3)
                score -= 20;
            else if (PartnerMoveIsSameNoTarget(BATTLE_PARTNER(battlerAtk), move, AI_DATA->partnerMove)
              && gSideTimers[GetBattlerSide(battlerDef)].spikesAmount == 2)
                score -= 20; // only one mon needs to set up the last layer of Spikes
            break;
        case EFFECT_STEALTH_ROCK:
            if (AI_DATA->abilities[battlerDef] == ABILITY_MAGIC_BOUNCE)
                score -= 30;
            else if (gSideTimers[GetBattlerSide(battlerDef)].stealthRockAmount > 0
              || PartnerMoveIsSameNoTarget(BATTLE_PARTNER(battlerAtk), move, AI_DATA->partnerMove)) //Only one mon needs to set up Stealth Rocks
                score -= 20;
            break;
        case EFFECT_TOXIC_SPIKES:
            if (AI_DATA->abilities[battlerDef] == ABILITY_MAGIC_BOUNCE)
                score -= 30;
            else if (gSideTimers[GetBattlerSide(battlerDef)].toxicSpikesAmount >= 2)
                score -= 20;
            else if (PartnerMoveIsSameNoTarget(BATTLE_PARTNER(battlerAtk), move, AI_DATA->partnerMove) && gSideTimers[GetBattlerSide(battlerDef)].toxicSpikesAmount == 1)
                score -= 20; // only one mon needs to set up the last layer of Toxic Spikes
            break;
        case EFFECT_STICKY_WEB:
            if (AI_DATA->abilities[battlerDef] == ABILITY_MAGIC_BOUNCE)
                score -= 30;
            else if (gSideTimers[GetBattlerSide(battlerDef)].stickyWebAmount)
                score -= 20;
            else if (PartnerMoveIsSameNoTarget(BATTLE_PARTNER(battlerAtk), move, AI_DATA->partnerMove) && gSideTimers[GetBattlerSide(battlerDef)].stickyWebAmount)
                score -= 20; // only one mon needs to set up Sticky Web
            break;
        case EFFECT_FORESIGHT:
            if (gBattleMons[battlerDef].status2 & STATUS2_FORESIGHT
                || AI_DATA->abilities[battlerAtk] == ABILITY_SCRAPPY 
                || AI_DATA->abilities[battlerAtk] == ABILITY_MINDS_EYE)
                score -= 20;
            else if (gBattleMons[battlerDef].statStages[STAT_EVASION] <= 4
              || !(IS_BATTLER_OF_TYPE(battlerDef, TYPE_GHOST))
              || DoesPartnerHaveSameMoveEffect(BATTLE_PARTNER(battlerAtk), battlerDef, move, AI_DATA->partnerMove))
                score -= 20;
            break;
        case EFFECT_PERISH_SONG:
            if (isDoubleBattle)
            {
                if (CountUsablePartyMons(battlerAtk) == 0
                  && AI_DATA->abilities[battlerAtk] != ABILITY_SOUNDPROOF
                  && AI_DATA->abilities[BATTLE_PARTNER(battlerAtk)] != ABILITY_SOUNDPROOF
                  && CountUsablePartyMons(FOE(battlerAtk)) >= 1)
                {
                    score -= 20; //Don't wipe your team if you're going to lose
                }
                else if ((!IsBattlerAlive(FOE(battlerAtk)) || AI_DATA->abilities[FOE(battlerAtk)] == ABILITY_SOUNDPROOF
                  || gStatuses3[FOE(battlerAtk)] & STATUS3_PERISH_SONG)
                  && (!IsBattlerAlive(BATTLE_PARTNER(FOE(battlerAtk))) || AI_DATA->abilities[BATTLE_PARTNER(FOE(battlerAtk))] == ABILITY_SOUNDPROOF
                  || gStatuses3[BATTLE_PARTNER(FOE(battlerAtk))] & STATUS3_PERISH_SONG))
                {
                    score -= 20; //Both enemies are perish songed
                }
                else if (DoesPartnerHaveSameMoveEffect(BATTLE_PARTNER(battlerAtk), battlerDef, move, AI_DATA->partnerMove))
                {
                    score -= 20;
                }
            }
            else
            {
                if (CountUsablePartyMons(battlerAtk) == 0 && AI_DATA->abilities[battlerAtk] != ABILITY_SOUNDPROOF
                  && CountUsablePartyMons(battlerDef) >= 1)
                    score -= 20;

                if (gStatuses3[FOE(battlerAtk)] & STATUS3_PERISH_SONG || AI_DATA->abilities[FOE(battlerAtk)] == ABILITY_SOUNDPROOF)
                    score -= 20;
            }
            break;
        case EFFECT_SANDSTORM:
            if (gBattleWeather & (B_WEATHER_SANDSTORM | B_WEATHER_PRIMAL_ANY)
             || PartnerMoveEffectIsWeather(BATTLE_PARTNER(battlerAtk), AI_DATA->partnerMove))
                score -= 20;
            break;
        case EFFECT_SUNNY_DAY:
            if (gBattleWeather & (B_WEATHER_SUN | B_WEATHER_PRIMAL_ANY)
             || PartnerMoveEffectIsWeather(BATTLE_PARTNER(battlerAtk), AI_DATA->partnerMove))
                score -= 20;
            break;
        case EFFECT_RAIN_DANCE:
            if (gBattleWeather & (B_WEATHER_RAIN | B_WEATHER_PRIMAL_ANY)
             || PartnerMoveEffectIsWeather(BATTLE_PARTNER(battlerAtk), AI_DATA->partnerMove))
                score -= 20;
            break;
        case EFFECT_HAIL:
            if (gBattleWeather & (B_WEATHER_HAIL | B_WEATHER_PRIMAL_ANY)
             || PartnerMoveEffectIsWeather(BATTLE_PARTNER(battlerAtk), AI_DATA->partnerMove))
                score -= 20;
            else if (gBattleWeather & B_WEATHER_SNOW)
                score -= 2; // mainly to prevent looping between hail and snow
            break;
        case EFFECT_SNOWSCAPE:
            if (gBattleWeather & (B_WEATHER_SNOW | B_WEATHER_PRIMAL_ANY)
             || PartnerMoveEffectIsWeather(BATTLE_PARTNER(battlerAtk), AI_DATA->partnerMove))
                score -= 20;
            else if (gBattleWeather & B_WEATHER_HAIL)
                score -= 2; // mainly to prevent looping between hail and snow
            break;
        case EFFECT_ATTRACT:
            if (!AI_CanBeInfatuated(battlerAtk, battlerDef, AI_DATA->abilities[battlerDef]))
                score -= 20;
            break;
        case EFFECT_SAFEGUARD:
            if (gSideStatuses[GetBattlerSide(battlerAtk)] & SIDE_STATUS_SAFEGUARD
              || PartnerHasSameMoveEffectWithoutTarget(BATTLE_PARTNER(battlerAtk), move, AI_DATA->partnerMove))
                score -= 20;
            break;
        case EFFECT_MAGNITUDE:
            if (AI_DATA->abilities[battlerDef] == ABILITY_LEVITATE)
                score -= 20;
            break;
        case EFFECT_PARTING_SHOT:
            if (CountUsablePartyMons(battlerAtk) == 0)
                score -= 20;
            break;
        case EFFECT_BATON_PASS:
            if (!AnyStatIsRaised(battlerAtk) 
                && (HasMoveEffect(battlerDef, EFFECT_ROAR) || HasMoveEffect(battlerDef, EFFECT_HIT_SWITCH_TARGET)))
            {
                score -= 20;
                break;
            }
            if (CountUsablePartyMons(battlerAtk) == 0)
            {
                score -= 20;
                break;
            }
            break;
        case EFFECT_TELEPORT:
        case EFFECT_CHILLY_RECEPTION:
            if (CountUsablePartyMons(battlerAtk) == 0)
            {
                score -= 20;
                break;
            }
            if (CanTargetFaintAi(battlerDef, battlerAtk) && AI_WhosFaster(battlerAtk, battlerDef) == AI_IS_FASTER)
            {
                if ((IsBattlerSashSturdyProtected(battlerAtk))
                || IsMimikyuDisguiseProtected(battlerAtk, battlerDef)
                || IsBattlerSubstituteProtected(battlerAtk, battlerDef))
                    break;
                score -= 20;
            }
            break;
        case EFFECT_HIT_ESCAPE:
            break;
        case EFFECT_RAPID_SPIN:
            if (GetMoveDamageResult(move) == MOVE_POWER_BEST)
                break;
            if ((gBattleMons[battlerAtk].status2 & STATUS2_WRAPPED) || (gStatuses3[battlerAtk] & STATUS3_LEECHSEED))
                break;  // check damage/accuracy
            if (GetMoveDamageResult(move) == MOVE_POWER_BEST
             || TestSheerForceFlag(battlerAtk, move))
                break;
            //Spin checks
            if (!(gSideStatuses[GetBattlerSide(battlerAtk)] & SIDE_STATUS_HAZARDS_ANY)) {
                if (AI_WhosFaster(battlerAtk, battlerDef) == AI_IS_FASTER)
                    score -= 20;
                else if (!BattlerStatCanRise(battlerAtk, AI_DATA->abilities[battlerAtk], STAT_SPEED))
                    score -= 20;
                else if (HasMoveEffect(battlerDef, EFFECT_HAZE) 
                    || HasMoveEffect(battlerDef, EFFECT_FREEZY_FROST)
                    || HasMoveEffect(battlerDef, EFFECT_TOPSY_TURVY)
                    || HasMoveEffect(battlerDef, EFFECT_CLEAR_SMOG)
                    || (HasMoveEffect(battlerDef, EFFECT_ROAR) && CountUsablePartyMons(battlerAtk) != 0)
                    || (HasMoveEffect(battlerDef, EFFECT_HIT_SWITCH_TARGET) && CountUsablePartyMons(battlerAtk) != 0))
                    score -= 20;
            }
            break;
        case EFFECT_CLANGOROUS_SOUL:
            if (AI_DATA->abilities[battlerDef] == ABILITY_UNAWARE
                || HasMoveEffect(battlerDef, EFFECT_HAZE) 
                || HasMoveEffect(battlerDef, EFFECT_FREEZY_FROST)
                || HasMoveEffect(battlerDef, EFFECT_TOPSY_TURVY)
                || HasMoveEffect(battlerDef, EFFECT_CLEAR_SMOG)
                || (HasMoveEffect(battlerDef, EFFECT_ROAR) && CountUsablePartyMons(battlerAtk) != 0)
                || (HasMoveEffect(battlerDef, EFFECT_HIT_SWITCH_TARGET) && CountUsablePartyMons(battlerAtk) != 0))
            {
                score -= 20;
                break;
            }
            if (CanTargetFaintAi(battlerDef, battlerAtk))
                score -= 20;
            if (gBattleMons[battlerAtk].hp <= gBattleMons[battlerAtk].maxHP / 3)
                score -= 20;
            break;
        case EFFECT_BELLY_DRUM:
        case EFFECT_FILLET_AWAY:
            if (AI_DATA->abilities[battlerDef] == ABILITY_UNAWARE
                || HasMoveEffect(battlerDef, EFFECT_HAZE) 
                || HasMoveEffect(battlerDef, EFFECT_FREEZY_FROST)
                || HasMoveEffect(battlerDef, EFFECT_TOPSY_TURVY)
                || HasMoveEffect(battlerDef, EFFECT_CLEAR_SMOG)
                || (HasMoveEffect(battlerDef, EFFECT_ROAR) && CountUsablePartyMons(battlerAtk) != 0)
                || (HasMoveEffect(battlerDef, EFFECT_HIT_SWITCH_TARGET) && CountUsablePartyMons(battlerAtk) != 0))
            {
                score -= 20;
                break;
            }
            if (AI_DATA->abilities[battlerAtk] == ABILITY_CONTRARY)
                score -= 20;
            else if (GetHealthPercentage(battlerAtk) <= 50)
                score -= 20;
            else if (gBattleMons[battlerAtk].statStages[STAT_ATK] > 6)
                score -= 20;
            break;
        case EFFECT_FUTURE_SIGHT:
            if (gSideStatuses[GetBattlerSide(battlerDef)] & SIDE_STATUS_FUTUREATTACK
              || gSideStatuses[GetBattlerSide(battlerAtk)] & SIDE_STATUS_FUTUREATTACK)
                score -= 20;
            else
                score += 6;
            break;
        case EFFECT_FAKE_OUT:
            if (move == MOVE_FIRST_IMPRESSION)
                break;
            if (!gDisableStructs[battlerAtk].isFirstTurn)
            {
                score -= 30;
                break;
            }
            if (AI_DATA->abilities[battlerDef] == ABILITY_INNER_FOCUS 
                || AI_DATA->abilities[battlerDef] == ABILITY_SHIELD_DUST
                || AI_DATA->items[battlerDef] == ITEM_COVERT_CLOAK)
            {
                if (!CanIndexMoveFaintTarget(battlerAtk, battlerDef, AI_THINKING_STRUCT->movesetIndex, 0))
                {
                    score -= 30;
                    break;
                }
            }
            break;
        case EFFECT_STOCKPILE:
            if (gDisableStructs[battlerAtk].stockpileCounter >= 3)
                score -= 20;
            break;
        case EFFECT_SPIT_UP:
            if (gDisableStructs[battlerAtk].stockpileCounter <= 1)
                score -= 20;
            break;
        case EFFECT_SWALLOW:
            if (gDisableStructs[battlerAtk].stockpileCounter == 0)
            {
                score -= 20;
            }
            else
            {
                if (AtMaxHp(battlerAtk))
                    score -= 20;
                else if (AI_DATA->hpPercents[battlerAtk] >= 80)
                    score -= 5; // do it if nothing better
            }
            break;
        case EFFECT_TORMENT:
            if (gBattleMons[battlerDef].status2 & STATUS2_TORMENT
              || DoesPartnerHaveSameMoveEffect(BATTLE_PARTNER(battlerAtk), battlerDef, move, AI_DATA->partnerMove))
            {
                score -= 20;
                break;
            }
        #if B_MENTAL_HERB >= GEN_5
            if (AI_DATA->holdEffects[battlerDef] == HOLD_EFFECT_MENTAL_HERB)
                score -= 6;
        #endif
            break;
        case EFFECT_WILL_O_WISP:
            if (!AI_CanBurn(battlerAtk, battlerDef, AI_DATA->abilities[battlerDef], BATTLE_PARTNER(battlerAtk), move, AI_DATA->partnerMove))
                score -= 20;
            break;
        case EFFECT_FOLLOW_ME:
        case EFFECT_HELPING_HAND:
        case EFFECT_DRAGON_CHEER:
            if (!isDoubleBattle
              || !IsBattlerAlive(BATTLE_PARTNER(battlerAtk))
              || PartnerHasSameMoveEffectWithoutTarget(BATTLE_PARTNER(battlerAtk), move, AI_DATA->partnerMove)
              || (AI_DATA->partnerMove != MOVE_NONE && IS_MOVE_STATUS(AI_DATA->partnerMove))
              || *(gBattleStruct->monToSwitchIntoId + BATTLE_PARTNER(battlerAtk)) != PARTY_SIZE) //Partner is switching out.
                score -= 20;
            break;
        case EFFECT_COACHING:
        if (!isDoubleBattle
            || !IsBattlerAlive(BATTLE_PARTNER(battlerAtk))
            || (AI_DATA->abilities[BATTLE_PARTNER(battlerAtk)] == ABILITY_CONTRARY)
            || *(gBattleStruct->monToSwitchIntoId + BATTLE_PARTNER(battlerAtk)) != PARTY_SIZE //Partner is switching out.
            || !HasMoveWithSplit(BATTLE_PARTNER(battlerAtk), SPLIT_PHYSICAL))
                score -= 20;
            break; 
        case EFFECT_TRICK:
        case EFFECT_CORROSIVE_GAS:
            if (AI_DATA->abilities[battlerDef] == ABILITY_STICKY_HOLD || AI_DATA->holdEffects[battlerDef] == HOLD_EFFECT_NONE)
                score -= 20;
            break;
        case EFFECT_INGRAIN:
            if (gStatuses3[battlerAtk] & STATUS3_ROOTED)
                score -= 20;
            break;
        case EFFECT_AQUA_RING:
            if (gStatuses3[battlerAtk] & STATUS3_AQUA_RING)
                score -= 20;
            break;
        case EFFECT_RECYCLE:
            if (GetUsedHeldItem(battlerAtk) == 0 || AI_DATA->items[battlerAtk] != 0)
                score -= 20;
            break;
        case EFFECT_IMPRISON:
            if (gStatuses3[battlerAtk] & STATUS3_IMPRISONED_OTHERS)
                score -= 20;
            break;
        case EFFECT_REFRESH:
            if (CanTargetFaintAi(battlerDef, battlerAtk))
                score -= 20;
            if (!(gBattleMons[battlerDef].status1 & (STATUS1_PSN_ANY | STATUS1_BURN | STATUS1_PARALYSIS)))
                score -= 20;
            break;
        case EFFECT_REFLECT_TYPE:
            if (gBattleMons[battlerAtk].type1 == gBattleMons[battlerDef].type1
            &&  gBattleMons[battlerAtk].type2 == gBattleMons[battlerDef].type2
            &&  gBattleMons[battlerAtk].type3 == gBattleMons[battlerDef].type3) {
                score -= 20;
            }
            break;
        case EFFECT_PSYCHO_SHIFT:
            if (gBattleMons[battlerAtk].status1 & STATUS1_PSN_ANY && !AI_CanPoison(battlerAtk, battlerDef, AI_DATA->abilities[battlerDef], move, AI_DATA->partnerMove))
                score -= 20;
            else if (gBattleMons[battlerAtk].status1 & STATUS1_BURN && !AI_CanBurn(battlerAtk, battlerDef,
              AI_DATA->abilities[battlerDef], BATTLE_PARTNER(battlerAtk), move, AI_DATA->partnerMove))
                score -= 20;
            else if (gBattleMons[battlerAtk].status1 & STATUS1_FROSTBITE && !AI_CanGiveFrostbite(battlerAtk, battlerDef,
              AI_DATA->abilities[battlerDef], BATTLE_PARTNER(battlerAtk), move, AI_DATA->partnerMove))
                score -= 20;
            else if (gBattleMons[battlerAtk].status1 & STATUS1_PARALYSIS && !AI_CanParalyze(battlerAtk, battlerDef, AI_DATA->abilities[battlerDef], move, AI_DATA->partnerMove))
                score -= 20;
            else if (gBattleMons[battlerAtk].status1 & STATUS1_SLEEP && !AI_CanPutToSleep(battlerAtk, battlerDef, AI_DATA->abilities[battlerDef], move, AI_DATA->partnerMove))
                score -= 20;
            else if (gBattleMons[battlerAtk].status1 == STATUS1_NONE)
                score -= 20;    // attacker has no status to transmit
            break;
        case EFFECT_SALT_CURE:
            if (GetMoveDamageResult(move) == MOVE_POWER_BEST)
                break;
            if (gStatuses4[battlerDef] & STATUS4_SALT_CURE)
                score -= 20;
            break;
        case EFFECT_SYRUP_BOMB:
            if (GetMoveDamageResult(move) == MOVE_POWER_BEST)
                break;
            if (gStatuses4[battlerDef] & STATUS4_SYRUP_BOMB)
                score -= 20;
            break;
        case EFFECT_MUD_SPORT:
            if (gFieldStatuses & STATUS_FIELD_MUDSPORT
              || gStatuses4[battlerAtk] & STATUS4_MUD_SPORT
              || PartnerHasSameMoveEffectWithoutTarget(BATTLE_PARTNER(battlerAtk), move, AI_DATA->partnerMove))
                score -= 20;
            break;
        case EFFECT_WATER_SPORT:
            if (gFieldStatuses & STATUS_FIELD_WATERSPORT
              || gStatuses4[battlerAtk] & STATUS4_WATER_SPORT
              || PartnerHasSameMoveEffectWithoutTarget(BATTLE_PARTNER(battlerAtk), move, AI_DATA->partnerMove))
                score -= 20;
            break;
        case EFFECT_ME_FIRST:
            if (gBattleMoves[predictedMove].meFirstBanned || AI_WhosFaster(battlerAtk, battlerDef) == AI_IS_SLOWER)
            {
                score -= 20;
                break;
            }
            return AI_CheckBadMove(battlerAtk, battlerDef, predictedMove, score);
        case EFFECT_COPYCAT:
            if (AI_WhosFaster(battlerAtk, battlerDef) == AI_IS_SLOWER)
                break;
            if (gBattleMoves[predictedMove].copycatBanned || predictedMove == MOVE_NONE)
            {
                score -= 20;
                break;
            }
            return AI_CheckBadMove(battlerAtk, battlerDef, predictedMove, score);
        case EFFECT_MIRROR_MOVE:
            if (AI_WhosFaster(battlerAtk, battlerDef) == AI_IS_SLOWER)
                break;
            if (gBattleMoves[predictedMove].mirrorMoveBanned || predictedMove == MOVE_NONE)
            {
                score -= 20;
                break;
            }
            return AI_CheckBadMove(battlerAtk, battlerDef, predictedMove, score);
        case EFFECT_FLOWER_SHIELD:
            if (!IS_BATTLER_OF_TYPE(battlerAtk, TYPE_GRASS)
              && !(isDoubleBattle && IS_BATTLER_OF_TYPE(BATTLE_PARTNER(battlerAtk), TYPE_GRASS)))
                score -= 20;
            break;
        case EFFECT_AROMATIC_MIST:
            if (!isDoubleBattle || gBattleMons[BATTLE_PARTNER(battlerAtk)].hp == 0 || !BattlerStatCanRise(BATTLE_PARTNER(battlerAtk), AI_DATA->abilities[BATTLE_PARTNER(battlerAtk)], STAT_SPDEF))
                score -= 20;
            break;
        case EFFECT_BIDE:
            if (!HasDamagingMove(battlerDef)
              || AI_DATA->hpPercents[battlerAtk] < 30 //Close to death
              || gBattleMons[battlerDef].status1 & (STATUS1_SLEEP | STATUS1_FREEZE)) //No point in biding if can't take damage
                score -= 20;
            break;
        case EFFECT_HIT_SWITCH_TARGET:
            if (gStatuses3[battlerDef] & STATUS3_PERISH_SONG)
                score -= 20;
            if (CanTargetFaintAi(battlerDef, battlerAtk))
            {
                if ((IsBattlerSashSturdyProtected(battlerAtk))
                || IsMimikyuDisguiseProtected(battlerAtk, battlerDef)
                || IsBattlerSubstituteProtected(battlerAtk, battlerDef))
                    break;
                score -= 20;
            }
            break;
        case EFFECT_CONVERSION:
            //Check first move type
            if (IS_BATTLER_OF_TYPE(battlerAtk, gBattleMoves[gBattleMons[battlerAtk].moves[0]].type))
                score -= 20;
            break;
        case EFFECT_TEATIME:
            if (!IsTargetingPartner(battlerAtk, battlerDef))
            {
                score -= 20;
                break;
            }
            break;
        case EFFECT_REST:
            if (!AI_CanSleep(battlerAtk, AI_DATA->abilities[battlerAtk]))
                score -= 20;
            //fallthrough
        case EFFECT_SHORE_UP:
        case EFFECT_RESTORE_HP:
        case EFFECT_SOFTBOILED:
        case EFFECT_MORNING_SUN:
        case EFFECT_SYNTHESIS:
        case EFFECT_MOONLIGHT:
        case EFFECT_ROOST:
        case EFFECT_STRENGTH_SAP:
            if (AtMaxHp(battlerAtk))
                score -= 20;
            else if (GetHealthPercentage(battlerAtk) >= 85)
                score -= 6; //No point in healing, but should at least do it if nothing better
            break;
        case EFFECT_PURIFY:
            if (!(gBattleMons[battlerDef].status1 & STATUS1_ANY))
                score -= 20;
            else if (battlerDef == BATTLE_PARTNER(battlerAtk))
                break; //Always heal your ally
            else if (AtMaxHp(battlerAtk))
                score -= 20;
            else if (AI_DATA->hpPercents[battlerAtk] >= 90)
                score -= 6; //No point in healing, but should at least do it if nothing better
            break;
        case EFFECT_TRANSFORM:
            if (gBattleMons[battlerAtk].status2 & STATUS2_TRANSFORMED
              || (gBattleMons[battlerDef].status2 & (STATUS2_TRANSFORMED | STATUS2_SUBSTITUTE))) //Leave out Illusion b/c AI is supposed to be fooled
                score -= 20;
            break;
        case EFFECT_RECHARGE:
            if (AI_DATA->abilities[battlerDef] == ABILITY_WONDER_GUARD && effectiveness < AI_EFFECTIVENESS_x2)
                score -= 20;
            else if (AI_DATA->abilities[battlerAtk] != ABILITY_TRUANT
              && !CanIndexMoveFaintTarget(battlerAtk, battlerDef, AI_THINKING_STRUCT->movesetIndex, 0))
                score -= 1;
            break;
        case EFFECT_SPITE:
        case EFFECT_MIMIC:
            if (AI_WhosFaster(battlerAtk, battlerDef) == AI_IS_FASTER) // Attacker should go first
            {
                if (gLastMoves[battlerDef] == MOVE_NONE
                  || gLastMoves[battlerDef] == 0xFFFF)
                    score -= 20;
            }
            else if (predictedMove == MOVE_NONE)
            {
                score -= 20;
            }
            break;
        case EFFECT_METRONOME:
            if (CountAllAlivePartyMons(battlerAtk) * 100 / CountAllPartyMons(battlerAtk) < 35) //Last third of the team cannot use Metronome
                score -= 5;
            break;
        case EFFECT_CONVERSION_2:
            if (gLastMoves[battlerDef] == MOVE_NONE || AI_GetMoveEffectiveness(gLastMoves[battlerDef], battlerDef, battlerAtk) < AI_EFFECTIVENESS_x1)
                score -= 20;
            break;
        case EFFECT_LOCK_ON:
            if (gStatuses3[battlerDef] & STATUS3_ALWAYS_HITS
              || AI_DATA->abilities[battlerAtk] == ABILITY_NO_GUARD
              || AI_DATA->abilities[battlerDef] == ABILITY_NO_GUARD
              || DoesPartnerHaveSameMoveEffect(BATTLE_PARTNER(battlerAtk), battlerDef, move, AI_DATA->partnerMove))
                score -= 20;
            break;
        case EFFECT_SKETCH:
            if (gLastMoves[battlerDef] == MOVE_NONE)
                score -= 20;
            break;
        case EFFECT_DESTINY_BOND:
            if (gBattleMons[battlerDef].status2 & STATUS2_DESTINY_BOND)
                score -= 20;
            if (IsMimikyuDisguiseProtected(battlerAtk, battlerDef))
                score -= 20;
            if (IsBattlerSashSturdyProtected(battlerAtk))
                score -=20;
            break;
        case EFFECT_HEAL_BELL:
            if (!AnyPartyMemberStatused(battlerAtk, gBattleMoves[move].soundMove) || PartnerHasSameMoveEffectWithoutTarget(BATTLE_PARTNER(battlerAtk), move, AI_DATA->partnerMove))
                score -= 20;
            break;
        case EFFECT_ENDURE:
            if (gBattleMons[battlerAtk].hp == 1 || GetBattlerSecondaryDamage(battlerAtk)) //Don't use Endure if you'll die after using it
                score -= 20;
            break;
        case EFFECT_MAT_BLOCK:
        case EFFECT_PROTECT:
            {
                bool32 decreased = FALSE;
                switch (move)
                {
                case MOVE_QUICK_GUARD:
                case MOVE_WIDE_GUARD:
                case MOVE_CRAFTY_SHIELD:
                case MOVE_MAT_BLOCK:
                    if (!isDoubleBattle)
                    {
                        score -= 20;
                        decreased = TRUE;
                    }
                    if (move == MOVE_MAT_BLOCK && !gDisableStructs[battlerAtk].isFirstTurn)
                    {
                        score -= 20;
                        decreased = TRUE;
                    }
                    break;
                } // move check

                if (decreased)
                    break;
                if (IsBattlerIncapacitated(battlerDef, AI_DATA->abilities[battlerDef]))
                {
                    score -= 20;
                    break;
                }

                if (move != MOVE_QUICK_GUARD
                  && move != MOVE_WIDE_GUARD
                  && move != MOVE_CRAFTY_SHIELD) // These moves have infinite usage
                {
                    if (GetBattlerSecondaryDamage(battlerAtk) >= gBattleMons[battlerAtk].hp
                      && AI_DATA->abilities[battlerDef] != ABILITY_MOXIE
                      && AI_DATA->abilities[battlerDef] != ABILITY_BEAST_BOOST)
                    {
                        score -= 10; // Don't protect if you're going to faint after protecting
                    }
                    else if (gDisableStructs[battlerAtk].protectUses == 1 && Random() % 100 < 50)
                    {
                        score -= 20;
                    }
                    else if (gDisableStructs[battlerAtk].protectUses >= 2)
                    {
                        score -= 20;
                    }
                }

                /*if (AI_THINKING_STRUCT->aiFlags == AI_SCRIPT_CHECK_BAD_MOVE //Only basic AI
                && IS_DOUBLE_BATTLE) //Make the regular AI know how to use Protect minimally in Doubles
                {
                    u8 shouldProtect = ShouldProtect(battlerAtk, battlerDef, move);
                    if (shouldProtect == USE_PROTECT || shouldProtect == PROTECT_FROM_FOES)
                        IncreaseFoeProtectionViability(&viability, 0xFF, battlerAtk, battlerDef);
                    else if (shouldProtect == PROTECT_FROM_ALLIES)
                        IncreaseAllyProtectionViability(&viability, 0xFF);
                }*/
            break;
            }
        case EFFECT_MIRACLE_EYE:
            if (gStatuses3[battlerDef] & STATUS3_MIRACLE_EYED)
                score -= 20;

            if (gBattleMons[battlerDef].statStages[STAT_EVASION] <= 4
              || !(IS_BATTLER_OF_TYPE(battlerDef, TYPE_DARK))
              || DoesPartnerHaveSameMoveEffect(BATTLE_PARTNER(battlerAtk), battlerDef, move, AI_DATA->partnerMove))
                score -= 20;
            break;
        case EFFECT_BURN_UP:
            if (!IS_BATTLER_OF_TYPE(battlerAtk, TYPE_FIRE))
                score -= 20;
            break;
        case EFFECT_DOUBLE_SHOCK:
            if (!IS_BATTLER_OF_TYPE(battlerAtk, TYPE_ELECTRIC))
                score -= 20;
            break;
        case EFFECT_DEFOG:
            if (gSideStatuses[GetBattlerSide(battlerDef)] & (SIDE_STATUS_SCREEN_ANY | SIDE_STATUS_SAFEGUARD | SIDE_STATUS_MIST)
              || gSideStatuses[GetBattlerSide(battlerAtk)] & SIDE_STATUS_HAZARDS_ANY)
            {
                if (PartnerHasSameMoveEffectWithoutTarget(BATTLE_PARTNER(battlerAtk), move, AI_DATA->partnerMove))
                {
                    score -= 20; //Only need one hazards removal
                    break;
                }
            }

            if (gSideStatuses[GetBattlerSide(battlerDef)] & SIDE_STATUS_HAZARDS_ANY)
            {
                score -= 20; //Don't blow away opposing hazards
                break;
            }

            if (isDoubleBattle)
            {
                if (IsHazardMoveEffect(gBattleMoves[AI_DATA->partnerMove].effect) // partner is going to set up hazards
                  && AI_WhoStrikesFirst(BATTLE_PARTNER(battlerAtk), battlerAtk, AI_DATA->partnerMove) == AI_IS_FASTER) // partner is going to set up before the potential Defog
                {
                    score -= 20;
                    break; // Don't use Defog if partner is going to set up hazards
                }
            }

            // evasion check
            if (gBattleMons[battlerDef].statStages[STAT_EVASION] == MIN_STAT_STAGE
              || ((AI_DATA->abilities[battlerDef] == ABILITY_CONTRARY) && !IsTargetingPartner(battlerAtk, battlerDef))) // don't want to raise target stats unless its your partner
                score -= 20;
            break;

        case EFFECT_PSYCH_UP:   // haze stats check
            {
                for (i = STAT_ATK; i < NUM_BATTLE_STATS; i++)
                {
                    if (gBattleMons[battlerAtk].statStages[i] > DEFAULT_STAT_STAGE || gBattleMons[BATTLE_PARTNER(battlerAtk)].statStages[i] > DEFAULT_STAT_STAGE)
                    {
                        score -= 20;  // Don't want to reset our boosted stats
                        break;
                    }
                }
                for (i = STAT_ATK; i < NUM_BATTLE_STATS; i++)
                {
                    if (gBattleMons[battlerDef].statStages[i] <= DEFAULT_STAT_STAGE || gBattleMons[BATTLE_PARTNER(battlerDef)].statStages[i] <= DEFAULT_STAT_STAGE)
                    {
                        score -= 20; //Don't want to copy enemy lowered stats
                        break;
                    }
                }
            }
            break;
        case EFFECT_SPECTRAL_THIEF:
            break;
        /*
        case EFFECT_SOLAR_BEAM:
            if (AI_DATA->holdEffects[battlerAtk] == HOLD_EFFECT_POWER_HERB
              || (AI_WeatherHasEffect() && gBattleWeather & B_WEATHER_SUN && AI_DATA->holdEffects[battlerAtk] != HOLD_EFFECT_UTILITY_UMBRELLA))
                break;
            if (CanTargetFaintAi(battlerDef, battlerAtk)) //Attacker can be knocked out
                score -= 4;
            break;
        */
        case EFFECT_SEMI_INVULNERABLE:
            if (predictedMove != MOVE_NONE
              && AI_WhosFaster(battlerAtk, battlerDef) == AI_IS_SLOWER
              && gBattleMoves[predictedMove].effect == EFFECT_SEMI_INVULNERABLE)
                score -= 20; // Don't Fly/dig/etc if opponent is going to fly/dig/etc after you

            if (BattlerWillFaintFromWeather(battlerAtk, AI_DATA->abilities[battlerAtk])
              && (move == MOVE_FLY || move == MOVE_BOUNCE))
                score -= 20; // Attacker will faint while in the air
            break;
        case EFFECT_HEALING_WISH:   //healing wish, lunar dance
            if (CountUsablePartyMons(battlerAtk) == 0 || DoesPartnerHaveSameMoveEffect(BATTLE_PARTNER(battlerAtk), battlerDef, move, AI_DATA->partnerMove))
                score -= 20;
            else if (IsPartyFullyHealedExceptBattler(battlerAtk))
                score -= 20;
            break;
        case EFFECT_MEMENTO:
            if (gBattleMons[battlerDef].statStages[STAT_ATK] == MIN_STAT_STAGE && gBattleMons[battlerDef].statStages[STAT_SPATK] == MIN_STAT_STAGE)
            {
                score -= 20;
                break;
            }
            else if (AI_DATA->items[battlerDef] == ITEM_CLEAR_AMULET)
            {
                score -= 20;
                break;
            }
            else if (CountUsablePartyMons(battlerAtk) == 0)
            {
                if (CountUsablePartyMons(battlerDef) != 0)
                    score -= 10;
                else
                    score--;
            }
            break;
        case EFFECT_EXPLOSION:
            if (effectiveness == AI_EFFECTIVENESS_x0)
            {
                score -= 20;
                break;
            }
            else if (IsAbilityOnField(ABILITY_DAMP) && !DoesBattlerIgnoreAbilityChecks(AI_DATA->abilities[battlerAtk], move))
            {
                score -= 20;
                break;
            }
            else if (CountUsablePartyMons(battlerAtk) == 0)
            {
                if (CountUsablePartyMons(battlerDef) != 0)
                    score -= 10;
                else
                    score--;
            }
            break;
        case EFFECT_FINAL_GAMBIT:
            if (CountUsablePartyMons(battlerAtk) == 0)
            {
                if (CountUsablePartyMons(battlerDef) != 0)
                    score -= 10;
                else
                    score--;
            }
            break;
        case EFFECT_NATURE_POWER:
            return AI_CheckBadMove(battlerAtk, battlerDef, GetNaturePowerMove(), score);
        case EFFECT_TAUNT:
            if (gDisableStructs[battlerDef].tauntTimer > 0
              || DoesPartnerHaveSameMoveEffect(BATTLE_PARTNER(battlerAtk), battlerDef, move, AI_DATA->partnerMove))
                score -= 20;
            break;
        case EFFECT_BESTOW:
            if (AI_DATA->holdEffects[battlerAtk] == HOLD_EFFECT_NONE
              || !CanBattlerGetOrLoseItem(battlerAtk, AI_DATA->items[battlerAtk]))    // AI knows its own item
                score -= 20;
            break;
        case EFFECT_ROLE_PLAY:
            if (AI_DATA->abilities[battlerAtk] == AI_DATA->abilities[battlerDef]
              || AI_DATA->abilities[battlerDef] == ABILITY_NONE
              || IsRolePlayDoodleBannedAbilityAttacker(AI_DATA->abilities[battlerAtk])
              || IsRolePlayDoodleBannedAbility(AI_DATA->abilities[battlerDef]))
                score -= 20;
            break;
        case EFFECT_DOODLE: // Same as Role Play, but also check if the partner's ability should be replaced
            if (AI_DATA->abilities[battlerAtk] == AI_DATA->abilities[battlerDef]
             || AI_DATA->abilities[BATTLE_PARTNER(battlerAtk)] == AI_DATA->abilities[battlerDef]
             || AI_DATA->abilities[battlerDef] == ABILITY_NONE
             || IsRolePlayDoodleBannedAbilityAttacker(AI_DATA->abilities[battlerAtk])
             || IsRolePlayDoodleBannedAbility(AI_DATA->abilities[battlerDef]))
                score -= 20;
            break;
        case EFFECT_WISH:
            if (gWishFutureKnock.wishCounter[battlerAtk] > gBattleTurnCounter)
                score -= 20;
            break;
        case EFFECT_ASSIST:
            break;
        case EFFECT_MAGIC_COAT:
            if (!HasMagicCoatAffectedMove(battlerDef))
                score -= 20;
            break;
        case EFFECT_YAWN:
            if (gStatuses3[battlerDef] & STATUS3_YAWN)
                score -= 20;
            else if (!AI_CanPutToSleep(battlerAtk, battlerDef, AI_DATA->abilities[battlerDef], move, AI_DATA->partnerMove))
                score -= 20;
            break;
        case EFFECT_SKILL_SWAP:
            if (AI_DATA->abilities[battlerAtk] == ABILITY_NONE || AI_DATA->abilities[battlerDef] == ABILITY_NONE
              || IsSkillSwapBannedAbility(AI_DATA->abilities[battlerAtk]) || IsSkillSwapBannedAbility(AI_DATA->abilities[battlerDef])
              || AI_DATA->holdEffects[battlerDef] == HOLD_EFFECT_ABILITY_SHIELD)
                score -= 20;
            break;
        case EFFECT_WORRY_SEED:
            if (AI_DATA->abilities[battlerDef] == ABILITY_INSOMNIA
              || IsWorrySeedBannedAbility(AI_DATA->abilities[battlerDef])
              || AI_DATA->holdEffects[battlerDef] == HOLD_EFFECT_ABILITY_SHIELD)
                score -= 20;
            break;
        case EFFECT_GASTRO_ACID:
            if (gStatuses3[battlerDef] & STATUS3_GASTRO_ACID
              || IsGastroAcidBannedAbility(AI_DATA->abilities[battlerDef]))
                score -= 20;
            break;
        case EFFECT_ENTRAINMENT:
            if (AI_DATA->abilities[battlerAtk] == ABILITY_NONE
              || IsEntrainmentBannedAbilityAttacker(AI_DATA->abilities[battlerAtk])
              || IsEntrainmentBannedAbility(AI_DATA->abilities[battlerDef])
              || AI_DATA->holdEffects[battlerDef] == HOLD_EFFECT_ABILITY_SHIELD
              || IsAbilityOfRating(AI_DATA->abilities[battlerAtk], 0))
                score -= 20;
            break;
        case EFFECT_CORE_ENFORCER:
            break;
        case EFFECT_SIMPLE_BEAM:
            if (IsSimpleBeamBannedAbility(AI_DATA->abilities[battlerDef])
              || AI_DATA->holdEffects[battlerDef] == HOLD_EFFECT_ABILITY_SHIELD)
                score -= 20;
            break;
        case EFFECT_SNATCH:
            if (!HasSnatchAffectedMove(battlerDef)
              || PartnerHasSameMoveEffectWithoutTarget(BATTLE_PARTNER(battlerAtk), move, AI_DATA->partnerMove))
                score -= 20;
            break;
        case EFFECT_POWER_TRICK:
            if (IsTargetingPartner(battlerAtk, battlerDef))
                score -= 20;
            else if (gBattleMons[battlerAtk].defense >= gBattleMons[battlerAtk].attack && !HasMoveWithSplit(battlerAtk, SPLIT_PHYSICAL))
                score -= 20;
            break;
        case EFFECT_POWER_SWAP: // Don't use if attacker's stat stages are higher than opponents
            if (IsTargetingPartner(battlerAtk, battlerDef))
                score -= 20;
            else if (gBattleMons[battlerAtk].statStages[STAT_ATK] >= gBattleMons[battlerDef].statStages[STAT_ATK]
              && gBattleMons[battlerAtk].statStages[STAT_SPATK] >= gBattleMons[battlerDef].statStages[STAT_SPATK])
                score -= 20;
            break;
        case EFFECT_GUARD_SWAP: // Don't use if attacker's stat stages are higher than opponents
            if (IsTargetingPartner(battlerAtk, battlerDef))
                score -= 20;
            else if (gBattleMons[battlerAtk].statStages[STAT_DEF] >= gBattleMons[battlerDef].statStages[STAT_DEF]
              && gBattleMons[battlerAtk].statStages[STAT_SPDEF] >= gBattleMons[battlerDef].statStages[STAT_SPDEF])
                score -= 20;
            break;
        case EFFECT_SPEED_SWAP:
            if (IsTargetingPartner(battlerAtk, battlerDef))
            {
                score -= 20;
            }
            else
            {
                if (gFieldStatuses & STATUS_FIELD_TRICK_ROOM && (gBattleMons[battlerAtk].speed <= gBattleMons[battlerDef].speed))
                    score -= 20;
                else if (gBattleMons[battlerAtk].speed >= gBattleMons[battlerDef].speed)
                    score -= 20;
            }
            break;
        case EFFECT_HEART_SWAP:
            if (IsTargetingPartner(battlerAtk, battlerDef))
            {
                score -= 20;
            }
            else
            {
                u32 atkPositiveStages = CountPositiveStatStages(battlerAtk);
                u32 atkNegativeStages = CountNegativeStatStages(battlerAtk);
                u32 defPositiveStages = CountPositiveStatStages(battlerDef);
                u32 defNegativeStages = CountNegativeStatStages(battlerDef);

                if (atkPositiveStages >= defPositiveStages && atkNegativeStages <= defNegativeStages)
                    score -= 20;
                break;
            }
            break;
        case EFFECT_POWER_SPLIT:
            if (IsTargetingPartner(battlerAtk, battlerDef))
            {
                score -= 20;
            }
            else
            {
                u8 atkAttack = gBattleMons[battlerAtk].attack;
                u8 defAttack = gBattleMons[battlerDef].attack;
                u8 atkSpAttack = gBattleMons[battlerAtk].spAttack;
                u8 defSpAttack = gBattleMons[battlerDef].spAttack;

                if (atkAttack + atkSpAttack >= defAttack + defSpAttack) // Combined attacker stats are > than combined target stats
                    score -= 200;
                break;
            }
            break;
        case EFFECT_GUARD_SPLIT:
            if (IsTargetingPartner(battlerAtk, battlerDef))
            {
                score -= 20;
            }
            else
            {
                u8 atkDefense = gBattleMons[battlerAtk].defense;
                u8 defDefense = gBattleMons[battlerDef].defense;
                u8 atkSpDefense = gBattleMons[battlerAtk].spDefense;
                u8 defSpDefense = gBattleMons[battlerDef].spDefense;

                if (atkDefense + atkSpDefense >= defDefense + defSpDefense) //Combined attacker stats are > than combined target stats
                    score -= 10;
                break;
            }
            break;
        case EFFECT_NATURAL_GIFT:
            if (AI_DATA->abilities[battlerAtk] == ABILITY_KLUTZ
              || gFieldStatuses & STATUS_FIELD_MAGIC_ROOM
              || GetPocketByItemId(AI_DATA->items[battlerAtk]) != POCKET_BERRIES)
                score -= 20;
            break;
        case EFFECT_GRASSY_TERRAIN:
            if (PartnerMoveEffectIsTerrain(BATTLE_PARTNER(battlerAtk), AI_DATA->partnerMove) || gFieldStatuses & STATUS_FIELD_GRASSY_TERRAIN)
                score -= 20;
            break;
        case EFFECT_ELECTRIC_TERRAIN:
            if (PartnerMoveEffectIsTerrain(BATTLE_PARTNER(battlerAtk), AI_DATA->partnerMove) || gFieldStatuses & STATUS_FIELD_ELECTRIC_TERRAIN)
                score -= 20;
            break;
        case EFFECT_PSYCHIC_TERRAIN:
            if (PartnerMoveEffectIsTerrain(BATTLE_PARTNER(battlerAtk), AI_DATA->partnerMove) || gFieldStatuses & STATUS_FIELD_PSYCHIC_TERRAIN)
                score -= 20;
            break;
        case EFFECT_MISTY_TERRAIN:
            if (PartnerMoveEffectIsTerrain(BATTLE_PARTNER(battlerAtk), AI_DATA->partnerMove) || gFieldStatuses & STATUS_FIELD_MISTY_TERRAIN)
                score -= 20;
            break;
        case EFFECT_PLEDGE:
            if (isDoubleBattle && gBattleMons[BATTLE_PARTNER(battlerAtk)].hp > 0)
            {
                if (AI_DATA->partnerMove != MOVE_NONE
                  && gBattleMoves[AI_DATA->partnerMove].effect == EFFECT_PLEDGE
                  && move != AI_DATA->partnerMove) // Different pledge moves
                {
                    if (gBattleMons[BATTLE_PARTNER(battlerAtk)].status1 & (STATUS1_SLEEP | STATUS1_FREEZE))
                    // && gBattleMons[BATTLE_PARTNER(battlerAtk)].status1 != 1) // Will wake up this turn - how would AI know
                        score -= 20; // Don't use combo move if your partner will cause failure
                }
            }
            break;
        case EFFECT_TRICK_ROOM:
            if (PartnerMoveIs(BATTLE_PARTNER(battlerAtk), AI_DATA->partnerMove, MOVE_TRICK_ROOM))
            {
                score -= 20;
            }
            else if (gFieldStatuses & STATUS_FIELD_TRICK_ROOM) // Trick Room Up
            {
                score -= 20; // Keep the Trick Room up
            }
            break;
        case EFFECT_MAGIC_ROOM:
            if (gFieldStatuses & STATUS_FIELD_MAGIC_ROOM || PartnerMoveIsSameNoTarget(BATTLE_PARTNER(battlerAtk), move, AI_DATA->partnerMove))
                score -= 20;
            break;
        case EFFECT_WONDER_ROOM:
            if (gFieldStatuses & STATUS_FIELD_WONDER_ROOM || PartnerMoveIsSameNoTarget(BATTLE_PARTNER(battlerAtk), move, AI_DATA->partnerMove))
                score -= 20;
            break;
        case EFFECT_GRAVITY:
            if (gFieldStatuses & STATUS_FIELD_GRAVITY)
                score -= 20;
            break;
        case EFFECT_ION_DELUGE:
            if (gFieldStatuses & STATUS_FIELD_ION_DELUGE
              || PartnerMoveIsSameNoTarget(BATTLE_PARTNER(battlerAtk), move, AI_DATA->partnerMove))
                score -= 20;
            break;
        case EFFECT_FLING:
            if (!CanFling(battlerAtk))
            {
                score -= 20;
            }
            else
            {
                /* TODO Fling
                u8 effect = gFlingTable[gBattleMons[battlerAtk].item].effect;
                switch (effect)
                {
                case MOVE_EFFECT_BURN:
                    if (!AI_CanBurn(battlerAtk, battlerDef, BATTLE_PARTNER(battlerAtk), move, AI_DATA->partnerMove))
                        score -= 10;
                    break;
                case MOVE_EFFECT_PARALYSIS:
                    if (!AI_CanParalyze(battlerAtk, battlerDef, AI_DATA->abilities[battlerDef], move, AI_DATA->partnerMove))
                        score -= 10;
                    break;
                case MOVE_EFFECT_POISON:
                    if (!AI_CanPoison(battlerAtk, battlerDef, AI_DATA->abilities[battlerDef], move, AI_DATA->partnerMove))
                        score -= 10;
                    break;
                case MOVE_EFFECT_TOXIC:
                    if (!AI_CanPoison(battlerAtk, battlerDef, AI_DATA->abilities[battlerDef], move, AI_DATA->partnerMove))
                        score -= 10;
                    break;
                case MOVE_EFFECT_FREEZE:
                    if (!CanBeFrozen(battlerDef, TRUE)
                     || MoveBlockedBySubstitute(move, battlerAtk, battlerDef))
                        score -= 10;
                    break;
                }*/
            }
            break;
        case EFFECT_EMBARGO:
            if (AI_DATA->abilities[battlerDef] == ABILITY_KLUTZ
              || gFieldStatuses & STATUS_FIELD_MAGIC_ROOM
              || gStatuses3[battlerDef] & STATUS3_EMBARGO
              || PartnerMoveIsSameAsAttacker(BATTLE_PARTNER(battlerAtk), battlerDef, move, AI_DATA->partnerMove))
                score -= 20;
            break;
        case EFFECT_POWDER:
            if (!HasMoveWithType(battlerDef, TYPE_FIRE)
              || PartnerMoveIsSameAsAttacker(BATTLE_PARTNER(battlerAtk), battlerDef, move, AI_DATA->partnerMove))
                score -= 20;
            break;
        case EFFECT_TELEKINESIS:
            if (gStatuses3[battlerDef] & (STATUS3_TELEKINESIS | STATUS3_ROOTED | STATUS3_SMACKED_DOWN)
              || gFieldStatuses & STATUS_FIELD_GRAVITY
              || AI_DATA->holdEffects[battlerDef] == HOLD_EFFECT_IRON_BALL
              || IsTelekinesisBannedSpecies(gBattleMons[battlerDef].species)
              || PartnerMoveIsSameAsAttacker(BATTLE_PARTNER(battlerAtk), battlerDef, move, AI_DATA->partnerMove))
                score -= 20;
            break;
        case EFFECT_THROAT_CHOP:
            break;
        case EFFECT_HEAL_BLOCK:
            if (gStatuses3[battlerDef] & STATUS3_HEAL_BLOCK
              || PartnerMoveIsSameAsAttacker(BATTLE_PARTNER(battlerAtk), battlerDef, move, AI_DATA->partnerMove))
                score -= 20;
            break;
        case EFFECT_SOAK:
            if(move == MOVE_SOAK)
            {
                if (PartnerMoveIsSameAsAttacker(BATTLE_PARTNER(battlerAtk), battlerDef, move, AI_DATA->partnerMove)
                || (gBattleMons[battlerDef].type1 == TYPE_WATER
                && gBattleMons[battlerDef].type2 == TYPE_WATER
                && gBattleMons[battlerDef].type3 == TYPE_MYSTERY))
                score -= 20;    // target is already water-only
            }
            if(move == MOVE_MAGIC_POWDER)
            {
                if (PartnerMoveIsSameAsAttacker(BATTLE_PARTNER(battlerAtk), battlerDef, move, AI_DATA->partnerMove)
                || (gBattleMons[battlerDef].type1 == TYPE_PSYCHIC
                && gBattleMons[battlerDef].type2 == TYPE_PSYCHIC
                && gBattleMons[battlerDef].type3 == TYPE_MYSTERY))
                score -= 20;    // target is already water-only
            }
            break;
        case EFFECT_THIRD_TYPE:
            switch (move)
            {
            case MOVE_TRICK_OR_TREAT:
                if (IS_BATTLER_OF_TYPE(battlerDef, TYPE_GHOST) || PartnerMoveIsSameAsAttacker(BATTLE_PARTNER(battlerAtk), battlerDef, move, AI_DATA->partnerMove))
                    score -= 20;
                break;
            case MOVE_FORESTS_CURSE:
                if (IS_BATTLER_OF_TYPE(battlerDef, TYPE_GRASS) || PartnerMoveIsSameAsAttacker(BATTLE_PARTNER(battlerAtk), battlerDef, move, AI_DATA->partnerMove))
                    score -= 20;
                break;
            }
            break;
        case EFFECT_HEAL_PULSE: // and floral healing
            if (!IsTargetingPartner(battlerAtk, battlerDef)) // Don't heal enemies
            {
                score -= 20;
                break;
            }
            // fallthrough
        case EFFECT_HIT_ENEMY_HEAL_ALLY:    // pollen puff
            if (IsTargetingPartner(battlerAtk, battlerDef))
            {
                if (gStatuses3[battlerDef] & STATUS3_HEAL_BLOCK)
                    return 0;
                if (AtMaxHp(battlerDef))
                    score -= 20;
                else if (gBattleMons[battlerDef].hp > gBattleMons[battlerDef].maxHP / 2)
                    score -= 5;
            }
            break;
        case EFFECT_ELECTRIFY:
            if (AI_WhosFaster(battlerAtk, battlerDef) == AI_IS_SLOWER
              //|| GetMoveTypeSpecial(battlerDef, predictedMove) == TYPE_ELECTRIC // Move will already be electric type
              || PartnerMoveIsSameAsAttacker(BATTLE_PARTNER(battlerAtk), battlerDef, move, AI_DATA->partnerMove))
                score -= 20;
            break;
        case EFFECT_TOPSY_TURVY:
            if (!IsTargetingPartner(battlerAtk, battlerDef))
            {
                u8 targetPositiveStages = CountPositiveStatStages(battlerDef);
                u8 targetNegativeStages = CountNegativeStatStages(battlerDef);

                if (targetPositiveStages == 0 //No good stat changes to make bad
                  || PartnerMoveIsSameAsAttacker(BATTLE_PARTNER(battlerAtk), battlerDef, move, AI_DATA->partnerMove))
                    score -= 20;

                else if (targetNegativeStages < targetPositiveStages)
                    score -= 20; //More stages would be made positive than negative
            }
            break;
        case EFFECT_FAIRY_LOCK:
            if ((gFieldStatuses & STATUS_FIELD_FAIRY_LOCK) || PartnerMoveIsSameNoTarget(BATTLE_PARTNER(battlerAtk), move, AI_DATA->partnerMove))
                score -= 20;
            break;
        case EFFECT_DO_NOTHING:
            score -= 20;
            break;
        case EFFECT_INSTRUCT:
            {
                u16 instructedMove;
                instructedMove = gLastMoves[battlerDef];

                if (instructedMove == MOVE_NONE
                  || gBattleMoves[instructedMove].instructBanned
                  || MoveRequiresRecharging(instructedMove)
                  || MoveCallsOtherMove(instructedMove)
                  || IsZMove(instructedMove)
                  || (gLockedMoves[battlerDef] != 0 && gLockedMoves[battlerDef] != 0xFFFF)
                  || gBattleMons[battlerDef].status2 & STATUS2_MULTIPLETURNS
                  || PartnerMoveIsSameAsAttacker(BATTLE_PARTNER(battlerAtk), battlerDef, move, AI_DATA->partnerMove))
                {
                    score -= 20;
                }
                else if (isDoubleBattle)
                {
                    if (!IsTargetingPartner(battlerAtk, battlerDef))
                        score -= 20;
                }
            }
            break;
        case EFFECT_QUASH:
            if (!isDoubleBattle
            || AI_WhosFaster(battlerAtk, battlerDef) == AI_IS_SLOWER
            || PartnerMoveIsSameAsAttacker(BATTLE_PARTNER(battlerAtk), battlerDef, move, AI_DATA->partnerMove))
                score -= 20;
            break;
        case EFFECT_AFTER_YOU:
            if (!IsTargetingPartner(battlerAtk, battlerDef)
              || !isDoubleBattle
              || AI_WhosFaster(battlerAtk, battlerDef) == AI_IS_SLOWER
              || PartnerMoveIsSameAsAttacker(BATTLE_PARTNER(battlerAtk), battlerDef, move, AI_DATA->partnerMove))
                score -= 20;
            break;
        case EFFECT_TAILWIND:
            if (gSideStatuses[GetBattlerSide(battlerAtk)] & SIDE_STATUS_TAILWIND
              || PartnerMoveIs(BATTLE_PARTNER(battlerAtk), AI_DATA->partnerMove, MOVE_TAILWIND)
              || (gFieldStatuses & STATUS_FIELD_TRICK_ROOM)) // Trick Room active and not ending this turn
                score -= 20;
            break;
        case EFFECT_LUCKY_CHANT:
            if (gSideStatuses[GetBattlerSide(battlerAtk)] & SIDE_STATUS_LUCKY_CHANT
              || PartnerMoveIsSameNoTarget(BATTLE_PARTNER(battlerAtk), move, AI_DATA->partnerMove))
                score -= 20;
            break;
        case EFFECT_MAGNET_RISE:
            if (gFieldStatuses & STATUS_FIELD_GRAVITY
              ||  gDisableStructs[battlerAtk].magnetRiseTimer > gBattleTurnCounter
              || AI_DATA->holdEffects[battlerAtk] == HOLD_EFFECT_IRON_BALL
              || gStatuses3[battlerAtk] & (STATUS3_ROOTED | STATUS3_MAGNET_RISE | STATUS3_SMACKED_DOWN)
              || !IsBattlerGrounded(battlerAtk))
                score -= 20;
            break;
        case EFFECT_CAMOUFLAGE:
            if (!CanCamouflage(battlerAtk))
                score -= 20;
            break;
        case EFFECT_LAST_RESORT:
            if (!CanUseLastResort(battlerAtk))
                score -= 60;
            break;
        case EFFECT_SYNCHRONOISE:
            //Check holding ring target or is of same type
            if (AI_DATA->holdEffects[battlerDef] == HOLD_EFFECT_RING_TARGET
              || IS_BATTLER_OF_TYPE(battlerDef, gBattleMons[battlerAtk].type1)
              || IS_BATTLER_OF_TYPE(battlerDef, gBattleMons[battlerAtk].type2)
              || IS_BATTLER_OF_TYPE(battlerDef, gBattleMons[battlerAtk].type3))
                break;
            else
                score -= 20;
            break;
        case EFFECT_SKY_DROP:
            if (IS_BATTLER_OF_TYPE(battlerDef, TYPE_FLYING))
                score -= 20;
            if (BattlerWillFaintFromWeather(battlerAtk, AI_DATA->abilities[battlerAtk])
            ||  DoesSubstituteBlockMove(battlerAtk, battlerDef, move)
            ||  GetBattlerWeight(battlerDef) >= 2000) //200.0 kg
                score -= 20;
            break;
        case EFFECT_NO_RETREAT:
            if (gDisableStructs[battlerAtk].noRetreat)
            {
                score -= 20;
                break;
            }
            if (CanTargetFaintAi(battlerDef, battlerAtk))
            {
                if ((IsBattlerSashSturdyProtected(battlerAtk))
                || IsMimikyuDisguiseProtected(battlerAtk, battlerDef)
                || IsBattlerSubstituteProtected(battlerAtk, battlerDef))
                    break;
                score -= 20;
                break;
            }
            if (AI_DATA->abilities[battlerDef] == ABILITY_UNAWARE
                || HasMoveEffect(battlerDef, EFFECT_HAZE) 
                || HasMoveEffect(battlerDef, EFFECT_FREEZY_FROST)
                || HasMoveEffect(battlerDef, EFFECT_TOPSY_TURVY)
                || HasMoveEffect(battlerDef, EFFECT_CLEAR_SMOG))
                score -= 20;
            break;
        case EFFECT_REVIVAL_BLESSING:
            if (GetFirstFaintedPartyIndex(battlerAtk) == PARTY_SIZE)
                score -= 20;
            else if (CanAIFaintTarget(battlerAtk, battlerDef, 0))
                score -= 10;
            else if (CanTargetFaintAi(battlerDef, battlerAtk)
             && AI_WhosFaster(battlerAtk, battlerDef) == AI_IS_SLOWER)
                score -= 20;
            break;
        case EFFECT_UPPER_HAND:
            if (AI_RandLessThan(204))
            {
                score -= 20;
                break;
            }
            if (!HasMoveWithAttackPrio(battlerDef))
                score -= 20;
            break;
        case EFFECT_PLACEHOLDER:
            return 0;   // cannot even select
    } // move effect checks

    if (score < 0)
        score = 0;

    return score;
}

static s16 AI_TryToFaint(u8 battlerAtk, u8 battlerDef, u16 move, s16 score)
{
    if (IsTargetingPartner(battlerAtk, battlerDef))
        return score;

    if (IS_MOVE_STATUS(move) && IsHoldEffectChoice(AI_DATA->holdEffects[battlerAtk]) && !IsChoiceStatusMove(move))
        return score -= 20;
        
    if (IS_MOVE_STATUS(move)
        && move != MOVE_NATURE_POWER
        && move != MOVE_MEMENTO)
        return score += 6;   // tie status moves with highest damage
    
    if (gBattleMoves[move].effect == EFFECT_OHKO) //tie ohko moves with highest damage
        return score += 6;

    if (CanIndexMoveFaintTarget(battlerAtk, battlerDef, AI_THINKING_STRUCT->movesetIndex, 0) 
    && gBattleMoves[move].effect != EFFECT_EXPLOSION && gBattleMoves[move].effect != EFFECT_FINAL_GAMBIT
    && gBattleMoves[move].effect != EFFECT_ROLLOUT)
    {
        //fast kill over slow kill
        if (AI_WhosFaster(battlerAtk, battlerDef) == AI_IS_FASTER || GetMovePriority(battlerAtk, move) > 0)
            score += 6;
        else
            score += 3;
    }

    return score;
}

// double battle logic
static s16 AI_DoubleBattle(u8 battlerAtk, u8 battlerDef, u16 move, s16 score)
{
    // move data
    u8 moveType = gBattleMoves[move].type;
    u16 effect = gBattleMoves[move].effect;
    u16 moveTarget = AI_GetBattlerMoveTargetType(battlerAtk, move);
    // ally data
    u8 battlerAtkPartner = BATTLE_PARTNER(battlerAtk);
    u16 atkAbility = AI_DATA->abilities[battlerAtk];
    u16 atkPartnerAbility = AI_DATA->abilities[BATTLE_PARTNER(battlerAtk)];
    u16 atkPartnerHoldEffect = AI_DATA->holdEffects[BATTLE_PARTNER(battlerAtk)];
    bool32 partnerProtecting = (gBattleMoves[AI_DATA->partnerMove].effect == EFFECT_PROTECT);
    bool32 attackerHasBadAbility = (GetAbilityRating(AI_DATA->abilities[battlerAtk]) < 0);
    bool32 partnerHasBadAbility = (GetAbilityRating(atkPartnerAbility) < 0);
    u16 predictedMove = AI_DATA->predictedMoves[battlerDef];
    u32 effectiveness = AI_DATA->effectiveness[battlerAtk][battlerDef][AI_THINKING_STRUCT->movesetIndex];

    SetTypeBeforeUsingMove(move, battlerAtk);
    GET_MOVE_TYPE(move, moveType);

    // check what effect partner is using
    if (AI_DATA->partnerMove != 0)
    {
        switch (gBattleMoves[AI_DATA->partnerMove].effect)
        {
        case EFFECT_ROUND:
            //This only works if the Slower Round user is in Slot 2
            //Slot 1 functionality is hardcoded
            if (AI_WhosFaster(battlerAtk, battlerAtkPartner) == AI_IS_SLOWER && move == MOVE_ROUND)
                score += 13;
            break;
        }
    } // check partner move effect


    // consider our move effect relative to partner state
    switch (effect)
    {
    case EFFECT_ROUND:
        if (AI_RandLessThan(26))
            break;
        if (AI_WhosFaster(battlerAtk, battlerAtkPartner) == AI_IS_FASTER && HasMove(battlerAtkPartner, MOVE_ROUND)) {
            score += 7;
            //If the faster Round user is in Slot 2, it has to override Slot 1s prior decision.
            //This ugly way is the only way to do it. 
            //0 will always be the Round index because in Teambuilding I will always put Round in Index 0
            gBattleStruct->aiFinalScore[battlerAtkPartner][battlerDef][0] += 13;
        }
        break;
    case EFFECT_HELPING_HAND:
        if (AI_DATA->partnerMove != 0 && !HasDamagingMove(battlerAtkPartner))
            score -= 5;
        break;
    } // our effect relative to partner


    // consider global move effects
    switch (effect)
    {
    case EFFECT_SANDSTORM:
        if (ShouldSetSandstorm(battlerAtkPartner, atkPartnerAbility, atkPartnerHoldEffect))
        {
            RETURN_SCORE_PLUS(1);   // our partner benefits from sandstorm
        }
        break;
    case EFFECT_RAIN_DANCE:
        if (ShouldSetRain(battlerAtkPartner, atkPartnerAbility, atkPartnerHoldEffect))
        {
            RETURN_SCORE_PLUS(1);   // our partner benefits from rain
        }
        break;
    case EFFECT_SUNNY_DAY:
        if (ShouldSetSun(battlerAtkPartner, atkPartnerAbility, atkPartnerHoldEffect))
        {
            RETURN_SCORE_PLUS(1);   // our partner benefits from sun
        }
        break;
    case EFFECT_HAIL:
        if (IsBattlerAlive(battlerAtkPartner)
         && ShouldSetHail(battlerAtkPartner, atkPartnerAbility, atkPartnerHoldEffect))
        {
            RETURN_SCORE_PLUS(2);   // our partner benefits from hail
        }
        break;
    case EFFECT_SNOWSCAPE:
        if (IsBattlerAlive(battlerAtkPartner)
         && ShouldSetSnow(battlerAtkPartner, atkPartnerAbility, atkPartnerHoldEffect))
        {
            RETURN_SCORE_PLUS(2);   // our partner benefits from snow
        }
        break;
    } // global move effect check


    // check specific target
    if (IsTargetingPartner(battlerAtk, battlerDef))
    {
        switch (effect) { 
            //Not necessary to check partner protecting here since those moves bypass protection
            case EFFECT_ROLE_PLAY:
                if (AI_DATA->abilities[battlerAtk] != ABILITY_PURE_POWER 
                    && AI_DATA->abilities[battlerAtk] != ABILITY_HUGE_POWER
                    && (AI_DATA->abilities[BATTLE_PARTNER(battlerAtk)] == ABILITY_PURE_POWER 
                        || AI_DATA->abilities[BATTLE_PARTNER(battlerAtk)] == ABILITY_HUGE_POWER
                        || AI_DATA->abilities[BATTLE_PARTNER(battlerAtk)] == ABILITY_ILLUMINATE))
                    RETURN_SCORE_PLUS(AI_RandLessThan(64) ? 10 : 8)
                else 
                    RETURN_SCORE_MINUS(30)
                break;
            case EFFECT_TEATIME:
                if (AI_RandLessThan(128))
                    score--;
                if (IsPinchBerryItemEffect(AI_DATA->holdEffects[battlerAtk]) || IsPinchBerryItemEffect(AI_DATA->holdEffects[BATTLE_PARTNER(battlerAtk)]))
                    score += 8;
                else if (ItemId_GetPocket(AI_DATA->items[battlerAtk]) == POCKET_BERRIES 
                || ItemId_GetPocket(AI_DATA->items[BATTLE_PARTNER(battlerAtk)]) == POCKET_BERRIES)
                    score += 6;
                return score;
            case EFFECT_PSYCH_UP:
                if (gBattleMons[battlerAtk].statStages[STAT_ATK] < gBattleMons[battlerAtkPartner].statStages[STAT_ATK]
                && gBattleMons[battlerAtkPartner].statStages[STAT_ATK] > DEFAULT_STAT_STAGE)
                    RETURN_SCORE_PLUS(AI_RandLessThan(64) ? 10 : 8);
                if (gBattleMons[battlerAtk].statStages[STAT_SPATK] < gBattleMons[battlerAtkPartner].statStages[STAT_SPATK]
                && gBattleMons[battlerAtkPartner].statStages[STAT_SPATK] > DEFAULT_STAT_STAGE)
                    RETURN_SCORE_PLUS(AI_RandLessThan(64) ? 10 : 8);
                if (gBattleMons[battlerAtk].statStages[STAT_SPEED] < gBattleMons[battlerAtkPartner].statStages[STAT_SPEED]
                && gBattleMons[battlerAtkPartner].statStages[STAT_SPATK] > DEFAULT_STAT_STAGE)
                    RETURN_SCORE_PLUS(AI_RandLessThan(64) ? 10 : 8);
                break;
                    
        }
            // partner ability checks
            if (!partnerProtecting && moveTarget != MOVE_TARGET_BOTH && !DoesBattlerIgnoreAbilityChecks(AI_DATA->abilities[battlerAtk], move))
            {
            switch (atkPartnerAbility)
            {
            case ABILITY_VOLT_ABSORB:
                if (moveType == TYPE_ELECTRIC && AI_DATA->hpPercents[battlerAtkPartner] <= 75)
                {
                    if (AI_RandLessThan(64))
                        score += 6;
                    return score;
                }
                break;
            case ABILITY_EARTH_EATER:
                if (moveType == TYPE_GROUND && AI_DATA->hpPercents[battlerAtkPartner] <= 75)
                {
                    if (AI_RandLessThan(64))
                        score += 6;
                    return score;
                }
                break;
            case ABILITY_MOTOR_DRIVE:
                if (moveType == TYPE_ELECTRIC && BattlerStatCanRise(battlerAtkPartner, atkPartnerAbility, STAT_SPEED))
                {
                    if (AI_RandLessThan(64))
                        score += 6;
                    return score;
                }
                break;
            case ABILITY_LIGHTNING_ROD:
                if (moveType == TYPE_ELECTRIC
                    && HasMoveWithSplit(battlerAtkPartner, SPLIT_SPECIAL)
                    && BattlerStatCanRise(battlerAtkPartner, atkPartnerAbility, STAT_SPATK))
                {
                    if (AI_RandLessThan(64))
                        score += 6;
                    return score;
                }
                break;
            case ABILITY_WATER_ABSORB:
            case ABILITY_DRY_SKIN:
                if (moveType == TYPE_WATER && AI_DATA->hpPercents[battlerAtkPartner] <= 75)
                {
                    if (AI_RandLessThan(64))
                        score += 6;
                    return score;
                }
                break; 
            case ABILITY_STORM_DRAIN:
                if (moveType == TYPE_WATER
                    && HasMoveWithSplit(battlerAtkPartner, SPLIT_SPECIAL)
                    && BattlerStatCanRise(battlerAtkPartner, atkPartnerAbility, STAT_SPATK))
                {
                    if (AI_RandLessThan(64))
                        score += 6;
                    return score;
                }
                break;
            case ABILITY_FLASH_FIRE:
                if (moveType == TYPE_FIRE
                    && HasMoveWithType(battlerAtkPartner, TYPE_FIRE)
                    && !gDisableStructs[battlerAtkPartner].flashFireBoosted)
                {
                    if (AI_RandLessThan(64))
                        score += 6;
                    return score;
                }
                break;
            case ABILITY_SAP_SIPPER:
                if (moveType == TYPE_GRASS
                    && HasMoveWithSplit(battlerAtkPartner, SPLIT_PHYSICAL)
                    && BattlerStatCanRise(battlerAtkPartner, atkPartnerAbility, STAT_ATK))
                {
                    if (AI_RandLessThan(64))
                        score += 6;
                    return score;
                }
                break;
            case ABILITY_CONTRARY:
                if (IsStatLoweringEffect(effect))
                {
                    if (AI_RandLessThan(64))
                        score += 6;
                    return score;
                }
                break;
            }
        } // move power check

        // attacker move effects specifically targeting partner
        if (!partnerProtecting)
        {
            switch (effect)
            {
            case EFFECT_HIT:
                if ((atkPartnerAbility == ABILITY_MUMMY || atkPartnerAbility == ABILITY_WANDERING_SPIRIT)
                    && (atkAbility == ABILITY_EMERGENCY_EXIT || atkAbility == ABILITY_TRUANT || atkAbility == ABILITY_DEFIANT || atkAbility == ABILITY_SLOW_START)
                    && moveTarget == MOVE_TARGET_FOES_AND_ALLY
                    && !CanIndexMoveFaintTarget(battlerAtk, battlerAtkPartner, AI_THINKING_STRUCT->movesetIndex, 1))
                    RETURN_SCORE_PLUS(AI_RandLessThan(64) ? 9 : 7)
                else if (atkPartnerHoldEffect == HOLD_EFFECT_WEAKNESS_POLICY
                    && (move == MOVE_ICE_SHARD || move == MOVE_FLING)
                    && AI_GetMoveEffectiveness(move, battlerAtk, battlerAtkPartner) >= AI_EFFECTIVENESS_x2)
                    RETURN_SCORE_PLUS(AI_RandLessThan(64) ? 12 : 7)
                RETURN_SCORE_MINUS(30);
            case EFFECT_FLING:
                if (AI_DATA->holdEffects[battlerAtk] == HOLD_EFFECT_SPEED_UP) {
                    if (atkPartnerHoldEffect == HOLD_EFFECT_WEAKNESS_POLICY
                        || AI_GetMoveEffectiveness(move, battlerAtk, battlerAtkPartner) >= AI_EFFECTIVENESS_x2)
                    {
                        RETURN_SCORE_PLUS(AI_RandLessThan(64) ? 12 : 8)
                    }
                    else if (GetBattlerTotalSpeedStat(battlerAtkPartner) < GetBattlerTotalSpeedStat(FOE(battlerAtkPartner))
                        || GetBattlerTotalSpeedStat(battlerAtkPartner) < GetBattlerTotalSpeedStat(FOE(battlerAtk)))
                        RETURN_SCORE_PLUS(AI_RandLessThan(64) ? 10 : 8)
                    else 
                        RETURN_SCORE_PLUS(AI_RandLessThan(64) ? 8 : 7)
                }
                RETURN_SCORE_MINUS(30)
            case EFFECT_SWAGGER:
                if (gBattleMons[battlerAtkPartner].statStages[STAT_ATK] < MAX_STAT_STAGE
                 && HasMoveWithSplit(battlerAtkPartner, SPLIT_PHYSICAL)
                 && (!AI_CanBeConfused(battlerAtk, battlerAtkPartner, move, TRUE)
                  || atkPartnerHoldEffect == HOLD_EFFECT_CURE_CONFUSION
                  || atkPartnerHoldEffect == HOLD_EFFECT_CURE_STATUS))
                {
                    RETURN_SCORE_PLUS(AI_RandLessThan(64) ? 9 : 8)
                }
                break;
            case EFFECT_FLATTER:
                if (gBattleMons[battlerAtkPartner].statStages[STAT_SPATK] < MAX_STAT_STAGE
                 && HasMoveWithSplit(battlerAtkPartner, SPLIT_SPECIAL)
                 && (!AI_CanBeConfused(battlerAtk, battlerAtkPartner, move, TRUE)
                  || atkPartnerHoldEffect == HOLD_EFFECT_CURE_CONFUSION
                  || atkPartnerHoldEffect == HOLD_EFFECT_CURE_STATUS))
                {
                    RETURN_SCORE_PLUS(AI_RandLessThan(64) ? 9 : 8)
                }
                break;
            case EFFECT_BEAT_UP:
                if ((atkPartnerAbility == ABILITY_JUSTIFIED || AI_DATA->items[battlerAtkPartner] == ITEM_SCRAFTINITE)
                  && moveType == TYPE_DARK
                  && !IS_MOVE_STATUS(move)
                  && HasMoveWithSplit(battlerAtkPartner, SPLIT_PHYSICAL)
                  && gBattleMons[battlerAtkPartner].statStages[STAT_ATK] < 8
                  && !CanIndexMoveFaintTarget(battlerAtk, battlerAtkPartner, AI_THINKING_STRUCT->movesetIndex, 0)
                  && gDisableStructs[battlerAtkPartner].isFirstTurn)
                {
                    score += 9;
                    if (AI_RandLessThan(64)) 
                        score += 3;
                    return score;
                }
                else 
                {
                    score -= 30;
                    return score;
                }
                break;
            case EFFECT_INSTRUCT:
                {
                    u16 instructedMove;
                    score += 6;
                    if (AI_WhoStrikesFirst(battlerAtk, battlerAtkPartner, move) == AI_IS_SLOWER)
                        instructedMove = AI_DATA->partnerMove;
                    else
                        instructedMove = gLastMoves[battlerAtkPartner];

                    if (instructedMove != MOVE_NONE
                      && !IS_MOVE_STATUS(instructedMove)
                      && (AI_GetBattlerMoveTargetType(battlerAtkPartner, instructedMove) & (MOVE_TARGET_BOTH | MOVE_TARGET_FOES_AND_ALLY))
                      && AI_RandLessThan(128)) // Use instruct on multi-target moves
                    {
                        RETURN_SCORE_PLUS(1);
                    }
                }
                break;
            case EFFECT_TOPSY_TURVY:
                if (CountPositiveStatStages(battlerAtkPartner) < CountNegativeStatStages(battlerAtkPartner))
                    RETURN_SCORE_PLUS(AI_RandLessThan(64) ? 10 : 8)
                break;
            case EFFECT_ACUPRESSURE:
            case EFFECT_SPEED_SWAP:
                RETURN_SCORE_PLUS(AI_RandLessThan(64) ? 8 : 0)

            //Revisit this as this is leftover code
            case EFFECT_SKILL_SWAP:
                if (AI_DATA->abilities[battlerAtk] != AI_DATA->abilities[BATTLE_PARTNER(battlerAtk)] && !attackerHasBadAbility)
                {
                    if (AI_DATA->abilities[BATTLE_PARTNER(battlerAtk)] == ABILITY_TRUANT)
                    {
                        RETURN_SCORE_PLUS(10);
                    }
                    else if (AI_DATA->abilities[battlerAtk] == ABILITY_COMPOUND_EYES
                     && HasMoveWithLowAccuracy(battlerAtkPartner, FOE(battlerAtkPartner), 90, TRUE, atkPartnerAbility, AI_DATA->abilities[FOE(battlerAtkPartner)], atkPartnerHoldEffect, AI_DATA->holdEffects[FOE(battlerAtkPartner)]))
                    {
                        RETURN_SCORE_PLUS(3);
                    }
                }
                break;
            case EFFECT_WORRY_SEED:
            case EFFECT_GASTRO_ACID:
            case EFFECT_SIMPLE_BEAM:
                if (partnerHasBadAbility)
                {
                    RETURN_SCORE_PLUS(2);
                }
                break;
            case EFFECT_ENTRAINMENT:
                if (AI_DATA->abilities[battlerAtk] == ABILITY_HUGE_POWER
                 && AI_DATA->abilities[BATTLE_PARTNER(battlerAtk)] != ABILITY_PURE_POWER
                 && AI_DATA->abilities[BATTLE_PARTNER(battlerAtk)] != ABILITY_HUGE_POWER)
                {
                    RETURN_SCORE_PLUS(AI_RandLessThan(64) ? 10 : 8)
                }
                break;
            case EFFECT_SOAK:
                if (atkPartnerAbility == ABILITY_WONDER_GUARD
                 && (gBattleMons[battlerAtkPartner].type1 != TYPE_WATER
                 || gBattleMons[battlerAtkPartner].type2 != TYPE_WATER
                 || gBattleMons[battlerAtkPartner].type3 != TYPE_WATER))
                {
                    RETURN_SCORE_PLUS(1);
                }
                break;
            case EFFECT_AFTER_YOU:
                if (AI_WhoStrikesFirst(battlerAtkPartner, FOE(battlerAtkPartner), AI_DATA->partnerMove) == AI_IS_SLOWER  // Opponent mon 1 goes before partner
                  || AI_WhoStrikesFirst(battlerAtkPartner, BATTLE_PARTNER(FOE(battlerAtkPartner)), AI_DATA->partnerMove) == AI_IS_SLOWER) // Opponent mon 2 goes before partner
                {
                    if (gBattleMoves[AI_DATA->partnerMove].effect == EFFECT_COUNTER || gBattleMoves[AI_DATA->partnerMove].effect == EFFECT_MIRROR_COAT)
                        break; // These moves need to go last
                    RETURN_SCORE_PLUS(1);
                }
                break;
            case EFFECT_HEAL_PULSE:
            case EFFECT_HIT_ENEMY_HEAL_ALLY:
                if (AI_WhoStrikesFirst(battlerAtk, FOE(battlerAtk), move) == AI_IS_FASTER
                  && AI_WhoStrikesFirst(battlerAtk, BATTLE_PARTNER(FOE(battlerAtk)), move) == AI_IS_FASTER
                  && gBattleMons[battlerAtkPartner].hp < gBattleMons[battlerAtkPartner].maxHP / 2)
                    RETURN_SCORE_PLUS(6);
                break;
            case EFFECT_HELPING_HAND:
                if (HasDamagingMove(battlerAtkPartner))
                    break;
                if (!IS_MOVE_STATUS(AI_DATA->partnerMove) && AI_RandLessThan(128))
                    score += 7;
                break;
            case EFFECT_DRAGON_CHEER:
                if (gBattleMons[battlerAtkPartner].status2 & STATUS2_FOCUS_ENERGY_ANY || !HasDamagingMove(battlerAtkPartner))
                    break;
                score += 6;
                if (IS_BATTLER_OF_TYPE(BATTLE_PARTNER(battlerAtk), TYPE_DRAGON))
                    score += 1;
                break;
            default: 
                score -= 30; // otherwise, don't target partner
            } // attacker move effects
        } // check partner protecting

    }
    else // checking opponent
    {
        // these checks mostly handled in AI_CheckBadMove and AI_CheckViability
        switch (effect)
        {
        case EFFECT_SKILL_SWAP:
            if (AI_DATA->abilities[battlerAtk] == ABILITY_TRUANT)
                score += 5;
            else if (IsAbilityOfRating(AI_DATA->abilities[battlerAtk], 0) || IsAbilityOfRating(AI_DATA->abilities[battlerDef], 10))
                score += 2; // we want to transfer our bad ability or take their awesome ability
            break;
        }

        // lightning rod, flash fire against enemy handled in AI_CheckBadMove
    }

    return score;
}

static bool32 IsPinchBerryItemEffect(u32 holdEffect)
{
    switch (holdEffect)
    {
    case HOLD_EFFECT_ATTACK_UP:
    case HOLD_EFFECT_DEFENSE_UP:
    case HOLD_EFFECT_SPEED_UP:
    case HOLD_EFFECT_SP_ATTACK_UP:
    case HOLD_EFFECT_SP_DEFENSE_UP:
    case HOLD_EFFECT_CRITICAL_UP:
    case HOLD_EFFECT_RANDOM_STAT_UP:
    case HOLD_EFFECT_CUSTAP_BERRY:
    case HOLD_EFFECT_MICLE_BERRY:
        return TRUE;
    }

    return FALSE;
}

static u32 GetAIMostDamagingMoveId(u8 battlerAtk, u8 battlerDef)
{
    u32 i, id = 0;
    u32 mostDmg = 0;

    for (i = 0; i < MAX_MON_MOVES; i++)
    {
        if (AI_DATA->simulatedDmg[battlerAtk][battlerDef][i] > mostDmg)
            id = i, mostDmg = AI_DATA->simulatedDmg[battlerAtk][battlerDef][i];
    }
    return id;
}

// AI_FLAG_CHECK_VIABILITY - a weird mix of increasing and decreasing scores
static s16 AI_CheckViability(u8 battlerAtk, u8 battlerDef, u16 move, s16 score)
{
    // move data
    u16 moveEffect = gBattleMoves[move].effect;
    u16 moveEffectChance = gBattleMoves[move].secondaryEffectChance;
    u32 effectiveness = AI_DATA->effectiveness[battlerAtk][battlerDef][AI_THINKING_STRUCT->movesetIndex];
    s8 atkPriority = GetMovePriority(battlerAtk, move);
    u16 predictedMove = AI_DATA->predictedMoves[battlerDef];
    bool32 isDoubleBattle = IsValidDoubleBattle(battlerAtk);
    u32 i;
    u8 atkHpPercent = GetHealthPercentage(battlerAtk);
    u8 defHpPercent = GetHealthPercentage(battlerDef);
    u8 battlerAtkPartner = BATTLE_PARTNER(battlerAtk);
    // We only check for moves that have a 20% chance or more for their secondary effect to happen because moves with a smaller chance are rather worthless. We don't want the AI to use those.
    bool32 sereneGraceBoost = (AI_DATA->abilities[battlerAtk] == ABILITY_SERENE_GRACE && (gBattleMoves[move].secondaryEffectChance >= 20 && gBattleMoves[move].secondaryEffectChance < 100));

    if (AI_DATA->abilities[battlerAtk] == ABILITY_SERENE_GRACE)
        moveEffectChance = moveEffectChance * 2;
    if (moveEffectChance > 100)
        moveEffectChance = 100;

    // Targeting partner, check benefits of doing that instead
    if (IsTargetingPartner(battlerAtk, battlerDef))
        return score;

    if (gBattleMoves[move].power)
    {
        if (GetNoOfHitsToKOBattler(battlerAtk, battlerDef, AI_THINKING_STRUCT->movesetIndex) == UINT32_MAX)
            score -= 20;
    }

    // check already dead
    if (!IsBattlerIncapacitated(battlerDef, AI_DATA->abilities[battlerDef])
      && CanTargetFaintAi(battlerDef, battlerAtk)
      && !(IsBattlerSashSturdyProtected(battlerAtk))
      && AI_WhosFaster(battlerAtk, battlerDef) == AI_IS_SLOWER
      && gBattleMoves[move].power != 0) // Opponent should go first
    {
        if (atkPriority > 0 
            && !IsMimikyuDisguiseProtected(battlerAtk, battlerDef) 
            && moveEffect != EFFECT_FEINT) 
        {
            if (GetMoveDamageResult(move) == MOVE_POWER_BEST)
                score += 5;
            else
                score += 11;
        }
    }
    else if (!IsBattlerIncapacitated(battlerDef, AI_DATA->abilities[battlerDef])
        && CountUsablePartyMons(battlerAtk) != 0
        && AI_DATA->holdEffects[battlerAtk] == HOLD_EFFECT_EJECT_BUTTON
        && AI_WhosFaster(battlerAtk, battlerDef) == AI_IS_SLOWER
        && gBattleMoves[move].power != 0)
    {
        if (atkPriority > 0)
            score += 11;
    }

    // check thawing moves
    if ((gBattleMons[battlerAtk].status1 & (STATUS1_FREEZE | STATUS1_FROSTBITE)) && gBattleMoves[move].thawsUser)
        score += 12;

    // attacker ability checks
    switch (AI_DATA->abilities[battlerAtk])
    {
    case ABILITY_MOXIE:
    case ABILITY_BEAST_BOOST:
    case ABILITY_CHILLING_NEIGH:
    case ABILITY_GRIM_NEIGH:
    case ABILITY_AS_ONE_ICE_RIDER:
    case ABILITY_AS_ONE_SHADOW_RIDER:
        if (AI_WhosFaster(battlerAtk, battlerDef) == AI_IS_FASTER) // Attacker should go first
        {
            if (CanIndexMoveFaintTarget(battlerAtk, battlerDef, AI_THINKING_STRUCT->movesetIndex, 0))
                score++; // prioritize killing target for stat boost
        }
        break;
    } // ability checks

    // move effect checks
    switch (moveEffect)
    {
    case EFFECT_HIT:
        break;
    case EFFECT_DARK_VOID:
        if (IsDoubleBattle() && AI_RandLessThan(204))
            score += 2;
    case EFFECT_SLEEP:
    case EFFECT_YAWN:
        if (AI_RandLessThan(64))
            IncreaseSleepScore(battlerAtk, battlerDef, move, &score);
        break;
    case EFFECT_EXPLOSION:
    case EFFECT_MEMENTO:
        if (atkHpPercent < 10)
            score += 10;
        else if (atkHpPercent < 33 && AI_RandLessThan(178))
            score += 8;
        else if (atkHpPercent < 66 && AI_RandLessThan(128)) 
            score += 7;
        else if (AI_RandLessThan(13))
            score += 7;
        break;
    case EFFECT_FINAL_GAMBIT:
        if (AI_WhosFaster(battlerAtk, battlerDef) == AI_IS_FASTER)
        {
            if (gBattleMons[battlerAtk].hp >= gBattleMons[battlerDef].hp)
                score += 8;
            else if (CanTargetFaintAi(battlerDef, battlerAtk))
                score += 7;
        }
        else
            score += 6;
        break;
    case EFFECT_ABSORB:
        if (GetMoveDamageResult(move) == MOVE_POWER_BEST || gBattleMons[battlerDef].ability == ABILITY_LIQUID_OOZE)
            break;
        if (AI_DATA->hpPercents[battlerAtk] <= 50 && (GetNoOfHitsToKOBattler(battlerAtk, battlerDef, move) <= 4))
            score += 6;
        if (AI_DATA->items[battlerAtk] == ITEM_BIG_ROOT)
            score += 1;
        break;
    case EFFECT_COPYCAT:
        if (gBattleMoves[predictedMove].copycatBanned)
        {
            break;
        }
        return AI_CheckViability(battlerAtk, battlerDef, predictedMove, score);
        break;
    case EFFECT_MIRROR_MOVE:
        if (gBattleMoves[predictedMove].mirrorMoveBanned)
        {
            break;
        }
        return AI_CheckViability(battlerAtk, battlerDef, predictedMove, score);
        break;
    case EFFECT_ME_FIRST:
        if (gBattleMoves[predictedMove].meFirstBanned)
        {
            break;
        }
         return AI_CheckViability(battlerAtk, battlerDef, predictedMove, score);
         break;
    case EFFECT_SECRET_POWER:
        if (GetMoveDamageResult(move) == MOVE_POWER_BEST
             || AI_DATA->items[battlerDef] == ITEM_COVERT_CLOAK
             || AI_DATA->abilities[battlerDef] == ABILITY_SHIELD_DUST
             || TestSheerForceFlag(battlerAtk, move))
            break;
        if ((gFieldStatuses & STATUS_FIELD_GRASSY_TERRAIN)
            && AI_RandLessThan(64))
            score += 6;
// stat raising effects
    case EFFECT_ATTACK_UP_HIT:
        if (GetMoveDamageResult(move) == MOVE_POWER_BEST
             || moveEffectChance != 100 
             || TestSheerForceFlag(battlerAtk, move))
            break;
        ShouldSetupOffensive(battlerAtk, battlerDef, STAT_ATK, &score, TRUE);  
        break;
    case EFFECT_SPECIAL_ATTACK_UP_HIT:
    case EFFECT_SP_ATTACK_UP_HIT:
        if (GetMoveDamageResult(move) == MOVE_POWER_BEST
             || moveEffectChance != 100 
             || TestSheerForceFlag(battlerAtk, move))
            break;
        ShouldSetupOffensive(battlerAtk, battlerDef, STAT_SPATK, &score, TRUE);
        break;   
    case EFFECT_DEFENSE_UP_HIT:
        if (GetMoveDamageResult(move) == MOVE_POWER_BEST
             || moveEffectChance != 100 
             || TestSheerForceFlag(battlerAtk, move))
            break;
        ShouldSetupDefensive(battlerAtk, battlerDef, STAT_DEF, &score, TRUE);
        break;
    case EFFECT_STUFF_CHEEKS:
        if (ItemId_GetPocket(AI_DATA->items[battlerAtk]) == POCKET_BERRIES)
            ShouldSetupDefensive(battlerAtk, battlerDef, STAT_DEF, &score, FALSE);
        break;
    case EFFECT_TIDY_UP:
        if (gSideStatuses[GetBattlerSide(battlerAtk)] & SIDE_STATUS_HAZARDS_ANY && CountUsablePartyMons(battlerAtk) != 0)
        {
            score += 2;
            break;
        }
        ShouldSetupOffensive(battlerAtk, battlerDef, STAT_ATK, &score, FALSE);
        break;
    case EFFECT_DRAGON_DANCE:
    case EFFECT_SHIFT_GEAR:
    case EFFECT_ATTACK_UP:
    case EFFECT_ATTACK_UP_2:
    case EFFECT_ATTACK_UP_USER_ALLY:
        ShouldSetupOffensive(battlerAtk, battlerDef, STAT_ATK, &score, FALSE);
        break;
    case EFFECT_DEFENSE_UP:
    case EFFECT_DEFENSE_UP_2:
    case EFFECT_DEFENSE_UP_3:
        ShouldSetupDefensive(battlerAtk, battlerDef, STAT_DEF, &score, FALSE);
        break;
    case EFFECT_MULTI_HIT:
        if (move != MOVE_SCALE_SHOT)
            break;
    case EFFECT_SPEED_UP_HIT:
        if (GetMoveDamageResult(move) == MOVE_POWER_BEST
             || moveEffectChance != 100 
             || TestSheerForceFlag(battlerAtk, move))
            break;
        if (HasMoveEffect(battlerDef, EFFECT_HAZE) 
            || HasMoveEffect(battlerDef, EFFECT_FREEZY_FROST)
            || HasMoveEffect(battlerDef, EFFECT_TOPSY_TURVY)
            || HasMoveEffect(battlerDef, EFFECT_CLEAR_SMOG)
            || (HasMoveEffect(battlerDef, EFFECT_ROAR) && CountUsablePartyMons(battlerAtk) != 0)
            || (HasMoveEffect(battlerDef, EFFECT_HIT_SWITCH_TARGET) && CountUsablePartyMons(battlerAtk) != 0))
            break;
        if (AI_WhosFaster(battlerAtk, battlerDef) == AI_IS_SLOWER)
        {
            score += 6;
            if (AI_RandLessThan(204))
                score++;
        }
        else
        {
            score--;
        }
        break;
    case EFFECT_AUTOTOMIZE:
    case EFFECT_SPEED_UP:
    case EFFECT_SPEED_UP_2:
        if (AI_WhosFaster(battlerAtk, battlerDef) == AI_IS_SLOWER && AI_RandLessThan(204))
            score++;
        break;
    case EFFECT_SPECIAL_ATTACK_UP:
    case EFFECT_SPECIAL_ATTACK_UP_2:
    case EFFECT_SPECIAL_ATTACK_UP_3:
        ShouldSetupOffensive(battlerAtk, battlerDef, STAT_SPATK, &score, FALSE);
        break;
    case EFFECT_SPECIAL_DEFENSE_UP:
    case EFFECT_SPECIAL_DEFENSE_UP_2:
        ShouldSetupDefensive(battlerAtk, battlerDef, STAT_SPDEF, &score, FALSE);
        break;
    case EFFECT_ACCURACY_UP:
    case EFFECT_ACCURACY_UP_2:
        if (gBattleMons[battlerAtk].statStages[STAT_ACC] >= 9 && !AI_RandLessThan(50))
            score -= 2;
        else if (AI_DATA->hpPercents[battlerAtk] <= 70)
            score -= 2;
        else
            score++;
        break;
    case EFFECT_EVASION_UP:
    case EFFECT_EVASION_UP_2:
        if (atkHpPercent > 90 && AI_RandLessThan(204))
            score += 1;
        else if (atkHpPercent > 60 && AI_RandLessThan(153))
            score++;
        break;
    case EFFECT_SYRUP_BOMB:
        if (GetMoveDamageResult(move) == MOVE_POWER_BEST)
            break;
        if (AI_WhosFaster(battlerAtk, battlerDef) == AI_IS_SLOWER)
            score += 6;
        break;
// stat lowering effects
    case EFFECT_ATTACK_DOWN_HIT:
        if (GetMoveDamageResult(move) == MOVE_POWER_BEST 
            || moveEffectChance != 100 
            || AI_DATA->items[battlerDef] == ITEM_COVERT_CLOAK
            || AI_DATA->abilities[battlerDef] == ABILITY_SHIELD_DUST
            || TestSheerForceFlag(battlerAtk, move))
            break;
        if (ShouldLowerAttack(battlerAtk, battlerDef, AI_DATA->abilities[battlerDef]))
            score += 6;
        else
            score += 5;
        if (isDoubleBattle && gBattleMoves[move].target == MOVE_TARGET_BOTH)
            score++;
        break;
    case EFFECT_ATTACK_DOWN:
    case EFFECT_ATTACK_DOWN_2:
        if (!ShouldLowerAttack(battlerAtk, battlerDef, AI_DATA->abilities[battlerDef]))
            score -= 2;
        if (gBattleMons[battlerDef].statStages[STAT_ATK] < DEFAULT_STAT_STAGE)
            score -= 2;
        else if (AI_RandLessThan(51))
            score++;
        break;
    case EFFECT_DEFENSE_DOWN_HIT:
        if (GetMoveDamageResult(move) == MOVE_POWER_BEST 
            || moveEffectChance != 100 
            || AI_DATA->items[battlerDef] == ITEM_COVERT_CLOAK
            || AI_DATA->abilities[battlerDef] == ABILITY_SHIELD_DUST
            || TestSheerForceFlag(battlerAtk, move))
            break;
        if (ShouldLowerDefense(battlerAtk, battlerDef, AI_DATA->abilities[battlerDef]))
            score += 6;
        break;
    case EFFECT_DEFENSE_DOWN:
    case EFFECT_DEFENSE_DOWN_2:
        if (!ShouldLowerDefense(battlerAtk, battlerDef, AI_DATA->abilities[battlerDef]))
            score -= 2;
        if (gBattleMons[battlerDef].statStages[STAT_DEF] < DEFAULT_STAT_STAGE)
            score -= 2;
        else if (AI_RandLessThan(51))
            score++;
        break;
    case EFFECT_SPEED_DOWN_HIT:
        if (GetMoveDamageResult(move) == MOVE_POWER_BEST 
            || moveEffectChance != 100 
            || AI_DATA->items[battlerDef] == ITEM_COVERT_CLOAK
            || AI_DATA->abilities[battlerDef] == ABILITY_SHIELD_DUST
            || TestSheerForceFlag(battlerAtk, move))
            break;
        if (ShouldLowerSpeed(battlerAtk, battlerDef, AI_DATA->abilities[battlerDef]) 
            && AI_WhosFaster(battlerAtk, battlerDef) == AI_IS_SLOWER)
            score += 6;
        else
            score += 4;
        if (isDoubleBattle && gBattleMoves[move].target == MOVE_TARGET_BOTH)
            score++;
        break;
    case EFFECT_SPEED_DOWN:
    case EFFECT_SPEED_DOWN_2:
        if (!(gBattleMons[battlerAtk].status1 & (STATUS1_PARALYSIS)))
            score -= 2;
        if (AI_WhosFaster(battlerAtk, battlerDef) == AI_IS_FASTER)
            score -= 2;
        else if (AI_RandLessThan(128))
            score++;
        break;
    case EFFECT_SPECIAL_ATTACK_DOWN_HIT:
        if (GetMoveDamageResult(move) == MOVE_POWER_BEST 
            || moveEffectChance != 100 
            || AI_DATA->items[battlerDef] == ITEM_COVERT_CLOAK
            || AI_DATA->abilities[battlerDef] == ABILITY_SHIELD_DUST
            || TestSheerForceFlag(battlerAtk, move))
            break;
        if (ShouldLowerSpAtk(battlerAtk, battlerDef, AI_DATA->abilities[battlerDef]))
            score += 6;
        else
            score += 5;
        if (isDoubleBattle && gBattleMoves[move].target == MOVE_TARGET_BOTH)
            score++;
        break;
    case EFFECT_SPECIAL_ATTACK_DOWN:
    case EFFECT_SPECIAL_ATTACK_DOWN_2:
        if (!ShouldLowerSpAtk(battlerAtk, battlerDef, AI_DATA->abilities[battlerDef]))
            score -= 2;
        if (gBattleMons[battlerDef].statStages[STAT_SPATK] < DEFAULT_STAT_STAGE)
            score -= 2;
        else if (AI_RandLessThan(51))
            score++;
        break;
    case EFFECT_SPECIAL_DEFENSE_DOWN_HIT:
    case EFFECT_SPECIAL_DEFENSE_DOWN_HIT_2:
        if (GetMoveDamageResult(move) == MOVE_POWER_BEST 
            || moveEffectChance != 100 
            || AI_DATA->items[battlerDef] == ITEM_COVERT_CLOAK
            || AI_DATA->abilities[battlerDef] == ABILITY_SHIELD_DUST
            || TestSheerForceFlag(battlerAtk, move))
            break;
        if (ShouldLowerSpDef(battlerAtk, battlerDef, AI_DATA->abilities[battlerDef]))
            score += 6;
        break;
    case EFFECT_SPECIAL_DEFENSE_DOWN:
    case EFFECT_SPECIAL_DEFENSE_DOWN_2:
        if (gBattleMons[battlerDef].statStages[STAT_SPDEF] < DEFAULT_STAT_STAGE && AI_RandLessThan(204))
            score -= 2;
        else if (AI_RandLessThan(51))
            score++;
        break;
    case EFFECT_ACCURACY_DOWN:
    case EFFECT_ACCURACY_DOWN_2:
        if (!ShouldLowerAccuracy(battlerAtk, battlerDef, AI_DATA->abilities[battlerDef]))
            score -= 3;
        if (gBattleMons[battlerDef].statStages[STAT_ACC] < 4 && AI_RandLessThan(204))
            score -= 2;
        if (atkHpPercent > 90 && AI_RandLessThan(204))
            score += 2;
        else if (atkHpPercent > 60 && AI_RandLessThan(204))
            score++;
        else if (atkHpPercent < 60)
            score--;
        break;
    case EFFECT_EVASION_DOWN:
    case EFFECT_EVASION_DOWN_2:
        break;
	case EFFECT_BIDE:
        if (AI_DATA->hpPercents[battlerAtk] < 90)
            score -= 2;
        break;
    case EFFECT_ACUPRESSURE:
        if (IsBattlerIncapacitated(battlerDef, AI_DATA->abilities[battlerDef]) && AI_RandLessThan(230))
            score += 3;
        else if (!CanAIFaintTarget(battlerDef, battlerAtk, 3) && AI_RandLessThan(204)) //%80
        {
            score += 1;
            if (AI_WhosFaster(battlerAtk, battlerDef) == AI_IS_FASTER)
                score += 1;
        }
        //If Player outspeeds and 2HKO'd AI, AI cannot setup. 
        //AI accounts for Disguise properly
        if (AI_WhosFaster(battlerAtk, battlerDef) == AI_IS_SLOWER
        && CanAIFaintTarget(battlerDef, battlerAtk, 2)
        && !IsMimikyuDisguiseProtected(battlerAtk, battlerDef))
            score -= 5;
        if (AI_WhosFaster(battlerAtk, battlerDef) == AI_IS_SLOWER
            && CanAIFaintTarget(battlerDef, battlerAtk, 1)
            && IsMimikyuDisguiseProtected(battlerAtk, battlerDef))
            score -= 5;
        break;
    case EFFECT_ATTACK_ACCURACY_UP: // hone claws
        ShouldSetupOffensive(battlerAtk, battlerDef, STAT_ATK, &score, FALSE);
        break;
    case EFFECT_GROWTH:
    case EFFECT_ATTACK_SPATK_UP:    // work up
        ShouldSetupOffensive(battlerAtk, battlerDef, STAT_SPATK, &score, FALSE);
        break;
    case EFFECT_HAZE:
        if (AnyStatIsRaised(BATTLE_PARTNER(battlerAtk))
          || PartnerHasSameMoveEffectWithoutTarget(BATTLE_PARTNER(battlerAtk), move, AI_DATA->partnerMove))
            score -= 3;
            break;
    case EFFECT_ROAR:
        if (gSideTimers[GetBattlerSide(battlerDef)].spikesAmount > 2)
            score++;
        if (gSideTimers[GetBattlerSide(battlerDef)].spikesAmount > 1 && AI_RandLessThan(192))
            score++;
        if (gSideTimers[GetBattlerSide(battlerDef)].spikesAmount > 0 && AI_RandLessThan(128))
            score++;            
        if (gSideTimers[GetBattlerSide(battlerDef)].stealthRockAmount > 0 && AI_RandLessThan(128))
            score += 2;
        if (AI_RandLessThan(128))
            score--;
        break;
    case EFFECT_CLEAR_SMOG:
        if (isDoubleBattle)
            score += min(CountPositiveStatStages(battlerDef) + CountPositiveStatStages(BATTLE_PARTNER(battlerDef)), 7);
        else
            score += min(CountPositiveStatStages(battlerDef), 4);
        break;
    case EFFECT_CONVERSION:
        if (!IS_BATTLER_OF_TYPE(battlerAtk, gBattleMoves[gBattleMons[battlerAtk].moves[0]].type))
            score += 2;
        break;
    case EFFECT_CONVERSION_2:
        if (CanTargetFaintAi(battlerDef, battlerAtk))
            score += 1;
        break;
    case EFFECT_SWALLOW:
        if (gDisableStructs[battlerAtk].stockpileCounter == 0)
        {
            break;
        }
        else
        {
            u32 healPercent = 0;
            switch (gDisableStructs[battlerAtk].stockpileCounter)
            {
            case 1:
                healPercent = 25;
                break;
            case 2:
                healPercent = 50;
                break;
            case 3:
                healPercent = 100;
                break;
            default:
                break;
            }

            if (ShouldRecover(battlerAtk, battlerDef, move, healPercent))
                score += 2;
        }
        break;
    case EFFECT_SHORE_UP:
        if (AI_WeatherHasEffect() && (gBattleWeather & B_WEATHER_SANDSTORM)
        && ShouldRecover(battlerAtk, battlerDef, move, 67))
        {
            score++;
        }
        else if (ShouldRecover(battlerAtk, battlerDef, move, 50))
            score++;
        else
            score--;
        break;
    case EFFECT_MORNING_SUN:
    case EFFECT_SYNTHESIS:
    case EFFECT_MOONLIGHT:
        if (AI_WeatherHasEffect() && (gBattleWeather & B_WEATHER_SUN)
        && ShouldRecover(battlerAtk, battlerDef, move, 67))
        {
            score++;
            break;
        }
    case EFFECT_RESTORE_HP:
    case EFFECT_SOFTBOILED:
    case EFFECT_ROOST:
    case EFFECT_STRENGTH_SAP:
        if (ShouldRecover(battlerAtk, battlerDef, move, 50))
            score++;
        else
            score--;
        break;
    case EFFECT_TOXIC:
    case EFFECT_POISON:
    case EFFECT_MORTAL_SPIN:
        if (AI_RandLessThan(96))
            IncreasePoisonScore(battlerAtk, battlerDef, move, &score);
        break;
    case EFFECT_LIGHT_SCREEN:
    case EFFECT_REFLECT:
    case EFFECT_AURORA_VEIL:
        if (ShouldSetScreen(battlerAtk, battlerDef, moveEffect))
        {
            if (AI_RandLessThan(128))
                score++;
            if (AI_DATA->holdEffects[battlerAtk] == HOLD_EFFECT_LIGHT_CLAY)
                score++;
        }
        break;
    case EFFECT_REST:
        if (!(AI_CanSleep(battlerAtk, AI_DATA->abilities[battlerAtk])))
        {
            break;
        }
        else if (ShouldRecover(battlerAtk, battlerDef, move, 100))
        {
            if (AI_DATA->holdEffects[battlerAtk] == HOLD_EFFECT_CURE_SLP
              || AI_DATA->holdEffects[battlerAtk] == HOLD_EFFECT_CURE_STATUS
              || HasMoveEffect(EFFECT_SLEEP_TALK, battlerAtk)
              || HasMoveEffect(EFFECT_SNORE, battlerAtk)
              || AI_DATA->abilities[battlerAtk] == ABILITY_SHED_SKIN
              || AI_DATA->abilities[battlerAtk] == ABILITY_EARLY_BIRD
              || (gBattleWeather & B_WEATHER_RAIN && gWishFutureKnock.weatherDuration != 1 && AI_DATA->abilities[battlerAtk] == ABILITY_HYDRATION && AI_DATA->holdEffects[battlerAtk] != HOLD_EFFECT_UTILITY_UMBRELLA))
            {
                score += 2;
            }
            else
            {
                score++;
            }
        }
        break;
    case EFFECT_OHKO:
        if (gStatuses3[battlerAtk] & STATUS3_ALWAYS_HITS)
            score += 8;
        break;
    case EFFECT_SALT_CURE:
        if (GetMoveDamageResult(move) == MOVE_POWER_BEST)
            break;
        if (AI_RandLessThan(204))
            score += 6;
        else
            score += 7;
        if (IS_BATTLER_OF_TYPE(battlerDef, TYPE_WATER) || IS_BATTLER_OF_TYPE(battlerDef, TYPE_STEEL))
            score += 1;
        break;
    case EFFECT_TRAP:
        if (GetMoveDamageResult(move) == MOVE_POWER_BEST)
            break;
        if (AI_RandLessThan(204))
            score += 6;
        else
            score += 8;
        if (AI_DATA->items[battlerAtk] == ITEM_BINDING_BAND || AI_DATA->items[battlerAtk] == ITEM_GRIP_CLAW)
            score += 1;
        break;
    case EFFECT_OCTOLOCK:
    case EFFECT_MEAN_LOOK:
        score -= 6;
        if (AI_RandLessThan(204))
            score += 6;
        else
            score += 8;
        break;
    case EFFECT_HIT_PREVENT_ESCAPE:
    case EFFECT_JAW_LOCK:
        if (GetMoveDamageResult(move) == MOVE_POWER_BEST 
        || moveEffectChance != 100 
        || !IsBattlerTrapped(battlerDef, TRUE))
        break;
        score += 6;
        break;
    case EFFECT_LASER_FOCUS:
        if (AI_DATA->abilities[battlerAtk] == ABILITY_SNIPER)
            score++;
        break;
    case EFFECT_FOCUS_ENERGY:
        if (AI_DATA->abilities[battlerAtk] == ABILITY_SUPER_LUCK
          || AI_DATA->abilities[battlerAtk] == ABILITY_SNIPER
          || AI_DATA->holdEffects[battlerAtk] == HOLD_EFFECT_SCOPE_LENS
          || HasHighCritRatioMove(battlerAtk))
            score++;
        break;
    case EFFECT_CONFUSE:
        IncreaseConfusionScore(battlerAtk, battlerDef, move, &score);
        break;
    case EFFECT_PARALYZE:
        IncreaseParalyzeScore(battlerAtk, battlerDef, move, &score);
        break;
    case EFFECT_PARALYZE_HIT:
        if (GetMoveDamageResult(move) == MOVE_POWER_BEST 
            || moveEffectChance != 100 
            || AI_DATA->items[battlerDef] == ITEM_COVERT_CLOAK
            || AI_DATA->abilities[battlerDef] == ABILITY_SHIELD_DUST
            || TestSheerForceFlag(battlerAtk, move)
            || move == MOVE_ZAP_CANNON)
            break;
        if (AI_CanParalyze(battlerAtk, battlerDef, AI_DATA->holdEffects[battlerDef], move, AI_DATA->partnerMove))
            score += 6;
        IncreaseParalyzeScore(battlerAtk, battlerDef, move, &score);
        if (AI_RandLessThan(128))
            score--;
        break;
    case EFFECT_SHED_TAIL:
        if (AI_RandLessThan(128) 
            && CanTargetFaintAi(battlerDef, battlerAtk)
            && AI_WhosFaster(battlerAtk, battlerDef) == AI_IS_FASTER)
            score += 1;
        else if (AI_RandLessThan(64))
            score += 1;
        break;
    case EFFECT_SUBSTITUTE:
        if (gStatuses3[battlerDef] & STATUS3_PERISH_SONG)
            score++;
        if (gBattleMons[battlerDef].status1 & (STATUS1_BURN | STATUS1_PSN_ANY | STATUS1_FROSTBITE) && AI_RandLessThan(64))
            score++;
        if (!gBattleMons[battlerDef].status2 & (STATUS2_WRAPPED | STATUS2_ESCAPE_PREVENTION && AI_DATA->hpPercents[battlerAtk] > 70) && AI_RandLessThan(64))
            score++;
        if (AI_DATA->abilities[battlerAtk] == ABILITY_SPEED_BOOST && AI_RandLessThan(64))
            score++;
        if (AI_RandLessThan(128))
            score--;
        break;
    case EFFECT_MIMIC:
        if (AI_WhosFaster(battlerAtk, battlerDef) == AI_IS_FASTER)
        {
            if (gLastMoves[battlerDef] != MOVE_NONE && gLastMoves[battlerDef] != 0xFFFF)
                return AI_CheckViability(battlerAtk, battlerDef, gLastMoves[battlerDef], score);
        }
        break;
    case EFFECT_LEECH_SEED:
        if (IS_BATTLER_OF_TYPE(battlerDef, TYPE_GRASS)
          || gStatuses3[battlerDef] & STATUS3_LEECHSEED
          || HasMoveEffect(battlerDef, EFFECT_RAPID_SPIN)
          || AI_DATA->abilities[battlerDef] == ABILITY_LIQUID_OOZE
          || AI_DATA->abilities[battlerDef] == ABILITY_MAGIC_GUARD)
            break;
        if (AI_RandLessThan(128))
            score++;
        if (GetNoOfHitsToKOBattlerDmg(GetHighestDmg(battlerAtk, battlerDef), battlerDef) > 3 && AI_RandLessThan(128))
            score++;
        break;
    case EFFECT_DO_NOTHING:
        score -= 10;
        break;
    case EFFECT_TELEPORT:
        if (!(gBattleTypeFlags & BATTLE_TYPE_TRAINER) || GetBattlerSide(battlerAtk) != B_SIDE_PLAYER)
            break;
        if (GetNoOfHitsToKOBattlerDmg(GetHighestDmg(battlerAtk, battlerDef), battlerDef) > 3 && AI_RandLessThan(128))
            score++;
        break;
    case EFFECT_HIT_ESCAPE:
        if (CountUsablePartyMons(battlerAtk) == 0 || GetMoveDamageResult(move) == MOVE_POWER_BEST)
            break;
        if (gBattleMons[battlerAtk].species == SPECIES_PALAFIN)
        {
            score += 12;
            break;
        }
        if (GetNoOfHitsToKOBattlerDmg(GetHighestDmg(battlerAtk, battlerDef), battlerDef) > 3)
            score += 6;
        if (HasMoveEffect(battlerDef, EFFECT_ROAR) || HasMoveEffect(battlerDef, EFFECT_HIT_SWITCH_TARGET))
            score--;
        break;
    case EFFECT_PARTING_SHOT:
        if (gBattleMons[battlerAtk].species == SPECIES_PALAFIN && CountUsablePartyMons(battlerAtk) > 0)
            score += 12;
        if (HasMoveEffect(battlerDef, EFFECT_ROAR) || HasMoveEffect(battlerDef, EFFECT_HIT_SWITCH_TARGET))
            score--;
        break;
    case EFFECT_METEOR_BEAM:
        if (AI_DATA->holdEffects[battlerAtk] == HOLD_EFFECT_POWER_HERB 
            || (move == MOVE_ELECTRO_SHOT && (gBattleWeather & B_WEATHER_RAIN)))
            score += 9;
        break;
    case EFFECT_TWO_TURNS_ATTACK:
        if (AI_DATA->holdEffects[battlerAtk] == HOLD_EFFECT_POWER_HERB && AI_RandLessThan(204))
            score += 9;
        break;
    case EFFECT_BATON_PASS:
        if (AI_RandLessThan(64))
            score--;
        if (gBattleMons[battlerAtk].status2 & STATUS2_SUBSTITUTE
          || (gStatuses3[battlerAtk] & (STATUS3_ROOTED | STATUS3_AQUA_RING | STATUS3_MAGNET_RISE | STATUS3_POWER_TRICK))
          || AnyStatIsRaised(battlerAtk))
            score += 1;
        break;
    case EFFECT_DISABLE:
        if (gDisableStructs[battlerDef].disableTimer == 0)
        {
            if (AI_WhosFaster(battlerAtk, battlerDef) == AI_IS_FASTER) // AI goes first
            {
                if (gLastMoves[battlerDef] != MOVE_NONE
                  && gLastMoves[battlerDef] != 0xFFFF)
                {
                    if (CanMoveFaintBattler(gLastMoves[battlerDef], battlerDef, battlerAtk, 1))
                        score++; //Disable move that can kill attacker
                }
            }
            else
                score -= 2;
        }
        else
            score -= 2;
        break;
    case EFFECT_ENCORE:
        if (gDisableStructs[battlerDef].encoreTimer == 0
            && AI_WhosFaster(battlerAtk, battlerDef) == AI_IS_FASTER
        )
        {
            if (IsEncoreEncouragedEffect(gLastMoves[battlerDef]))
                score += 2;
        }
        else
            if (AI_RandLessThan(128))
                score--;
        break;
    case EFFECT_PAIN_SPLIT:
        {
            u16 newHp = (gBattleMons[battlerAtk].hp + gBattleMons[battlerDef].hp) / 2;
            u16 healthBenchmark = ((gBattleMons[battlerAtk].maxHP * 3) / 10) + gBattleMons[battlerAtk].hp;
            if (newHp > healthBenchmark && ShouldRecover(battlerAtk, battlerDef, move, 30))
                score += 2;
            else
                score--;
        }
        break;
    case EFFECT_SLEEP_TALK:
    case EFFECT_SNORE:
        if (!IsWakeupTurn(battlerAtk) && gBattleMons[battlerAtk].status1 & STATUS1_SLEEP)
            score += 15;
        break;
    case EFFECT_LOCK_ON:
        if (HasMoveEffect(battlerAtk, EFFECT_OHKO))
            score += 1;
        break;
    case EFFECT_DESTINY_BOND:
        if (AI_WhosFaster(battlerAtk, battlerDef) == AI_IS_FASTER)
        {
            if (CanTargetFaintAi(battlerDef, battlerAtk) && AI_RandLessThan(208))
                score++;
        }
        else if (AI_RandLessThan(128))
            score--;
        break;
    case EFFECT_SPITE:
        break;
    case EFFECT_WISH:
    case EFFECT_HEAL_BELL:
        if (ShouldUseWishAromatherapy(battlerAtk, battlerDef, move))
            score++;
        else
            score--;
        break;
    case EFFECT_NIGHTMARE:
        if (AI_DATA->abilities[battlerDef] != ABILITY_MAGIC_GUARD
          && !(gBattleMons[battlerDef].status2 & STATUS2_NIGHTMARE)
          && AI_IsBattlerAsleepOrComatose(battlerDef))
        {
            score++;
            if (IsBattlerTrapped(battlerDef, TRUE))
                score++;
        }
        break;
    case EFFECT_PROTECT:
        if (move == MOVE_KINGS_SHIELD) {
            if (!IsBattlerIncapacitated(battlerDef, AI_DATA->abilities[battlerDef])
            && CanTargetFaintAi(battlerDef, battlerAtk)
            && !(IsBattlerSashSturdyProtected(battlerAtk))
            && AI_WhosFaster(battlerAtk, battlerDef) == AI_IS_SLOWER
            && AI_RandLessThan(204)) {
                score += 10;            
            }
            if (HasMoveWithSplit(battlerDef, SPLIT_PHYSICAL) && AI_RandLessThan(192))
                score += 1;
            else if (AI_RandLessThan(96))
                score += 1;
            break;
        }
        if (AI_DATA->abilities[battlerAtk] == ABILITY_POWER_CONSTRUCT 
            && gBattleMons[battlerAtk].species == SPECIES_ZYGARDE_50_POWER_CONSTRUCT
            && AI_RandLessThan(204))
            score += 8;
        if (AI_DATA->abilities[battlerAtk] == ABILITY_ZEN_MODE 
            && (gBattleMons[battlerAtk].species == SPECIES_DARMANITAN || gBattleMons[battlerAtk].species == SPECIES_DARMANITAN_GALARIAN)
            && AI_RandLessThan(204))
            score += 8;
        if (gBattleMons[battlerAtk].status1 & (STATUS1_PSN_ANY | STATUS1_BURN)
        || gBattleMons[battlerAtk].status2 & (STATUS2_CURSED | STATUS2_INFATUATION)
        || gStatuses3[battlerAtk] & (STATUS3_PERISH_SONG | STATUS3_LEECHSEED | STATUS3_YAWN))
            score -= 1;
        if (gBattleMons[battlerDef].status1 & (STATUS1_PSN_ANY | STATUS1_BURN)
        || gBattleMons[battlerDef].status2 & STATUS2_CURSED
        || gStatuses3[battlerDef] & (STATUS3_PERISH_SONG | STATUS3_LEECHSEED | STATUS3_YAWN))
            score += 1;
        if (gDisableStructs[battlerAtk].isFirstTurn 
            && (AI_DATA->abilities[battlerAtk] == ABILITY_SPEED_BOOST)
            && AI_WhosFaster(battlerAtk, battlerDef) == AI_IS_SLOWER
            && AI_RandLessThan(204))
            score += 3;
        else if (gDisableStructs[battlerAtk].isFirstTurn 
            && !isDoubleBattle 
            && (AI_DATA->abilities[battlerAtk] != ABILITY_GUTS && AI_DATA->abilities[battlerAtk] != ABILITY_QUICK_FEET && AI_DATA->abilities[battlerAtk] != ABILITY_FLARE_BOOST))
            score--;
        break;
    case EFFECT_ENDURE:
        if (gDisableStructs[battlerAtk].protectUses == 1 && Random() % 100 < 50)
        {
            score -= 10;
        }
        else if (gDisableStructs[battlerAtk].protectUses >= 2)
        {
            score -= 10;
        }

        if (CanTargetFaintAi(battlerDef, battlerAtk))
        {
            if (gBattleMons[battlerAtk].hp > gBattleMons[battlerAtk].maxHP / 4 // Pinch berry couldn't have activated yet
              && IsPinchBerryItemEffect(AI_DATA->holdEffects[battlerAtk]))
            {
                score += 2;
            }
            else if (gBattleMons[battlerAtk].hp > 1) // Only spam endure for Flail/Reversal if you're not at Min Health
            {
                if (HasMoveEffect(battlerAtk, EFFECT_FLAIL) || HasMoveEffect(battlerAtk, EFFECT_ENDEAVOR) || HasMoveEffect(battlerAtk, EFFECT_RAGE_FIST))
                    score += 2;
            }
            else if (AI_DATA->abilities[battlerAtk] == ABILITY_SPEED_BOOST)
            {
                score++;
                if (HasMoveEffect(battlerAtk, EFFECT_BATON_PASS))
                    score++;
            }
            else if (AI_RandLessThan(128))
                score--;
        }
        else if (AI_RandLessThan(128))
            score--;
        break;
    case EFFECT_HIT_SET_ENTRY_HAZARD:
        if (GetMoveDamageResult(move) == MOVE_POWER_BEST)
            break;
        score += 6;
        if (CountUsablePartyMons(battlerDef) == 0)
        {
            score -= 10;
            break;
        }
        if (gDisableStructs[battlerAtk].isFirstTurn && CountAllAlivePartyMons(battlerAtk) > 1 && AI_RandLessThan(250))
            score += 2;
        if (AI_RandLessThan(192 * CountAllAlivePartyMons(battlerAtk) / CountAllPartyMons(battlerAtk)))
            score++;
    case EFFECT_SPIKES:
        if (CountUsablePartyMons(battlerDef) == 0)
        {
            score -= 10;
            break;
        }
        if (gDisableStructs[battlerAtk].isFirstTurn && CountAllAlivePartyMons(battlerAtk) > 1 && AI_RandLessThan(250))
            score += 2;
        if (gSideTimers[GetBattlerSide(battlerDef)].spikesAmount >= 1 && AI_RandLessThan(250))
            score--;
        else if (AI_RandLessThan(192 * CountAllAlivePartyMons(battlerAtk) / CountAllPartyMons(battlerAtk)))
            score++;
        break;  
    case EFFECT_STEALTH_ROCK:
        if (CountUsablePartyMons(battlerDef) == 0)
        {
            score -= 10;
            break;
        }
        if (gDisableStructs[battlerAtk].isFirstTurn && CountAllAlivePartyMons(battlerAtk) > 1 && AI_RandLessThan(250))
            score += 2;
        if (AI_RandLessThan(192 * CountAllAlivePartyMons(battlerAtk) / CountAllPartyMons(battlerAtk)))
            score++;
        break;
    case EFFECT_STICKY_WEB:
        if (CountUsablePartyMons(battlerDef) == 0)
        {
            score -= 10;
            break;
        }
        if (gDisableStructs[battlerAtk].isFirstTurn)
            score += 3;
        if (AI_RandLessThan(192 * CountAllAlivePartyMons(battlerAtk) / CountAllPartyMons(battlerAtk)))
            score++;
        break; 
    case EFFECT_TOXIC_SPIKES:
        if (CountUsablePartyMons(battlerDef) == 0)
        {
            score -= 10;
            break;
        }
        if (gDisableStructs[battlerAtk].isFirstTurn && CountAllAlivePartyMons(battlerAtk) != 0 && AI_RandLessThan(250))
            score += 2;
        if (gSideTimers[GetBattlerSide(battlerDef)].toxicSpikesAmount >= 1 && AI_RandLessThan(250))
            score--;
        else if (AI_RandLessThan(192 * CountAllAlivePartyMons(battlerAtk) / CountAllPartyMons(battlerAtk)))
            score++;
        break;
    case EFFECT_PERISH_SONG:
        if (IsBattlerTrapped(battlerDef, TRUE))
            score += 2;
        break;
    case EFFECT_SANDSTORM:
        if (ShouldSetSandstorm(battlerAtk, AI_DATA->abilities[battlerAtk], AI_DATA->holdEffects[battlerAtk]))
        {
            score += 2;
            if (AI_DATA->holdEffects[battlerAtk] == HOLD_EFFECT_SMOOTH_ROCK && AI_RandLessThan(204))
                score++;           
        }
        break;
    case EFFECT_HAIL:
        if (ShouldSetHail(battlerAtk, AI_DATA->abilities[battlerAtk], AI_DATA->holdEffects[battlerAtk]))
        {
            score += 2;
            if (AI_DATA->holdEffects[battlerAtk] == HOLD_EFFECT_ICY_ROCK && AI_RandLessThan(204))
                score++;
        }
        break;
    case EFFECT_RAIN_DANCE:
        if (ShouldSetRain(battlerAtk, AI_DATA->abilities[battlerAtk], AI_DATA->holdEffects[battlerAtk]))
        {
            score += 2;
            if (AI_DATA->holdEffects[battlerAtk] == HOLD_EFFECT_DAMP_ROCK && AI_RandLessThan(204))
                score++;
        }
        break;
    case EFFECT_SUNNY_DAY:
        if (ShouldSetSun(battlerAtk, AI_DATA->abilities[battlerAtk], AI_DATA->holdEffects[battlerAtk]))
        {
            score += 2;
            if (AI_DATA->holdEffects[battlerAtk] == HOLD_EFFECT_HEAT_ROCK && AI_RandLessThan(204))
                score++;
        }
        break;
    case EFFECT_SNOWSCAPE:
        if (ShouldSetSnow(battlerAtk, AI_DATA->abilities[battlerAtk], AI_DATA->holdEffects[battlerAtk]))
        {
            score += 2;
            if (AI_DATA->holdEffects[battlerAtk] == HOLD_EFFECT_ICY_ROCK && AI_RandLessThan(204))
                score++;
        }
        break;
    case EFFECT_KNOCK_OFF:
        if (GetMoveDamageResult(move) == MOVE_POWER_BEST)
            break;
        if (AI_DATA->items[battlerDef] == ITEM_NONE || !CanBattlerGetOrLoseItem(battlerDef, AI_DATA->items[battlerDef]))
            break;
        if (GetNoOfHitsToKOBattlerDmg(GetHighestDmg(battlerAtk, battlerDef), battlerDef) > 3)
            score += 6;
        break;
    case EFFECT_FELL_STINGER:
        if (gBattleMons[battlerAtk].statStages[STAT_ATK] < MAX_STAT_STAGE
          && AI_DATA->abilities[battlerAtk] != ABILITY_CONTRARY
          && CanIndexMoveFaintTarget(battlerAtk, battlerDef, AI_THINKING_STRUCT->movesetIndex, 0))
        {
            if (AI_WhosFaster(battlerAtk, battlerDef) == AI_IS_FASTER) // Attacker goes first
                score += 9;
            else
                score += 6;
        }
        break;
    case EFFECT_CLANGOROUS_SOUL:
        if (IsBattlerIncapacitated(battlerDef, AI_DATA->abilities[battlerDef]) && AI_RandLessThan(230))
            score += 3;
        else if (!CanFaintTargetAfterDrum(battlerAtk, battlerDef, MOVE_CLANGOROUS_SOUL))
            score += 2;
        else
            score -= 2;
        break;
    case EFFECT_BELLY_DRUM:
    case EFFECT_FILLET_AWAY:
        if (IsBattlerIncapacitated(battlerDef, AI_DATA->abilities[battlerDef]) && AI_RandLessThan(230))
            score += 3;
        else if (!CanFaintTargetAfterDrum(battlerAtk, battlerDef, MOVE_BELLY_DRUM))
            score += 2;
        else
            score -= 2;
        break;
    case EFFECT_SPECTRAL_THIEF:
        if (GetMoveDamageResult(move) == MOVE_POWER_BEST)
            break;
        score += 5;
        //fallthrough
    case EFFECT_PSYCH_UP:
        // Want to copy positive stat changes
        for (i = STAT_ATK; i < NUM_BATTLE_STATS; i++)
        {
            if (gBattleMons[battlerDef].statStages[i] > gBattleMons[battlerAtk].statStages[i])
            {
                switch (i)
                {
                case STAT_ATK:
                    if (HasMoveWithSplit(battlerAtk, SPLIT_PHYSICAL))
                        score++;
                    break;
                case STAT_SPATK:
                    if (HasMoveWithSplit(battlerAtk, SPLIT_SPECIAL))
                        score++;
                    break;
                case STAT_ACC:
                case STAT_EVASION:
                case STAT_SPEED:
                case STAT_DEF:
                case STAT_SPDEF:
                    score++;
                    break;
                }
            }
        }
        break;
    case EFFECT_DEFENSE_CURL:
        if (HasMoveEffect(battlerAtk, EFFECT_ROLLOUT) && !(gBattleMons[battlerAtk].status2 & STATUS2_DEFENSE_CURL))
            score++;
        ShouldSetupDefensive(battlerAtk, battlerDef, STAT_DEF, &score, FALSE);
        break;
    case EFFECT_FAKE_OUT:
        if (move == MOVE_FAKE_OUT)   // filter out first impression
            score += 9;
        break;
    case EFFECT_FEINT:
        if (isDoubleBattle 
            && HasMoveEffect(battlerDef, EFFECT_PROTECT) 
            && !HasMove(battlerDef, MOVE_WIDE_GUARD)
            && !HasMove(battlerDef, MOVE_QUICK_GUARD)
            && AI_RandLessThan(26)) // ~10.1%
            score += 13;
        else if (CanIndexMoveFaintTarget(battlerAtk, battlerDef, AI_THINKING_STRUCT->movesetIndex, 0) && AI_RandLessThan(26))
            score += 6;
        break;
    case EFFECT_ALLY_SWITCH:
        if (isDoubleBattle 
            && CanTargetFaintAi(battlerDef, BATTLE_PARTNER(battlerAtk))
            && AI_RandLessThan(51)) // 20%
            {
                if (AI_WhosFaster(BATTLE_PARTNER(battlerAtk), battlerDef) == AI_IS_SLOWER)
                    score += 2;
                else 
                    score += 1;
            }
        break;
    case EFFECT_STOCKPILE:
        if (AI_DATA->abilities[battlerAtk] == ABILITY_CONTRARY)
            break;
        if (HasMoveEffect(battlerAtk, EFFECT_SWALLOW)
          || HasMoveEffect(battlerAtk, EFFECT_SPIT_UP))
            score += 1;
        ShouldSetupDefensive(battlerAtk, battlerDef, STAT_HP, &score, FALSE);
        break;
    case EFFECT_SPIT_UP:
        if (gDisableStructs[battlerAtk].stockpileCounter >= 2)
            score++;
        break;
    case EFFECT_ROLLOUT:
        if (GetMoveDamageResult(move) == MOVE_POWER_BEST)
            break;
        score += 6;
        if (AI_RandLessThan(128))
            score++;
        if (gBattleMons[battlerAtk].status2 & STATUS2_DEFENSE_CURL)
            score += 1;
        break;
    case EFFECT_FURY_CUTTER:
    case EFFECT_ECHOED_VOICE:
        if (GetMoveDamageResult(move) == MOVE_POWER_BEST)
            break;
        score += 6;
        //Technically this following check isn't ideal if the Mon has both Fury Cutter and Echoed Voice
        //But for the purpose of my game this is fine
        if (gLastMoves[battlerAtk] == MOVE_FURY_CUTTER || gLastMoves[battlerAtk] == MOVE_ECHOED_VOICE)
            score += 2;
        break;
    case EFFECT_SWAGGER:
        if (HasMoveEffect(battlerAtk, EFFECT_FOUL_PLAY)
          || HasMoveEffect(battlerAtk, EFFECT_PSYCH_UP)
          || HasMoveEffect(battlerAtk, EFFECT_SPECTRAL_THIEF)
          || AI_DATA->holdEffects[battlerAtk] == HOLD_EFFECT_MIRROR_HERB)
            score++;
        IncreaseConfusionScore(battlerAtk, battlerDef, move, &score);
        break;
    case EFFECT_FLATTER:
        if (HasMoveEffect(battlerAtk, EFFECT_PSYCH_UP)
          || HasMoveEffect(battlerAtk, EFFECT_SPECTRAL_THIEF)
          || AI_DATA->holdEffects[battlerAtk] == HOLD_EFFECT_MIRROR_HERB)
            score++;
        IncreaseConfusionScore(battlerAtk, battlerDef, move, &score);
        break;
    case EFFECT_ATTRACT:
        if (!isDoubleBattle && BattlerWillFaintFromSecondaryDamage(battlerDef, AI_DATA->abilities[battlerDef])
          && AI_WhosFaster(battlerAtk, battlerDef) == AI_IS_SLOWER) // Target goes first
            break; // Don't use if the attract won't have a change to activate

        if (gBattleMons[battlerDef].status1 & STATUS1_ANY
          || (gBattleMons[battlerDef].status2 & STATUS2_CONFUSION)
          || IsBattlerTrapped(battlerDef, TRUE))
            score++;
        break;
    case EFFECT_SAFEGUARD:
        if (!AI_IsTerrainAffected(battlerAtk, STATUS_FIELD_MISTY_TERRAIN) || !IsBattlerGrounded(battlerAtk))
            score++;
        break;
    case EFFECT_PURSUIT:
        if ((AI_DATA->abilities[battlerAtk] == ABILITY_TRUANT || AI_DATA->abilities[battlerAtk] == ABILITY_DEFEATIST) 
            && (AI_DATA->abilities[battlerDef] == ABILITY_WANDERING_SPIRIT || AI_DATA->abilities[battlerDef] == ABILITY_MUMMY || AI_DATA->abilities[battlerDef] == ABILITY_LINGERING_AROMA))
            {
                score += 12;
                break;
            }
        if (CanIndexMoveFaintTarget(battlerAtk, battlerDef, AI_THINKING_STRUCT->movesetIndex, 0))
        {
            score += 11;
        }
        else if ((defHpPercent < 20))
            score += 10;
        else if ((defHpPercent < 40) && AI_RandLessThan(128))
            score += 8;
        if (AI_WhosFaster(battlerAtk, battlerDef) == AI_IS_FASTER)
            score += 3;
        break;
    case EFFECT_RAPID_SPIN:
        if (GetMoveDamageResult(move) == MOVE_POWER_BEST)
            break;
        score += 6;
        if (AI_WhosFaster(battlerAtk, battlerDef) == AI_IS_SLOWER && AI_RandLessThan(204))
            score += 1;   // Gen 8 increases speed
        if (gStatuses3[battlerAtk] & STATUS3_LEECHSEED || gBattleMons[battlerAtk].status2 & STATUS2_WRAPPED)
            score += 1;
        //fallthrough
    case EFFECT_DEFOG:
        if (gSideStatuses[GetBattlerSide(battlerAtk)] & SIDE_STATUS_HAZARDS_ANY 
            && CountUsablePartyMons(battlerAtk) != 0
            && AI_RandLessThan(128))
            score += 2;
        break;
    case EFFECT_TORMENT:
        break;
    case EFFECT_FREEZY_FROST:
        if (GetMoveDamageResult(move) == MOVE_POWER_BEST)
            break;
        if (AI_RandLessThan(96))
            IncreaseFrostbiteScore(battlerAtk, battlerDef, move, &score);
        break;
    case EFFECT_BURN_HIT:
        if (GetMoveDamageResult(move) == MOVE_POWER_BEST 
            || moveEffectChance != 100 
            || AI_DATA->items[battlerDef] == ITEM_COVERT_CLOAK
            || AI_DATA->abilities[battlerDef] == ABILITY_SHIELD_DUST
            || TestSheerForceFlag(battlerAtk, move)
            || move == MOVE_BURNING_JEALOUSY || move == MOVE_INFERNO)
            break;
        if (AI_CanBurn(battlerAtk, battlerDef, AI_DATA->abilities[battlerDef], BATTLE_PARTNER(battlerAtk), move, AI_DATA->partnerMove))
            score += 6;
        //fallthrough
    case EFFECT_WILL_O_WISP:
        if (AI_RandLessThan(96))
            IncreaseBurnScore(battlerAtk, battlerDef, move, &score);
        break;
    case EFFECT_FOLLOW_ME:
        if (CanTargetFaintAi(battlerDef, BATTLE_PARTNER(battlerAtk)) && AI_RandLessThan(128))
            score += 1;
        break;
    case EFFECT_NATURE_POWER:
        return AI_CheckViability(battlerAtk, battlerDef, GetNaturePowerMove(), score);
    case EFFECT_CHARGE:
        if (HasMoveWithType(battlerDef, TYPE_ELECTRIC) && AI_RandLessThan(128))
            score += 1;
        break;
    case EFFECT_TAUNT:
        if (HasMoveWithSplit(battlerDef, SPLIT_STATUS) && (AI_RandLessThan(128)))
            score += 1;
        else if (AI_RandLessThan(128))
            score -= 1;
        break;
    case EFFECT_TRICK:
    case EFFECT_BESTOW:
        break;
    case EFFECT_ROLE_PLAY:
    case EFFECT_DOODLE:
        if (!IsRolePlayDoodleBannedAbilityAttacker(AI_DATA->abilities[battlerAtk])
          && !IsRolePlayDoodleBannedAbility(AI_DATA->abilities[battlerDef])
          && !IsAbilityOfRating(AI_DATA->abilities[battlerAtk], 5)
          && IsAbilityOfRating(AI_DATA->abilities[battlerDef], 5))
            break;
        score--;
        break;
    case EFFECT_INGRAIN:
        break;
    case EFFECT_SUPERPOWER:
        if (AI_DATA->abilities[battlerAtk] == ABILITY_CONTRARY)
        {
            if (GetMoveDamageResult(move) == MOVE_POWER_BEST)
                break;
            else 
            {
                ShouldSetupOthers(battlerAtk, battlerDef, MOVE_BULK_UP, &score, TRUE);
            }
        }
        break;
    case EFFECT_MAKE_IT_RAIN:
    case EFFECT_OVERHEAT:
        if (AI_DATA->abilities[battlerAtk] == ABILITY_CONTRARY)
        {
            if (GetMoveDamageResult(move) == MOVE_POWER_BEST)
                break;
            else 
            {
                ShouldSetupOffensive(battlerAtk, battlerDef, STAT_SPATK, &score, TRUE);
            }
        }
        break;
    case EFFECT_MAGIC_COAT:
        if (HasMoveEffect(battlerDef, EFFECT_STEALTH_ROCK)
            || HasMoveEffect(battlerDef, EFFECT_SPIKES)
            || HasMoveEffect(battlerDef, EFFECT_TOXIC_SPIKES)
            || HasMoveEffect(battlerDef, EFFECT_STICKY_WEB)) {
                if (AI_RandLessThan(128))
                    score++;
        }
        score--;
        break;
    case EFFECT_RECYCLE:
        if (GetUsedHeldItem(battlerAtk) != ITEM_NONE)
            score++;
        if (IsRecycleEncouragedItem(GetUsedHeldItem(battlerAtk)))
            score++;
        if (AI_DATA->abilities[battlerAtk] == ABILITY_RIPEN)
        {
            u16 item = GetUsedHeldItem(battlerAtk);
            u16 toHeal = (ItemId_GetHoldEffectParam(item) == 10) ? 10 : gBattleMons[battlerAtk].maxHP / ItemId_GetHoldEffectParam(item);

            if (IsStatBoostingBerry(item) && AI_DATA->hpPercents[battlerAtk] > 60)
                score++;
            else if (ShouldRestoreHpBerry(battlerAtk, item) && !CanAIFaintTarget(battlerAtk, battlerDef, 0)
              && ((GetWhichBattlerFasterOrTies(battlerAtk, battlerDef, TRUE) == 0 && CanTargetFaintAiWithMod(battlerDef, battlerAtk, 0, 0))
               || !CanTargetFaintAiWithMod(battlerDef, battlerAtk, toHeal, 0)))
                score++;    // Recycle healing berry if we can't otherwise faint the target and the target wont kill us after we activate the berry
        }
        break;
    case EFFECT_SKILL_SWAP:
        if (GetAbilityRating(AI_DATA->abilities[battlerDef]) > GetAbilityRating(AI_DATA->abilities[battlerAtk]))
            score++;
        break;
    case EFFECT_WORRY_SEED:
    case EFFECT_GASTRO_ACID:
    case EFFECT_SIMPLE_BEAM:
        if (IsAbilityOfRating(AI_DATA->abilities[battlerDef], 5))
            score++;
        break;
    case EFFECT_ENTRAINMENT:
        if (IsAbilityOfRating(AI_DATA->abilities[battlerDef], 5) || GetAbilityRating(AI_DATA->abilities[battlerAtk]) <= 0)
        {
            if (AI_DATA->abilities[battlerDef] != AI_DATA->abilities[battlerAtk] && !(gStatuses3[battlerDef] & STATUS3_GASTRO_ACID))
                score++;
        }
        break;
    case EFFECT_IMPRISON:
        if (ShouldImprison(battlerAtk, battlerDef))
            score += 3;
        break;
    case EFFECT_REFRESH:
        if (gBattleMons[battlerAtk].status1 & STATUS1_ANY)
            score++;
        break;
    case EFFECT_REFLECT_TYPE:
        if (ShouldReflectType(battlerAtk, battlerDef) && AI_RandLessThan(128)) {
            score++;
        }
        else if (AI_RandLessThan(128)) {
            score--;
        }
        break;
    case EFFECT_PSYCHO_SHIFT:
        if (gBattleMons[battlerAtk].status1 & STATUS1_PSN_ANY)
            IncreasePoisonScore(battlerAtk, battlerDef, move, &score);
        else if (gBattleMons[battlerAtk].status1 & STATUS1_BURN)
            IncreaseBurnScore(battlerAtk, battlerDef, move, &score);
        else if (gBattleMons[battlerAtk].status1 & STATUS1_PARALYSIS)
            IncreaseParalyzeScore(battlerAtk, battlerDef, move, &score);
        else if (gBattleMons[battlerAtk].status1 & STATUS1_SLEEP)
            IncreaseSleepScore(battlerAtk, battlerDef, move, &score);
        else if (gBattleMons[battlerAtk].status1 & STATUS1_FROSTBITE)
            IncreaseFrostbiteScore(battlerAtk, battlerDef, move, &score);
        break;
    case EFFECT_SNATCH:
        break;
    case EFFECT_MUD_SPORT:
    case EFFECT_WATER_SPORT:
        if (AI_RandLessThan(64))
            score += 3;
        break;
    case EFFECT_TICKLE:
        if (!ShouldLowerAttack(battlerAtk, battlerDef, AI_DATA->abilities[battlerDef]))
            score -= 2;
        if (gBattleMons[battlerDef].statStages[STAT_ATK] < DEFAULT_STAT_STAGE)
            score -= 2;
        if (!ShouldLowerDefense(battlerAtk, battlerDef, AI_DATA->abilities[battlerDef]))
            score -= 2;
        if (gBattleMons[battlerDef].statStages[STAT_DEF] < DEFAULT_STAT_STAGE)
            score -= 2;
        else if (AI_RandLessThan(51))
            score++;
        break;
     case EFFECT_COSMIC_POWER:
        ShouldSetupDefensive(battlerAtk, battlerDef, STAT_HP, &score, FALSE);
        break;
    case EFFECT_CURSE:
        if (IS_BATTLER_OF_TYPE(battlerAtk, TYPE_GHOST))
            break;
    case EFFECT_NO_RETREAT:
    case EFFECT_VICTORY_DANCE:
    case EFFECT_COIL:
    case EFFECT_BULK_UP:
        ShouldSetupOthers(battlerAtk, battlerDef, MOVE_BULK_UP, &score, FALSE);
        break;
    case EFFECT_CALM_MIND:
        ShouldSetupOthers(battlerAtk, battlerDef, move, &score, FALSE);
        break;
    case EFFECT_GEOMANCY:
        if (AI_DATA->holdEffects[battlerAtk] == HOLD_EFFECT_POWER_HERB && AI_RandLessThan(204))
        {
            score += 3;
            break;
        }
        score--;
    case EFFECT_QUIVER_DANCE:
        ShouldSetupOthers(battlerAtk, battlerDef, MOVE_CALM_MIND, &score, FALSE);
        break;
    case EFFECT_COACHING:
        if (isDoubleBattle && AI_RandLessThan(128))
        {
            if (AI_RandLessThan(128))
                score += 1;
            if (gBattleMons[battlerAtkPartner].statStages[STAT_ATK] < 8 && AI_RandLessThan(204))
                score += 1;
            if (gBattleMons[battlerAtkPartner].statStages[STAT_DEF] < 8 && AI_RandLessThan(204))
                score += 1;
        }
        else
            score -= 20;
        break;
    case EFFECT_SHELL_SMASH:
        if (IsBattlerIncapacitated(battlerDef, AI_DATA->abilities[battlerDef]) && AI_RandLessThan(230))
            score += 3;
        if (!CanFaintTargetAfterSmash(battlerAtk, battlerDef))
        {
            score += 2;
        }
        else
            score -= 2;
        break;
    case EFFECT_GUARD_SWAP:
        if (gBattleMons[battlerDef].statStages[STAT_DEF] > gBattleMons[battlerAtk].statStages[STAT_DEF]
          && gBattleMons[battlerDef].statStages[STAT_SPDEF] >= gBattleMons[battlerAtk].statStages[STAT_SPDEF])
            break;
        else if (gBattleMons[battlerDef].statStages[STAT_SPDEF] > gBattleMons[battlerAtk].statStages[STAT_SPDEF]
          && gBattleMons[battlerDef].statStages[STAT_DEF] >= gBattleMons[battlerAtk].statStages[STAT_DEF])
            break;
        score--;
    case EFFECT_POWER_SWAP:
        if (gBattleMons[battlerDef].statStages[STAT_ATK] > gBattleMons[battlerAtk].statStages[STAT_ATK]
          && gBattleMons[battlerDef].statStages[STAT_SPATK] >= gBattleMons[battlerAtk].statStages[STAT_SPATK])
            break;
        else if (gBattleMons[battlerDef].statStages[STAT_SPATK] > gBattleMons[battlerAtk].statStages[STAT_SPATK]
          && gBattleMons[battlerDef].statStages[STAT_ATK] >= gBattleMons[battlerAtk].statStages[STAT_ATK])
            break;
        score--;
    case EFFECT_POWER_TRICK:
        if (!(gStatuses3[battlerAtk] & STATUS3_POWER_TRICK))
        {
            if (gBattleMons[battlerAtk].defense > gBattleMons[battlerAtk].attack && HasMoveWithSplit(battlerAtk, SPLIT_PHYSICAL))
                score += 1;
            break;
        }
        break;
    case EFFECT_HEART_SWAP:
        {
            bool32 hasHigherStat = FALSE;
            //Only use if all target stats are >= attacker stats to prevent infinite loop
            for (i = STAT_ATK; i < NUM_BATTLE_STATS; i++)
            {
                if (gBattleMons[battlerDef].statStages[i] < gBattleMons[battlerAtk].statStages[i])
                    break;
                if (gBattleMons[battlerDef].statStages[i] > gBattleMons[battlerAtk].statStages[i])
                    hasHigherStat = TRUE;
            }
            if (hasHigherStat && i == NUM_BATTLE_STATS)
                score++;
        }
        score--;
        break;
    case EFFECT_SPEED_SWAP:
        if ((GetBattlerTotalSpeedStat(battlerAtk) < GetBattlerTotalSpeedStat(battlerDef)))
            score += 1;
        else {
            score -= 6;
        }
        break;
    case EFFECT_GUARD_SPLIT:
        {
            u16 newDefense = (gBattleMons[battlerAtk].defense + gBattleMons[battlerDef].defense) / 2;
            u16 newSpDef = (gBattleMons[battlerAtk].spDefense + gBattleMons[battlerDef].spDefense) / 2;

            if ((newDefense > gBattleMons[battlerAtk].defense && newSpDef >= gBattleMons[battlerAtk].spDefense)
              || (newSpDef > gBattleMons[battlerAtk].spDefense && newDefense >= gBattleMons[battlerAtk].defense))
                score++;
            else 
                score--;
        }
        break;
    case EFFECT_POWER_SPLIT:
        {
            u16 newAttack = (gBattleMons[battlerAtk].attack + gBattleMons[battlerDef].attack) / 2;
            u16 newSpAtk = (gBattleMons[battlerAtk].spAttack + gBattleMons[battlerDef].spAttack) / 2;

            if ((newAttack > gBattleMons[battlerAtk].attack && newSpAtk >= gBattleMons[battlerAtk].spAttack)
              || (newSpAtk > gBattleMons[battlerAtk].spAttack && newAttack >= gBattleMons[battlerAtk].attack))
                score++;
            else 
                score--;
        }
        break;
    case EFFECT_RELIC_SONG:
        if (GetMoveDamageResult(move) == MOVE_POWER_BEST)
            break;
        if (gBattleMons[battlerAtk].species == SPECIES_MELOETTA)
            score += 10;
        else if (gBattleMons[battlerAtk].species == SPECIES_MELOETTA_PIROUETTE && AI_RandLessThan(64))
            score++;
        break;
    case EFFECT_ELECTRIC_TERRAIN:
    case EFFECT_MISTY_TERRAIN:
    case EFFECT_GRASSY_TERRAIN:    
    case EFFECT_PSYCHIC_TERRAIN:
        score += 2;
        if (AI_DATA->holdEffects[battlerAtk] == HOLD_EFFECT_TERRAIN_EXTENDER && AI_RandLessThan(204))
            score++;
        break;
    case EFFECT_PLEDGE:
        if (isDoubleBattle)
        {
            if (HasMoveEffect(BATTLE_PARTNER(battlerAtk), EFFECT_PLEDGE))
                score += 3; // Partner might use pledge move
        }
        break;
    case EFFECT_TRICK_ROOM:
        if (!ShouldTailwind(battlerAtk, battlerDef)) {
            score--;
            break;
        }
        if (HasMoveEffect(battlerDef, EFFECT_TRICK_ROOM)
            || HasMoveEffect(battlerDef, EFFECT_TAUNT)) {
                if (AI_RandLessThan(128)) 
                    break;
        }
        score += 4;
        break;
    case EFFECT_MAGIC_ROOM:
        if (AI_RandLessThan(64))
            score += 3;
        break;
    case EFFECT_WONDER_ROOM:
        if ((HasMoveWithSplit(battlerDef, SPLIT_PHYSICAL) && gBattleMons[battlerAtk].defense < gBattleMons[battlerAtk].spDefense)
          || (HasMoveWithSplit(battlerDef, SPLIT_SPECIAL) && gBattleMons[battlerAtk].spDefense < gBattleMons[battlerAtk].defense))
            score += 1;
        if (AI_RandLessThan(64))
            score += 2;
        break;
    case EFFECT_GRAVITY:
        if (!(gFieldStatuses & STATUS_FIELD_GRAVITY) && AI_RandLessThan(128))
        {
            score++;
            if (AI_RandLessThan(64))
                score += 4;
        }
        break;
    case EFFECT_ION_DELUGE:
        if ((AI_DATA->abilities[battlerAtk] == ABILITY_VOLT_ABSORB
          || AI_DATA->abilities[battlerAtk] == ABILITY_MOTOR_DRIVE
          || AI_DATA->abilities[battlerAtk] == ABILITY_LIGHTNING_ROD)
          && gBattleMoves[predictedMove].type == TYPE_NORMAL)
            score += 1;
        score--;
        break;
    case EFFECT_FLING:
        if (GetMoveDamageResult(move) == MOVE_POWER_BEST)
            break;
        if (AI_DATA->holdEffects[battlerAtk] == HOLD_EFFECT_FLINCH 
            && AI_WhosFaster(battlerAtk, battlerDef) == AI_IS_FASTER
            && !(AI_DATA->abilities[battlerDef] == ABILITY_INNER_FOCUS || AI_DATA->abilities[battlerDef] == ABILITY_SHIELD_DUST))
            score += 9;
        break;
    case EFFECT_EMBARGO:
        if (AI_DATA->holdEffects[battlerDef] != HOLD_EFFECT_NONE)
            break;
        score--;
        break;
    case EFFECT_POWDER:
        if (HasMoveWithType(battlerDef, TYPE_FIRE))
            score += 1;
        score--;
        break;
    case EFFECT_TOPSY_TURVY:
        if (CountPositiveStatStages(battlerDef) > CountNegativeStatStages(battlerDef))
            score++;
        score--;
        break;
    case EFFECT_FAIRY_LOCK:
        if (!IsBattlerTrapped(battlerDef, TRUE))
        {
            if (ShouldTrap(battlerAtk, battlerDef, move))
                score++;
        }
        score--;
        break;
    case EFFECT_TAILWIND:
        if (HasMoveEffect(battlerDef, EFFECT_TRICK_ROOM)
            || HasMoveEffect(battlerDef, EFFECT_TAUNT)) 
        {
            if (AI_RandLessThan(128))
                score--;
        }
        if (ShouldTailwind(battlerAtk, battlerDef))
            score += 3;
        if (gSideStatuses[GetBattlerSide(battlerDef)] & SIDE_STATUS_TAILWIND
            && (GetBattlerTotalSpeedStat(battlerAtk) < GetBattlerTotalSpeedStat(battlerDef)))
            score += 3;
        break;
    case EFFECT_LUCKY_CHANT:
        break;
    case EFFECT_MAGNET_RISE:
        if (IsBattlerGrounded(battlerAtk) && HasDamagingMoveOfType(battlerDef, TYPE_GROUND))
        {
            if (AI_WhosFaster(battlerAtk, battlerDef) == AI_IS_FASTER) // Attacker goes first
           {
                if (AI_RandLessThan(230))
                    score += 2; // Cause the enemy's move to fail
                break;
            }
            else // Opponent Goes First
                score--;
        }
        else
            score--;
        break;
    case EFFECT_CAMOUFLAGE:
        break;
    case EFFECT_TOXIC_THREAD:
        IncreasePoisonScore(battlerAtk, battlerDef, move, &score);
        if (AI_WhosFaster(battlerAtk, battlerDef) == AI_IS_FASTER && AI_RandLessThan(128))
            score++;
        break;
    case EFFECT_COUNTER:
        score += 6;

        if (CanTargetFaintAi(battlerDef, battlerAtk))
        {
            if ((IsBattlerSashSturdyProtected(battlerAtk))
            && (HasOnlyMovesWithSplit(battlerDef, SPLIT_PHYSICAL, TRUE)))
                score += 2;
        }
        else if (HasOnlyMovesWithSplit(battlerDef, SPLIT_PHYSICAL, TRUE)
                 && AI_RandLessThan(204))
                score += 2;

        if (AI_WhosFaster(battlerAtk, battlerDef) == AI_IS_FASTER
            && AI_RandLessThan(64))
            score--;

        if (HasMoveWithSplit(battlerDef, SPLIT_STATUS) && AI_RandLessThan(64))
            score--;
        break;
    case EFFECT_MIRROR_COAT:
        score += 6;
        if (CanTargetFaintAi(battlerDef, battlerAtk))
        {
            if (IsBattlerSashSturdyProtected(battlerAtk)
            && (HasOnlyMovesWithSplit(battlerDef, SPLIT_SPECIAL, TRUE)))
                score += 2;
        }
        else if (HasOnlyMovesWithSplit(battlerDef, SPLIT_SPECIAL, TRUE)
                 && AI_RandLessThan(204))
                score += 2;
        if (AI_WhosFaster(battlerAtk, battlerDef) == AI_IS_FASTER
            && AI_RandLessThan(64))
            score--;
        if (HasMoveWithSplit(battlerDef, SPLIT_STATUS) && AI_RandLessThan(64))
            score--;        
        break;
    case EFFECT_METAL_BURST:
        score += 6;
        if (IsBattlerSashSturdyProtected(battlerAtk))
            score += 2;
        if (AI_RandLessThan(64))
            score++;
        if (HasMoveWithSplit(battlerDef, SPLIT_STATUS) && AI_RandLessThan(128))
            score--;
        break;
    case EFFECT_REVIVAL_BLESSING:
        if (GetFirstFaintedPartyIndex(battlerAtk) != PARTY_SIZE && AI_RandLessThan(128))
            score += 2;
        break;
    case EFFECT_UPPER_HAND:
        if (HasMoveWithAttackPrio(battlerDef) && AI_RandLessThan(26)) // ~10.1%
            score += 13;
        break;
    case EFFECT_ORDER_UP:
        if (GetMoveDamageResult(move) == MOVE_POWER_BEST)
            break;
        switch (gBattleStruct->commanderActive[battlerAtk])
        {
            case SPECIES_TATSUGIRI:
                ShouldSetupOffensive(battlerAtk, battlerDef, STAT_ATK, &score, TRUE);
                break;
            case SPECIES_TATSUGIRI_DROOPY:
                ShouldSetupDefensive(battlerAtk, battlerDef, STAT_DEF, &score, TRUE);
                break;
            case SPECIES_TATSUGIRI_STRETCHY:
                if (AI_WhosFaster(battlerAtk, battlerDef) == AI_IS_SLOWER)
                {
                    score += 6;
                    if (AI_RandLessThan(204))
                        score++;
                }
                else
                {
                    score--;
                }
                break;
            default:
                break;
        }
        break;
    case EFFECT_TRI_ATTACK:
        if (GetMoveDamageResult(move) == MOVE_POWER_BEST 
            || moveEffectChance != 100 
            || AI_DATA->items[battlerDef] == ITEM_COVERT_CLOAK
            || AI_DATA->abilities[battlerDef] == ABILITY_SHIELD_DUST
            || TestSheerForceFlag(battlerAtk, move))
            break;
        if (AI_CanStatus(battlerDef, AI_DATA->abilities[battlerDef]))
            score += 6;
        break;
    } // move effect checks

    return score;
}

// Effects that are encouraged on the first turn of battle
static s16 AI_SetupFirstTurn(u8 battlerAtk, u8 battlerDef, u16 move, s16 score)
{
    if (IsTargetingPartner(battlerAtk, battlerDef)
      || gBattleResults.battleTurnCounter != 0)
        return score;

    if (AI_THINKING_STRUCT->aiFlags & AI_FLAG_SMART_SWITCHING
      && AI_WhosFaster(battlerAtk, battlerDef) == AI_IS_SLOWER
      && CanTargetFaintAi(battlerDef, battlerAtk)
      && GetMovePriority(battlerAtk, move) == 0)
    {
        RETURN_SCORE_MINUS(20);    // No point in setting up if you will faint. Should just switch if possible..
    }

    // check effects to prioritize first turn
    switch (gBattleMoves[move].effect)
    {
    case EFFECT_ATTACK_UP:
    case EFFECT_ATTACK_UP_USER_ALLY:
    case EFFECT_DEFENSE_UP:
    case EFFECT_SPEED_UP:
    case EFFECT_SPECIAL_ATTACK_UP:
    case EFFECT_SPECIAL_DEFENSE_UP:
    case EFFECT_ACCURACY_UP:
    case EFFECT_EVASION_UP:
    case EFFECT_ATTACK_DOWN:
    case EFFECT_DEFENSE_DOWN:
    case EFFECT_SPEED_DOWN:
    case EFFECT_SPECIAL_ATTACK_DOWN:
    case EFFECT_SPECIAL_DEFENSE_DOWN:
    case EFFECT_ACCURACY_DOWN:
    case EFFECT_EVASION_DOWN:
    case EFFECT_CONVERSION:
    case EFFECT_LIGHT_SCREEN:
    case EFFECT_FOCUS_ENERGY:
    case EFFECT_CONFUSE:
    case EFFECT_ATTACK_UP_2:
    case EFFECT_DEFENSE_UP_2:
    case EFFECT_DEFENSE_UP_3:
    case EFFECT_SPEED_UP_2:
    case EFFECT_SPECIAL_ATTACK_UP_2:
    case EFFECT_SPECIAL_ATTACK_UP_3:
    case EFFECT_SPECIAL_DEFENSE_UP_2:
    case EFFECT_ACCURACY_UP_2:
    case EFFECT_EVASION_UP_2:
    case EFFECT_ATTACK_DOWN_2:
    case EFFECT_DEFENSE_DOWN_2:
    case EFFECT_SPEED_DOWN_2:
    case EFFECT_SPECIAL_ATTACK_DOWN_2:
    case EFFECT_SPECIAL_DEFENSE_DOWN_2:
    case EFFECT_ACCURACY_DOWN_2:
    case EFFECT_EVASION_DOWN_2:
    case EFFECT_REFLECT:
    case EFFECT_POISON:
    case EFFECT_PARALYZE:
    case EFFECT_SUBSTITUTE:
    case EFFECT_LEECH_SEED:
    case EFFECT_MINIMIZE:
    case EFFECT_CURSE:
    case EFFECT_SWAGGER:
    case EFFECT_CAMOUFLAGE:
    case EFFECT_YAWN:
    case EFFECT_DEFENSE_CURL:
    case EFFECT_TORMENT:
    case EFFECT_FLATTER:
    case EFFECT_WILL_O_WISP:
    case EFFECT_INGRAIN:
    case EFFECT_IMPRISON:
    case EFFECT_TICKLE:
    case EFFECT_COSMIC_POWER:
    case EFFECT_BULK_UP:
    case EFFECT_CALM_MIND:
    case EFFECT_ACUPRESSURE:
    case EFFECT_AUTOTOMIZE:
    case EFFECT_SHIFT_GEAR:
    case EFFECT_SHELL_SMASH:
    case EFFECT_GROWTH:
    case EFFECT_QUIVER_DANCE:
    case EFFECT_ATTACK_SPATK_UP:
    case EFFECT_ATTACK_ACCURACY_UP:
    case EFFECT_PSYCHIC_TERRAIN:
    case EFFECT_GRASSY_TERRAIN:
    case EFFECT_ELECTRIC_TERRAIN:
    case EFFECT_MISTY_TERRAIN:
    case EFFECT_STEALTH_ROCK:
    case EFFECT_TOXIC_SPIKES:
    case EFFECT_TRICK_ROOM:
    case EFFECT_WONDER_ROOM:
    case EFFECT_MAGIC_ROOM:
    case EFFECT_TAILWIND:
    case EFFECT_DRAGON_DANCE:
    case EFFECT_STICKY_WEB:
    case EFFECT_RAIN_DANCE:
    case EFFECT_SUNNY_DAY:
    case EFFECT_SANDSTORM:
    case EFFECT_HAIL:
    case EFFECT_SNOWSCAPE:
    case EFFECT_GEOMANCY:
    case EFFECT_VICTORY_DANCE:
    case EFFECT_HIT_SET_ENTRY_HAZARD:
        score += 2;
        break;
    default:
        break;
    }

    return score;
}

// Adds score bonus to 'riskier' move effects and high crit moves
static s16 AI_Risky(u8 battlerAtk, u8 battlerDef, u16 move, s16 score)
{
    if (IsTargetingPartner(battlerAtk, battlerDef))
        return score;

    if (gBattleMoves[move].highCritRatio)
        score += 2;

    switch (gBattleMoves[move].effect)
    {
    case EFFECT_SLEEP:
    case EFFECT_EXPLOSION:
    case EFFECT_MIRROR_MOVE:
    case EFFECT_OHKO:
    case EFFECT_CONFUSE:
    case EFFECT_METRONOME:
    case EFFECT_PSYWAVE:
    case EFFECT_COUNTER:
    case EFFECT_DESTINY_BOND:
    case EFFECT_SWAGGER:
    case EFFECT_ATTRACT:
    case EFFECT_PRESENT:
    case EFFECT_ALL_STATS_UP_HIT:
    case EFFECT_BELLY_DRUM:
    case EFFECT_MIRROR_COAT:
    case EFFECT_FOCUS_PUNCH:
    case EFFECT_REVENGE:
        if (Random() & 1)
            score += 2;
        break;
    default:
        break;
    }

    return score;
}

// Adds score bonus to best powered move
static s16 AI_PreferStrongestMove(u8 battlerAtk, u8 battlerDef, u16 move, s16 score)
{
    if (IsTargetingPartner(battlerAtk, battlerDef))
        return score;

    if (GetMoveDamageResult(move) == MOVE_POWER_BEST)
    {
        if (AI_RandLessThan(191)) //highest damage move - 75% +6, 25% +8 
            score += 6;
        else
            score += 8;
    }

    return score;
}

// Prefers moves that are good for baton pass
static s16 AI_PreferBatonPass(u8 battlerAtk, u8 battlerDef, u16 move, s16 score)
{
    u32 i;

    if (IsTargetingPartner(battlerAtk, battlerDef)
      || CountUsablePartyMons(battlerAtk) == 0
      || GetMoveDamageResult(move) != MOVE_POWER_OTHER
      || !HasMoveEffect(battlerAtk, EFFECT_BATON_PASS)
      || IsBattlerTrapped(battlerAtk, TRUE))
        return score;

    if (IsStatRaisingEffect(gBattleMoves[move].effect))
    {
        if (gBattleResults.battleTurnCounter == 0)
            score += 5;
        else if (AI_DATA->hpPercents[battlerAtk] < 60)
            score -= 10;
        else
            score++;
    }

    // other specific checks
    switch (gBattleMoves[move].effect)
    {
    case EFFECT_INGRAIN:
        if (!(gStatuses3[battlerAtk] & STATUS3_ROOTED))
            score += 2;
        break;
    case EFFECT_AQUA_RING:
        if (!(gStatuses3[battlerAtk] & STATUS3_AQUA_RING))
            score += 2;
        break;
    case EFFECT_PROTECT:
        if (gLastMoves[battlerAtk] == MOVE_PROTECT || gLastMoves[battlerAtk] == MOVE_DETECT)
            score -= 2;
        else
            score += 2;
        break;
    case EFFECT_BATON_PASS:
        for (i = STAT_ATK; i < NUM_BATTLE_STATS; i++)
        {
            IncreaseStatUpScore(battlerAtk, battlerDef, i, &score);
        }
        if (gStatuses3[battlerAtk] & (STATUS3_ROOTED | STATUS3_AQUA_RING))
            score += 2;
        if (gStatuses3[battlerAtk] & STATUS3_LEECHSEED)
            score -= 3;
        break;
    default:
        break;
    }

    return score;
}

static s16 AI_HPAware(u8 battlerAtk, u8 battlerDef, u16 move, s16 score)
{
    u16 effect = gBattleMoves[move].effect;
    u8 moveType = gBattleMoves[move].type;

    SetTypeBeforeUsingMove(move, battlerAtk);
    GET_MOVE_TYPE(move, moveType);

    if (IsTargetingPartner(battlerAtk, battlerDef))
    {
        if ((effect == EFFECT_HEAL_PULSE || effect == EFFECT_HIT_ENEMY_HEAL_ALLY)
         || (moveType == TYPE_ELECTRIC && AI_DATA->abilities[BATTLE_PARTNER(battlerAtk)] == ABILITY_VOLT_ABSORB)
         || (moveType == TYPE_GROUND && AI_DATA->abilities[BATTLE_PARTNER(battlerAtk)] == ABILITY_EARTH_EATER)
         || (moveType == TYPE_WATER && (AI_DATA->abilities[BATTLE_PARTNER(battlerAtk)] == ABILITY_DRY_SKIN || AI_DATA->abilities[BATTLE_PARTNER(battlerAtk)] == ABILITY_WATER_ABSORB)))
        {
            if (gStatuses3[battlerDef] & STATUS3_HEAL_BLOCK)
                return 0;

            if (CanTargetFaintAi(FOE(battlerAtk), BATTLE_PARTNER(battlerAtk))
              || (CanTargetFaintAi(BATTLE_PARTNER(FOE(battlerAtk)), BATTLE_PARTNER(battlerAtk))))
                score--;

            if (AI_DATA->hpPercents[battlerDef] <= 50)
                score++;
        }
    }
    else
    {
        // Consider AI HP
        if (AI_DATA->hpPercents[battlerAtk] > 70)
        {
            // high hp
            switch (effect)
            {
            case EFFECT_EXPLOSION:
            case EFFECT_RESTORE_HP:
            case EFFECT_REST:
            case EFFECT_DESTINY_BOND:
            case EFFECT_FLAIL:
            case EFFECT_ENDURE:
            case EFFECT_MORNING_SUN:
            case EFFECT_SYNTHESIS:
            case EFFECT_MOONLIGHT:
            case EFFECT_SHORE_UP:
            case EFFECT_SOFTBOILED:
            case EFFECT_ROOST:
            case EFFECT_MEMENTO:
            case EFFECT_GRUDGE:
                score -= 2;
                break;
            default:
                break;
            }
        }
        else if (AI_DATA->hpPercents[battlerAtk] > 30)
        {
            // med hp
            if (IsStatRaisingEffect(effect) || IsStatLoweringEffect(effect))
                score -= 2;

            switch (effect)
            {
            case EFFECT_EXPLOSION:
            case EFFECT_BIDE:
            case EFFECT_CONVERSION:
            case EFFECT_LIGHT_SCREEN:
            case EFFECT_MIST:
            case EFFECT_FOCUS_ENERGY:
            case EFFECT_CONVERSION_2:
            case EFFECT_SAFEGUARD:
            case EFFECT_BELLY_DRUM:
                score -= 2;
                break;
            default:
                break;
            }
        }
        else
        {
            // low hp
            if (IsStatRaisingEffect(effect) || IsStatLoweringEffect(effect))
                score -= 2;

            // check other discouraged low hp effects
            switch (effect)
            {
            case EFFECT_BIDE:
            case EFFECT_CONVERSION:
            case EFFECT_REFLECT:
            case EFFECT_LIGHT_SCREEN:
            case EFFECT_AURORA_VEIL:
            case EFFECT_MIST:
            case EFFECT_FOCUS_ENERGY:
            case EFFECT_RAGE:
            case EFFECT_CONVERSION_2:
            case EFFECT_LOCK_ON:
            case EFFECT_SAFEGUARD:
            case EFFECT_BELLY_DRUM:
            case EFFECT_PSYCH_UP:
            case EFFECT_MIRROR_COAT:
            case EFFECT_SOLAR_BEAM:
            case EFFECT_TWO_TURNS_ATTACK:
            case EFFECT_ERUPTION:
            case EFFECT_TICKLE:
            case EFFECT_SUNNY_DAY:
            case EFFECT_SANDSTORM:
            case EFFECT_HAIL:
            case EFFECT_SNOWSCAPE:
            case EFFECT_RAIN_DANCE:
                score -= 2;
                break;
            default:
                break;
            }
        }
    }

    // consider target HP
    if (CanIndexMoveFaintTarget(battlerAtk, battlerDef, AI_THINKING_STRUCT->movesetIndex, 0))
    {
        score += 2;
    }
    else
    {
        if (AI_DATA->hpPercents[battlerDef] > 70)
        {
            // high HP
            ; // nothing yet
        }
        else if (AI_DATA->hpPercents[battlerDef] > 30)
        {
            // med HP - check discouraged effects
            switch (effect)
            {
            case EFFECT_ATTACK_UP:
            case EFFECT_ATTACK_UP_USER_ALLY:
            case EFFECT_DEFENSE_UP:
            case EFFECT_SPEED_UP:
            case EFFECT_SPECIAL_ATTACK_UP:
            case EFFECT_SPECIAL_DEFENSE_UP:
            case EFFECT_ACCURACY_UP:
            case EFFECT_EVASION_UP:
            case EFFECT_ATTACK_DOWN:
            case EFFECT_DEFENSE_DOWN:
            case EFFECT_SPEED_DOWN:
            case EFFECT_SPECIAL_ATTACK_DOWN:
            case EFFECT_SPECIAL_DEFENSE_DOWN:
            case EFFECT_ACCURACY_DOWN:
            case EFFECT_EVASION_DOWN:
            case EFFECT_MIST:
            case EFFECT_FOCUS_ENERGY:
            case EFFECT_ATTACK_UP_2:
            case EFFECT_DEFENSE_UP_2:
            case EFFECT_SPEED_UP_2:
            case EFFECT_SPECIAL_ATTACK_UP_2:
            case EFFECT_SPECIAL_DEFENSE_UP_2:
            case EFFECT_ACCURACY_UP_2:
            case EFFECT_EVASION_UP_2:
            case EFFECT_ATTACK_DOWN_2:
            case EFFECT_DEFENSE_DOWN_2:
            case EFFECT_SPEED_DOWN_2:
            case EFFECT_SPECIAL_ATTACK_DOWN_2:
            case EFFECT_SPECIAL_DEFENSE_DOWN_2:
            case EFFECT_ACCURACY_DOWN_2:
            case EFFECT_EVASION_DOWN_2:
            case EFFECT_POISON:
            case EFFECT_PAIN_SPLIT:
            case EFFECT_PERISH_SONG:
            case EFFECT_SAFEGUARD:
            case EFFECT_TICKLE:
            case EFFECT_COSMIC_POWER:
            case EFFECT_BULK_UP:
            case EFFECT_CALM_MIND:
            case EFFECT_DRAGON_DANCE:
            case EFFECT_DEFENSE_UP_3:
            case EFFECT_SPECIAL_ATTACK_UP_3:
                score -= 2;
                break;
            default:
                break;
            }
        }
        else
        {
            // low HP
            if (IS_MOVE_STATUS(move))
                score -= 2; // don't use status moves if target is at low health
        }
    }

    return score;
}

static void AI_Flee(void)
{
    AI_THINKING_STRUCT->aiAction |= (AI_ACTION_DONE | AI_ACTION_FLEE | AI_ACTION_DO_NOT_ATTACK);
}

static void AI_Watch(void)
{
    AI_THINKING_STRUCT->aiAction |= (AI_ACTION_DONE | AI_ACTION_WATCH | AI_ACTION_DO_NOT_ATTACK);
}

// Roaming pokemon logic
static s16 AI_Roaming(u8 battlerAtk, u8 battlerDef, u16 move, s16 score)
{
    if (IsBattlerTrapped(battlerAtk, FALSE))
        return score;

    AI_Flee();
    return score;
}

// Safari pokemon logic
static s16 AI_Safari(u8 battlerAtk, u8 battlerDef, u16 move, s16 score)
{
    u8 safariFleeRate = gBattleStruct->safariEscapeFactor * 5; // Safari flee rate, from 0-20.

    if ((Random() % 100) < safariFleeRate)
        AI_Flee();
    else
        AI_Watch();

    return score;
}

// First battle logic
static s16 AI_FirstBattle(u8 battlerAtk, u8 battlerDef, u16 move, s16 score)
{
    if (AI_DATA->hpPercents[battlerDef] <= 20)
        AI_Flee();

    return score;
}
