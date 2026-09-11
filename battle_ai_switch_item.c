#include "global.h"
#include "battle.h"
#include "constants/battle_ai.h"
#include "battle_ai_main.h"
#include "battle_ai_util.h"
#include "battle_util.h"
#include "battle_anim.h"
#include "battle_controllers.h"
#include "battle_main.h"
#include "constants/hold_effects.h"
#include "battle_setup.h"
#include "data.h"
#include "item.h"
#include "party_menu.h"
#include "pokemon.h"
#include "random.h"
#include "util.h"
#include "constants/abilities.h"
#include "constants/item_effects.h"
#include "constants/battle_move_effects.h"
#include "constants/items.h"
#include "constants/moves.h"

// this file's functions
static bool8 HasSuperEffectiveMoveAgainstOpponents(bool8 noRng);
static bool8 FindMonWithFlagsAndSuperEffective(u16 flags, u8 moduloPercent);
static bool8 ShouldUseItem(void);
static bool32 AiExpectsToFaintPlayer(void);
static bool32 AI_ShouldHeal(u32 healAmount);
static bool32 AI_OpponentCanFaintAiWithMod(u32 healAmount);
static bool32 IsAiPartyMonOHKOBy(u32 battlerAtk, struct Pokemon *aiMon);

static bool32 IsAceMon(u32 battlerId, u32 monPartyId)
{
    if (AI_THINKING_STRUCT->aiFlags & AI_FLAG_ACE_POKEMON
        && !gBattleStruct->battlerState[battlerId].forcedSwitch
        && monPartyId == CalculateEnemyPartyCount()-1)
            return TRUE;
    return FALSE;
}

void GetAIPartyIndexes(u32 battlerId, s32 *firstId, s32 *lastId)
{
    if (BATTLE_TWO_VS_ONE_OPPONENT && (battlerId & BIT_SIDE) == B_SIDE_OPPONENT)
    {
        *firstId = 0, *lastId = PARTY_SIZE;
    }
    else if (gBattleTypeFlags & (BATTLE_TYPE_TWO_OPPONENTS | BATTLE_TYPE_INGAME_PARTNER | BATTLE_TYPE_TOWER_LINK_MULTI))
    {
        if ((battlerId & BIT_FLANK) == B_FLANK_LEFT)
            *firstId = 0, *lastId = PARTY_SIZE / 2;
        else
            *firstId = PARTY_SIZE / 2, *lastId = PARTY_SIZE;
    }
    else
    {
        *firstId = 0, *lastId = PARTY_SIZE;
    }
}

static bool8 ShouldSwitchIfAllBadMoves(void)
{
    if (gBattleResources->ai->switchMon)
    {
        gBattleResources->ai->switchMon = 0;
        *(gBattleStruct->AI_monToSwitchIntoId + gActiveBattler) = PARTY_SIZE;
        BtlController_EmitTwoReturnValues(BUFFER_B, B_ACTION_SWITCH, 0);
        return TRUE;
    }
    else
    {
        return FALSE;
    }
}

static bool8 ShouldSwitchIfWonderGuard(void)
{
    u8 opposingPosition;
    u8 opposingBattler;
    s32 i, j;
    s32 firstId;
    s32 lastId; // + 1
    struct Pokemon *party = NULL;
    u16 move;

    if (gBattleTypeFlags & BATTLE_TYPE_DOUBLE)
        return FALSE;

    opposingPosition = BATTLE_OPPOSITE(GetBattlerPosition(gActiveBattler));

    if (GetBattlerAbility(GetBattlerAtPosition(opposingPosition)) != ABILITY_WONDER_GUARD)
        return FALSE;

    // Check if Pokemon has a super effective move.
    for (opposingBattler = GetBattlerAtPosition(opposingPosition), i = 0; i < MAX_MON_MOVES; i++)
    {
        move = gBattleMons[gActiveBattler].moves[i];
        if (move != MOVE_NONE)
        {
            if (AI_GetTypeEffectiveness(move, gActiveBattler, opposingBattler) >= UQ_4_12(2.0))
                return FALSE;
        }
    }

    // Get party information.
    GetAIPartyIndexes(gActiveBattler, &firstId, &lastId);

    if (GetBattlerSide(gActiveBattler) == B_SIDE_PLAYER)
        party = gPlayerParty;
    else
        party = gEnemyParty;

    // Find a Pokemon in the party that has a super effective move.
    for (i = firstId; i < lastId; i++)
    {
        if (!IsValidForBattle(&party[i]))
            continue;
        if (i == gBattlerPartyIndexes[gActiveBattler])
            continue;
        if (IsAceMon(gActiveBattler, i))
            continue;

        for (opposingBattler = GetBattlerAtPosition(opposingPosition), j = 0; j < MAX_MON_MOVES; j++)
        {
            move = GetMonData(&party[i], MON_DATA_MOVE1 + j);
            if (move != MOVE_NONE)
            {
                if (AI_GetTypeEffectiveness(move, gActiveBattler, opposingBattler) >= UQ_4_12(2.0) && Random() % 3 < 2)
                {
                    // We found a mon.
                    *(gBattleStruct->AI_monToSwitchIntoId + gActiveBattler) = i;
                    BtlController_EmitTwoReturnValues(BUFFER_B, B_ACTION_SWITCH, 0);
                    return TRUE;
                }
            }
        }
    }

    return FALSE; // There is not a single Pokemon in the party that has a super effective move against a mon with Wonder Guard.
}

//Only called for AI calculations
s32 AI_HazardsModifyHP(u32 battlerAtk)
{
    u16 ability = GetBattlerAbility(battlerAtk);
    u16 holdEffect;
    u32 flags = gSideStatuses[GetBattlerSide(battlerAtk)] & (SIDE_STATUS_SPIKES | SIDE_STATUS_STEALTH_ROCK | SIDE_STATUS_STICKY_WEB | SIDE_STATUS_TOXIC_SPIKES);
    s32 hazardDamage = 0;
    u8 type1 = gSpeciesInfo[battlerAtk].types[0];
    u8 type2 = gSpeciesInfo[battlerAtk].types[1];
    u32 maxHp = gBattleMons[battlerAtk].maxHP;

    if (flags == 0)
        return 0;

    if (ability == ABILITY_MAGIC_GUARD)
        return 0;
    if (gFieldStatuses & STATUS_FIELD_MAGIC_ROOM || ability == ABILITY_KLUTZ)
        holdEffect = HOLD_EFFECT_NONE;
    else
        holdEffect = AI_DATA->holdEffects[battlerAtk];
    if (holdEffect == HOLD_EFFECT_HEAVY_DUTY_BOOTS)
        return 0;

    if (flags & SIDE_STATUS_STEALTH_ROCK)
        hazardDamage += GetStealthHazardDamageByTypesAndHP(gBattleMoves[MOVE_STEALTH_ROCK].type, type1, type2, maxHp);

    if (flags & SIDE_STATUS_SPIKES && ((type1 != TYPE_FLYING && type2 != TYPE_FLYING
        && ability != ABILITY_LEVITATE && holdEffect != HOLD_EFFECT_AIR_BALLOON)
        || holdEffect == HOLD_EFFECT_IRON_BALL || gFieldStatuses & STATUS_FIELD_GRAVITY))
    {
        u8 spikesDmg = maxHp / ((5 - gSideTimers[GetBattlerSide(battlerAtk)].spikesAmount) * 2);
        if (spikesDmg == 0)
            spikesDmg = 1;
        hazardDamage += spikesDmg;
    }

    return hazardDamage;
}

