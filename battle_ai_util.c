#include "global.h"
#include "battle_z_move.h"
#include "malloc.h"
#include "battle.h"
#include "battle_anim.h"
#include "battle_ai_util.h"
#include "battle_ai_main.h"
#include "battle_ai_switch_items.h"
#include "battle_factory.h"
#include "battle_setup.h"
#include "data.h"
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

#define CHECK_MOVE_FLAG(flag)                                                                                   \
    s32 i;                                                                                                      \
    u16 *moves = GetMovesArray(battler);                                                                        \
    for (i = 0; i < MAX_MON_MOVES; i++)                                                                         \
    {                                                                                                           \
        if (moves[i] != MOVE_NONE && moves[i] != MOVE_UNAVAILABLE && gBattleMoves[moves[i]].flag)               \
            return TRUE;                                                                                        \
    }                                                                                                           \
    return FALSE

static u32 AI_GetEffectiveness(uq4_12_t multiplier);

// Const Data
static const s8 sAiAbilityRatings[ABILITIES_COUNT] =
{
    [ABILITY_ADAPTABILITY] = 8,
    [ABILITY_AFTERMATH] = 5,
    [ABILITY_AERILATE] = 8,
    [ABILITY_AIR_LOCK] = 5,
    [ABILITY_ANALYTIC] = 5,
    [ABILITY_ANGER_POINT] = 4,
    [ABILITY_ANTICIPATION] = 2,
    [ABILITY_ARENA_TRAP] = 9,
    [ABILITY_AROMA_VEIL] = 3,
    [ABILITY_AURA_BREAK] = 3,
    [ABILITY_BAD_DREAMS] = 4,
    [ABILITY_BATTERY] = 0,
    [ABILITY_BATTLE_ARMOR] = 2,
    [ABILITY_BATTLE_BOND] = 6,
    [ABILITY_BEAST_BOOST] = 7,
    [ABILITY_BERSERK] = 5,
    [ABILITY_BIG_PECKS] = 1,
    [ABILITY_BLAZE] = 5,
    [ABILITY_BULLETPROOF] = 7,
    [ABILITY_CHEEK_POUCH] = 4,
    [ABILITY_CHLOROPHYLL] = 6,
    [ABILITY_CLEAR_BODY] = 4,
    [ABILITY_CLOUD_NINE] = 5,
    [ABILITY_COLOR_CHANGE] = 2,
    [ABILITY_COMATOSE] = 6,
    [ABILITY_COMPETITIVE] = 5,
    [ABILITY_COMPOUND_EYES] = 7,
    [ABILITY_CONTRARY] = 8,
    [ABILITY_CORROSION] = 5,
    [ABILITY_CURSED_BODY] = 4,
    [ABILITY_CUTE_CHARM] = 2,
    [ABILITY_DAMP] = 2,
    [ABILITY_DANCER] = 5,
    [ABILITY_DARK_AURA] = 6,
    [ABILITY_DAZZLING] = 5,
    [ABILITY_DEFEATIST] = -1,
    [ABILITY_DEFIANT] = 5,
    [ABILITY_DELTA_STREAM] = 10,
    [ABILITY_DESOLATE_LAND] = 10,
    [ABILITY_DISGUISE] = 8,
    [ABILITY_DOWNLOAD] = 7,
    [ABILITY_DRIZZLE] = 9,
    [ABILITY_DROUGHT] = 9,
    [ABILITY_DRY_SKIN] = 6,
    [ABILITY_EARLY_BIRD] = 4,
    [ABILITY_EFFECT_SPORE] = 4,
    [ABILITY_ELECTRIC_SURGE] = 8,
    [ABILITY_EMERGENCY_EXIT] = 3,
    [ABILITY_FAIRY_AURA] = 6,
    [ABILITY_FILTER] = 6,
    [ABILITY_FLAME_BODY] = 4,
    [ABILITY_FLARE_BOOST] = 5,
    [ABILITY_FLASH_FIRE] = 6,
    [ABILITY_FLOWER_GIFT] = 4,
    [ABILITY_FLOWER_VEIL] = 0,
    [ABILITY_FLUFFY] = 5,
    [ABILITY_FORECAST] = 6,
    [ABILITY_FOREWARN] = 2,
    [ABILITY_FRIEND_GUARD] = 0,
    [ABILITY_FRISK] = 3,
    [ABILITY_FULL_METAL_BODY] = 4,
    [ABILITY_FUR_COAT] = 7,
    [ABILITY_GALE_WINGS] = 6,
    [ABILITY_GALVANIZE] = 8,
    [ABILITY_GLUTTONY] = 3,
    [ABILITY_GOOEY] = 5,
    [ABILITY_GRASS_PELT] = 2,
    [ABILITY_GRASSY_SURGE] = 8,
    [ABILITY_GUTS] = 6,
    [ABILITY_HARVEST] = 5,
    [ABILITY_HEALER] = 0,
    [ABILITY_HEATPROOF] = 5,
    [ABILITY_HEAVY_METAL] = -1,
    [ABILITY_HONEY_GATHER] = 0,
    [ABILITY_HUGE_POWER] = 10,
    [ABILITY_HUSTLE] = 7,
    [ABILITY_HYDRATION] = 4,
    [ABILITY_HYPER_CUTTER] = 3,
    [ABILITY_ICE_BODY] = 3,
    [ABILITY_ILLUMINATE] = 0,
    [ABILITY_ILLUSION] = 8,
    [ABILITY_IMMUNITY] = 4,
    [ABILITY_IMPOSTER] = 9,
    [ABILITY_INFILTRATOR] = 6,
    [ABILITY_INNARDS_OUT] = 5,
    [ABILITY_INNER_FOCUS] = 2,
    [ABILITY_INSOMNIA] = 4,
    [ABILITY_INTIMIDATE] = 7,
    [ABILITY_IRON_BARBS] = 6,
    [ABILITY_IRON_FIST] = 6,
    [ABILITY_JUSTIFIED] = 4,
    [ABILITY_KEEN_EYE] = 1,
    [ABILITY_KLUTZ] = -1,
    [ABILITY_LEAF_GUARD] = 2,
    [ABILITY_LEVITATE] = 7,
    [ABILITY_LIGHT_METAL] = 2,
    [ABILITY_LIGHTNING_ROD] = 7,
    [ABILITY_LIMBER] = 3,
    [ABILITY_LIQUID_OOZE] = 3,
    [ABILITY_LIQUID_VOICE] = 5,
    [ABILITY_LONG_REACH] = 3,
    [ABILITY_MAGIC_BOUNCE] = 9,
    [ABILITY_MAGIC_GUARD] = 9,
    [ABILITY_MAGICIAN] = 3,
    [ABILITY_MAGMA_ARMOR] = 1,
    [ABILITY_MAGNET_PULL] = 9,
    [ABILITY_MARVEL_SCALE] = 5,
    [ABILITY_MEGA_LAUNCHER] = 7,
    [ABILITY_MERCILESS] = 4,
    [ABILITY_MINUS] = 0,
    [ABILITY_MISTY_SURGE] = 8,
    [ABILITY_MOLD_BREAKER] = 7,
    [ABILITY_MOODY] = 10,
    [ABILITY_MOTOR_DRIVE] = 6,
    [ABILITY_MOXIE] = 7,
    [ABILITY_MULTISCALE] = 8,
    [ABILITY_MULTITYPE] = 8,
    [ABILITY_MUMMY] = 5,
    [ABILITY_NATURAL_CURE] = 7,
    [ABILITY_NEUROFORCE] = 6,
    [ABILITY_NO_GUARD] = 8,
    [ABILITY_NORMALIZE] = -1,
    [ABILITY_OBLIVIOUS] = 2,
    [ABILITY_OVERCOAT] = 5,
    [ABILITY_OVERGROW] = 5,
    [ABILITY_OWN_TEMPO] = 3,
    [ABILITY_PARENTAL_BOND] = 10,
    [ABILITY_PICKUP] = 1,
    [ABILITY_PICKPOCKET] = 3,
    [ABILITY_PIXILATE] = 8,
    [ABILITY_PLUS] = 0,
    [ABILITY_POISON_HEAL] = 8,
    [ABILITY_POISON_POINT] = 4,
    [ABILITY_POISON_TOUCH] = 4,
    [ABILITY_POWER_CONSTRUCT] = 10,
    [ABILITY_POWER_OF_ALCHEMY] = 0,
    [ABILITY_PRANKSTER] = 8,
    [ABILITY_PRESSURE] = 5,
    [ABILITY_PRIMORDIAL_SEA] = 10,
    [ABILITY_PRISM_ARMOR] = 6,
    [ABILITY_PROTEAN] = 8,
    [ABILITY_PSYCHIC_SURGE] = 8,
    [ABILITY_PURE_POWER] = 10,
    [ABILITY_QUEENLY_MAJESTY] = 6,
    [ABILITY_QUICK_FEET] = 5,
    [ABILITY_RAIN_DISH] = 3,
    [ABILITY_RATTLED] = 3,
    [ABILITY_RECEIVER] = 0,
    [ABILITY_RECKLESS] = 6,
    [ABILITY_REFRIGERATE] = 8,
    [ABILITY_REGENERATOR] = 8,
    [ABILITY_RIVALRY] = 1,
    [ABILITY_RKS_SYSTEM] = 8,
    [ABILITY_ROCK_HEAD] = 5,
    [ABILITY_ROUGH_SKIN] = 6,
    [ABILITY_RUN_AWAY] = 0,
    [ABILITY_SAND_FORCE] = 4,
    [ABILITY_SAND_RUSH] = 6,
    [ABILITY_SAND_STREAM] = 9,
    [ABILITY_SAND_VEIL] = 3,
    [ABILITY_SAP_SIPPER] = 7,
    [ABILITY_SCHOOLING] = 6,
    [ABILITY_SCRAPPY] = 6,
    [ABILITY_SERENE_GRACE] = 8,
    [ABILITY_SHADOW_SHIELD] = 8,
    [ABILITY_SHADOW_TAG] = 10,
    [ABILITY_SHED_SKIN] = 7,
    [ABILITY_SHEER_FORCE] = 8,
    [ABILITY_SHELL_ARMOR] = 2,
    [ABILITY_SHIELD_DUST] = 5,
    [ABILITY_SHIELDS_DOWN] = 6,
    [ABILITY_SIMPLE] = 8,
    [ABILITY_SKILL_LINK] = 7,
    [ABILITY_SLOW_START] = -2,
    [ABILITY_SLUSH_RUSH] = 5,
    [ABILITY_SNIPER] = 3,
    [ABILITY_SNOW_CLOAK] = 3,
    [ABILITY_SNOW_WARNING] = 8,
    [ABILITY_SOLAR_POWER] = 3,
    [ABILITY_SOLID_ROCK] = 6,
    [ABILITY_SOUL_HEART] = 7,
    [ABILITY_SOUNDPROOF] = 4,
    [ABILITY_SPEED_BOOST] = 9,
    [ABILITY_STAKEOUT] = 6,
    [ABILITY_STALL] = -1,
    [ABILITY_STAMINA] = 6,
    [ABILITY_STANCE_CHANGE] = 10,
    [ABILITY_STATIC] = 4,
    [ABILITY_STEADFAST] = 2,
    [ABILITY_STEELWORKER] = 6,
    [ABILITY_STENCH] = 1,
    [ABILITY_STICKY_HOLD] = 3,
    [ABILITY_STORM_DRAIN] = 7,
    [ABILITY_STRONG_JAW] = 6,
    [ABILITY_STURDY] = 6,
    [ABILITY_SUCTION_CUPS] = 2,
    [ABILITY_SUPER_LUCK] = 3,
    [ABILITY_SURGE_SURFER] = 4,
    [ABILITY_SWARM] = 5,
    [ABILITY_SWEET_VEIL] = 4,
    [ABILITY_SWIFT_SWIM] = 6,
    [ABILITY_SYMBIOSIS] = 0,
    [ABILITY_SYNCHRONIZE] = 4,
    [ABILITY_TANGLED_FEET] = 2,
    [ABILITY_TANGLING_HAIR] = 5,
    [ABILITY_TECHNICIAN] = 8,
    [ABILITY_TELEPATHY] = 0,
    [ABILITY_TERAVOLT] = 7,
    [ABILITY_THICK_FAT] = 7,
    [ABILITY_TINTED_LENS] = 7,
    [ABILITY_TORRENT] = 5,
    [ABILITY_TOXIC_BOOST] = 6,
    [ABILITY_TOUGH_CLAWS] = 7,
    [ABILITY_TRACE] = 6,
    [ABILITY_TRIAGE] = 7,
    [ABILITY_TRUANT] = -2,
    [ABILITY_TURBOBLAZE] = 7,
    [ABILITY_UNAWARE] = 6,
    [ABILITY_UNBURDEN] = 7,
    [ABILITY_UNNERVE] = 3,
    [ABILITY_VICTORY_STAR] = 6,
    [ABILITY_VITAL_SPIRIT] = 4,
    [ABILITY_VOLT_ABSORB] = 7,
    [ABILITY_WATER_ABSORB] = 7,
    [ABILITY_WATER_BUBBLE] = 8,
    [ABILITY_WATER_COMPACTION] = 4,
    [ABILITY_WATER_VEIL] = 4,
    [ABILITY_WEAK_ARMOR] = 2,
    [ABILITY_WHITE_SMOKE] = 4,
    [ABILITY_WIMP_OUT] = 3,
    [ABILITY_WONDER_GUARD] = 10,
    [ABILITY_WONDER_SKIN] = 4,
    [ABILITY_ZEN_MODE] = -1,
    [ABILITY_INTREPID_SWORD] = 3,
    [ABILITY_DAUNTLESS_SHIELD] = 3,
    [ABILITY_BALL_FETCH] = 0,
    [ABILITY_COTTON_DOWN] = 3,
    [ABILITY_MIRROR_ARMOR] = 6,
    [ABILITY_GULP_MISSILE] = 3,
    [ABILITY_STALWART] = 2,
    [ABILITY_PROPELLER_TAIL] = 2,
    [ABILITY_STEAM_ENGINE] = 3,
    [ABILITY_PUNK_ROCK] = 2,
    [ABILITY_SAND_SPIT] = 5,
    [ABILITY_ICE_SCALES] = 7,
    [ABILITY_RIPEN] = 4,
    [ABILITY_ICE_FACE] = 4,
    [ABILITY_POWER_SPOT] = 2,
    [ABILITY_MIMICRY] = 2,
    [ABILITY_SCREEN_CLEANER] = 3,
    [ABILITY_NEUTRALIZING_GAS] = 5,
    [ABILITY_HUNGER_SWITCH] = 2,
    [ABILITY_PASTEL_VEIL] = 4,
    [ABILITY_STEELY_SPIRIT] = 2,
    [ABILITY_PERISH_BODY] = -1,
    [ABILITY_WANDERING_SPIRIT] = 2,
    [ABILITY_GORILLA_TACTICS] = 4,
};

static const u16 sEncouragedEncoreEffects[] =
{
    EFFECT_DREAM_EATER,
    EFFECT_ATTACK_UP,
    EFFECT_DEFENSE_UP,
    EFFECT_SPEED_UP,
    EFFECT_SPECIAL_ATTACK_UP,
    EFFECT_HAZE,
    EFFECT_ROAR,
    EFFECT_CONVERSION,
    EFFECT_TOXIC,
    EFFECT_LIGHT_SCREEN,
    EFFECT_REST,
    EFFECT_SUPER_FANG,
    EFFECT_SPECIAL_DEFENSE_UP_2,
    EFFECT_CONFUSE,
    EFFECT_POISON,
    EFFECT_PARALYZE,
    EFFECT_LEECH_SEED,
    EFFECT_DO_NOTHING,
    EFFECT_ATTACK_UP_2,
    EFFECT_ENCORE,
    EFFECT_CONVERSION_2,
    EFFECT_LOCK_ON,
    EFFECT_HEAL_BELL,
    EFFECT_MEAN_LOOK,
    EFFECT_NIGHTMARE,
    EFFECT_PROTECT,
    EFFECT_SKILL_SWAP,
    EFFECT_FORESIGHT,
    EFFECT_PERISH_SONG,
    EFFECT_SANDSTORM,
    EFFECT_ENDURE,
    EFFECT_SWAGGER,
    EFFECT_ATTRACT,
    EFFECT_SAFEGUARD,
    EFFECT_RAIN_DANCE,
    EFFECT_SUNNY_DAY,
    EFFECT_BELLY_DRUM,
    EFFECT_PSYCH_UP,
    EFFECT_FUTURE_SIGHT,
    EFFECT_FAKE_OUT,
    EFFECT_STOCKPILE,
    EFFECT_SPIT_UP,
    EFFECT_SWALLOW,
    EFFECT_HAIL,
    EFFECT_SNOWSCAPE,
    EFFECT_TORMENT,
    EFFECT_WILL_O_WISP,
    EFFECT_FOLLOW_ME,
    EFFECT_CHARGE,
    EFFECT_TRICK,
    EFFECT_ROLE_PLAY,
    EFFECT_INGRAIN,
    EFFECT_RECYCLE,
    EFFECT_KNOCK_OFF,
    EFFECT_SKILL_SWAP,
    EFFECT_IMPRISON,
    EFFECT_REFRESH,
    EFFECT_GRUDGE,
    EFFECT_MUD_SPORT,
    EFFECT_WATER_SPORT,
    EFFECT_DRAGON_DANCE,
    EFFECT_CAMOUFLAGE,
    EFFECT_STEALTH_ROCK,
    EFFECT_SPIKES,
    EFFECT_STICKY_WEB,
    EFFECT_TOXIC_SPIKES,
    EFFECT_RESTORE_HP,
};

// For the purposes of determining the most powerful move in a moveset, these
// moves are treated the same as having a power of 0 or 1
#define IGNORED_MOVES_END 0xFFFF
static const u16 sIgnoredPowerfulMoveEffects[] =
{
    EFFECT_EXPLOSION,
    EFFECT_FINAL_GAMBIT,
    EFFECT_METEOR_BEAM,
    EFFECT_TWO_TURNS_ATTACK,
    EFFECT_FUTURE_SIGHT,
    EFFECT_FEINT,
    EFFECT_UPPER_HAND,
    IGNORED_MOVES_END
};

static const u16 sIgnoreMoldBreakerMoves[] =
{
    MOVE_MOONGEIST_BEAM,
    MOVE_SUNSTEEL_STRIKE,
    MOVE_PHOTON_GEYSER,
    MOVE_LIGHT_THAT_BURNS_THE_SKY,
    MOVE_MENACING_MOONRAZE_MAELSTROM,
    MOVE_SEARING_SUNRAZE_SMASH,
};

static const u16 sRechargeMoves[] =
{
    MOVE_HYPER_BEAM,
    MOVE_BLAST_BURN,
    MOVE_HYDRO_CANNON,
    MOVE_FRENZY_PLANT,
    MOVE_GIGA_IMPACT,
    MOVE_ROCK_WRECKER,
    MOVE_ROAR_OF_TIME,
    MOVE_PRISMATIC_LASER,
    MOVE_METEOR_ASSAULT,
    MOVE_ETERNABEAM,
};

static const u16 sOtherMoveCallingMoves[] =
{
    MOVE_ASSIST,
    MOVE_COPYCAT,
    MOVE_ME_FIRST,
    MOVE_METRONOME,
    MOVE_MIRROR_MOVE,
    MOVE_NATURE_POWER,
    MOVE_SLEEP_TALK,
};

// Functions
u16 GetAIChosenMove(u8 battlerId)
{
    return (gBattleMons[battlerId].moves[gBattleStruct->aiMoveOrAction[battlerId]]);
}

bool32 WillAIStrikeFirst(void)
{
    return (AI_WhoStrikesFirst(sBattler_AI, gBattlerTarget, AI_THINKING_STRUCT->moveConsidered) == AI_IS_FASTER);
}

bool32 AI_RandLessThan(u8 val)
{
    if ((Random() % 0xFF) < val)
        return TRUE;
    return FALSE;
}

void RecordLastUsedMoveByTarget(void)
{
    RecordKnownMove(gBattlerTarget, gLastMoves[gBattlerTarget]);
}

bool32 BattlerHasAi(u32 battlerId)
{
    switch (GetBattlerPosition(battlerId))
    {
    case B_POSITION_PLAYER_LEFT:
    default:
        return FALSE;
    case B_POSITION_OPPONENT_LEFT:
        return TRUE;
    case B_POSITION_PLAYER_RIGHT:
        return ((gBattleTypeFlags & BATTLE_TYPE_INGAME_PARTNER) != 0);
    case B_POSITION_OPPONENT_RIGHT:
        return TRUE;
    }
}

bool32 IsAiBattlerAware(u32 battlerId)
{
    if (AI_THINKING_STRUCT->aiFlags & AI_FLAG_OMNISCIENT)
        return TRUE;

    return BattlerHasAi(battlerId);
}

void ClearBattlerMoveHistory(u8 battlerId)
{
    memset(BATTLE_HISTORY->usedMoves[battlerId], 0, sizeof(BATTLE_HISTORY->usedMoves[battlerId]));
    memset(BATTLE_HISTORY->moveHistory[battlerId], 0, sizeof(BATTLE_HISTORY->moveHistory[battlerId]));
    BATTLE_HISTORY->moveHistoryIndex[battlerId] = 0;
}

void RecordLastUsedMoveBy(u32 battlerId, u32 move)
{
    u8 *index = &BATTLE_HISTORY->moveHistoryIndex[battlerId];

    if (++(*index) >= AI_MOVE_HISTORY_COUNT)
        *index = 0;
    BATTLE_HISTORY->moveHistory[battlerId][*index] = move;
}

void RecordKnownMove(u8 battlerId, u32 move)
{
    s32 i;
    for (i = 0; i < MAX_MON_MOVES; i++)
    {
        if (BATTLE_HISTORY->usedMoves[battlerId][i] == move)
            break;
        if (BATTLE_HISTORY->usedMoves[battlerId][i] == MOVE_NONE)
        {
            BATTLE_HISTORY->usedMoves[battlerId][i] = move;
            AI_PARTY->mons[GetBattlerSide(battlerId)][gBattlerPartyIndexes[battlerId]].moves[i] = move;
            break;
        }
    }
}

void RecordAbilityBattle(u8 battlerId, u16 abilityId)
{
    BATTLE_HISTORY->abilities[battlerId] = abilityId;
    AI_PARTY->mons[GetBattlerSide(battlerId)][gBattlerPartyIndexes[battlerId]].ability = abilityId;
}

void ClearBattlerAbilityHistory(u8 battlerId)
{
    BATTLE_HISTORY->abilities[battlerId] = ABILITY_NONE;
}

void RecordItemEffectBattle(u8 battlerId, u8 itemEffect)
{
    BATTLE_HISTORY->itemEffects[battlerId] = itemEffect;
    AI_PARTY->mons[GetBattlerSide(battlerId)][gBattlerPartyIndexes[battlerId]].heldEffect = itemEffect;
}

void ClearBattlerItemEffectHistory(u8 battlerId)
{
    BATTLE_HISTORY->itemEffects[battlerId] = 0;
}

void SaveIllusionData(u8 battlerId)
{
    AI_THINKING_STRUCT->saved[battlerId].ability = gBattleMons[battlerId].ability;
    AI_THINKING_STRUCT->saved[battlerId].hp = gBattleMons[battlerId].hp;
    AI_THINKING_STRUCT->saved[battlerId].attack = gBattleMons[battlerId].attack;
    AI_THINKING_STRUCT->saved[battlerId].defense = gBattleMons[battlerId].defense;
    AI_THINKING_STRUCT->saved[battlerId].spAttack = gBattleMons[battlerId].spAttack;
    AI_THINKING_STRUCT->saved[battlerId].spDefense = gBattleMons[battlerId].spDefense;
    AI_THINKING_STRUCT->saved[battlerId].speed = gBattleMons[battlerId].speed;
    AI_THINKING_STRUCT->saved[battlerId].heldItem = gBattleMons[battlerId].item;
    AI_THINKING_STRUCT->saved[battlerId].species = gBattleMons[battlerId].species;
    // Save and restore types even for AI controlled battlers in case it gets changed during move evaluation process.
    AI_THINKING_STRUCT->saved[battlerId].types[0] = gBattleMons[battlerId].type1;
    AI_THINKING_STRUCT->saved[battlerId].types[1] = gBattleMons[battlerId].type2;
}

