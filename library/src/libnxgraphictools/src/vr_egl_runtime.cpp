#include <stdio.h>
#include <stdlib.h>
#include <linux/fb.h>

#include "vr_common_inc.h"
#include "vr_private.h"

#include <errno.h>
#include <string.h>
#include <stdarg.h>
#include <unistd.h>		// for open/close/usleep  
#include <sys/time.h>

PFNEGLCREATEIMAGEKHRPROC _NX_eglCreateImageKHR = NULL;
PFNEGLDESTROYIMAGEKHRPROC _NX_eglDestroyImageKHR = NULL;
PFNGLEGLIMAGETARGETTEXTURE2DOESPROC _NX_glEGLImageTargetTexture2DOES = NULL;
PFNGLEGLIMAGETARGETRENDERBUFFERSTORAGEOESPROC _NX_glEGLImageTargetRenderbufferStorageOES = NULL;

void __vr_base_dbg_halt( void )
{
	VR_PRINTF("__vr_base_dbg_halt, user debugging halt!\n");	
	while(1)
		usleep(100*1000);	
}

VR_ULONG base_util_time_get_usec(void)
{
	int result;
	struct timeval tod;

	result = gettimeofday(&tod, NULL);

	/* gettimeofday returns non-null on error*/
	if (0 != result) return 0;

	return ((VR_ULONG)tod.tv_sec) * 1000000ULL + tod.tv_usec;
}

int nxGSurfaceGetVidFD(void* pData, int is_native_video_buf_handle)
{
	int fd_handle = -1;
	if (is_native_video_buf_handle)
	{
		#if defined( VR_PLATFORM_DRM_ALLOC_USE )
		fd_handle = (int)(((NX_MEMORY_HANDLE)pData)->dmaFd);
		#elif defined( VR_PLATFORM_DRM_USER_ALLOC_USE )
		NxDrmVmem* vmem_drm = (NxDrmVmem*)pData;
		fd_handle = vmem_drm->dma_fd;
		#else
		fd_handle = (int)(((NX_MEMORY_HANDLE)pData)->privateDesc);
		#endif
	}
	else
	{
		#if defined( VR_PLATFORM_DRM_ALLOC_USE )
		fd_handle = (int)pData;
		#elif defined( VR_PLATFORM_DRM_USER_ALLOC_USE )
		fd_handle = (int)pData;
		#else
		fd_handle = (int)pData;
		#endif
	}

	VR_ASSERT("fd_handle must be valid", fd_handle);
	
	return fd_handle;
}