s32 AI_CalcPartyMonScore(u32 battlerAtk, u32 battlerDef, struct Pokemon *attackerMon, struct Pokemon *targetMon, bool32 hardSwitch)
{
    s32 i, moveTaken, move, score;
    int bestDmg, bestDmgTaken, dmgTaken, dmg = 0;
    u8 effectiveness;
    s32 attackerHP = 0;
    s32 defenderHP = 0;
    u16 atkAbility = 0;
    struct BattlePokemon *savedBattleMons = AllocSaveBattleMons();
    bestDmg = 0;
    bestDmgTaken = 0;

    PokemonToBattleMon(attackerMon, &gBattleMons[battlerAtk]);
    SetBattlerAiData(battlerAtk);
    SetBattlerAiData(battlerDef);
    atkAbility = GetBattlerAbility(battlerAtk);

    //Clear edge cases
    gStatuses3[battlerAtk] &= ~STATUS3_GASTRO_ACID;
    gDisableStructs[battlerAtk].usedMoves = 0;
    for (i = 0; i < MAX_MON_MOVES; i++)
    {
        move = gBattleMons[battlerAtk].moves[i];
        if (move == MOVE_EXPLOSION || move == MOVE_SELF_DESTRUCT || move == MOVE_MISTY_EXPLOSION || move == MOVE_FINAL_GAMBIT) {
            continue;
        }
        if (!GetImprisonedMovesCount(battlerAtk, move) 
            && !IsGravityPreventingMove(move)
            && gBattleMons[battlerAtk].pp[i] != 0 
            && gBattleMoves[move].power != 0)
        {
            dmg = AI_CalcDamage(move, battlerAtk, battlerDef, &effectiveness, FALSE, FALSE);
            if (dmg > bestDmg)
                bestDmg = dmg;
        }
    }
    for (i = 0; i < MAX_MON_MOVES; i++)
    {
        moveTaken = gBattleMons[battlerDef].moves[i];
        if (moveTaken == MOVE_EXPLOSION || moveTaken == MOVE_SELF_DESTRUCT || moveTaken == MOVE_MISTY_EXPLOSION || moveTaken == MOVE_FINAL_GAMBIT) {
            continue;
        }
        if (!GetImprisonedMovesCount(battlerDef, moveTaken) 
            && !IsGravityPreventingMove(moveTaken)
            && gBattleMoves[moveTaken].power != 0)
        {
            dmgTaken = AI_CalcDamage(moveTaken, battlerDef, battlerAtk, &effectiveness, FALSE, FALSE);
            if (dmgTaken > bestDmgTaken)
                bestDmgTaken = dmgTaken;
        }
    }

    DebugPrintf("bestDmg: %d", bestDmg);
    DebugPrintf("bestDmgTaken: %d", bestDmgTaken);
    attackerHP = gBattleMons[battlerAtk].hp;
    defenderHP = gBattleMons[battlerDef].hp;
    attackerHP = attackerHP - AI_HazardsModifyHP(battlerAtk); //AI is aware of damaging hazards

    if (bestDmgTaken >= attackerHP && AI_WhosFaster(battlerAtk, battlerDef) == AI_IS_SLOWER 
        && !IsBattlerSashSturdyProtected(battlerAtk)
        && !IsMimikyuDisguiseProtected(battlerAtk, battlerDef)
        && atkAbility != ABILITY_IMPOSTER)
        score = -1;
    else if (bestDmg >= defenderHP && AI_WhosFaster(battlerAtk, battlerDef) == AI_IS_FASTER) 
        score = 5;
    else if ((bestDmg >= defenderHP && AI_WhosFaster(battlerAtk, battlerDef) == AI_IS_SLOWER) || gBattleMons[battlerAtk].species == SPECIES_PALAFIN)
        score = 4;
    else if ((GetNoOfHitsToKOBattlerDmg(bestDmg, battlerDef) <= GetNoOfHitsToKOBattlerDmg(bestDmgTaken, battlerAtk) && AI_WhosFaster(battlerAtk, battlerDef) == AI_IS_FASTER) || gBattleMons[battlerAtk].species == SPECIES_DITTO)
        score = 3;
    else if ((GetNoOfHitsToKOBattlerDmg(bestDmg, battlerDef) < GetNoOfHitsToKOBattlerDmg(bestDmgTaken, battlerAtk) && AI_WhosFaster(battlerAtk, battlerDef) == AI_IS_SLOWER) || gBattleMons[battlerAtk].species == SPECIES_WYNAUT || gBattleMons[battlerAtk].species == SPECIES_WOBBUFFET)
        score = 2;
    else if (AI_WhosFaster(battlerAtk, battlerDef) == AI_IS_FASTER)
        score = 1;
    else 
        score = 0;

    //Edge cases AI score adjustments
    if (MonIsDedicatedSupport(battlerAtk) && AI_RandLessThan(26) && score != -1)  
        score += 2;

    //Hardswitch AI score adjustments
    if (hardSwitch && GetNoOfHitsToKOBattlerDmg(bestDmgTaken, battlerAtk) == 1)
        score = -2;
    if (hardSwitch && GetNoOfHitsToKOBattlerDmg(bestDmgTaken, battlerAtk) == 2 && AI_WhosFaster(battlerAtk, battlerDef) == AI_IS_SLOWER)
        score = -1;

    DebugPrintf("Score: %d", score);

    FreeRestoreBattleMons(savedBattleMons);
    return score;
}

static bool8 FindMonThatAbsorbsOpponentsMove(void)
{
    u8 battlerIn1, battlerIn2;
    u8 numAbsorbingAbilities = 0; 
    u16 absorbingTypeAbilities[3]; // Array size is maximum number of absorbing abilities for a single type
    s32 firstId;
    s32 lastId; // + 1
    struct Pokemon *party;
    s32 i, j;

    if (gLastMoves[gBattlerTarget] == MOVE_NONE)
        return FALSE;
    if (gLastMoves[gBattlerTarget] == MOVE_UNAVAILABLE)
        return FALSE;
    if (gBattleMoves[gLastMoves[gBattlerTarget]].target == MOVE_TARGET_USER 
        || gBattleMoves[gLastMoves[gBattlerTarget]].target == MOVE_TARGET_OPPONENTS_FIELD
        || gBattleMoves[gLastMoves[gBattlerTarget]].target == MOVE_TARGET_ALLY
        || gBattleMoves[gLastMoves[gBattlerTarget]].target == MOVE_TARGET_ALL_BATTLERS)
        return FALSE;
    if (gDisableStructs[gActiveBattler].isFirstTurn)
        return FALSE;

    if (gBattleTypeFlags & BATTLE_TYPE_DOUBLE)
    {
        battlerIn1 = gActiveBattler;
        if (gAbsentBattlerFlags & gBitTable[GetBattlerAtPosition(BATTLE_PARTNER(GetBattlerPosition(gActiveBattler)))])
            battlerIn2 = gActiveBattler;
        else
            battlerIn2 = GetBattlerAtPosition(BATTLE_PARTNER(GetBattlerPosition(gActiveBattler)));
    }
    else
    {
        battlerIn1 = gActiveBattler;
        battlerIn2 = gActiveBattler;
    }

    // Create an array of possible absorb abilities so the AI considers all of them
    if (gBattleMoves[gLastMoves[gBattlerTarget]].type == TYPE_FIRE)
        {
        absorbingTypeAbilities[0] = ABILITY_FLASH_FIRE;
        absorbingTypeAbilities[1] = ABILITY_WELL_BAKED_BODY;
        numAbsorbingAbilities = 2;
    }
    else if (gBattleMoves[gLastMoves[gBattlerTarget]].type == TYPE_WATER)
        {
        absorbingTypeAbilities[0] = ABILITY_WATER_ABSORB;
        absorbingTypeAbilities[1] = ABILITY_STORM_DRAIN;
        absorbingTypeAbilities[2] = ABILITY_DRY_SKIN;
        numAbsorbingAbilities = 3;
    }
    else if (gBattleMoves[gLastMoves[gBattlerTarget]].type == TYPE_ELECTRIC)
        {
        absorbingTypeAbilities[0] = ABILITY_VOLT_ABSORB;
        absorbingTypeAbilities[1] = ABILITY_MOTOR_DRIVE;
        absorbingTypeAbilities[2] = ABILITY_LIGHTNING_ROD;
        numAbsorbingAbilities = 3;
    }
    else if (gBattleMoves[gLastMoves[gBattlerTarget]].type == TYPE_GRASS)
    {
        absorbingTypeAbilities[0] = ABILITY_SAP_SIPPER;
        numAbsorbingAbilities = 1;
    }
    else
        return FALSE;


    // Check current mon for all absorbing abilities
    for (i = 0; i < numAbsorbingAbilities; i++)
    {
        if (AI_DATA->abilities[gActiveBattler] == absorbingTypeAbilities[i])
            return FALSE;
    }

    GetAIPartyIndexes(gActiveBattler, &firstId, &lastId);

    if (GetBattlerSide(gActiveBattler) == B_SIDE_PLAYER)
        party = gPlayerParty;
    else
        party = gEnemyParty;

    for (i = firstId; i < lastId; i++)
    {
        u16 monAbility;

        if (!IsValidForBattle(&party[i]))
            continue;
        if (i == gBattlerPartyIndexes[battlerIn1])
            continue;
        if (i == gBattlerPartyIndexes[battlerIn2])
            continue;
        if (i == *(gBattleStruct->monToSwitchIntoId + battlerIn1))
            continue;
        if (i == *(gBattleStruct->monToSwitchIntoId + battlerIn2))
            continue;
        if (IsAceMon(gActiveBattler, i))
            continue;

        monAbility = GetMonAbility(&party[i]);

        for (j = 0; j < numAbsorbingAbilities; j++) {
            if (absorbingTypeAbilities[j] == monAbility 
                && AI_RandLessThan(192)
                && AI_CalcPartyMonScore(gActiveBattler, gBattlerTarget, &party[i], NULL, TRUE) >= 0)
            {
                // we found a mon.
                *(gBattleStruct->AI_monToSwitchIntoId + gActiveBattler) = i;
                BtlController_EmitTwoReturnValues(1, B_ACTION_SWITCH, 0);
                return TRUE;
            }
        }
    }

    return FALSE;
}

