/*
 * Copyright (C) 2008-2012 TrinityCore <http://www.trinitycore.org/>
 * Copyright (C) 2005-2009 MaNGOS <http://getmangos.com/>
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the
 * Free Software Foundation; either version 2 of the License, or (at your
 * option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE. See the GNU General Public License for
 * more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program. If not, see <http://www.gnu.org/licenses/>.
 */

#ifndef TRINITY_FORMULAS_H
#define TRINITY_FORMULAS_H

#include "World.h"
#include "SharedDefines.h"
#include "ScriptMgr.h"

namespace MoPCore
{
    namespace Honor
    {
        inline float hk_honor_at_level_f(uint8 level, float multiplier = 1.0f)
        {
            float honor = multiplier * float(level / 10) * 100; // MOP: 9 Honor points per kill divided among all friendly players in range from group @ lvl 90.
            sScriptMgr->OnHonorCalculation(honor, level, multiplier);
            return honor;
        }

        inline uint32 hk_honor_at_level(uint8 level, float multiplier = 1.0f)
        {
            return uint32(ceil(hk_honor_at_level_f(level, multiplier)));
        }
    }

    namespace XP
    {
        inline uint8 GetGrayLevel(uint8 pl_level)
        {
            uint8 level;

            if (pl_level <= 5)
                level = 0;
            else if (pl_level <= 39)
                level = (pl_level - 5) - floor(pl_level / 10);
            else if (pl_level <= 59)
                level = (pl_level - 1) - floor(pl_level / 5);
            else
                level = pl_level - 9;

            sScriptMgr->OnGrayLevelCalculation(level, pl_level);
            return level;
        }

        inline XPColorChar GetColorCode(uint8 pl_level, uint8 mob_level)
        {
            XPColorChar color;

            if (mob_level >= pl_level + 5)
                color = XP_RED;
            else if (mob_level >= pl_level + 3)
                color = XP_ORANGE;
            else if (mob_level >= pl_level - 2)
                color = XP_YELLOW;
            else if (mob_level > GetGrayLevel(pl_level))
                color = XP_GREEN;
            else
                color = XP_GRAY;

            sScriptMgr->OnColorCodeCalculation(color, pl_level, mob_level);
            return color;
        }

        inline uint8 GetZeroDifference(uint8 pl_level)
        {
            uint8 diff;

            if (pl_level < 8)
                diff = 5;
            else if (pl_level < 10)
                diff = 6;
            else if (pl_level < 12)
                diff = 7;
            else if (pl_level < 16)
                diff = 8;
            else if (pl_level < 20)
                diff = 9;
            else if (pl_level < 30)
                diff = 11;
            else if (pl_level < 40)
                diff = 12;
            else if (pl_level < 45)
                diff = 13;
            else if (pl_level < 50)
                diff = 14;
            else if (pl_level < 55)
                diff = 15;
            else if (pl_level < 60)
                diff = 16;
            else
                diff = 17;

            sScriptMgr->OnZeroDifferenceCalculation(diff, pl_level);
            return diff;
        }

        inline uint32 BaseGain(uint8 pl_level, uint8 mob_level, ContentLevels content)
        {
            uint32 baseGain;
            uint32 nBaseExp;

            switch (content)
            {
                case CONTENT_1_60:
                    nBaseExp = 45;
                    break;
                case CONTENT_61_70:
                    nBaseExp = 235;
                    break;
                case CONTENT_71_80:
                    nBaseExp = 580;
                    break;
                case CONTENT_81_85:
                    nBaseExp = 1878;
                    break;
                case CONTENT_86_90:
                    nBaseExp = 7512;
                    break;
                default:
                    sLog->outError(LOG_FILTER_GENERAL, "BaseGain: Unsupported content level %u", content);
                    nBaseExp = 45;
                    break;
            }

            uint32 baseXp = pl_level * 5 + nBaseExp;

            if (mob_level == pl_level)
                baseGain = baseXp;
            else if (mob_level > pl_level) // Creature level is higher.
            {
                // Starting from Cataclysm: XP = (Base XP) * (1 + 0.05 * (Mob Level - Char Level) ), where Mob Level > Char Level.
                // This is the amount of experience you will get for a solo kill on a mob whose level is higher than your level.
                // This is known to be valid for up to Mob Level = Char Level + 4 (Orange and high-Yellow Mobs).
                // Red mobs cap out at the same experience as orange. Higher elite mobs may also cap at the same amount (ie are not doubled).

                uint8 nLevelDiff = mob_level - pl_level;
                if (nLevelDiff > 4) // Orange is pl_level + 4. Red caps at Orange, does not double.
                    nLevelDiff = 4;

                baseGain = baseXp * (1 + 0.05 * nLevelDiff); // Old Patches: (baseXp * (20 + nLevelDiff) / 10 + 1) / 2.
            }
            else                           // Creature level is lower.
            {
                // Starting from Cataclysm: XP = (Base XP) * (1 - ((Char Level - Mob Level) / ZD)), where Mob Level < Char Level, and Mob Level > Gray Level.
                // For a given character level, the amount of XP given by lower-level mobs is a linear function of the Mob Level.
                // The amount of experience reaches zero when the difference between the Char Level and Mob Level reaches a certain point (Zero Difference).
                uint8 gray_level = GetGrayLevel(pl_level);
                if (mob_level > gray_level)
                {
                    uint8 ZD = GetZeroDifference(pl_level);
                    baseGain = baseXp * (1 - ((pl_level - mob_level) / ZD)); // Old Patches: baseXp * (ZD + mob_level - pl_level) / ZD.
                }
                else baseGain = 0;
            }

            sScriptMgr->OnBaseGainCalculation(baseGain, pl_level, mob_level, content);
            return baseGain;
        }

