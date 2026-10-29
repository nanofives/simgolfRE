#ifndef TOURNAMENT_H
#define TOURNAMENT_H

void InitTournamentGolfers();
void RespawnGolferPairsPerHole();
void EndTournament();
void PrepareTournamentCourse();
int ShowActionDialog(int cx, int top, int sel, int pickOne, int disabled);
int StartTournament();

#endif
