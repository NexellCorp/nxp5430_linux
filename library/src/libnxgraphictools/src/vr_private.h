#ifndef __VR_DEINTERACE_PRIVATE_
#define __VR_DEINTERACE_PRIVATE_

#include "vr_common_def.h"
#if defined( VR_PLATFORM_DRM_ALLOC_USE )
#include <nx_video_alloc.h>
#elif defined( VR_PLATFORM_DRM_USER_ALLOC_USE )
#include <nx-drm-allocator.h>
#else
#include <nx_alloc_mem.h>
#endif
#include "vr_egl_runtime.h"


typedef struct
{
	/* Shader variables. */
	GLuint iVertName;
	GLuint iFragName;
	GLuint iProgName;
	GLint iLocPosition;
	GLint iLocColor;
	GLint iLocTexCoord[VR_INPUT_MODE_YUV_MAX];
	GLint iLocInputWidth;
	GLint iLocInputHeight;
	GLint iLocOutputHeight;
	GLint iLocMainTex[VR_INPUT_MODE_YUV_MAX];
	GLint iLocRefTex;
	GLint iLocTexInvSize;
	GLint iLocFilterGain;
}Shader;

typedef struct
{
	EGLInfo    egl_info;
	Shader     shader[VR_PROGRAM_MAX];
	
	NX_MEMORY_HANDLE   default_target_memory[VR_PROGRAM_MAX];
	struct vrSurfaceTarget* default_target[VR_PROGRAM_MAX];
	EGLSurface display_surface;
	EGLNativeWindowType platform_win;
	GLuint tex_deinterlace_ref_id;
	GLuint tex_display_id;
}Statics;


//interanl API
Statics *vrGetStatics();

#endif  /* __VR_DEINTERACE_PRIVATE_ */

