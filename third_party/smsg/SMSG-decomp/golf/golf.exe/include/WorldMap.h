#ifndef WORLDMAP_H
#define WORLDMAP_H
void TriggerAccomplishment(int id, int worldX, int worldY);
void RenderAccomplishmentsScreen();
void ShowAccomplishmentsScreen();
void InitWorldMapCourseList();
int ShowWorldMapScreen();
int ChooseWorldMapCourse(int mode);
void GenerateCourseMap(int course);
void UnlockLandmarkByGolfer(int golfer, int theme);
int BuildThemeList();
void ShowThemeSelect();
int HighScore_InsertEntry();
void RenderHighScoreScreen();
#endif