void SaveBattlerData(u8 battlerId)
{
    if (!BattlerHasAi(battlerId))
    {
        u32 i;

        AI_THINKING_STRUCT->saved[battlerId].ability = gBattleMons[battlerId].ability;
        AI_THINKING_STRUCT->saved[battlerId].heldItem = gBattleMons[battlerId].item;
        AI_THINKING_STRUCT->saved[battlerId].species = gBattleMons[battlerId].species;
        for (i = 0; i < 4; i++)
            AI_THINKING_STRUCT->saved[battlerId].moves[i] = gBattleMons[battlerId].moves[i];
    }
    // Save and restore types even for AI controlled battlers in case it gets changed during move evaluation process.
    AI_THINKING_STRUCT->saved[battlerId].types[0] = gBattleMons[battlerId].type1;
    AI_THINKING_STRUCT->saved[battlerId].types[1] = gBattleMons[battlerId].type2;
}

static bool32 ShouldFailForIllusion(u32 battlerId)
{
    if (GetBattlerSide(battlerId) == B_SIDE_OPPONENT)
        return FALSE;
    return gDisableStructs[battlerId].isFirstTurn 
        && !gDisableStructs[battlerId].illusionExposed;
}

void SetIllusionData(u8 battlerId)
{
    u32 i, species, illusionSpecies, side;
    struct Pokemon *illusionMon;
    side = GetBattlerSide(battlerId);

    if (gBattleMons[battlerId].ability != ABILITY_ILLUSION)
        return;
    if (!ShouldFailForIllusion(battlerId))
        return;

    // Simulate Illusion
    species = gBattleMons[battlerId].species;
    illusionMon = GetIllusionMonPtr(battlerId);
    illusionSpecies = GetIllusionMonSpecies(battlerId);
    if (illusionSpecies != SPECIES_NONE)
    {
        // If the battler's type has not been changed, AI assumes the types of the illusion mon.
        if (gBattleMons[battlerId].type1 == gSpeciesInfo[species].types[0]
            && gBattleMons[battlerId].type2 == gSpeciesInfo[species].types[1])
        {
            gBattleMons[battlerId].type1 = gSpeciesInfo[illusionSpecies].types[0];
            gBattleMons[battlerId].type2 = gSpeciesInfo[illusionSpecies].types[1];
        }
        species = illusionSpecies;
        gBattleMons[battlerId].hp = GetMonData(illusionMon, MON_DATA_HP);
        gBattleMons[battlerId].attack = GetMonData(illusionMon, MON_DATA_ATK);
        gBattleMons[battlerId].defense = GetMonData(illusionMon, MON_DATA_DEF);
        gBattleMons[battlerId].spAttack = GetMonData(illusionMon, MON_DATA_SPATK);
        gBattleMons[battlerId].spDefense = GetMonData(illusionMon, MON_DATA_SPDEF);
        gBattleMons[battlerId].speed = GetMonData(illusionMon, MON_DATA_SPEED);
        gBattleMons[battlerId].ability = GetAbilityBySpecies(illusionSpecies, GetMonData(illusionMon, MON_DATA_ABILITY_NUM));
    }
}

void SetBattlerData(u8 battlerId)
{
    if (!BattlerHasAi(battlerId))
    {
        u32 i, species, illusionSpecies, side;
        side = GetBattlerSide(battlerId);

        // Simulate Illusion
        species = gBattleMons[battlerId].species;
        illusionSpecies = GetIllusionMonSpecies(battlerId);
        if (illusionSpecies != SPECIES_NONE && ShouldFailForIllusion(battlerId))
        {
            // If the battler's type has not been changed, AI assumes the types of the illusion mon.
            if (gBattleMons[battlerId].type1 == gSpeciesInfo[species].types[0]
                && gBattleMons[battlerId].type2 == gSpeciesInfo[species].types[1])
            {
                gBattleMons[battlerId].type1 = gSpeciesInfo[illusionSpecies].types[0];
                gBattleMons[battlerId].type2 = gSpeciesInfo[illusionSpecies].types[1];
            }
            species = illusionSpecies;
        }

        // Use the known battler's ability.
        if (AI_PARTY->mons[side][gBattlerPartyIndexes[battlerId]].ability != ABILITY_NONE)
            gBattleMons[battlerId].ability = AI_PARTY->mons[side][gBattlerPartyIndexes[battlerId]].ability;
        // Check if mon can only have one ability.
        else if (gSpeciesInfo[species].abilities[1] == ABILITY_NONE
                || gSpeciesInfo[species].abilities[1] == gSpeciesInfo[species].abilities[0])
            gBattleMons[battlerId].ability = gSpeciesInfo[species].abilities[0];
        // The ability is unknown.
        else
            gBattleMons[battlerId].ability = ABILITY_NONE;

        if (AI_PARTY->mons[side][gBattlerPartyIndexes[battlerId]].heldEffect == 0)
            gBattleMons[battlerId].item = 0;

        for (i = 0; i < MAX_MON_MOVES; i++)
        {
            if (AI_PARTY->mons[side][gBattlerPartyIndexes[battlerId]].moves[i] == 0)
                gBattleMons[battlerId].moves[i] = 0;
        }
    }
}

void RestoreIllusionData(u8 battlerId)
{
    gBattleMons[battlerId].ability = AI_THINKING_STRUCT->saved[battlerId].ability;
    gBattleMons[battlerId].item = AI_THINKING_STRUCT->saved[battlerId].heldItem;
    gBattleMons[battlerId].species = AI_THINKING_STRUCT->saved[battlerId].species;
    gBattleMons[battlerId].type1 = AI_THINKING_STRUCT->saved[battlerId].types[0];
    gBattleMons[battlerId].type2 = AI_THINKING_STRUCT->saved[battlerId].types[1];
    gBattleMons[battlerId].hp = AI_THINKING_STRUCT->saved[battlerId].hp;
    gBattleMons[battlerId].attack = AI_THINKING_STRUCT->saved[battlerId].attack;
    gBattleMons[battlerId].defense = AI_THINKING_STRUCT->saved[battlerId].defense;
    gBattleMons[battlerId].spAttack = AI_THINKING_STRUCT->saved[battlerId].spAttack;
    gBattleMons[battlerId].spDefense = AI_THINKING_STRUCT->saved[battlerId].spDefense;
    gBattleMons[battlerId].speed = AI_THINKING_STRUCT->saved[battlerId].speed;
}

void RestoreBattlerData(u8 battlerId)
{
    if (!BattlerHasAi(battlerId))
    {
        u32 i;

        gBattleMons[battlerId].ability = AI_THINKING_STRUCT->saved[battlerId].ability;
        gBattleMons[battlerId].item = AI_THINKING_STRUCT->saved[battlerId].heldItem;
        gBattleMons[battlerId].species = AI_THINKING_STRUCT->saved[battlerId].species;
        for (i = 0; i < 4; i++)
            gBattleMons[battlerId].moves[i] = AI_THINKING_STRUCT->saved[battlerId].moves[i];
    }
    gBattleMons[battlerId].type1 = AI_THINKING_STRUCT->saved[battlerId].types[0];
    gBattleMons[battlerId].type2 = AI_THINKING_STRUCT->saved[battlerId].types[1];
}

u32 GetHealthPercentage(u8 battlerId)
{
    return (u32)((100 * gBattleMons[battlerId].hp) / gBattleMons[battlerId].maxHP);
}

bool32 AtMaxHp(u8 battlerId)
{
    if (AI_DATA->hpPercents[battlerId] == 100)
        return TRUE;
    return FALSE;
}

bool32 IsBattlerTrapped(u8 battler, bool8 checkSwitch)
{
    u8 holdEffect = AI_DATA->holdEffects[battler];

#if B_GHOSTS_ESCAPE >= GEN_6
    if (IS_BATTLER_OF_TYPE(battler, TYPE_GHOST))
        return FALSE;
#endif
    if (checkSwitch && holdEffect == HOLD_EFFECT_SHED_SHELL)
        return FALSE;
    else if (!checkSwitch && GetBattlerAbility(battler) == ABILITY_RUN_AWAY)
        return FALSE;
    else if (!checkSwitch && holdEffect == HOLD_EFFECT_CAN_ALWAYS_RUN)
        return FALSE;
    else if (gBattleMons[battler].status2 & (STATUS2_ESCAPE_PREVENTION | STATUS2_WRAPPED))
        return TRUE;
    else if (gStatuses3[battler] & (STATUS3_ROOTED | STATUS3_SKY_DROPPED))
        return TRUE;
    else if (gFieldStatuses & STATUS_FIELD_FAIRY_LOCK)
        return TRUE;
    else if (IsAbilityPreventingEscape(battler))
        return TRUE;

    return FALSE;
}

u32 GetTotalBaseStat(u32 species)
{
    return gSpeciesInfo[species].baseHP
        + gSpeciesInfo[species].baseAttack
        + gSpeciesInfo[species].baseDefense
        + gSpeciesInfo[species].baseSpeed
        + gSpeciesInfo[species].baseSpAttack
        + gSpeciesInfo[species].baseSpDefense;
}

bool32 IsTruantMonVulnerable(u32 battlerAI, u32 opposingBattler)
{
    int i;

    for (i = 0; i < MAX_MON_MOVES; i++)
    {
        u32 move = gBattleResources->battleHistory->usedMoves[opposingBattler][i];
        if (gBattleMoves[move].effect == EFFECT_PROTECT && move != MOVE_ENDURE)
            return TRUE;
        if (gBattleMoves[move].effect == EFFECT_SEMI_INVULNERABLE && AI_WhoStrikesFirst(battlerAI, opposingBattler, GetAIChosenMove(battlerAI)) == AI_IS_SLOWER)
            return TRUE;
    }
    return FALSE;
}

// move checks
bool32 IsAffectedByPowder(u8 battler, u16 ability, u16 holdEffect)
{
    if (ability == ABILITY_OVERCOAT
    #if B_POWDER_GRASS >= GEN_6
        || IS_BATTLER_OF_TYPE(battler, TYPE_GRASS)
    #endif
        || holdEffect == HOLD_EFFECT_SAFETY_GOGGLES)
        return FALSE;
    return TRUE;
}

// This function checks if all physical/special moves are either unusable or unreasonable to use.
// Consider a pokemon boosting their attack against a ghost pokemon having only normal-type physical attacks.
bool32 MovesWithSplitUnusable(u32 attacker, u32 target, u32 split)
{
    s32 i, moveType;
    u32 usable = 0;
    u32 unusable = AI_DATA->moveLimitations[attacker];
    u16 *moves = GetMovesArray(attacker);

    for (i = 0; i < MAX_MON_MOVES; i++)
    {
        if (moves[i] != MOVE_NONE
            && moves[i] != MOVE_UNAVAILABLE
            && GetBattleMoveSplit(moves[i]) == split
            && !(unusable & gBitTable[i]))
        {
            SetTypeBeforeUsingMove(moves[i], attacker);
            GET_MOVE_TYPE(moves[i], moveType);
            if (CalcTypeEffectivenessMultiplier(moves[i], moveType, attacker, target, FALSE) != 0)
                usable |= gBitTable[i];
        }
    }

    return (usable == 0);
}

static bool32 AI_GetIfCrit(u32 move, u8 battlerAtk, u8 battlerDef)
{
    bool32 isCrit;

    switch (CalcCritChanceStage(battlerAtk, battlerDef, move, FALSE))
    {
    case -1:
    case 0:
    default:
        isCrit = FALSE;
        break;
    case 1:
        if (gBattleMoves[move].highCritRatio && (Random() % 5 == 0))
            isCrit = TRUE;
        else
            isCrit = FALSE;
        break;
    case 2:
        if (gBattleMoves[move].highCritRatio && (Random() % 2 == 0))
            isCrit = TRUE;
        else if (!(gBattleMoves[move].highCritRatio) && (Random() % 4) == 0)
            isCrit = TRUE;
        else
            isCrit = FALSE;
        break;
    case -2:
    case 3:
    case 4:
        isCrit = TRUE;
        break;
    }

    return isCrit;
}

bool32 IsDamageMoveUsable(u32 move, u32 battlerAtk, u32 battlerDef)
{
    s32 moveType;
    u32 battlerDefAbility;
    u32 partnerDefAbility;
    GET_MOVE_TYPE(move, moveType);

    if (gDisableStructs[battlerAtk].throatChopTimer > gBattleTurnCounter && gBattleMoves[move].soundMove)
        return TRUE;
    if (gStatuses3[battlerAtk] & STATUS3_HEAL_BLOCK && IsHealingMoveEffect(gBattleMoves[move].effect))
        return TRUE;
    if (gProtectStructs[battlerAtk].palaceUnableToUseMove && move == gLastResultingMoves[battlerAtk])
        return TRUE;

    if (DoesBattlerIgnoreAbilityChecks(AI_DATA->abilities[battlerAtk], move)) {
        battlerDefAbility = ABILITY_NONE;
        partnerDefAbility = ABILITY_NONE;
    }
    else {
        battlerDefAbility = AI_DATA->abilities[battlerDef];
        partnerDefAbility = AI_GetBattlerAbility(BATTLE_PARTNER(battlerDef));
    }

    if (battlerDef == BATTLE_PARTNER(battlerAtk))
        battlerDefAbility = AI_DATA->abilities[battlerDef];

    if (gBattleStruct->battlerState[battlerDef].commandingDondozo)
        return TRUE;

    switch (battlerDefAbility)
    {
    case ABILITY_VOLT_ABSORB:
    case ABILITY_MOTOR_DRIVE:
    case ABILITY_LIGHTNING_ROD:
        if (moveType == TYPE_ELECTRIC)
            return TRUE;
        break;
    case ABILITY_WATER_ABSORB:
    case ABILITY_DRY_SKIN:
    case ABILITY_STORM_DRAIN:
        if (moveType == TYPE_WATER)
            return TRUE;
        break;
    case ABILITY_FLASH_FIRE:
        if (moveType == TYPE_FIRE)
            return TRUE;
        break;
    case ABILITY_SOUNDPROOF:
        if (gBattleMoves[move].soundMove)
            return TRUE;
        break;
    case ABILITY_BULLETPROOF:
        if (gBattleMoves[move].ballisticMove)
            return TRUE;
        break;
    case ABILITY_SAP_SIPPER:
        if (moveType == TYPE_GRASS)
            return TRUE;
        break;
    case ABILITY_EARTH_EATER:
        if (moveType == TYPE_GROUND)
            return TRUE;
        break;
    case ABILITY_DAZZLING:
    case ABILITY_QUEENLY_MAJESTY:
    case ABILITY_ARMOR_TAIL:
        if (GetMovePriority(battlerAtk, move) > 0)
            return TRUE;
        break;
    case ABILITY_WIND_RIDER:
        if (gBattleMoves[move].windMove)
            return TRUE;
        break;
    }

    switch(partnerDefAbility)
    {
    case ABILITY_LIGHTNING_ROD:
        if (moveType == TYPE_ELECTRIC)
            return TRUE;
        break;
    case ABILITY_STORM_DRAIN:
        if (moveType == TYPE_WATER)
            return TRUE;
        break;
    case ABILITY_DAZZLING:
    case ABILITY_QUEENLY_MAJESTY:
    case ABILITY_ARMOR_TAIL:
        if (GetMovePriority(battlerAtk, move) > 0)
            return TRUE;
        break;
    }

    switch (gBattleMoves[move].effect)
    {
    case EFFECT_DREAM_EATER:
        if (!AI_IsBattlerAsleepOrComatose(battlerDef))
            return TRUE;
        break;
    case EFFECT_GIGATON_HAMMER:
        if (gLastResultingMoves[battlerAtk] == MOVE_GIGATON_HAMMER)
            return TRUE;
        break;
    case EFFECT_BELCH:
        if (ItemId_GetPocket(GetUsedHeldItem(battlerAtk)) != POCKET_BERRIES)
            return TRUE;
        break;
    case EFFECT_LAST_RESORT:
        if (!CanUseLastResort(battlerAtk))
            return TRUE;
        break;
    case EFFECT_HIT_SET_REMOVE_TERRAIN:
    case EFFECT_REMOVE_TERRAIN:
        if (!(gFieldStatuses & STATUS_FIELD_TERRAIN_ANY))
            return TRUE;
        break;
    case EFFECT_POLTERGEIST:
        if (AI_DATA->items[battlerDef] == ITEM_NONE || gFieldStatuses & STATUS_FIELD_MAGIC_ROOM || GetBattlerAbility(battlerDef) == ABILITY_KLUTZ)
            return TRUE;
        break;
    case EFFECT_BURN_UP:
        if (!IS_BATTLER_OF_TYPE(battlerAtk, TYPE_FIRE))
            return TRUE;
        break;
    case EFFECT_FUTURE_SIGHT:
        if ((gSideStatuses[GetBattlerSide(battlerDef)] & SIDE_STATUS_FUTUREATTACK))
            return TRUE;
        break;
    case EFFECT_SUCKER_PUNCH:
        if (gLastPrintedMoves[battlerAtk] == MOVE_SUCKER_PUNCH && AI_RandLessThan(171)) 
            return TRUE;
        break;
    case EFFECT_METEOR_BEAM:
        if (move == MOVE_ELECTRO_SHOT && (gBattleWeather & B_WEATHER_RAIN))
            break;
        if (AI_DATA->holdEffects[battlerAtk] != HOLD_EFFECT_POWER_HERB)
            return TRUE;
        break;
    case EFFECT_FAKE_OUT:
        if (!gDisableStructs[battlerAtk].isFirstTurn)
            return TRUE;
        break;
    case EFFECT_TWO_TURNS_ATTACK:
        if (AI_DATA->holdEffects[battlerAtk] != HOLD_EFFECT_POWER_HERB)
            return TRUE;
        break;
    }

    return FALSE;
}

s32 AI_CalcDamage(u16 move, u8 battlerAtk, u8 battlerDef, u8 *typeEffectiveness, bool32 considerZPower, bool32 useRandomRoll)
{
    s32 dmg, moveType, critDmg, normalDmg, fixedBasePower;
    s8 critChance;
    uq4_12_t effectivenessMultiplier;
    bool32 isDamageMoveUnusable = FALSE;
    struct AiLogicData *aiData = AI_DATA; //Unused, this wasn't updated properly on switchin ai

    fixedBasePower = 0;

    if (considerZPower && IsViableZMove(battlerAtk, move))
    {
        //temporarily enable z moves for damage calcs
        gBattleStruct->zmove.baseMoves[battlerAtk] = move;
        gBattleStruct->zmove.active = TRUE;
    }

    if (gBattleMoves[move].effect == EFFECT_BOLT_BEAK 
        || gBattleMoves[move].effect == EFFECT_PAYBACK
        || gBattleMons[battlerAtk].ability == ABILITY_ANALYTIC
        || gBattleMons[battlerAtk].ability == ABILITY_STAKEOUT)
        gBattleStruct->noDoubleDamage = TRUE;
    else if (gBattleMoves[move].effect == EFFECT_PHOTON_GEYSER)
        gBattleStruct->swapDamageCategory = (GetSplitBasedOnStats(gBattlerAttacker) != gBattleMoves[gCurrentMove].split);
    else if (gBattleMoves[move].effect == EFFECT_SHELL_SIDE_ARM)
        gBattleStruct->swapDamageCategory = (gBattleStruct->shellSideArmCategory[battlerAtk][battlerDef] != gBattleMoves[gCurrentMove].split);
    else if (gBattleMoves[move].effect == EFFECT_NATURE_POWER)
        move = GetNaturePowerMove();

    gBattleStruct->dynamicMoveType = 0;

    SetTypeBeforeUsingMove(move, battlerAtk);
    GET_MOVE_TYPE(move, moveType);

    if (gBattleMoves[move].effect == EFFECT_TRIPLE_KICK)
        gMultiHitCounter = 3;

    if (gBattleMoves[move].power)
        isDamageMoveUnusable = IsDamageMoveUsable(move, battlerAtk, battlerDef);

    if (gBattleMoves[move].power && !isDamageMoveUnusable)
    {
        s32 critChanceIndex;
        // store battler type
        u32 type1 = gBattleMons[battlerAtk].type1;
        u32 type2 = gBattleMons[battlerAtk].type2;
        u32 type3 = gBattleMons[battlerAtk].type3;
        ProteanTryChangeType(battlerAtk, aiData->abilities[battlerAtk], move, moveType);
        critChanceIndex = CalcCritChanceStageArgs(battlerAtk, battlerDef, move, FALSE, gBattleMons[battlerAtk].ability, gBattleMons[battlerDef].ability, GetBattlerHoldEffect(battlerAtk, TRUE));
        
        if (critChanceIndex == -2 || critChanceIndex > 2) // Consider crit damage only if a move has at least +1 crit chance
        {
            critDmg = CalculateMoveDamage(move, battlerAtk, battlerDef, moveType, 0, TRUE, useRandomRoll, FALSE);
            dmg = critDmg;
        }
        else {
            normalDmg = CalculateMoveDamageAndEffectiveness(move, battlerAtk, battlerDef, moveType, fixedBasePower, &effectivenessMultiplier, useRandomRoll);
            dmg = normalDmg;
        }

        if (gBattleMons[battlerAtk].ability == ABILITY_ANALYTIC && gBattleMons[battlerAtk].speed < gBattleMons[battlerDef].speed) {
            dmg *= 1.3;
        }

        if (gBattleMons[battlerAtk].ability == ABILITY_PARENTAL_BOND) {
            dmg *= 1.25;
        }

        if (!gBattleStruct->zmove.active)
        {
            // Handle dynamic move damage
            switch (gBattleMoves[move].effect)
            {
            case EFFECT_WEATHER_BALL:
                if (gBattleMons[battlerAtk].species == SPECIES_CASTFORM) {
                    dmg *= 1.5;
                }
                break;
            case EFFECT_MULTI_HIT:
                if (AI_DATA->items[battlerAtk] != ITEM_LOADED_DICE)
                {
                    dmg *= (gBattleMons[battlerAtk].ability == ABILITY_SKILL_LINK ? 5 : 3);
                }
                else 
                {
                    dmg *= 4;
                }
                break;
            case EFFECT_TRIPLE_KICK:
                while (gMultiHitCounter-- > 1)
                    dmg += CalculateMoveDamageAndEffectiveness(move, battlerAtk, battlerDef, moveType, fixedBasePower, &effectivenessMultiplier, useRandomRoll);
                break;
            case EFFECT_ENDEAVOR:
                // If target has less HP than user, Endeavor does no damage
                effectivenessMultiplier = CalcTypeEffectivenessMultiplier(move, moveType, battlerAtk, battlerDef, FALSE);
                *typeEffectiveness = AI_GetEffectiveness(effectivenessMultiplier);
                if (*typeEffectiveness == AI_EFFECTIVENESS_x0)
                    dmg = 0;
                else
                    dmg = max(0, gBattleMons[battlerDef].hp - gBattleMons[battlerAtk].hp);
                break;
            case EFFECT_FINAL_GAMBIT:
                effectivenessMultiplier = CalcTypeEffectivenessMultiplier(move, moveType, battlerAtk, battlerDef, FALSE);
                *typeEffectiveness = AI_GetEffectiveness(effectivenessMultiplier);
                if (*typeEffectiveness == AI_EFFECTIVENESS_x0)
                    dmg = 0;
                else
                    dmg = gBattleMons[battlerAtk].hp;
                break;
            }

            // Handle other multi-strike moves
            if (gBattleMoves[move].strikeCount > 1 && gBattleMoves[move].effect != EFFECT_TRIPLE_KICK)
                dmg *= gBattleMoves[move].strikeCount;
                
            if (dmg == 0 && move != MOVE_ENDEAVOR && move != MOVE_FINAL_GAMBIT)
                dmg = 1;
            if (move == MOVE_COUNTER || move == MOVE_MIRROR_COAT || move == MOVE_BIDE || move == MOVE_METAL_BURST)
                dmg = 0;
        }
        // restore battler type
        gBattleMons[battlerAtk].type1 = type1;
        gBattleMons[battlerAtk].type2 = type2;
        gBattleMons[battlerAtk].type3 = type3;
    }
    else
    {
        effectivenessMultiplier = CalcTypeEffectivenessMultiplier(move, moveType, battlerAtk, battlerDef, FALSE);
        dmg = 0;
    }

    // convert multiper to AI_EFFECTIVENESS_xX
    *typeEffectiveness = AI_GetEffectiveness(effectivenessMultiplier);

    gBattleStruct->noDoubleDamage = FALSE;
    gBattleStruct->swapDamageCategory = FALSE;
    gBattleStruct->zmove.active = FALSE;
    gBattleStruct->zmove.baseMoves[battlerAtk] = MOVE_NONE;
    return dmg;
}

