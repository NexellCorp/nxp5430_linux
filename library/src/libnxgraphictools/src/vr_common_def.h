#ifndef __VR_COMMON_DEF__
#define __VR_COMMON_DEF__



//------------------------------------------------------------------------------
//
//    Includes
//    
//------------------------------------------------------------------------------
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <assert.h>



//------------------------------------------------------------------------------
//
//    Defines
//    
//------------------------------------------------------------------------------
// 
//	debugging mode
//
//#define VR_DEBUG
// 
//	fd mode
//
//#define VR_FEATURE_SEPERATE_FD_USE 		//for camera input source
// 
//	platform native video mem alloc mode
//
//#define VR_PLATFORM_DRM_ALLOC_USE
#define VR_FEATURE_ION_ALLOC_USE
//#define VR_PLATFORM_DRM_USER_ALLOC_USE
// 
//	destination
//
#define VR_FEATURE_OUTPUT_TEXTURE_USE
// 
//	source
//
#define VR_FEATURE_INPUT_EGLIMAGE_USE
// 
//	etc
//
//#define VR_FEATURE_PLATFORM_HIGHP_USE //don't use this in case of half gcc toolchain
#define VR_FEATURE_YCRCB_NV21_USE
#define VR_FEATURE_HEIGHT_ALIGN16_USE
#define VR_FEATURE_STRIDE_USE

#ifdef VR_FEATURE_STRIDE_USE
#if 1 /* org */
#define VR_FEATURE_INPUT_HEIGHT_ALIGN_BYTE	16
#define VR_FEATURE_OUTPUT_HEIGHT_ALIGN_BYTE	VR_FEATURE_INPUT_HEIGHT_ALIGN_BYTE
#else /* temp test, ������ Ȯ���ϱ� ���Ͽ� */
#define VR_FEATURE_INPUT_HEIGHT_ALIGN_BYTE	0
#define VR_FEATURE_OUTPUT_HEIGHT_ALIGN_BYTE 0
#endif

#else
#define VR_FEATURE_INPUT_HEIGHT_ALIGN_BYTE	0
#define VR_FEATURE_OUTPUT_HEIGHT_ALIGN_BYTE 0
#endif


//------------------------------------------------------------------------------
//    Defines
//------------------------------------------------------------------------------
#ifdef VR_DEBUG
#ifdef ANDROID
#include <cutils/log.h>

#define NxDbgMsg	ALOGD 
#define NxErrMsg 	ALOGE 

#else	//	Linux
#include <stdio.h>

#define NxDbgMsg	printf
#define NxErrMsg 	printf
#endif
#else
#define NxDbgMsg	printf
#define NxErrMsg  	printf
#endif
#define NxDbgPoint()	printf(" --- called at %s(%d)\n", __FUNCTION__, __LINE__)


//------------------------------------------------------------------------------
//    Defines
//------------------------------------------------------------------------------
#if defined( WIN32 )
typedef __int64 VR_LONG;
typedef unsigned __int64 VR_ULONG;
#else
typedef long long VR_LONG;
typedef unsigned long long VR_ULONG;
#endif
typedef unsigned int 	VR_BOOL;

#define VR_TRUE 	1
#define VR_FALSE	0
#define VR_FAIL		-1


//program
enum{
	VR_PROGRAM_DEINTERLACE,
	VR_PROGRAM_SCALE,
	VR_PROGRAM_CVT2UV,
	VR_PROGRAM_CVT2Y,
	VR_PROGRAM_CVT2RGBA,
	VR_PROGRAM_USERFILTER,
	VR_PROGRAM_YUVFILTER_Y,
	VR_PROGRAM_YUVFILTER_UV,
	VR_PROGRAM_SCALE_RGBA,
	VR_PROGRAM_DISPLAY,
	VR_PROGRAM_MAX
};

//texture unit
typedef enum{
	VR_INPUT_MODE_DEINTERLACE,
	VR_INPUT_MODE_DEINTERLACE_REF,
	VR_INPUT_MODE_TEXTURE0,
	VR_INPUT_MODE_TEXTURE1,
	VR_INPUT_MODE_USERFILTER,
	VR_INPUT_MODE_YUV_FILTER,
	VR_INPUT_MODE_SCALE_RGBA,
	VR_INPUT_MODE_DISPLAY,
	VR_INPUT_MODE_MAX
}VRInputMode;

//YUV texture unit 
typedef enum{
	VR_INPUT_MODE_Y,
	VR_INPUT_MODE_U,
	VR_INPUT_MODE_V,
	VR_INPUT_MODE_YUV_MAX
}VRInputYUVMode;

//format 
typedef enum{
	VR_IMAGE_FORMAT_RGBA,
	VR_IMAGE_FORMAT_Y,
	VR_IMAGE_FORMAT_U,
	VR_IMAGE_FORMAT_V,
	VR_IMAGE_FORMAT_UV,
	VR_IMAGE_FORMAT_YUV,
	VR_IMAGE_FORMAT_MAX
}VRImageFormatMode;