static bool8 FindMonThatsImmuneToOpponentsMove(void)
{
    u8 battlerIn1, battlerIn2;
    u8 numImmunityAbilities = 0; 
    u16 immunityAbilities[3]; // Array size is maximum number of absorbing abilities for a single type
    s32 firstId;
    s32 lastId; // + 1
    struct Pokemon *party;
    s32 i, j;

    if (gLastMoves[gBattlerTarget] == MOVE_NONE)
        return FALSE;
    if (gLastMoves[gBattlerTarget] == MOVE_UNAVAILABLE)
        return FALSE;
    if (gBattleMoves[gLastMoves[gBattlerTarget]].target == MOVE_TARGET_USER 
        || gBattleMoves[gLastMoves[gBattlerTarget]].target == MOVE_TARGET_OPPONENTS_FIELD
        || gBattleMoves[gLastMoves[gBattlerTarget]].target == MOVE_TARGET_ALLY
        || gBattleMoves[gLastMoves[gBattlerTarget]].target == MOVE_TARGET_ALL_BATTLERS)
        return FALSE;
    if (gDisableStructs[gActiveBattler].isFirstTurn)
        return FALSE;

    if (gBattleTypeFlags & BATTLE_TYPE_DOUBLE)
    {
        battlerIn1 = gActiveBattler;
        if (gAbsentBattlerFlags & gBitTable[GetBattlerAtPosition(BATTLE_PARTNER(GetBattlerPosition(gActiveBattler)))])
            battlerIn2 = gActiveBattler;
        else
            battlerIn2 = GetBattlerAtPosition(BATTLE_PARTNER(GetBattlerPosition(gActiveBattler)));
    }
    else
    {
        battlerIn1 = gActiveBattler;
        battlerIn2 = gActiveBattler;
    }

    // Create an array of possible absorb abilities so the AI considers all of them
    if (gBattleMoves[gLastMoves[gBattlerTarget]].windMove)
        {
        immunityAbilities[0] = ABILITY_WIND_RIDER;
        numImmunityAbilities = 1;
    }
    else
        return FALSE;


    // Check current mon for all absorbing abilities
    for (i = 0; i < numImmunityAbilities; i++)
    {
        if (AI_DATA->abilities[gActiveBattler] == immunityAbilities[i])
            return FALSE;
    }

    GetAIPartyIndexes(gActiveBattler, &firstId, &lastId);

    if (GetBattlerSide(gActiveBattler) == B_SIDE_PLAYER)
        party = gPlayerParty;
    else
        party = gEnemyParty;

    for (i = firstId; i < lastId; i++)
    {
        u16 monAbility;

        if (!IsValidForBattle(&party[i]))
            continue;
        if (i == gBattlerPartyIndexes[battlerIn1])
            continue;
        if (i == gBattlerPartyIndexes[battlerIn2])
            continue;
        if (i == *(gBattleStruct->monToSwitchIntoId + battlerIn1))
            continue;
        if (i == *(gBattleStruct->monToSwitchIntoId + battlerIn2))
            continue;

        monAbility = GetMonAbility(&party[i]);

        for (j = 0; j < numImmunityAbilities; j++) {
            if (immunityAbilities[j] == monAbility 
                && AI_RandLessThan(192)
                && AI_CalcPartyMonScore(gActiveBattler, gBattlerTarget, &party[i], NULL, TRUE) >= 0)
            {
                // we found a mon.
                *(gBattleStruct->AI_monToSwitchIntoId + gActiveBattler) = i;
                BtlController_EmitTwoReturnValues(1, B_ACTION_SWITCH, 0);
                return TRUE;
            }
        }
    }

    return FALSE;
}

static bool8 HasValidSwitchCandidate(u8 battler, s32 *firstId, s32 *lastId, struct Pokemon **party)
{
    s32 i;

    u8 battlerIn1 = battler;
    u8 battlerIn2 = battler;

    GetAIPartyIndexes(battler, firstId, lastId);

    if (GetBattlerSide(battler) == B_SIDE_PLAYER)
        *party = gPlayerParty;
    else
        *party = gEnemyParty;

    // Determine partner (for doubles)
    if (IsDoubleBattle())
    {
        u8 partnerPos = BATTLE_PARTNER(GetBattlerPosition(battler));

        if (!(gAbsentBattlerFlags & gBitTable[partnerPos]))
            battlerIn2 = GetBattlerAtPosition(partnerPos);
    }

    for (i = *firstId; i < *lastId; i++)
    {
        // Must be usable
        if (!IsValidForBattle(&(*party)[i]))
            continue;

        // Cannot switch into active mons
        if (i == gBattlerPartyIndexes[battlerIn1]
            || i == gBattlerPartyIndexes[battlerIn2])
            continue;

        // Cannot switch into already chosen mons (double AI coordination)
        if (i == *(gBattleStruct->monToSwitchIntoId + battlerIn1)
            || i == *(gBattleStruct->monToSwitchIntoId + battlerIn2))
            continue;

        return TRUE;
    }

    return FALSE;
}