// Checks if one of the moves has side effects or perks
static u32 WhichMoveBetter(u32 move1, u32 move2)
{
    return 1;
}

u32 GetHighestDmg(u32 battlerAtk, u32 battlerDef) {
    u32 highestDmg = 0;
    u32 i = 0;
    for (i = 0; i < MAX_MON_MOVES; i++) {
        u32 dmg;
        dmg = AI_DATA->maxrollDmg[battlerAtk][battlerDef][i];
        if (dmg > highestDmg)
            highestDmg = dmg;
    }
    return highestDmg;
}

u32 GetNoOfHitsToKO(u32 dmg, s32 hp)
{
    if (dmg == 0)
        return UINT32_MAX;
    return (hp + dmg - 1) / dmg;
}

u32 GetNoOfHitsToKOBattlerDmg(u32 dmg, u32 battlerDef)
{
    u32 result;
    result = GetNoOfHitsToKO(dmg, gBattleMons[battlerDef].hp);
    if (result == 1 && IsBattlerSashSturdyProtected(battlerDef))
        result = 2;
    return result;
}

u32 GetNoOfHitsToKOBattler(u32 battlerAtk, u32 battlerDef, u32 moveIndex)
{
    return GetNoOfHitsToKOBattlerDmg(AI_DATA->simulatedDmg[battlerAtk][battlerDef][moveIndex], battlerDef);
}

u8 GetMoveDamageResult(u16 move)
{
    s32 i, checkedMove, bestId, currId, hp;
    s32 moveDmgs[MAX_MON_MOVES];
    u8 result;

    for (i = 0; sIgnoredPowerfulMoveEffects[i] != IGNORED_MOVES_END; i++)
    {
        if (gBattleMoves[move].effect == sIgnoredPowerfulMoveEffects[i])
            break;
    }

    if (gBattleMoves[move].power != 0 && sIgnoredPowerfulMoveEffects[i] == IGNORED_MOVES_END)
    {
        // Considered move has power and is not in sIgnoredPowerfulMoveEffects
        // Check all other moves and calculate their power
        for (checkedMove = 0; checkedMove < MAX_MON_MOVES; checkedMove++)
        {
            for (i = 0; sIgnoredPowerfulMoveEffects[i] != IGNORED_MOVES_END; i++)
            {
                if (gBattleMoves[gBattleMons[sBattler_AI].moves[checkedMove]].effect == sIgnoredPowerfulMoveEffects[i])
                    break;
            }

            if (gBattleMoves[gBattleMons[sBattler_AI].moves[checkedMove]].effect == EFFECT_HIT_SWITCH_TARGET
            && CanTargetFaintAi(gBattlerTarget, sBattler_AI))
            {
                moveDmgs[checkedMove] = 0;
                continue;
            }

            if (gBattleMons[sBattler_AI].moves[checkedMove] != MOVE_NONE
                && sIgnoredPowerfulMoveEffects[i] == IGNORED_MOVES_END
                && gBattleMoves[gBattleMons[sBattler_AI].moves[checkedMove]].power != 0)
            {
                moveDmgs[checkedMove] = AI_DATA->simulatedDmg[sBattler_AI][gBattlerTarget][checkedMove];
            }
            else
            {
                moveDmgs[checkedMove] = 0;
            }
        }

        hp = gBattleMons[gBattlerTarget].hp;
        // If a move can faint battler, it doesn't matter how much damage it does
        for (i = 0; i < MAX_MON_MOVES; i++)
            if (moveDmgs[i] > hp)
                moveDmgs[i] = hp;

        for (bestId = 0, i = 1; i < MAX_MON_MOVES; i++)
            if (moveDmgs[i] > moveDmgs[bestId])
                bestId = i;

        currId = AI_THINKING_STRUCT->movesetIndex;
        if (currId == bestId || (moveDmgs[currId] >= hp && moveDmgs[bestId] >= hp))
            AI_THINKING_STRUCT->funcResult = MOVE_POWER_BEST;
        // Compare percentage difference.
        else if ((moveDmgs[currId] >= hp || moveDmgs[bestId] < hp) // If current move can faint as well, or if neither can
                 && GetNoOfHitsToKO(moveDmgs[currId], hp) - GetNoOfHitsToKO(moveDmgs[bestId], hp) <= 2 // Consider a move weak if it needs to be used at least 2 times more to faint the target, compared to the best move.
                 && WhichMoveBetter(gBattleMons[sBattler_AI].moves[bestId], gBattleMons[sBattler_AI].moves[currId]) != 0)
            AI_THINKING_STRUCT->funcResult = MOVE_POWER_GOOD;
        else
            AI_THINKING_STRUCT->funcResult = MOVE_POWER_WEAK;
    }
    else
    {
        // Move has a power of 0/1, or is in the group sIgnoredPowerfulMoveEffects
        AI_THINKING_STRUCT->funcResult = MOVE_POWER_OTHER;
    }

    return AI_THINKING_STRUCT->funcResult;
}

u32 GetCurrDamageHpPercent(u8 battlerAtk, u8 battlerDef)
{
    int bestDmg = AI_DATA->simulatedDmg[battlerAtk][battlerDef][AI_THINKING_STRUCT->movesetIndex];

    return (bestDmg * 100) / gBattleMons[battlerDef].maxHP;
}

uq4_12_t AI_GetTypeEffectiveness(u16 move, u8 battlerAtk, u8 battlerDef)
{
    uq4_12_t typeEffectiveness;
    u16 moveType;

    gBattleStruct->dynamicMoveType = 0;
    SetTypeBeforeUsingMove(move, battlerAtk);
    GET_MOVE_TYPE(move, moveType);
    typeEffectiveness = CalcTypeEffectivenessMultiplier(move, moveType, battlerAtk, battlerDef, FALSE);

    return typeEffectiveness;
}

u32 AI_GetMoveEffectiveness(u16 move, u8 battlerAtk, u8 battlerDef)
{
    gMoveResultFlags = 0;
    return AI_GetEffectiveness(AI_GetTypeEffectiveness(move, battlerAtk, battlerDef));
}

static u32 AI_GetEffectiveness(uq4_12_t multiplier)
{
    switch (multiplier)
    {
    case UQ_4_12(0.0):
        return AI_EFFECTIVENESS_x0;
    case UQ_4_12(0.125):
        return AI_EFFECTIVENESS_x0_125;
    case UQ_4_12(0.25):
        return AI_EFFECTIVENESS_x0_25;
    case UQ_4_12(0.5):
        return AI_EFFECTIVENESS_x0_5;
    case UQ_4_12(1.0):
    default:
        return AI_EFFECTIVENESS_x1;
    case UQ_4_12(2.0):
        return AI_EFFECTIVENESS_x2;
    case UQ_4_12(4.0):
        return AI_EFFECTIVENESS_x4;
    case UQ_4_12(8.0):
        return AI_EFFECTIVENESS_x8;
    }
}

/* Checks to see if AI will move ahead of another battler
 * Output:
    * AI_IS_FASTER: is user(ai) faster
    * AI_IS_SLOWER: is target faster
*/
u8 AI_WhoStrikesFirst(u8 battlerAI, u8 battler2, u16 moveConsidered)
{
    u32 fasterAI = 0, fasterPlayer = 0, i;
    s8 prioAI = 0;
    s8 prioPlayer = 0;
    s8 prioBattler2 = 0;
    u16 *battler2Moves = GetMovesArray(battler2);

    // Check move priorities first.
    prioAI = GetMovePriority(battlerAI, moveConsidered);
    for (i = 0; i < MAX_MON_MOVES; i++)
    {
        prioBattler2 = GetMovePriority(battler2, battler2Moves[i]);
        if (battler2Moves[i] == MOVE_NONE || battler2Moves[i] == MOVE_UNAVAILABLE
            || (prioBattler2 > prioAI && !CanIndexMoveFaintTarget(battler2, battlerAI, i , 2)))
            continue;

        if (prioAI > prioBattler2)
            fasterAI++;
        else if (prioBattler2 > prioAI)
            fasterPlayer++;
    }

    if (fasterAI > fasterPlayer)
    {
        return AI_IS_FASTER;
    }
    else if (fasterAI < fasterPlayer)
    {
        return AI_IS_SLOWER;
    }
    else
    {
        if (prioAI > prioBattler2)
            return AI_IS_FASTER;    // if we didn't know any of battler 2's moves to compare priorities, assume they don't have a prio+ move
        // Priorities are the same(at least comparing to moves the AI is aware of), decide by speed.
        if (GetWhichBattlerFasterOrTies(battlerAI, battler2, TRUE) == 0)
            return AI_IS_FASTER;
        else
            return AI_IS_SLOWER;
    }
}

//Support is defined as having 1 or less damaging moves
//and not having any strong attacking moves that would make it want to attack instead of support.
//In this context, some support oriented moves like Fake Out and Endeavor are not considered damaging moves.
bool32 MonIsDedicatedSupport(u8 battlerAtk)
{
    u16 *moves = GetMovesArray(battlerAtk);
    u32 i, count = 0;

    if (gBattleMons[battlerAtk].ability == ABILITY_IMPOSTER)
        return FALSE;

    for (i = 0; i < MAX_MON_MOVES; i++)
    {
        //Special case Roles
        if ((gBattleMons[battlerAtk].species == SPECIES_CRAMORANT || gBattleMons[battlerAtk].species == SPECIES_CRAMORANT_GORGING || gBattleMons[battlerAtk].species == SPECIES_CRAMORANT_GULPING)
            && (moves[i] == MOVE_SURF || moves[i] == MOVE_DIVE))
                continue;
        if (gBattleMoves[moves[i]].power >= 80)
            return FALSE;
        if ((moves[i] != MOVE_NONE && moves[i] != MOVE_UNAVAILABLE) 
            && ((GetBattleMoveSplit(moves[i]) != SPLIT_STATUS) 
            && moves[i] != MOVE_ENDEAVOR && moves[i] != MOVE_FEINT
            && moves[i] != MOVE_FAKE_OUT && moves[i] != MOVE_KNOCK_OFF
            && gBattleMoves[moves[i]].effect != EFFECT_SPEED_DOWN_HIT
            && gBattleMoves[moves[i]].effect != EFFECT_SUPER_FANG)
            && gBattleMoves[moves[i]].effect != EFFECT_HIT_SWITCH_TARGET
            && gBattleMoves[moves[i]].effect != EFFECT_FUTURE_SIGHT
            && gBattleMoves[moves[i]].effect != EFFECT_FOUL_PLAY
            && gBattleMoves[moves[i]].effect != EFFECT_UPPER_HAND
            && gBattleMoves[moves[i]].effect != EFFECT_BUG_BITE
            && gBattleMoves[moves[i]].effect != EFFECT_HIT_ENEMY_HEAL_ALLY)
            count++;
    }
    return count <= 1;
}

bool32 WeatherTerrainMonShouldSwitch(u8 battlerAtk)
{
    u16 ability = AI_DATA->abilities[battlerAtk];
    if (gBattleWeather & B_WEATHER_NONE)
        if (ability == ABILITY_DROUGHT || ability == ABILITY_DRIZZLE || ability == ABILITY_SNOW_WARNING || ability == ABILITY_SAND_STREAM)
            return TRUE;
    if (!(gFieldStatuses & STATUS_FIELD_TERRAIN_ANY))
        if (ability == ABILITY_GRASSY_SURGE || ability == ABILITY_MISTY_SURGE || ability == ABILITY_ELECTRIC_SURGE || ability == ABILITY_PSYCHIC_SURGE)
            return TRUE;
    return FALSE;
}

// Check if target has means to faint ai mon.
bool32 CanTargetFaintAi(u8 battlerDef, u8 battlerAtk)
{
    s32 i, dmg;
    u32 unusable = AI_DATA->moveLimitations[battlerDef];
    u16 *moves = GetMovesArray(battlerDef);
    u8 effectiveness;

    for (i = 0; i < MAX_MON_MOVES; i++)
    {
        if (moves[i] != MOVE_NONE && moves[i] != MOVE_UNAVAILABLE && !(unusable & gBitTable[i])
            && AI_DATA->maxrollDmg[battlerDef][battlerAtk][i] >= gBattleMons[battlerAtk].hp)
        {
            return TRUE;
        }
    }

    return FALSE;
}

// Check if AI mon has the means to faint the target with any of its moves.
// If numHits > 1, check if the target will be KO'ed by that number of hits (ignoring healing effects)
bool32 CanAIFaintTarget(u8 battlerAtk, u8 battlerDef, u8 numHits)
{
    s32 i, dmg;
    u32 moveLimitations = AI_DATA->moveLimitations[battlerAtk];
    u16 *moves = gBattleMons[battlerAtk].moves;
    u8 effectiveness;

    for (i = 0; i < MAX_MON_MOVES; i++)
    {
        if (moves[i] != MOVE_NONE && moves[i] != MOVE_UNAVAILABLE && !(moveLimitations & gBitTable[i]))
        {
            dmg = AI_DATA->maxrollDmg[battlerAtk][battlerDef][i];

            if (numHits)
                dmg *= numHits;

            if (gBattleMons[battlerDef].hp <= dmg)
                return TRUE;
        }
    }

    return FALSE;
}

bool32 CanMoveFaintBattler(u16 move, u8 battlerDef, u8 battlerAtk, u8 nHits)
{
    s32 i, dmg;
    u8 effectiveness;
    u32 unusable = AI_DATA->moveLimitations[battlerDef];

    if (move != MOVE_NONE
      && move != 0xFFFF
      && !(unusable & gBitTable[i])
      && AI_CalcDamage(move, battlerDef, battlerAtk, &effectiveness, FALSE, FALSE) >= gBattleMons[battlerAtk].hp)
        return TRUE;

    return FALSE;
}

// Check if target has means to faint ai mon after modding hp/dmg
bool32 CanTargetFaintAiWithMod(u8 battlerDef, u8 battlerAtk, s32 hpMod, s32 dmgMod)
{
    u32 i;
    u32 unusable = AI_DATA->moveLimitations[battlerDef];
    s32 dmg;
    u16 *moves = gBattleResources->battleHistory->usedMoves[battlerDef];
    u32 hpCheck = gBattleMons[battlerAtk].hp + hpMod;

    if (hpCheck > gBattleMons[battlerAtk].maxHP)
        hpCheck = gBattleMons[battlerAtk].maxHP;

    for (i = 0; i < MAX_MON_MOVES; i++)
    {
        dmg = AI_DATA->simulatedDmg[battlerAtk][battlerDef][i];
        if (dmgMod)
            dmg *= dmgMod;

        if (moves[i] != MOVE_NONE && moves[i] != MOVE_UNAVAILABLE && !(unusable & gBitTable[i]) && dmg >= hpCheck)
        {
            return TRUE;
        }
    }

    return FALSE;
}

bool32 AI_IsAbilityOnSide(u32 battlerId, u32 ability)
{
    if (IsBattlerAlive(battlerId) && AI_DATA->abilities[battlerId] == ability)
        return TRUE;
    else if (IsBattlerAlive(BATTLE_PARTNER(battlerId)) && AI_DATA->abilities[BATTLE_PARTNER(battlerId)] == ability)
        return TRUE;
    else
        return FALSE;
}

u32 AI_GetBattlerAbility(u32 battler)
{
    if (gStatuses3[battler] & STATUS3_GASTRO_ACID)
        return ABILITY_NONE;

    if (IsNeutralizingGasOnField()
     && gBattleMons[battler].ability != ABILITY_NEUTRALIZING_GAS
     && GetBattlerHoldEffectIgnoreAbility(battler, TRUE) != HOLD_EFFECT_ABILITY_SHIELD)
        return ABILITY_NONE;

    return gBattleMons[battler].ability;
}

// does NOT include ability suppression checks
s32 AI_GetAbility(u32 battlerId)
{
    u32 knownAbility = GetBattlerAbility(battlerId);

    // We've had ability overwritten by e.g. Worry Seed. It is not part of AI_PARTY in case of switching
    if (gBattleStruct->overwrittenAbilities[battlerId])
        return gBattleStruct->overwrittenAbilities[battlerId];

    // The AI knows its own ability.
    if (IsAiBattlerAware(battlerId))
        return knownAbility;

    // Check neutralizing gas, gastro acid
    if (knownAbility == ABILITY_NONE)
        return knownAbility;

    if (AI_PARTY->mons[GetBattlerSide(battlerId)][gBattlerPartyIndexes[battlerId]].ability != ABILITY_NONE)
        return AI_PARTY->mons[GetBattlerSide(battlerId)][gBattlerPartyIndexes[battlerId]].ability;

    // Abilities that prevent fleeing - treat as always known
    if (knownAbility == ABILITY_SHADOW_TAG || knownAbility == ABILITY_MAGNET_PULL || knownAbility == ABILITY_ARENA_TRAP)
        return knownAbility;

    // Else, guess the ability
    if (gSpeciesInfo[gBattleMons[battlerId].species].abilities[0] != ABILITY_NONE)
    {
        u16 abilityGuess = ABILITY_NONE;
        while (abilityGuess == ABILITY_NONE)
        {
            abilityGuess = gSpeciesInfo[gBattleMons[battlerId].species].abilities[Random() % NUM_ABILITY_SLOTS];
        }

        return abilityGuess;
    }

    return ABILITY_NONE; // Unknown.
}

u16 AI_GetHoldEffect(u32 battlerId)
{
    u32 holdEffect;

    if (!IsAiBattlerAware(battlerId))
        holdEffect = AI_PARTY->mons[GetBattlerSide(battlerId)][gBattlerPartyIndexes[battlerId]].heldEffect;
    else
        holdEffect = GetBattlerHoldEffect(battlerId, FALSE);

    if (AI_THINKING_STRUCT->aiFlags & AI_FLAG_NEGATE_UNAWARE)
        return holdEffect;

    if (gStatuses3[battlerId] & STATUS3_EMBARGO)
        return HOLD_EFFECT_NONE;
    if (gFieldStatuses & STATUS_FIELD_MAGIC_ROOM)
        return HOLD_EFFECT_NONE;
    if (AI_DATA->abilities[battlerId] == ABILITY_KLUTZ && !(gStatuses3[battlerId] & STATUS3_GASTRO_ACID))
        return HOLD_EFFECT_NONE;

    return holdEffect;
}

bool32 AI_IsTerrainAffected(u8 battlerId, u32 flags)
{
    if (gStatuses3[battlerId] & STATUS3_SEMI_INVULNERABLE)
        return FALSE;
    else if (!(gFieldStatuses & flags))
        return FALSE;
    return AI_IsBattlerGrounded(battlerId);
}

// different from IsBattlerGrounded in that we don't always know battler's hold effect or ability
bool32 AI_IsBattlerGrounded(u32 battlerId)
{
    u32 holdEffect = AI_DATA->holdEffects[battlerId];

    if (holdEffect == HOLD_EFFECT_IRON_BALL)
        return TRUE;
    else if (gFieldStatuses & STATUS_FIELD_GRAVITY)
        return TRUE;
    else if (gStatuses3[battlerId] & STATUS3_ROOTED)
        return TRUE;
    else if (gStatuses3[battlerId] & STATUS3_SMACKED_DOWN)
        return TRUE;
    else if (gStatuses3[battlerId] & STATUS3_TELEKINESIS)
        return FALSE;
    else if (gStatuses3[battlerId] & STATUS3_MAGNET_RISE)
        return FALSE;
    else if (holdEffect == HOLD_EFFECT_AIR_BALLOON)
        return FALSE;
    else if (AI_DATA->abilities[battlerId] == ABILITY_LEVITATE)
        return FALSE;
    else if (IS_BATTLER_OF_TYPE(battlerId, TYPE_FLYING))
        return FALSE;
    else
        return TRUE;
}

bool32 DoesBattlerIgnoreAbilityChecks(u16 atkAbility, u16 move)
{
    u32 i;

    for (i = 0; i < ARRAY_COUNT(sIgnoreMoldBreakerMoves); i++)
    {
        if (move == sIgnoreMoldBreakerMoves[i])
            return TRUE;
    }

    if (atkAbility == ABILITY_MOLD_BREAKER
      || atkAbility == ABILITY_TERAVOLT
      || atkAbility == ABILITY_TURBOBLAZE
      || (atkAbility == ABILITY_MYCELIUM_MIGHT && IS_MOVE_STATUS(gCurrentMove) ))
        return TRUE;

    return FALSE;
}

bool32 AI_WeatherHasEffect(void)
{
    u32 i;
    if (AI_THINKING_STRUCT->aiFlags & AI_FLAG_NEGATE_UNAWARE)
        return TRUE;   // AI doesn't understand weather supression (handicap)

    return WEATHER_HAS_EFFECT;  // weather damping abilities are announced
}

u32 AI_GetBattlerMoveTargetType(u8 battlerId, u16 move)
{
    u32 target;

    if (gBattleMoves[move].effect == EFFECT_EXPANDING_FORCE && AI_IsTerrainAffected(battlerId, STATUS_FIELD_PSYCHIC_TERRAIN))
        return MOVE_TARGET_BOTH;
    else
        return gBattleMoves[move].target;
}

bool32 IsAromaVeilProtectedMove(u16 move)
{
    u32 i;

    switch (move)
    {
    case MOVE_DISABLE:
    case MOVE_ATTRACT:
    case MOVE_ENCORE:
    case MOVE_TORMENT:
    case MOVE_TAUNT:
    case MOVE_HEAL_BLOCK:
        return TRUE;
    default:
        return FALSE;
    }
}

bool32 IsNonVolatileStatusMoveEffect(u16 moveEffect)
{
    switch (moveEffect)
    {
    case EFFECT_SLEEP:
    case EFFECT_TOXIC:
    case EFFECT_POISON:
    case EFFECT_PARALYZE:
    case EFFECT_WILL_O_WISP:
    case EFFECT_YAWN:
        return TRUE;
    default:
        return FALSE;
    }
}

bool32 IsConfusionMoveEffect(u16 moveEffect)
{
    switch (moveEffect)
    {
    case EFFECT_CONFUSE:
    case EFFECT_SWAGGER:
    case EFFECT_FLATTER:
        return TRUE;
    default:
        return FALSE;
    }
}