EGLNativePixmapType nxGSurfaceCreatePixmap(unsigned int uiWidth, unsigned int uiHeight, void* pData, 
						int is_native_video_buf_handle, unsigned int pixel_bits)
{	
	VR_PLATFORM_PIXMAP_STRUCT* pPixmap = (VR_PLATFORM_PIXMAP_STRUCT *)NX_CALLOC(1, sizeof(VR_PLATFORM_PIXMAP_STRUCT));
	if (pPixmap == NULL)
	{
		NxErrMsg("Error: Out of memory at %s:%i\n", __FILE__, __LINE__);
		return (EGLNativePixmapType)0;
	}

	uiWidth = VR_ALIGN(uiWidth, 2);
	pPixmap->width = uiWidth;
	pPixmap->height = uiHeight;
	if (32 == pixel_bits)
	{
		pPixmap->bytes_per_pixel = 4;
		pPixmap->buffer_size = 32;
		pPixmap->red_size = 8;
		pPixmap->green_size = 8;
		pPixmap->blue_size = 8;
		pPixmap->alpha_size = 8;
		pPixmap->luminance_size = 0;
	}
	else if (16 == pixel_bits)
	{
		pPixmap->bytes_per_pixel = 2;
		pPixmap->buffer_size = 16;
		pPixmap->red_size = 0;
		pPixmap->green_size = 0;
		pPixmap->blue_size = 0;
		pPixmap->alpha_size = 8;
		pPixmap->luminance_size = 8;
	}		
	else if (8 == pixel_bits)
	{
		pPixmap->bytes_per_pixel = 1;
		pPixmap->buffer_size = 8;
		pPixmap->red_size = 0;
		pPixmap->green_size = 0;
		pPixmap->blue_size = 0;
		pPixmap->alpha_size = 0;
		pPixmap->luminance_size = 8;
	}	
	else
	{
		NxErrMsg("Error: pixel_bits(%d) is not valid. %s:%i\n", pixel_bits, __FILE__, __LINE__);
		NX_FREE(pPixmap);
		return (EGLNativePixmapType)0;
	}
	pPixmap->is_no_readback_proc = VR_TRUE;

	int fd_handle = nxGSurfaceGetVidFD(pData, is_native_video_buf_handle);
	
	pPixmap->flags = (fbdev_pixmap_flags)FBDEV_PIXMAP_COLORSPACE_sRGB;
	pPixmap->flags = (fbdev_pixmap_flags)(pPixmap->flags |FBDEV_PIXMAP_DMA_BUF);
	#if 0 /* for current mali version */
	fbdev_dma_buf *pinfo_dma_buf = (fbdev_dma_buf *)NX_CALLOC(1, sizeof(fbdev_dma_buf));
	if (pinfo_dma_buf == NULL)
	{
		NxErrMsg("Error: NULL memory at %s:%i\n", __FILE__, __LINE__);
		NX_FREE(pPixmap);
		return (EGLNativePixmapType)0;
	}
	pinfo_dma_buf->fd = fd_handle;
	pinfo_dma_buf->ptr = NULL;
	pinfo_dma_buf->size = uiWidth * uiHeight * (pixel_bits/8);

	pPixmap->data = (unsigned short *)pinfo_dma_buf;
	*((int*)pPixmap->data) = (int)fd_handle;
	#else /* for old mali version(VRlib) */
	pPixmap->data = (unsigned short *)NX_CALLOC(1, sizeof(int));
	if (pPixmap->data == NULL)
	{
		NxErrMsg("Error: NULL memory at %s:%i\n", __FILE__, __LINE__);
		NX_FREE(pPixmap);
		return NULL;
	}
	*((int*)pPixmap->data) = (int)fd_handle;	
	#endif
	
	return (EGLNativePixmapType)pPixmap;
}

void nxGSurfaceDestroyPixmap(EGLNativePixmapType pPixmap)
{
#ifdef VR_FEATURE_ION_ALLOC_USE	
	if (((VR_PLATFORM_PIXMAP_STRUCT*)pPixmap)->data)
		NX_FREE(((VR_PLATFORM_PIXMAP_STRUCT*)pPixmap)->data);
#endif
	if (pPixmap)
		NX_FREE(pPixmap);
}