static void FindBestSwitchCandidate(u8 battler, u8 opposingBattler, s32 firstId, s32 lastId, struct Pokemon *party, s32 *chosenMonId, s32 *bestScore, bool8 allowVariance)
{
    s32 i;
    s32 score;

    u8 battlerIn1 = battler;
    u8 battlerIn2 = battler;

    bool8 foundCandidate = FALSE;

    // Determine partner battler (for doubles)
    if (IsDoubleBattle())
    {
        u8 partnerPos = BATTLE_PARTNER(GetBattlerPosition(battler));

        if (!(gAbsentBattlerFlags & gBitTable[partnerPos]))
            battlerIn2 = GetBattlerAtPosition(partnerPos);
    }

    *bestScore = -2;
    *chosenMonId = firstId;

    for (i = firstId; i < lastId; i++)
    {
        // Skip invalid mons
        if (!IsValidForBattle(&party[i]))
            continue;

        // Skip mons already on the field (self + partner)
        if (i == gBattlerPartyIndexes[battlerIn1]
            || i == gBattlerPartyIndexes[battlerIn2])
            continue;

        // Skip mons already chosen by AI this turn (prevents duplicate switches in doubles)
        if (i == *(gBattleStruct->monToSwitchIntoId + battlerIn1)
            || i == *(gBattleStruct->monToSwitchIntoId + battlerIn2))
            continue;

        foundCandidate = TRUE;
        score = AI_CalcPartyMonScore(battler, opposingBattler, &party[i], NULL, TRUE);

        if (score > *bestScore)
        {
            *bestScore = score;
            *chosenMonId = i;
        }
        else if (score == *bestScore && allowVariance && AI_RandLessThan(128)) // 50% variance
        {
            *chosenMonId = i;
        }
    }
    if (!foundCandidate)
    {
        *bestScore = -2;
        *chosenMonId = PARTY_SIZE; // invalid sentinel
    }
}

static bool8 ShouldSwitchIfGameStatePrompt(void)
{
    bool8 switchMon = FALSE;
    bool8 avoidSwitch = FALSE;
    bool8 hasHighScore = FALSE;
    bool8 ignoreScore = FALSE;
    bool8 allowVariance = FALSE;

    u8 battler = gActiveBattler;
    u8 opposingBattler = GetBattlerAtPosition(BATTLE_OPPOSITE(GetBattlerPosition(battler)));

    u16 monAbility = AI_DATA->abilities[battler];
    bool8 isFirstTurn = gDisableStructs[battler].isFirstTurn;

    s32 firstId, lastId;
    struct Pokemon *party;

    //Cheap check if switching is even possible
    if (!HasValidSwitchCandidate(battler, &firstId, &lastId, &party))
        return FALSE;

    //Avoid switching conditions
    if (AnyStatIsRaised(battler)
        || CanTargetFaintAi(battler, gBattlerTarget)
        || (IsDoubleBattle() && CanTargetFaintAi(battler, BATTLE_PARTNER(gBattlerTarget)))
        || HasMoveEffect(gBattlerTarget, EFFECT_ROAR)
        || (IsDoubleBattle() && HasMoveEffect(BATTLE_PARTNER(gBattlerTarget), EFFECT_ROAR))
        || HasMoveEffect(gBattlerTarget, EFFECT_HIT_SWITCH_TARGET)
        || (IsDoubleBattle() && HasMoveEffect(BATTLE_PARTNER(gBattlerTarget), EFFECT_HIT_SWITCH_TARGET))
        || HasMoveEffect(gBattlerTarget, EFFECT_PURSUIT)
        || (IsDoubleBattle() && HasMoveEffect(BATTLE_PARTNER(gBattlerTarget), EFFECT_PURSUIT)))
    {
        avoidSwitch = TRUE;
    }

    //Guaranteed switches

    // Perish Song
    if ((gStatuses3[battler] & STATUS3_PERISH_SONG)
        && gDisableStructs[battler].perishSongTimer == 0
        && monAbility != ABILITY_SOUNDPROOF)
    {
        switchMon = TRUE;
    }

    // Palafin
    if (!switchMon
        && gBattleMons[battler].species == SPECIES_PALAFIN
        && CanTargetFaintAi(gBattlerTarget, battler)
        && AI_WhosFaster(battler, gBattlerTarget) == AI_IS_SLOWER)
    {
        switchMon = TRUE;
    }

    //Conditional switching ----
    if (!switchMon)
    {
        // Anti Setup - 40%
        if (!(gStatuses3[opposingBattler] & STATUS3_SEMI_INVULNERABLE)
            && gDisableStructs[battler].numTurns > 1
            && AI_RandLessThan(102))
        {
            s32 i;
            for (i = 0; i < MAX_MON_MOVES; i++)
            {
                if (gBattleStruct->aiFinalScore[battler][opposingBattler][i] > 95)
                {
                    hasHighScore = TRUE;
                    break;
                }
            }

            if (!hasHighScore)
            {
                switchMon = TRUE;
                allowVariance = TRUE;
            }
        }

        // Anti Setup Extreme - 40%
        if (!switchMon
            && (gDisableStructs[battler].numTurns > 25) //have to stay in for 25 turns, can switch starting 21'th
            && AI_RandLessThan(102))
        {
            switchMon = TRUE;
            allowVariance = TRUE;
            ignoreScore = TRUE;
        }

        // Walled AI - 20%
        if (!switchMon
            && gDisableStructs[battler].numTurns > 5
            && !IsDoubleBattle()
            && !avoidSwitch
            && AI_RandLessThan(51))
        {
            u32 highestDmg = GetHighestDmg(battler, gBattlerTarget);
            u32 permilleDmg = highestDmg * 1000 / gBattleMons[gBattlerTarget].maxHP;

            u32 stallThreshold = 100;
            if (AI_DATA->holdEffects[gBattlerTarget] == HOLD_EFFECT_LEFTOVERS
                || (AI_DATA->holdEffects[gBattlerTarget] == HOLD_EFFECT_BLACK_SLUDGE
                    && IS_BATTLER_OF_TYPE(gBattlerTarget, TYPE_POISON))
                || (AI_DATA->abilities[gBattlerTarget] == ABILITY_POISON_HEAL
                    && (gBattleMons[gBattlerTarget].status1 & STATUS1_PSN_ANY))
                || ((gStatuses3[battler] & STATUS3_LEECHSEED)
                    && monAbility != ABILITY_MAGIC_GUARD))
            {
                stallThreshold = 200;
            }

            if (permilleDmg <= stallThreshold)
            {
                switchMon = TRUE;
            }
        }

        // Support AI - 20%
        if (!switchMon
            && (MonIsDedicatedSupport(battler) || WeatherTerrainMonShouldSwitch(battler) || gDisableStructs[battler].numTurns > 12)
            && gDisableStructs[battler].numTurns > 1
            && !avoidSwitch
            && AI_RandLessThan(51))
        {
            switchMon = TRUE;
        }

        // Regenerator - 40%
        if (!switchMon
            && monAbility == ABILITY_REGENERATOR
            && gBattleMons[battler].hp < gBattleMons[battler].maxHP * 2 / 3
            && !avoidSwitch
            && AI_RandLessThan(102))
        {
            switchMon = TRUE;
        }
    }

    // ---- STEP 4: Execute switch (ONLY NOW compute best mon) ----
    if (switchMon)
    {
        s32 chosenMonId;
        s32 bestScore;

        FindBestSwitchCandidate(battler, opposingBattler, firstId, lastId, party, &chosenMonId, &bestScore, allowVariance);

        // Preserve original safety: don't switch into garbage
        if ((bestScore >= 0 || ignoreScore) && chosenMonId < PARTY_SIZE)
        {
            *(gBattleStruct->AI_monToSwitchIntoId + battler) = chosenMonId;
            BtlController_EmitTwoReturnValues(BUFFER_B, B_ACTION_SWITCH, 0);
            return TRUE;
        }
    }

    return FALSE;
}