bool32 IsStatLoweringMoveEffect(u16 moveEffect)
{
    switch (moveEffect)
    {
    case EFFECT_ATTACK_DOWN:
    case EFFECT_DEFENSE_DOWN:
    case EFFECT_SPEED_DOWN:
    case EFFECT_SPECIAL_ATTACK_DOWN:
    case EFFECT_SPECIAL_DEFENSE_DOWN:
    case EFFECT_ACCURACY_DOWN:
    case EFFECT_EVASION_DOWN:
    case EFFECT_ATTACK_DOWN_2:
    case EFFECT_DEFENSE_DOWN_2:
    case EFFECT_SPEED_DOWN_2:
    case EFFECT_SPECIAL_ATTACK_DOWN_2:
    case EFFECT_SPECIAL_DEFENSE_DOWN_2:
    case EFFECT_ACCURACY_DOWN_2:
    case EFFECT_EVASION_DOWN_2:
        return TRUE;
    default:
        return FALSE;
    }
}

bool32 IsHazardMoveEffect(u16 moveEffect)
{
    switch (moveEffect)
    {
    case EFFECT_SPIKES:
    case EFFECT_TOXIC_SPIKES:
    case EFFECT_STICKY_WEB:
    case EFFECT_STEALTH_ROCK:
        return TRUE;
    default:
        return FALSE;
    }
}

bool32 IsMoveRedirectionPrevented(u16 move, u16 atkAbility)
{
    if (AI_THINKING_STRUCT->aiFlags & AI_FLAG_NEGATE_UNAWARE)
        return FALSE;

    if (move == MOVE_SKY_DROP
      || move == MOVE_SNIPE_SHOT
      || atkAbility == ABILITY_PROPELLER_TAIL
      || atkAbility == ABILITY_STALWART)
        return TRUE;
    return FALSE;
}

u32 AI_GetMoveAccuracy(u8 battlerAtk, u8 battlerDef, u16 move)
{
    return GetTotalAccuracy(battlerAtk, battlerDef, move, AI_DATA->abilities[battlerAtk], AI_DATA->abilities[battlerDef],
                            AI_DATA->holdEffects[battlerAtk], AI_DATA->holdEffects[battlerDef]);
}

bool32 IsSemiInvulnerable(u8 battlerDef, u16 move)
{
    if (gStatuses3[battlerDef] & STATUS3_PHANTOM_FORCE)
        return TRUE;
    else if (gBattleStruct->battlerState[battlerDef].commandingDondozo)
        return TRUE;
    else if (!gBattleMoves[move].damagesAirborne && gStatuses3[battlerDef] & STATUS3_ON_AIR)
        return TRUE;
    else if (!gBattleMoves[move].damagesUnderwater && gStatuses3[battlerDef] & STATUS3_UNDERWATER)
        return TRUE;
    else if (!gBattleMoves[move].damagesUnderground && gStatuses3[battlerDef] & STATUS3_UNDERGROUND)
        return TRUE;
    else
        return FALSE;
}

bool32 IsMoveEncouragedToHit(u8 battlerAtk, u8 battlerDef, u16 move)
{
    if (IsSemiInvulnerable(battlerDef, move))
        return FALSE;

    //TODO - anticipate protect move?

    // always hits
    if (gStatuses3[battlerDef] & STATUS3_ALWAYS_HITS || gDisableStructs[battlerDef].battlerWithSureHit == battlerAtk)
        return TRUE;

    if (AI_DATA->abilities[battlerDef] == ABILITY_NO_GUARD || AI_DATA->abilities[battlerAtk] == ABILITY_NO_GUARD)
        return TRUE;

#if B_TOXIC_NEVER_MISS >= GEN_6
    if (gBattleMoves[move].effect == EFFECT_TOXIC && IS_BATTLER_OF_TYPE(battlerAtk, TYPE_POISON))
        return TRUE;
#endif

    // discouraged from hitting
    if (AI_WeatherHasEffect() && (gBattleWeather & B_WEATHER_SUN)
      && (gBattleMoves[move].effect == EFFECT_THUNDER || gBattleMoves[move].effect == EFFECT_HURRICANE))
        return FALSE;

    // increased accuracy but don't always hit
    if ((AI_WeatherHasEffect() &&
            (((gBattleWeather & B_WEATHER_RAIN) && (gBattleMoves[move].effect == EFFECT_THUNDER || gBattleMoves[move].effect == EFFECT_HURRICANE))
            || (((gBattleWeather & (B_WEATHER_HAIL | B_WEATHER_SNOW)) && move == MOVE_BLIZZARD))))
        || (gBattleMoves[move].effect == EFFECT_VITAL_THROW)
    #if B_MINIMIZE_DMG_ACC >= GEN_6
        || ((gStatuses3[battlerDef] & STATUS3_MINIMIZED) && gBattleMoves[move].minimizeDoubleDamage)
    #endif
        || (gBattleMoves[move].accuracy == 0))
    {
        return TRUE;
    }

    return FALSE;
}

bool32 ShouldTryOHKO(u8 battlerAtk, u8 battlerDef, u16 atkAbility, u16 defAbility, u16 move)
{
    u32 holdEffect = AI_DATA->holdEffects[battlerDef];
    u32 accuracy = AI_GetMoveAccuracy(battlerAtk, battlerDef, move);

    gPotentialItemEffectBattler = battlerDef;
    if (holdEffect == HOLD_EFFECT_FOCUS_BAND)
        return FALSE;   //probabilistically speaking, focus band should activate so dont OHKO
    else if (holdEffect == HOLD_EFFECT_FOCUS_SASH && AtMaxHp(battlerDef))
        return FALSE;

    if (!DoesBattlerIgnoreAbilityChecks(atkAbility, move) && defAbility == ABILITY_STURDY)
        return FALSE;

    if ((((gStatuses3[battlerDef] & STATUS3_ALWAYS_HITS)
        && gDisableStructs[battlerDef].battlerWithSureHit == battlerAtk)
        || atkAbility == ABILITY_NO_GUARD || defAbility == ABILITY_NO_GUARD)
        && gBattleMons[battlerAtk].level >= gBattleMons[battlerDef].level)
    {
        return TRUE;
    }
    else    // test the odds
    {
        u16 odds = accuracy + (gBattleMons[battlerAtk].level - gBattleMons[battlerDef].level);
    #if B_SHEER_COLD_ACC >= GEN_7
        if (gCurrentMove == MOVE_SHEER_COLD && !IS_BATTLER_OF_TYPE(gBattlerAttacker, TYPE_ICE))
            odds -= 10;
    #endif
        if (odds > 10)
            return TRUE;
    }
    return FALSE;
}

bool32 ShouldSetSandstorm(u8 battler, u16 ability, u16 holdEffect)
{
    if (!AI_WeatherHasEffect())
        return FALSE;
    else if (gBattleWeather & B_WEATHER_SANDSTORM)
        return FALSE;

    if (ability == ABILITY_SAND_VEIL
      || ability == ABILITY_SAND_RUSH
      || ability == ABILITY_SAND_FORCE
      || ability == ABILITY_OVERCOAT
      || ability == ABILITY_MAGIC_GUARD
      || holdEffect == HOLD_EFFECT_SAFETY_GOGGLES
      || IS_BATTLER_OF_TYPE(battler, TYPE_ROCK)
      || IS_BATTLER_OF_TYPE(battler, TYPE_STEEL)
      || IS_BATTLER_OF_TYPE(battler, TYPE_GROUND)
      || HasMoveEffect(battler, EFFECT_SHORE_UP)
      || HasMoveEffect(battler, EFFECT_WEATHER_BALL))
    {
        return TRUE;
    }
    return FALSE;
}

bool32 ShouldSetHail(u8 battler, u16 ability, u16 holdEffect)
{
    if (!AI_WeatherHasEffect())
        return FALSE;
    else if (gBattleWeather & (B_WEATHER_HAIL | B_WEATHER_SNOW))
        return FALSE;

    if (ability == ABILITY_SNOW_CLOAK
      || ability == ABILITY_ICE_BODY
      || ability == ABILITY_FORECAST
      || ability == ABILITY_SLUSH_RUSH
      || ability == ABILITY_MAGIC_GUARD
      || ability == ABILITY_OVERCOAT
      || holdEffect == HOLD_EFFECT_SAFETY_GOGGLES
      || IS_BATTLER_OF_TYPE(battler, TYPE_ICE)
      || HasMove(battler, MOVE_BLIZZARD)
      || HasMoveEffect(battler, EFFECT_AURORA_VEIL)
      || HasMoveEffect(battler, EFFECT_WEATHER_BALL))
    {
        return TRUE;
    }
    return FALSE;
}

bool32 ShouldSetRain(u8 battlerAtk, u16 atkAbility, u16 holdEffect)
{
    if (!AI_WeatherHasEffect())
        return FALSE;
    else if (gBattleWeather & B_WEATHER_RAIN)
        return FALSE;

    if (holdEffect != HOLD_EFFECT_UTILITY_UMBRELLA
     && (atkAbility == ABILITY_SWIFT_SWIM
      || atkAbility == ABILITY_FORECAST
      || atkAbility == ABILITY_HYDRATION
      || atkAbility == ABILITY_RAIN_DISH
      || atkAbility == ABILITY_DRY_SKIN
      || HasMoveEffect(battlerAtk, EFFECT_THUNDER)
      || HasMoveEffect(battlerAtk, EFFECT_HURRICANE)
      || HasMoveEffect(battlerAtk, EFFECT_WEATHER_BALL)
      || HasMoveWithType(battlerAtk, TYPE_WATER)))
    {
        return TRUE;
    }
    return FALSE;
}

bool32 ShouldSetSun(u8 battlerAtk, u16 atkAbility, u16 holdEffect)
{
    if (!AI_WeatherHasEffect())
        return FALSE;
    else if (gBattleWeather & B_WEATHER_SUN)
        return FALSE;

    if (holdEffect != HOLD_EFFECT_UTILITY_UMBRELLA
     && (atkAbility == ABILITY_CHLOROPHYLL
      || atkAbility == ABILITY_FLOWER_GIFT
      || atkAbility == ABILITY_FORECAST
      || atkAbility == ABILITY_LEAF_GUARD
      || atkAbility == ABILITY_SOLAR_POWER
      || atkAbility == ABILITY_HARVEST
      || HasMoveEffect(battlerAtk, EFFECT_SOLAR_BEAM)
      || HasMoveEffect(battlerAtk, EFFECT_MORNING_SUN)
      || HasMoveEffect(battlerAtk, EFFECT_SYNTHESIS)
      || HasMoveEffect(battlerAtk, EFFECT_MOONLIGHT)
      || HasMoveEffect(battlerAtk, EFFECT_WEATHER_BALL)
      || HasMoveEffect(battlerAtk, EFFECT_GROWTH)
      || HasMoveWithType(battlerAtk, TYPE_FIRE)))
    {
        return TRUE;
    }
    return FALSE;
}

bool32 ShouldSetSnow(u8 battler, u16 ability, u16 holdEffect)
{
    if (!AI_WeatherHasEffect())
        return FALSE;
    else if (gBattleWeather & (B_WEATHER_SNOW | B_WEATHER_HAIL))
        return FALSE;

    if (ability == ABILITY_SNOW_CLOAK
      || ability == ABILITY_ICE_BODY
      || ability == ABILITY_FORECAST
      || ability == ABILITY_SLUSH_RUSH
      || IS_BATTLER_OF_TYPE(battler, TYPE_ICE)
      || HasMove(battler, MOVE_BLIZZARD)
      || HasMoveEffect(battler, EFFECT_AURORA_VEIL)
      || HasMoveEffect(battler, EFFECT_WEATHER_BALL))
    {
        return TRUE;
    }
    return FALSE;
}

void ProtectChecks(u8 battlerAtk, u8 battlerDef, u16 move, u16 predictedMove, s16 *score)
{
    // TODO more sophisticated logic
    u16 predictedEffect = gBattleMoves[predictedMove].effect;
    u8 defAbility = AI_DATA->abilities[battlerDef];
    u32 uses = gDisableStructs[battlerAtk].protectUses;

    /*if (GetMoveResultFlags(predictedMove) & (MOVE_RESULT_NO_EFFECT | MOVE_RESULT_MISSED))
    {
        (*score) -= 5;
        return;
    }*/

    if (uses == 0)
    {
        if (predictedMove != MOVE_NONE && predictedMove != 0xFFFF && !IS_MOVE_STATUS(predictedMove))
            (*score) += 2;
        else if (Random() % 256 < 100)
            (*score)++;
    }
    else
    {
        if (IsDoubleBattle())
            (*score) -= 2 * min(uses, 3);
        else
            (*score) -= min(uses, 3);
    }

    if (gBattleMons[battlerAtk].status1 & (STATUS1_PSN_ANY | STATUS1_BURN | STATUS1_FROSTBITE)
     || gBattleMons[battlerAtk].status2 & (STATUS2_CURSED | STATUS2_INFATUATION)
     || gStatuses3[battlerAtk] & (STATUS3_PERISH_SONG | STATUS3_LEECHSEED | STATUS3_YAWN))
    {
        (*score)--;
    }

    if (gBattleMons[battlerDef].status1 & STATUS1_TOXIC_POISON
      || gBattleMons[battlerDef].status2 & (STATUS2_CURSED | STATUS2_INFATUATION)
      || gStatuses3[battlerDef] & (STATUS3_PERISH_SONG | STATUS3_LEECHSEED | STATUS3_YAWN))
        (*score) += 2;
}

// stat stages
bool32 ShouldLowerStat(u8 battler, u16 battlerAbility, u8 stat)
{
    if ((gBattleMons[battler].statStages[stat] > MIN_STAT_STAGE && battlerAbility != ABILITY_CONTRARY)
      || (battlerAbility == ABILITY_CONTRARY && gBattleMons[battler].statStages[stat] < MAX_STAT_STAGE))
    {
        if (AI_DATA->holdEffects[battler] == HOLD_EFFECT_CLEAR_AMULET
         || battlerAbility == ABILITY_MIRROR_ARMOR
         || battlerAbility == ABILITY_CLEAR_BODY
         || battlerAbility == ABILITY_WHITE_SMOKE
         || battlerAbility == ABILITY_FULL_METAL_BODY)
            return FALSE;

        return TRUE;
    }

    return FALSE;
}

bool32 BattlerStatCanRise(u8 battler, u16 battlerAbility, u8 stat)
{
    if ((gBattleMons[battler].statStages[stat] < MAX_STAT_STAGE && battlerAbility != ABILITY_CONTRARY)
      || (battlerAbility == ABILITY_CONTRARY && gBattleMons[battler].statStages[stat] > MIN_STAT_STAGE))
        return TRUE;
    return FALSE;
}

bool32 AreBattlersStatsMaxed(u8 battlerId)
{
    u32 i;
    for (i = STAT_ATK; i < NUM_BATTLE_STATS; i++)
    {
        if (gBattleMons[battlerId].statStages[i] < MAX_STAT_STAGE)
            return FALSE;
    }
    return TRUE;
}

bool32 AnyStatIsRaised(u8 battlerId)
{
    u32 i;

    for (i = STAT_ATK; i < NUM_BATTLE_STATS; i++)
    {
        if (gBattleMons[battlerId].statStages[i] > DEFAULT_STAT_STAGE)
            return TRUE;
    }
    return FALSE;
}

u32 CountPositiveStatStages(u8 battlerId)
{
    u32 count = 0;
    u32 i;
    for (i = STAT_ATK; i < NUM_BATTLE_STATS; i++)
    {
        if (gBattleMons[battlerId].statStages[i] > DEFAULT_STAT_STAGE)
            count++;
    }
    return count;
}

u32 CountNegativeStatStages(u8 battlerId)
{
    u32 count = 0;
    u32 i;
    for (i = STAT_ATK; i < NUM_BATTLE_STATS; i++)
    {
        if (gBattleMons[battlerId].statStages[i] < DEFAULT_STAT_STAGE)
            count++;
    }
    return count;
}

bool32 ShouldLowerAttack(u8 battlerAtk, u8 battlerDef, u16 defAbility)
{
    if (WillAIStrikeFirst() && (AI_THINKING_STRUCT->aiFlags & AI_FLAG_TRY_TO_FAINT) && CanAIFaintTarget(battlerAtk, battlerDef, 0))
        return FALSE; // Don't bother lowering stats if can kill enemy.

    if (gBattleMons[battlerDef].statStages[STAT_ATK] > 4
      && HasMoveWithSplit(battlerDef, SPLIT_PHYSICAL)
      && defAbility != ABILITY_CONTRARY
      && defAbility != ABILITY_MIRROR_ARMOR
      && defAbility != ABILITY_CLEAR_BODY
      && defAbility != ABILITY_WHITE_SMOKE
      && defAbility != ABILITY_FULL_METAL_BODY
      && defAbility != ABILITY_HYPER_CUTTER
      && AI_DATA->holdEffects[battlerDef] != HOLD_EFFECT_CLEAR_AMULET)
        return TRUE;
    return FALSE;
}

bool32 ShouldLowerDefense(u8 battlerAtk, u8 battlerDef, u16 defAbility)
{
    if (WillAIStrikeFirst() && (AI_THINKING_STRUCT->aiFlags & AI_FLAG_TRY_TO_FAINT) && CanAIFaintTarget(battlerAtk, battlerDef, 0))
        return FALSE; // Don't bother lowering stats if can kill enemy.

    if (gBattleMons[battlerDef].statStages[STAT_DEF] > 4
      && HasMoveWithSplit(battlerAtk, SPLIT_PHYSICAL)
      && defAbility != ABILITY_CONTRARY
      && defAbility != ABILITY_MIRROR_ARMOR
      && defAbility != ABILITY_CLEAR_BODY
      && defAbility != ABILITY_WHITE_SMOKE
      && defAbility != ABILITY_FULL_METAL_BODY
      && defAbility != ABILITY_BIG_PECKS
      && AI_DATA->holdEffects[battlerDef] != HOLD_EFFECT_CLEAR_AMULET)
        return TRUE;
    return FALSE;
}

bool32 ShouldLowerSpeed(u8 battlerAtk, u8 battlerDef, u16 defAbility)
{
    if (WillAIStrikeFirst() && (AI_THINKING_STRUCT->aiFlags & AI_FLAG_TRY_TO_FAINT) && CanAIFaintTarget(battlerAtk, battlerDef, 0))
        return FALSE; // Don't bother lowering stats if can kill enemy.

    if (!WillAIStrikeFirst()
      && defAbility != ABILITY_CONTRARY
      && defAbility != ABILITY_MIRROR_ARMOR
      && defAbility != ABILITY_CLEAR_BODY
      && defAbility != ABILITY_FULL_METAL_BODY
      && defAbility != ABILITY_WHITE_SMOKE
      && AI_DATA->holdEffects[battlerDef] != HOLD_EFFECT_CLEAR_AMULET)
        return TRUE;
    return FALSE;
}

bool32 ShouldLowerSpAtk(u8 battlerAtk, u8 battlerDef, u16 defAbility)
{
    if (WillAIStrikeFirst() && (AI_THINKING_STRUCT->aiFlags & AI_FLAG_TRY_TO_FAINT) && CanAIFaintTarget(battlerAtk, battlerDef, 0))
        return FALSE; // Don't bother lowering stats if can kill enemy.

    if (gBattleMons[battlerDef].statStages[STAT_SPATK] > 4
      && HasMoveWithSplit(battlerDef, SPLIT_SPECIAL)
      && defAbility != ABILITY_CONTRARY
      && defAbility != ABILITY_MIRROR_ARMOR
      && defAbility != ABILITY_CLEAR_BODY
      && defAbility != ABILITY_FULL_METAL_BODY
      && defAbility != ABILITY_WHITE_SMOKE
      && AI_DATA->holdEffects[battlerDef] != HOLD_EFFECT_CLEAR_AMULET)
        return TRUE;
    return FALSE;
}

bool32 ShouldLowerSpDef(u8 battlerAtk, u8 battlerDef, u16 defAbility)
{
    if (WillAIStrikeFirst() && (AI_THINKING_STRUCT->aiFlags & AI_FLAG_TRY_TO_FAINT) && CanAIFaintTarget(battlerAtk, battlerDef, 0))
        return FALSE; // Don't bother lowering stats if can kill enemy.

    if (gBattleMons[battlerDef].statStages[STAT_SPDEF] > 4
      && HasMoveWithSplit(battlerAtk, SPLIT_SPECIAL)
      && defAbility != ABILITY_CONTRARY
      && defAbility != ABILITY_MIRROR_ARMOR
      && defAbility != ABILITY_CLEAR_BODY
      && defAbility != ABILITY_FULL_METAL_BODY
      && defAbility != ABILITY_WHITE_SMOKE
      && AI_DATA->holdEffects[battlerDef] != HOLD_EFFECT_CLEAR_AMULET)
        return TRUE;
    return FALSE;
}

bool32 ShouldLowerAccuracy(u8 battlerAtk, u8 battlerDef, u16 defAbility)
{
    if (WillAIStrikeFirst() && (AI_THINKING_STRUCT->aiFlags & AI_FLAG_TRY_TO_FAINT) && CanAIFaintTarget(battlerAtk, battlerDef, 0))
        return FALSE; // Don't bother lowering stats if can kill enemy.

    if (defAbility != ABILITY_CONTRARY
      && defAbility != ABILITY_MIRROR_ARMOR
      && defAbility != ABILITY_CLEAR_BODY
      && defAbility != ABILITY_WHITE_SMOKE
      && defAbility != ABILITY_FULL_METAL_BODY
      && defAbility != ABILITY_KEEN_EYE
      && defAbility != ABILITY_MINDS_EYE
      && AI_DATA->holdEffects[battlerDef] != HOLD_EFFECT_CLEAR_AMULET)
        return TRUE;
    return FALSE;
}

bool32 ShouldLowerEvasion(u8 battlerAtk, u8 battlerDef, u16 defAbility)
{
    if (WillAIStrikeFirst() && (AI_THINKING_STRUCT->aiFlags & AI_FLAG_TRY_TO_FAINT) && CanAIFaintTarget(battlerAtk, battlerDef, 0))
        return FALSE; // Don't bother lowering stats if can kill enemy.

    if (gBattleMons[battlerDef].statStages[STAT_EVASION] > DEFAULT_STAT_STAGE
      && defAbility != ABILITY_CONTRARY
      && defAbility != ABILITY_MIRROR_ARMOR
      && defAbility != ABILITY_CLEAR_BODY
      && defAbility != ABILITY_FULL_METAL_BODY
      && defAbility != ABILITY_WHITE_SMOKE
      && AI_DATA->holdEffects[battlerDef] != HOLD_EFFECT_CLEAR_AMULET)
        return TRUE;
    return FALSE;
}

bool32 CanIndexMoveFaintTarget(u8 battlerAtk, u8 battlerDef, u8 index, u8 numHits)
{
    s32 dmg = AI_DATA->simulatedDmg[battlerAtk][battlerDef][index];

    if (numHits)
        dmg *= numHits;

    if (gBattleMons[battlerDef].hp <= dmg)
        return TRUE;
    return FALSE;
}

u16 *GetMovesArray(u32 battler)
{
    if (IsAiBattlerAware(battler) || IsAiBattlerAware(BATTLE_PARTNER(battler)))
        return gBattleMons[battler].moves;
    else
        return gBattleResources->battleHistory->usedMoves[battler];
}

bool32 HasOnlyMovesWithSplit(u32 battlerId, u32 split, bool32 onlyOffensive)
{
    u32 i;
    u16 *moves = GetMovesArray(battlerId);

    for (i = 0; i < MAX_MON_MOVES; i++)
    {
        if (onlyOffensive && IS_MOVE_STATUS(moves[i]))
            continue;
        if (moves[i] != MOVE_NONE && moves[i] != MOVE_UNAVAILABLE && GetBattleMoveSplit(moves[i]) != split)
            return FALSE;
    }

    return TRUE;
}