int vrInitializeEGLConfig(EGLDisplay display)
{
	Statics *pStatics = vrGetStatics();
	EGLBoolean bResult = EGL_FALSE;
	EGLConfig config8Bit = 0, config16Bit = 0, config32Bit = 0, config_display = 0;

	EGLint aEGLAttributes_32bit[] =
	{
		EGL_SAMPLES,			0,
		EGL_RED_SIZE,			8,
		EGL_GREEN_SIZE, 		8,
		EGL_BLUE_SIZE,			8,
		EGL_ALPHA_SIZE, 		8,
		EGL_LUMINANCE_SIZE, 	0,
		EGL_BUFFER_SIZE,		32,
		EGL_DEPTH_SIZE, 		0,
		EGL_STENCIL_SIZE,		0,
		EGL_SURFACE_TYPE,		EGL_PIXMAP_BIT,
		EGL_COLOR_BUFFER_TYPE,	EGL_RGB_BUFFER,
		EGL_NONE
	};

	EGLint aEGLAttributes_8bit[] =
	{
		EGL_SAMPLES,			0,
		EGL_RED_SIZE,			0,
		EGL_GREEN_SIZE, 		0,
		EGL_BLUE_SIZE,			0,
		EGL_ALPHA_SIZE, 		0,
		EGL_LUMINANCE_SIZE, 	8,
		EGL_BUFFER_SIZE,		8,
		EGL_DEPTH_SIZE, 		0,
		EGL_STENCIL_SIZE,		0,
		EGL_SURFACE_TYPE,		EGL_PIXMAP_BIT,
		EGL_COLOR_BUFFER_TYPE,	EGL_LUMINANCE_BUFFER,
		EGL_NONE
	};
	EGLint aEGLAttributes_16bit[] =
	{
		EGL_SAMPLES,			0,
		EGL_RED_SIZE,			0,
		EGL_GREEN_SIZE, 		0,
		EGL_BLUE_SIZE,			0,
		EGL_ALPHA_SIZE, 		8,
		EGL_LUMINANCE_SIZE, 	8,
		EGL_BUFFER_SIZE,		16,
		EGL_DEPTH_SIZE, 		0,
		EGL_STENCIL_SIZE,		0,
		EGL_SURFACE_TYPE,		EGL_PIXMAP_BIT,
		EGL_COLOR_BUFFER_TYPE,	EGL_LUMINANCE_BUFFER,
		EGL_NONE
	};

	EGLint aEGLAttributes_display[] =
	{
		EGL_SAMPLES,			0,
		EGL_RED_SIZE,			8,
		EGL_GREEN_SIZE, 		8,
		EGL_BLUE_SIZE,			8,
		EGL_ALPHA_SIZE, 		8,
		EGL_LUMINANCE_SIZE, 	0,
		EGL_BUFFER_SIZE,		32,
		EGL_DEPTH_SIZE, 		0,
		EGL_STENCIL_SIZE,		0,
		EGL_SURFACE_TYPE,		EGL_WINDOW_BIT,
		EGL_COLOR_BUFFER_TYPE,	EGL_RGB_BUFFER,
		EGL_NONE
	};

	if (display == EGL_NO_DISPLAY)
	{
		NxErrMsg("Error: No EGL Display available at %s:%i\n", __FILE__, __LINE__);
		return -1;
	}
	pStatics->egl_info.sEGLDisplay = display;
	
	/* Choose 8bit config. */
	{
		EGLConfig *pEGLConfig = NULL;
		int iEGLConfig = 0;
		EGLint cEGLConfigs = 0;

		/* Enumerate available EGL configurations which match or exceed our required attribute list. */
		bResult = eglChooseConfig(pStatics->egl_info.sEGLDisplay, aEGLAttributes_8bit, NULL, 0, &cEGLConfigs);
		if (bResult != EGL_TRUE)
		{
			EGLint iError = eglGetError();
			NxErrMsg("eglGetError(): %i (0x%.4x)\n", (int)iError, (int)iError);
			NxErrMsg("Error: Failed to enumerate EGL configs at %s:%i\n", __FILE__, __LINE__);
			return -1;
		}
		
		/* Allocate space for all EGL configs available and get them. */
		pEGLConfig = (EGLConfig *)NX_CALLOC(cEGLConfigs, sizeof(EGLConfig));
		if (pEGLConfig == NULL)
		{
			NxErrMsg("Error: Out of memory at %s:%i\n", __FILE__, __LINE__);
			return -1;
		}
		bResult = eglChooseConfig(pStatics->egl_info.sEGLDisplay, aEGLAttributes_8bit, pEGLConfig, cEGLConfigs, &cEGLConfigs);
		if (bResult != EGL_TRUE)
		{
			EGLint iError = eglGetError();
			NxErrMsg("eglGetError(): %i (0x%.4x)\n", (int)iError, (int)iError);
			NxErrMsg("Error: Failed to enumerate EGL configs at %s:%i\n", __FILE__, __LINE__);
			NX_FREE(pEGLConfig);
			return -1;
		}
		if (cEGLConfigs == 0)
		{
			NxErrMsg("thers is no config at %s:%i\n", __FILE__, __LINE__);
			NX_FREE(pEGLConfig);
			return -1;
		}

		//NxErrMsg("8bit config search start(total:%d)\n", cEGLConfigs);
		
		/* Loop through the EGL configs to find a color depth match.
		 * NB This is necessary, since EGL considers a higher color depth than requested to be 'better'
		 * even though this may force the driver to use a slow color conversion blitting routine. */
		for(iEGLConfig = 0; iEGLConfig < cEGLConfigs; iEGLConfig ++)
		{
			EGLint iEGLValue = 0;
			bResult = eglGetConfigAttrib(pStatics->egl_info.sEGLDisplay, pEGLConfig[iEGLConfig], EGL_BUFFER_SIZE, &iEGLValue);
			if (bResult != EGL_TRUE)
			{
				EGLint iError = eglGetError();
				NxErrMsg("eglGetError(): %i (0x%.4x)\n", (int)iError, (int)iError);
				NxErrMsg("Error: Failed to get EGL attribute at %s:%i\n", __FILE__, __LINE__);			
				NX_FREE(pEGLConfig);
				return -1;			
			}
			if (iEGLValue == 8) break;
		}

		//NxErrMsg("8bit config search end(n:%d)\n", iEGLConfig);
		
		if (iEGLConfig >= cEGLConfigs)
		{
			NxErrMsg("Error: Failed to find matching EGL config at %s:%i\n", __FILE__, __LINE__);
			NX_FREE(pEGLConfig);
			return -1;	
		}
		config8Bit = pEGLConfig[iEGLConfig];
		NX_FREE(pEGLConfig);
		pEGLConfig = NULL;
	}

	/* Choose 16bit config. */
	{
		EGLConfig *pEGLConfig = NULL;
		int iEGLConfig = 0;
		EGLint cEGLConfigs = 0;

		/* Enumerate available EGL configurations which match or exceed our required attribute list. */
		bResult = eglChooseConfig(pStatics->egl_info.sEGLDisplay, aEGLAttributes_16bit, NULL, 0, &cEGLConfigs);
		if (bResult != EGL_TRUE)
		{
			EGLint iError = eglGetError();
			NxErrMsg("eglGetError(): %i (0x%.4x)\n", (int)iError, (int)iError);
			NxErrMsg("Error: Failed to enumerate EGL configs at %s:%i\n", __FILE__, __LINE__);
			return -1;
		}
		
		/* Allocate space for all EGL configs available and get them. */
		pEGLConfig = (EGLConfig *)NX_CALLOC(cEGLConfigs, sizeof(EGLConfig));
		if (pEGLConfig == NULL)
		{
			NxErrMsg("Error: Out of memory at %s:%i\n", __FILE__, __LINE__);
			return -1;
		}
		bResult = eglChooseConfig(pStatics->egl_info.sEGLDisplay, aEGLAttributes_16bit, pEGLConfig, cEGLConfigs, &cEGLConfigs);
		if (bResult != EGL_TRUE)
		{
			EGLint iError = eglGetError();
			NxErrMsg("eglGetError(): %i (0x%.4x)\n", (int)iError, (int)iError);
			NxErrMsg("Error: Failed to enumerate EGL configs at %s:%i\n", __FILE__, __LINE__);
			NX_FREE(pEGLConfig);
			return -1;
		}
		if (cEGLConfigs == 0)
		{
			NxErrMsg("thers is no config at %s:%i\n", __FILE__, __LINE__);
			NX_FREE(pEGLConfig);
			return -1;
		}
		
		//NxErrMsg("16bit config search start(total:%d)\n", cEGLConfigs);
		
		/* Loop through the EGL configs to find a color depth match.
		 * NB This is necessary, since EGL considers a higher color depth than requested to be 'better'
		 * even though this may force the driver to use a slow color conversion blitting routine. */
		for(iEGLConfig = 0; iEGLConfig < cEGLConfigs; iEGLConfig ++)
		{
			EGLint iEGLValue = 0;
			bResult = eglGetConfigAttrib(pStatics->egl_info.sEGLDisplay, pEGLConfig[iEGLConfig], EGL_BUFFER_SIZE, &iEGLValue);
			if (bResult != EGL_TRUE)
			{
				EGLint iError = eglGetError();
				NxErrMsg("eglGetError(): %i (0x%.4x)\n", (int)iError, (int)iError);
				NxErrMsg("Error: Failed to get EGL attribute at %s:%i\n", __FILE__, __LINE__);			
				NX_FREE(pEGLConfig);
				return -1;			
			}
			if (iEGLValue == 16)
			{
				//eglGetConfigAttrib(pStatics->egl_info.sEGLDisplay, pEGLConfig[iEGLConfig], EGL_CONFIG_ID, &iEGLValue);
				//NxErrMsg("16bit config id(n:%d)\n", iEGLValue);
				break;
			}
		}
		//NxErrMsg("16bit config search end(n:%d)\n", iEGLConfig);

		if (iEGLConfig >= cEGLConfigs)
		{
			NxErrMsg("Error: Failed to find matching EGL config at %s:%i\n", __FILE__, __LINE__);
			NX_FREE(pEGLConfig);
			return -1;	
		}
		config16Bit = pEGLConfig[iEGLConfig];
		NX_FREE(pEGLConfig);
		pEGLConfig = NULL;		
	}

	/* Choose 32bit config. */
	{
		EGLConfig *pEGLConfig = NULL;
		int iEGLConfig = 0;
		EGLint cEGLConfigs = 0;

		/* Enumerate available EGL configurations which match or exceed our required attribute list. */
		bResult = eglChooseConfig(pStatics->egl_info.sEGLDisplay, aEGLAttributes_32bit, NULL, 0, &cEGLConfigs);
		if (bResult != EGL_TRUE)
		{
			EGLint iError = eglGetError();
			NxErrMsg("eglGetError(): %i (0x%.4x)\n", (int)iError, (int)iError);
			NxErrMsg("Error: Failed to enumerate EGL configs at %s:%i\n", __FILE__, __LINE__);
			return -1;
		}
		
		/* Allocate space for all EGL configs available and get them. */
		pEGLConfig = (EGLConfig *)NX_CALLOC(cEGLConfigs, sizeof(EGLConfig));
		if (pEGLConfig == NULL)
		{
			NxErrMsg("Error: Out of memory at %s:%i\n", __FILE__, __LINE__);
			return -1;
		}
		bResult = eglChooseConfig(pStatics->egl_info.sEGLDisplay, aEGLAttributes_32bit, pEGLConfig, cEGLConfigs, &cEGLConfigs);
		if (bResult != EGL_TRUE)
		{
			EGLint iError = eglGetError();
			NxErrMsg("eglGetError(): %i (0x%.4x)\n", (int)iError, (int)iError);
			NxErrMsg("Error: Failed to enumerate EGL configs at %s:%i\n", __FILE__, __LINE__);
			NX_FREE(pEGLConfig);
			return -1;
		}
		if (cEGLConfigs == 0)
		{
			NxErrMsg("thers is no config at %s:%i\n", __FILE__, __LINE__);
			NX_FREE(pEGLConfig);
			return -1;
		}

		//NxErrMsg("32bit config search start(total:%d)\n", cEGLConfigs);		
		/* Loop through the EGL configs to find a color depth match.
		 * NB This is necessary, since EGL considers a higher color depth than requested to be 'better'
		 * even though this may force the driver to use a slow color conversion blitting routine. */
		for(iEGLConfig = 0; iEGLConfig < cEGLConfigs; iEGLConfig ++)
		{
			EGLint iEGLValue = 0;
			bResult = eglGetConfigAttrib(pStatics->egl_info.sEGLDisplay, pEGLConfig[iEGLConfig], EGL_BUFFER_SIZE, &iEGLValue);
			if (bResult != EGL_TRUE)
			{
				EGLint iError = eglGetError();
				NxErrMsg("eglGetError(): %i (0x%.4x)\n", (int)iError, (int)iError);
				NxErrMsg("Error: Failed to get EGL attribute at %s:%i\n", __FILE__, __LINE__);			
				NX_FREE(pEGLConfig);
				return -1;			
			}
			if (iEGLValue == 32) break;
		}
		//NxErrMsg("32bit config search end(n:%d)\n", iEGLConfig);

		if (iEGLConfig >= cEGLConfigs)
		{
			NxErrMsg("Error: Failed to find matching EGL config at %s:%i\n", __FILE__, __LINE__);
			NX_FREE(pEGLConfig);
			return -1;	
		}
		config32Bit = pEGLConfig[iEGLConfig];
		NX_FREE(pEGLConfig);
		pEGLConfig = NULL;	
	}	

	/* Choose display config. */
	{
		EGLConfig *pEGLConfig = NULL;
		int iEGLConfig = 0;
		EGLint cEGLConfigs = 0;

		/* Enumerate available EGL configurations which match or exceed our required attribute list. */
		bResult = eglChooseConfig(pStatics->egl_info.sEGLDisplay, aEGLAttributes_display, NULL, 0, &cEGLConfigs);
		if (bResult != EGL_TRUE)
		{
			EGLint iError = eglGetError();
			NxErrMsg("eglGetError(): %i (0x%.4x)\n", (int)iError, (int)iError);
			NxErrMsg("Error: Failed to enumerate EGL configs at %s:%i\n", __FILE__, __LINE__);
			return -1;
		}
		
		/* Allocate space for all EGL configs available and get them. */
		pEGLConfig = (EGLConfig *)NX_CALLOC(cEGLConfigs, sizeof(EGLConfig));
		if (pEGLConfig == NULL)
		{
			NxErrMsg("Error: Out of memory at %s:%i\n", __FILE__, __LINE__);
			return -1;
		}
		bResult = eglChooseConfig(pStatics->egl_info.sEGLDisplay, aEGLAttributes_display, pEGLConfig, cEGLConfigs, &cEGLConfigs);
		if (bResult != EGL_TRUE)
		{
			EGLint iError = eglGetError();
			NxErrMsg("eglGetError(): %i (0x%.4x)\n", (int)iError, (int)iError);
			NxErrMsg("Error: Failed to enumerate EGL configs at %s:%i\n", __FILE__, __LINE__);
			NX_FREE(pEGLConfig);
			return -1;
		}
		if (cEGLConfigs == 0)
		{
			NxErrMsg("thers is no config at %s:%i\n", __FILE__, __LINE__);
			NX_FREE(pEGLConfig);
			return -1;
		}

		//NxErrMsg("32bit config search start(total:%d)\n", cEGLConfigs);		
		/* Loop through the EGL configs to find a color depth match.
		 * NB This is necessary, since EGL considers a higher color depth than requested to be 'better'
		 * even though this may force the driver to use a slow color conversion blitting routine. */
		for(iEGLConfig = 0; iEGLConfig < cEGLConfigs; iEGLConfig ++)
		{
			EGLint iEGLValue = 0;
			bResult = eglGetConfigAttrib(pStatics->egl_info.sEGLDisplay, pEGLConfig[iEGLConfig], EGL_BUFFER_SIZE, &iEGLValue);
			if (bResult != EGL_TRUE)
			{
				EGLint iError = eglGetError();
				NxErrMsg("eglGetError(): %i (0x%.4x)\n", (int)iError, (int)iError);
				NxErrMsg("Error: Failed to get EGL attribute at %s:%i\n", __FILE__, __LINE__);			
				NX_FREE(pEGLConfig);
				return -1;			
			}
			if (iEGLValue == 32) break;
		}
		//NxErrMsg("32bit config search end(n:%d)\n", iEGLConfig);

		if (iEGLConfig >= cEGLConfigs)
		{
			NxErrMsg("Error: Failed to find matching EGL config at %s:%i\n", __FILE__, __LINE__);
			NX_FREE(pEGLConfig);
			return -1;	
		}
		config_display = pEGLConfig[iEGLConfig];
		NX_FREE(pEGLConfig);
		pEGLConfig = NULL;	
	}
	
	pStatics->egl_info.sEGLConfig[VR_PROGRAM_DEINTERLACE] = config32Bit;
	pStatics->egl_info.sEGLConfig[VR_PROGRAM_SCALE] = config8Bit;
	pStatics->egl_info.sEGLConfig[VR_PROGRAM_CVT2Y] = config8Bit;
	pStatics->egl_info.sEGLConfig[VR_PROGRAM_CVT2UV] = config16Bit;
	pStatics->egl_info.sEGLConfig[VR_PROGRAM_CVT2RGBA] = config32Bit;
	pStatics->egl_info.sEGLConfig[VR_PROGRAM_YUVFILTER_Y] = config8Bit;
	pStatics->egl_info.sEGLConfig[VR_PROGRAM_USERFILTER] = config32Bit;
	pStatics->egl_info.sEGLConfig[VR_PROGRAM_DISPLAY] = config_display;
	pStatics->egl_info.sEGLConfig[VR_PROGRAM_SCALE_RGBA] = config32Bit;
	
	return 0;
}

