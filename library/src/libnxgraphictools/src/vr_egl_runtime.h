#ifndef __VR_EGL_RUNTIME__
#define __VR_EGL_RUNTIME__

#include "vr_common_inc.h"

typedef struct
{
    EGLDisplay sEGLDisplay;
	EGLContext sEGLContext[VR_PROGRAM_MAX];
	EGLConfig sEGLConfig[VR_PROGRAM_MAX];
	unsigned int sEGLContextRef[VR_PROGRAM_MAX];
} EGLInfo;

extern PFNEGLCREATEIMAGEKHRPROC _NX_eglCreateImageKHR;
extern PFNEGLDESTROYIMAGEKHRPROC _NX_eglDestroyImageKHR;
extern PFNGLEGLIMAGETARGETTEXTURE2DOESPROC _NX_glEGLImageTargetTexture2DOES;
extern PFNGLEGLIMAGETARGETRENDERBUFFERSTORAGEOESPROC _NX_glEGLImageTargetRenderbufferStorageOES;

/**
 * Initializes extensions required for EGLImage
 *
 * @return 0 on success, non-0 on failure
 */
int vrInitEglExtensions(void);
int vrInitializeEGLConfig(EGLDisplay display);
int nxGSurfaceCreateEGLContext(unsigned int program);
int nxGSurfaceDestroyEGLContext(unsigned int program);
EGLNativePixmapType nxGSurfaceCreatePixmap(unsigned int uiWidth, unsigned int uiHeight, void* pData, 
							int is_native_video_buf_handle, unsigned int pixel_bits);
void nxGSurfaceDestroyPixmap(EGLNativePixmapType pPixmap);
int nxGSurfaceGetVidFD(void* pData, int is_native_video_buf_handle);
void __vr_base_dbg_halt( void );
VR_ULONG base_util_time_get_usec(void);

#endif /* __VR_EGL_RUNTIME__ */