bool32 HasMoveWithSplit(u32 battler, u32 split)
{
    u32 i;
    u16 *moves = GetMovesArray(battler);

    for (i = 0; i < MAX_MON_MOVES; i++)
    {
        if (moves[i] != MOVE_NONE && moves[i] != MOVE_UNAVAILABLE && GetBattleMoveSplit(moves[i]) == split)
            return TRUE;
    }

    return FALSE;
}

bool32 HasMoveWithAttackPrio(u32 battler)
{
    u32 i;
    u16 *moves = GetMovesArray(battler);

    for (i = 0; i < MAX_MON_MOVES; i++)
    {
        if (moves[i] != MOVE_NONE && moves[i] != MOVE_UNAVAILABLE 
            && GetBattleMoveSplit(moves[i]) != SPLIT_STATUS 
            && GetMovePriority(battler, moves[i]) > 0)
            return TRUE;
    }

    return FALSE;
}

bool32 HasMoveWithType(u32 battler, u8 type)
{
    s32 i;
    u16 *moves = GetMovesArray(battler);

    for (i = 0; i < MAX_MON_MOVES; i++)
    {
        if (moves[i] != MOVE_NONE && moves[i] != MOVE_UNAVAILABLE && gBattleMoves[moves[i]].type == type)
            return TRUE;
    }

    return FALSE;
}

bool32 HasMoveEffect(u32 battlerId, u16 moveEffect)
{
    s32 i;
    u16 *moves = GetMovesArray(battlerId);

    for (i = 0; i < MAX_MON_MOVES; i++)
    {
        if (moves[i] != MOVE_NONE && moves[i] != MOVE_UNAVAILABLE && gBattleMoves[moves[i]].effect == moveEffect)
            return TRUE;
    }

    return FALSE;
}

bool32 HasMove(u32 battlerId, u32 move)
{
    s32 i;
    u16 *moves = GetMovesArray(battlerId);

    for (i = 0; i < MAX_MON_MOVES; i++)
    {
        if (moves[i] != MOVE_NONE && moves[i] != MOVE_UNAVAILABLE && moves[i] == move)
            return TRUE;
    }

    return FALSE;
}

bool32 HasMoveWithLowAccuracy(u8 battlerAtk, u8 battlerDef, u8 accCheck, bool32 ignoreStatus, u16 atkAbility, u16 defAbility, u16 atkHoldEffect, u16 defHoldEffect)
{
    s32 i;
    u16 *moves = GetMovesArray(battlerAtk);
    u8 moveLimitations = AI_DATA->moveLimitations[battlerAtk];

    for (i = 0; i < MAX_MON_MOVES; i++)
    {
        if (moves[i] == MOVE_NONE || moves[i] == MOVE_UNAVAILABLE)
            continue;

        if (!(gBitTable[i] & moveLimitations))
        {
            if (ignoreStatus && IS_MOVE_STATUS(moves[i]))
                continue;
            else if ((!IS_MOVE_STATUS(moves[i]) && gBattleMoves[moves[i]].accuracy == 0)
              || AI_GetBattlerMoveTargetType(battlerAtk, moves[i]) & (MOVE_TARGET_USER | MOVE_TARGET_OPPONENTS_FIELD))
                continue;

            if (AI_GetMoveAccuracy(battlerAtk, battlerDef, moves[i]) <= accCheck)
                return TRUE;
        }
    }

    return FALSE;
}

bool32 HasSleepMoveWithLowAccuracy(u8 battlerAtk, u8 battlerDef)
{
    u8 moveLimitations = AI_DATA->moveLimitations[battlerAtk];
    u32 i;
    u16 *moves = GetMovesArray(battlerAtk);

    for (i = 0; i < MAX_MON_MOVES; i++)
    {
        if (moves[i] == MOVE_NONE)
            break;
        if (!(gBitTable[i] & moveLimitations))
        {
            if (gBattleMoves[moves[i]].effect == EFFECT_SLEEP
              && AI_GetMoveAccuracy(battlerAtk, battlerDef, moves[i]) < 85)
                return TRUE;
        }
    }
    return FALSE;
}

bool32 IsHealingMoveEffect(u16 effect)
{
    switch (effect)
    {
    case EFFECT_RESTORE_HP:
    case EFFECT_MORNING_SUN:
    case EFFECT_SYNTHESIS:
    case EFFECT_MOONLIGHT:
    case EFFECT_SOFTBOILED:
    case EFFECT_ROOST:
    case EFFECT_SWALLOW:
    case EFFECT_WISH:
    case EFFECT_HEALING_WISH:
    case EFFECT_HEAL_PULSE:
    case EFFECT_REST:
    case EFFECT_JUNGLE_HEALING:
        return TRUE;
    default:
        return FALSE;
    }
}

bool32 HasHealingEffect(u32 battlerId)
{
    s32 i;
    u16 *moves = GetMovesArray(battlerId);

    for (i = 0; i < MAX_MON_MOVES; i++)
    {
        if (moves[i] != MOVE_NONE && moves[i] != MOVE_UNAVAILABLE && IsHealingMoveEffect(gBattleMoves[moves[i]].effect))
            return TRUE;
    }

    return FALSE;
}

bool32 IsTrappingMoveEffect(u16 effect)
{
    switch (effect)
    {
    case EFFECT_MEAN_LOOK:
    case EFFECT_TRAP:
    case EFFECT_HIT_PREVENT_ESCAPE:
    case EFFECT_FAIRY_LOCK:
    //case EFFECT_NO_RETREAT:   // TODO
        return TRUE;
    default:
        return FALSE;
    }
}

bool32 HasTrappingMoveEffect(u8 battler)
{
    s32 i;
    u16 *moves = GetMovesArray(battler);

    for (i = 0; i < MAX_MON_MOVES; i++)
    {
        if (moves[i] != MOVE_NONE && moves[i] != MOVE_UNAVAILABLE && IsTrappingMoveEffect(gBattleMoves[moves[i]].effect))
            return TRUE;
    }

    return FALSE;
}

bool32 HasThawingMove(u8 battler)
{
    CHECK_MOVE_FLAG(thawsUser);
}

bool32 IsUngroundingEffect(u16 effect)
{
    switch (effect)
    {
    case EFFECT_MAGNET_RISE:
        return TRUE;
    default:
        return FALSE;
    }
}

// for anger point
bool32 IsAttackBoostMoveEffect(u16 effect)
{
    switch (effect)
    {
    case EFFECT_ATTACK_UP:
	case EFFECT_ATTACK_UP_2:
    case EFFECT_ATTACK_ACCURACY_UP:
    case EFFECT_ATTACK_SPATK_UP:
    case EFFECT_DRAGON_DANCE:
    case EFFECT_COIL:
    case EFFECT_BELLY_DRUM:
    case EFFECT_BULK_UP:
    case EFFECT_GROWTH:
        return TRUE;
    default:
        return FALSE;
    }
}

bool32 IsStatRaisingEffect(u16 effect)
{
    switch (effect)
    {
    case EFFECT_ATTACK_UP:
	case EFFECT_ATTACK_UP_2:
	case EFFECT_DEFENSE_UP:
	case EFFECT_DEFENSE_UP_2:
    case EFFECT_DEFENSE_UP_3:
	case EFFECT_SPEED_UP:
	case EFFECT_SPEED_UP_2:
	case EFFECT_SPECIAL_ATTACK_UP:
	case EFFECT_SPECIAL_ATTACK_UP_2:
    case EFFECT_SPECIAL_ATTACK_UP_3:
	case EFFECT_SPECIAL_DEFENSE_UP:
	case EFFECT_SPECIAL_DEFENSE_UP_2:
    case EFFECT_ACCURACY_UP:
    case EFFECT_ACCURACY_UP_2:
    case EFFECT_EVASION_UP:
    case EFFECT_EVASION_UP_2:
    case EFFECT_MINIMIZE:
    case EFFECT_DEFENSE_CURL:
    #if B_CHARGE_SPDEF_RAISE >= GEN_5
    case EFFECT_CHARGE:
    #endif
	case EFFECT_CALM_MIND:
    case EFFECT_COSMIC_POWER:
	case EFFECT_DRAGON_DANCE:
	case EFFECT_ACUPRESSURE:
	case EFFECT_SHELL_SMASH:
	case EFFECT_SHIFT_GEAR:
	case EFFECT_ATTACK_ACCURACY_UP:
	case EFFECT_ATTACK_SPATK_UP:
	case EFFECT_GROWTH:
	case EFFECT_COIL:
	case EFFECT_QUIVER_DANCE:
    case EFFECT_BULK_UP:
    case EFFECT_GEOMANCY:
    case EFFECT_STOCKPILE:
    case EFFECT_VICTORY_DANCE:
        return TRUE;
    default:
        return FALSE;
    }
}

bool32 IsStatLoweringEffect(u16 effect)
{
    // ignore other potentially-beneficial effects like defog, gravity
    switch (effect)
    {
    case EFFECT_ATTACK_DOWN:
    case EFFECT_DEFENSE_DOWN:
    case EFFECT_SPEED_DOWN:
    case EFFECT_SPECIAL_ATTACK_DOWN:
    case EFFECT_SPECIAL_DEFENSE_DOWN:
    case EFFECT_ACCURACY_DOWN:
    case EFFECT_EVASION_DOWN:
    case EFFECT_ATTACK_DOWN_2:
    case EFFECT_DEFENSE_DOWN_2:
    case EFFECT_SPEED_DOWN_2:
    case EFFECT_SPECIAL_ATTACK_DOWN_2:
    case EFFECT_SPECIAL_DEFENSE_DOWN_2:
    case EFFECT_ACCURACY_DOWN_2:
    case EFFECT_EVASION_DOWN_2:
    case EFFECT_TICKLE:
    case EFFECT_CAPTIVATE:
    case EFFECT_NOBLE_ROAR:
    case EFFECT_MEMENTO:
        return TRUE;
    default:
        return FALSE;
    }
}

bool32 HasDamagingMove(u8 battlerId)
{
    u32 i;
    u16 *moves = GetMovesArray(battlerId);

    for (i = 0; i < MAX_MON_MOVES; i++)
    {
        if (moves[i] != MOVE_NONE && moves[i] != MOVE_UNAVAILABLE && gBattleMoves[moves[i]].power != 0)
            return TRUE;
    }

    return FALSE;
}

bool32 HasDamagingMoveOfType(u8 battlerId, u8 type)
{
    s32 i;
    u16 *moves = GetMovesArray(battlerId);

    for (i = 0; i < MAX_MON_MOVES; i++)
    {
        if (moves[i] != MOVE_NONE && moves[i] != MOVE_UNAVAILABLE
          && gBattleMoves[moves[i]].type == type && gBattleMoves[moves[i]].power != 0)
            return TRUE;
    }

    return FALSE;
}

bool32 HasSoundMove(u8 battler)
{
    CHECK_MOVE_FLAG(soundMove);
}

bool32 HasHighCritRatioMove(u8 battler)
{
    CHECK_MOVE_FLAG(highCritRatio);
}

bool32 HasMagicCoatAffectedMove(u8 battler)
{
    CHECK_MOVE_FLAG(magicCoatAffected);
}

bool32 HasSnatchAffectedMove(u8 battler)
{
    CHECK_MOVE_FLAG(snatchAffected);
}

bool32 IsEncoreEncouragedEffect(u16 move)
{
    if (GetBattleMoveSplit(move) == SPLIT_STATUS)
        return TRUE;
    else if (gBattleMoves[move].effect == EFFECT_FAKE_OUT)
        return TRUE;
    return FALSE;
}

bool32 MoveRequiresRecharging(u16 move)
{
    u32 i;
    for (i = 0; i < ARRAY_COUNT(sRechargeMoves); i++)
    {
        if (move == sRechargeMoves[i])
            return TRUE;
    }
    return FALSE;
}

bool32 MoveCallsOtherMove(u16 move)
{
    u32 i;
    for (i = 0; i < ARRAY_COUNT(sOtherMoveCallingMoves); i++)
    {
        if (move == sOtherMoveCallingMoves[i])
            return TRUE;
    }
    return FALSE;
}

static u32 GetLeechSeedDamage(u8 battlerId)
{
    u32 damage = 0;
    if ((gStatuses3[battlerId] & STATUS3_LEECHSEED)
     && gBattleMons[gStatuses3[battlerId] & STATUS3_LEECHSEED_BATTLER].hp != 0)
     {
        damage = gBattleMons[battlerId].maxHP / 8;
        if (damage == 0)
            damage = 1;
     }
     return damage;
}

static u32 GetNightmareDamage(u8 battlerId)
{
    u32 damage = 0;
    if ((gBattleMons[battlerId].status2 & STATUS2_NIGHTMARE) && gBattleMons[battlerId].status1 & STATUS1_SLEEP)
    {
        damage = gBattleMons[battlerId].maxHP / 4;
        if (damage == 0)
            damage = 1;
    }
    return damage;
}

static u32 GetCurseDamage(u8 battlerId)
{
    u32 damage = 0;
    if (gBattleMons[battlerId].status2 & STATUS2_CURSED)
    {
        damage = gBattleMons[battlerId].maxHP / 4;
        if (damage == 0)
            damage = 1;
    }
    return damage;
}

static u32 GetTrapDamage(u8 battlerId)
{
    // ai has no knowledge about turns remaining
    u32 damage = 0;
    u32 holdEffect = AI_DATA->holdEffects[gBattleStruct->wrappedBy[battlerId]];
    if (gBattleMons[battlerId].status2 & STATUS2_WRAPPED)
    {
        if (holdEffect == HOLD_EFFECT_BINDING_BAND)
    #if B_BINDING_DAMAGE >= GEN_6
            damage = gBattleMons[battlerId].maxHP / 6;
        else
            damage = gBattleMons[battlerId].maxHP / 8;
    #else
            damage = gBattleMons[battlerId].maxHP / 8;
        else
            damage = gBattleMons[battlerId].maxHP / 16;
    #endif
        if (damage == 0)
            damage = 1;
    }
    return damage;
}

static u32 GetPoisonDamage(u8 battlerId)
{
    u32 damage = 0;

    if (AI_DATA->abilities[battlerId] == ABILITY_POISON_HEAL)
        return damage;

    if (gBattleMons[battlerId].status1 & STATUS1_POISON)
    {
        damage = gBattleMons[battlerId].maxHP / 8;
        if (damage == 0)
            damage = 1;
    }
    else if (gBattleMons[battlerId].status1 & STATUS1_TOXIC_POISON)
    {
        u32 status1Temp = gBattleMons[battlerId].status1;
        damage = gBattleMons[battlerId].maxHP / 16;
        if (damage == 0)
            damage = 1;
        if ((status1Temp & STATUS1_TOXIC_COUNTER) != STATUS1_TOXIC_TURN(15)) // not 16 turns
            status1Temp += STATUS1_TOXIC_TURN(1);
        damage *= (status1Temp & STATUS1_TOXIC_COUNTER) >> 8;
    }
    return damage;
}

static bool32 BattlerAffectedBySandstorm(u8 battlerId, u16 ability)
{
    if (!IS_BATTLER_OF_TYPE(battlerId, TYPE_ROCK)
      && !IS_BATTLER_OF_TYPE(battlerId, TYPE_GROUND)
      && !IS_BATTLER_OF_TYPE(battlerId, TYPE_STEEL)
      && ability != ABILITY_SAND_VEIL
      && ability != ABILITY_SAND_FORCE
      && ability != ABILITY_SAND_RUSH
      && ability != ABILITY_OVERCOAT)
        return TRUE;
    return FALSE;
}

static bool32 BattlerAffectedByHail(u8 battlerId, u16 ability)
{
    if (!IS_BATTLER_OF_TYPE(battlerId, TYPE_ICE)
      && ability != ABILITY_SNOW_CLOAK
      && ability != ABILITY_OVERCOAT
      && ability != ABILITY_ICE_BODY)
        return TRUE;
    return FALSE;
}

static u32 GetWeatherDamage(u8 battlerId)
{
    u32 ability = AI_DATA->abilities[battlerId];
    u32 holdEffect = AI_DATA->holdEffects[battlerId];
    u32 damage = 0;
    if (!AI_WeatherHasEffect())
        return 0;

    if (gBattleWeather & B_WEATHER_SANDSTORM)
    {
        if (BattlerAffectedBySandstorm(battlerId, ability)
          && !(gStatuses3[battlerId] & (STATUS3_UNDERGROUND | STATUS3_UNDERWATER))
          && holdEffect != HOLD_EFFECT_SAFETY_GOGGLES)
        {
            damage = gBattleMons[battlerId].maxHP / 16;
            if (damage == 0)
                damage = 1;
        }
    }
    if ((gBattleWeather & B_WEATHER_HAIL) && ability != ABILITY_ICE_BODY)
    {
        if (BattlerAffectedByHail(battlerId, ability)
          && !(gStatuses3[battlerId] & (STATUS3_UNDERGROUND | STATUS3_UNDERWATER))
          && holdEffect != HOLD_EFFECT_SAFETY_GOGGLES)
        {
            damage = gBattleMons[battlerId].maxHP / 16;
            if (damage == 0)
                damage = 1;
        }
    }
    return damage;
}

u32 GetBattlerSecondaryDamage(u8 battlerId)
{
    u32 secondaryDamage;

    if (AI_DATA->abilities[battlerId] == ABILITY_MAGIC_GUARD)
        return FALSE;

    secondaryDamage = GetLeechSeedDamage(battlerId)
     + GetNightmareDamage(battlerId)
     + GetCurseDamage(battlerId)
     + GetTrapDamage(battlerId)
     + GetPoisonDamage(battlerId)
     + GetWeatherDamage(battlerId);

    return secondaryDamage;
}

bool32 BattlerWillFaintFromWeather(u8 battler, u16 ability)
{
    if ((BattlerAffectedBySandstorm(battler, ability) && (gBattleWeather & B_WEATHER_SANDSTORM))
        || (BattlerAffectedByHail(battler, ability) && (gBattleWeather & B_WEATHER_HAIL)))
        if (gBattleMons[battler].hp <= gBattleMons[battler].maxHP / 16)
            return TRUE;

    return FALSE;
}

bool32 BattlerWillFaintFromSecondaryDamage(u8 battler, u16 ability)
{
    if (GetBattlerSecondaryDamage(battler) != 0
      && gBattleMons[battler].hp <= gBattleMons[battler].maxHP / 16)
        return TRUE;
    return FALSE;
}

static bool32 AnyUsefulStatIsRaised(u8 battler)
{
    u8 statId;

    for (statId = STAT_ATK; statId < NUM_BATTLE_STATS; statId++)
    {
        if (gBattleMons[battler].statStages[statId] > DEFAULT_STAT_STAGE)
        {
            switch (statId)
            {
            case STAT_ATK:
                if (HasMoveWithSplit(battler, SPLIT_PHYSICAL))
                    return TRUE;
                break;
            case STAT_SPATK:
                if (HasMoveWithSplit(battler, SPLIT_SPECIAL))
                    return TRUE;
                break;
            case STAT_SPEED:
                return TRUE;
            }
        }
    }

    return FALSE;
}

struct Pokemon *GetPartyBattlerPartyData(u8 battlerId, u8 switchBattler)
{
    struct Pokemon *mon;
    if (GetBattlerSide(battlerId) == B_SIDE_PLAYER)
        mon = &gPlayerParty[switchBattler];
    else
        mon = &gEnemyParty[switchBattler];
    return mon;
}

static bool32 PartyBattlerShouldAvoidHazards(u8 currBattler, u8 switchBattler)
{
    struct Pokemon *mon = GetPartyBattlerPartyData(currBattler, switchBattler);
    u16 ability = GetMonAbility(mon);   // we know our own party data
    u16 holdEffect;
    u16 species = GetMonData(mon, MON_DATA_SPECIES);
    u32 flags = gSideStatuses[GetBattlerSide(currBattler)] & (SIDE_STATUS_SPIKES | SIDE_STATUS_STEALTH_ROCK | SIDE_STATUS_STICKY_WEB | SIDE_STATUS_TOXIC_SPIKES);
    s32 hazardDamage = 0;
    u8 type1 = gSpeciesInfo[species].types[0];
    u8 type2 = gSpeciesInfo[species].types[1];
    u32 maxHp = GetMonData(mon, MON_DATA_MAX_HP);

    if (flags == 0)
        return FALSE;

    if (ability == ABILITY_MAGIC_GUARD)
        return FALSE;
    if (gFieldStatuses & STATUS_FIELD_MAGIC_ROOM || ability == ABILITY_KLUTZ)
        holdEffect = HOLD_EFFECT_NONE;
    else
        holdEffect = gItems[GetMonData(mon, MON_DATA_HELD_ITEM)].holdEffect;
    if (holdEffect == HOLD_EFFECT_HEAVY_DUTY_BOOTS)
        return FALSE;

    if (flags & SIDE_STATUS_STEALTH_ROCK)
        hazardDamage += GetStealthHazardDamageByTypesAndHP(gBattleMoves[MOVE_STEALTH_ROCK].type, type1, type2, maxHp);

    if (flags & SIDE_STATUS_SPIKES && ((type1 != TYPE_FLYING && type2 != TYPE_FLYING
        && ability != ABILITY_LEVITATE && holdEffect != HOLD_EFFECT_AIR_BALLOON)
        || holdEffect == HOLD_EFFECT_IRON_BALL || gFieldStatuses & STATUS_FIELD_GRAVITY))
    {
        u8 spikesDmg = maxHp / ((5 - gSideTimers[GetBattlerSide(currBattler)].spikesAmount) * 2);
        if (spikesDmg == 0)
            spikesDmg = 1;
        hazardDamage += spikesDmg;
    }

    if (hazardDamage >= GetMonData(mon, MON_DATA_HP))
        return TRUE;
    return FALSE;
}