int nxGSurfaceCreateEGLContext(unsigned int program)
{
	Statics *pStatics = vrGetStatics();

	static const EGLint aEGLContextAttributes[] =
	{
		EGL_CONTEXT_CLIENT_VERSION, 2,
		EGL_NONE
	};	

	/* Create context. */
	pStatics->egl_info.sEGLContext[program] = eglCreateContext(pStatics->egl_info.sEGLDisplay, pStatics->egl_info.sEGLConfig[program], EGL_NO_CONTEXT, aEGLContextAttributes);
	if (pStatics->egl_info.sEGLContext[program] == EGL_NO_CONTEXT)
	{
		EGLint iError = eglGetError();
		NxErrMsg("eglGetError(): %i (0x%.4x)\n", (int)iError, (int)iError);
		NxErrMsg("Error: Failed to create EGL context at %s:%i\n", __FILE__, __LINE__);
		return -1;	
	}
	
	return 0;
}

int nxGSurfaceDestroyEGLContext(unsigned int program)
{
	Statics *pStatics = vrGetStatics();
	EGLBoolean bResult = EGL_FALSE;

	bResult = eglMakeCurrent(pStatics->egl_info.sEGLDisplay, 
				EGL_NO_SURFACE, EGL_NO_SURFACE, EGL_NO_CONTEXT);
	if (bResult == EGL_FALSE)
	{
		EGLint iError = eglGetError();
		NxErrMsg("eglGetError(): %i (0x%.4x)\n", (int)iError, (int)iError);
		NxErrMsg("Error: Failed to make context current at %s:%i\n", __FILE__, __LINE__);
	}			

	bResult = eglDestroyContext(pStatics->egl_info.sEGLDisplay, pStatics->egl_info.sEGLContext[program]);	
	if (bResult == EGL_FALSE)
	{
		EGLint iError = eglGetError();
		NxErrMsg("eglGetError(): %i (0x%.4x)\n", (int)iError, (int)iError);
		NxErrMsg("Error: Failed to destroy context at %s:%i\n", __FILE__, __LINE__);
	}			
	pStatics->egl_info.sEGLContext[program] = EGL_NO_CONTEXT;
	return 0;
}

int vrInitEglExtensions(void)
{
	_NX_eglCreateImageKHR = (PFNEGLCREATEIMAGEKHRPROC) eglGetProcAddress("eglCreateImageKHR");
	if ( NULL == _NX_eglCreateImageKHR ) return -1;

	_NX_eglDestroyImageKHR = (PFNEGLDESTROYIMAGEKHRPROC) eglGetProcAddress("eglDestroyImageKHR");
	if ( NULL == _NX_eglDestroyImageKHR ) return -1;

	_NX_glEGLImageTargetTexture2DOES = (PFNGLEGLIMAGETARGETTEXTURE2DOESPROC) eglGetProcAddress("glEGLImageTargetTexture2DOES");
	if ( NULL == _NX_glEGLImageTargetTexture2DOES ) return -1;

	_NX_glEGLImageTargetRenderbufferStorageOES = (PFNGLEGLIMAGETARGETRENDERBUFFERSTORAGEOESPROC) eglGetProcAddress("glEGLImageTargetRenderbufferStorageOES");
	if ( NULL == _NX_glEGLImageTargetRenderbufferStorageOES ) return -1;

	return 0;
}

