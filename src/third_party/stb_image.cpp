#include <tinygl/core/gl_defs.h>
// Export stb_image symbols from the tinygl_framework DLL so they can be
// accessed by static test utilities or other external modules.
#define STBIDEF TINYGL_API
#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"