enum {
    DONT_PIVOT,
    CAN_TRY_PIVOT,
    PIVOT,
};
bool32 ShouldPivot(u8 battlerAtk, u8 battlerDef, u16 defAbility, u16 move, u8 moveIndex)
{
    bool8 hasStatBoost = AnyUsefulStatIsRaised(battlerAtk) || gBattleMons[battlerDef].statStages[STAT_EVASION] >= 9; //Significant boost in evasion for any class
    u8 backupBattler = gActiveBattler;
    bool32 shouldSwitch;
    u8 battlerToSwitch;

    gActiveBattler = battlerAtk;
    shouldSwitch = ShouldSwitch();
    battlerToSwitch = *(gBattleStruct->AI_monToSwitchIntoId + gActiveBattler);
    gActiveBattler = backupBattler;

    if (PartyBattlerShouldAvoidHazards(battlerAtk, battlerToSwitch))
        return DONT_PIVOT;

    if (!IsDoubleBattle())
    {
        if (CountUsablePartyMons(battlerAtk) == 0)
            return CAN_TRY_PIVOT; // can't switch, but attack might still be useful

        //TODO - predict opponent switching
        /*if (IsPredictedToSwitch(battlerDef, battlerAtk) && !hasStatBoost)
            return PIVOT; // Try pivoting so you can switch to a better matchup to counter your new opponent*/

        if (AI_WhoStrikesFirst(battlerAtk, battlerDef, move) == AI_IS_FASTER) // Attacker goes first
        {
            if (!CanAIFaintTarget(battlerAtk, battlerDef, 0)) // Can't KO foe otherwise
            {
                if (CanAIFaintTarget(battlerAtk, battlerDef, 2))
                {
                    // attacker can kill target in two hits (theoretically)
                    if (CanTargetFaintAi(battlerDef, battlerAtk))
                        return PIVOT;   // Won't get the two turns, pivot

                    if (!IS_MOVE_STATUS(move) && (shouldSwitch
                        || (AtMaxHp(battlerDef) && (AI_DATA->holdEffects[battlerDef] == HOLD_EFFECT_FOCUS_SASH
                    #if B_STURDY >= GEN_5
                        || defAbility == ABILITY_STURDY
                    #endif
                        || defAbility == ABILITY_MULTISCALE
                        || defAbility == ABILITY_SHADOW_SHIELD))))
                        return PIVOT;   // pivot to break sash/sturdy/multiscale
                }
                else if (!hasStatBoost)
                {
                    if (!IS_MOVE_STATUS(move) && (AtMaxHp(battlerDef) && (AI_DATA->holdEffects[battlerDef] == HOLD_EFFECT_FOCUS_SASH
                    #if B_STURDY >= GEN_5
                        || (defAbility == ABILITY_STURDY)
                    #endif
                        || defAbility == ABILITY_MULTISCALE
                        || defAbility == ABILITY_SHADOW_SHIELD)))
                        return PIVOT;   // pivot to break sash/sturdy/multiscale

                    if (shouldSwitch)
                        return PIVOT;

                    /* TODO - check if switchable mon unafffected by/will remove hazards
                    if (gSideStatuses[battlerAtk] & SIDE_STATUS_SPIKES && switchScore >= SWITCHING_INCREASE_CAN_REMOVE_HAZARDS)
                        return PIVOT;*/

                    /*if (BattlerWillFaintFromSecondaryDamage(battlerAtk, AI_DATA->abilities[battlerAtk]) && switchScore >= SWITCHING_INCREASE_WALLS_FOE)
                        return PIVOT;*/

                    /*if (IsClassDamager(class) && switchScore >= SWITCHING_INCREASE_HAS_SUPER_EFFECTIVE_MOVE)
                    {
                        bool8 physMoveInMoveset = PhysicalMoveInMoveset(battlerAtk);
                        bool8 specMoveInMoveset = SpecialMoveInMoveset(battlerAtk);

                        //Pivot if attacking stats are bad
                        if (physMoveInMoveset && !specMoveInMoveset)
                        {
                            if (STAT_STAGE_ATK < 6)
                                return PIVOT;
                        }
                        else if (!physMoveInMoveset && specMoveInMoveset)
                        {
                            if (STAT_STAGE_SPATK < 6)
                                return PIVOT;
                        }
                        else if (physMoveInMoveset && specMoveInMoveset)
                        {
                            if (STAT_STAGE_ATK < 6 && STAT_STAGE_SPATK < 6)
                                return PIVOT;
                        }

                        return CAN_TRY_PIVOT;
                    }*/
                }
            }
        }
        else // Opponent Goes First
        {
            if (CanTargetFaintAi(battlerDef, battlerAtk))
            {
                if (gBattleMoves[move].effect == EFFECT_TELEPORT)
                    return DONT_PIVOT; // If you're going to faint because you'll go second, use a different move
                else
                    return CAN_TRY_PIVOT; // You're probably going to faint anyways so if for some reason you don't, better switch
            }
            else if (CanTargetFaintAiWithMod(battlerDef, battlerAtk, 0, 2)) // Foe can 2HKO AI
            {
                if (CanAIFaintTarget(battlerAtk, battlerDef, 0))
                {
                    if (!BattlerWillFaintFromSecondaryDamage(battlerAtk, AI_DATA->abilities[battlerAtk]))
                        return CAN_TRY_PIVOT; // Use this move to KO if you must
                }
                else // Can't KO the foe
                {
                    return PIVOT;
                }
            }
            else // Foe can 3HKO+ AI
            {
                if (CanAIFaintTarget(battlerAtk, battlerDef, 0))
                {
                    if (!BattlerWillFaintFromSecondaryDamage(battlerAtk, AI_DATA->abilities[battlerAtk]) // This is the only move that can KO
                      && !hasStatBoost) //You're not wasting a valuable stat boost
                    {
                        return CAN_TRY_PIVOT;
                    }
                }
                else if (CanAIFaintTarget(battlerAtk, battlerDef, 2))
                {
                    // can knock out foe in 2 hits
                    if (IS_MOVE_STATUS(move) && (shouldSwitch //Damaging move
                      //&& (switchScore >= SWITCHING_INCREASE_RESIST_ALL_MOVES + SWITCHING_INCREASE_KO_FOE //remove hazards
                     || (AI_DATA->holdEffects[battlerDef] == HOLD_EFFECT_FOCUS_SASH && AtMaxHp(battlerDef))))
                        return DONT_PIVOT; // Pivot to break the sash
                    else
                        return CAN_TRY_PIVOT;
                }
                else
                {
                    //if (IsClassDamager(class) && switchScore >= SWITCHING_INCREASE_KO_FOE)
                        //return PIVOT; //Only switch if way better matchup

                    if (!hasStatBoost)
                    {
                        // TODO - check if switching prevents/removes hazards
                        //if (gSideStatuses[battlerAtk] & SIDE_STATUS_SPIKES && switchScore >= SWITCHING_INCREASE_CAN_REMOVE_HAZARDS)
                            //return PIVOT;

                        // TODO - not always a good idea
                        //if (BattlerWillFaintFromSecondaryDamage(battlerAtk) && switchScore >= SWITCHING_INCREASE_HAS_SUPER_EFFECTIVE_MOVE)
                            //return PIVOT;

                        /*if (IsClassDamager(class) && switchScore >= SWITCHING_INCREASE_HAS_SUPER_EFFECTIVE_MOVE)
                        {
                            bool8 physMoveInMoveset = PhysicalMoveInMoveset(battlerAtk);
                            bool8 specMoveInMoveset = SpecialMoveInMoveset(battlerAtk);

                            //Pivot if attacking stats are bad
                            if (physMoveInMoveset && !specMoveInMoveset)
                            {
                                if (STAT_STAGE_ATK < 6)
                                    return PIVOT;
                            }
                            else if (!physMoveInMoveset && specMoveInMoveset)
                            {
                                if (STAT_STAGE_SPATK < 6)
                                    return PIVOT;
                            }
                            else if (physMoveInMoveset && specMoveInMoveset)
                            {
                                if (STAT_STAGE_ATK < 6 && STAT_STAGE_SPATK < 6)
                                    return PIVOT;
                            }
                        }*/

                        return CAN_TRY_PIVOT;
                    }
                }
            }
        }
    }

    return DONT_PIVOT;
}

bool32 CanKnockOffItem(u8 battler, u16 item)
{
    if (item == ITEM_NONE)
        return FALSE;

    if (!(gBattleTypeFlags & (BATTLE_TYPE_EREADER_TRAINER
      | BATTLE_TYPE_FRONTIER
      | BATTLE_TYPE_LINK
      | BATTLE_TYPE_RECORDED_LINK
      | BATTLE_TYPE_SECRET_BASE
      #if B_TRAINERS_KNOCK_OFF_ITEMS == TRUE
      | BATTLE_TYPE_TRAINER
      #endif
      )) && GetBattlerSide(battler) == B_SIDE_PLAYER)
        return FALSE;

    if (AI_DATA->abilities[battler] == ABILITY_STICKY_HOLD)
        return FALSE;

    if (!CanBattlerGetOrLoseItem(battler, item))
        return FALSE;

    return TRUE;
}

// status checks
bool32 IsBattlerIncapacitated(u8 battler, u16 ability)
{
    if ((gBattleMons[battler].status1 & STATUS1_FREEZE) && !HasThawingMove(battler))
        return TRUE;    // if battler has thawing move we assume they will definitely use it, and thus being frozen should be neglected

    if (gBattleMons[battler].status1 & STATUS1_SLEEP)
        return TRUE;

    if (gBattleMons[battler].status2 & STATUS2_RECHARGE || (ability == ABILITY_TRUANT && gDisableStructs[battler].truantCounter != 0))
        return TRUE;

    return FALSE;
}

//For Tri Attack effect
bool32 AI_CanStatus(u8 battler, u16 ability)
{
    if (ability == ABILITY_COMATOSE
      || ability == ABILITY_PURIFYING_SALT
      || gBattleMons[battler].status1 & STATUS1_ANY
      || gSideStatuses[GetBattlerSide(battler)] & SIDE_STATUS_SAFEGUARD
      || IsAbilityStatusProtected(battler))
        return FALSE;
    return TRUE;
}

bool32 AI_CanSleep(u8 battler, u16 ability)
{
    if (ability == ABILITY_INSOMNIA
      || ability == ABILITY_VITAL_SPIRIT
      || ability == ABILITY_COMATOSE
      || ability == ABILITY_PURIFYING_SALT
      || gBattleMons[battler].status1 & STATUS1_ANY
      || gSideStatuses[GetBattlerSide(battler)] & SIDE_STATUS_SAFEGUARD
      || (gFieldStatuses & (STATUS_FIELD_MISTY_TERRAIN | STATUS_FIELD_ELECTRIC_TERRAIN))
      || IsAbilityStatusProtected(battler))
        return FALSE;
    return TRUE;
}

bool32 AI_CanPutToSleep(u8 battlerAtk, u8 battlerDef, u16 defAbility, u16 move, u16 partnerMove)
{
    if (!AI_CanSleep(battlerDef, defAbility)
      || DoesSubstituteBlockMove(battlerAtk, battlerDef, move)
      || PartnerMoveEffectIsStatusSameTarget(BATTLE_PARTNER(battlerAtk), battlerDef, partnerMove))   // shouldn't try to sleep mon that partner is trying to make sleep
        return FALSE;
    return TRUE;
}

static bool32 AI_CanBePoisoned(u8 battlerAtk, u8 battlerDef)
{
    u16 ability = AI_DATA->abilities[battlerDef];

    if (!(CanPoisonType(battlerAtk, battlerDef))
     || gSideStatuses[GetBattlerSide(battlerDef)] & SIDE_STATUS_SAFEGUARD
     || gBattleMons[battlerDef].status1 & STATUS1_ANY
     || ability == ABILITY_IMMUNITY
     || ability == ABILITY_COMATOSE
     || ability == ABILITY_POISON_HEAL
     || ability == ABILITY_PURIFYING_SALT
     || AI_IsAbilityOnSide(battlerDef, ABILITY_PASTEL_VEIL)
     || gBattleMons[battlerDef].status1 & STATUS1_ANY
     || IsAbilityStatusProtected(battlerDef)
     || AI_IsTerrainAffected(battlerDef, STATUS_FIELD_MISTY_TERRAIN))
        return FALSE;
    return TRUE;
}

bool32 ShouldPoisonSelf(u8 battler, u16 ability)
{
    if (AI_CanBePoisoned(battler, battler) && (
     ability == ABILITY_MARVEL_SCALE
      || ability == ABILITY_POISON_HEAL
      || ability == ABILITY_QUICK_FEET
      || ability == ABILITY_MAGIC_GUARD
      || (ability == ABILITY_TOXIC_BOOST && HasMoveWithSplit(battler, SPLIT_PHYSICAL))
      || (ability == ABILITY_GUTS && HasMoveWithSplit(battler, SPLIT_PHYSICAL))
      || HasMoveEffect(battler, EFFECT_FACADE)
      || HasMoveEffect(battler, EFFECT_PSYCHO_SHIFT)))
        return TRUE;    // battler can be poisoned and has move/ability that synergizes with being poisoned
    return FALSE;
}

bool32 AI_CanPoison(u8 battlerAtk, u8 battlerDef, u16 defAbility, u16 move, u16 partnerMove)
{
    if (!AI_CanBePoisoned(battlerAtk, battlerDef)
      || AI_GetMoveEffectiveness(move, battlerAtk, battlerDef) == AI_EFFECTIVENESS_x0
      || DoesSubstituteBlockMove(battlerAtk, battlerDef, move)
      || PartnerMoveEffectIsStatusSameTarget(BATTLE_PARTNER(battlerAtk), battlerDef, partnerMove))
        return FALSE;
    else if (defAbility != ABILITY_CORROSION && (IS_BATTLER_OF_TYPE(battlerDef, TYPE_POISON) || IS_BATTLER_OF_TYPE(battlerDef, TYPE_STEEL)))
        return FALSE;
    else if (IsValidDoubleBattle(battlerAtk) && AI_DATA->abilities[BATTLE_PARTNER(battlerDef)] == ABILITY_PASTEL_VEIL)
        return FALSE;

    return TRUE;
}

static bool32 AI_CanBeParalyzed(u8 battler, u16 ability)
{
    if (ability == ABILITY_LIMBER
      || ability == ABILITY_PURIFYING_SALT
      || ability == ABILITY_COMATOSE
      || IS_BATTLER_OF_TYPE(battler, TYPE_ELECTRIC)
      || gBattleMons[battler].status1 & STATUS1_ANY
      || IsAbilityStatusProtected(battler))
        return FALSE;
    return TRUE;
}

bool32 AI_CanParalyze(u8 battlerAtk, u8 battlerDef, u16 defAbility, u16 move, u16 partnerMove)
{
    if (!AI_CanBeParalyzed(battlerDef, defAbility)
      || AI_GetMoveEffectiveness(move, battlerAtk, battlerDef) == AI_EFFECTIVENESS_x0
      || gSideStatuses[GetBattlerSide(battlerDef)] & SIDE_STATUS_SAFEGUARD
      || DoesSubstituteBlockMove(battlerAtk, battlerDef, move)
      || PartnerMoveEffectIsStatusSameTarget(BATTLE_PARTNER(battlerAtk), battlerDef, partnerMove))
        return FALSE;
    return TRUE;
}

bool32 AI_CanBeConfused(u32 battlerAtk, u32 battlerDef, u32 move, u32 ability)
{
    if ((gBattleMons[battlerDef].status2 & STATUS2_CONFUSION)
     || (ability == ABILITY_OWN_TEMPO && !DoesBattlerIgnoreAbilityChecks(AI_DATA->abilities[battlerAtk], move))
     || (IsBattlerGrounded(battlerDef) && (gFieldStatuses & STATUS_FIELD_MISTY_TERRAIN))
     || gSideStatuses[GetBattlerSide(battlerDef)] & SIDE_STATUS_SAFEGUARD
     || DoesSubstituteBlockMove(battlerAtk, battlerDef, move))
        return FALSE;
    return TRUE;
}

bool32 AI_CanConfuse(u8 battlerAtk, u8 battlerDef, u16 defAbility, u8 battlerAtkPartner, u16 move, u16 partnerMove)
{
    if (gBattleMoves[move].target == MOVE_TARGET_FOES_AND_ALLY
        && AI_CanBeConfused(battlerAtk, battlerDef, move, defAbility)
        && !AI_CanBeConfused(battlerAtk, BATTLE_PARTNER(battlerDef), move, AI_DATA->abilities[BATTLE_PARTNER(battlerDef)]))
           return FALSE;
   
       if (!AI_CanBeConfused(battlerAtk, battlerDef, move, defAbility)
        || DoesPartnerHaveSameMoveEffect(battlerAtkPartner, battlerDef, move, partnerMove))
    {
        return FALSE;
    }

    return TRUE;
}

bool32 AI_CanBeBurned(u8 battler, u16 ability)
{
    if (ability == ABILITY_WATER_VEIL
      || ability == ABILITY_WATER_BUBBLE
      || ability == ABILITY_COMATOSE
      || ability == ABILITY_PURIFYING_SALT
      || IS_BATTLER_OF_TYPE(battler, TYPE_FIRE)
      || gBattleMons[battler].status1 & STATUS1_ANY
      || IsAbilityStatusProtected(battler)
      || gSideStatuses[GetBattlerSide(battler)] & SIDE_STATUS_SAFEGUARD)
        return FALSE;
    return TRUE;
}

bool32 AI_CanGetFrostbite(u8 battler, u16 ability)
{
    if (ability == ABILITY_MAGMA_ARMOR
      || ability == ABILITY_COMATOSE
      || IS_BATTLER_OF_TYPE(battler, TYPE_ICE)
      || gBattleMons[battler].status1 & STATUS1_ANY
      || IsAbilityStatusProtected(battler)
      || gSideStatuses[GetBattlerSide(battler)] & SIDE_STATUS_SAFEGUARD)
        return FALSE;
    return TRUE;
}

bool32 ShouldBurnSelf(u8 battler, u16 ability)
{
    if (AI_CanBeBurned(battler, ability) && (
     ability == ABILITY_QUICK_FEET
      || ability == ABILITY_HEATPROOF
      || ability == ABILITY_MAGIC_GUARD
      || (ability == ABILITY_FLARE_BOOST && HasMoveWithSplit(battler, SPLIT_SPECIAL))
      || (ability == ABILITY_GUTS && HasMoveWithSplit(battler, SPLIT_PHYSICAL))
      || HasMoveEffect(battler, EFFECT_FACADE)
      || HasMoveEffect(battler, EFFECT_PSYCHO_SHIFT)))
        return TRUE;
    return FALSE;
}

bool32 AI_CanBurn(u8 battlerAtk, u8 battlerDef, u16 defAbility, u8 battlerAtkPartner, u16 move, u16 partnerMove)
{
    if (!AI_CanBeBurned(battlerDef, defAbility)
      || AI_GetMoveEffectiveness(move, battlerAtk, battlerDef) == AI_EFFECTIVENESS_x0
      || DoesSubstituteBlockMove(battlerAtk, battlerDef, move)
      || PartnerMoveEffectIsStatusSameTarget(battlerAtkPartner, battlerDef, partnerMove))
    {
        return FALSE;
    }
    return TRUE;
}

bool32 AI_CanGiveFrostbite(u8 battlerAtk, u8 battlerDef, u16 defAbility, u8 battlerAtkPartner, u16 move, u16 partnerMove)
{
    if (!AI_CanGetFrostbite(battlerDef, defAbility)
      || AI_GetMoveEffectiveness(move, battlerAtk, battlerDef) == AI_EFFECTIVENESS_x0
      || DoesSubstituteBlockMove(battlerAtk, battlerDef, move)
      || PartnerMoveEffectIsStatusSameTarget(battlerAtkPartner, battlerDef, partnerMove))
    {
        return FALSE;
    }
    return TRUE;
}

bool32 AI_CanBeInfatuated(u8 battlerAtk, u8 battlerDef, u16 defAbility)
{
    if ((gBattleMons[battlerDef].status2 & STATUS2_INFATUATION)
      || AI_GetMoveEffectiveness(AI_THINKING_STRUCT->moveConsidered, battlerAtk, battlerDef) == AI_EFFECTIVENESS_x0
      || defAbility == ABILITY_OBLIVIOUS
      || !AreBattlersOfOppositeGender(battlerAtk, battlerDef)
      || AI_IsAbilityOnSide(battlerDef, ABILITY_AROMA_VEIL))
        return FALSE;
    return TRUE;
}

u32 ShouldTryToFlinch(u8 battlerAtk, u8 battlerDef, u16 atkAbility, u16 defAbility, u16 move)
{
    if (defAbility == ABILITY_INNER_FOCUS
      || DoesSubstituteBlockMove(battlerAtk, battlerDef, move)
      || AI_WhoStrikesFirst(battlerAtk, battlerDef, move) == AI_IS_SLOWER) // Opponent goes first
    {
        return 0;   // don't try to flinch
    }
    else if ((gBattleMons[battlerDef].status1 & STATUS1_SLEEP) && !HasMoveEffect(battlerDef, EFFECT_SLEEP_TALK) && !HasMoveEffect(battlerDef, EFFECT_SNORE))
    {
        return 0;   // don't try to flinch sleeping pokemon
    }
    else if (atkAbility == ABILITY_SERENE_GRACE
      || gBattleMons[battlerDef].status1 & STATUS1_PARALYSIS
      || gBattleMons[battlerDef].status2 & STATUS2_INFATUATION
      || gBattleMons[battlerDef].status2 & STATUS2_CONFUSION)
    {
        return 2;   // good idea to flinch
    }
    return 1;   // decent idea to flinch
}

bool32 ShouldTrap(u8 battlerAtk, u8 battlerDef, u16 move)
{
    if (BattlerWillFaintFromSecondaryDamage(battlerDef, AI_DATA->abilities[battlerDef]))
        return TRUE;    // battler is taking secondary damage with low HP

    if (AI_THINKING_STRUCT->aiFlags & AI_FLAG_STALL)
    {
        if (!CanTargetFaintAi(battlerDef, battlerAtk))
            return TRUE;    // attacker goes first and opponent can't kill us
    }

    return FALSE;
}

bool32 ShouldFakeOut(u8 battlerAtk, u8 battlerDef, u16 move)
{
    if (AI_DATA->holdEffects[battlerAtk] == HOLD_EFFECT_CHOICE_BAND && CountUsablePartyMons(battlerAtk) == 0)
        return FALSE;   // don't lock attacker into fake out if can't switch out

    if (gDisableStructs[battlerAtk].isFirstTurn
      && ShouldTryToFlinch(battlerAtk, battlerDef, AI_DATA->abilities[battlerAtk], AI_DATA->abilities[battlerDef], move)
      && !DoesSubstituteBlockMove(battlerAtk, battlerDef, move))
        return TRUE;

    return FALSE;
}

static u32 FindMoveUsedXTurnsAgo(u32 battlerId, u32 x)
{
    s32 i, index = BATTLE_HISTORY->moveHistoryIndex[battlerId];
    for (i = 0; i < x; i++)
    {
        if (--index < 0)
            index = AI_MOVE_HISTORY_COUNT - 1;
    }
    return BATTLE_HISTORY->moveHistory[battlerId][index];
}

bool32 IsWakeupTurn(u8 battler)
{
    // Check if rest was used 2 turns ago
    if ((gBattleMons[battler].status1 & STATUS1_SLEEP) == 1)
        return TRUE;
    else // no way to know
        return FALSE;
}

bool32 AnyPartyMemberStatused(u8 battlerId, bool32 checkSoundproof)
{
    struct Pokemon *party;
    u32 i;

    if (GetBattlerSide(battlerId) == B_SIDE_PLAYER)
        party = gPlayerParty;
    else
        party = gEnemyParty;

    for (i = 0; i < PARTY_SIZE; i++)
    {
        if (checkSoundproof && GetMonAbility(&party[i]) == ABILITY_SOUNDPROOF)
            continue;

        if (GetMonData(&party[i], MON_DATA_STATUS) != STATUS1_NONE)
            return TRUE;
    }

    return FALSE;
}

u16 GetBattlerSideSpeedAverage(u8 battler)
{
    u16 speed1 = 0;
    u16 speed2 = 0;
    u8 numBattlersAlive = 0;

    if (IsBattlerAlive(battler))
    {
        speed1 = GetBattlerTotalSpeedStat(battler);
        numBattlersAlive++;
    }

    if (IsDoubleBattle() && IsBattlerAlive(BATTLE_PARTNER(battler)))
    {
        speed2 = GetBattlerTotalSpeedStat(BATTLE_PARTNER(battler));
        numBattlersAlive++;
    }

    return (speed1 + speed2) / numBattlersAlive;
}

bool32 ShouldUseRecoilMove(u8 battlerAtk, u8 battlerDef, u32 recoilDmg, u8 moveIndex)
{
    if (recoilDmg >= gBattleMons[battlerAtk].hp //Recoil kills attacker
      && CountUsablePartyMons(battlerDef) != 0) //Foe has more than 1 target left
    {
        if (recoilDmg >= gBattleMons[battlerDef].hp && !CanAIFaintTarget(battlerAtk, battlerDef, 0))
            return TRUE; //If it's the only KO move then just use it
        else
            return FALSE; //Not as good to use move if you'll faint and not win
    }

    return TRUE;
}

