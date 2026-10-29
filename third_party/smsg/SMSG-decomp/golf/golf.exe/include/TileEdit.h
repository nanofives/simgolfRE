#ifndef TILEEDIT_H
#define TILEEDIT_H

void SetTileDataAt(int row, int col, int undo, unsigned char refund);
int BuildDemolishTooltipText(int row, int col);
void DemolishTileAtPosition(int row, int col);

#endif
