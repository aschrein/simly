// same as sokol.c, but compiled as C++
#define SOKOL_IMPL
/* this is only needed for the debug-inspection headers */
#define SOKOL_TRACE_HOOKS
/* sokol 3D-API defines are provided by build options */
#include "sokol_app.h"
#include "sokol_gfx.h"
#include "sokol_time.h"
#include "sokol_audio.h"
#include "sokol_log.h"
//#include "sokol_fetch.h"
#include "sokol_glue.h"