bool32 ShouldAbsorb(u8 battlerAtk, u8 battlerDef, u16 move, s32 damage)
{
    if (move == 0xFFFF || AI_WhoStrikesFirst(battlerAtk, battlerDef, move) == AI_IS_FASTER)
    {
        // using item or user goes first
        u8 healPercent = (gBattleMoves[move].argument == 0) ? 50 : gBattleMoves[move].argument;
        s32 healDmg = (healPercent * damage) / 100;

        if (gStatuses3[battlerAtk] & STATUS3_HEAL_BLOCK)
            healDmg = 0;

        if (CanTargetFaintAi(battlerDef, battlerAtk)
          && !CanTargetFaintAiWithMod(battlerDef, battlerAtk, healDmg, 0))
            return TRUE;    // target can faint attacker unless they heal
        else if (!CanTargetFaintAi(battlerDef, battlerAtk) && AI_DATA->hpPercents[battlerAtk] < 60 && (Random() % 3))
            return TRUE;    // target can't faint attacker at all, attacker health is about half, 2/3rds rate of encouraging healing
    }
    else
    {
        // opponent goes first
        if (!CanTargetFaintAi(battlerDef, battlerAtk))
            return TRUE;
    }

    return FALSE;
}

//TODO: Keep this? From Run&Bun
bool32 ShouldRecover(u8 battlerAtk, u8 battlerDef, u16 move, u8 healPercent)
{
    s32 highestDamage = 0;
    s32 damage = 0;
    u32 unusable = CheckMoveLimitations(battlerDef, 0, MOVE_LIMITATIONS_ALL);
    u8 i;
    u16 *moves = gBattleMons[battlerDef].moves;
    s32 healAmount = 0;
    u8 effectiveness;

    if ((gBattleMons[battlerAtk].status1 & STATUS1_TOXIC_POISON) && move != MOVE_REST)
        return FALSE;

    for (i = 0; i < MAX_MON_MOVES; i++)
    {
        if (moves[i] != MOVE_NONE && moves[i] != 0xFFFF && !(unusable & gBitTable[i]))
        {   
            damage = AI_DATA->maxrollDmg[battlerDef][battlerAtk][i];
            if (damage > highestDamage)
                highestDamage = damage;               
        }
    }
    
    healAmount = ((healPercent * gBattleMons[battlerAtk].maxHP) / 100) - highestDamage;
    if (gStatuses3[battlerAtk] & STATUS3_HEAL_BLOCK)
        healAmount = 0;
    
    if (healAmount <= 0)
        return FALSE;

    if (AI_WhoStrikesFirst(battlerAtk, battlerDef, MOVE_SCRATCH) == AI_IS_FASTER)
    {
        if (CanTargetFaintAi(battlerDef, battlerAtk)
        && !CanTargetFaintAiWithMod(battlerDef, battlerAtk, healAmount, 0))
        {
            return TRUE;
        }
        else if (!CanTargetFaintAi(battlerDef, battlerAtk))
        {
            if (GetHealthPercentage(battlerAtk) < 66 && AI_RandLessThan(128))
                return TRUE;
            else if (GetHealthPercentage(battlerAtk) < 40)
                return TRUE;
        }
    }

    if (AI_WhoStrikesFirst(battlerAtk, battlerDef, move) == AI_IS_SLOWER)
    {
        if (GetHealthPercentage(battlerAtk) < 70 && AI_RandLessThan(192))
            return TRUE;
        else if (GetHealthPercentage(battlerAtk) < 50)
            return TRUE;
    }
    return FALSE;
}

bool32 ShouldSetScreen(u8 battlerAtk, u8 battlerDef, u16 moveEffect)
{
    u8 atkSide = GetBattlerSide(battlerAtk);
    switch (moveEffect)
    {
    case EFFECT_AURORA_VEIL:
        // Use only in Hail and only if AI doesn't already have Reflect, Light Screen or Aurora Veil itself active.
        if ((gBattleWeather & (B_WEATHER_HAIL | B_WEATHER_SNOW))
            && !(gSideStatuses[atkSide] & (SIDE_STATUS_REFLECT | SIDE_STATUS_LIGHTSCREEN | SIDE_STATUS_AURORA_VEIL)))
            return TRUE;
        break;
    case EFFECT_REFLECT:
        // Use only if the player has a physical move and AI doesn't already have Reflect itself active.
        if (HasMoveWithSplit(battlerDef, SPLIT_PHYSICAL)
            && !(gSideStatuses[atkSide] & SIDE_STATUS_REFLECT))
            return TRUE;
        break;
    case EFFECT_LIGHT_SCREEN:
        // Use only if the player has a special move and AI doesn't already have Light Screen itself active.
        if (HasMoveWithSplit(battlerDef, SPLIT_SPECIAL)
            && !(gSideStatuses[atkSide] & SIDE_STATUS_LIGHTSCREEN))
            return TRUE;
        break;
    }

    return FALSE;
}

// Partner Logic
bool32 IsValidDoubleBattle(u8 battlerAtk)
{
    if (IsDoubleBattle()
      && ((IsBattlerAlive(BATTLE_OPPOSITE(battlerAtk)) && IsBattlerAlive(BATTLE_PARTNER(BATTLE_OPPOSITE(battlerAtk)))) || IsBattlerAlive(BATTLE_PARTNER(battlerAtk))))
        return TRUE;
    return FALSE;
}

u16 GetAllyChosenMove(u8 battlerId)
{
    u8 partnerBattler = BATTLE_PARTNER(battlerId);

    if (!IsBattlerAlive(partnerBattler) || !IsAiBattlerAware(partnerBattler))
        return MOVE_NONE;
    else if (partnerBattler > battlerId) // Battler with the lower id chooses the move first.
        return gLastMoves[partnerBattler];
    else {
        //return gBattleMons[partnerBattler].moves[gBattleStruct->chosenMovePositions[partnerBattler]];
        return GetAIChosenMove(partnerBattler);
    }
}

bool32 IsTargetingPartner(u8 battlerAtk, u8 battlerDef)
{
    if ((battlerAtk & BIT_SIDE) == (battlerDef & BIT_SIDE))
        return TRUE;

    return FALSE;
}

//PARTNER_MOVE_EFFECT_IS_SAME
bool32 DoesPartnerHaveSameMoveEffect(u8 battlerAtkPartner, u8 battlerDef, u16 move, u16 partnerMove)
{
    if (!IsDoubleBattle())
        return FALSE;

    if (gBattleMoves[move].effect == gBattleMoves[partnerMove].effect
      && gChosenMoveByBattler[battlerAtkPartner] != MOVE_NONE
      && gBattleStruct->moveTarget[battlerAtkPartner] == battlerDef)
    {
        return TRUE;
    }
    return FALSE;
}

//PARTNER_MOVE_EFFECT_IS_SAME_NO_TARGET
bool32 PartnerHasSameMoveEffectWithoutTarget(u8 battlerAtkPartner, u16 move, u16 partnerMove)
{
    if (!IsDoubleBattle())
        return FALSE;

    if (gBattleMoves[move].effect == gBattleMoves[partnerMove].effect
      && gChosenMoveByBattler[battlerAtkPartner] != MOVE_NONE)
        return TRUE;
    return FALSE;
}

//PARTNER_MOVE_EFFECT_IS_STATUS_SAME_TARGET
bool32 PartnerMoveEffectIsStatusSameTarget(u8 battlerAtkPartner, u8 battlerDef, u16 partnerMove)
{
    if (!IsDoubleBattle())
        return FALSE;

    if (gChosenMoveByBattler[battlerAtkPartner] != MOVE_NONE
     && gBattleStruct->moveTarget[battlerAtkPartner] == battlerDef
     && (gBattleMoves[partnerMove].effect == EFFECT_SLEEP
       || gBattleMoves[partnerMove].effect == EFFECT_POISON
       || gBattleMoves[partnerMove].effect == EFFECT_TOXIC
       || gBattleMoves[partnerMove].effect == EFFECT_PARALYZE
       || gBattleMoves[partnerMove].effect == EFFECT_WILL_O_WISP
       || gBattleMoves[partnerMove].effect == EFFECT_YAWN))
        return TRUE;
    return FALSE;
}

//PARTNER_MOVE_EFFECT_IS_WEATHER
bool32 PartnerMoveEffectIsWeather(u8 battlerAtkPartner, u16 partnerMove)
{
    if (!IsDoubleBattle())
        return FALSE;

    if (gChosenMoveByBattler[battlerAtkPartner] != MOVE_NONE
     && (gBattleMoves[partnerMove].effect == EFFECT_SUNNY_DAY
      || gBattleMoves[partnerMove].effect == EFFECT_RAIN_DANCE
      || gBattleMoves[partnerMove].effect == EFFECT_SANDSTORM
      || gBattleMoves[partnerMove].effect == EFFECT_HAIL
      || gBattleMoves[partnerMove].effect == EFFECT_SNOWSCAPE))
        return TRUE;

    return FALSE;
}

//PARTNER_MOVE_EFFECT_IS_TERRAIN
bool32 PartnerMoveEffectIsTerrain(u8 battlerAtkPartner, u16 partnerMove)
{
    if (!IsDoubleBattle())
        return FALSE;

    if (gChosenMoveByBattler[battlerAtkPartner] != MOVE_NONE
     && (gBattleMoves[partnerMove].effect == EFFECT_GRASSY_TERRAIN
      || gBattleMoves[partnerMove].effect == EFFECT_MISTY_TERRAIN
      || gBattleMoves[partnerMove].effect == EFFECT_ELECTRIC_TERRAIN
      || gBattleMoves[partnerMove].effect == EFFECT_PSYCHIC_TERRAIN))
        return TRUE;

    return FALSE;
}

//PARTNER_MOVE_IS_TAILWIND_TRICKROOM
bool32 PartnerMoveIs(u8 battlerAtkPartner, u16 partnerMove, u16 moveCheck)
{
    if (!IsDoubleBattle())
        return FALSE;

    if (partnerMove != MOVE_NONE && partnerMove == moveCheck)
        return TRUE;
    return FALSE;
}

//PARTNER_MOVE_IS_SAME
bool32 PartnerMoveIsSameAsAttacker(u8 battlerAtkPartner, u8 battlerDef, u16 move, u16 partnerMove)
{
    if (!IsDoubleBattle())
        return FALSE;

    if (gChosenMoveByBattler[battlerAtkPartner] != MOVE_NONE && move == partnerMove && gBattleStruct->moveTarget[battlerAtkPartner] == battlerDef)
        return TRUE;
    return FALSE;
}

//PARTNER_MOVE_IS_SAME_NO_TARGET
bool32 PartnerMoveIsSameNoTarget(u8 battlerAtkPartner, u16 move, u16 partnerMove)
{
    if (!IsDoubleBattle())
        return FALSE;
    if (gChosenMoveByBattler[battlerAtkPartner] != MOVE_NONE && move == partnerMove)
        return TRUE;
    return FALSE;
}

bool32 ShouldUseWishAromatherapy(u8 battlerAtk, u8 battlerDef, u16 move)
{
    u32 i;
    u32 firstId, lastId;
    struct Pokemon* party;
    bool32 hasStatus = FALSE;
    bool32 needHealing = FALSE;

    GetAIPartyIndexes(battlerAtk, &firstId, &lastId);

    if (GetBattlerSide(gActiveBattler) == B_SIDE_PLAYER)
        party = gPlayerParty;
    else
        party = gEnemyParty;

    if (CountUsablePartyMons(battlerAtk) == 0
      && (CanTargetFaintAi(battlerDef, battlerAtk) || BattlerWillFaintFromSecondaryDamage(battlerAtk, AI_DATA->abilities[battlerAtk])))
        return FALSE; // Don't heal if last mon and will faint

    for (i = 0; i < PARTY_SIZE; i++)
    {
        u16 currHp = GetMonData(&party[i], MON_DATA_HP);
        u16 maxHp = GetMonData(&party[i], MON_DATA_MAX_HP);

        if (!GetMonData(&party[i], MON_DATA_IS_EGG, NULL) && currHp > 0)
        {
            if ((currHp * 100) / maxHp < 65 // Less than 65% health remaining
              && i >= firstId && i < lastId) // Can only switch to mon on your team
            {
                needHealing = TRUE;
            }

            if (GetMonData(&party[i], MON_DATA_STATUS, NULL) != STATUS1_NONE)
            {
                if (move != MOVE_HEAL_BELL || GetMonAbility(&party[i]) != ABILITY_SOUNDPROOF)
                    hasStatus = TRUE;
            }
        }
    }

    if (!IsDoubleBattle())
    {
        switch (gBattleMoves[move].effect)
        {
        case EFFECT_WISH:
            if (needHealing)
                return TRUE;
            break;
        case EFFECT_HEAL_BELL:
            if (hasStatus)
                return TRUE;
        }
    }
    else
    {
        switch (gBattleMoves[move].effect)
        {
        case EFFECT_WISH:
            return ShouldRecover(battlerAtk, battlerDef, move, 50); // Switch recovery isn't good idea in doubles
        case EFFECT_HEAL_BELL:
            if (hasStatus)
                return TRUE;
        }
    }

    return FALSE;
}

#define SIZE_G_BATTLE_MONS (sizeof(struct BattlePokemon) * MAX_BATTLERS_COUNT)

struct BattlePokemon *AllocSaveBattleMons(void)
{
    struct BattlePokemon *savedBattleMons = Alloc(SIZE_G_BATTLE_MONS);
    memcpy(savedBattleMons, gBattleMons, SIZE_G_BATTLE_MONS);
    return savedBattleMons;
}

void FreeRestoreBattleMons(struct BattlePokemon *savedBattleMons)
{
    memcpy(gBattleMons, savedBattleMons, SIZE_G_BATTLE_MONS);
    Free(savedBattleMons);
}

// party logic
s32 AI_CalcPartyMonBestMoveDamage(u32 battlerAtk, u32 battlerDef, struct Pokemon *attackerMon, struct Pokemon *targetMon)
{
    s32 i, move, bestDmg, dmg;
    u8 effectiveness;
    struct BattlePokemon *savedBattleMons = AllocSaveBattleMons();

    if (attackerMon != NULL)
        PokemonToBattleMon(attackerMon, &gBattleMons[battlerAtk]);
    if (targetMon != NULL)
        PokemonToBattleMon(targetMon, &gBattleMons[battlerDef]);

    for (bestDmg = 0, i = 0; i < MAX_MON_MOVES; i++)
    {
        if (BattlerHasAi(battlerAtk))
            move = GetMonData(attackerMon, MON_DATA_MOVE1 + i);
        else
            move = AI_PARTY->mons[GET_BATTLER_SIDE2(battlerAtk)][gBattlerPartyIndexes[battlerAtk]].moves[i];

        if (move != MOVE_NONE && gBattleMoves[move].power != 0)
        {
            dmg = AI_CalcDamage(move, battlerAtk, battlerDef, &effectiveness, FALSE, FALSE);
            if (dmg > bestDmg)
                bestDmg = dmg;
        }
    }



    FreeRestoreBattleMons(savedBattleMons);

    return dmg;
}

s32 CountUsablePartyMons(u8 battlerId)
{
    s32 battlerOnField1, battlerOnField2, i, ret;
    struct Pokemon *party;

    if (GetBattlerSide(battlerId) == B_SIDE_PLAYER)
        party = gPlayerParty;
    else
        party = gEnemyParty;

    if (gBattleTypeFlags & BATTLE_TYPE_DOUBLE)
    {
        battlerOnField1 = gBattlerPartyIndexes[battlerId];
        battlerOnField2 = gBattlerPartyIndexes[GetBattlerAtPosition(BATTLE_PARTNER(GetBattlerPosition(battlerId)))];
    }
    else // In singles there's only one battlerId by side.
    {
        battlerOnField1 = gBattlerPartyIndexes[battlerId];
        battlerOnField2 = gBattlerPartyIndexes[battlerId];
    }

    ret = 0;
    for (i = 0; i < PARTY_SIZE; i++)
    {
        if (i != battlerOnField1 && i != battlerOnField2
         && GetMonData(&party[i], MON_DATA_HP) != 0
         && GetMonData(&party[i], MON_DATA_SPECIES_OR_EGG) != SPECIES_NONE
         && GetMonData(&party[i], MON_DATA_SPECIES_OR_EGG) != SPECIES_EGG)
        {
            ret++;
        }
    }

    return ret;
}

s32 CountAllAlivePartyMons(u8 battlerId)
{
    s32 battlerOnField1, battlerOnField2, i, ret;
    struct Pokemon *party;

    if (GetBattlerSide(battlerId) == B_SIDE_PLAYER)
        party = gPlayerParty;
    else
        party = gEnemyParty;

    ret = 0;
    for (i = 0; i < PARTY_SIZE; i++)
    {
        if (i != battlerOnField1 && i != battlerOnField2
         && GetMonData(&party[i], MON_DATA_HP) != 0
         && GetMonData(&party[i], MON_DATA_SPECIES_OR_EGG) != SPECIES_NONE
         && GetMonData(&party[i], MON_DATA_SPECIES_OR_EGG) != SPECIES_EGG)
        {
            ret++;
        }
    }

    return ret;
}

s32 CountAllPartyMons(u8 battlerId)
{
    s32 battlerOnField1, battlerOnField2, i, ret;
    struct Pokemon *party;

    if (GetBattlerSide(battlerId) == B_SIDE_PLAYER)
        party = gPlayerParty;
    else
        party = gEnemyParty;

    ret = 0;
    for (i = 0; i < PARTY_SIZE; i++)
    {
        if (GetMonData(&party[i], MON_DATA_SPECIES_OR_EGG) != SPECIES_NONE
            && GetMonData(&party[i], MON_DATA_SPECIES_OR_EGG) != SPECIES_EGG)
        {
            ret++;
        }
    }

    return ret;
}

bool32 IsPartyFullyHealedExceptBattler(u8 battlerId)
{
    struct Pokemon *party;
    u32 i;

    if (GetBattlerSide(battlerId) == B_SIDE_PLAYER)
        party = gPlayerParty;
    else
        party = gEnemyParty;

    for (i = 0; i < PARTY_SIZE; i++)
    {
        if (i != gBattlerPartyIndexes[battlerId]
         && GetMonData(&party[i], MON_DATA_HP) != 0
         && GetMonData(&party[i], MON_DATA_SPECIES_OR_EGG) != SPECIES_NONE
         && GetMonData(&party[i], MON_DATA_SPECIES_OR_EGG) != SPECIES_EGG
         && GetMonData(&party[i], MON_DATA_HP) < GetMonData(&party[i], MON_DATA_MAX_HP))
            return FALSE;
    }
    return TRUE;
}

bool32 PartyHasMoveSplit(u8 battlerId, u8 split)
{
    u8 firstId, lastId;
    struct Pokemon *party = GetBattlerParty(battlerId);
    u32 i, j;

    for (i = 0; i < PARTY_SIZE; i++)
    {
        if (GetMonData(&party[i], MON_DATA_HP, NULL) == 0)
            continue;

        for (j = 0; j < MAX_MON_MOVES; j++)
        {
            u16 move = GetMonData(&party[i], MON_DATA_MOVE1 + j, NULL);
            u16 pp = GetMonData(&party[i], MON_DATA_PP1 + j, NULL);

            if (pp > 0 && move != MOVE_NONE)
            {
                //TODO - handle photon geyser, light that burns the sky
                if (gBattleMoves[move].split == split)
                    return TRUE;
            }
        }
    }

    return FALSE;
}

bool32 SideHasMoveSplit(u8 battlerId, u8 split)
{
    if (IsDoubleBattle())
    {
        if (HasMoveWithSplit(battlerId, split) || HasMoveWithSplit(BATTLE_PARTNER(battlerId), split))
            return TRUE;
    }
    else
    {
        if (HasMoveWithSplit(battlerId, split))
            return TRUE;
    }
    return FALSE;
}

bool32 IsAbilityOfRating(u16 ability, s8 rating)
{
    if (sAiAbilityRatings[ability] >= rating)
        return TRUE;
    return FALSE;
}

s8 GetAbilityRating(u16 ability)
{
    return sAiAbilityRatings[ability];
}

static const u16 sRecycleEncouragedItems[] =
{
    ITEM_CHESTO_BERRY,
    ITEM_LUM_BERRY,
    ITEM_STARF_BERRY,
    ITEM_SITRUS_BERRY,
    ITEM_MICLE_BERRY,
    ITEM_CUSTAP_BERRY,
    ITEM_MENTAL_HERB,
    ITEM_FOCUS_SASH,
    // TODO expand this
};

// Its assumed that the berry is strategically given, so no need to check benefits of the berry
bool32 IsStatBoostingBerry(u16 item)
{
    switch (item)
    {
    case ITEM_LIECHI_BERRY:
    case ITEM_GANLON_BERRY:
    case ITEM_SALAC_BERRY:
    case ITEM_PETAYA_BERRY:
    case ITEM_APICOT_BERRY:
    //case ITEM_LANSAT_BERRY:
    case ITEM_STARF_BERRY:
    case ITEM_MICLE_BERRY:
        return TRUE;
    default:
        return FALSE;
    }
}

bool32 ShouldRestoreHpBerry(u8 battlerAtk, u16 item)
{
    switch (item)
    {
    case ITEM_ORAN_BERRY:
        if (gBattleMons[battlerAtk].maxHP <= 50)
            return TRUE;    // Only worth it in the early game
        return FALSE;
    case ITEM_SITRUS_BERRY:
    case ITEM_FIGY_BERRY:
    case ITEM_WIKI_BERRY:
    case ITEM_MAGO_BERRY:
    case ITEM_AGUAV_BERRY:
    case ITEM_IAPAPA_BERRY:
        return TRUE;
    default:
        return FALSE;
    }
}

bool32 IsRecycleEncouragedItem(u16 item)
{
    u32 i;
    for (i = 0; i < ARRAY_COUNT(sRecycleEncouragedItems); i++)
    {
        if (item == sRecycleEncouragedItems[i])
            return TRUE;
    }
    return FALSE;
}

// score increases
#define STAT_UP_2_STAGE     8
#define STAT_UP_STAGE       10
void IncreaseStatUpScore(u8 battlerAtk, u8 battlerDef, u8 statId, s16 *score)
{
    if (AI_DATA->abilities[battlerAtk] == ABILITY_CONTRARY)
        return;

    if (AI_DATA->hpPercents[battlerAtk] < 80 && AI_RandLessThan(128))
        return;

    if ((AI_THINKING_STRUCT->aiFlags & AI_FLAG_TRY_TO_FAINT) && CanAIFaintTarget(battlerAtk, battlerDef, 0))
        return; // Damaging moves would get a score boost from AI_TryToFaint or PreferStrongestMove so we don't consider them here

    switch (statId)
    {
    case STAT_ATK:
        if (HasMoveWithSplit(battlerAtk, SPLIT_PHYSICAL) && AI_DATA->hpPercents[battlerAtk] > 40)
        {
            if (gBattleMons[battlerAtk].statStages[STAT_ATK] < STAT_UP_2_STAGE)
                *score += 2;
            else if (gBattleMons[battlerAtk].statStages[STAT_ATK] < STAT_UP_STAGE)
                *score += 1;
        }
        if (HasMoveEffect(battlerAtk, EFFECT_FOUL_PLAY))
            *score += 1;
        break;
    case STAT_DEF:
        if ((HasMoveWithSplit(battlerDef, SPLIT_PHYSICAL)|| IS_MOVE_PHYSICAL(gLastMoves[battlerDef]))
          && AI_DATA->hpPercents[battlerAtk] > 70)
        {
            if (gBattleMons[battlerAtk].statStages[STAT_DEF] < STAT_UP_2_STAGE)
                *score += 2; // seems better to raise def at higher HP
            else if (gBattleMons[battlerAtk].statStages[STAT_DEF] < STAT_UP_STAGE)
                *score += 1;
        }
        break;
    case STAT_SPEED:
        if (!WillAIStrikeFirst())
        {
            if (gBattleMons[battlerAtk].statStages[STAT_SPEED] < STAT_UP_2_STAGE)
                *score += 2;
            else if (gBattleMons[battlerAtk].statStages[STAT_SPEED] < STAT_UP_STAGE)
                *score += 1;
        }
        break;
    case STAT_SPATK:
        if (HasMoveWithSplit(battlerAtk, SPLIT_SPECIAL) && AI_DATA->hpPercents[battlerAtk] > 40)
        {
            if (gBattleMons[battlerAtk].statStages[STAT_SPATK] < STAT_UP_2_STAGE)
                *score += 2;
            else if (gBattleMons[battlerAtk].statStages[STAT_SPATK] < STAT_UP_STAGE)
                *score += 1;
        }
        break;
    case STAT_SPDEF:
        if ((HasMoveWithSplit(battlerDef, SPLIT_SPECIAL) || IS_MOVE_SPECIAL(gLastMoves[battlerDef]))
          && AI_DATA->hpPercents[battlerAtk] > 70)
        {
            if (gBattleMons[battlerAtk].statStages[STAT_SPDEF] < STAT_UP_2_STAGE)
                *score += 2; // seems better to raise spdef at higher HP
            else if (gBattleMons[battlerAtk].statStages[STAT_SPDEF] < STAT_UP_STAGE)
                *score += 1;
        }
        break;
    case STAT_ACC:
        if (HasMoveWithLowAccuracy(battlerAtk, battlerDef, 80, TRUE, AI_DATA->abilities[battlerAtk], AI_DATA->abilities[battlerDef], AI_DATA->holdEffects[battlerAtk], AI_DATA->holdEffects[battlerDef]))
            *score += 2; // has moves with less than 80% accuracy
        else if (HasMoveWithLowAccuracy(battlerAtk, battlerDef, 90, TRUE, AI_DATA->abilities[battlerAtk], AI_DATA->abilities[battlerDef], AI_DATA->holdEffects[battlerAtk], AI_DATA->holdEffects[battlerDef]))
            *score += 1;
        break;
    case STAT_EVASION:
        if (!BattlerWillFaintFromWeather(battlerAtk, AI_DATA->abilities[battlerAtk]))
        {
            if (!GetBattlerSecondaryDamage(battlerAtk) && !(gStatuses3[battlerAtk] & STATUS3_ROOTED))
                *score += 2;
            else
                *score += 1;
        }
        break;
    }
}