static bool8 HasSuperEffectiveMoveAgainstOpponents(bool8 noRng)
{
    u8 opposingPosition;
    u8 opposingBattler;
    s32 i;
    u16 move;

    opposingPosition = BATTLE_OPPOSITE(GetBattlerPosition(gActiveBattler));
    opposingBattler = GetBattlerAtPosition(opposingPosition);

    if (!(gAbsentBattlerFlags & gBitTable[opposingBattler]))
    {
        for (i = 0; i < MAX_MON_MOVES; i++)
        {
            move = gBattleMons[gActiveBattler].moves[i];
            if (move == MOVE_NONE)
                continue;

            if (AI_GetTypeEffectiveness(move, gActiveBattler, opposingBattler) >= UQ_4_12(2.0))
            {
                if (noRng)
                    return TRUE;
                if (Random() % 10 != 0)
                    return TRUE;
            }
        }
    }
    if (!(gBattleTypeFlags & BATTLE_TYPE_DOUBLE))
        return FALSE;

    opposingBattler = GetBattlerAtPosition(BATTLE_PARTNER(opposingPosition));

    if (!(gAbsentBattlerFlags & gBitTable[opposingBattler]))
    {
        for (i = 0; i < MAX_MON_MOVES; i++)
        {
            move = gBattleMons[gActiveBattler].moves[i];
            if (move == MOVE_NONE)
                continue;

            if (AI_GetTypeEffectiveness(move, gActiveBattler, opposingBattler) >= UQ_4_12(2.0))
            {
                if (noRng)
                    return TRUE;
                if (Random() % 10 != 0)
                    return TRUE;
            }
        }
    }

    return FALSE;
}

static bool8 AreStatsRaised(void)
{
    u8 buffedStatsValue = 0;
    s32 i;

    for (i = 0; i < NUM_BATTLE_STATS; i++)
    {
        if (gBattleMons[gActiveBattler].statStages[i] > DEFAULT_STAT_STAGE)
            buffedStatsValue += gBattleMons[gActiveBattler].statStages[i] - DEFAULT_STAT_STAGE;
    }

    return (buffedStatsValue > 3);
}

static bool8 FindMonWithFlagsAndSuperEffective(u16 flags, u8 moduloPercent)
{
    u8 battlerIn1, battlerIn2;
    s32 firstId;
    s32 lastId; // + 1
    struct Pokemon *party;
    s32 i, j;
    u16 move;

    if (gLastLandedMoves[gActiveBattler] == MOVE_NONE)
        return FALSE;
    if (gLastLandedMoves[gActiveBattler] == MOVE_UNAVAILABLE)
        return FALSE;
    if (gLastHitBy[gActiveBattler] == 0xFF)
        return FALSE;
    if (IS_MOVE_STATUS(gLastLandedMoves[gActiveBattler]))
        return FALSE;

    if (gBattleTypeFlags & BATTLE_TYPE_DOUBLE)
    {
        battlerIn1 = gActiveBattler;
        if (gAbsentBattlerFlags & gBitTable[GetBattlerAtPosition(BATTLE_PARTNER(GetBattlerPosition(gActiveBattler)))])
            battlerIn2 = gActiveBattler;
        else
            battlerIn2 = GetBattlerAtPosition(BATTLE_PARTNER(GetBattlerPosition(gActiveBattler)));
    }
    else
    {
        battlerIn1 = gActiveBattler;
        battlerIn2 = gActiveBattler;
    }

    GetAIPartyIndexes(gActiveBattler, &firstId, &lastId);

    if (GetBattlerSide(gActiveBattler) == B_SIDE_PLAYER)
        party = gPlayerParty;
    else
        party = gEnemyParty;

    for (i = firstId; i < lastId; i++)
    {
        u16 species, monAbility;

        if (!IsValidForBattle(&party[i]))
            continue;
        if (i == gBattlerPartyIndexes[battlerIn1])
            continue;
        if (i == gBattlerPartyIndexes[battlerIn2])
            continue;
        if (i == *(gBattleStruct->monToSwitchIntoId + battlerIn1))
            continue;
        if (i == *(gBattleStruct->monToSwitchIntoId + battlerIn2))
            continue;
        if (IsAceMon(gActiveBattler, i))
            continue;

        species = GetMonData(&party[i], MON_DATA_SPECIES_OR_EGG);
        monAbility = GetMonAbility(&party[i]);

        CalcPartyMonTypeEffectivenessMultiplier(gLastLandedMoves[gActiveBattler], species, monAbility);
        if (gMoveResultFlags & flags)
        {
            battlerIn1 = gLastHitBy[gActiveBattler];

            for (j = 0; j < MAX_MON_MOVES; j++)
            {
                move = GetMonData(&party[i], MON_DATA_MOVE1 + j);
                if (move == 0)
                    continue;

                if (AI_GetTypeEffectiveness(move, gActiveBattler, battlerIn1) >= UQ_4_12(2.0) && Random() % moduloPercent == 0)
                {
                    *(gBattleStruct->AI_monToSwitchIntoId + gActiveBattler) = i;
                    BtlController_EmitTwoReturnValues(BUFFER_B, B_ACTION_SWITCH, 0);
                    return TRUE;
                }
            }
        }
    }

    return FALSE;
}

bool32 ShouldSwitch(void)
{
    u8 battlerIn1, battlerIn2;
    s32 firstId;
    s32 lastId; // + 1
    struct Pokemon *party;
    s32 i;
    s32 availableToSwitch;
    bool32 hasAceMon = FALSE;

    //return FALSE; //For now I don't want non trivial switches. Trivial switches handled in battle_ai_main

    if (gBattleMons[gActiveBattler].status2 & (STATUS2_WRAPPED | STATUS2_ESCAPE_PREVENTION))
        return FALSE;
    if (gStatuses3[gActiveBattler] & STATUS3_ROOTED)
        return FALSE;
    if (IsAbilityPreventingEscape(gActiveBattler))
        return FALSE;
    if (CountUsablePartyMons(gActiveBattler) == 0)
        return FALSE;
    if (gBattleTypeFlags & BATTLE_TYPE_ARENA)
        return FALSE;
    if (gBattleStruct->commanderActive[gActiveBattler])
        return FALSE;

    availableToSwitch = 0;

    if (gBattleTypeFlags & BATTLE_TYPE_DOUBLE)
    {
        battlerIn1 = gActiveBattler;
        if (gAbsentBattlerFlags & gBitTable[GetBattlerAtPosition(BATTLE_PARTNER(GetBattlerPosition(gActiveBattler)))])
            battlerIn2 = gActiveBattler;
        else
            battlerIn2 = GetBattlerAtPosition(BATTLE_PARTNER(GetBattlerPosition(gActiveBattler)));
    }
    else
    {
        battlerIn1 = gActiveBattler;
        battlerIn2 = gActiveBattler;
    }

    GetAIPartyIndexes(gActiveBattler, &firstId, &lastId);

    if (GetBattlerSide(gActiveBattler) == B_SIDE_PLAYER)
        party = gPlayerParty;
    else
        party = gEnemyParty;

    for (i = firstId; i < lastId; i++)
    {
        if (!IsValidForBattle(&party[i]))
            continue;
        if (i == gBattlerPartyIndexes[battlerIn1])
            continue;
        if (i == gBattlerPartyIndexes[battlerIn2])
            continue;
        if (i == *(gBattleStruct->monToSwitchIntoId + battlerIn1))
            continue;
        if (i == *(gBattleStruct->monToSwitchIntoId + battlerIn2))
            continue;
        if (IsAceMon(gActiveBattler, i) && FALSE) //I dont want this bs
        {
            hasAceMon = TRUE;
            continue;
        }

        availableToSwitch++;
    }

    if (availableToSwitch == 0)
    {
        if (hasAceMon) // If the ace mon is the only available mon, use it
            availableToSwitch++;
        else
            return FALSE;
    }

    //NOTE: The sequence of the below functions matter! Do not change unless you have carefully considered the outcome.
    //Since the order is sequencial, and some of these functions prompt switch to specific party members. - WIP!!

    //These Functions can prompt switch to specific party members
    if (ShouldSwitchIfWonderGuard())
        return TRUE;
    if (ShouldSwitchIfGameStatePrompt())
        return TRUE;
    if (FindMonThatAbsorbsOpponentsMove())
        return TRUE;
    if (FindMonThatsImmuneToOpponentsMove())
        return TRUE;

    //These Functions can prompt switch to generic party members
    //if (ShouldSwitchIfAllBadMoves())
        //return TRUE;

    //Removing switch capabilites under specific conditions
    //These Functions prevent the "FindMonWithFlagsAndSuperEffective" from getting out of hand.
    //if (HasSuperEffectiveMoveAgainstOpponents(FALSE))
        //return FALSE;
    //if (AreStatsRaised())
        //return FALSE;

    //Default Function
    //Can prompt switch if AI has a pokemon in party that resists current opponent & has super effective move
    //if (FindMonWithFlagsAndSuperEffective(MOVE_RESULT_DOESNT_AFFECT_FOE, 2)
        //|| FindMonWithFlagsAndSuperEffective(MOVE_RESULT_NOT_VERY_EFFECTIVE, 3))
        //return TRUE;

    return FALSE;
}

