// Minimal GL/glx.h for GLFW compilation on systems without libgl-dev.
// Provides the GLX type definitions and function-pointer typedefs that GLFW's
// glx_context.c requires for compilation. GLFW loads all GLX functions
// dynamically (dlsym), so only types/typedefs are needed here — no linked symbols.
#ifndef __glx_h__
#define __glx_h__

#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include "glcorearb.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef XID GLXContextID;
typedef XID GLXDrawable;
typedef struct __GLXcontextRec *GLXContext;
typedef struct __GLXFBConfigRec *GLXFBConfig;
typedef XID GLXWindow;
typedef XID GLXPbuffer;
typedef void (*__GLXextproc)(void);

/* GLX 1.0 function pointer types */
typedef GLXContext (*glXCreateContextProc)(Display*, XVisualInfo*, GLXContext, Bool);
typedef void (*glXDestroyContextProc)(Display*, GLXContext);
typedef Bool (*glXMakeCurrentProc)(Display*, GLXDrawable, GLXContext);
typedef void (*glXSwapBuffersProc)(Display*, GLXDrawable);
typedef GLXContext (*glXGetCurrentContextProc)(void);
typedef GLXDrawable (*glXGetCurrentDrawableProc)(void);
typedef Bool (*glXQueryExtensionProc)(Display*, int*, int*);
typedef Bool (*glXQueryVersionProc)(Display*, int*, int*);
typedef int (*glXGetConfigProc)(Display*, XVisualInfo*, int, int*);
typedef XVisualInfo* (*glXChooseVisualProc)(Display*, int, int*);
typedef void (*glXCopyContextProc)(Display*, GLXContext, GLXContext, unsigned long);
typedef GLXPbuffer (*glXCreateGLXPbufferProc)(Display*, GLXFBConfig, const int*);
typedef void (*glXDestroyGLXPbufferProc)(Display*, GLXPbuffer);

/* GLX 1.3+ function pointer types */
typedef GLXFBConfig* (*glXGetFBConfigsProc)(Display*, int, int*);
typedef int (*glXGetFBConfigAttribProc)(Display*, GLXFBConfig, int, int*);
typedef XVisualInfo* (*glXGetVisualFromFBConfigProc)(Display*, GLXFBConfig);
typedef GLXWindow (*glXCreateWindowProc)(Display*, GLXFBConfig, Window, const int*);
typedef void (*glXDestroyWindowProc)(Display*, GLXWindow);
typedef GLXContext (*glXCreateNewContextProc)(Display*, GLXFBConfig, int, GLXContext, Bool);
typedef const char* (*glXGetClientStringProc)(Display*, int);
typedef const char* (*glXQueryExtensionsStringProc)(Display*, int);

/* GLX extension function pointer types */
typedef __GLXextproc (*glXGetProcAddressProc)(const char*);
typedef __GLXextproc (*glXGetProcAddressARBProc)(const char*);
typedef GLXContext (*glXCreateContextAttribsARBProc)(Display*, GLXFBConfig, GLXContext, Bool, const int*);
typedef int (*glXSwapIntervalSGIProc)(int);
typedef int (*glXSwapIntervalMESAPROC)(int);
typedef int (*glXSwapIntervalEXTPROC)(Display*, GLXDrawable, int);

/* GLX constants (subset used by GLFW) */
#define GLX_USE_GL              1
#define GLX_BUFFER_SIZE         2
#define GLX_LEVEL               3
#define GLX_RGBA                4
#define GLX_DOUBLEBUFFER        5
#define GLX_STEREO              6
#define GLX_AUX_BUFFERS         7
#define GLX_RED_SIZE            8
#define GLX_GREEN_SIZE          9
#define GLX_BLUE_SIZE           10
#define GLX_ALPHA_SIZE          11
#define GLX_DEPTH_SIZE          12
#define GLX_STENCIL_SIZE        13
#define GLX_ACCUM_RED_SIZE      14
#define GLX_ACCUM_GREEN_SIZE    15
#define GLX_ACCUM_BLUE_SIZE     16
#define GLX_ACCUM_ALPHA_SIZE    17
#define GLX_SAMPLES             0x186A1
#define GLX_SAMPLE_BUFFERS      0x186A0
#define GLX_X_VISUAL_TYPE       0x22
#define GLX_CONFIG_CAVEAT       0x20
#define GLX_TRANSPARENT_TYPE    0x23
#define GLX_VISUAL_ID           0x800B
#define GLX_DRAWABLE_TYPE       0x8010
#define GLX_RENDER_TYPE         0x8011
#define GLX_X_RENDERABLE        0x8012
#define GLX_FBCONFIG_ID         0x8013
#define GLX_WINDOW_BIT          0x00000001
#define GLX_PIXMAP_BIT          0x00000002
#define GLX_RGBA_BIT            0x00000001
#define GLX_COLOR_INDEX_BIT     0x00000002
#define GLX_NONE                0x8000
#define GLX_FRONT_LEFT_BUFFER_BIT  0x00000001
#define GLX_RGBA_TYPE           0x8014
#define GLX_COLOR_INDEX_TYPE    0x8015
#define GLX_CONTEXT_MAJOR_VERSION_ARB  0x2091
#define GLX_CONTEXT_MINOR_VERSION_ARB  0x2092
#define GLX_CONTEXT_FLAGS_ARB          0x2094
#define GLX_CONTEXT_PROFILE_MASK_ARB   0x9126
#define GLX_CONTEXT_DEBUG_BIT_ARB      0x0001
#define GLX_CONTEXT_FORWARD_COMPATIBLE_BIT_ARB 0x0002
#define GLX_CONTEXT_CORE_PROFILE_BIT_ARB       0x00000001
#define GLX_CONTEXT_COMPATIBILITY_PROFILE_BIT_ARB 0x00000002
#define GLXBadFBConfig 0  /* GLFW only uses the name */
#define GLX_VENDOR              0x1
#define GLX_VERSION             0x2
#define GLX_EXTENSIONS          0x3

#ifdef __cplusplus
}
#endif

#endif /* __glx_h__ */