void IncreasePoisonScore(u8 battlerAtk, u8 battlerDef, u16 move, s16 *score)
{
    if (AI_RandLessThan(128))
        return;

    if ((AI_THINKING_STRUCT->aiFlags & AI_FLAG_TRY_TO_FAINT) && CanAIFaintTarget(battlerAtk, battlerDef, 0))
        return;

    if (AI_CanPoison(battlerAtk, battlerDef, AI_DATA->abilities[battlerDef], move, AI_DATA->partnerMove) && AI_DATA->hpPercents[battlerDef] > 20)
    {
        if (!HasDamagingMove(battlerDef))
            *score += 1;

        if (HasMoveEffect(battlerAtk, EFFECT_PROTECT))
            *score += 1;    // stall tactic

        if (HasMoveEffect(battlerAtk, EFFECT_VENOSHOCK)
          || HasMoveEffect(battlerAtk, EFFECT_HEX)
          || HasMoveEffect(battlerAtk, EFFECT_INFERNAL_PARADE)
          || HasMoveEffect(battlerAtk, EFFECT_VENOM_DRENCH)
          || AI_DATA->abilities[battlerAtk] == ABILITY_MERCILESS)
            *score += 1;
    }
}

void IncreaseBurnScore(u8 battlerAtk, u8 battlerDef, u16 move, s16 *score)
{
    if (AI_RandLessThan(128))
        return;

    if ((AI_THINKING_STRUCT->aiFlags & AI_FLAG_TRY_TO_FAINT) && CanAIFaintTarget(battlerAtk, battlerDef, 0))
        return;

    if (AI_CanBurn(battlerAtk, battlerDef, AI_DATA->abilities[battlerDef], BATTLE_PARTNER(battlerAtk), move, AI_DATA->partnerMove))
    {
        *score += 1; // burning is good
        if (HasMoveWithSplit(battlerDef, SPLIT_PHYSICAL))
            *score += 1; // burning the target to stay alive is cool
        else if (HasMoveEffect(battlerAtk, EFFECT_HEX) || HasMoveEffect(BATTLE_PARTNER(battlerAtk), EFFECT_HEX)
          || HasMoveEffect(battlerAtk, EFFECT_INFERNAL_PARADE) || HasMoveEffect(BATTLE_PARTNER(battlerAtk), EFFECT_INFERNAL_PARADE))
            *score += 1;
    }
}

void IncreaseParalyzeScore(u8 battlerAtk, u8 battlerDef, u16 move, s16 *score)
{
    if ((AI_THINKING_STRUCT->aiFlags & AI_FLAG_TRY_TO_FAINT) && CanAIFaintTarget(battlerAtk, battlerDef, 0))
        return;

    if (AI_CanParalyze(battlerAtk, battlerDef, AI_DATA->abilities[battlerDef], move, AI_DATA->partnerMove))
    {
        u8 atkSpeed = GetBattlerTotalSpeedStat(battlerAtk);
        u8 defSpeed = GetBattlerTotalSpeedStat(battlerDef);

        if ((defSpeed >= atkSpeed && defSpeed / 4 < atkSpeed) // You'll go first after paralyzing foe
          || HasMoveEffect(battlerAtk, EFFECT_HEX)
          || HasMoveEffect(battlerAtk, EFFECT_INFERNAL_PARADE)
          || HasMoveEffect(battlerAtk, EFFECT_FLINCH_HIT)
          || gBattleMons[battlerDef].status2 & STATUS2_INFATUATION
          || gBattleMons[battlerDef].status2 & STATUS2_CONFUSION)
            *score += 2;
        else
            *score += 1;
    }
}

void IncreaseSleepScore(u8 battlerAtk, u8 battlerDef, u16 move, s16 *score)
{
    if (AI_RandLessThan(128))
        return;
    if ((AI_THINKING_STRUCT->aiFlags & AI_FLAG_TRY_TO_FAINT) && CanAIFaintTarget(battlerAtk, battlerDef, 0))
        return;

    if (AI_CanPutToSleep(battlerAtk, battlerDef, AI_DATA->abilities[battlerDef], move, AI_DATA->partnerMove))
        *score += 1;
    else
        return;

    if ((HasMoveEffect(battlerAtk, EFFECT_DREAM_EATER) || HasMoveEffect(battlerAtk, EFFECT_NIGHTMARE))
      && !(HasMoveEffect(battlerDef, EFFECT_SNORE) || HasMoveEffect(battlerDef, EFFECT_SLEEP_TALK)))
        (*score)++;

    if (HasMoveEffect(battlerAtk, EFFECT_HEX) || HasMoveEffect(BATTLE_PARTNER(battlerAtk), EFFECT_HEX)
      || HasMoveEffect(battlerAtk, EFFECT_INFERNAL_PARADE) || HasMoveEffect(BATTLE_PARTNER(battlerAtk), EFFECT_INFERNAL_PARADE)
    )
        (*score)++;
}

void IncreaseConfusionScore(u8 battlerAtk, u8 battlerDef, u16 move, s16 *score)
{
    if (AI_RandLessThan(64))
        return;
    if ((AI_THINKING_STRUCT->aiFlags & AI_FLAG_TRY_TO_FAINT) && CanAIFaintTarget(battlerAtk, battlerDef, 0))
        return;

    if (AI_CanConfuse(battlerAtk, battlerDef, AI_DATA->abilities[battlerDef], BATTLE_PARTNER(battlerAtk), move, AI_DATA->partnerMove))
    {
        if (gBattleMons[battlerDef].status1 & STATUS1_PARALYSIS
          || gBattleMons[battlerDef].status2 & STATUS2_INFATUATION
          || (AI_DATA->abilities[battlerAtk] == ABILITY_SERENE_GRACE && HasMoveEffect(battlerAtk, EFFECT_FLINCH_HIT)))
            *score += 2;
        else
            *score += 1;
    }
}

void IncreaseFrostbiteScore(u8 battlerAtk, u8 battlerDef, u16 move, s16 *score)
{
    if (AI_RandLessThan(128))
        return;

    if ((AI_THINKING_STRUCT->aiFlags & AI_FLAG_TRY_TO_FAINT) && CanAIFaintTarget(battlerAtk, battlerDef, 0))
        return;

    if (AI_CanGiveFrostbite(battlerAtk, battlerDef, AI_DATA->abilities[battlerDef], BATTLE_PARTNER(battlerAtk), move, AI_DATA->partnerMove))
    {
        *score += 1; // frostbite is good
        if (HasMoveWithSplit(battlerDef, SPLIT_SPECIAL))
            *score += 1;
        else if (HasMoveEffect(battlerAtk, EFFECT_HEX) || HasMoveEffect(BATTLE_PARTNER(battlerAtk), EFFECT_HEX)
          || HasMoveEffect(battlerAtk, EFFECT_INFERNAL_PARADE) || HasMoveEffect(BATTLE_PARTNER(battlerAtk), EFFECT_INFERNAL_PARADE))
            *score += 1;
    }
}

bool32 AI_MoveMakesContact(u32 ability, u32 holdEffect, u16 move)
{
    if (gBattleMoves[move].makesContact
      && ability != ABILITY_LONG_REACH
      && holdEffect != HOLD_EFFECT_PROTECTIVE_PADS)
        return TRUE;
    return FALSE;
}

//TODO - this could use some more sophisticated logic
bool32 ShouldUseZMove(u8 battlerAtk, u8 battlerDef, u16 chosenMove)
{
    // simple logic. just upgrades chosen move to z move if possible, unless regular move would kill opponent
    if ((gBattleTypeFlags & BATTLE_TYPE_DOUBLE) && battlerDef == BATTLE_PARTNER(battlerAtk))
        return FALSE; //don't use z move on partner
    if (gBattleStruct->zmove.used[battlerAtk])
        return FALSE;   //cant use z move twice

    if (IsViableZMove(battlerAtk, chosenMove))
    {
        u8 effectiveness;

        if (IsMimikyuDisguiseProtected(battlerDef, battlerAtk))
            return FALSE; // Don't waste a Z-Move busting disguise
        if (gBattleMons[battlerDef].ability == ABILITY_ICE_FACE && gBattleMons[battlerDef].species == SPECIES_EISCUE && IS_MOVE_PHYSICAL(chosenMove))
            return FALSE; // Don't waste a Z-Move busting Ice Face

        if (IS_MOVE_STATUS(chosenMove) && !IS_MOVE_STATUS(gBattleStruct->zmove.chosenZMove))
            return FALSE;
        else if (!IS_MOVE_STATUS(chosenMove) && IS_MOVE_STATUS(gBattleStruct->zmove.chosenZMove))
            return FALSE;

        if (!IS_MOVE_STATUS(chosenMove) && AI_CalcDamage(chosenMove, battlerAtk, battlerDef, &effectiveness, FALSE, FALSE) >= gBattleMons[battlerDef].hp)
            return FALSE;   // don't waste damaging z move if can otherwise faint target

        return TRUE;
    }

    return FALSE;
}

bool32 AI_IsBattlerAsleepOrComatose(u8 battlerId)
{
    return (gBattleMons[battlerId].status1 & STATUS1_SLEEP) || AI_DATA->abilities[battlerId] == ABILITY_COMATOSE;
}

void ShouldSetupOffensive(u8 battlerAtk, u8 battlerDef, u8 statId, s16 *score, bool32 isDamagingMove)
{
    u8 faster = AI_WhosFaster(battlerAtk, battlerDef);

    if (IsBattlerIncapacitated(battlerDef, AI_DATA->abilities[battlerDef]) && AI_RandLessThan(230)) //%90
        *score += 3;
    else if (!CanAIFaintTarget(battlerDef, battlerAtk, 3) && AI_RandLessThan(204)) //%80
    {
        *score += 1;
        if (faster == AI_IS_FASTER)
            *score += 1;
    }

    //If Player outspeeds and 2HKO'd AI, AI cannot setup. 
    //AI accounts for Disguise properly
    if (faster == AI_IS_SLOWER
    && CanAIFaintTarget(battlerDef, battlerAtk, 2)
    && !IsMimikyuDisguiseProtected(battlerAtk, battlerDef))
        *score -= 5;
    if (faster == AI_IS_SLOWER
        && CanAIFaintTarget(battlerDef, battlerAtk, 1)
        && IsMimikyuDisguiseProtected(battlerAtk, battlerDef))
        *score -= 5;

    if (((HasMove(battlerDef, MOVE_BURNING_JEALOUSY) || HasMove(battlerDef, MOVE_ALLURING_VOICE)) && faster == AI_IS_FASTER && statId == STAT_ATK)
        || (HasMoveEffect(battlerDef, EFFECT_FOUL_PLAY) && statId == STAT_ATK)
        || (HasMoveEffect(battlerDef, EFFECT_CONFUSE) && statId == STAT_ATK)
        || (HasMoveEffect(battlerDef, EFFECT_ROAR) && CountUsablePartyMons(battlerAtk) != 0)
        || (HasMoveEffect(battlerDef, EFFECT_HIT_SWITCH_TARGET) && CountUsablePartyMons(battlerAtk) != 0))
        *score -= 5;
    
    if (gBattleMons[battlerAtk].statStages[statId] > 7 && AI_RandLessThan(204))
        *score -= 1;

    if (isDamagingMove)
        *score += 6;
    
    return;
}

void ShouldSetupDefensive(u8 battlerAtk, u8 battlerDef, u8 statId, s16 *score, bool32 isDamagingMove)
{
    u8 faster = AI_WhosFaster(battlerAtk, battlerDef);

    //If Player outspeeds and 2HKO'd AI, AI cannot setup. 
    //AI accounts for Disguise properly
    if (faster == AI_IS_SLOWER 
        && CanAIFaintTarget(battlerDef, battlerAtk, 2)
        && !IsMimikyuDisguiseProtected(battlerAtk, battlerDef))
        *score -= 5;

    if (faster == AI_IS_SLOWER
        && CanAIFaintTarget(battlerDef, battlerAtk, 1)
        && IsMimikyuDisguiseProtected(battlerAtk, battlerDef))
        *score -= 5;

    if ((HasMoveEffect(battlerDef, EFFECT_ROAR) && CountUsablePartyMons(battlerAtk) != 0)
        || (HasMoveEffect(battlerDef, EFFECT_HIT_SWITCH_TARGET) && CountUsablePartyMons(battlerAtk) != 0))
        *score -= 5;
    
    if (AI_RandLessThan(51)) //20%
        return;

    if (IsBattlerIncapacitated(battlerDef, AI_DATA->abilities[battlerDef]) && AI_RandLessThan(230)) //%90
        *score += 2;

    if (statId == STAT_DEF)
    {
        if (HasMoveWithSplit(battlerDef, SPLIT_PHYSICAL) && !HasMoveWithSplit(battlerDef, SPLIT_SPECIAL))
            *score += 1;
        if (gBattleMons[battlerAtk].statStages[statId] > 7 && !HasMoveEffect(battlerAtk, EFFECT_BODY_PRESS))
            *score -= 1;
    }

    if (statId == STAT_SPDEF)
    {
        if (HasMoveWithSplit(battlerDef, SPLIT_SPECIAL) && !HasMoveWithSplit(battlerDef, SPLIT_PHYSICAL))
            *score += 1;
        if (gBattleMons[battlerAtk].statStages[statId] > 7 && !HasMoveEffect(battlerAtk, EFFECT_STORED_POWER))
            *score -= 1;
    }

    if (statId == STAT_HP) // Cosmic Power
    {
        if (gBattleMons[battlerAtk].statStages[STAT_SPDEF] < 7
        || gBattleMons[battlerAtk].statStages[STAT_DEF] < 7)
            *score += 2;
    }

    if ((HasMoveEffect(battlerAtk, EFFECT_STORED_POWER)
    || HasMoveEffect(battlerAtk, EFFECT_BODY_PRESS))
    && Random() % 2 == 0)
        *score += 1;

    if (isDamagingMove)
        *score += 6;

    return;
}

void ShouldSetupOthers(u8 battlerAtk, u8 battlerDef, u16 move, s16 *score, bool32 isDamagingMove)
{
    if (move == MOVE_BULK_UP)
    {
        if (HasMoveWithSplit(battlerDef, SPLIT_PHYSICAL) && !HasMoveWithSplit(battlerDef, SPLIT_SPECIAL))
            ShouldSetupDefensive(battlerAtk, battlerDef, STAT_DEF, score, isDamagingMove);
        else ShouldSetupOffensive(battlerAtk, battlerDef, STAT_ATK, score, isDamagingMove);
    }
    else if (move == MOVE_CALM_MIND)
    {
        if (HasMoveWithSplit(battlerDef, SPLIT_SPECIAL) && !HasMoveWithSplit(battlerDef, SPLIT_PHYSICAL))
            ShouldSetupDefensive(battlerAtk, battlerDef, STAT_SPDEF, score, isDamagingMove);
        else ShouldSetupOffensive(battlerAtk, battlerDef, STAT_SPATK, score, isDamagingMove);
    }
    return;
}

bool32 CanFaintTargetAfterDrum(u8 battlerAtk, u8 battlerDef, u16 move)
{
    u8 faster = AI_WhosFaster(battlerAtk, battlerDef);
    s32 i, dmg;
    u32 moveLimitations = AI_DATA->moveLimitations[battlerAtk];
    u16 *moves = gBattleMons[battlerDef].moves;
    u16 hpTreshold;
    u16 currentHP = gBattleMons[battlerAtk].hp;
    u16 maxHP = gBattleMons[battlerAtk].maxHP;
    u8 effectiveness;
    u8 factor;

    if (move == MOVE_BELLY_DRUM)
        factor = 2;
    else if (move == MOVE_CLANGOROUS_SOUL)
        factor = 3;

    if (AI_DATA->items[battlerAtk] == ITEM_SITRUS_BERRY)
        currentHP = maxHP * 0.25 + currentHP;
    else if ((gBattleMons[battlerAtk].ability == ABILITY_GLUTTONY && 
            (AI_DATA->items[battlerAtk] == ITEM_IAPAPA_BERRY
            || AI_DATA->items[battlerAtk] == ITEM_WIKI_BERRY
            || AI_DATA->items[battlerAtk] == ITEM_AGUAV_BERRY
            || AI_DATA->items[battlerAtk] == ITEM_FIGY_BERRY)))
                currentHP = maxHP * 0.5 + currentHP;
    
    hpTreshold = currentHP - (maxHP / factor);

    for (i = 0; i < MAX_MON_MOVES; i++)
    {
        if (moves[i] != MOVE_NONE && moves[i] != 0xFFFF && !(moveLimitations & gBitTable[i]))
        {
            dmg = AI_DATA->maxrollDmg[battlerDef][battlerAtk][i];

            if (hpTreshold <= dmg)
                return TRUE;
        }
    }
    return FALSE;
}

bool32 CanFaintTargetAfterSmash(u8 battlerAtk, u8 battlerDef)
{
    s32 i, dmg;
    u32 moveLimitations = AI_DATA->moveLimitations[battlerAtk];
    u16 *moves = gBattleMons[battlerDef].moves;
    u8 faster = AI_WhosFaster(battlerAtk, battlerDef);
    u8 effectiveness;

    if (((HasMove(battlerDef, MOVE_BURNING_JEALOUSY) || HasMove(battlerDef, MOVE_ALLURING_VOICE) || HasMoveEffect(battlerDef, EFFECT_FOUL_PLAY)) && faster == AI_IS_FASTER))
        return TRUE;

    if ((AI_DATA->items[battlerAtk] == ITEM_FOCUS_SASH
    || gBattleMons[battlerAtk].ability == ABILITY_STURDY)
    && gBattleMons[battlerAtk].hp == gBattleMons[battlerAtk].maxHP)
        return FALSE;

    for (i = 0; i < MAX_MON_MOVES; i++)
    {
        if (moves[i] != MOVE_NONE && moves[i] != 0xFFFF && !(moveLimitations & gBitTable[i]))
        {
            dmg = AI_DATA->maxrollDmg[battlerDef][battlerAtk][i];

            if (faster == AI_IS_FASTER && AI_DATA->items[battlerAtk] != ITEM_WHITE_HERB)
                dmg = dmg * 1.5;

            if (gBattleMons[battlerAtk].hp <= dmg)
                return TRUE;
        }
    }
    return FALSE;
}

bool16 ShouldImprison(u8 battlerAtk, u8 battlerDef)
{
    u8 i, j;

    for (i = 0; i < MAX_MON_MOVES; i++)
        for (j = 0; j < MAX_MON_MOVES; j++)
            if (gBattleMons[battlerAtk].moves[i] == gBattleMons[battlerDef].moves[j])
                return TRUE;
    
    return FALSE;
}

bool16 ShouldTailwind(u8 battlerAtk, u8 battlerDef)
{
    u16 atkSpeed1 = 1000;
    u16 atkSpeed2 = 1000;
    u16 defSpeed1 = 0;
    u16 defSpeed2 = 0;
    
    if (IsBattlerAlive(battlerAtk))
        atkSpeed1 = GetBattlerTotalSpeedStat(battlerAtk);

    if (IsDoubleBattle() && IsBattlerAlive(BATTLE_PARTNER(battlerAtk)))
        atkSpeed2 = GetBattlerTotalSpeedStat(BATTLE_PARTNER(battlerAtk));
    
    if (IsBattlerAlive(battlerDef))
        defSpeed1 = GetBattlerTotalSpeedStat(battlerDef);

    if (IsDoubleBattle() && IsBattlerAlive(BATTLE_PARTNER(battlerDef)))
        defSpeed2 = GetBattlerTotalSpeedStat(BATTLE_PARTNER(battlerDef));
    
    if ((atkSpeed1 < defSpeed1) || (atkSpeed1 < defSpeed2)
    || (atkSpeed2 < defSpeed1) || (atkSpeed2 < defSpeed2))
        return TRUE;
    
    return FALSE;
}

bool16 AI_WhosFaster(u8 battlerAtk, u8 battlerDef)
{
    u16 atkSpeed = 0;
    u16 defSpeed = 0;
    
    if (IsBattlerAlive(battlerAtk))
        atkSpeed = GetBattlerTotalSpeedStat(battlerAtk);

    if (IsBattlerAlive(battlerDef))
        defSpeed = GetBattlerTotalSpeedStat(battlerDef);

    if (gFieldStatuses & STATUS_FIELD_TRICK_ROOM)
    {
        if (atkSpeed <= defSpeed)
            return AI_IS_FASTER;
        return AI_IS_SLOWER;
    }
    if (atkSpeed >= defSpeed)
        return AI_IS_FASTER;
    return AI_IS_SLOWER;
}

bool16 ShouldReflectType(u8 battlerAtk, u8 battlerDef)
{
    int i;
    for (i = 0; i < MAX_MON_MOVES; i++) {
        if (AI_GetMoveEffectiveness(gBattleMons[battlerAtk].moves[i], battlerAtk, battlerDef) > AI_EFFECTIVENESS_x0_5) {
            return FALSE;
        }
    }
    return TRUE;
}

bool16 IsMimikyuDisguiseProtected(u8 mimikyu, u8 opponent)
{
    if (gBattleMons[mimikyu].species != SPECIES_MIMIKYU)
        return FALSE;
    if (DoesBattlerIgnoreAbilityChecks(opponent, MOVE_NONE))
        return FALSE;
    else if (AI_DATA->abilities[mimikyu] == ABILITY_DISGUISE)
        return TRUE;
} 

bool16 IsBattlerSashSturdyProtected(u8 battler)
{
    return ((AI_DATA->holdEffects[battler] == HOLD_EFFECT_FOCUS_SASH || AI_DATA->abilities[battler] == ABILITY_STURDY)
                && GetHealthPercentage(battler) == 100);
}

bool16 IsBattlerSubstituteProtected(u8 substituteBattler, u8 opponent)
{
    return (gBattleMons[substituteBattler].status2 & STATUS2_SUBSTITUTE && AI_DATA->abilities[opponent] != ABILITY_INFILTRATOR);
}