void AI_TrySwitchOrUseItem(void)
{
    struct Pokemon *party;
    u8 battlerIn1, battlerIn2;
    s32 firstId;
    s32 lastId; // + 1
    u8 battlerIdentity = GetBattlerPosition(gActiveBattler);

    if (GetBattlerSide(gActiveBattler) == B_SIDE_PLAYER)
        party = gPlayerParty;
    else
        party = gEnemyParty;

    if (gBattleTypeFlags & BATTLE_TYPE_TRAINER)
    {
        if (ShouldSwitch())
        {
            if (*(gBattleStruct->AI_monToSwitchIntoId + gActiveBattler) == PARTY_SIZE)
            {
                s32 monToSwitchId = GetMostSuitableMonToSwitchInto(TRUE);
                if (monToSwitchId == PARTY_SIZE)
                {
                    if (!(gBattleTypeFlags & BATTLE_TYPE_DOUBLE))
                    {
                        battlerIn1 = GetBattlerAtPosition(battlerIdentity);
                        battlerIn2 = battlerIn1;
                    }
                    else
                    {
                        battlerIn1 = GetBattlerAtPosition(battlerIdentity);
                        battlerIn2 = GetBattlerAtPosition(BATTLE_PARTNER(battlerIdentity));
                    }

                    GetAIPartyIndexes(gActiveBattler, &firstId, &lastId);

                    for (monToSwitchId = (lastId-1); monToSwitchId >= firstId; monToSwitchId--)
                    {
                        if (!IsValidForBattle(&party[monToSwitchId]))
                            continue;
                        if (monToSwitchId == gBattlerPartyIndexes[battlerIn1])
                            continue;
                        if (monToSwitchId == gBattlerPartyIndexes[battlerIn2])
                            continue;
                        if (monToSwitchId == *(gBattleStruct->monToSwitchIntoId + battlerIn1))
                            continue;
                        if (monToSwitchId == *(gBattleStruct->monToSwitchIntoId + battlerIn2))
                            continue;
                        if (IsAceMon(gActiveBattler, monToSwitchId))
                            continue;

                        break;
                    }
                }

                *(gBattleStruct->AI_monToSwitchIntoId + gActiveBattler) = monToSwitchId;
            }

            *(gBattleStruct->monToSwitchIntoId + gActiveBattler) = *(gBattleStruct->AI_monToSwitchIntoId + gActiveBattler);
            return;
        }
        else if (ShouldUseItem())
        {
            return;
        }
    }

    BtlController_EmitTwoReturnValues(BUFFER_B, B_ACTION_USE_MOVE, BATTLE_OPPOSITE(gActiveBattler) << 8);
}

// If there are two(or more) mons to choose from, always choose one that has baton pass
// as most often it can't do much on its own.
static u32 GetBestMonBatonPass(struct Pokemon *party, int firstId, int lastId, u8 invalidMons, int aliveCount, u32 opposingBattler)
{
    int i, j, bits = 0;

    for (i = firstId; i < lastId; i++)
    {
        if (invalidMons & gBitTable[i])
            continue;
        if (IsAiPartyMonOHKOBy(opposingBattler, &party[i]))
            continue;

        for (j = 0; j < MAX_MON_MOVES; j++)
        {
            if (GetMonData(&party[i], MON_DATA_MOVE1 + j, NULL) == MOVE_BATON_PASS)
            {
                bits |= gBitTable[i];
                break;
            }
        }
    }

    if ((aliveCount == 2 || (aliveCount > 2 && Random() % 3 == 0)) && bits)
    {
        do
        {
            i = (Random() % (lastId - firstId)) + firstId;
        } while (!(bits & gBitTable[i]));
        return i;
    }

    return PARTY_SIZE;
}

static u32 GetBestMonTypeMatchup(struct Pokemon *party, int firstId, int lastId, u8 invalidMons, u32 opposingBattler)
{
    int i, bits = 0;

    while (bits != 0x3F) // All mons were checked.
    {
        uq4_12_t bestResist = UQ_4_12(1.0);
        int bestMonId = PARTY_SIZE;
        // Find the mon whose type is the most suitable defensively.
        for (i = firstId; i < lastId; i++)
        {
            if (!(gBitTable[i] & invalidMons) && !(gBitTable[i] & bits))
            {
                u16 species = GetMonData(&party[i], MON_DATA_SPECIES);
                uq4_12_t typeEffectiveness = UQ_4_12(1.0);

                u8 atkType1 = gBattleMons[opposingBattler].type1;
                u8 atkType2 = gBattleMons[opposingBattler].type2;
                u8 defType1 = gSpeciesInfo[species].types[0];
                u8 defType2 = gSpeciesInfo[species].types[1];

                if (IsAiPartyMonOHKOBy(opposingBattler, &party[i]))
                    continue;

                typeEffectiveness = uq4_12_multiply(typeEffectiveness, (GetTypeModifier(atkType1, defType1)));
                if (atkType2 != atkType1)
                    typeEffectiveness = uq4_12_multiply(typeEffectiveness, (GetTypeModifier(atkType2, defType1)));
                if (defType2 != defType1)
                {
                    typeEffectiveness = uq4_12_multiply(typeEffectiveness, (GetTypeModifier(atkType1, defType2)));
                    if (atkType2 != atkType1)
                        typeEffectiveness = uq4_12_multiply(typeEffectiveness, (GetTypeModifier(atkType2, defType2)));
                }
                if (typeEffectiveness < bestResist)
                {
                    bestResist = typeEffectiveness;
                    bestMonId = i;
                }
            }
        }

        // Ok, we know the mon has the right typing but does it have at least one super effective move?
        if (bestMonId != PARTY_SIZE)
        {
            for (i = 0; i < MAX_MON_MOVES; i++)
            {
                u32 move = GetMonData(&party[bestMonId], MON_DATA_MOVE1 + i);
                if (move != MOVE_NONE && AI_GetTypeEffectiveness(move, gActiveBattler, opposingBattler) >= UQ_4_12(2.0))
                    break;
            }

            if (i != MAX_MON_MOVES)
                return bestMonId; // Has both the typing and at least one super effective move.

            bits |= gBitTable[bestMonId]; // Sorry buddy, we want something better.
        }
        else
        {
            bits = 0x3F; // No viable mon to switch.
        }
    }

    return PARTY_SIZE;
}

static u32 GetBestMonDmg(struct Pokemon *party, int firstId, int lastId, u8 invalidMons, u32 opposingBattler)
{
    int i, j;
    int dmg, bestDmg, dmgTaken = 0;
    int bestMonId = PARTY_SIZE;

    gMoveResultFlags = 0;
    // If we couldn't find the best mon in terms of typing, find the one that deals most damage.
    for (i = firstId; i < lastId; i++)
    {
        if (gBitTable[i] & invalidMons)
            continue;

        if (IsAiPartyMonOHKOBy(opposingBattler, &party[i]))
            continue;

        dmg = AI_CalcPartyMonBestMoveDamage(gActiveBattler, opposingBattler, &party[i], NULL);
        if (bestDmg < dmg)
        {
            bestDmg = dmg;
            bestMonId = i;
        }
    }

    return bestMonId;
}