//rotate mode
typedef enum{
	VR_ROTATE_0_DEGREE,
	VR_ROTATE_90_DEGREE,
	VR_ROTATE_180_DEGREE,
	VR_ROTATE_270_DEGREE,
	VR_ROTATE_MIRROR_DEGREE,
	VR_ROTATE_VFLIP_DEGREE,
}VRImageRotateMode;

#define NX_MALLOC(a) malloc(a)
#define NX_CALLOC(a,b) calloc(a, b)
#define NX_REALLOC(a,b) realloc(a, b)
#define NX_FREE(a) free((void*)a)
#define NX_MEMSET(dst,val,size) memset(dst,val,size)
#define NX_MEMCPY(dst,src,size) memcpy(dst,src,size)


//------------------------------------------------------------------------------
//    Defines
//------------------------------------------------------------------------------
// 
//	debugging info: conditional run, not halt  
//
#if defined( __STDC_WANT_SECURE_LIB__ ) && ( 0 != __STDC_WANT_SECURE_LIB__ )
#define VSNPRINTF           vsnprintf_s
#else
#define VSNPRINTF           vsnprintf
#endif
#define VR_PRINTF			printf

#if defined(_MSC_VER)
#define VR_EXPLICIT_FALSE (1,0)
#else
#define VR_EXPLICIT_FALSE 0
#endif
#define VR_ASSERT(info, expr)			do { \
											if (!(expr)){ \
												VR_PRINTF("*ASSERT(%s): %s\n\n", info, #expr); \
												VR_PRINTF("func: %s(%d)\n\n", __FUNCTION__, __LINE__); \
												assert(!"VR_HALT!"); \
												__vr_base_dbg_halt(); \
											} \
										} while(VR_EXPLICIT_FALSE)
											
#if defined(VR_DEBUG)
#define VR_INFO(prefix, info_enable, msg, ...) 	\
										do { \
											if (info_enable ){	\
												VR_PRINTF(prefix "*I:"); \
												VR_PRINTF(msg, ##__VA_ARGS__); \
											} \
										} while(VR_EXPLICIT_FALSE)
#else
#define VR_INFO(prefix, info_enable, msg, ...) 	{}
#endif

#define VR_ALIGN( value, base ) (((value) + ((base) - 1)) & ~((base) - 1))
#define VR_DISABLE_UNUSED_VAR_WARNING(X)	((void)(&(X)))

#if 1
#define EGL_CHECK(x) \
	x; \
	{ \
		EGLint eglError = eglGetError(); \
		if (eglError != EGL_SUCCESS) { \
			VR_PRINTF("eglGetError() = %i (0x%.8x) at %s:%i\n", eglError, eglError, __FILE__, __LINE__); \
			exit(1); \
		} \
	}

#define GL_CHECK(x) \
	x; \
	{ \
		GLenum glError = glGetError(); \
		if (glError != GL_NO_ERROR) { \
			VR_PRINTF( "glGetError() = %i (0x%.8x) at %s:%i\n", glError, glError, __FILE__, __LINE__); \
			exit(1); \
		} \
	}
#else
#define EGL_CHECK(x) 	x
#define GL_CHECK(x)		x
#endif



//------------------------------------------------------------------------------
//
//    Functions
//    
//------------------------------------------------------------------------------
#if defined( VR_PLATFORM_DRM_ALLOC_USE )
#include <fcntl.h>
#include <gbm.h>
#include <drm/drm_fourcc.h>
#include <nx_video_alloc.h>
#include <EGL/drm_window.h>

typedef fbdev_pixmap VR_PLATFORM_PIXMAP_STRUCT;

#elif defined( VR_PLATFORM_DRM_USER_ALLOC_USE )
typedef struct tagNxDrmVmem
{
	int drm_fd;
	int gem_fd;
	int flinks;
	int dma_fd;
	void *virAddr;
	unsigned int size;
}NxDrmVmem;

#include <fcntl.h>
#include <gbm.h>
#include <drm/drm_fourcc.h>
#include <nx-drm-allocator.h>
#include <EGL/drm_window.h>

typedef fbdev_pixmap VR_PLATFORM_PIXMAP_STRUCT;
typedef void* NX_MEMORY_HANDLE;

#else
#include <nx_fourcc.h>
#include <nx_alloc_mem.h>
#include <nx_graphictools.h>
#include <EGL/fbdev_window.h>

typedef fbdev_window VR_PLATFORM_WINDOW_STRUCT;
typedef fbdev_pixmap VR_PLATFORM_PIXMAP_STRUCT;
#endif

#endif /* __VR_COMMON_DEF__ */

