// Minimal GL/gl.h for GLFW compilation on systems without libgl-dev.
// GLFW only needs the GL type definitions (GLenum, GLuint, ...) and a few
// constants; it loads all GL functions dynamically via its own loader. We
// delegate the full type/function set to glcorearb.h (Khronos).
#ifndef __GL_H__
#define __GL_H__
#include "glcorearb.h"
#endif /* __GL_H__ */