static inline bool32 IsFreeSwitch(u32 battlerSwitchingOut, u32 opposingBattler)
{
    bool32 movedSecond = GetBattlerTurnOrderNum(battlerSwitchingOut) > GetBattlerTurnOrderNum(opposingBattler) ? TRUE : FALSE;
    u8 moveEffect = gBattleMoves[gCurrentMove].effect;

    // Switch out effects
    if (moveEffect == EFFECT_CHILLY_RECEPTION || moveEffect == EFFECT_TELEPORT)
        return TRUE;
    if ((moveEffect == EFFECT_HIT_ESCAPE || moveEffect == EFFECT_PARTING_SHOT || moveEffect == EFFECT_BATON_PASS) && movedSecond)
        return TRUE;
    if (AI_DATA->ejectPackSwitch && movedSecond)
        return TRUE;
    if (AI_DATA->ejectButtonSwitch)
        return TRUE;

    return FALSE;
}

static u32 GetBestMonScore(struct Pokemon *party, int firstId, int lastId, u8 invalidMons, u32 opposingBattler, bool32 hardSwitch)
{
    int i, j;
    s32 score, bestScore = -1;
    int bestMonId = PARTY_SIZE;
    u32 storeCurrBattlerPartyIndex = gBattlerPartyIndexes[gActiveBattler];

    gMoveResultFlags = 0;
    if (!hardSwitch)
    {
        AI_DATA->switchInCalc = TRUE; //For Retaliate
        if (gSideTimers[B_SIDE_OPPONENT].retaliateTimer != 2 && !IsFreeSwitch(gActiveBattler, opposingBattler))
            hardSwitch = TRUE;
    }
    for (i = firstId; i < lastId; i++)
    {
        if (gBitTable[i] & invalidMons)
            continue;

        // Edge case for Rage Fist
        gBattlerPartyIndexes[gActiveBattler] = i;

        score = AI_CalcPartyMonScore(gActiveBattler, opposingBattler, &party[i], NULL, hardSwitch);
        if (bestScore < score)
        {
            bestScore = score;
            bestMonId = i;
        }

        // [Restore] Edge case for Rage Fist
        gBattlerPartyIndexes[gActiveBattler] = storeCurrBattlerPartyIndex;
    }
    AI_DATA->switchInCalc = FALSE;
    return bestMonId;
}

//Special case for the Tate&Liza 6v8
u8 GetMostSuitableMonToSwitchIntoSpecial(void)
{
    u32 opposingBattler = 0;
    u32 bestMonId = PARTY_SIZE;
    u8 battlerIn1 = 0, battlerIn2 = 0;
    s32 firstId = 0;
    s32 lastId = 0; // + 1
    struct Pokemon *party;
    s32 i, j, aliveCount = 0;
    u32 invalidMons = 0, aceMonId = PARTY_SIZE;

    if (*(gBattleStruct->monToSwitchIntoId + gActiveBattler) != PARTY_SIZE)
        return *(gBattleStruct->monToSwitchIntoId + gActiveBattler);
    if (gBattleTypeFlags & BATTLE_TYPE_ARENA)
        return gBattlerPartyIndexes[gActiveBattler] + 1;

    if (gBattleTypeFlags & BATTLE_TYPE_DOUBLE)
    {
        battlerIn1 = gActiveBattler;
        if (gAbsentBattlerFlags & gBitTable[GetBattlerAtPosition(BATTLE_PARTNER(GetBattlerPosition(gActiveBattler)))])
            battlerIn2 = gActiveBattler;
        else
            battlerIn2 = GetBattlerAtPosition(BATTLE_PARTNER(GetBattlerPosition(gActiveBattler)));

        opposingBattler = BATTLE_OPPOSITE(battlerIn1);
        if (gAbsentBattlerFlags & gBitTable[opposingBattler])
            opposingBattler ^= BIT_FLANK;
    }
    else
    {
        opposingBattler = GetBattlerAtPosition(BATTLE_OPPOSITE(GetBattlerPosition(gActiveBattler)));
        battlerIn1 = gActiveBattler;
        battlerIn2 = gActiveBattler;
    }

    GetAIPartyIndexes(gActiveBattler, &firstId, &lastId);

    if (GetBattlerSide(gActiveBattler) == B_SIDE_PLAYER)
        party = gPlayerParty;
    else
        party = gEnemyParty;

    // Get invalid slots ids.
    for (i = firstId; i < lastId; i++)
    {
        if (!IsValidForBattle(&party[i]))
        {
            invalidMons |= gBitTable[i];
        }
        else
        {
            aliveCount++;
        }
    }

    bestMonId = GetBestMonScore(party, firstId, lastId, invalidMons, opposingBattler, FALSE);
    if (bestMonId != PARTY_SIZE)
        return bestMonId;

    return PARTY_SIZE;
}

u8 GetMostSuitableMonToSwitchInto(bool32 hardSwitch)
{
    u32 opposingBattler = 0;
    u32 bestMonId = PARTY_SIZE;
    u8 battlerIn1 = 0, battlerIn2 = 0;
    s32 firstId = 0;
    s32 lastId = 0; // + 1
    struct Pokemon *party;
    s32 i, j, aliveCount = 0;
    u32 invalidMons = 0, aceMonId = PARTY_SIZE;

    if (*(gBattleStruct->monToSwitchIntoId + gActiveBattler) != PARTY_SIZE)
        return *(gBattleStruct->monToSwitchIntoId + gActiveBattler);
    if (gBattleTypeFlags & BATTLE_TYPE_ARENA)
        return gBattlerPartyIndexes[gActiveBattler] + 1;

    if (gBattleTypeFlags & BATTLE_TYPE_DOUBLE)
    {
        battlerIn1 = gActiveBattler;
        if (gAbsentBattlerFlags & gBitTable[GetBattlerAtPosition(BATTLE_PARTNER(GetBattlerPosition(gActiveBattler)))])
            battlerIn2 = gActiveBattler;
        else
            battlerIn2 = GetBattlerAtPosition(BATTLE_PARTNER(GetBattlerPosition(gActiveBattler)));

        opposingBattler = BATTLE_OPPOSITE(battlerIn1);
        if (gAbsentBattlerFlags & gBitTable[opposingBattler])
            opposingBattler ^= BIT_FLANK;
    }
    else
    {
        opposingBattler = GetBattlerAtPosition(BATTLE_OPPOSITE(GetBattlerPosition(gActiveBattler)));
        battlerIn1 = gActiveBattler;
        battlerIn2 = gActiveBattler;
    }

    GetAIPartyIndexes(gActiveBattler, &firstId, &lastId);

    if (GetBattlerSide(gActiveBattler) == B_SIDE_PLAYER)
        party = gPlayerParty;
    else
        party = gEnemyParty;

    // Get invalid slots ids.
    for (i = firstId; i < lastId; i++)
    {
        if (!IsValidForBattle(&party[i])
            || gBattlerPartyIndexes[battlerIn1] == i
            || gBattlerPartyIndexes[battlerIn2] == i
            || i == *(gBattleStruct->monToSwitchIntoId + battlerIn1)
            || i == *(gBattleStruct->monToSwitchIntoId + battlerIn2))
        {
            invalidMons |= gBitTable[i];
        }
        else
        {
            aliveCount++;
        }
    }

    bestMonId = GetBestMonScore(party, firstId, lastId, invalidMons, opposingBattler, hardSwitch);
    if (bestMonId != PARTY_SIZE)
        return bestMonId;

    return PARTY_SIZE;
}