        inline uint32 Gain(Player* player, Unit* u)
        {
            uint32 gain;

            if (u->GetTypeId() == TYPEID_UNIT &&
                (((Creature*)u)->isTotem() || ((Creature*)u)->isPet() ||
                (((Creature*)u)->GetCreatureTemplate()->flags_extra & CREATURE_FLAG_EXTRA_NO_XP_AT_KILL) ||
                ((Creature*)u)->GetCreatureTemplate()->type == CREATURE_TYPE_CRITTER))
                gain = 0;
            else
            {
                gain = BaseGain(player->getLevel(), u->getLevel(), GetContentLevelsForMapAndZone(u->GetMapId(), u->GetZoneId()));

                if (gain != 0 && u->GetTypeId() == TYPEID_UNIT && ((Creature*)u)->isElite())
                {
                    // Mobs that are flagged as elite will give twice the amount of experience as a normal mob for the same level.
                    // Elites in instances have a 2.75x XP bonus instead of the regular 2x world bonus.
                    if (u->GetMap() && u->GetMap()->IsDungeon())
                       gain = uint32(gain * 2.75);
                    else
                        gain *= 2;
                }

                float KillXpRate = 1;

                if (player->GetPersonnalXpRate())
                    KillXpRate = player->GetPersonnalXpRate();
                else
                    KillXpRate = sWorld->getRate(RATE_XP_KILL);

                float premium_rate = player->GetSession()->GetVipLevel() ? sWorld->getRate(RATE_XP_KILL_VIP) : 1.0f;

            return uint32(gain*sWorld->getRate(RATE_XP_KILL)*premium_rate);
            }

            sScriptMgr->OnGainCalculation(gain, player, u);
            return gain;
        }

        inline float xp_in_group_rate(uint32 count, bool isRaid)
        {
            float rate;

            if (isRaid)
            {
                // FIXME: Must apply decrease modifiers depending on raid size.
                rate = 1.0f;
            }
            else
            {
                switch (count)
                {
                    case 0:
                    case 1:
                    case 2:
                        rate = 1.0f;
                        break;
                    case 3:
                        rate = 1.166f;
                        break;
                    case 4:
                        rate = 1.3f;
                        break;
                    case 5:
                    default:
                        rate = 1.4f;
                }
            }

            sScriptMgr->OnGroupRateCalculation(rate, count, isRaid);
            return rate;
        }
    }

    namespace Currency
    {
        inline uint32 ConquestRatingCalculator(uint32 rate)
        {
            if (rate <= 1500)
                return 1800; // Default conquest points
            else if (rate > 3000)
                rate = 3600;

            // http://www.arenajunkies.com/topic/179536-conquest-point-cap-vs-personal-rating-chart/page__st__60#entry3085246
            return uint32(1.4326 * ((1511.26 / (1 + 1639.28 / exp(0.00412 * rate))) + 1050.15));
        }

        inline uint32 BgConquestRatingCalculator(uint32 rate)
        {
            // WowWiki: Battleground ratings receive a bonus of 22.2% to the cap they generate
            return uint32((ConquestRatingCalculator(rate) * 1.222f) + 0.5f);
        }
    }
}

#endif
