#include "jgldefs.h"
#include "GolferDialogue.h"
#include "MainLoop.h"

void SetGolferAction(int golfer, int action, int actionArg)
{
    int himask = MOOD_ANNOYED;
    Golfer saved;
    char pronoun[8];
    int i, voice, mood, ti, code, mood0;

    signed char st = g_golferPool[golfer].holeStrokes;
    if (st > 9)
        return;
    if (golfer >= 152)
        return;
    if (g_runModeFlags & GAME_SCRIPT_PLAYING)
        return;

    if (action == 0x13) {
        if ((g_golferPool[golfer].persona & PERSONA_KIND) != PERSONA_ROSTER) {
            int hole = g_golferPool[golfer].holeIdx;
            if (g_allHolesArray[hole].rounds >= 10) {
                int over = g_golferPool[golfer].holeScore[hole] - g_allHolesArray[hole].par;
                int hf = g_allHolesArray[hole].flags;
                if (((hf & 4) && over > 1) || ((hf & 8) && over < 0))
                    action = 0x17;
            }
        }
    }

    code = g_golferPool[golfer].holeIdx * 11 + st;
    BuildGolferDialogue(action, actionArg, code, golfer);
    FormatGolferMessage(golfer, 0);

    ti = g_gameSpeakerIsPartner ? g_golferPool[golfer].partner : golfer;
    g_golferPool[ti].talkTimer = TALK_SHOW_TICKS;
    g_golferPool[ti].msgLine = action;
    g_golferPool[ti].msgStamp = (unsigned char)(g_golferPool[golfer].holeIdx * 11 + g_golferPool[golfer].holeStrokes);
    g_golferPool[ti].msgCode = actionArg;
    if (g_gameSpeakerIsPartner)
        g_golferPool[ti].msgLine |= MSG_LINE_PARTNER;
    g_actionArgs[action] = actionArg;

    if (action == 0x23) {
        PlayPositionedSound(IsGolferMale(golfer) + 0x46, g_golferPool[golfer].x, g_golferPool[golfer].y, 0);
    } else if (action == 0x13) {
        return;
    }
    if (action == 0x23 && g_golferPool[golfer].slotKind[0] == action)
        return;

    saved = g_golferPool[golfer];
    for (i = 9; i > 0; i--) {
        g_golferPool[golfer].slotKind[i] = g_golferPool[golfer].slotKind[i - 1];
        g_golferPool[golfer].slotStamp[i] = g_golferPool[golfer].slotStamp[i - 1];
        g_golferPool[golfer].slotParams[i] = g_golferPool[golfer].slotParams[i - 1];
    }
    g_golferPool[golfer].slotKind[0] = action;
    g_golferPool[golfer].slotStamp[0] =
        (signed char)(g_golferPool[golfer].holeIdx * 11 + g_golferPool[golfer].holeStrokes);
    g_golferPool[golfer].slotParams[0] = actionArg;
    if (action >= 4)
        g_golferPool[golfer].slotStamp[0]++;

    voice = (g_golferTypeArray[g_golferPool[golfer].typeIdx].appearance >> 4) & 0xf;
    if (!IsGolferMale(golfer))
        voice += 5;

    mood = 0;
    switch (action - 1) {
    case 0:
        mood = 1;
        PlayPositionedSound(((g_golferPool[golfer].flags & GOLFER_ALT_REACTION) ? 0xe6 : 0xd2) + voice,
            g_golferPool[golfer].x, g_golferPool[golfer].y, 0);
        break;
    case 5:
        mood = 1;
        PlayPositionedSound(0x14 + IsGolferMale(golfer), g_golferPool[golfer].x, g_golferPool[golfer].y, 0);
        break;
    case 38:
        mood = (g_level < 2);
        PlayPositionedSound(0x14 + IsGolferMale(golfer), g_golferPool[golfer].x, g_golferPool[golfer].y, 0);
        break;
    case 31:
    case 32:
        mood = 1;
        PlayPositionedSound(0xa2 + IsGolferMale(golfer), g_golferPool[golfer].x, g_golferPool[golfer].y, 0);
        break;
    case 1:
        mood = -1;
        PlayPositionedSound(((g_golferPool[golfer].flags & GOLFER_ALT_REACTION) ? 0x10 : 0xdc) + voice,
            g_golferPool[golfer].x, g_golferPool[golfer].y,
            g_gameTileTerrainKind[(g_golferPool[golfer].x >> 10) * 50 + (g_golferPool[golfer].y >> 10)] == TERRAIN_WATER
                ? 1000
                : 0);
        break;
    case 2:
        mood = -2;
        PlayPositionedSound(((g_golferPool[golfer].flags & GOLFER_ALT_REACTION) ? 0x10 : 0xdc) + voice,
            g_golferPool[golfer].x, g_golferPool[golfer].y,
            g_gameTileTerrainKind[(g_golferPool[golfer].x >> 10) * 50 + (g_golferPool[golfer].y >> 10)] == TERRAIN_WATER
                ? 1000
                : 0);
        break;
    case 7:
        mood = -2;
        PlayPositionedSound(((g_golferPool[golfer].flags & GOLFER_ALT_REACTION) ? 0x10 : 0xdc) + voice,
            g_golferPool[golfer].x, g_golferPool[golfer].y, 0);
        break;
    case 8:
        mood = -3;
        PlayPositionedSound(0x46 + IsGolferMale(golfer), g_golferPool[golfer].x, g_golferPool[golfer].y, 0);
        break;
    case 34:
        mood = -2;
        PlayPositionedSound(0x46 + IsGolferMale(golfer), g_golferPool[golfer].x, g_golferPool[golfer].y, 0);
        break;
    case 3:
        mood = -2;
        PlayPositionedSound(0x12 + IsGolferMale(golfer), g_golferPool[golfer].x, g_golferPool[golfer].y, 0);
        break;
    case 4:
    case 30: PlayPositionedSound(0x10 + IsGolferMale(golfer), g_golferPool[golfer].x, g_golferPool[golfer].y, 0); break;
    case 6:
        if (actionArg)
            break;
        mood = 1;
        PlayPositionedSound(0x4c + IsGolferMale(golfer), g_golferPool[golfer].x, g_golferPool[golfer].y, 0);
        break;
    case 64:
        if (g_level > 1)
            mood = -2;
        PlayPositionedSound(0x16 + IsGolferMale(golfer), g_golferPool[golfer].x, g_golferPool[golfer].y, 0);
        break;
    case 35:
        mood = -3;
        PlayPositionedSound(0x3e + IsGolferMale(golfer), g_golferPool[golfer].x, g_golferPool[golfer].y, 0);
        break;
    case 9:
    case 42:
        mood = -2;
        PlayPositionedSound(0x50 + IsGolferMale(golfer), g_golferPool[golfer].x, g_golferPool[golfer].y, 0);
        break;
    case 10:
        mood = 1;
        PlayPositionedSound(0x4a + IsGolferMale(golfer), g_golferPool[golfer].x, g_golferPool[golfer].y, 0);
        break;
    case 27:
        mood = 1;
        PlayPositionedSound(0x4c + IsGolferMale(golfer), g_golferPool[golfer].x, g_golferPool[golfer].y, 0);
        g_golferPool[golfer].activity = ACTIVITY_TALK;
        g_golferPool[golfer].waitTimer = -0x18;
        break;
    case 11:
    case 12:
        mood = -1;
        PlayPositionedSound(0x8 + IsGolferMale(golfer), g_golferPool[golfer].x, g_golferPool[golfer].y, 0x1f4);
        break;
    case 13:
        mood = -1;
        PlayPositionedSound(0x9e + IsGolferMale(golfer), g_golferPool[golfer].x, g_golferPool[golfer].y, 0);
        break;
    case 14:
        mood = -1;
        PlayPositionedSound(0x9c + IsGolferMale(golfer), g_golferPool[golfer].x, g_golferPool[golfer].y, 0);
        break;
    case 25:
        mood = -1;
        PlayPositionedSound(0xa0 + IsGolferMale(golfer), g_golferPool[golfer].x, g_golferPool[golfer].y, 0);
        break;
    case 17:
        mood = (g_golferPool[golfer].hunger >= 8);
        if (mood) {
            if (g_golferTypeArray[g_golferPool[golfer].typeIdx].caps & 1)
                OnTraitMatched(golfer);
        }
        break;
    case 24:
        mood = (g_golferPool[golfer].thirst >= 8);
        PlayPositionedSound(0x3c + IsGolferMale(golfer), g_golferPool[golfer].x, g_golferPool[golfer].y, 0);
        if (mood) {
            if (g_golferTypeArray[g_golferPool[golfer].typeIdx].caps & 1)
                OnTraitMatched(golfer);
        }
        break;
    case 26:
        mood = (g_golferPool[golfer].fatigue >= 0x3c);
        PlayPositionedSound(0x48 + IsGolferMale(golfer), g_golferPool[golfer].x, g_golferPool[golfer].y, 0);
        if (mood) {
            if (g_golferTypeArray[g_golferPool[golfer].typeIdx].caps & 1)
                OnTraitMatched(golfer);
        }
        break;
    case 46: mood = -2; break;
    case 19:
        mood = -2;
        PlayPositionedSound(0x56 + IsGolferMale(golfer), g_golferPool[golfer].x, g_golferPool[golfer].y, 0);
        g_golferPool[golfer].activity = ACTIVITY_TALK;
        g_golferPool[golfer].waitTimer = -0x18;
        break;
    case 23:
        mood = -2;
        PlayPositionedSound(0x42 + IsGolferMale(golfer), g_golferPool[golfer].x, g_golferPool[golfer].y, 0);
        break;
    case 21:
        mood = 1;
        PlayPositionedSound(0x4e + IsGolferMale(golfer), g_golferPool[golfer].x, g_golferPool[golfer].y, 0);
        g_golferPool[golfer].activity = ACTIVITY_TALK;
        g_golferPool[golfer].waitTimer = -0x18;
        break;
    case 20:
        mood = -2;
        PlayPositionedSound(0x44 + IsGolferMale(golfer), g_golferPool[golfer].x, g_golferPool[golfer].y, 0);
        break;
    case 50:
    case 51:
    case 52:
        mood = (g_level == 0);
        if (g_golferTypeArray[g_golferPool[golfer].typeIdx].caps & 4)
            OnTraitMatched(golfer);
        break;
    case 53: mood = (g_level <= 1); break;
    case 22:
        mood = (g_level ? -2 : -1);
        PlayPositionedSound(0x9a + IsGolferMale(golfer), g_golferPool[golfer].x, g_golferPool[golfer].y, 0);
        break;
    case 28:
        mood = 1;
        PlayPositionedSound(0x96 + IsGolferMale(golfer), g_golferPool[golfer].x, g_golferPool[golfer].y, 0);
        break;
    case 29:
        mood = -2;
        PlayPositionedSound(0x98 + IsGolferMale(golfer), g_golferPool[golfer].x, g_golferPool[golfer].y, 0);
        break;
    case 57:
        PlayPositionedSound(0x5a + (IsGolferMale(golfer) ? 0x4b : 0), g_golferPool[golfer].x, g_golferPool[golfer].y,
            0);
        break;
    case 33:
        mood = 1;
        PlayPositionedSound(0xa4 + (IsGolferMale(golfer) ? 3 : 0), g_golferPool[golfer].x, g_golferPool[golfer].y, 0);
        break;
    case 58:
        if (g_golferHistoryPerCharacter[g_golferPool[golfer].typeIdx].perHoleFlags[g_golferPool[golfer].holeIdx])
            mood = 1;
        break;
    case 43:
    case 45: mood = 1; break;
    }

    if (action == 0x28)
        g_golferPool[golfer] = saved;
    if (!mood && action >= 0x30)
        return;

    mood0 = mood;
    if (g_golferPool[golfer].persona) {
        int opposingSign = -Sign(g_golferPool[golfer].attitude);
        if (opposingSign == Sign(mood))
            g_golferPool[golfer].attitude = 0;
        else
            g_golferPool[golfer].attitude += mood;
        if (g_golferPool[golfer].persona) {
            if (action == 2 || action == 3 || action == 0xd || action == 0xc) {
                if (g_golferPool[golfer].attitude < 0 && mood < 0 && g_golferPool[golfer].attitude < mood)
                    g_golferPool[golfer].attitude = 1;
            }
        }
        if (golfer == g_watchedPlayerIdx && mood < 0 && (g_golferPool[golfer].flags & GOLFER_BALL_MOVING))
            ConfigureAndPlaySound(SOUND_APPLAUSE_BAD, SOUND_FULL_VOLUME, 0, 0, 0);
    }

    if (mood == -1) {
        int mode = g_currentFeesOption;
        if (mode != 2 && !g_golferPool[golfer].persona) {
            int limit;
            mood = 0;
            for (i = 1;; i++) {
                if (g_level)
                    limit = (g_level > 2 ? 5 : 0) + 5;
                else
                    limit = 3;
                if (i >= limit)
                    break;
                if (g_golferPool[golfer].slotKind[i] == action)
                    mood = -1;
            }
            if (g_golferPool[golfer].slotParams[1] & himask)
                mood = -1;
            if (mode == 1) {
                mood = 0;
            }
        }
    }
    if (mood < 0) {

        if ((g_golferPool[golfer].persona & PERSONA_KIND) != PERSONA_COMMISSIONER)
            mood = (mood - 1) / 2;
        if (mood < 0 && (g_golferPool[golfer].persona & PERSONA_KIND) != PERSONA_ROSTER) {
            if (g_level >= ((g_golferPool[golfer].holeStrokes + golfer) & 1) + 2) {
                if (action != 0x2f && action != 0x15 && action != 0x18) {
                    int p = g_golferPool[golfer].partner;
                    if (g_golferPool[p].holeIdx > 0 && !(g_golferPool[p].talkTimer > 0)
                        && g_golferPool[p].enjoyment > 0) {
                        SetGolferAction(p, 0x2f, 0x14);
                        g_golferPool[g_golferPool[golfer].partner].talkTimer += 2;
                    }
                }
            }
        }
    }

    g_golferPool[golfer].enjoyment += mood;
    g_golferPool[golfer].enjoyment = Clamp(g_golferPool[golfer].enjoyment, -10, 10);
    g_allHolesArray[g_golferPool[golfer].holeIdx].moodSum += mood;
    if (g_rng.NextInt(6) <= g_level || (g_golferPool[golfer].flags & GOLFER_WALKED_OFF)) {
        if (mood < 0 && !(g_runModeFlags & GAME_CHAMPIONSHIP) && action != 0x2f
            && InLandmarkInfluence(g_golferPool[golfer].x, g_golferPool[golfer].y, 2) == 0) {
            int tx = g_golferPool[golfer].x >> 10;
            int ty = g_golferPool[golfer].y >> 10;
            int tile = tx * 50 + ty;
            unsigned short tf = g_tileExtra[tile];
            short blocked = tf & 0xc00;
            if (!blocked && g_gameTileTerrainKind[tx * 50 + ty] != TERRAIN_WATER) {
                tf |= 0x4800;
                g_tileAnimFrame[tile] = 1;
                g_tileExtra[tile] = tf;
                g_paintedCameraTileX = -1;
            }
        }
    }

    if (mood) {
        g_allHolesArray[g_golferPool[golfer].holeIdx].cmt[action]++;
        g_allHolesArray[g_golferPool[golfer].holeIdx].cmtCode[action] = actionArg;
        if (mood > 0)
            g_golferPool[golfer].slotParams[0] |= MOOD_PLEASED;
    }
    if (mood0 < 0)
        g_golferPool[golfer].slotParams[0] |= MOOD_ANNOYED;
    if (mood < 0)
        g_golferPool[golfer].slotParams[0] |= MOOD_MASK;
    if (mood > 0)
        g_posReactionTileCount[(g_golferPool[golfer].x >> 10) * 50 + (g_golferPool[golfer].y >> 10)]++;
    if (mood < 0)
        g_gameDispleasedTally[(g_golferPool[golfer].x >> 10) * 50 + (g_golferPool[golfer].y >> 10)]++;
    if (action == 0x32)
        return;

    strcpy(pronoun, IsGolferMale(golfer) ? "he" : "she");
    {
        int seen;
        if (action < 0x20)
            seen = (1 << action) & g_knownEventsLowMask;
        else
            seen = (1 << (action - 0x20)) & g_knownEventsHighMask;
        if (g_clock - g_lastShownStoryMsgTick <= 500)
            return;
        if (g_golferPool[golfer].rapport)
            return;
        if (seen)
            return;
        if (g_golferPool[golfer].screenX <= 100 || g_golferPool[golfer].screenX >= 700)
            return;
        if (g_golferPool[golfer].screenY <= 100 || g_golferPool[golfer].screenY >= 400)
            return;
    }

    strcpy(g_printBuffer, "'");
    BuildGolferDialogue(action, actionArg, g_golferPool[golfer].holeIdx * 11 + g_golferPool[golfer].holeStrokes,
        golfer);
    strcat(g_printBuffer, "' ");
    AppendGolferName(golfer, 0);

    switch (action - 1) {
    case 0: strcat(g_printBuffer, " has successfully made a tough-looking shot and is enjoying your course."); break;
    case 5: strcat(g_printBuffer, " plans to use the slope of the hill to run the ball towards the hole."); break;
    case 1:
        strcat(g_printBuffer, " is disappointed that an easy looking shot ended up in the ");
        strcat(g_printBuffer, g_squareTypeTable[actionArg].name);
        strcat(g_printBuffer, ". If it happens again ");
        strcat(g_printBuffer, pronoun);
        strcat(g_printBuffer, " will become discouraged.");
        break;
    case 2:
        strcat(g_printBuffer, " is disappointed that the shot has ended up in a worse location, the ");
        strcat(g_printBuffer, g_squareTypeTable[actionArg].name);
        strcat(g_printBuffer, ".");
        break;
    case 7:
        strcat(g_printBuffer,
            " cannot find a safe place to aim the next shot.  This can be frustrating. Perhaps you should add more fairway to this hole.");
        break;
    case 8:
        strcat(g_printBuffer,
            " was almost hit by a shot from another hole. This is very scary.  Try to keep your holes seperated, sometimes trees can be useful.");
        break;
    case 34: strcat(g_printBuffer, " has quit playing in frustration. Security may be required."); break;
    case 35: strcat(g_printBuffer, " does not enjoy seeing an angry golfer on your course."); break;
    case 3:
        strcat(g_printBuffer,
            "'s next shot does not involve many dangers or hazards. Too many shots like this may tend to bore your players.");
        break;
    case 4:
    case 36:
    case 37:
        strcat(g_printBuffer, "'s next shot looks challenging - ");
        strcat(g_printBuffer, pronoun);
        strcat(g_printBuffer, " will be happy if lands on the green or fairway.");
        break;
    case 30:
        strcat(g_printBuffer, "'s next shot looks extremely difficult. ");
        AppendGolferName(golfer, 0);
        strcat(g_printBuffer, " will be thrilled if it finds the green or fairway.");
        break;
    case 31:
    case 32:
        strcat(g_printBuffer, " has an interesting decision to make on this next shot.");
        strcat(g_printBuffer, " This is a characteristic of a well designed hole.");
        break;
    case 64:
        strcat(g_printBuffer,
            " hit a good shot but cannot see exactly where it landed because of trees or elevation. ");
        break;
    case 9:
        strcat(g_printBuffer, " is unhappy because no path has been provided to walk through the ");
        strcat(g_printBuffer, g_squareTypeTable[actionArg].name);
        strcat(g_printBuffer, ". Press 'p' to select the path tool.");
        break;
    case 38: strcat(g_printBuffer, " is intrigued by the wildlife on your course."); break;
    case 27:
        strcat(g_printBuffer, " is enjoying the lovely ");
        GetSceneryName(actionArg % 50, actionArg / 50, -1);
        strcat(g_printBuffer, " you have included in your course.");
        break;
    case 28: strcat(g_printBuffer, " likes the fact that this hole is very different from the last hole."); break;
    case 29:
        strcat(g_printBuffer,
            " dislikes the fact that this hole is very similar to the last hole. Try to vary the par and direction of your holes.");
        break;
    case 10:
        strcat(g_printBuffer, " is pleased to have a scenic ");
        GetSceneryName(actionArg % 50, actionArg / 50, -1);
        strcat(g_printBuffer, " in view for the next shot.");
        break;
    case 11:
        strcat(g_printBuffer, "'s ball just bounced off a tree. ");
        AppendGolferName(golfer, 0);
        strcat(g_printBuffer, " will be more careful next time.");
        break;
    case 12:
        strcat(g_printBuffer, "'s ball somehow ended up in the ");
        strcat(g_printBuffer, g_squareTypeTable[actionArg].name);
        strcat(g_printBuffer, ". ");
        AppendGolferName(golfer, 0);
        strcat(g_printBuffer, " will be more careful next time.");
        break;
    case 13:
        strcat(g_printBuffer, " needs to find a soda vendor soon or ");
        strcat(g_printBuffer, pronoun);
        strcat(g_printBuffer, " will be unhappy.");
        break;
    case 25:
        strcat(g_printBuffer, " needs to find a bench to rest on soon or ");
        strcat(g_printBuffer, pronoun);
        strcat(g_printBuffer, " will start to slow down.");
        break;
    case 26:
        strcat(g_printBuffer, " is relaxing on your comfy bench and will soon resume play with renewed energy.");
        break;
    case 14:
        strcat(g_printBuffer, " needs to find a snack bar soon or ");
        strcat(g_printBuffer, pronoun);
        strcat(g_printBuffer, " will be unhappy.");
        break;
    case 16: strcat(g_printBuffer, " is amazed that the last shot faded off to the right."); break;
    case 15: strcat(g_printBuffer, " is amazed that the last shot curved off to the left."); break;
    case 17: strcat(g_printBuffer, " is enjoying a tasty snack at your conveniently located snack bar."); break;
    case 24: strcat(g_printBuffer, " is enjoying a cool refreshing beverage from your friendly soda vendor."); break;
    case 19:
        strcat(g_printBuffer, " is disappointed to see a ");
        GetSceneryName(actionArg % 50, actionArg / 50, -1);
        strcat(g_printBuffer, " in the middle of your otherwise lovely golf course.");
        break;
    case 23:
        strcat(g_printBuffer,
            " is disappointed to see dandelions growing in the middle of your otherwise lovely golf course.");
        strcat(g_printBuffer, " Hire a groundskeeper to eradicate these weeds.");
        strcat(g_printBuffer, " Click the employees button on the People menu to hire course employees.");
        break;
    case 21:
        strcat(g_printBuffer, " is excited to see ");
        strcat(g_printBuffer, g_themeCelebsEntries[actionArg].name);
        strcat(g_printBuffer, "'s vacation home on your golf course.");
        break;
    case 20:
        strcat(g_printBuffer, " is frustrated that a lot of golfers are waiting at this hole.");
        strcat(g_printBuffer, " Placing a Ranger/Marshall near the tee will speed up play on this hole.");
        strcat(g_printBuffer, " Click the employees button on the People menu to hire course employees.");
        break;
    case 55: strcat(g_printBuffer, " is setting up to hit a fade shot which curves from left to right."); break;
    case 54: strcat(g_printBuffer, " is setting up to hit a draw shot which curves from right to left."); break;
    case 56:
        strcat(g_printBuffer, " is setting up to hit a high shot which will spin back once it hits the green.");
        break;
    case 59: strcat(g_printBuffer, " is setting up to hit a low shot which will run under the trees."); break;
    case 51: strcat(g_printBuffer, " appreciates the new pro shop you added to your course."); break;
    case 50: strcat(g_printBuffer, " appreciates the new driving range you added to your course."); break;
    case 52: strcat(g_printBuffer, " appreciates the new putting green you added to your course."); break;
    case 53:
        strcat(g_printBuffer,
            " is using this club for the first time today. A good course will use every club in the bag.");
        break;
    default: g_printBuffer[0] = 0; break;
    }

    {
        int id = 0x80000210;
        if (mood0 > 0)
            id = 0x80000288;
        if (mood0 < 0)
            id = 0x80005084;
        if (g_level && mood0 < -1)
            id = 0x80007084;
        if (!(g_configBitMask & 4))
            return;
        if (!g_printBuffer[0])
            return;
        if (ShowStoryMessage(id, -8, golfer)) {
            if (action < 0x20)
                g_knownEventsLowMask |= (1 << action);
            else
                g_knownEventsHighMask |= (1 << (action - 0x20));
            g_lastShownStoryMsgTick = g_clock;
        }
    }
}