static bool32 AiExpectsToFaintPlayer(void)
{
    bool32 canFaintPlayer;
    u32 i;
    u8 target = gBattleStruct->aiChosenTarget[gActiveBattler];

    if (gBattleStruct->aiMoveOrAction[gActiveBattler] > 3)
        return FALSE; // AI not planning to use move

    if (GetBattlerSide(target) != GetBattlerSide(gActiveBattler)
      && CanIndexMoveFaintTarget(gActiveBattler, target, gBattleStruct->aiMoveOrAction[gActiveBattler], 0)
      && AI_WhoStrikesFirst(gActiveBattler, target, GetAIChosenMove(gActiveBattler)) == AI_IS_FASTER) {
        // We expect to faint the target and move first -> dont use an item
        return TRUE;
    }

    return FALSE;
}

static bool8 ShouldUseItem(void)
{
    struct Pokemon *party;
    s32 i;
    u8 validMons = 0;
    bool8 shouldUse = FALSE;

    // If teaming up with player and Pokemon is on the right, or Pokemon is currently held by Sky Drop
    if ((gBattleTypeFlags & BATTLE_TYPE_INGAME_PARTNER && GetBattlerPosition(gActiveBattler) == B_POSITION_PLAYER_RIGHT)
       || gStatuses3[gActiveBattler] & STATUS3_SKY_DROPPED)
        return FALSE;

    if (gStatuses3[gActiveBattler] & STATUS3_EMBARGO)
        return FALSE;

    if (AiExpectsToFaintPlayer())
        return FALSE;

    if (GetBattlerSide(gActiveBattler) == B_SIDE_PLAYER)
        party = gPlayerParty;
    else
        party = gEnemyParty;

    for (i = 0; i < PARTY_SIZE; i++)
    {
        if (IsValidForBattle(&party[i]))
        {
            validMons++;
        }
    }

    for (i = 0; i < MAX_TRAINER_ITEMS; i++)
    {
        u16 item;
        const u8 *itemEffects;
        u8 paramOffset;
        u8 battlerSide;

        item = gBattleResources->battleHistory->trainerItems[i];
        if (item == ITEM_NONE)
            continue;
        itemEffects = GetItemEffect(item);
        if (itemEffects == NULL)
            continue;

        switch (ItemId_GetBattleUsage(item))
        {
        case EFFECT_ITEM_HEAL_AND_CURE_STATUS:
            shouldUse = AI_ShouldHeal(0);
            break;
        case EFFECT_ITEM_RESTORE_HP:
            shouldUse = AI_ShouldHeal(itemEffects[GetItemEffectParamOffset(item, 4, 4)]);
            break;
        case EFFECT_ITEM_CURE_STATUS:
            if (itemEffects[3] & ITEM3_SLEEP && gBattleMons[gActiveBattler].status1 & STATUS1_SLEEP)
                shouldUse = TRUE;
            if (itemEffects[3] & ITEM3_POISON && (gBattleMons[gActiveBattler].status1 & STATUS1_POISON
                                               || gBattleMons[gActiveBattler].status1 & STATUS1_TOXIC_POISON))
                shouldUse = TRUE;
            if (itemEffects[3] & ITEM3_BURN && gBattleMons[gActiveBattler].status1 & STATUS1_BURN)
                shouldUse = TRUE;
            if (itemEffects[3] & ITEM3_FREEZE && (gBattleMons[gActiveBattler].status1 & STATUS1_FREEZE || gBattleMons[gActiveBattler].status1 & STATUS1_FROSTBITE))
                shouldUse = TRUE;
            if (itemEffects[3] & ITEM3_PARALYSIS && gBattleMons[gActiveBattler].status1 & STATUS1_PARALYSIS)
                shouldUse = TRUE;
            if (itemEffects[3] & ITEM3_CONFUSION && gBattleMons[gActiveBattler].status2 & STATUS2_CONFUSION)
                shouldUse = TRUE;
            break;
        case EFFECT_ITEM_INCREASE_STAT:
        case EFFECT_ITEM_INCREASE_ALL_STATS:
            if (!gDisableStructs[gActiveBattler].isFirstTurn
                || AI_OpponentCanFaintAiWithMod(0))
                break;
            shouldUse = TRUE;
            break;
        case EFFECT_ITEM_SET_FOCUS_ENERGY:
            if (!gDisableStructs[gActiveBattler].isFirstTurn
                || gBattleMons[gActiveBattler].status2 & STATUS2_FOCUS_ENERGY_ANY
                || AI_OpponentCanFaintAiWithMod(0))
                break;
            shouldUse = TRUE;
            break;
        case EFFECT_ITEM_SET_MIST:
            battlerSide = GetBattlerSide(gActiveBattler);
            if (gDisableStructs[gActiveBattler].isFirstTurn && !(gSideStatuses[battlerSide] & SIDE_STATUS_MIST))
                shouldUse = TRUE;
            break;
        case EFFECT_ITEM_REVIVE:
            gBattleStruct->itemPartyIndex[gActiveBattler] = GetFirstFaintedPartyIndex(gActiveBattler);
            if (gBattleStruct->itemPartyIndex[gActiveBattler] != PARTY_SIZE) // Revive if possible.
                shouldUse = TRUE;
            break;
        default:
            return FALSE;
        }
        if (shouldUse)
        {
            // Set selected party ID to current battler if none chosen.
            if (gBattleStruct->itemPartyIndex[gActiveBattler] == PARTY_SIZE)
                gBattleStruct->itemPartyIndex[gActiveBattler] = gBattlerPartyIndexes[gActiveBattler];
            BtlController_EmitTwoReturnValues(BUFFER_B, B_ACTION_USE_ITEM, 0);
            gBattleStruct->chosenItem[gActiveBattler] = item;
            gBattleResources->battleHistory->trainerItems[i] = 0;
            return shouldUse;
        }
    }

    return FALSE;
}

static bool32 AI_ShouldHeal(u32 healAmount)
{
    bool32 shouldHeal = FALSE;

    if (gBattleMons[gActiveBattler].hp < gBattleMons[gActiveBattler].maxHP / 4
     || gBattleMons[gActiveBattler].hp == 0
     || (healAmount != 0 && gBattleMons[gActiveBattler].maxHP - gBattleMons[gActiveBattler].hp > healAmount)) {
        // We have low enough HP to consider healing
        shouldHeal = !AI_OpponentCanFaintAiWithMod(healAmount); // if target can kill us even after we heal, why bother
    }

    return shouldHeal;
}

static bool32 AI_OpponentCanFaintAiWithMod(u32 healAmount)
{
    u32 i;
    // Check special cases to NOT heal
    for (i = 0; i < gBattlersCount; i++) {
        if (GetBattlerSide(i) == B_SIDE_PLAYER) {
            if (CanTargetFaintAiWithMod(i, gActiveBattler, healAmount, 0)) {
                // Target is expected to faint us
                return TRUE;
            }
        }
    }
    return FALSE;
}

static bool32 IsAiPartyMonOHKOBy(u32 battlerAtk, struct Pokemon *aiMon)
{
    bool32 ret = FALSE;
    struct BattlePokemon *savedBattleMons;
    s32 hp = GetMonData(aiMon, MON_DATA_HP);
    s32 bestDmg = AI_CalcPartyMonBestMoveDamage(battlerAtk, gActiveBattler, NULL, aiMon);

    switch (GetNoOfHitsToKO(bestDmg, hp))
    {
    case 1:
        ret = TRUE;
        break;
    case 2: // if AI mon is faster allow 2 turns
        savedBattleMons = AllocSaveBattleMons();
        PokemonToBattleMon(aiMon, &gBattleMons[gActiveBattler]);
        if (AI_WhoStrikesFirst(gActiveBattler, battlerAtk, 0) == AI_IS_SLOWER)
            ret = TRUE;
        else
            ret = FALSE;
        FreeRestoreBattleMons(savedBattleMons);
        break;
    }

    return ret;
}
