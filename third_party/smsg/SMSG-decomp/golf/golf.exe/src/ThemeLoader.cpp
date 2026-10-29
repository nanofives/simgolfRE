#include "jgldefs.h"
#include "ThemeLoader.h"

void LoadThemeFiles(int enable)
{
    LoadThemeData();
    if (enable == 0)
        return;

    strcpy(g_printBuffer, "Themes\\");
    strcat(g_printBuffer, g_activeThemeDir);
    strcat(g_printBuffer, "\\*.chr");
    int n = FindFilesWithFilter(g_printBuffer, g_empty, g_empty);

    for (int i = 0; i < n; i++) {
        g_golferHistoryPerCharacter[i + 1].flags |= 1;
        LoadGolferFromTheme(g_fileList[i], i + 1, -1);
    }
}

#define PRO_LOAD_GOLFER 0x4c

void LoadThemeData()
{
    char line[256];
    FILE *fp;
    int i, j, k, sum, ci, n;

    LoadGolferFromTheme("Joe Pro.glf", PRO_TYPE, -1);
    LoadGolferFromTheme("I.M.Picky.glf", 0x4d, -1);
    LoadGolferFromTheme("Ivana Richman.glf", 0x4e, -1);
    LoadGolferFromTheme("J.P.Bigdome.glf", 0x4f, -1);
    g_runModeFlags &= ~0x10000000;
    memset(g_gameStoryNameTable, 0, sizeof(g_gameStoryNameTable));
    strcpy(g_printBuffer, "Themes\\");
    strcat(g_printBuffer, g_activeThemeDir);
    strcat(g_printBuffer, "\\*.txt");
    if (FindFilesWithFilter(g_printBuffer, g_empty, g_empty) == 0) {
        FindFilesWithFilter("Themes\\Standard\\*.txt", g_empty, g_empty);
        g_runModeFlags |= 0x10000000;
    }
    memcpy(g_gameStoryNameTable, g_fileList, sizeof(g_gameStoryNameTable));
    for (i = 0; i < 100; i++)
        if (strstr(g_fileList[i], "OpeningDay")) {
            g_openingDayStoryId = i;
            break;
        }
    for (i = 0; i < 8; i++)
        g_themeSlots[i].id = -1;
    g_nextThemeSlot = 0;

    strcpy(g_printBuffer, "Themes\\");
    strcat(g_printBuffer, g_activeThemeDir);
    strcat(g_printBuffer, "\\celebrities.dta");
    fp = fopen(g_printBuffer, "rt");
    if (fp == 0 && (fp = fopen("Themes\\Standard\\celebrities.dta", "rt")) == 0) {
        strcat(g_printBuffer, " - file not found.\n");
        ShowListDialogWithParams(g_printBuffer, 100, 50);
    } else {
        for (i = 0; i < 100; i++)
            g_themeCelebsEntries[i].kind = -1;
        ci = 0;
        while (fgets(line, 250, fp)) {
            if (line[0] == '*')
                continue;
            if (strlen(line) < 10)
                continue;
            line[strlen(line) - 1] = 0;
            strcpy(g_printBuffer, strtok(line, ","));
            strcpy(g_themeCelebsEntries[ci].name, g_printBuffer);
            strcpy(g_printBuffer, strtok(0, ","));
            g_themeCelebsEntries[ci].kind = (g_printBuffer[0] - 'A') % 11;
            strcpy(g_printBuffer, strtok(0, ","));
            if (g_printBuffer[0] >= '1' && g_printBuffer[0] <= '3')
                g_themeCelebsEntries[ci].skinTone = g_printBuffer[0] - '0';
            else
                g_themeCelebsEntries[ci].skinTone = 0;
            strcpy(g_printBuffer, strtok(0, ","));
            g_themeCelebsEntries[ci].appearanceColor = (g_printBuffer[0] - '0') % 10;
            strcpy(g_printBuffer, strtok(0, ","));
            g_themeCelebsEntries[ci].shirtColor = (g_printBuffer[0] - '0') % 10;
            strcpy(g_printBuffer, strtok(0, ","));
            g_themeCelebsEntries[ci].pantsColor = (g_printBuffer[0] - '0') % 10;
            ci++;
            if (ci >= 100)
                break;
        }
        fclose(fp);
    }

    strcpy(g_printBuffer, "Themes\\");
    strcat(g_printBuffer, g_activeThemeDir);
    strcat(g_printBuffer, "\\progolfers.dta");
    fp = fopen(g_printBuffer, "rt");
    if (fp == 0)
        fp = fopen("Themes\\Standard\\progolfers.dta", "rt");
    k = 0;
    if (fp == 0) {
        strcat(g_printBuffer, " - file not found.\n");
        ShowListDialogWithParams(g_printBuffer, 100, 50);
    } else {
        for (i = 0; i < 100; i++)
            g_golfProList[i].figure = -1;
        k = 0;
        while (fgets(line, 250, fp)) {
            if (line[0] == '*')
                continue;
            if (strlen(line) < 10)
                continue;
            line[strlen(line) - 1] = 0;
            strcpy(g_printBuffer, strtok(line, ","));
            strcpy(g_golfProList[k].name, g_printBuffer);
            strcpy(g_printBuffer, strtok(0, ","));
            g_golfProList[k].figure = (g_printBuffer[0] - '0') % 8;
            strcpy(g_printBuffer, strtok(0, ","));
            g_golfProList[k].skinTone = (g_printBuffer[0] - '0') % 4;
            strcpy(g_printBuffer, strtok(0, ","));
            g_golfProList[k].appearanceColor = (g_printBuffer[0] - '0') % 10;
            strcpy(g_printBuffer, strtok(0, ","));
            g_golfProList[k].shirtColor = (g_printBuffer[0] - '0') % 10;
            strcpy(g_printBuffer, strtok(0, ","));
            g_golfProList[k].pantsColor = (g_printBuffer[0] - '0') % 10;
            sum = 0;
            strcpy(g_printBuffer, strtok(0, ","));
            for (j = 0; j < 10; j++) {
                int v;
                char c = g_printBuffer[j];
                if (c >= '0' && c <= '9') {
                    g_golfProList[k].ratings[j] = (c - '0') % 10;
                } else {
                    if (c >= 'A' && c <= 'Z')
                        v = c - 'A';
                    else
                        v = c - 'a';
                    g_golfProList[k].ratings[j] = v % 10 + 10;
                }
                sum += g_golfProList[k].ratings[j];
            }
            g_golfProList[k].ratingTotal = sum;
            k++;
            if (k >= 100)
                break;
        }
        fclose(fp);
    }

    strcpy(g_printBuffer, "Themes\\");
    strcat(g_printBuffer, g_activeThemeDir);
    strcat(g_printBuffer, "\\*.pro");
    n = FindFilesWithFilter(g_printBuffer, g_empty, g_empty);
    for (i = 0; i < n; i++) {
        LoadGolferFromTheme(g_fileList[i], PRO_TYPE, PRO_LOAD_GOLFER);
        strcpy(g_golfProList[k].name, g_golferTypeArray[PRO_TYPE].name);
        g_golfProList[k].figure = g_golferTypeArray[PRO_TYPE].appearance >> 4;
        g_golfProList[k].skinTone = g_golferTypeArray[PRO_TYPE].skinTone;
        g_golfProList[k].appearanceColor = g_golferTypeArray[PRO_TYPE].appearance & APPEARANCE_LOW_BITS;
        g_golfProList[k].shirtColor = g_golferTypeArray[PRO_TYPE].shirtColor;
        g_golfProList[k].pantsColor = g_golferTypeArray[PRO_TYPE].pantsColor;
        k++;
        sum = 0;
        for (j = 0; j < 10; j++) {
            g_golfProList[k].ratings[j] = g_golferPool[PRO_LOAD_GOLFER].ratings[j];
            sum += g_golferPool[PRO_LOAD_GOLFER].ratings[j];
        }
        g_golfProList[k].ratingTotal = sum;
    }
}

