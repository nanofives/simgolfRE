#ifndef POLYFILL_H
#define POLYFILL_H

struct AssetSurface;

struct Vert {
    int x;
    int y;
};

struct EdgeScanPoly {
    int dir;
    int rowsLeft;
    int endVertex;
    int x;
    int xStep;
    int xRound;
    int err;
    int errInc;
    int errDec;
};

int setup_edge_poly(EdgeScanPoly *edge, int vertex);
int step_edge_poly(EdgeScanPoly *edge);
void scan_out_line_poly_16(int xLeft, int xRight);
void scan_out_line_poly_8(int xLeft, int xRight);
int FillPolygon8(AssetSurface *surface, Vert *pts, int count, int color);
int FillPolygon16(AssetSurface *surface, Vert *pts, int count, int color);
int FillPolygon(AssetSurface *surface, Vert *pts, int count, int color);

#endif
