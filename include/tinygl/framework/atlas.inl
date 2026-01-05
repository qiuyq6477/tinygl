#pragma once
#include <tinygl/third_party/microui.h>

#ifdef __cplusplus
extern "C" {
#endif

enum { ATLAS_WHITE = MU_ICON_MAX, ATLAS_FONT };
enum { ATLAS_WIDTH = 128, ATLAS_HEIGHT = 128 };

extern unsigned char atlas_texture[ATLAS_WIDTH * ATLAS_HEIGHT];
extern mu_Rect atlas[]; // Assuming size sufficient for char range, actually it uses ATLAS_FONT + 127 roughly. 
// Using a safe upper bound or incomplete type if feasible. 
// atlas.c uses sparse init, max index is roughly ATLAS_FONT + 127 ~ 200. 
// Let's use incomplete type or large enough. 
// In C++, extern mu_Rect atlas[]; works.

#ifdef __cplusplus
}
#endif