void AppendPartnerNote(int enjoyment, int mode)
{
    switch (enjoyment) {
    case 0: strcat(g_printBuffer, "I hate this stupid course."); break;
    case 1: strcat(g_printBuffer, "I don't like this course much."); break;
    case 2: strcat(g_printBuffer, "I'm not having much fun today."); break;
    case 3: strcat(g_printBuffer, "This course is almost adequate."); break;
    case 4: strcat(g_printBuffer, "Well, I guess this course is OK."); break;
    case 5: strcat(g_printBuffer, "This course is quite nice."); break;
    case 6:
        if (mode <= 1)
            strcat(g_printBuffer, "This is a really good course.");
        else
            strcat(g_printBuffer, "This is a really surprising course.");
        break;
    case 7:
        if (mode < 1)
            strcat(g_printBuffer, "This course is excellent.");
        else
            strcat(g_printBuffer, "This course is fascinating.");
        break;
    default:
        if (enjoyment >= 0 && enjoyment <= 127) {
            if (mode < 1)
                strcat(g_printBuffer, "This is best course I've ever played.");
            else
                strcat(g_printBuffer, "yet this is a most interesting course.");
        } else
            strcat(g_printBuffer, "I hate this course, I'm leaving!");
        break;
    }
}

void BuildGolferDialogue(int msgId, int actionArg, int strokeCode, int golfer)
{
    int hole;
    char ownName[64];
    char dataName[64];
    char partnerName[64];
    char incoming[1024];
    const char *s;
    int diff, mask;
    int holeFlags, typeBits;
    short k;
    signed char mine, his;
    int typeLine = 0;

    hole = strokeCode / 11;
    g_gameSpeakerIsPartner = 0;
    if (golfer == SCRATCH_GOLFER) {
        memset(&g_golferPool[SCRATCH_GOLFER], 0, sizeof(Golfer));
    } else if (golfer < 0x98) {
        int i = 0;
        do {
            if (g_typeLineIds[i] == msgId
                && g_phrasesPerGolferType[g_golferPool[golfer].typeIdx * 1250 + i * 50] != 0) {
                strcat(g_printBuffer, &g_phrasesPerGolferType[g_golferPool[golfer].typeIdx * 1250 + i * 50]);
                typeLine = 1;
                break;
            }
            ++i;
        } while (g_typeLineIds[i] != -1);
    }
    if (!typeLine) {
        g_remarkColourValue = 0x80006318;

        switch (msgId) {
        case 62: {
            int sp = g_golferTypeArray[g_golferPool[golfer].typeIdx].head;
            if (!IsGolferMale(golfer))
                sp += 20;
            strcat(g_printBuffer, g_headPresets[sp].line);
            break;
        }
        case 63:
            if ((g_golferPool[golfer].persona & PERSONA_KIND) == PERSONA_ROSTER) {
                switch (golfer & 3) {
                case 0: strcat(g_printBuffer, "I brought my A game today."); break;
                case 1: strcat(g_printBuffer, "Might as well just give me the trophy."); break;
                case 2: strcat(g_printBuffer, "I hope my caddie counted my clubs."); break;
                case 3: strcat(g_printBuffer, "You losers might as well go home."); break;
                }
            } else {
                switch (golfer & 3) {
                case 0: strcat(g_printBuffer, "It's good to be back on the course."); break;
                case 1: strcat(g_printBuffer, "Wasn't that a great tournament, PARTNER?"); break;
                case 2: strcat(g_printBuffer, "I've already got tickets for the next tournament."); break;
                case 3: strcat(g_printBuffer, "I think I could have won that tournament."); break;
                }
            }
            break;
        case 45: strcat(g_printBuffer, "Hmmm, a tricky uphill shot."); break;
        case 46:
            g_remarkColourValue = 0x800023e8;
            strcat(g_printBuffer, "OK, a nice downhill shot.");
            break;
        case 47: {
            switch (g_golferPool[golfer].typeIdx & 3) {
            case 0:
                s = "PARTNER, your attitude stinks.";
                strcat(g_printBuffer, s);
                break;
            case 1:
                s = "Stop whining and play, PARTNER.";
                strcat(g_printBuffer, s);
                break;
            case 2:
                s = "Talk to the hand, PARTNER.";
                strcat(g_printBuffer, s);
                break;
            case 3:
                s = "Quiet PARTNER, I'm concentrating.";
                strcat(g_printBuffer, s);
                break;
            }
            g_remarkColourValue = 0x80007d08;
            break;
        }
        case 4:
            if (g_golferPool[golfer].rapport == 4) {
                strcat(g_printBuffer, "Only you could miss this shot ");
                AppendGolferName(golfer, 0);
                g_gameSpeakerIsPartner = 1;
                g_remarkColourValue = 0x80007d08;
            } else {
                strcat(g_printBuffer, "This shot looks pretty easy.");
                g_remarkColourValue = 0x80007d08;
            }
            break;
        case 31:
            switch (g_golferPool[golfer].typeIdx & 3) {
            case 1: strcat(g_printBuffer, "I see a ton of DATA."); break;
            case 3: strcat(g_printBuffer, "Could there be any more DATA?"); break;
            case 2: strcat(g_printBuffer, "Look at all the DATA."); break;
            case 0: strcat(g_printBuffer, "I've never seen such DATA."); break;
            }
            break;
        case 32:
            g_remarkColourValue = 0x800023e8;
            strcat(g_printBuffer, "Should I go long or play it safe?");
            break;
        case 33:
            g_remarkColourValue = 0x800023e8;
            strcat(g_printBuffer, "Hmmm, should I go left or right?");
            break;
        case 5:
        case 37:
        case 38: {
            switch (g_golferPool[golfer].rapport) {
            case 0:
            case 4:
                switch (msgId) {
                case 5: strcat(g_printBuffer, "Eeek, I gotta stay away from the "); break;
                case 37: strcat(g_printBuffer, "Eeek, I gotta hit it past the "); break;
                case 38: strcat(g_printBuffer, "Eeek, I gotta watch out for the "); break;
                }
                strcat(g_printBuffer, "DATA");
                break;
            case 3:
            case 5:
                strcat(g_printBuffer, "Watch out for the DATA");
                AppendGolferName(golfer, 1);
                g_gameSpeakerIsPartner = 1;
                break;
            case 1:
                strcat(g_printBuffer, "Careful of the DATA");
                GetPetNameSuffix(golfer);
                g_gameSpeakerIsPartner = 1;
                break;
            case 2:
                if (golfer & 1) {
                    strcat(g_printBuffer, "You're going into the DATA");
                } else {
                    strcat(g_printBuffer, "Say hello to Mr. DATA");
                    strcat(g_printBuffer, " for me");
                }
                strcat(g_printBuffer, " ");
                AppendGolferName(golfer, 0);
                g_gameSpeakerIsPartner = 1;
                break;
            }
            strcat(g_printBuffer, ".");
            break;
        }
        case 65:
            strcat(g_printBuffer, "I didn't see where that shot landed.");
            if (g_level > 1)
                g_remarkColourValue = 0x80007d08;
            break;
        case 1: {
            typeBits = (g_golferPool[golfer].typeIdx & 3) << 5;
            if (g_golferPool[golfer].rapport & 1) {
                strcat(g_printBuffer, "MYNAME");
                if (g_golferPool[g_golferPool[golfer].partner].typeIdx & 1) {
                    {
                        int m = IsGolferMale(g_golferPool[golfer].partner);
                        strcat(g_printBuffer, !m ? ", you truly rock." : ", what a great shot.");
                    }
                } else {
                    if (!IsGolferMale(g_golferPool[golfer].partner))
                        strcat(g_printBuffer, ", you are soooo GOOD!");
                    else
                        strcat(g_printBuffer, ", I couldn't do it any better.");
                }
                g_gameSpeakerIsPartner = 1;
                g_remarkColourValue = 0x800023e8;
                break;
            }
            if (typeBits & 0x20) {
                s = "Oh yeah, I rock.";
                if (!(typeBits & 0x40))
                    s = "Whoa, I am so GOOD!";
            } else if (!(typeBits & 0x40) && golfer != -1) {
                strcat(g_printBuffer, "How'd you like that shot, ");
                s = "PARTNER!";
            } else {
                s = "Oh yeah, nothin' but DATA!";
            }
            strcat(g_printBuffer, s);
            g_remarkColourValue = 0x800023e8;
            break;
        }
        case 2: {
            switch (g_golferPool[golfer].rapport) {
            case 2:
                strcat(g_printBuffer, "Hey ");
                AppendGolferName(golfer, 0);
                strcat(g_printBuffer, ", you're DATA");
                g_gameSpeakerIsPartner = 1;
                break;
            case 4:
                strcat(g_printBuffer, "Oh darn, you're DATA ");
                GetPetNameSuffix(golfer);
                strcat(g_printBuffer, "!");
                g_gameSpeakerIsPartner = 1;
                break;
            default: strcat(g_printBuffer, "Darn, I'm DATA!"); break;
            }
            g_remarkColourValue = 0x80007d08;
            break;
        }
        case 3: {
            switch (g_golferPool[golfer].rapport) {
            case 2:
                strcat(g_printBuffer, "You look even better DATA");
                strcat(g_printBuffer, " ");
                AppendGolferName(golfer, 0);
                strcat(g_printBuffer, ".");
                g_gameSpeakerIsPartner = 1;
                break;
            case 3:
                strcat(g_printBuffer, "My ");
                AppendGolferName(golfer, 0);
                strcat(g_printBuffer, ", now you're DATA?");
                g_gameSpeakerIsPartner = 1;
                break;
            case 4:
                strcat(g_printBuffer, "You prefer being DATA, do you?");
                g_gameSpeakerIsPartner = 1;
                break;
            default:
                switch (g_golferPool[golfer].typeIdx & 3) {
                case 0: strcat(g_printBuffer, "Great"); break;
                case 1: strcat(g_printBuffer, "Oh no"); break;
                case 2: strcat(g_printBuffer, "Argh"); break;
                case 3: strcat(g_printBuffer, "Lovely"); break;
                }
                strcat(g_printBuffer, ", now I'm DATA.");
                break;
            }
            g_remarkColourValue = 0x80007d08;
            break;
        }
        case 8:
            g_remarkColourValue = 0x80007d08;
            strcat(g_printBuffer, "Who designed this !*?(+ course?");
            break;
        case 6:
            g_remarkColourValue = 0x800023e8;
            strcat(g_printBuffer, "I'll use the slope on this shot.");
            break;
        case 9:
            g_remarkColourValue = 0x80007d08;
            strcat(g_printBuffer, "Hey, that ball almost hit me!");
            break;
        case 10:
            strcat(g_printBuffer, "Must I walk through ");
            if (g_squareTypeTable[actionArg].name[strlen(g_squareTypeTable[actionArg].name) - 1] != 's')
                strcat(g_printBuffer, "this ");
            else
                strcat(g_printBuffer, "these ");
            g_remarkColourValue = 0x80007d08;
            strcat(g_printBuffer, g_squareTypeTable[actionArg].name);
            strcat(g_printBuffer, "?");
            break;
        case 43:
            g_remarkColourValue = 0x80007d08;
            strcat(g_printBuffer, "Must I walk up this steep slope?");
            break;
        case 20:
            g_remarkColourValue = 0x80007d08;
            strcat(g_printBuffer, "Gee what an ugly DATA");
            strcat(g_printBuffer, ".");
            break;
        case 24:
            g_remarkColourValue = 0x80007d08;
            strcat(g_printBuffer, "Yuck, there's something growing here!");
            break;
        case 21:
            g_remarkColourValue = 0x80007d08;
            strcat(g_printBuffer, "I'm tired of waiting for these bozos.");
            break;
        case 22:
            strcat(g_printBuffer, "Hey that's ");
            g_remarkColourValue = 0x800023e8;
            strcat(g_printBuffer, g_themeCelebsEntries[actionArg].name);
            strcat(g_printBuffer, "'s house!");
            break;
        case 56: strcat(g_printBuffer, "I'll fade this shot from left to right."); break;
        case 55: strcat(g_printBuffer, "I'll draw this shot from right to left."); break;
        case 57: strcat(g_printBuffer, "I'll bring this ball in high and soft."); break;
        case 60:
            strcat(g_printBuffer, "I'll run this ball low");
            if (g_squareTypeTable[actionArg].category == CATEGORY_TREES) {
                strcat(g_printBuffer, " under the ");
                strcat(g_printBuffer, g_squareTypeTable[actionArg].name);
            }
            strcat(g_printBuffer, ".");
            break;
        case 29:
            g_remarkColourValue = 0x800023e8;
            strcat(g_printBuffer, "I like the variety on this course.");
            break;
        case 30: {
            if ((g_allHolesArray[hole].flags & HOLE_DOGLEG_LEFT)
                && (g_allHolesArray[hole - 1].flags & HOLE_DOGLEG_LEFT)) {
                s = "Great, another dogleg left.";
            } else if ((g_allHolesArray[hole].flags & HOLE_DOGLEG_RIGHT)
                       && (g_allHolesArray[hole - 1].flags & HOLE_DOGLEG_RIGHT)) {
                s = "Yawn, another dogleg right.";
            } else if (g_allHolesArray[hole].par == g_allHolesArray[hole - 1].par) {
                strcat(g_printBuffer, "Wow, yet another par ");
                strcat(g_printBuffer, itoa(g_allHolesArray[hole].par, g_dialogueNum, 10));
                s = ".";
            } else {
                s = "This hole is like the last hole.";
            }
            strcat(g_printBuffer, s);
            g_remarkColourValue = 0x80007d08;
            break;
        }
        case 58:
            switch (actionArg) {
            case 0: strcat(g_printBuffer, "Hey, I'm trying to play golf here."); break;
            case 1: strcat(g_printBuffer, "Ummm, fine thanks."); break;
            case 2: strcat(g_printBuffer, "Lousy, now that you mention it."); break;
            }
            break;
        case 61:
            switch (actionArg) {
            case 3:
                if (golfer & 1)
                    strcat(g_printBuffer, "How are you today, PARTNER?");
                else
                    strcat(g_printBuffer, "Fine.");
                break;
            case 4:
                if (golfer & 1)
                    strcat(g_printBuffer, "Are those new clubs?");
                else
                    strcat(g_printBuffer, "They belong to my brother-in-law.");
                break;
            case 5:
                if (golfer & 1)
                    strcat(g_printBuffer, "Looks like a great day for golf.");
                else
                    strcat(g_printBuffer, "Sure does!");
                break;
            case 6:
                if (!(golfer & 1))
                    strcat(g_printBuffer, "It doesn't get any better than this!");
                else
                    strcat(g_printBuffer, "Listen to those birds singing.");
                break;
            case 7:
                if (golfer & 1)
                    strcat(g_printBuffer, "Prepare to be amazed.");
                else
                    strcat(g_printBuffer, "It's magic time!");
                break;
            default:
                if (golfer & 1)
                    strcat(g_printBuffer, "Shall I tee off first?");
                else
                    strcat(g_printBuffer, "I don't care.");
                break;
            }
            break;
        case 59: {
            int ty = g_golferPool[golfer].typeIdx;
            if (g_golferHistoryPerCharacter[ty].perHoleFlags[hole]) {
                strcat(g_printBuffer, "I've always liked this hole.");
                g_remarkColourValue = 0x800023e8;
                break;
            }
            holeFlags = g_allHolesArray[hole].flags;
            if (holeFlags & HOLE_TOP_100) {
                if (holeFlags & HOLE_TOP_18) {
                    s = "This hole is rated in the top 18!";
                    strcat(g_printBuffer, s);
                    break;
                }
                strcat(g_printBuffer, "You know ");
                AppendGolferName(golfer ^ 1, 0);
                strcat(g_printBuffer, ", ");
                if (g_golferPool[golfer].typeIdx & 1) {
                    AppendHoleName(hole);
                    s = " is a top 100 hole.";
                    strcat(g_printBuffer, s);
                    break;
                }
                strcat(g_printBuffer, "they call this hole ");
                AppendHoleName(hole);
                s = ".";
                strcat(g_printBuffer, s);
                break;
            }
            strcat(g_printBuffer, "I had a ");
            strcat(g_printBuffer, itoa(g_golferHistoryPerCharacter[ty].perHoleStrokes[hole], g_dialogueNum, 10));
            s = " on this hole last time.";
            strcat(g_printBuffer, s);
            break;
        }
        case 34: {
            switch (actionArg) {
            case 0: strcat(g_printBuffer, "Great, thanks for asking."); break;
            case 1: strcat(g_printBuffer, "Good, thanks for noticing."); break;
            case 2: strcat(g_printBuffer, "Thanks for the encouragement."); break;
            }
            g_remarkColourValue = 0x800023e8;
            break;
        }
        case 35: {
            switch (((g_clock + golfer * 45) / 80) & 3) {
            case 0: strcat(g_printBuffer, "I will never play this game again!"); break;
            case 1: strcat(g_printBuffer, "I hate my clubs, I hate my hat, I hate my life."); break;
            case 2: strcat(g_printBuffer, "I am so mad I could just scream."); break;
            case 3: strcat(g_printBuffer, "Come on, you want a piece of me?"); break;
            }
            g_remarkColourValue = 0x80007d08;
            break;
        }
        case 36:
            if (actionArg)
                strcat(g_printBuffer, "That guy's gone crazy.");
            else
                strcat(g_printBuffer, "Whoa, she's flipped out.");
            g_remarkColourValue = 0x80007d08;
            break;
        case 40: {
            switch (g_golferPool[golfer].typeIdx % 8) {
            case 0:
                s = "Oh mama.";
                strcat(g_printBuffer, s);
                break;
            case 1:
                s = "Ohh baby.";
                strcat(g_printBuffer, s);
                break;
            case 2:
                s = "Yes!";
                strcat(g_printBuffer, s);
                break;
            case 3:
                s = "Gadzooks.";
                strcat(g_printBuffer, s);
                break;
            case 4:
                s = "Oh yeah.";
                strcat(g_printBuffer, s);
                break;
            case 5:
                s = "A smoker!";
                strcat(g_printBuffer, s);
                break;
            case 6:
                s = "A screamer!";
                strcat(g_printBuffer, s);
                break;
            case 7:
                s = "Nailed it.";
                strcat(g_printBuffer, s);
                break;
            }
            if (actionArg > 0) {
                strcat(g_printBuffer, " (");
                GetSceneryName(0, 0, actionArg);
                strcat(g_printBuffer, ")");
            }
            break;
        }
        case 42:
            strcat(g_printBuffer, "Lucky bounce, ");
            AppendGolferName(golfer, 0);
            g_gameSpeakerIsPartner = 1;
            break;
        case 44:
            strcat(g_printBuffer, "Never mind. (nice ");
            GetSceneryName(0, 0, actionArg);
            g_remarkColourValue = 0x800023e8;
            strcat(g_printBuffer, ")");
            break;
        case 41:
            if (golfer & 1)
                strcat(g_printBuffer, "Now I'll hit it straight.");
            else
                strcat(g_printBuffer, "Clean and shiny!");
            break;
        case 39:
            if (IsGolferMale(golfer))
                strcat(g_printBuffer, "I guess I scared that little DATA.");
            else
                strcat(g_printBuffer, "I think I frightened that poor DATA.");
            g_remarkColourValue = 0x800023e8;
            break;
        case 7:
            if (actionArg == 0) {
                if (g_golferPool[golfer].flags & 0x10000) {
                    strcpy(g_printBuffer, "I love riding over this scenic bridge.");
                    g_remarkColourValue = 0x800023e8;
                    break;
                }
                strcpy(g_printBuffer, "I love walking over this scenic bridge.");
                g_remarkColourValue = 0x800023e8;
                break;
            }
            strcpy(g_printBuffer, "I think I've seen this bridge before.");
            break;
        case 28: {
            k = g_golferPool[golfer].rapport;
            switch (k) {
            case 1:
            case 3:
            case 5:
                strcat(g_printBuffer, "Look at that lovely DATA");
                if (k == 1)
                    GetPetNameSuffix(g_golferPool[golfer].partner);
                else
                    AppendGolferName(g_golferPool[golfer].partner, 1);
                strcat(g_printBuffer, ".");
                break;
            default:
                if (IsGolferMale(golfer)) {
                    if (g_golferPool[golfer].typeIdx & 1)
                        strcat(g_printBuffer, "Gee, that's a cool DATA.");
                    else
                        strcat(g_printBuffer, "I am liking that DATA.");
                    break;
                }
                if (g_golferPool[golfer].typeIdx & 1)
                    strcat(g_printBuffer, "Oh, what a nice DATA.");
                else
                    strcat(g_printBuffer, "My, that's a lovely DATA.");
                break;
            }
            g_remarkColourValue = 0x800023e8;
            break;
        }
        case 11: {
            k = g_golferPool[golfer].rapport;
            if (k != 1)
                strcat(g_printBuffer, "Check out this ");
            else
                strcat(g_printBuffer, "Look at this ");
            strcat(g_printBuffer, actionArg & 0x100 ? "nice " : "beautiful ");
            strcat(g_printBuffer, "DATA");
            if (k != 1)
                strcat(g_printBuffer, "!");
            else
                GetPetNameSuffix(g_golferPool[golfer].partner);
            g_remarkColourValue = 0x800023e8;
            break;
        }
        case 12:
            g_remarkColourValue = 0x80007d08;
            strcat(g_printBuffer, "Darn that stupid DATA.");
            break;
        case 13: {
            k = g_golferPool[golfer].rapport;
            if (k == 2 || k == 4) {
                strcat(g_printBuffer, "Did I hear a splash ");
                AppendGolferName(golfer, 0);
                strcat(g_printBuffer, "?");
                g_gameSpeakerIsPartner = 1;
            } else {
                switch (g_golferPool[golfer].typeIdx % 3) {
                case 0: strcat(g_printBuffer, "Argh, I drowned it!"); break;
                case 1: strcat(g_printBuffer, "Did my ball go for a swim?"); break;
                case 2: strcat(g_printBuffer, "Nooooo, I'm in the drink!"); break;
                }
            }
            g_remarkColourValue = 0x80007d08;
            break;
        }
        case 14:
            g_remarkColourValue = 0x80007d08;
            strcat(g_printBuffer, "I'm getting a little thirsty.");
            break;
        case 15:
            g_remarkColourValue = 0x80007d08;
            strcat(g_printBuffer, "I'm starting to get hungry.");
            break;
        case 26: {
            k = g_golferPool[golfer].rapport;
            if (k & 1) {
                strcat(g_printBuffer, "You look a bit tired");
                if (k == 1)
                    GetPetNameSuffix(golfer);
                else
                    AppendGolferName(g_golferPool[golfer].partner, 1);
                strcat(g_printBuffer, ".");
                g_gameSpeakerIsPartner = 1;
                break;
            }
            s = "I'm starting to get tired.";
            strcat(g_printBuffer, s);
            break;
        }
        case 16: strcat(g_printBuffer, "Gosh, I kinda hooked that shot!"); break;
        case 17: strcat(g_printBuffer, "Oops, I really sliced that shot!"); break;
        case 18:
            g_remarkColourValue = 0x800023e8;
            strcat(g_printBuffer, "There's nothing like a good snack.");
            break;
        case 25:
            g_remarkColourValue = 0x800023e8;
            strcat(g_printBuffer, "Ahhh, a cool foamy beverage.");
            break;
        case 27:
            g_remarkColourValue = 0x800023e8;
            strcat(g_printBuffer, "This bench is so comfortable.");
            break;
        case 51:
            if ((golfer + g_clock + golfer * 4) & 8) {
                switch (g_buildingLevelPerKind[SCENERY_DRIVING_RANGE]) {
                case 1: strcat(g_printBuffer, "This new driving range has"); break;
                case LEVEL_UPGRADED: strcat(g_printBuffer, "This upgraded driving range has"); break;
                case LEVEL_DELUXE: strcat(g_printBuffer, "Our deluxe driving range has"); break;
                }
            } else {
                strcat(g_printBuffer, "really helped my distance.");
            }
            break;
        case 53:
            if ((golfer + g_clock + golfer * 4) & 8) {
                switch (g_buildingLevelPerKind[SCENERY_PUTTING_GREEN]) {
                case 1: strcat(g_printBuffer, "This new putting green has"); break;
                case LEVEL_UPGRADED: strcat(g_printBuffer, "This upgraded putting green has"); break;
                case LEVEL_DELUXE: strcat(g_printBuffer, "Our deluxe putting green has"); break;
                }
            } else {
                strcat(g_printBuffer, "really improved my putting.");
            }
            break;
        case 54:
            strcat(g_printBuffer, "I'll use my ");
            AppendClubName(actionArg);
            strcat(g_printBuffer, " for this shot.");
            if (g_level <= 1)
                g_remarkColourValue = 0x800023e8;
            break;
        case 52:
            if ((golfer + g_clock + golfer * 4) & 8) {
                switch (g_buildingLevelPerKind[SCENERY_PRO_SHOP]) {
                case 1: strcat(g_printBuffer, "This new pro shop has"); break;
                case LEVEL_UPGRADED: strcat(g_printBuffer, "This upgraded pro shop has"); break;
                case LEVEL_DELUXE: strcat(g_printBuffer, "Our deluxe pro shop has"); break;
                }
            } else {
                strcat(g_printBuffer, "really improved my accuracy.");
            }
            break;
        case 48: strcat(g_printBuffer, "So what do you do, PARTNER?"); break;
        case 49:
            strcat(g_printBuffer, "I'm a ");
            strcat(g_printBuffer, g_golferTypeArray[g_golferPool[golfer].typeIdx].title);
            strcat(g_printBuffer, ".");
            break;
        case 50:
            g_printBuffer[0] = 0;
            k = g_golferPool[golfer].storyId;
            AppendThemeText(k, actionArg & 15, actionArg >> 4, (g_golferPool[golfer].flags >> 20) & 1);
            break;
        case 19:
        case 23: {
            mine = g_golferPool[golfer].holeScore[hole];
            diff = mine - g_allHolesArray[hole].par;
            switch (diff + 4) {
            case 0: strcat(g_printBuffer, "Triple Eagle. "); break;
            case 1: strcat(g_printBuffer, "Double Eagle. "); break;
            case 2: strcat(g_printBuffer, "Eagle. "); break;
            case 3: strcat(g_printBuffer, "Birdie. "); break;
            case 4: strcat(g_printBuffer, "Par. "); break;
            case 5: strcat(g_printBuffer, "Bogey. "); break;
            case 6: strcat(g_printBuffer, "Double Bogey. "); break;
            case 7: strcat(g_printBuffer, "Triple Bogey. "); break;
            default: strcat(g_printBuffer, "Arggg. "); break;
            }
            if (g_allHolesArray[hole].flags & 4) {
                if (diff > 1 || golfer == SCRATCH_GOLFER) {
                    strcat(g_printBuffer, "This hole is too hard!");
                    g_remarkColourValue = 0x80007d08;
                    break;
                }
            }
            if (g_allHolesArray[hole].flags & 8) {
                if (diff < 0 || golfer == SCRATCH_GOLFER) {
                    strcat(g_printBuffer, "This hole is too easy.");
                    g_remarkColourValue = 0x80007d08;
                    break;
                }
            }
            if (msgId == 23) {
                strcat(g_printBuffer, "This hole is too hard/easy.");
                g_remarkColourValue = 0x80007d08;
                break;
            }
            mask = 0;
            switch ((golfer + hole) & 3) {
            case 0:
                if (g_golferPool[golfer].talents & TALENT_LENGTH)
                    mask = 0x100;
                break;
            case 1:
                if (g_golferPool[golfer].talents & TALENT_ACCURACY)
                    mask = 0x200;
                break;
            case 2:
                if (g_golferPool[golfer].talents & TALENT_IMAGINATION)
                    mask = 0x400;
                break;
            }
            if ((mask & g_allHolesArray[hole].flags) && diff <= 0) {
                switch (mask) {
                case 0x100:
                    s = "My length skill helps on this hole.";
                    strcat(g_printBuffer, s);
                    break;
                case 0x200:
                    s = "This hole rewards my accuracy.";
                    strcat(g_printBuffer, s);
                    break;
                case 0x400:
                    s = "I had to use some imaginative shots.";
                    strcat(g_printBuffer, s);
                    break;
                }
                break;
            }
            if (g_golferPool[golfer].rapport == 2 || g_golferPool[golfer].rapport == 4) {
                his = g_golferPool[g_golferPool[golfer].partner].holeScore[hole];
                if (!his)
                    goto c19rating;
                s = "take that loser!";
                if (mine >= his)
                    s = "you were lucky on this hole.";
                strcat(g_printBuffer, s);
                break;
            }
c19rating:
            AppendPartnerNote(actionArg, diff);
            break;
        }
        }
    }
    if (actionArg == -1)
        return;
    strcpy(incoming, g_printBuffer);
    g_printBuffer[0] = 0;
    AppendGolferName(golfer, 0);
    strcpy(ownName, g_printBuffer);
    g_printBuffer[0] = 0;
    AppendGolferName(golfer ^ 1, 0);
    strcpy(partnerName, g_printBuffer);
    g_printBuffer[0] = 0;
    if (golfer > 0x98)
        strcat(g_printBuffer, "DATA(");
    switch (msgId) {
    case 2:
    case 3: {
        int tv = actionArg;
        switch (g_squareTypeTable[tv].category) {
        case CATEGORY_TREES: strcat(g_printBuffer, "under the "); break;
        case CATEGORY_OUT_OF_BOUNDS: strcat(g_printBuffer, "in the "); break;
        default: strcat(g_printBuffer, "in the "); break;
        }
        strcat(g_printBuffer, g_squareTypeTable[tv].name);
        break;
    }
    case 11:
    case 20:
    case 28: {
        int tv = actionArg;
        GetSceneryName(tv % 50, tv / 50, -1);
        break;
    }
    case 12: {
        int tv = actionArg;
        strcat(g_printBuffer, g_squareTypeTable[tv].name);
        break;
    }
    case 39: {
        int tv = actionArg;
        strcat(g_printBuffer, g_animalKinds[tv].name);
        break;
    }
    default: {
        int tv = actionArg;
        s = &g_squareTypeTable[tv].name[16];
        if (g_squareTypeTable[tv].category != CATEGORY_TREES)
            s = g_squareTypeTable[tv].name;
        strcat(g_printBuffer, s);
        break;
    }
    }
    if (golfer > 0x98)
        strcat(g_printBuffer, ")");
    strcpy(dataName, g_printBuffer);
    strcpy(g_printBuffer, incoming);
    StringReplace("MYNAME", ownName);
    StringReplace("PARTNER", partnerName);
    StringReplace("DATA", dataName);
}

int IsGolferMale(int golfer)
{
    return ((unsigned)~g_golferTypeArray[g_golferPool[golfer].typeIdx].ageBits >> 7) & 1;
}

char g_dialogueNum[12];
int g_actionArgs[65] = {0};