GolferType g_golferTypeArray[83] = {{"Golf Pro", "Gary Golf", 20, 10, 8, 0, 0, 0, 0, 0, 0, {0, 0, 0}, 0, ""},
    {"Executive", "Jim", 3, 18, 9, 0, 0, 0, 0, 0, 0, {0, 0, 0}, 0, ""},
    {"Attorney", "Clarence", 5, 9, 3, 0, 0, 0, 0, 0, 0, {0, 0, 0}, 0, ""},
    {"Caddie", "Billy", 24, 9, 5, 0, 0, 0, 0, 0, 0, {0, 0, 0}, 0, ""},
    {"Salesman", "Larry", 6, 34, 2, 0, 0, 0, 0, 0, 0, {0, 0, 0}, 0, ""},
    {"Doctor", "Ben", 17, 18, 7, 0, 0, 0, 0, 0, 0, {0, 0, 0}, 0, ""},
    {"Preacher", "Zeke", 18, 18, 0, 0, 0, 0, 0, 0, 0, {0, 0, 0}, 0, ""},
    {"Tycoon", "J.P.", 9, 68, 11, 0, 0, 0, 0, 0, 0, {0, 0, 0}, 0, ""},
    {"Athlete", "Dennis", 12, 17, 17, 0, 0, 0, 0, 0, 0, {0, 0, 0}, 0, ""},
    {"Musician", "Ringo", 10, 9, 1, 0, 0, 0, 0, 0, 0, {0, 0, 0}, 0, ""},
    {"Maitre d'", "Alphonse", 3, 10, 10, 0, 0, 0, 0, 0, 0, {0, 0, 0}, 0, ""},
    {"Decorator", "Pierre", 1, 10, 18, 0, 0, 0, 0, 0, 0, {0, 0, 0}, 0, ""},
    {"Programmer", "Bart", 24, 9, 13, 0, 0, 0, 0, 0, 0, {0, 0, 0}, 0, ""},
    {"Surfer", "Chip", 22, 9, 4, 0, 0, 0, 0, 0, 0, {0, 0, 0}, 0, ""},
    {"Professor", "Albert", 17, 18, 15, 0, 0, 0, 0, 0, 0, {0, 0, 0}, 0, ""},
    {"Guru", "Baba", 3, 20, 16, 0, 0, 0, 0, 0, 0, {0, 0, 0}, 0, ""},
    {"Psychiatrist", "Sigmund", 9, 36, 14, 0, 0, 0, 0, 0, 0, {0, 0, 0}, 0, ""},
    {"Rapper", "Snoop", 10, 9, 12, 0, 0, 0, 0, 0, 0, {0, 0, 0}, 0, ""},
    {"Student", "Dobie", 28, 9, 6, 0, 0, 0, 0, 0, 0, {0, 0, 0}, 0, ""},
    {"Golf Pro", "Jack", 20, 10, 8, 0, 0, 0, 0, 0, 0, {0, 0, 0}, 0, ""},
    {"Executive", "Bob", 1, 20, 9, 0, 0, 0, 0, 0, 0, {0, 0, 0}, 0, ""},
    {"Attorney", "Slick", 4, 10, 3, 0, 0, 0, 0, 0, 0, {0, 0, 0}, 0, ""},
    {"Caddie", "Roberto", 24, 9, 5, 0, 0, 0, 0, 0, 0, {0, 0, 0}, 0, ""},
    {"Salesman", "Sammy", 6, 34, 2, 0, 0, 0, 0, 0, 0, {0, 0, 0}, 0, ""},
    {"Doctor", "Marcus", 17, 18, 7, 0, 0, 0, 0, 0, 0, {0, 0, 0}, 0, ""},
    {"Preacher", "Kingsley", 18, 18, 0, 0, 0, 0, 0, 0, 0, {0, 0, 0}, 0, ""},
    {"Tycoon", "J.W.", 13, 10, 11, 0, 0, 0, 0, 0, 0, {0, 0, 0}, 0, ""},
    {"Athlete", "Kareem", 6, 10, 17, 0, 0, 0, 0, 0, 0, {0, 0, 0}, 0, ""},
    {"Musician", "Dizzy", 10, 10, 1, 0, 0, 0, 0, 0, 0, {0, 0, 0}, 0, ""},
    {"Maitre d'", "Luigi", 3, 18, 10, 0, 0, 0, 0, 0, 0, {0, 0, 0}, 0, ""},
    {"Decorator", "Alain", 1, 10, 18, 0, 0, 0, 0, 0, 0, {0, 0, 0}, 0, ""},
    {"Programmer", "Mark", 24, 9, 13, 0, 0, 0, 0, 0, 0, {0, 0, 0}, 0, ""},
    {"Surfer", "Biff", 6, 9, 4, 0, 0, 0, 0, 0, 0, {0, 0, 0}, 0, ""},
    {"Professor", "Horace", 17, 18, 15, 0, 0, 0, 0, 0, 0, {0, 0, 0}, 0, ""},
    {"Guru", "Maharish", 3, 20, 16, 0, 0, 0, 0, 0, 0, {0, 0, 0}, 0, ""},
    {"Psychiatrist", "Solomon", 3, 36, 14, 0, 0, 0, 0, 0, 0, {0, 0, 0}, 0, ""},
    {"Rapper", "Biggie", 10, 9, 12, 0, 0, 0, 0, 0, 0, {0, 0, 0}, 0, ""},
    {"Student", "Gilbert", 12, 9, 6, 0, 0, 0, 0, 0, 0, {0, 0, 0}, 0, ""},
    {"Golf Pro", "Arnie", 22, 18, 8, 0, 0, 0, 0, 0, 0, {0, 0, 0}, 0, ""},
    {"Executive", "Bill", 3, 18, 9, 0, 0, 0, 0, 0, 0, {0, 0, 0}, 0, ""},
    {"Attorney", "Johnnie", 5, 9, 3, 0, 0, 0, 0, 0, 0, {0, 0, 0}, 0, ""},
    {"Caddie", "Jimmy", 24, 9, 5, 0, 0, 0, 0, 0, 0, {0, 0, 0}, 0, ""},
    {"Salesman", "Leo", 6, 18, 2, 0, 0, 0, 0, 0, 0, {0, 0, 0}, 0, ""},
    {"Doctor", "Denton", 17, 18, 7, 0, 0, 0, 0, 0, 0, {0, 0, 0}, 0, ""},
    {"Preacher", "Joshua", 18, 18, 0, 0, 0, 0, 0, 0, 0, {0, 0, 0}, 0, ""},
    {"Tycoon", "J.R.", 9, 68, 11, 0, 0, 0, 0, 0, 0, {0, 0, 0}, 0, ""},
    {"Athlete", "Tyrone", 12, 17, 17, 0, 0, 0, 0, 0, 0, {0, 0, 0}, 0, ""},
    {"Musician", "Duke", 10, 9, 1, 0, 0, 0, 0, 0, 0, {0, 0, 0}, 0, ""},
    {"Maitre d'", "Michel", 3, 34, 10, 0, 0, 0, 0, 0, 0, {0, 0, 0}, 0, ""},
    {"Decorator", "Gianni", 1, 10, 18, 0, 0, 0, 0, 0, 0, {0, 0, 0}, 0, ""},
    {"Programmer", "Sid", 24, 9, 13, 0, 0, 0, 0, 0, 0, {0, 0, 0}, 0, ""},
    {"Surfer", "Bing", 22, 9, 4, 0, 0, 0, 0, 0, 0, {0, 0, 0}, 0, ""},
    {"Professor", "Boris", 17, 18, 15, 0, 0, 0, 0, 0, 0, {0, 0, 0}, 0, ""},
    {"Guru", "Mahatma", 3, 20, 16, 0, 0, 0, 0, 0, 0, {0, 0, 0}, 0, ""},
    {"Psychiatrist", "Kurt", 9, 36, 14, 0, 0, 0, 0, 0, 0, {0, 0, 0}, 0, ""},
    {"Rapper", "Prince", 10, 9, 12, 0, 0, 0, 0, 0, 0, {0, 0, 0}, 0, ""},
    {"Student", "Freddie", 28, 9, 6, 0, 0, 0, 0, 0, 0, {0, 0, 0}, 0, ""},
    {"Manager", "Mary", 3, -110, 6, 0, 0, 0, 0, 0, 0, {0, 0, 0}, 0, ""},
    {"Lawyer", "Marcia", 5, -95, 14, 0, 0, 0, 0, 0, 0, {0, 0, 0}, 0, ""},
    {"Coed", "Bobbie", 24, -119, 5, 0, 0, 0, 0, 0, 0, {0, 0, 0}, 0, ""},
    {"Nurse", "Florence", 20, -110, 3, 0, 0, 0, 0, 0, 0, {0, 0, 0}, 0, ""},
    {"Volunteer", "Julie", 6, -118, 2, 0, 0, 0, 0, 0, 0, {0, 0, 0}, 0, ""},
    {"Homemaker", "Harriet", 17, -110, 11, 0, 0, 0, 0, 0, 0, {0, 0, 0}, 0, ""},
    {"Teacher", "Trudy", 18, -116, 13, 0, 0, 0, 0, 0, 0, {0, 0, 0}, 0, ""},
    {"Grandmother", "Agnes", 9, -108, 9, 0, 0, 0, 0, 0, 0, {0, 0, 0}, 0, ""},
    {"Dancer", "Twyla", 12, -111, 0, 0, 0, 0, 0, 0, 0, {0, 0, 0}, 0, ""},
    {"Artist", "Georgia", 10, -119, 18, 0, 0, 0, 0, 0, 0, {0, 0, 0}, 0, ""},
    {"Critic", "Pauline", 1, -110, 10, 0, 0, 0, 0, 0, 0, {0, 0, 0}, 0, ""},
    {"Model", "Cindy", 7, -119, 1, 0, 0, 0, 0, 0, 0, {0, 0, 0}, 0, ""},
    {"Newscaster", "Jessica", 16, -110, 7, 0, 0, 0, 0, 0, 0, {0, 0, 0}, 0, ""},
    {"Soldier", "Jane", 6, -119, 8, 0, 0, 0, 0, 0, 0, {0, 0, 0}, 0, ""},
    {"Secretary", "Sally", 17, -119, 12, 0, 0, 0, 0, 0, 0, {0, 0, 0}, 0, ""},
    {"FlightAttendant", "Brenda", 22, -118, 4, 0, 0, 0, 0, 0, 0, {0, 0, 0}, 0, ""},
    {"Detective", "Nancy", 9, -108, 16, 0, 0, 0, 0, 0, 0, {0, 0, 0}, 0, ""},
    {"Gymnast", "Olga", 12, -119, 17, 0, 0, 0, 0, 0, 0, {0, 0, 0}, 0, ""},
    {"Socialite", "Jacqueline", 10, -94, 15, 0, 0, 0, 0, 0, 0, {0, 0, 0}, 0, ""},
    {"Golf Pro", "Gary Golf", 20, 10, 8, 0, 0, 0, 0, 0, 0, {0, 0, 0}, 0, ""},
    {"Commissioner", "I.M. Picky", 3, 18, 2, 0, 0, 0, 0, 0, 0, {0, 0, 0}, 0, ""},
    {"Heiress", "Ivana Richman", 6, -110, 10, 0, 0, 0, 0, 0, 0, {0, 0, 0}, 0, ""},
    {"CEO", "J.P.Bigdome", 5, 18, 11, 0, 0, 0, 0, 0, 0, {0, 0, 0}, 0, ""},
    {"Dummy", "J.Doe", 5, 18, 11, 0, 0, 0, 0, 0, 0, {0, 0, 0}, 0, ""},
    {"Dummy", "J.Doe", 5, 18, 11, 0, 0, 0, 0, 0, 0, {0, 0, 0}, 0, ""},
    {"Dummy", "J.Doe", 5, 18, 11, 0, 0, 0, 0, 0, 0, {0, 0, 0}, 0, ""}};
GolferType g_golferTypeBackup = {"Swapper", "Don't use", 5, 18, 11, 0, 0, 0, 0, 0, 0, {0, 0, 0}, 0, ""};
