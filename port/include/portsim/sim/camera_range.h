// P31A: the camera's legal range (mainLoop.cameraBoundByZoom). Its own header so that files that cannot include bridge.h (its
// helpers collide with local ones, terrain_course.cpp) can include it alone; bridge.h includes it.
#pragma once
#include <cstdint>

namespace portsim {

class Ctx;

// P31A (decision G33 item 1, Mariano's answer "Limit by zoom"; log/roadmap/P31/P31A_notes.md). `play` switch
// mainLoop.cameraBoundByZoom, 1 = the original: every clamp of the camera tile pair gCameraTileX / gCameraTileY is 0..0x31 on both
// axes (H1's arrow and edge-pan cases), which keeps the view's CENTRE on the grid, so at the map's edge up to half the view is black.
// At 0 the legal range of each axis comes from the land and the view: the land is the bounding box of the tiles whose type is not
// 0x14 (the black border the maps carry, 5 tiles wide on Scorpion Cove), and each of the land's four corners must be reachable with
// the view's edge stopping kCameraBoundMargin units (800-space; kCameraBoundBottom at the bottom edge, which the bottom HUD band
// covers) beyond it, for the zoom, the rotation and the view width (view block word +0). The four box margins grow one tile at a
// time, the smallest first, while every corner stays inside the view; an axis on which the land is smaller than the view (zoom 1)
// is pinned to the land's centre. Every camera writer clamps to this range (camera_on_map, the arrow, glide and edge-pan clamps,
// stepCourseEditQueue's goal; the app's arrow glide takes the same range through ArrowGlide::set_range).
extern int g_defect_camera_bound_zoom;
constexpr int32_t kCameraBoundMargin = 16;
// the bottom band's panels start at y 0x1da (the panels' art top: log/roadmap/P27/P27A_notes.md section 10), 600 - 0x1da = 126 units
constexpr int32_t kCameraBoundBottom = 600 - 0x1da + kCameraBoundMargin;
extern int32_t g_camera_bound_margins[4];  // left, top, right, bottom (800-space units); a development override only changes them
struct CameraRange {
  int32_t lo[2] = {0, 0}, hi[2] = {0x31, 0x31};  // [0] gCameraTileX, [1] gCameraTileY
};
// the legal range now (at switch 1, or before a land exists: 0..0x31 on both axes)
CameraRange camera_range(Ctx &ctx);

}  // namespace portsim
