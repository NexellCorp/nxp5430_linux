#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <assert.h>

#include "vr_common_inc.h"
#include "vr_private.h"

#define TPOS1 	0.0f, 1.0f
#define TPOS2 	0.0f, 0.0f
#define TPOS3 	1.0f, 0.0f
#define TPOS4 	1.0f, 1.0f

static bool    s_Initialized = false;
static Statics s_Statics;

static void vrInitializeStatics( void )
{
	NX_MEMSET(&s_Statics, 0x0, sizeof(s_Statics));
}

Statics* vrGetStatics( void )
{
	return &s_Statics;
}

#if 0
namespace 
{
	class AutoBackupCurrentEGL 
	{
	public:
		EGLContext eglCurrentContext;
		EGLSurface eglCurrentSurface[2];
		EGLDisplay eglCurrentDisplay;
		AutoBackupCurrentEGL(void)
		{
			eglCurrentContext    = eglGetCurrentContext();
			eglCurrentSurface[0] = eglGetCurrentSurface(EGL_DRAW);
			eglCurrentSurface[1] = eglGetCurrentSurface(EGL_READ);
			eglCurrentDisplay    = eglGetCurrentDisplay();	
		}
		~AutoBackupCurrentEGL()
		{
			//????
			//eglMakeCurrent(eglCurrentDisplay, eglCurrentSurface[0], eglCurrentSurface[1], eglCurrentContext);
		}
	};
	#define _AUTO_BACKUP_CURRENT_EGL_ AutoBackupCurrentEGL instanceAutoBackupCurrentEGL
};
#else
#define _AUTO_BACKUP_CURRENT_EGL_
#endif

struct vrSurfaceTarget
{
#ifdef VR_FEATURE_STRIDE_USE
	unsigned int		stride;
#endif
	unsigned int        width;
	unsigned int        height;
#ifdef VR_FEATURE_SEPERATE_FD_USE	
	EGLNativePixmapType target_native_pixmap;
	EGLSurface			target_pixmap_surface;
#else
	EGLNativePixmapType target_native_pixmaps[VR_INPUT_MODE_YUV_MAX];
	EGLSurface			target_pixmap_surfaces[VR_INPUT_MODE_YUV_MAX];
#endif
};

struct vrSurfaceBoundTarget
{
	EGLImageKHR eglImage;
	unsigned int texture_name;
};

struct vrSurfaceSource
{
#ifdef VR_FEATURE_STRIDE_USE
	unsigned int		stride;
#endif
	unsigned int        width        ;
	unsigned int        height       ;
	unsigned int        total_texture_src_height[VR_INPUT_MODE_YUV_MAX];
	GLuint              texture_name[VR_INPUT_MODE_YUV_MAX] ;
	/* 
	case VR_FEATURE_SEPERATE_FD_USE => use Y,U,V
	*/	
	EGLNativePixmapType src_native_pixmaps[VR_INPUT_MODE_YUV_MAX];
	EGLImageKHR         src_pixmap_images[VR_INPUT_MODE_YUV_MAX];
};

static int vrInitializeDeinterlace( HSURFTARGET hTarget );
static int vrInitializeScaler( HSURFTARGET hTarget );
static int vrInitializeCvt2Y( HSURFTARGET hTarget );
static int vrInitializeCvt2UV( HSURFTARGET hTarget );
static int vrInitializeCvt2Rgba( HSURFTARGET hTarget );
static int vrInitializeUserFilter( HSURFTARGET hTarget);
static int vrInitializeYuvFilter( HSURFTARGET hTarget);
static int vrInitializeScaleRGBA( HSURFTARGET hTarget );
static int vrDeinitializeDeinterlace( HSURFTARGET hTarget );
static int vrDeinitializeScaler( HSURFTARGET hTarget );
static int vrDeinitializeCvt2Y( HSURFTARGET hTarget );
static int vrDeinitializeCvt2UV( HSURFTARGET hTarget );
static int vrDeinitializeCvt2Rgba( HSURFTARGET hTarget );
static int vrDeinitializeUserFilter( HSURFTARGET hTarget);
static int vrDeinitializeYuvFilter( HSURFTARGET hTarget);
static int vrDeinitializeScaleRGBA( HSURFTARGET hTarget);
static char *loadShader( const char *sFilename);
static int processShader(GLuint *pShader, const char *sFilename, GLint iShaderType);
static HSURFTARGET nxGSurfaceCreateCvt2YTargetDefault  (unsigned int uiStride, unsigned int uiWidth, unsigned int uiHeight, NX_MEMORY_HANDLE hData);
static HSURFTARGET nxGSurfaceCreateCvt2UVTargetDefault  (unsigned int uiStride, unsigned int uiWidth, unsigned int uiHeight, NX_MEMORY_HANDLE hData);
static void vrResetShaderInfo(Shader* pShader);
static void vrWaitForDone      ( void );


static int VR_GRAPHIC_DBG_OPEN_CLOSE = 1;
static int VR_GRAPHIC_DBG_TARGET = 1;
static int VR_GRAPHIC_DBG_SOURCE = 1;
static int VR_GRAPHIC_DBG_CTX = 1;
static int VR_GRAPHIC_DBG_RUN = 1;
static int VR_GRAPHIC_DBG_HEIGHT_ALIGN = 1;

#ifdef VR_FEATURE_SEPERATE_FD_USE
	#if 1 //only for scaler(because this usage is handle pointer case)
	#define VR_YUV_CNT  3
	#else
	#define VR_YUV_CNT  1
	#endif
#else
	#define VR_YUV_CNT 	3 
	#define VR_Y_UV_CNT 2
#endif
static int nxGSurfaceCreateDefaultSurface( EGLDisplay display )
{
	Statics* pStatics = &s_Statics;
	if( !pStatics ){ return -1; }

	pStatics->default_target_memory[VR_PROGRAM_DEINTERLACE] = NX_AllocateMemory(64*64, 4);		
	pStatics->default_target_memory[VR_PROGRAM_SCALE] = NX_AllocateMemory(64*64, 4);		
	pStatics->default_target_memory[VR_PROGRAM_CVT2Y] = NX_AllocateMemory(64*64*4, 4);		
	pStatics->default_target_memory[VR_PROGRAM_CVT2UV] = NX_AllocateMemory(64*64*4, 4);		
	pStatics->default_target_memory[VR_PROGRAM_CVT2RGBA] = NX_AllocateMemory(64*64*4, 4);		
	#ifdef VR_FEATURE_SEPERATE_FD_USE	
	#else
	pStatics->default_target_memory[VR_PROGRAM_USERFILTER] = NX_AllocateMemory(64*64*4, 4);		
	#endif
	pStatics->default_target_memory[VR_PROGRAM_YUVFILTER_Y] = NX_AllocateMemory(64*64*4, 4);		
	pStatics->default_target_memory[VR_PROGRAM_SCALE_RGBA] = NX_AllocateMemory(64*64*4, 4);		

	VR_INFO("", VR_GRAPHIC_DBG_OPEN_CLOSE, "nxGSurfaceCreateDefaultSurface end\n"); 	

	return 0;
}

static void  nxGSurfaceDestroyDefaultSurface( void )
{
	Statics *pStatics = vrGetStatics();
	if( !pStatics ){ return; }
	NX_FreeMemory( pStatics->default_target_memory[VR_PROGRAM_DEINTERLACE] );
	NX_FreeMemory( pStatics->default_target_memory[VR_PROGRAM_SCALE] );
	NX_FreeMemory( pStatics->default_target_memory[VR_PROGRAM_CVT2Y] );
	NX_FreeMemory( pStatics->default_target_memory[VR_PROGRAM_CVT2UV] );
	NX_FreeMemory( pStatics->default_target_memory[VR_PROGRAM_CVT2RGBA] );
	#ifdef VR_FEATURE_SEPERATE_FD_USE	
	#else
	NX_FreeMemory( pStatics->default_target_memory[VR_PROGRAM_USERFILTER] );
	#endif
	NX_FreeMemory( pStatics->default_target_memory[VR_PROGRAM_YUVFILTER_Y] );
	NX_FreeMemory( pStatics->default_target_memory[VR_PROGRAM_SCALE_RGBA] );

	pStatics->default_target_memory[VR_PROGRAM_DEINTERLACE] = NULL;
	pStatics->default_target_memory[VR_PROGRAM_SCALE] = NULL;
	pStatics->default_target_memory[VR_PROGRAM_CVT2Y] = NULL;
	pStatics->default_target_memory[VR_PROGRAM_CVT2UV] = NULL;
	pStatics->default_target_memory[VR_PROGRAM_CVT2RGBA] = NULL;
#ifdef VR_FEATURE_SEPERATE_FD_USE	
#else
	pStatics->default_target_memory[VR_PROGRAM_USERFILTER] = NULL;
#endif
	pStatics->default_target_memory[VR_PROGRAM_YUVFILTER_Y] = NULL;
	pStatics->default_target_memory[VR_PROGRAM_SCALE_RGBA] = NULL;

	
	VR_INFO("", VR_GRAPHIC_DBG_OPEN_CLOSE, "nxGSurfaceDestroyDefaultSurface done ===>\n"); 
}

EGLBoolean nxGSurfaceCreateGraphicSurface( EGLDisplay display )
{
	int ret;
	if( s_Initialized )
	{ 
		return EGL_TRUE; 
	}

	vrInitializeStatics();
	
	VR_INFO("\n", VR_GRAPHIC_DBG_OPEN_CLOSE, "nxGSurfaceCreateDefaultSurface start <===\n");	
	ret = vrInitializeEGLConfig(display);
	if( ret ){ NxErrMsg("Error: nxGSurfaceCreateDefaultSurface at %s:%i\n", __FILE__, __LINE__); return ret; }
			
	ret = nxGSurfaceCreateDefaultSurface(display);
	if( ret ){ NxErrMsg("Error: nxGSurfaceCreateGraphicSurface at %s:%i\n", __FILE__, __LINE__); return ret; }

	Statics* pStatics = &s_Statics;
	if( !pStatics ){ return EGL_FALSE; }
		
	VR_INFO("", VR_GRAPHIC_DBG_OPEN_CLOSE, "nxGSurfaceCreateGraphicSurface start <=\n"); 	
	ret = vrInitEglExtensions();
	if( ret ){ NxErrMsg("Error: nxGSurfaceCreateGraphicSurface at %s:%i\n", __FILE__, __LINE__); return ret; }	

	if(!pStatics->default_target[VR_PROGRAM_DEINTERLACE])
	{
		#ifdef VR_FEATURE_SEPERATE_FD_USE	
		pStatics->default_target[VR_PROGRAM_DEINTERLACE] = nxGSurfaceCreateDeinterlaceTarget(64, 64, pStatics->default_target_memory[VR_PROGRAM_DEINTERLACE], VR_TRUE);
		#else
		pStatics->default_target[VR_PROGRAM_DEINTERLACE] = nxGSurfaceCreateDeinterlaceTarget(0, 64, 64, pStatics->default_target_memory[VR_PROGRAM_DEINTERLACE], VR_TRUE);
		#endif
	}
	if(!pStatics->default_target[VR_PROGRAM_SCALE])
	{		
		#ifdef VR_FEATURE_SEPERATE_FD_USE	
		nxGSurfaceCreateScaleTarget(&pStatics->default_target[VR_PROGRAM_SCALE], 64, 64, &pStatics->default_target_memory[VR_PROGRAM_SCALE], VR_TRUE);
		#else
		pStatics->default_target[VR_PROGRAM_SCALE] = nxGSurfaceCreateScaleTarget(0, 64, 64, pStatics->default_target_memory[VR_PROGRAM_SCALE], VR_TRUE);
		#endif
	}
	if(!pStatics->default_target[VR_PROGRAM_YUVFILTER_Y])
	{		
		#ifdef VR_FEATURE_SEPERATE_FD_USE	
		nxGSurfaceCreateYuvFilterTarget(&pStatics->default_target[VR_PROGRAM_YUVFILTER_Y], 64, 64, &pStatics->default_target_memory[VR_PROGRAM_YUVFILTER_Y], VR_TRUE);
		#else
		pStatics->default_target[VR_PROGRAM_YUVFILTER_Y] = nxGSurfaceCreateYuvFilterTarget(0, 64, 64, pStatics->default_target_memory[VR_PROGRAM_YUVFILTER_Y], VR_TRUE);
		#endif
	}

	#ifdef VR_FEATURE_SEPERATE_FD_USE	
	if(!pStatics->default_target[VR_PROGRAM_CVT2Y])
		pStatics->default_target[VR_PROGRAM_CVT2Y] = nxGSurfaceCreateCvt2YTarget(64, 64, pStatics->default_target_memory[VR_PROGRAM_CVT2Y], VR_TRUE);
	if(!pStatics->default_target[VR_PROGRAM_CVT2UV])
		pStatics->default_target[VR_PROGRAM_CVT2UV] = nxGSurfaceCreateCvt2UVTarget(64, 64, pStatics->default_target_memory[VR_PROGRAM_CVT2UV], VR_TRUE);
	if(!pStatics->default_target[VR_PROGRAM_CVT2RGBA])
		pStatics->default_target[VR_PROGRAM_CVT2RGBA] = nxGSurfaceCreateCvt2RgbaTarget(64, 64, pStatics->default_target_memory[VR_PROGRAM_CVT2RGBA], VR_TRUE);
	#else
	if(!pStatics->default_target[VR_PROGRAM_CVT2Y])
		pStatics->default_target[VR_PROGRAM_CVT2Y] = nxGSurfaceCreateCvt2YTargetDefault(0, 64, 64, pStatics->default_target_memory[VR_PROGRAM_CVT2Y]);
	if(!pStatics->default_target[VR_PROGRAM_CVT2UV])
		pStatics->default_target[VR_PROGRAM_CVT2UV] = nxGSurfaceCreateCvt2UVTargetDefault(0, 64, 64, pStatics->default_target_memory[VR_PROGRAM_CVT2UV]);
	if(!pStatics->default_target[VR_PROGRAM_CVT2RGBA])
		pStatics->default_target[VR_PROGRAM_CVT2RGBA] = nxGSurfaceCreateCvt2RgbaTarget(0, 64, 64, pStatics->default_target_memory[VR_PROGRAM_CVT2RGBA], VR_TRUE);
	if(!pStatics->default_target[VR_PROGRAM_USERFILTER])
		pStatics->default_target[VR_PROGRAM_USERFILTER] = nxGSurfaceCreateUserFilterTarget(0, 64, 64, pStatics->default_target_memory[VR_PROGRAM_USERFILTER], VR_TRUE);
	#endif
	if(!pStatics->default_target[VR_PROGRAM_SCALE_RGBA])
		pStatics->default_target[VR_PROGRAM_SCALE_RGBA] = nxGSurfaceCreateScaleRGBATarget(0, 64, 64, pStatics->default_target_memory[VR_PROGRAM_SCALE_RGBA], VR_TRUE);
	
	for(unsigned int program = 0 ; program < VR_PROGRAM_MAX ; program++)
	{
		pStatics->shader[program].iVertName = 0;
		pStatics->shader[program].iFragName = 0;
		pStatics->shader[program].iProgName = 0;
		pStatics->shader[program].iLocPosition = -1;
		pStatics->shader[program].iLocTexCoord[0] = -1;
		pStatics->shader[program].iLocTexCoord[1] = -1;
		pStatics->shader[program].iLocTexCoord[2] = -1;
		pStatics->shader[program].iLocInputWidth = -1;
		pStatics->shader[program].iLocInputHeight = -1;
		pStatics->shader[program].iLocOutputHeight = -1;
		pStatics->shader[program].iLocMainTex[0] = -1;
		pStatics->shader[program].iLocMainTex[1] = -1;
		pStatics->shader[program].iLocMainTex[2] = -1;
		pStatics->shader[program].iLocRefTex = -1;
		pStatics->shader[program].iLocTexInvSize = -1;
		pStatics->shader[program].iLocFilterGain = -1;
	}	
 	pStatics->tex_deinterlace_ref_id = 0;

	if(vrInitializeDeinterlace( pStatics->default_target[VR_PROGRAM_DEINTERLACE] ) != 0)
		return EGL_FALSE;
	if(vrInitializeScaler(pStatics->default_target[VR_PROGRAM_SCALE] ) != 0)
		return EGL_FALSE;
	if(vrInitializeCvt2Y(pStatics->default_target[VR_PROGRAM_CVT2Y] ) != 0)
		return EGL_FALSE;
	if(vrInitializeCvt2UV(pStatics->default_target[VR_PROGRAM_CVT2UV] ) != 0)
		return EGL_FALSE;
	if(vrInitializeCvt2Rgba(pStatics->default_target[VR_PROGRAM_CVT2RGBA] ) != 0)
		return EGL_FALSE;
	if(vrInitializeScaleRGBA(pStatics->default_target[VR_PROGRAM_SCALE_RGBA] ) != 0)
		return EGL_FALSE;
	#ifdef VR_FEATURE_SEPERATE_FD_USE
	#else
	if(vrInitializeUserFilter(pStatics->default_target[VR_PROGRAM_USERFILTER] ) != 0)
		return EGL_FALSE;
	#endif
	if(vrInitializeYuvFilter(pStatics->default_target[VR_PROGRAM_YUVFILTER_Y] ) != 0)
		return EGL_FALSE;
		
	s_Initialized = true;

	VR_INFO("", VR_GRAPHIC_DBG_OPEN_CLOSE, "nxGSurfaceCreateGraphicSurface end\n"); 	
	return EGL_TRUE;
}

void  nxGSurfaceDestroyGraphicSurface( void )
{	
	if( ! s_Initialized ){ return; }

	Statics *pStatics = vrGetStatics();
	if( !pStatics ){ return; }

	VR_INFO("", VR_GRAPHIC_DBG_OPEN_CLOSE, "nxGSurfaceDestroyGraphicSurface start\n"); 	

	if(vrDeinitializeDeinterlace(pStatics->default_target[VR_PROGRAM_DEINTERLACE]) != 0)
		NxErrMsg("Error: vrDeinitializeDeinterlace() %s:%i\n", __FILE__, __LINE__);
	if(vrDeinitializeScaler(pStatics->default_target[VR_PROGRAM_SCALE]) != 0)
		NxErrMsg("Error: vrDeinitializeScaler() %s:%i\n", __FILE__, __LINE__);
	if(vrDeinitializeCvt2Y(pStatics->default_target[VR_PROGRAM_CVT2Y]) != 0)
		NxErrMsg("Error: vrDeinitializeCvt2Y() %s:%i\n", __FILE__, __LINE__);
	if(vrDeinitializeCvt2UV(pStatics->default_target[VR_PROGRAM_CVT2UV]) != 0)
		NxErrMsg("Error: vrDeinitializeCvt2UV() %s:%i\n", __FILE__, __LINE__);
	if(vrDeinitializeCvt2Rgba(pStatics->default_target[VR_PROGRAM_CVT2RGBA]) != 0)
		NxErrMsg("Error: vrDeinitializeCvt2Rgba() %s:%i\n", __FILE__, __LINE__);
	#ifdef VR_FEATURE_SEPERATE_FD_USE			
	#else
	if(vrDeinitializeUserFilter(pStatics->default_target[VR_PROGRAM_USERFILTER]) != 0)
		NxErrMsg("Error: vrDeinitializeUserFilter() %s:%i\n", __FILE__, __LINE__);
	#endif	
	if(vrDeinitializeYuvFilter(pStatics->default_target[VR_PROGRAM_YUVFILTER_Y]) != 0)
		NxErrMsg("Error: vrDeinitializeYuvFilter() %s:%i\n", __FILE__, __LINE__);
	if(vrDeinitializeScaleRGBA(pStatics->default_target[VR_PROGRAM_SCALE_RGBA]) != 0)
		NxErrMsg("Error: vrDeinitializeScaleRGBA() %s:%i\n", __FILE__, __LINE__);

	for(int i = 0 ; i < VR_PROGRAM_MAX ; i++)
	{
		if(pStatics->egl_info.sEGLContext[i])
			NxErrMsg("ERROR: nxGSurfaceDestroyGraphicSurface(0x%x), idx(%d), ref(%d)\n", (int)pStatics->egl_info.sEGLContext[i], i, pStatics->egl_info.sEGLContextRef[i]);
		VR_ASSERT("ctx_ref must be zero", !pStatics->egl_info.sEGLContextRef[i]);
	}
	
	if(pStatics->default_target[VR_PROGRAM_DEINTERLACE])	
		nxGSurfaceDestroyDeinterlaceTarget( pStatics->default_target[VR_PROGRAM_DEINTERLACE], VR_TRUE );
	if(pStatics->default_target[VR_PROGRAM_SCALE])	
	{	
		#ifdef VR_FEATURE_SEPERATE_FD_USE
		nxGSurfaceDestroyScaleTarget( &pStatics->default_target[VR_PROGRAM_SCALE], VR_TRUE );
		#else
		nxGSurfaceDestroyScaleTarget( pStatics->default_target[VR_PROGRAM_SCALE], VR_TRUE );
		#endif
	}
	if(pStatics->default_target[VR_PROGRAM_YUVFILTER_Y])	
	{	
	#ifdef VR_FEATURE_SEPERATE_FD_USE
		nxGSurfaceDestroyYuvFilterTarget( &pStatics->default_target[VR_PROGRAM_YUVFILTER_Y], VR_TRUE );
	#else
		nxGSurfaceDestroyYuvFilterTarget( pStatics->default_target[VR_PROGRAM_YUVFILTER_Y], VR_TRUE );
	#endif
	}
	#ifdef VR_FEATURE_SEPERATE_FD_USE
	if(pStatics->default_target[VR_PROGRAM_CVT2Y])	
		nxGSurfaceDestroyCvt2YTarget( pStatics->default_target[VR_PROGRAM_CVT2Y], VR_TRUE );
	if(pStatics->default_target[VR_PROGRAM_CVT2UV])	
		nxGSurfaceDestroyCvt2UVTarget( pStatics->default_target[VR_PROGRAM_CVT2UV], VR_TRUE );
	#else
	if(pStatics->default_target[VR_PROGRAM_CVT2Y])	
		nxGSurfaceDestroyCvt2YuvTarget( pStatics->default_target[VR_PROGRAM_CVT2Y], VR_TRUE );
	if(pStatics->default_target[VR_PROGRAM_CVT2UV])	
		nxGSurfaceDestroyCvt2YuvTarget( pStatics->default_target[VR_PROGRAM_CVT2UV], VR_TRUE );
	#endif	
	if(pStatics->default_target[VR_PROGRAM_CVT2RGBA])	
		nxGSurfaceDestroyCvt2RgbaTarget( pStatics->default_target[VR_PROGRAM_CVT2RGBA], VR_TRUE );
	#ifdef VR_FEATURE_SEPERATE_FD_USE	
	#else
	if(pStatics->default_target[VR_PROGRAM_USERFILTER])	
		nxGSurfaceDestroyUserFilterTarget( pStatics->default_target[VR_PROGRAM_USERFILTER], VR_TRUE );
	#endif	
	if(pStatics->default_target[VR_PROGRAM_SCALE_RGBA])	
		nxGSurfaceDestroyScaleRGBATarget( pStatics->default_target[VR_PROGRAM_SCALE_RGBA], VR_TRUE );

	pStatics->default_target[VR_PROGRAM_DEINTERLACE] = NULL;
	pStatics->default_target[VR_PROGRAM_SCALE] = NULL;
	pStatics->default_target[VR_PROGRAM_CVT2Y] = NULL;
	pStatics->default_target[VR_PROGRAM_CVT2UV] = NULL;
	pStatics->default_target[VR_PROGRAM_CVT2RGBA] = NULL;	
	pStatics->default_target[VR_PROGRAM_USERFILTER] = NULL;	
	pStatics->default_target[VR_PROGRAM_YUVFILTER_Y] = NULL;	
	pStatics->default_target[VR_PROGRAM_SCALE_RGBA] = NULL;	
	VR_INFO("", VR_GRAPHIC_DBG_OPEN_CLOSE, "nxGSurfaceDestroyGraphicSurface end =>\n"); 

	nxGSurfaceDestroyDefaultSurface();	

	s_Initialized = false;	
}

int vrGSurfaceInitDisplay(unsigned int uiWidth, unsigned int uiHeight)
{
	Statics* pStatics = vrGetStatics();
	unsigned int program = VR_PROGRAM_DISPLAY;
	
	VR_INFO("", VR_GRAPHIC_DBG_CTX, "vrGSurfaceInitDisplay start\n"); 

	if(nxGSurfaceCreateEGLContext(program) != 0)
	{
		NxErrMsg("Error: Fail to create context %s:%i\n", __FILE__, __LINE__);
		return -1;	
	}

	pStatics->platform_win = vrPlatformCreateNativeWindow(uiWidth, uiHeight);
	if ( !pStatics->platform_win )
	{
		NxErrMsg("Error: Fail to create window %s:%i\n", __FILE__, __LINE__);
		return -1;			
	}

	/* Create a EGLSurface. */
	pStatics->display_surface = eglCreateWindowSurface(pStatics->egl_info.sEGLDisplay, 
			pStatics->egl_info.sEGLConfig[program], (EGLNativeWindowType)pStatics->platform_win, NULL);
	if(pStatics->display_surface == EGL_NO_SURFACE)
	{
		EGLint iError = eglGetError();
		NxErrMsg("eglGetError(): %i (0x%.4x)\n", (int)iError, (int)iError);
		NxErrMsg("Error: Failed to create EGL surface at %s:%i\n", __FILE__, __LINE__);
		vrPlatformDestroyNativeWindow(pStatics->platform_win);
		pStatics->platform_win = NULL;
		return -1;
	}
	
	/* Make context current. */
	EGLBoolean bResult = eglMakeCurrent(pStatics->egl_info.sEGLDisplay, pStatics->display_surface, 
							pStatics->display_surface, pStatics->egl_info.sEGLContext[program]);
	if(bResult == EGL_FALSE)
	{
		EGLint iError = eglGetError();
		NxErrMsg("eglGetError(): %i (0x%.4x)\n", (int)iError, (int)iError);
		NxErrMsg("Error: Failed to make context current at %s:%i\n", __FILE__, __LINE__);
		vrPlatformDestroyNativeWindow(pStatics->platform_win);
		pStatics->platform_win = NULL;
		return -1;	
	}	
	
	/* Load shaders. */
	if(processShader(&pStatics->shader[program].iVertName, VERTEX_SHADER_SOURCE_DISPLAY, GL_VERTEX_SHADER) < 0)
	{
		NxErrMsg("Error: wrong shader %s:%i\n", __FILE__, __LINE__);
		vrPlatformDestroyNativeWindow(pStatics->platform_win);
		pStatics->platform_win = NULL;
		return -1;
	}
	if(processShader(&pStatics->shader[program].iFragName, FRAGMENT_SHADER_SOURCE_DISPLAY, GL_FRAGMENT_SHADER) < 0)
	{
		NxErrMsg("Error: wrong shader %s:%i\n", __FILE__, __LINE__);
		vrPlatformDestroyNativeWindow(pStatics->platform_win);
		pStatics->platform_win = NULL;
		return -1;
	}	

	/* Set up shaders. */
	pStatics->shader[program].iProgName = GL_CHECK(glCreateProgram());
	//NxDbgMsg("Deinterlace iProgName(%d)\n", pStatics->shader[program].iProgName);		
	GL_CHECK(glAttachShader(pStatics->shader[program].iProgName, pStatics->shader[program].iVertName));
	GL_CHECK(glAttachShader(pStatics->shader[program].iProgName, pStatics->shader[program].iFragName));
	GL_CHECK(glLinkProgram(pStatics->shader[program].iProgName));
	GL_CHECK(glUseProgram(pStatics->shader[program].iProgName));
	
	/* Vertex positions. */
	pStatics->shader[program].iLocPosition = GL_CHECK(glGetAttribLocation(pStatics->shader[program].iProgName, "a_v4Position"));
	if(pStatics->shader[program].iLocPosition == -1)
	{
		NxErrMsg("Error: Attribute not found at %s:%i\n", __FILE__, __LINE__);
		vrPlatformDestroyNativeWindow(pStatics->platform_win);
		pStatics->platform_win = NULL;
		return -1;
	}
	//else GL_CHECK(glEnableVertexAttribArray(pStatics->shader[program].iLocPosition));

	/* Color positions. */
	pStatics->shader[program].iLocColor = GL_CHECK(glGetAttribLocation(pStatics->shader[program].iProgName, "a_v4Color"));
	if(pStatics->shader[program].iLocColor == -1)
	{
		NxErrMsg("Error: Attribute not found at %s:%i\n", __FILE__, __LINE__);
		vrPlatformDestroyNativeWindow(pStatics->platform_win);
		pStatics->platform_win = NULL;
		return -1;
	}
	//else GL_CHECK(glEnableVertexAttribArray(pStatics->shader[program].iLocPosition));

	/* Fill texture. */
	pStatics->shader[program].iLocTexCoord[0] = GL_CHECK(glGetAttribLocation(pStatics->shader[program].iProgName, "a_v2TexCoord"));
	if(pStatics->shader[program].iLocTexCoord[0] == -1)
	{
		NxErrMsg("Warning: Attribute not found at %s:%i\n", __FILE__, __LINE__);
		vrPlatformDestroyNativeWindow(pStatics->platform_win);
		pStatics->platform_win = NULL;
		return -1;
	}
	//else GL_CHECK(glEnableVertexAttribArray(pStatics->shader[program].iLocTexCoord));

	/* diffuse texture. */
	pStatics->shader[program].iLocMainTex[0] = GL_CHECK(glGetUniformLocation(pStatics->shader[program].iProgName, "diffuse"));
	if(pStatics->shader[program].iLocMainTex[0] == -1)
	{
		NxErrMsg("Warning: Uniform not found at %s:%i\n", __FILE__, __LINE__);
	}
	else 
	{
		//GL_CHECK(glUniform1i(pStatics->shader[program].iLocMainTex[0], VR_INPUT_MODE_DEINTERLACE));
	}	

	//set texture
	GL_CHECK(glGenTextures(1, &pStatics->tex_display_id));
	GL_CHECK(glActiveTexture(GL_TEXTURE0 + VR_INPUT_MODE_DISPLAY));
	GL_CHECK(glBindTexture(GL_TEXTURE_2D, pStatics->tex_display_id));
	GL_CHECK(glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST));
	GL_CHECK(glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST));
	GL_CHECK(glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT));
	GL_CHECK(glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT)); 
		
	VR_INFO("", VR_GRAPHIC_DBG_CTX, "vrInitializeDeinterlace end\n");	
	return 0;			
}

void nxGSurfaceRunDisplay(unsigned int uiDstX, unsigned int uiDstY, unsigned int uiDstWidth, unsigned int uiDstHeight, 
					unsigned int uiSrcWidth, unsigned int uiSrcHeight, void* pdata)
{
	Statics* pStatics = vrGetStatics();
	unsigned int program = VR_PROGRAM_DISPLAY;	
	#if 1
	const float aSquareVertex[] =
	{
		-1.0f,	-1.0f, 0.0f,
		-1.0f, 1.0f, 0.0f,
		 1.0f, 1.0f, 0.0f,
		 1.0f,	-1.0f, 0.0f,
	};	
	#else	
	const float aSquareVertex[] =
	{
		-1.0f,	-1.0f, 0.0f,
		-1.0f, 0.5f, 0.0f,
		 0.5f, 0.5f, 0.0f,
		 0.5f,	-1.0f, 0.0f,
	};	
	#endif
	float aSquareTexCoord[] =
	{
		0.0f, 1.0f,
		0.0f, 0.0f,
		1.0f, 0.0f,
		1.0f, 1.0f
	};
	float aColor[] = {1.f, 1.f, 1.f, 1.f, 
					1.f, 1.f, 1.f, 1.f, 
					1.f, 1.f, 1.f, 1.f, 
					1.f, 1.f, 1.f, 1.f
	};
	
	if( NULL == pStatics  )
	{
		NxErrMsg("Error: pStatics is NULL! %s:%i\n", __FILE__, __LINE__);
		return;
	}
	VR_INFO("", VR_GRAPHIC_DBG_RUN, "%s start\n", __FUNCTION__);
	
	_AUTO_BACKUP_CURRENT_EGL_;

	Shader* pshader = &(vrGetStatics()->shader[program]); 
	VR_ASSERT("Error: display_surface must exist", pStatics->display_surface);						

	/* Make context current. */
	EGLBoolean bResult = eglMakeCurrent(pStatics->egl_info.sEGLDisplay, 
									pStatics->display_surface, pStatics->display_surface, 
									pStatics->egl_info.sEGLContext[program]);
	if(bResult == EGL_FALSE)
	{
		EGLint iError = eglGetError();
		NxErrMsg("eglGetError(): %i (0x%.4x)\n", (int)iError, (int)iError);
		NxErrMsg("Error: Failed to make context current at %s:%i\n", __FILE__, __LINE__);
		return; 
	}

	GL_CHECK(glUseProgram(pshader->iProgName)); 	
	int x, y, width, height;
	x = uiDstX;
	y = uiDstY;
	width  = uiDstWidth;
	height = uiDstHeight;
	GL_CHECK(glViewport(x,y,width,height));
	
	glClearColor(0.f, 1.f, 0.f, 0.f);

	GL_CHECK(glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT)); // To optimize for tile-based renderer
	GL_CHECK(glVertexAttribPointer(pshader->iLocPosition, 3, GL_FLOAT, GL_FALSE, 0, aSquareVertex));
	GL_CHECK(glVertexAttribPointer(pshader->iLocColor, 4, GL_FLOAT, GL_FALSE, 0, aColor));
	GL_CHECK(glVertexAttribPointer(pshader->iLocTexCoord[0], 2, GL_FLOAT, GL_FALSE, 0, aSquareTexCoord));

	GL_CHECK(glEnableVertexAttribArray(pshader->iLocPosition));
	GL_CHECK(glEnableVertexAttribArray(pshader->iLocColor));
	GL_CHECK(glEnableVertexAttribArray(pshader->iLocTexCoord[0]));

	GL_CHECK(glUniform1i(pshader->iLocMainTex[0], VR_INPUT_MODE_DISPLAY));

	//GL_CHECK(glActiveTexture(GL_TEXTURE0 + VR_INPUT_MODE_DISPLAY));
	//GL_CHECK(glBindTexture(GL_TEXTURE_2D, pStatics->tex_display_id));
	//set texture
	GL_CHECK(glGenTextures(1, &pStatics->tex_display_id));
	GL_CHECK(glActiveTexture(GL_TEXTURE0 + VR_INPUT_MODE_DISPLAY));
	GL_CHECK(glBindTexture(GL_TEXTURE_2D, pStatics->tex_display_id));
	GL_CHECK(glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST));
	GL_CHECK(glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST));
	GL_CHECK(glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT));
	GL_CHECK(glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT)); 

	//memset(pdata, 0xCC, uiWidth * uiHeight * 4);
	
	GL_CHECK(glTexImage2D(GL_TEXTURE_2D,0,GL_RGBA,
			 uiSrcWidth,uiSrcHeight,0,
			 GL_RGBA,GL_UNSIGNED_BYTE,pdata));	

	GL_CHECK(glDrawArrays(GL_TRIANGLE_FAN, 0, 4));		

	VR_INFO("", VR_GRAPHIC_DBG_RUN, "draw UserFilter, (%d,%d) %dx%d\n", x, y, width, height );
}

void vrGSurfaceSwapDisplay(void)
{
	Statics* pStatics = vrGetStatics();
	EGL_CHECK(eglSwapBuffers(pStatics->egl_info.sEGLDisplay, pStatics->display_surface));
}

#if 1
void nxGSurfaceRunStatusBar(int status_value, float start_x, float end_x)
{
	Statics* pStatics = vrGetStatics();
	unsigned int program = VR_PROGRAM_DISPLAY;	
#if 1
	float aSquareVertex[] =
	{
		-1.0f,	-1.0f, 0.0f,
		-1.0f, 1.0f, 0.0f,
		 1.0f, 1.0f, 0.0f,
		 1.0f,	-1.0f, 0.0f,
	};	
#else	
	const float aSquareVertex[] =
	{
		-1.0f,	-1.0f, 0.0f,
		-1.0f, 0.5f, 0.0f,
		 0.5f, 0.5f, 0.0f,
		 0.5f,	-1.0f, 0.0f,
	};	
#endif
	float aSquareTexCoord[] =
	{
		0.0f, 1.0f,
		0.0f, 0.0f,
		1.0f, 0.0f,
		1.0f, 1.0f
	};
	float aColor[] = {0.f, 1.f, 0.f, 1.f, 
					0.f, 1.f, 0.f, 1.f, 
					0.f, 1.f, 0.f, 1.f, 
					0.f, 1.f, 0.f, 1.f
	};

	//start_x=0 	=> -1
	//start_x=0.5 => 0
	//start_x=1 	=> 1

	float edge_offset = 0.01f, bar_height = 0.02f;
#if 1
	float start = (start_x*2.f) - 1.f;
	float end = (end_x*2.f) - 1.f;
	//xmin +edge_offset	
	aSquareVertex[0] = aSquareVertex[3] = start + edge_offset;
	//xmax -edge_offset
	aSquareVertex[6] = aSquareVertex[9] = end - edge_offset;
#else //temp test
	float start = (end_x*2.f) - 1.f;
	float end = (start_x*2.f) - 1.f;
	//xmin +edge_offset 
	aSquareVertex[0] = aSquareVertex[3] = start + edge_offset;
	//xmax -edge_offset
	aSquareVertex[6] = aSquareVertex[9] = end - edge_offset;
#endif
	//ymin +edge_offset
	aSquareVertex[4] = aSquareVertex[7] = aSquareVertex[4] - edge_offset;
	//y height
	aSquareVertex[1] = aSquareVertex[10] = aSquareVertex[1] + ((2.f - (edge_offset*2.f)) - bar_height);

	if( NULL == pStatics  )
	{
		NxErrMsg("Error: pStatics is NULL! %s:%i\n", __FILE__, __LINE__);
		return;
	}
	VR_INFO("", VR_GRAPHIC_DBG_RUN, "%s start\n", __FUNCTION__);
	
	_AUTO_BACKUP_CURRENT_EGL_;

	Shader* pshader = &(vrGetStatics()->shader[program]); 
	VR_ASSERT("Error: display_surface must exist", pStatics->display_surface);						

/*
	int x, y, width, height;
	x = uiX;
	y = uiY;
	width  = uiWidth;
	height = uiHeight;
	//GL_CHECK(glViewport(x,y,width,height));
	*/
	
	GL_CHECK(glVertexAttribPointer(pshader->iLocPosition, 3, GL_FLOAT, GL_FALSE, 0, aSquareVertex));
	GL_CHECK(glVertexAttribPointer(pshader->iLocColor, 4, GL_FLOAT, GL_FALSE, 0, aColor));
	GL_CHECK(glVertexAttribPointer(pshader->iLocTexCoord[0], 2, GL_FLOAT, GL_FALSE, 0, aSquareTexCoord));

	GL_CHECK(glEnableVertexAttribArray(pshader->iLocPosition));
	GL_CHECK(glEnableVertexAttribArray(pshader->iLocColor));
	GL_CHECK(glEnableVertexAttribArray(pshader->iLocTexCoord[0]));

	GL_CHECK(glUniform1i(pshader->iLocMainTex[0], VR_INPUT_MODE_DISPLAY));

	//GL_CHECK(glActiveTexture(GL_TEXTURE0 + VR_INPUT_MODE_DISPLAY));
	//GL_CHECK(glBindTexture(GL_TEXTURE_2D, pStatics->tex_display_id));
	//set texture
	GL_CHECK(glGenTextures(1, &pStatics->tex_display_id));
	GL_CHECK(glActiveTexture(GL_TEXTURE0 + VR_INPUT_MODE_DISPLAY));
	GL_CHECK(glBindTexture(GL_TEXTURE_2D, pStatics->tex_display_id));
	GL_CHECK(glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST));
	GL_CHECK(glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST));
	GL_CHECK(glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT));
	GL_CHECK(glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT)); 

	//memset(pdata, 0xCC, uiWidth * uiHeight * 4);
	unsigned int tex_data = 0xFFFFFFFF;
	GL_CHECK(glTexImage2D(GL_TEXTURE_2D,0,GL_RGBA,
			 1,1,0,
			 GL_RGBA,GL_UNSIGNED_BYTE,&tex_data));	

	GL_CHECK(glDrawArrays(GL_TRIANGLE_FAN, 0, 4));		

	EGL_CHECK(eglSwapBuffers(pStatics->egl_info.sEGLDisplay, pStatics->display_surface));
	VR_INFO("", VR_GRAPHIC_DBG_RUN, "draw nxGSurfaceRunStatusBar\n");
}

#endif

void vrGSurfaceDeinitDisplay(void)
{
	Statics* pStatics = vrGetStatics();
	int ret = 0;
	unsigned int program = VR_PROGRAM_DISPLAY;
	EGLBoolean bResult = eglMakeCurrent(pStatics->egl_info.sEGLDisplay, pStatics->display_surface, 
								pStatics->display_surface, pStatics->egl_info.sEGLContext[program]);
	if(bResult == EGL_FALSE)
	{
		EGLint iError = eglGetError();
		NxErrMsg("eglGetError(): %i (0x%.4x)\n", (int)iError, (int)iError);
		NxErrMsg("Error: Failed to make context current at %s:%i\n", __FILE__, __LINE__);
		return; 
	}

	GL_CHECK(glDeleteTextures(1,&pStatics->tex_display_id));
	GL_CHECK(glDeleteShader(pStatics->shader[program].iVertName	));
	GL_CHECK(glDeleteShader(pStatics->shader[program].iFragName	));
	GL_CHECK(glDeleteProgram(pStatics->shader[program].iProgName ));
	
	VR_INFO("", VR_GRAPHIC_DBG_TARGET, "vrGSurfaceDeinitDisplay eglDestroyContext start, 32ctx\n"); 	
	vrResetShaderInfo(&pStatics->shader[program]);

	EGL_CHECK(eglDestroySurface(pStatics->egl_info.sEGLDisplay,pStatics->display_surface));

	nxGSurfaceDestroyEGLContext(program);	
	
	vrPlatformDestroyNativeWindow(pStatics->platform_win);
	pStatics->platform_win = NULL;
}

static void nxGSurfaceDestroySurfaceTarget ( HSURFTARGET hTarget)
{
	Statics *pStatics = vrGetStatics();
#ifdef VR_FEATURE_SEPERATE_FD_USE	
	if( hTarget->target_pixmap_surface ) { EGL_CHECK(eglDestroySurface(pStatics->egl_info.sEGLDisplay,hTarget->target_pixmap_surface)); }
	if( hTarget->target_native_pixmap  ) { nxGSurfaceDestroyPixmap(hTarget->target_native_pixmap); }
#else
	for(int i = 0 ; i < VR_INPUT_MODE_YUV_MAX ; i++)
	{		
		if( hTarget->target_pixmap_surfaces[i] ) { EGL_CHECK(eglDestroySurface(pStatics->egl_info.sEGLDisplay,hTarget->target_pixmap_surfaces[i])); }
		if( hTarget->target_native_pixmaps[i]  ) { nxGSurfaceDestroyPixmap(hTarget->target_native_pixmaps[i]); }
	}
#endif	
	NX_FREE(hTarget);
}

static void vrResetShaderInfo(Shader* pShader)
{
	pShader->iVertName = -1;
	pShader->iFragName = -1;
	pShader->iProgName = -1;
}

#ifdef VR_FEATURE_SEPERATE_FD_USE
HSURFTARGET nxGSurfaceCreateDeinterlaceTarget  (unsigned int uiWidth, unsigned int uiHeight, NX_MEMORY_HANDLE hData, int iIsDefault )
{
	Statics *pStatics = vrGetStatics();
	if( !pStatics ){ return (HSURFTARGET)0; }
	_AUTO_BACKUP_CURRENT_EGL_;
	HSURFTARGET result = (HSURFTARGET)NX_MALLOC(sizeof(vrSurfaceTarget));
	if( !result )
	{
		NxErrMsg("Error: Out of memory at %s:%i\n", __FILE__, __LINE__);
		return (HSURFTARGET)0;
	}
	NX_MEMSET( result, 0, sizeof(struct vrSurfaceTarget) );

	/* Create a EGLNativePixmapType. */
	/* Y4���� �Ѳ�����*/
	EGLNativePixmapType pixmap_output = (EGLNativePixmapType)nxGSurfaceCreatePixmap(uiWidth/4, uiHeight, hData, VR_TRUE, 32);
	if(pixmap_output == NULL || ((VR_PLATFORM_PIXMAP_STRUCT*)pixmap_output)->data == NULL)
	{
		NxErrMsg("Error: Out of memory at %s:%i\n", __FILE__, __LINE__);
		NX_FREE( result );
		return (HSURFTARGET)0;
	}

	/* Create a EGLSurface. */
	EGLSurface surface = eglCreatePixmapSurface(pStatics->egl_info.sEGLDisplay, pStatics->egl_info.sEGLConfig[VR_PROGRAM_DEINTERLACE], (EGLNativePixmapType)pixmap_output, NULL);
	if(surface == EGL_NO_SURFACE)
	{
		EGLint iError = eglGetError();
		NxErrMsg("eglGetError(): %i (0x%.4x)\n", (int)iError, (int)iError);
		NxErrMsg("Error: Failed to create EGL surface at %s:%i\n", __FILE__, __LINE__);
		nxGSurfaceDestroyPixmap(pixmap_output);
		NX_FREE( result );
		return (HSURFTARGET)0;
	}

	result->width          = uiWidth ;
	result->height         = uiHeight;
	result->target_pixmap_surface = surface;	
	result->target_native_pixmap  = pixmap_output;
	/* Increase Ctx ref. */
	if(!iIsDefault) ++pStatics->egl_info.sEGLContextRef[VR_PROGRAM_DEINTERLACE];
	VR_INFO("", VR_GRAPHIC_DBG_TARGET, "nxGSurfaceCreateDeinterlaceTarget, 32ctx ref(%d)\n", pStatics->egl_info.sEGLContextRef[VR_PROGRAM_DEINTERLACE]); 	
	
	return result;
}

void nxGSurfaceDestroyDeinterlaceTarget ( HSURFTARGET hTarget, int iIsDefault )
{
	if( !hTarget ){ return; }
	Statics *pStatics = vrGetStatics();
	_AUTO_BACKUP_CURRENT_EGL_;

	nxGSurfaceDestroySurfaceTarget(hTarget);
	
	/* Decrease Ctx ref. */
	if(!iIsDefault) 
	{
		VR_ASSERT("Ref must be greater than 0", pStatics->egl_info.sEGLContextRef[VR_PROGRAM_DEINTERLACE] > 0);
		--pStatics->egl_info.sEGLContextRef[VR_PROGRAM_DEINTERLACE];
	}
	
	VR_INFO("", VR_GRAPHIC_DBG_TARGET, "nxGSurfaceDestroyDeinterlaceTarget done, 32ctx ref(%d)\n", pStatics->egl_info.sEGLContextRef[VR_PROGRAM_DEINTERLACE]); 			
}

int nxGSurfaceCreateScaleTarget  (HSURFTARGET* ptarget, unsigned int uiWidth, unsigned int uiHeight, NX_MEMORY_HANDLE* pData, int iIsDefault )
{
	Statics *pStatics = vrGetStatics();
	ptarget[0] = NULL;
	ptarget[1] = NULL;
	ptarget[2] = NULL;

	if( iIsDefault )
	{
		if( !pStatics || !pData || !pData[0] )
		{ 
			NxErrMsg("Error: input is not valid\n");		
			return VR_FALSE; 
		}
	}
	else
	{
		if( !pStatics || !pData || !pData[0] || !pData[1] || !pData[2] )
		{ 
			NxErrMsg("Error: input is not valid\n");		
			return VR_FALSE; 
		}
	}
	_AUTO_BACKUP_CURRENT_EGL_;
	
	VRImageFormatMode format[3] = { VR_IMAGE_FORMAT_Y, VR_IMAGE_FORMAT_U, VR_IMAGE_FORMAT_V };
	
	unsigned int pixmap_width, pixmap_height;
	for(int i = 0 ; i < VR_YUV_CNT ; i++)
	{
		if(!pData[i])
		{
			VR_ASSERT("Error: Y must exist", i != 0);
			continue;
		}
		
		HSURFTARGET result = (HSURFTARGET)NX_MALLOC(sizeof(vrSurfaceTarget));
		if( !result )
		{
			NxErrMsg("Error: Out of memory at %s:%i\n", __FILE__, __LINE__);
			return VR_FALSE;
		}
		NX_MEMSET( result, 0, sizeof(struct vrSurfaceTarget) );

		if(0 == i)
		{
			//Y case
			pixmap_width = uiWidth;
			pixmap_height = uiHeight;
		}
		else if(1 == i)
		{
			//U case
			pixmap_width = uiWidth/2;
			pixmap_height = uiHeight/2;
		}
		else
		{
			//V case
			pixmap_width = uiWidth/2;
			pixmap_height = uiHeight/2;
		}

		/* Create a EGLNativePixmapType. */
		EGLNativePixmapType pixmap_output = (EGLNativePixmapType)nxGSurfaceCreatePixmap(pixmap_width, pixmap_height, pData[i], VR_TRUE, 8);
		if(pixmap_output == NULL || ((VR_PLATFORM_PIXMAP_STRUCT*)pixmap_output)->data == NULL)
		{
			NxErrMsg("Error: Out of memory at %s:%i\n", __FILE__, __LINE__);
			NX_FREE( result );
			return VR_FALSE;
		}
		
		/* Create a EGLSurface. */
		EGLSurface surface = eglCreatePixmapSurface(pStatics->egl_info.sEGLDisplay, pStatics->egl_info.sEGLConfig[VR_PROGRAM_SCALE], (EGLNativePixmapType)pixmap_output, NULL);
		if(surface == EGL_NO_SURFACE)
		{
			EGLint iError = eglGetError();
			NxErrMsg("eglGetError(): %i (0x%.4x)\n", (int)iError, (int)iError);
			NxErrMsg("Error: Failed to create EGL surface at %s:%i\n", __FILE__, __LINE__);
			nxGSurfaceDestroyPixmap(pixmap_output);
			NX_FREE( result );
			return VR_FALSE;
		}

		result->width          = pixmap_width ;
		result->height         = pixmap_height;
		result->target_native_pixmap  = pixmap_output;
		result->target_pixmap_surface = surface;	
		ptarget[i] = result;

		if(iIsDefault)
		{ 								
			break; 
		}
	}
	
	/* Increase Ctx ref. */
	if(!iIsDefault) ++pStatics->egl_info.sEGLContextRef[VR_PROGRAM_SCALE];
	VR_INFO("", VR_GRAPHIC_DBG_TARGET, "nxGSurfaceCreateScaleTarget, 8ctx ref(%d)\n", pStatics->egl_info.sEGLContextRef[VR_PROGRAM_SCALE]); 	
	
	return VR_TRUE;
}

void nxGSurfaceDestroyScaleTarget ( HSURFTARGET* ptarget, int iIsDefault )
{
	if( !ptarget ){ return; }
	Statics *pStatics = vrGetStatics();
	_AUTO_BACKUP_CURRENT_EGL_;

	for(int i = 0 ; i < VR_YUV_CNT ; i++)
	{		
		if(!ptarget[i])
		{
			VR_ASSERT("Error: Y must exist", i != 0);		
			continue;
		}			
		nxGSurfaceDestroySurfaceTarget(ptarget[i]);
		
		if(iIsDefault){ break; }
	}
	
	/* Decrease Ctx ref. */
	if(!iIsDefault)
	{
		VR_ASSERT("Ref must be greater than 0", pStatics->egl_info.sEGLContextRef[VR_PROGRAM_SCALE] > 0);
		--pStatics->egl_info.sEGLContextRef[VR_PROGRAM_SCALE];
	}
	VR_INFO("", VR_GRAPHIC_DBG_TARGET, "nxGSurfaceDestroyScaleTarget done, 8ctx ref(%d)\n", pStatics->egl_info.sEGLContextRef[VR_PROGRAM_SCALE]); 		
}

HSURFTARGET nxGSurfaceCreateCvt2YTarget  (unsigned int uiWidth, unsigned int uiHeight, NX_MEMORY_HANDLE hData, int iIsDefault)
{
	//NxErrMsg("nxGSurfaceCreateCvt2YTarget start(%dx%d)\n", uiWidth, uiHeight);

	Statics *pStatics = vrGetStatics();
	if( !pStatics ){ return (HSURFTARGET)0; }
	_AUTO_BACKUP_CURRENT_EGL_;
	HSURFTARGET result = (HSURFTARGET)NX_MALLOC(sizeof(vrSurfaceTarget));
	if( !result )
	{
		NxErrMsg("Error: Out of memory at %s:%i\n", __FILE__, __LINE__);
		return (HSURFTARGET)0;
	}
	NX_MEMSET( result, 0, sizeof(struct vrSurfaceTarget) );

	/* Create a EGLNativePixmapType. */
	EGLNativePixmapType pixmap_output = (EGLNativePixmapType)nxGSurfaceCreatePixmap(uiWidth, uiHeight, hData, VR_TRUE, 8);
	if(pixmap_output == NULL || ((VR_PLATFORM_PIXMAP_STRUCT*)pixmap_output)->data == NULL)
	{
		NxErrMsg("Error: Out of memory at %s:%i\n", __FILE__, __LINE__);
		NX_FREE( result );
		return (HSURFTARGET)0;
	}
	
	/* Create a EGLSurface. */
	EGLSurface surface = eglCreatePixmapSurface(pStatics->egl_info.sEGLDisplay, pStatics->egl_info.sEGLConfig[VR_PROGRAM_CVT2Y], (EGLNativePixmapType)pixmap_output, NULL);
	if(surface == EGL_NO_SURFACE)
	{
		EGLint iError = eglGetError();
		NxErrMsg("eglGetError(): %i (0x%.4x)\n", (int)iError, (int)iError);
		NxErrMsg("Error: Failed to create EGL surface at %s:%i\n", __FILE__, __LINE__);
		nxGSurfaceDestroyPixmap(pixmap_output);
		NX_FREE( result );
		return (HSURFTARGET)0;
	}

	result->width          = uiWidth ;
	result->height         = uiHeight;
	result->target_native_pixmap  = pixmap_output;
	result->target_pixmap_surface = surface;	
	
	/* Increase Ctx ref. */
	if(!iIsDefault) ++pStatics->egl_info.sEGLContextRef[VR_PROGRAM_CVT2Y];
	VR_INFO("", VR_GRAPHIC_DBG_TARGET, "nxGSurfaceCreateCvt2YTarget, 8ctx ref(%d)\n", pStatics->egl_info.sEGLContextRef[VR_PROGRAM_CVT2Y]); 	
	
	return result;
}

void nxGSurfaceDestroyCvt2YTarget ( HSURFTARGET hTarget, int iIsDefault )
{
	if( !hTarget ){ return; }
	Statics *pStatics = vrGetStatics();
	_AUTO_BACKUP_CURRENT_EGL_;
	
	nxGSurfaceDestroySurfaceTarget(hTarget);

	/* Decrease Ctx ref. */
	if(!iIsDefault)
	{
		VR_ASSERT("Ref must be greater than 0", pStatics->egl_info.sEGLContextRef[VR_PROGRAM_CVT2Y] > 0);
		--pStatics->egl_info.sEGLContextRef[VR_PROGRAM_CVT2Y];
	}	
	VR_INFO("", VR_GRAPHIC_DBG_TARGET, "nxGSurfaceDestroyCvt2YTarget done, 8ctx ref(%d)\n", pStatics->egl_info.sEGLContextRef[VR_PROGRAM_CVT2Y]); 		
}

HSURFTARGET nxGSurfaceCreateCvt2UVTarget  (unsigned int uiWidth, unsigned int uiHeight, NX_MEMORY_HANDLE hData, int iIsDefault)
{
	//NxErrMsg("nxGSurfaceCreateCvt2UVTarget start(%dx%d)\n", uiWidth, uiHeight);

	Statics *pStatics = vrGetStatics();
	if( !pStatics ){ return (HSURFTARGET)0; }
	_AUTO_BACKUP_CURRENT_EGL_;
	HSURFTARGET result = (HSURFTARGET)NX_MALLOC(sizeof(vrSurfaceTarget));
	if( !result )
	{
		NxErrMsg("Error: Out of memory at %s:%i\n", __FILE__, __LINE__);
		return (HSURFTARGET)0;
	}
	NX_MEMSET( result, 0, sizeof(struct vrSurfaceTarget) );

	//4pixel���� UV����
	uiWidth /= 2;
	uiHeight /= 2;
	
	/* Create a EGLNativePixmapType. */
	EGLNativePixmapType pixmap_output = (EGLNativePixmapType)nxGSurfaceCreatePixmap(uiWidth, uiHeight, hData, VR_TRUE, 16);
	if(pixmap_output == NULL || ((VR_PLATFORM_PIXMAP_STRUCT*)pixmap_output)->data == NULL)
	{
		NxErrMsg("Error: Out of memory at %s:%i\n", __FILE__, __LINE__);
		NX_FREE( result );
		return (HSURFTARGET)0;
	}
	
	/* Create a EGLSurface. */
	EGLSurface surface = eglCreatePixmapSurface(pStatics->egl_info.sEGLDisplay, pStatics->egl_info.sEGLConfig[VR_PROGRAM_CVT2UV], (EGLNativePixmapType)pixmap_output, NULL);
	if(surface == EGL_NO_SURFACE)
	{
		EGLint iError = eglGetError();
		NxErrMsg("eglGetError(): %i (0x%.4x)\n", (int)iError, (int)iError);
		NxErrMsg("Error: Failed to create EGL surface at %s:%i\n", __FILE__, __LINE__);
		nxGSurfaceDestroyPixmap(pixmap_output);
		NX_FREE( result );
		return (HSURFTARGET)0;
	}

	result->width          = uiWidth ;
	result->height         = uiHeight;
	result->target_native_pixmap  = pixmap_output;
	result->target_pixmap_surface = surface;	

	/* Increase Ctx ref. */
	if(!iIsDefault) ++pStatics->egl_info.sEGLContextRef[VR_PROGRAM_CVT2UV];
	VR_INFO("", VR_GRAPHIC_DBG_TARGET, "nxGSurfaceCreateCvt2UVTarget, 16ctx ref(%d)\n", pStatics->egl_info.sEGLContextRef[VR_PROGRAM_CVT2UV]); 		
	
	return result;
}

void nxGSurfaceDestroyCvt2UVTarget ( HSURFTARGET hTarget, int iIsDefault )
{
	if( !hTarget ){ return; }
	Statics *pStatics = vrGetStatics();
	_AUTO_BACKUP_CURRENT_EGL_;
	
	nxGSurfaceDestroySurfaceTarget(hTarget);

	/* Decrease Ctx ref. */
	if(!iIsDefault)
	{
		VR_ASSERT("Ref must be greater than 0", pStatics->egl_info.sEGLContextRef[VR_PROGRAM_CVT2UV] > 0);
		--pStatics->egl_info.sEGLContextRef[VR_PROGRAM_CVT2UV];		
	}	
	VR_INFO("", VR_GRAPHIC_DBG_TARGET, "nxGSurfaceDestroyCvt2UVTarget done, 16ctx ref(%d)\n", pStatics->egl_info.sEGLContextRef[VR_PROGRAM_CVT2UV]); 		
}

HSURFTARGET nxGSurfaceCreateCvt2RgbaTarget  (unsigned int uiWidth, unsigned int uiHeight, NX_MEMORY_HANDLE hData, int iIsDefault)
{
	Statics *pStatics = vrGetStatics();
	if( !pStatics ){ return (HSURFTARGET)0; }
	_AUTO_BACKUP_CURRENT_EGL_;
	HSURFTARGET result = (HSURFTARGET)NX_MALLOC(sizeof(vrSurfaceTarget));
	if( !result )
	{
		NxErrMsg("Error: Out of memory at %s:%i\n", __FILE__, __LINE__);
		return (HSURFTARGET)0;
	}
	NX_MEMSET( result, 0, sizeof(struct vrSurfaceTarget) );

	/* Create a EGLNativePixmapType. */
	EGLNativePixmapType pixmap_output = (EGLNativePixmapType)nxGSurfaceCreatePixmap(uiWidth, uiHeight, hData, VR_TRUE, 32);
	if(pixmap_output == NULL || ((VR_PLATFORM_PIXMAP_STRUCT*)pixmap_output)->data == NULL)
	{
		NxErrMsg("Error: Out of memory at %s:%i\n", __FILE__, __LINE__);
		NX_FREE( result );
		return (HSURFTARGET)0;
	}
	
	/* Create a EGLSurface. */
	EGLSurface surface = eglCreatePixmapSurface(pStatics->egl_info.sEGLDisplay, pStatics->egl_info.sEGLConfig[VR_PROGRAM_CVT2RGBA], (EGLNativePixmapType)pixmap_output, NULL);
	if(surface == EGL_NO_SURFACE)
	{
		EGLint iError = eglGetError();
		NxErrMsg("eglGetError(): %i (0x%.4x)\n", (int)iError, (int)iError);
		NxErrMsg("Error: Failed to create EGL surface at %s:%i\n", __FILE__, __LINE__);
		nxGSurfaceDestroyPixmap(pixmap_output);
		NX_FREE( result );
		return (HSURFTARGET)0;
	}

	result->width          = uiWidth ;
	result->height         = uiHeight;
	result->target_native_pixmap  = pixmap_output;
	result->target_pixmap_surface = surface;	

	/* Increase Ctx ref. */
	if(!iIsDefault) ++pStatics->egl_info.sEGLContextRef[VR_PROGRAM_CVT2RGBA];
	VR_INFO("", VR_GRAPHIC_DBG_TARGET, "nxGSurfaceCreateCvt2RgbaTarget, 32ctx ref(%d)\n", pStatics->egl_info.sEGLContextRef[VR_PROGRAM_CVT2RGBA]); 		
	
	return result;
}

void nxGSurfaceDestroyCvt2RgbaTarget ( HSURFTARGET hTarget, int iIsDefault )
{
	if( !hTarget ){ return; }
	Statics *pStatics = vrGetStatics();
	_AUTO_BACKUP_CURRENT_EGL_;
		
	nxGSurfaceDestroySurfaceTarget(hTarget);

	/* Decrease Ctx ref. */
	if(!iIsDefault)
	{
		VR_ASSERT("Ref must be greater than 0", pStatics->egl_info.sEGLContextRef[VR_PROGRAM_CVT2RGBA] > 0);
		--pStatics->egl_info.sEGLContextRef[VR_PROGRAM_CVT2RGBA];
	}
	VR_INFO("", VR_GRAPHIC_DBG_TARGET, "nxGSurfaceDestroyCvt2RgbaTarget done, 32ctx ref(%d)\n", pStatics->egl_info.sEGLContextRef[VR_PROGRAM_CVT2RGBA]); 			
}

int nxGSurfaceCreateYuvFilterTarget  (HSURFTARGET* ptarget, unsigned int uiWidth, unsigned int uiHeight, NX_MEMORY_HANDLE* pData, int iIsDefault )
{
	Statics *pStatics = vrGetStatics();
	ptarget[0] = NULL;
	ptarget[1] = NULL;
	ptarget[2] = NULL;
	
	if( iIsDefault )
	{
		if( !pStatics || !pData || !pData[0] )
		{ 
			NxErrMsg("Error: input is not valid\n");		
			return VR_FALSE; 
		}
	}
	else
	{
		if( !pStatics || !pData || !pData[0] || !pData[1] || !pData[2] )
		{ 
			NxErrMsg("Error: input is not valid\n");		
			return VR_FALSE; 
		}
	}
	_AUTO_BACKUP_CURRENT_EGL_;

	VRImageFormatMode format[3] = { VR_IMAGE_FORMAT_Y, VR_IMAGE_FORMAT_U, VR_IMAGE_FORMAT_V };
	
	unsigned int pixmap_width, pixmap_height;
	for(int i = 0 ; i < VR_YUV_CNT ; i++)
	{
		if(!pData[i])
		{
			VR_ASSERT("Error: Y must exist", i != 0);
			continue;
		}
		
		HSURFTARGET result = (HSURFTARGET)NX_MALLOC(sizeof(vrSurfaceTarget));
		if( !result )
		{
			NxErrMsg("Error: Out of memory at %s:%i\n", __FILE__, __LINE__);
			return VR_FALSE;
		}
		NX_MEMSET( result, 0, sizeof(struct vrSurfaceTarget) );

		if(0 == i)
		{
			//Y case
			pixmap_width = uiWidth;
			pixmap_height = uiHeight;
		}
		else if(1 == i)
		{
			//U case
			pixmap_width = uiWidth/2;
			pixmap_height = uiHeight/2;
		}
		else
		{
			//V case
			pixmap_width = uiWidth/2;
			pixmap_height = uiHeight/2;
		}

		/* Create a EGLNativePixmapType. */
		EGLNativePixmapType pixmap_output = (EGLNativePixmapType)nxGSurfaceCreatePixmap(pixmap_width, pixmap_height, pData[i], VR_TRUE, 8);
		if(pixmap_output == NULL || ((VR_PLATFORM_PIXMAP_STRUCT*)pixmap_output)->data == NULL)
		{
			NxErrMsg("Error: Out of memory at %s:%i\n", __FILE__, __LINE__);
			NX_FREE( result );
			return VR_FALSE;
		}
		
		/* Create a EGLSurface. */
		EGLSurface surface = eglCreatePixmapSurface(pStatics->egl_info.sEGLDisplay, pStatics->egl_info.sEGLConfig[VR_PROGRAM_YUVFILTER_Y], (EGLNativePixmapType)pixmap_output, NULL);
		if(surface == EGL_NO_SURFACE)
		{
			EGLint iError = eglGetError();
			NxErrMsg("eglGetError(): %i (0x%.4x)\n", (int)iError, (int)iError);
			NxErrMsg("Error: Failed to create EGL surface at %s:%i\n", __FILE__, __LINE__);
			nxGSurfaceDestroyPixmap(pixmap_output);
			NX_FREE( result );
			return VR_FALSE;
		}

		result->width          = pixmap_width ;
		result->height         = pixmap_height;
		result->target_native_pixmap  = pixmap_output;
		result->target_pixmap_surface = surface;	
		ptarget[i] = result;

		if(iIsDefault)
		{ 								
			break; 
		}
	}
	
	/* Increase Ctx ref. */
	if(!iIsDefault) ++pStatics->egl_info.sEGLContextRef[VR_PROGRAM_YUVFILTER_Y];
	VR_INFO("", VR_GRAPHIC_DBG_TARGET, "nxGSurfaceCreateYuvFilterTarget, 8ctx ref(%d)\n", pStatics->egl_info.sEGLContextRef[VR_PROGRAM_YUVFILTER_Y]); 	
	
	return VR_TRUE;
}

void nxGSurfaceDestroyYuvFilterTarget ( HSURFTARGET* ptarget, int iIsDefault )
{
	if( !ptarget ){ return; }
	Statics *pStatics = vrGetStatics();
	_AUTO_BACKUP_CURRENT_EGL_;

	for(int i = 0 ; i < VR_YUV_CNT ; i++)
	{		
		if(!ptarget[i])
		{
			VR_ASSERT("Error: Y must exist", i != 0);		
			continue;
		}			
		nxGSurfaceDestroySurfaceTarget(ptarget[i]);
		
		if(iIsDefault){ break; }
	}
	
	/* Decrease Ctx ref. */
	if(!iIsDefault)
	{
		VR_ASSERT("Ref must be greater than 0", pStatics->egl_info.sEGLContextRef[VR_PROGRAM_YUVFILTER_Y] > 0);
		--pStatics->egl_info.sEGLContextRef[VR_PROGRAM_YUVFILTER_Y];
	}
	VR_INFO("", VR_GRAPHIC_DBG_TARGET, "nxGSurfaceDestroyYuvFilterTarget done, 8ctx ref(%d)\n", pStatics->egl_info.sEGLContextRef[VR_PROGRAM_YUVFILTER_Y]); 		
}


#else

HSURFTARGET nxGSurfaceCreateDeinterlaceTarget  (unsigned int uiStride, unsigned int uiWidth, unsigned int uiHeight, NX_MEMORY_HANDLE hData, int iIsDefault)
{
	VR_ASSERT("width must be 8X and height must be 2X", uiWidth && uiHeight && !(uiWidth % 8) && ((uiHeight&0x1) == 0x0) );
	VR_INFO("", VR_GRAPHIC_DBG_TARGET, "%s start\n", __FUNCTION__);

	Statics *pStatics = vrGetStatics();
	if( !pStatics || !hData ){ return VR_FALSE; }
	_AUTO_BACKUP_CURRENT_EGL_;

	VRImageFormatMode format[3] = { VR_IMAGE_FORMAT_V, VR_IMAGE_FORMAT_U, VR_IMAGE_FORMAT_Y };
	
	HSURFTARGET result = (HSURFTARGET)NX_MALLOC(sizeof(vrSurfaceTarget));
	if( !result )
	{
		NxErrMsg("Error: Out of memory at %s:%i\n", __FILE__, __LINE__);
		return (HSURFTARGET)0;
	}
	NX_MEMSET( result, 0, sizeof(struct vrSurfaceTarget) );
	result->width		   = uiWidth ;
	result->height		   = uiHeight;
	#ifdef VR_FEATURE_STRIDE_USE
	result->stride		   = uiStride;
	#endif

	unsigned int pixmap_stride, pixmap_width, pixmap_height;
	if(!uiStride){ uiStride = uiWidth; }
	for(int i = 0 ; i < VR_YUV_CNT ; i++)
	{
		if(2 == i)
		{
			//Y case
			pixmap_stride = uiStride;
			pixmap_width = uiWidth;
			pixmap_height = uiHeight;
		}
		else if(1 == i)
		{
			//U case
			pixmap_stride = uiWidth/2 + (uiStride-uiWidth)/2;
			pixmap_width = uiWidth/2;
			pixmap_height = uiHeight*2 + uiHeight/2;			
			#if (VR_FEATURE_OUTPUT_HEIGHT_ALIGN_BYTE > 0)
			{
				//add Y align blank offset
				unsigned int y_height, y_align_blank_height = 0;
				y_height = uiHeight;
				if(y_height % VR_FEATURE_OUTPUT_HEIGHT_ALIGN_BYTE)
				{	
					y_align_blank_height = (VR_FEATURE_OUTPUT_HEIGHT_ALIGN_BYTE - (y_height % VR_FEATURE_OUTPUT_HEIGHT_ALIGN_BYTE)) * 2;
				}
				pixmap_height += y_align_blank_height;
				VR_INFO("", VR_GRAPHIC_DBG_HEIGHT_ALIGN, "[%d]U. size(%dx%d), y_align_blank_height(%d)\n", i, pixmap_width, pixmap_height, y_align_blank_height);		
			}
			#endif
		}
		else
		{
			//V case
			pixmap_stride = uiWidth/2 + (uiStride-uiWidth)/2;
			pixmap_width = uiWidth/2;
			pixmap_height = uiHeight*2 + uiHeight;
			#if (VR_FEATURE_OUTPUT_HEIGHT_ALIGN_BYTE > 0)
			{
				unsigned int y_height, y_align_blank_height = 0;
				unsigned int u_height, u_align_blank_height = 0;
				//add Y align blank offset
				y_height = uiHeight;
				if(y_height % VR_FEATURE_OUTPUT_HEIGHT_ALIGN_BYTE)
				{	
					y_align_blank_height = (VR_FEATURE_OUTPUT_HEIGHT_ALIGN_BYTE - (y_height % VR_FEATURE_OUTPUT_HEIGHT_ALIGN_BYTE)) * 2;
				}
				//add U align blank offset
				u_height = uiHeight/2;
				if(u_height % VR_FEATURE_OUTPUT_HEIGHT_ALIGN_BYTE)
				{	
					u_align_blank_height = (VR_FEATURE_OUTPUT_HEIGHT_ALIGN_BYTE - (u_height % VR_FEATURE_OUTPUT_HEIGHT_ALIGN_BYTE));
				}
				pixmap_height += (y_align_blank_height + u_align_blank_height);
				VR_INFO("", VR_GRAPHIC_DBG_HEIGHT_ALIGN, "[%d]V. y_align_blank_height(%d), u_align_blank_height(%d)\n", i, y_align_blank_height, u_align_blank_height);		
			}
			#endif			
		}
		
		/* Create a EGLNativePixmapType. */
		/* input Y 4���� �Ѳ�����*/
		#ifdef VR_FEATURE_STRIDE_USE
		pixmap_width = pixmap_stride;
		#endif
		VR_INFO("", VR_GRAPHIC_DBG_HEIGHT_ALIGN, "eglCreatePixmapSurface (%dx%d)\n", pixmap_width, pixmap_height); 	
		EGLNativePixmapType pixmap_output = (EGLNativePixmapType)nxGSurfaceCreatePixmap(pixmap_width/4, pixmap_height, hData, VR_TRUE, 32);
		if(pixmap_output == NULL || ((VR_PLATFORM_PIXMAP_STRUCT*)pixmap_output)->data == NULL)
		{
			NxErrMsg("Error: Out of memory at %s:%i\n", __FILE__, __LINE__);
			NX_FREE( result );
			return (HSURFTARGET)0;
		}
		
		/* Create a EGLSurface. */		
		EGLSurface surface = eglCreatePixmapSurface(pStatics->egl_info.sEGLDisplay, pStatics->egl_info.sEGLConfig[VR_PROGRAM_DEINTERLACE], (EGLNativePixmapType)pixmap_output, NULL);
		if(surface == EGL_NO_SURFACE)
		{
			EGLint iError = eglGetError();
			NxErrMsg("eglGetError(): %i (0x%.4x)\n", (int)iError, (int)iError);
			NxErrMsg("Error: Failed to create EGL surface at %s:%i\n", __FILE__, __LINE__);
			nxGSurfaceDestroyPixmap(pixmap_output);
			NX_FREE( result );
			return (HSURFTARGET)0;
		}

		result->target_native_pixmaps[i]  = pixmap_output;
		result->target_pixmap_surfaces[i] = surface;	

		VR_INFO("", VR_GRAPHIC_DBG_TARGET, "size(%dx%d, %d)\n", pixmap_width, pixmap_height, pixmap_stride);		

		if(iIsDefault)
		{
			break; 
		}		
	}
	/* Increase Ctx ref. */
	if(!iIsDefault) ++pStatics->egl_info.sEGLContextRef[VR_PROGRAM_DEINTERLACE];
	VR_INFO("", VR_GRAPHIC_DBG_TARGET, "nxGSurfaceCreateDeinterlaceTarget, 32ctx ref(%d)\n", pStatics->egl_info.sEGLContextRef[VR_PROGRAM_DEINTERLACE]); 	
	
	return result;
}

void nxGSurfaceDestroyDeinterlaceTarget ( HSURFTARGET hTarget, int iIsDefault )
{
	if( !hTarget ){ return; }
	Statics *pStatics = vrGetStatics();
	_AUTO_BACKUP_CURRENT_EGL_;
			
	nxGSurfaceDestroySurfaceTarget(hTarget);

	/* Decrease Ctx ref. */
	if(!iIsDefault)
	{
		VR_ASSERT("Ref must be greater than 0", pStatics->egl_info.sEGLContextRef[VR_PROGRAM_DEINTERLACE] > 0);
		--pStatics->egl_info.sEGLContextRef[VR_PROGRAM_DEINTERLACE];
	}
	VR_INFO("", VR_GRAPHIC_DBG_TARGET, "nxGSurfaceDestroyDeinterlaceTarget done, 32ctx ref(%d)\n", pStatics->egl_info.sEGLContextRef[VR_PROGRAM_DEINTERLACE]); 		
}

HSURFTARGET nxGSurfaceCreateScaleTarget  (unsigned int uiStride, unsigned int uiWidth, unsigned int uiHeight, NX_MEMORY_HANDLE hData, int iIsDefault)
{
	VR_ASSERT("width must be 2X and height must be 2X", uiWidth && uiHeight && ((uiWidth&0x1) == 0x0) && ((uiHeight&0x1) == 0x0) );

	Statics *pStatics = vrGetStatics();
	if( !pStatics || !hData ){ return VR_FALSE; }
	_AUTO_BACKUP_CURRENT_EGL_;

	VR_INFO("", VR_GRAPHIC_DBG_TARGET, "%s start\n", __FUNCTION__);

	VRImageFormatMode format[3] = { VR_IMAGE_FORMAT_V, VR_IMAGE_FORMAT_U, VR_IMAGE_FORMAT_Y };
	
	HSURFTARGET result = (HSURFTARGET)NX_MALLOC(sizeof(vrSurfaceTarget));
	if( !result )
	{
		NxErrMsg("Error: Out of memory at %s:%i\n", __FILE__, __LINE__);
		return (HSURFTARGET)0;
	}
	NX_MEMSET( result, 0, sizeof(struct vrSurfaceTarget) );
	result->width		   = uiWidth ;
	result->height		   = uiHeight;
	#ifdef VR_FEATURE_STRIDE_USE
	result->stride		   = uiStride;
	#endif

	unsigned int pixmap_stride, pixmap_width, pixmap_height;
	if(!uiStride){ uiStride = uiWidth; }
	for(int i = 0 ; i < VR_YUV_CNT ; i++)
	{
		if(2 == i)
		{
			//Y case
			pixmap_stride = uiStride;
			pixmap_width = uiWidth;
			pixmap_height = uiHeight;
		}
		else if(1 == i)
		{
			//U case
			pixmap_stride = uiWidth/2 + (uiStride-uiWidth)/2;
			pixmap_width = uiWidth/2;
			pixmap_height = uiHeight*2 + uiHeight/2;
			#if (VR_FEATURE_OUTPUT_HEIGHT_ALIGN_BYTE > 0)
			{
				//add Y align blank offset
				unsigned int y_height, y_align_blank_height = 0;
				y_height = uiHeight;
				if(y_height % VR_FEATURE_OUTPUT_HEIGHT_ALIGN_BYTE)
				{	
					y_align_blank_height = (VR_FEATURE_OUTPUT_HEIGHT_ALIGN_BYTE - (y_height % VR_FEATURE_OUTPUT_HEIGHT_ALIGN_BYTE)) * 2;
				}
				pixmap_height += y_align_blank_height;
				VR_INFO("", VR_GRAPHIC_DBG_HEIGHT_ALIGN, "[%d] y_align_blank_height(%d)\n", i, y_align_blank_height);		
			}
			#endif
		}
		else
		{
			//V case
			pixmap_stride = uiWidth/2 + (uiStride-uiWidth)/2;
			pixmap_width = uiWidth/2;
			pixmap_height = uiHeight*2 + uiHeight;
			#if (VR_FEATURE_OUTPUT_HEIGHT_ALIGN_BYTE > 0)
			{
				unsigned int y_height, y_align_blank_height = 0;
				unsigned int u_height, u_align_blank_height = 0;
				//add Y align blank offset
				y_height = uiHeight;
				if(y_height % VR_FEATURE_OUTPUT_HEIGHT_ALIGN_BYTE)
				{	
					y_align_blank_height = (VR_FEATURE_OUTPUT_HEIGHT_ALIGN_BYTE - (y_height % VR_FEATURE_OUTPUT_HEIGHT_ALIGN_BYTE)) * 2;
				}
				//add U align blank offset
				u_height = uiHeight/2;
				if(u_height % VR_FEATURE_OUTPUT_HEIGHT_ALIGN_BYTE)
				{	
					u_align_blank_height = (VR_FEATURE_OUTPUT_HEIGHT_ALIGN_BYTE - (u_height % VR_FEATURE_OUTPUT_HEIGHT_ALIGN_BYTE));
				}
				pixmap_height += (y_align_blank_height + u_align_blank_height);
				VR_INFO("", VR_GRAPHIC_DBG_HEIGHT_ALIGN, "[%d] y_align_blank_height(%d), u_align_blank_height(%d)\n", i, y_align_blank_height, u_align_blank_height);		
			}
			#endif
		}
		
		/* Create a EGLNativePixmapType. */
		#ifdef VR_FEATURE_STRIDE_USE
		pixmap_width = pixmap_stride;
		#endif
		EGLNativePixmapType pixmap_output = (EGLNativePixmapType)nxGSurfaceCreatePixmap(pixmap_width, pixmap_height, hData, VR_TRUE, 8);
		if(pixmap_output == NULL || ((VR_PLATFORM_PIXMAP_STRUCT*)pixmap_output)->data == NULL)
		{
			NxErrMsg("Error: Out of memory at %s:%i\n", __FILE__, __LINE__);
			NX_FREE( result );
			return (HSURFTARGET)0;
		}
		
		/* Create a EGLSurface. */
		EGLSurface surface = eglCreatePixmapSurface(pStatics->egl_info.sEGLDisplay, pStatics->egl_info.sEGLConfig[VR_PROGRAM_SCALE], (EGLNativePixmapType)pixmap_output, NULL);
		if(surface == EGL_NO_SURFACE)
		{
			EGLint iError = eglGetError();
			NxErrMsg("eglGetError(): %i (0x%.4x)\n", (int)iError, (int)iError);
			NxErrMsg("Error: Failed to create EGL surface at %s:%i\n", __FILE__, __LINE__);
			nxGSurfaceDestroyPixmap(pixmap_output);
			NX_FREE( result );
			return (HSURFTARGET)0;
		}

		result->target_native_pixmaps[i]  = pixmap_output;
		result->target_pixmap_surfaces[i] = surface;	

		VR_INFO("", VR_GRAPHIC_DBG_TARGET, "size(%dx%d, %d)\n", pixmap_width, pixmap_height, pixmap_stride);		

		if(iIsDefault)
		{
			break; 
		}		
	}
	/* Increase Ctx ref. */
	if(!iIsDefault) ++pStatics->egl_info.sEGLContextRef[VR_PROGRAM_SCALE];
	VR_INFO("", VR_GRAPHIC_DBG_TARGET, "nxGSurfaceCreateScaleTarget, 8ctx ref(%d)\n", pStatics->egl_info.sEGLContextRef[VR_PROGRAM_SCALE]); 	
	
	return result;
}

void nxGSurfaceDestroyScaleTarget ( HSURFTARGET hTarget, int iIsDefault )
{
	if( !hTarget ){ return; }
	Statics *pStatics = vrGetStatics();
	_AUTO_BACKUP_CURRENT_EGL_;
			
	nxGSurfaceDestroySurfaceTarget(hTarget);

	/* Decrease Ctx ref. */
	if(!iIsDefault)
	{
		VR_ASSERT("Ref must be greater than 0", pStatics->egl_info.sEGLContextRef[VR_PROGRAM_SCALE] > 0);
		--pStatics->egl_info.sEGLContextRef[VR_PROGRAM_SCALE];
	}
	VR_INFO("", VR_GRAPHIC_DBG_TARGET, "nxGSurfaceDestroyScaleTarget done, 8ctx ref(%d)\n", pStatics->egl_info.sEGLContextRef[VR_PROGRAM_SCALE]); 		
}

static HSURFTARGET nxGSurfaceCreateCvt2YTargetDefault  (unsigned int uiStride, unsigned int uiWidth, unsigned int uiHeight, NX_MEMORY_HANDLE hData)
{
	VR_ASSERT("width must be 2X and height must be 2X", uiWidth && uiHeight && ((uiWidth&0x1) == 0x0) && ((uiHeight&0x1) == 0x0) );

	//NxErrMsg("nxGSurfaceCreateCvt2YTargetDefault start(%dx%d)\n", uiWidth, uiHeight);
	VR_INFO("", VR_GRAPHIC_DBG_TARGET, "%s start\n", __FUNCTION__);

	Statics *pStatics = vrGetStatics();
	if( !pStatics ){ return (HSURFTARGET)0; }
	_AUTO_BACKUP_CURRENT_EGL_;
	HSURFTARGET result = (HSURFTARGET)NX_MALLOC(sizeof(vrSurfaceTarget));
	if( !result )
	{
		NxErrMsg("Error: Out of memory at %s:%i\n", __FILE__, __LINE__);
		return (HSURFTARGET)0;
	}
	NX_MEMSET( result, 0, sizeof(struct vrSurfaceTarget) );

	/* Create a EGLNativePixmapType. */
	EGLNativePixmapType pixmap_output = (EGLNativePixmapType)nxGSurfaceCreatePixmap(uiWidth, uiHeight, hData, VR_TRUE, 8);
	if(pixmap_output == NULL || ((VR_PLATFORM_PIXMAP_STRUCT*)pixmap_output)->data == NULL)
	{
		NxErrMsg("Error: Out of memory at %s:%i\n", __FILE__, __LINE__);
		NX_FREE( result );
		return (HSURFTARGET)0;
	}
	
	/* Create a EGLSurface. */
	EGLSurface surface = eglCreatePixmapSurface(pStatics->egl_info.sEGLDisplay, pStatics->egl_info.sEGLConfig[VR_PROGRAM_CVT2Y], (EGLNativePixmapType)pixmap_output, NULL);
	if(surface == EGL_NO_SURFACE)
	{
		EGLint iError = eglGetError();
		NxErrMsg("eglGetError(): %i (0x%.4x)\n", (int)iError, (int)iError);
		NxErrMsg("Error: Failed to create EGL surface at %s:%i\n", __FILE__, __LINE__);
		nxGSurfaceDestroyPixmap(pixmap_output);
		NX_FREE( result );
		return (HSURFTARGET)0;
	}

	result->width          = uiWidth ;
	result->height         = uiHeight;
	result->target_native_pixmaps[0]  = pixmap_output;
	result->target_pixmap_surfaces[0] = surface;
	
	VR_INFO("", VR_GRAPHIC_DBG_TARGET, "nxGSurfaceCreateCvt2YTargetDefault\n"); 		
	
	return result;
}

static HSURFTARGET nxGSurfaceCreateCvt2UVTargetDefault  (unsigned int uiStride, unsigned int uiWidth, unsigned int uiHeight, NX_MEMORY_HANDLE hData)
{
	VR_ASSERT("width must be 2X and height must be 2X", uiWidth && uiHeight && ((uiWidth&0x1) == 0x0) && ((uiHeight&0x1) == 0x0) );

	//NxErrMsg("nxGSurfaceCreateCvt2UVTarget start(%dx%d)\n", uiWidth, uiHeight);
	VR_INFO("", VR_GRAPHIC_DBG_TARGET, "%s start\n", __FUNCTION__);

	Statics *pStatics = vrGetStatics();
	if( !pStatics ){ return (HSURFTARGET)0; }
	_AUTO_BACKUP_CURRENT_EGL_;
	HSURFTARGET result = (HSURFTARGET)NX_MALLOC(sizeof(vrSurfaceTarget));
	if( !result )
	{
		NxErrMsg("Error: Out of memory at %s:%i\n", __FILE__, __LINE__);
		return (HSURFTARGET)0;
	}
	NX_MEMSET( result, 0, sizeof(struct vrSurfaceTarget) );
	
	/* Create a EGLNativePixmapType. */
	EGLNativePixmapType pixmap_output = (EGLNativePixmapType)nxGSurfaceCreatePixmap(uiWidth, uiHeight, hData, VR_TRUE, 16);
	if(pixmap_output == NULL || ((VR_PLATFORM_PIXMAP_STRUCT*)pixmap_output)->data == NULL)
	{
		NxErrMsg("Error: Out of memory at %s:%i\n", __FILE__, __LINE__);
		NX_FREE( result );
		return (HSURFTARGET)0;
	}
	
	/* Create a EGLSurface. */
	EGLSurface surface = eglCreatePixmapSurface(pStatics->egl_info.sEGLDisplay, pStatics->egl_info.sEGLConfig[VR_PROGRAM_CVT2UV], (EGLNativePixmapType)pixmap_output, NULL);
	if(surface == EGL_NO_SURFACE)
	{
		EGLint iError = eglGetError();
		NxErrMsg("eglGetError(): %i (0x%.4x)\n", (int)iError, (int)iError);
		NxErrMsg("Error: Failed to create EGL surface at %s:%i\n", __FILE__, __LINE__);
		nxGSurfaceDestroyPixmap(pixmap_output);
		NX_FREE( result );
		return (HSURFTARGET)0;
	}

	result->width          = uiWidth ;
	result->height         = uiHeight;
	result->target_native_pixmaps[0]  = pixmap_output;
	result->target_pixmap_surfaces[0] = surface;	

	VR_INFO("", VR_GRAPHIC_DBG_TARGET, "nxGSurfaceCreateCvt2UVTargetDefault\n"); 		
	
	return result;
}


HSURFTARGET nxGSurfaceCreateCvt2YuvTarget  (unsigned int uiStride, unsigned int uiWidth, unsigned int uiHeight, NX_MEMORY_HANDLE hData, int iIsDefault)
{
	VR_ASSERT("width must be 8X and height must be 2X", uiWidth && uiHeight && ((uiWidth&0x1) == 0x0) && ((uiHeight&0x1) == 0x0) );

	Statics *pStatics = vrGetStatics();
	if( !pStatics || !hData ){ return VR_FALSE; }
	_AUTO_BACKUP_CURRENT_EGL_;

	VR_INFO("", VR_GRAPHIC_DBG_TARGET, "%s start\n", __FUNCTION__);

	VRImageFormatMode format[2] = { VR_IMAGE_FORMAT_UV, VR_IMAGE_FORMAT_Y };
	unsigned int target_ctx_bits[2] = {16, 8};
	
	HSURFTARGET result = (HSURFTARGET)NX_MALLOC(sizeof(vrSurfaceTarget));
	if( !result )
	{
		NxErrMsg("Error: Out of memory at %s:%i\n", __FILE__, __LINE__);
		return (HSURFTARGET)0;
	}
	NX_MEMSET( result, 0, sizeof(struct vrSurfaceTarget) );
	result->width		   = uiWidth ;
	result->height		   = uiHeight;
	#ifdef VR_FEATURE_STRIDE_USE
	result->stride		   = uiStride;
	#endif

	unsigned int pixmap_stride, pixmap_width, pixmap_height;
	if(!uiStride){ uiStride = uiWidth; }
	for(int i = 0 ; i < 2/*UV*/ ; i++)
	{
		if(1 == i)
		{
			//Y case
			pixmap_stride = uiStride;
			pixmap_width = uiWidth;
			pixmap_height = uiHeight;
		}
		else
		{
			//UV case
			pixmap_stride = uiStride/2;
			pixmap_width = uiWidth/2;
			pixmap_height = uiHeight + uiHeight/2;
			#if (VR_FEATURE_OUTPUT_HEIGHT_ALIGN_BYTE > 0)
			{
				//add Y align blank offset
				unsigned int y_height, y_align_blank_height = 0;
				y_height = uiHeight;
				if(y_height % VR_FEATURE_OUTPUT_HEIGHT_ALIGN_BYTE)
				{	
					y_align_blank_height = (VR_FEATURE_OUTPUT_HEIGHT_ALIGN_BYTE - (y_height % VR_FEATURE_OUTPUT_HEIGHT_ALIGN_BYTE));
				}
				pixmap_height += y_align_blank_height;
				VR_INFO("", VR_GRAPHIC_DBG_HEIGHT_ALIGN, "[%d] y_align_blank_height(%d)\n", i, y_align_blank_height);		
			}
			#endif			
		}
		
		/* Create a EGLNativePixmapType. */
		#ifdef VR_FEATURE_STRIDE_USE
		pixmap_width = pixmap_stride;
		#endif
		EGLNativePixmapType pixmap_output = (EGLNativePixmapType)nxGSurfaceCreatePixmap(pixmap_width, pixmap_height, hData, VR_TRUE, target_ctx_bits[i]);
		if(pixmap_output == NULL || ((VR_PLATFORM_PIXMAP_STRUCT*)pixmap_output)->data == NULL)
		{
			NxErrMsg("Error: Out of memory at %s:%i\n", __FILE__, __LINE__);
			NX_FREE( result );
			return (HSURFTARGET)0;
		}
		
		/* Create a EGLSurface. */
		EGLSurface surface = eglCreatePixmapSurface(pStatics->egl_info.sEGLDisplay, pStatics->egl_info.sEGLConfig[VR_PROGRAM_CVT2UV+i], (EGLNativePixmapType)pixmap_output, NULL);
		if(surface == EGL_NO_SURFACE)
		{
			EGLint iError = eglGetError();
			NxErrMsg("eglGetError(): %i (0x%.4x)\n", (int)iError, (int)iError);
			NxErrMsg("Error: Failed to create EGL surface at %s:%i\n", __FILE__, __LINE__);
			nxGSurfaceDestroyPixmap(pixmap_output);
			NX_FREE( result );
			return (HSURFTARGET)0;
		}

		result->target_native_pixmaps[i]  = pixmap_output;
		result->target_pixmap_surfaces[i] = surface;	

		VR_INFO("", VR_GRAPHIC_DBG_TARGET, "size(%dx%d, %d)\n", pixmap_width, pixmap_height, pixmap_stride);		

		if(iIsDefault)
		{
			break; 
		}		
		
		/* Increase Ctx ref. */
		if(!iIsDefault) ++pStatics->egl_info.sEGLContextRef[VR_PROGRAM_CVT2UV+i];
	}

	VR_INFO("", VR_GRAPHIC_DBG_TARGET, "nxGSurfaceCreateCvt2YuvTarget, 8ctx ref(%d), 16ctx ref(%d)\n", 
		pStatics->egl_info.sEGLContextRef[VR_PROGRAM_CVT2Y], pStatics->egl_info.sEGLContextRef[VR_PROGRAM_CVT2UV]); 	
	
	return result;
}

void nxGSurfaceDestroyCvt2YuvTarget ( HSURFTARGET hTarget, int iIsDefault )
{
	if( !hTarget ){ return; }
	Statics *pStatics = vrGetStatics();
	_AUTO_BACKUP_CURRENT_EGL_;
			
	nxGSurfaceDestroySurfaceTarget(hTarget);

	/* Decrease Ctx ref. */
	for(int i = 0 ; i < 2/*UV*/ ; i++)
	{
		if(!iIsDefault)
		{
			VR_ASSERT("Ref must be greater than 0", pStatics->egl_info.sEGLContextRef[VR_PROGRAM_CVT2UV+i] > 0);
			--pStatics->egl_info.sEGLContextRef[VR_PROGRAM_CVT2UV+i];
		}
	}
	VR_INFO("", VR_GRAPHIC_DBG_TARGET, "nxGSurfaceDestroyScaleTarget done, 8ctx ref(%d), 16ctx ref(%d)\n", 
			pStatics->egl_info.sEGLContextRef[VR_PROGRAM_CVT2Y], pStatics->egl_info.sEGLContextRef[VR_PROGRAM_CVT2UV]); 		
}

HSURFTARGET nxGSurfaceCreateCvt2RgbaTarget  (unsigned int uiStride, unsigned int uiWidth, unsigned int uiHeight, NX_MEMORY_HANDLE hData, int iIsDefault)
{
	VR_ASSERT("width must be 8X and height must be 2X", uiWidth && uiHeight && ((uiWidth&0x1) == 0x0) && ((uiHeight&0x1) == 0x0) );

	Statics *pStatics = vrGetStatics();
	if( !pStatics ){ return (HSURFTARGET)0; }
	_AUTO_BACKUP_CURRENT_EGL_;
	VR_INFO("", VR_GRAPHIC_DBG_TARGET, "%s start\n", __FUNCTION__);

	HSURFTARGET result = (HSURFTARGET)NX_MALLOC(sizeof(vrSurfaceTarget));
	if( !result )
	{
		NxErrMsg("Error: Out of memory at %s:%i\n", __FILE__, __LINE__);
		return (HSURFTARGET)0;
	}
	NX_MEMSET( result, 0, sizeof(struct vrSurfaceTarget) );
	result->width          = uiWidth ;
	result->height         = uiHeight;
	#ifdef VR_FEATURE_STRIDE_USE
	result->stride		   = uiStride;
	#endif	

	/* Create a EGLNativePixmapType. */
	unsigned int pixmap_stride, pixmap_width, pixmap_height;
	if(!uiStride){ uiStride = uiWidth; }
	pixmap_stride = uiStride;
	pixmap_width = uiWidth;
	pixmap_height = uiHeight;

	/* Create a EGLNativePixmapType. */
	#ifdef VR_FEATURE_STRIDE_USE
	pixmap_width = pixmap_stride;
	#endif			
	EGLNativePixmapType pixmap_output = (EGLNativePixmapType)nxGSurfaceCreatePixmap(pixmap_width, pixmap_height, hData, VR_TRUE, 32);
	if(pixmap_output == NULL || ((VR_PLATFORM_PIXMAP_STRUCT*)pixmap_output)->data == NULL)
	{
		NxErrMsg("Error: Out of memory at %s:%i\n", __FILE__, __LINE__);
		NX_FREE( result );
		return (HSURFTARGET)0;
	}
	
	/* Create a EGLSurface. */
	EGLSurface surface = eglCreatePixmapSurface(pStatics->egl_info.sEGLDisplay, pStatics->egl_info.sEGLConfig[VR_PROGRAM_CVT2RGBA], (EGLNativePixmapType)pixmap_output, NULL);
	if(surface == EGL_NO_SURFACE)
	{
		EGLint iError = eglGetError();
		NxErrMsg("eglGetError(): %i (0x%.4x)\n", (int)iError, (int)iError);
		NxErrMsg("Error: Failed to create EGL surface at %s:%i\n", __FILE__, __LINE__);
		nxGSurfaceDestroyPixmap(pixmap_output);
		NX_FREE( result );
		return (HSURFTARGET)0;
	}

	result->target_native_pixmaps[0]  = pixmap_output;
	result->target_pixmap_surfaces[0] = surface;	

	VR_INFO("", VR_GRAPHIC_DBG_TARGET, "size(%dx%d, %d)\n", pixmap_width, pixmap_height, pixmap_stride);		

	/* Increase Ctx ref. */
	if(!iIsDefault) ++pStatics->egl_info.sEGLContextRef[VR_PROGRAM_CVT2RGBA];
	VR_INFO("", VR_GRAPHIC_DBG_TARGET, "nxGSurfaceCreateCvt2RgbaTarget, 32ctx ref(%d)\n", pStatics->egl_info.sEGLContextRef[VR_PROGRAM_CVT2RGBA]); 		
	
	return result;
}

void nxGSurfaceDestroyCvt2RgbaTarget ( HSURFTARGET hTarget, int iIsDefault )
{
	if( !hTarget ){ return; }
	Statics *pStatics = vrGetStatics();
	_AUTO_BACKUP_CURRENT_EGL_;
		
	nxGSurfaceDestroySurfaceTarget(hTarget);

	/* Decrease Ctx ref. */
	if(!iIsDefault)
	{
		VR_ASSERT("Ref must be greater than 0", pStatics->egl_info.sEGLContextRef[VR_PROGRAM_CVT2RGBA] > 0);
		--pStatics->egl_info.sEGLContextRef[VR_PROGRAM_CVT2RGBA];
	}
	VR_INFO("", VR_GRAPHIC_DBG_TARGET, "nxGSurfaceDestroyCvt2RgbaTarget done, 32ctx ref(%d)\n", pStatics->egl_info.sEGLContextRef[VR_PROGRAM_CVT2RGBA]); 			
}

HSURFTARGET nxGSurfaceCreateUserFilterTarget  (unsigned int uiStride, unsigned int uiWidth, unsigned int uiHeight, NX_MEMORY_HANDLE hData, int iIsDefault)
{
	VR_ASSERT("width must be 8X and height must be 2X", uiWidth && uiHeight && ((uiWidth&0x1) == 0x0) && ((uiHeight&0x1) == 0x0) );

	Statics *pStatics = vrGetStatics();
	if( !pStatics ){ return (HSURFTARGET)0; }
	_AUTO_BACKUP_CURRENT_EGL_;
	VR_INFO("", VR_GRAPHIC_DBG_TARGET, "%s start\n", __FUNCTION__);

	HSURFTARGET result = (HSURFTARGET)NX_MALLOC(sizeof(vrSurfaceTarget));
	if( !result )
	{
		NxErrMsg("Error: Out of memory at %s:%i\n", __FILE__, __LINE__);
		return (HSURFTARGET)0;
	}
	NX_MEMSET( result, 0, sizeof(struct vrSurfaceTarget) );
	result->width          = uiWidth ;
	result->height         = uiHeight;
#ifdef VR_FEATURE_STRIDE_USE
	result->stride		   = uiStride;
#endif	

	/* Create a EGLNativePixmapType. */
	unsigned int pixmap_stride, pixmap_width, pixmap_height;
	if(!uiStride){ uiStride = uiWidth; }
	pixmap_stride = uiStride;
	pixmap_width = uiWidth;
	pixmap_height = uiHeight;

	/* Create a EGLNativePixmapType. */
#ifdef VR_FEATURE_STRIDE_USE
	pixmap_width = pixmap_stride;
#endif			
	EGLNativePixmapType pixmap_output = (EGLNativePixmapType)nxGSurfaceCreatePixmap(pixmap_width, pixmap_height, hData, VR_TRUE, 32);
	if(pixmap_output == NULL || ((VR_PLATFORM_PIXMAP_STRUCT*)pixmap_output)->data == NULL)
	{
		NxErrMsg("Error: Out of memory at %s:%i\n", __FILE__, __LINE__);
		NX_FREE( result );
		return (HSURFTARGET)0;
	}
	
	/* Create a EGLSurface. */
	EGLSurface surface = eglCreatePixmapSurface(pStatics->egl_info.sEGLDisplay, pStatics->egl_info.sEGLConfig[VR_PROGRAM_USERFILTER], (EGLNativePixmapType)pixmap_output, NULL);
	if(surface == EGL_NO_SURFACE)
	{
		EGLint iError = eglGetError();
		NxErrMsg("eglGetError(): %i (0x%.4x)\n", (int)iError, (int)iError);
		NxErrMsg("Error: Failed to create EGL surface at %s:%i\n", __FILE__, __LINE__);
		nxGSurfaceDestroyPixmap(pixmap_output);
		NX_FREE( result );
		return (HSURFTARGET)0;
	}

	result->target_native_pixmaps[0]  = pixmap_output;
	result->target_pixmap_surfaces[0] = surface;	

	VR_INFO("", VR_GRAPHIC_DBG_TARGET, "size(%dx%d, %d)\n", pixmap_width, pixmap_height, pixmap_stride);		

	/* Increase Ctx ref. */
	if(!iIsDefault) ++pStatics->egl_info.sEGLContextRef[VR_PROGRAM_USERFILTER];
	VR_INFO("", VR_GRAPHIC_DBG_TARGET, "nxGSurfaceCreateUserFilterTarget, 32ctx ref(%d)\n", pStatics->egl_info.sEGLContextRef[VR_PROGRAM_USERFILTER]); 			

	return result;
}

void nxGSurfaceDestroyUserFilterTarget ( HSURFTARGET hTarget, int iIsDefault )
{
	if( !hTarget ){ return; }
	Statics *pStatics = vrGetStatics();
	_AUTO_BACKUP_CURRENT_EGL_;
		
	nxGSurfaceDestroySurfaceTarget(hTarget);

	/* Decrease Ctx ref. */
	if(!iIsDefault)
	{
		VR_ASSERT("Ref must be greater than 0", pStatics->egl_info.sEGLContextRef[VR_PROGRAM_USERFILTER] > 0);
		--pStatics->egl_info.sEGLContextRef[VR_PROGRAM_USERFILTER];
	}
	VR_INFO("", VR_GRAPHIC_DBG_TARGET, "nxGSurfaceDestroyUserFilterTarget done, 32ctx ref(%d)\n", pStatics->egl_info.sEGLContextRef[VR_PROGRAM_USERFILTER]); 			
}

HSURFTARGET nxGSurfaceCreateYuvFilterTarget  (unsigned int uiStride, unsigned int uiWidth, unsigned int uiHeight, NX_MEMORY_HANDLE hData, int iIsDefault)
{
	VR_ASSERT("width must be 2X and height must be 2X", uiWidth && uiHeight && ((uiWidth&0x1) == 0x0) && ((uiHeight&0x1) == 0x0) );

	Statics *pStatics = vrGetStatics();
	if( !pStatics || !hData ){ return VR_FALSE; }
	_AUTO_BACKUP_CURRENT_EGL_;

	VR_INFO("", VR_GRAPHIC_DBG_TARGET, "%s start\n", __FUNCTION__);

	VRImageFormatMode format[3] = { VR_IMAGE_FORMAT_V, VR_IMAGE_FORMAT_U, VR_IMAGE_FORMAT_Y };
	
	HSURFTARGET result = (HSURFTARGET)NX_MALLOC(sizeof(vrSurfaceTarget));
	if( !result )
	{
		NxErrMsg("Error: Out of memory at %s:%i\n", __FILE__, __LINE__);
		return (HSURFTARGET)0;
	}
	NX_MEMSET( result, 0, sizeof(struct vrSurfaceTarget) );
	result->width		   = uiWidth ;
	result->height		   = uiHeight;
	#ifdef VR_FEATURE_STRIDE_USE
	result->stride		   = uiStride;
	#endif

	unsigned int pixmap_stride, pixmap_width, pixmap_height;
	if(!uiStride){ uiStride = uiWidth; }
	for(int i = 0 ; i < VR_YUV_CNT ; i++)
	{
		if(2 == i)
		{
			//Y case
			pixmap_stride = uiStride;
			pixmap_width = uiWidth;
			pixmap_height = uiHeight;
		}
		else if(1 == i)
		{
			//U case
			pixmap_stride = uiWidth/2 + (uiStride-uiWidth)/2;
			pixmap_width = uiWidth/2;
			pixmap_height = uiHeight*2 + uiHeight/2;
			#if (VR_FEATURE_OUTPUT_HEIGHT_ALIGN_BYTE > 0)
			{
				//add Y align blank offset
				unsigned int y_height, y_align_blank_height = 0;
				y_height = uiHeight;
				if(y_height % VR_FEATURE_OUTPUT_HEIGHT_ALIGN_BYTE)
				{	
					y_align_blank_height = (VR_FEATURE_OUTPUT_HEIGHT_ALIGN_BYTE - (y_height % VR_FEATURE_OUTPUT_HEIGHT_ALIGN_BYTE)) * 2;
				}
				pixmap_height += y_align_blank_height;
				VR_INFO("", VR_GRAPHIC_DBG_HEIGHT_ALIGN, "[%d] y_align_blank_height(%d)\n", i, y_align_blank_height);		
			}
			#endif
		}
		else
		{
			//V case
			pixmap_stride = uiWidth/2 + (uiStride-uiWidth)/2;
			pixmap_width = uiWidth/2;
			pixmap_height = uiHeight*2 + uiHeight;
			#if (VR_FEATURE_OUTPUT_HEIGHT_ALIGN_BYTE > 0)
			{
				unsigned int y_height, y_align_blank_height = 0;
				unsigned int u_height, u_align_blank_height = 0;
				//add Y align blank offset
				y_height = uiHeight;
				if(y_height % VR_FEATURE_OUTPUT_HEIGHT_ALIGN_BYTE)
				{	
					y_align_blank_height = (VR_FEATURE_OUTPUT_HEIGHT_ALIGN_BYTE - (y_height % VR_FEATURE_OUTPUT_HEIGHT_ALIGN_BYTE)) * 2;
				}
				//add U align blank offset
				u_height = uiHeight/2;
				if(u_height % VR_FEATURE_OUTPUT_HEIGHT_ALIGN_BYTE)
				{	
					u_align_blank_height = (VR_FEATURE_OUTPUT_HEIGHT_ALIGN_BYTE - (u_height % VR_FEATURE_OUTPUT_HEIGHT_ALIGN_BYTE));
				}
				pixmap_height += (y_align_blank_height + u_align_blank_height);
				VR_INFO("", VR_GRAPHIC_DBG_HEIGHT_ALIGN, "[%d] y_align_blank_height(%d), u_align_blank_height(%d)\n", i, y_align_blank_height, u_align_blank_height);		
			}
			#endif
		}
		
		/* Create a EGLNativePixmapType. */
		#ifdef VR_FEATURE_STRIDE_USE
		pixmap_width = pixmap_stride;
		#endif
		EGLNativePixmapType pixmap_output = (EGLNativePixmapType)nxGSurfaceCreatePixmap(pixmap_width, pixmap_height, hData, VR_TRUE, 8);
		if(pixmap_output == NULL || ((VR_PLATFORM_PIXMAP_STRUCT*)pixmap_output)->data == NULL)
		{
			NxErrMsg("Error: Out of memory at %s:%i\n", __FILE__, __LINE__);
			NX_FREE( result );
			return (HSURFTARGET)0;
		}
		
		/* Create a EGLSurface. */
		EGLSurface surface = eglCreatePixmapSurface(pStatics->egl_info.sEGLDisplay, pStatics->egl_info.sEGLConfig[VR_PROGRAM_YUVFILTER_Y], (EGLNativePixmapType)pixmap_output, NULL);
		if(surface == EGL_NO_SURFACE)
		{
			EGLint iError = eglGetError();
			NxErrMsg("eglGetError(): %i (0x%.4x)\n", (int)iError, (int)iError);
			NxErrMsg("Error: Failed to create EGL surface at %s:%i\n", __FILE__, __LINE__);
			nxGSurfaceDestroyPixmap(pixmap_output);
			NX_FREE( result );
			return (HSURFTARGET)0;
		}

		result->target_native_pixmaps[i]  = pixmap_output;
		result->target_pixmap_surfaces[i] = surface;	

		VR_INFO("", VR_GRAPHIC_DBG_TARGET, "size(%dx%d, %d)\n", pixmap_width, pixmap_height, pixmap_stride);		

		if(iIsDefault)
		{
			break; 
		}		
	}
	/* Increase Ctx ref. */
	if(!iIsDefault) ++pStatics->egl_info.sEGLContextRef[VR_PROGRAM_YUVFILTER_Y];
	VR_INFO("", VR_GRAPHIC_DBG_TARGET, "nxGSurfaceCreateYuvFilterTarget, 8ctx ref(%d)\n", pStatics->egl_info.sEGLContextRef[VR_PROGRAM_YUVFILTER_Y]); 	
	
	return result;
}

void nxGSurfaceDestroyYuvFilterTarget ( HSURFTARGET hTarget, int iIsDefault )
{
	if( !hTarget ){ return; }
	Statics *pStatics = vrGetStatics();
	_AUTO_BACKUP_CURRENT_EGL_;
			
	nxGSurfaceDestroySurfaceTarget(hTarget);

	/* Decrease Ctx ref. */
	if(!iIsDefault)
	{
		VR_ASSERT("Ref must be greater than 0", pStatics->egl_info.sEGLContextRef[VR_PROGRAM_YUVFILTER_Y] > 0);
		--pStatics->egl_info.sEGLContextRef[VR_PROGRAM_YUVFILTER_Y];
	}
	VR_INFO("", VR_GRAPHIC_DBG_TARGET, "nxGSurfaceDestroyYuvFilterTarget done, 8ctx ref(%d)\n", pStatics->egl_info.sEGLContextRef[VR_PROGRAM_YUVFILTER_Y]); 		
}
#endif

HSURFTARGET nxGSurfaceCreateScaleRGBATarget  (unsigned int uiStride, unsigned int uiWidth, unsigned int uiHeight, NX_MEMORY_HANDLE hData, int iIsDefault)
{
	//VR_ASSERT("width must be 8X and height must be 2X", uiWidth && uiHeight && ((uiWidth&0x1) == 0x0) && ((uiHeight&0x1) == 0x0) );

	Statics *pStatics = vrGetStatics();
	if( !pStatics ){ return (HSURFTARGET)0; }
	_AUTO_BACKUP_CURRENT_EGL_;
	VR_INFO("", VR_GRAPHIC_DBG_TARGET, "%s start\n", __FUNCTION__);

	HSURFTARGET result = (HSURFTARGET)NX_MALLOC(sizeof(vrSurfaceTarget));
	if( !result )
	{
		NxErrMsg("Error: Out of memory at %s:%i\n", __FILE__, __LINE__);
		return (HSURFTARGET)0;
	}
	NX_MEMSET( result, 0, sizeof(struct vrSurfaceTarget) );
	result->width		   = uiWidth ;
	result->height		   = uiHeight;
#ifdef VR_FEATURE_STRIDE_USE
	result->stride		   = uiStride;
#endif	

	/* Create a EGLNativePixmapType. */
	unsigned int pixmap_stride, pixmap_width, pixmap_height;
	if(!uiStride){ uiStride = uiWidth; }
	pixmap_stride = uiStride;
	pixmap_width = uiWidth;
	pixmap_height = uiHeight;

	/* Create a EGLNativePixmapType. */
#ifdef VR_FEATURE_STRIDE_USE
	pixmap_width = pixmap_stride;
#endif			
	EGLNativePixmapType pixmap_output = (EGLNativePixmapType)nxGSurfaceCreatePixmap(pixmap_width, pixmap_height, hData, VR_TRUE, 32);
	if(pixmap_output == NULL || ((VR_PLATFORM_PIXMAP_STRUCT*)pixmap_output)->data == NULL)
	{
		NxErrMsg("Error: Out of memory at %s:%i\n", __FILE__, __LINE__);
		NX_FREE( result );
		return (HSURFTARGET)0;
	}
	
	/* Create a EGLSurface. */
	EGLSurface surface = eglCreatePixmapSurface(pStatics->egl_info.sEGLDisplay, pStatics->egl_info.sEGLConfig[VR_PROGRAM_SCALE_RGBA], (EGLNativePixmapType)pixmap_output, NULL);
	if(surface == EGL_NO_SURFACE)
	{
		EGLint iError = eglGetError();
		NxErrMsg("eglGetError(): %i (0x%.4x)\n", (int)iError, (int)iError);
		NxErrMsg("Error: Failed to create EGL surface at %s:%i\n", __FILE__, __LINE__);
		nxGSurfaceDestroyPixmap(pixmap_output);
		NX_FREE( result );
		return (HSURFTARGET)0;
	}

	#ifdef VR_FEATURE_SEPERATE_FD_USE
	result->target_native_pixmap  = pixmap_output;
	result->target_pixmap_surface = surface;	
	#else
	result->target_native_pixmaps[0]  = pixmap_output;
	result->target_pixmap_surfaces[0] = surface;	
	#endif
	VR_INFO("", VR_GRAPHIC_DBG_TARGET, "size(%dx%d, %d)\n", pixmap_width, pixmap_height, pixmap_stride);		

	/* Increase Ctx ref. */
	if(!iIsDefault) ++pStatics->egl_info.sEGLContextRef[VR_PROGRAM_SCALE_RGBA];
	VR_INFO("", VR_GRAPHIC_DBG_TARGET, "nxGSurfaceCreateScaleRGBATarget, 32ctx ref(%d)\n", pStatics->egl_info.sEGLContextRef[VR_PROGRAM_SCALE_RGBA]);			

	return result;
}

void nxGSurfaceDestroyScaleRGBATarget ( HSURFTARGET hTarget, int iIsDefault )
{
	if( !hTarget ){ return; }
	Statics *pStatics = vrGetStatics();
	_AUTO_BACKUP_CURRENT_EGL_;
		
	nxGSurfaceDestroySurfaceTarget(hTarget);

	/* Decrease Ctx ref. */
	if(!iIsDefault)
	{
		VR_ASSERT("Ref must be greater than 0", pStatics->egl_info.sEGLContextRef[VR_PROGRAM_SCALE_RGBA] > 0);
		--pStatics->egl_info.sEGLContextRef[VR_PROGRAM_SCALE_RGBA];
	}
	VR_INFO("", VR_GRAPHIC_DBG_TARGET, "nxGSurfaceDestroyScaleRGBATarget done, 32ctx ref(%d)\n", pStatics->egl_info.sEGLContextRef[VR_PROGRAM_SCALE_RGBA]);			
}

#ifdef VR_FEATURE_SEPERATE_FD_USE
HSURFSOURCE nxGSurfaceCreateDeinterlaceSource  (unsigned int uiWidth, unsigned int uiHeight, NX_MEMORY_HANDLE hData)
{
	Statics *pStatics = vrGetStatics();
	if( !pStatics ){ return (HSURFSOURCE)0; }
	_AUTO_BACKUP_CURRENT_EGL_;
	HSURFSOURCE result = (HSURFSOURCE)NX_MALLOC(sizeof(struct vrSurfaceSource));
	if( !result )
	{
		NxErrMsg("Error: Out of memory at %s:%i\n", __FILE__, __LINE__);
		return (HSURFSOURCE)0;
	}
	NX_MEMSET( result, 0, sizeof(struct vrSurfaceSource) );
	
	/* Make context current. */
	EGLBoolean bResult = eglMakeCurrent(pStatics->egl_info.sEGLDisplay, pStatics->default_target[VR_PROGRAM_DEINTERLACE]->target_pixmap_surface, 
								pStatics->default_target[VR_PROGRAM_DEINTERLACE]->target_pixmap_surface, pStatics->egl_info.sEGLContext[VR_PROGRAM_DEINTERLACE]);
	if(bResult == EGL_FALSE)
	{
		EGLint iError = eglGetError();
		NxErrMsg("eglGetError(): %i (0x%.4x)\n", (int)iError, (int)iError);
		NxErrMsg("Error: Failed to make context current at %s:%i\n", __FILE__, __LINE__);
		return 0;	
	}

	/* Y4���� �Ѳ�����*/
	EGLNativePixmapType pixmapInput = (EGLNativePixmapType)nxGSurfaceCreatePixmap(uiWidth/4, uiHeight, hData, VR_TRUE, 32);
	if(pixmapInput == NULL || ((VR_PLATFORM_PIXMAP_STRUCT*)pixmapInput)->data == NULL)
	{
		NxErrMsg("Error: Out of memory at %s:%i\n", __FILE__, __LINE__);\
		NX_FREE(result);
		return (HSURFSOURCE)0;
	}

	//RGB is not supported	
	EGLint imageAttributes[] = {
		EGL_IMAGE_PRESERVED_KHR, EGL_TRUE, 
		EGL_NONE
	};	
	EGLImageKHR eglImage = EGL_CHECK(_NX_eglCreateImageKHR( pStatics->egl_info.sEGLDisplay, EGL_NO_CONTEXT, 
							           EGL_NATIVE_PIXMAP_KHR, (EGLClientBuffer)pixmapInput, imageAttributes));	

	GLuint textureName;
	GL_CHECK(glGenTextures(1, &textureName));
	GL_CHECK(glActiveTexture(GL_TEXTURE0 + VR_INPUT_MODE_DEINTERLACE));
	GL_CHECK(glBindTexture(GL_TEXTURE_2D, textureName));
	GL_CHECK(glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST));
	GL_CHECK(glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST));
	GL_CHECK(glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE));
	GL_CHECK(glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE));
	GL_CHECK(_NX_glEGLImageTargetTexture2DOES( GL_TEXTURE_2D, (GLeglImageOES)eglImage));		

	result->width        = uiWidth ;
	result->height       = uiHeight;
	result->texture_name[VR_INPUT_MODE_Y] = textureName;
	result->src_native_pixmaps[VR_INPUT_MODE_Y]= pixmapInput;
	result->src_pixmap_images[VR_INPUT_MODE_Y] = eglImage   ;	

	VR_INFO("", VR_GRAPHIC_DBG_SOURCE, "nxGSurfaceCreateDeinterlaceSource done\n"); 		
	return result;
}

void nxGSurfaceDestroyDeinterlaceSource ( HSURFSOURCE hSource )
{
	if( !hSource ){ return; }
	Statics *pStatics = vrGetStatics();
	_AUTO_BACKUP_CURRENT_EGL_;
	GL_CHECK(glDeleteTextures(1,&hSource->texture_name[VR_INPUT_MODE_Y]));
	EGL_CHECK(_NX_eglDestroyImageKHR(pStatics->egl_info.sEGLDisplay, hSource->src_pixmap_images[VR_INPUT_MODE_Y]));
	nxGSurfaceDestroyPixmap(hSource->src_native_pixmaps[VR_INPUT_MODE_Y]);
	NX_FREE(hSource);
	VR_INFO("", VR_GRAPHIC_DBG_SOURCE, "nxGSurfaceDestroyDeinterlaceSource done\n"); 		
}

int nxGSurfaceCreateScaleSource  (HSURFSOURCE* psource ,unsigned int uiWidth, unsigned int uiHeight, NX_MEMORY_HANDLE* pData)
{
	Statics *pStatics = vrGetStatics();
	psource[0] = NULL;
	psource[1] = NULL;
	psource[2] = NULL;

	if( !pStatics || !pData || !pData[0] || !pData[1] || !pData[2] )
	{	
		for(int i = 0 ; i < VR_YUV_CNT ; i++){ psource[i] = NULL; }
		NxErrMsg("Error: input is not valid\n");		
		return VR_FALSE; 
	}

	_AUTO_BACKUP_CURRENT_EGL_;
	VRImageFormatMode format[3] = { VR_IMAGE_FORMAT_Y, VR_IMAGE_FORMAT_U, VR_IMAGE_FORMAT_V };

	/* Make context current. */
	EGLBoolean bResult = eglMakeCurrent(pStatics->egl_info.sEGLDisplay, pStatics->default_target[VR_PROGRAM_SCALE]->target_pixmap_surface, 
									pStatics->default_target[VR_PROGRAM_SCALE]->target_pixmap_surface, pStatics->egl_info.sEGLContext[VR_PROGRAM_SCALE]);
	if(bResult == EGL_FALSE)
	{
		EGLint iError = eglGetError();
		NxErrMsg("eglGetError(): %i (0x%.4x)\n", (int)iError, (int)iError);
		NxErrMsg("Error: Failed to make context current at %s:%i\n", __FILE__, __LINE__);
		for(int i = 0 ; i < VR_YUV_CNT ; i++){ psource[i] = NULL; }
		return VR_FALSE;	
	}
	
	unsigned int pixmap_width, pixmap_height;
	for(int i = 0 ; i < VR_YUV_CNT ; i++)
	{	
		if(!pData[i])
		{
			VR_ASSERT("Error: Y must exist", i != 0);				
			continue;
		}
		HSURFSOURCE result = (HSURFSOURCE)NX_MALLOC(sizeof(struct vrSurfaceSource));
		if( !result )
		{
			NxErrMsg("Error: Out of memory at %s:%i\n", __FILE__, __LINE__);
			for(int i = 0 ; i < VR_YUV_CNT ; i++){ psource[i] = NULL; }
			return VR_FALSE;
		}
		NX_MEMSET( result, 0, sizeof(struct vrSurfaceSource) );

		if(0 == i)
		{
			//Y case
			pixmap_width = uiWidth;
			pixmap_height = uiHeight;
		}
		else if(1 == i)
		{
			//U case
			pixmap_width = uiWidth/2;
			pixmap_height = uiHeight/2;
		}
		else
		{
			//V case
			pixmap_width = uiWidth/2;
			pixmap_height = uiHeight/2;
		}

		EGLNativePixmapType pixmapInput = (EGLNativePixmapType)nxGSurfaceCreatePixmap(pixmap_width, pixmap_height, pData[i], VR_TRUE, 8);
		if(pixmapInput == NULL || ((VR_PLATFORM_PIXMAP_STRUCT*)pixmapInput)->data == NULL)
		{
			NxErrMsg("Error: Out of memory at %s:%i\n", __FILE__, __LINE__);\
			NX_FREE(result);
			for(int i = 0 ; i < VR_YUV_CNT ; i++){ psource[i] = NULL; }
			return VR_FALSE;
		}

		//RGB is not supported	
		EGLint imageAttributes[] = {
			EGL_IMAGE_PRESERVED_KHR, /*EGL_TRUE*/EGL_FALSE, 
			EGL_NONE
		};	
		EGLImageKHR eglImage = EGL_CHECK(_NX_eglCreateImageKHR( pStatics->egl_info.sEGLDisplay, EGL_NO_CONTEXT, 
								           EGL_NATIVE_PIXMAP_KHR, (EGLClientBuffer)pixmapInput, imageAttributes));	

		GLuint textureName;
		GL_CHECK(glGenTextures(1, &textureName));
		GL_CHECK(glActiveTexture(GL_TEXTURE0 + VR_INPUT_MODE_TEXTURE0));
		GL_CHECK(glBindTexture(GL_TEXTURE_2D, textureName));
		GL_CHECK(glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR));
		GL_CHECK(glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR));
		GL_CHECK(glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE));
		GL_CHECK(glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE));
		GL_CHECK(_NX_glEGLImageTargetTexture2DOES( GL_TEXTURE_2D, (GLeglImageOES)eglImage));		

		result->width        = pixmap_width ;
		result->height       = pixmap_height;
		result->texture_name[0] = textureName;
		result->src_native_pixmaps[0]= pixmapInput;
		result->src_pixmap_images[0] = eglImage   ;		

		psource[i] = result;
		VR_INFO("", VR_GRAPHIC_DBG_SOURCE, "nxGSurfaceCreateScaleSource done(%d)\n", i);		
	}
	return VR_TRUE;
}

void nxGSurfaceDestroyScaleSource ( HSURFSOURCE* psource )
{
	if( !psource ){ return; }
	Statics *pStatics = vrGetStatics();
	_AUTO_BACKUP_CURRENT_EGL_;
	
	for(int i = 0 ; i < VR_YUV_CNT ; i++)
	{
		if(!psource[i])
		{
			VR_ASSERT("Error: Y must exist", i != 0);				
			continue;
		}
		
		GL_CHECK(glDeleteTextures(1,&(psource[i]->texture_name[0])));
		EGL_CHECK(_NX_eglDestroyImageKHR(pStatics->egl_info.sEGLDisplay, psource[i]->src_pixmap_images[0]));
		nxGSurfaceDestroyPixmap(psource[i]->src_native_pixmaps[0]);
		NX_FREE(psource[i]);
		VR_INFO("", VR_GRAPHIC_DBG_SOURCE, "nxGSurfaceDestroyScaleSource done(%d)\n", i);		
	}
}

HSURFSOURCE nxGSurfaceCreateCvt2YSource  (unsigned int uiWidth, unsigned int uiHeight, NX_MEMORY_HANDLE hData)
{
	//NxErrMsg("nxGSurfaceCreateCvt2YSource start(%dx%d)\n", uiWidth, uiHeight);

	Statics *pStatics = vrGetStatics();
	if( !pStatics ){ return (HSURFSOURCE)0; }
	_AUTO_BACKUP_CURRENT_EGL_;
	HSURFSOURCE result = (HSURFSOURCE)NX_MALLOC(sizeof(struct vrSurfaceSource));
	if( !result )
	{
		NxErrMsg("Error: Out of memory at %s:%i\n", __FILE__, __LINE__);
		return (HSURFSOURCE)0;
	}
	NX_MEMSET( result, 0, sizeof(struct vrSurfaceSource) );
	
	/* Make context current. */
	EGLBoolean bResult = eglMakeCurrent(pStatics->egl_info.sEGLDisplay, pStatics->default_target[VR_PROGRAM_CVT2Y]->target_pixmap_surface, 
								pStatics->default_target[VR_PROGRAM_CVT2Y]->target_pixmap_surface, pStatics->egl_info.sEGLContext[VR_PROGRAM_CVT2Y]);
	if(bResult == EGL_FALSE)
	{
		EGLint iError = eglGetError();
		NxErrMsg("eglGetError(): %i (0x%.4x)\n", (int)iError, (int)iError);
		NxErrMsg("Error: Failed to make context current at %s:%i\n", __FILE__, __LINE__);
		return 0;	
	}

	EGLNativePixmapType pixmapInput = (EGLNativePixmapType)nxGSurfaceCreatePixmap(uiWidth, uiHeight, hData, VR_FALSE, 32);
	if(pixmapInput == NULL || ((VR_PLATFORM_PIXMAP_STRUCT*)pixmapInput)->data == NULL)
	{
		NxErrMsg("Error: Out of memory at %s:%i\n", __FILE__, __LINE__);\
		NX_FREE(result);
		return (HSURFSOURCE)0;
	}

	//RGB is not supported	
	EGLint imageAttributes[] = {
		EGL_IMAGE_PRESERVED_KHR, EGL_TRUE, 
		EGL_NONE
	};	
	EGLImageKHR eglImage = EGL_CHECK(_NX_eglCreateImageKHR( pStatics->egl_info.sEGLDisplay, EGL_NO_CONTEXT, 
							           EGL_NATIVE_PIXMAP_KHR, (EGLClientBuffer)pixmapInput, imageAttributes));	

	GLuint textureName;
	GL_CHECK(glGenTextures(1, &textureName));
	GL_CHECK(glActiveTexture(GL_TEXTURE0 + VR_INPUT_MODE_TEXTURE0));
	GL_CHECK(glBindTexture(GL_TEXTURE_2D, textureName));
	GL_CHECK(glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR));
	GL_CHECK(glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR));
	GL_CHECK(glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE));
	GL_CHECK(glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE));
	GL_CHECK(_NX_glEGLImageTargetTexture2DOES( GL_TEXTURE_2D, (GLeglImageOES)eglImage));		

	result->width        = uiWidth ;
	result->height       = uiHeight;
	result->texture_name[VR_INPUT_MODE_Y] = textureName;
	result->src_native_pixmaps[VR_INPUT_MODE_Y]= pixmapInput;
	result->src_pixmap_images[VR_INPUT_MODE_Y] = eglImage   ;	

	VR_INFO("", VR_GRAPHIC_DBG_SOURCE, "nxGSurfaceCreateCvt2YSource done\n"); 		
	return result;
}

void nxGSurfaceDestroyCvt2YSource ( HSURFSOURCE hSource )
{
	if( !hSource ){ return; }
	Statics *pStatics = vrGetStatics();
	_AUTO_BACKUP_CURRENT_EGL_;
	GL_CHECK(glDeleteTextures(1,&hSource->texture_name[VR_INPUT_MODE_Y]));
	EGL_CHECK(_NX_eglDestroyImageKHR(pStatics->egl_info.sEGLDisplay, hSource->src_pixmap_images[VR_INPUT_MODE_Y]));
	nxGSurfaceDestroyPixmap(hSource->src_native_pixmaps[VR_INPUT_MODE_Y]);
	NX_FREE(hSource);
	VR_INFO("", VR_GRAPHIC_DBG_SOURCE, "nxGSurfaceDestroyCvt2YSource done\n"); 		
}

HSURFSOURCE nxGSurfaceCreateCvt2UVSource  (unsigned int uiWidth, unsigned int uiHeight, NX_MEMORY_HANDLE hData)
{
	//NxErrMsg("nxGSurfaceCreateCvt2UVSource start(%dx%d)\n", uiWidth, uiHeight);

	Statics *pStatics = vrGetStatics();
	if( !pStatics ){ return (HSURFSOURCE)0; }
	_AUTO_BACKUP_CURRENT_EGL_;
	HSURFSOURCE result = (HSURFSOURCE)NX_MALLOC(sizeof(struct vrSurfaceSource));
	if( !result )
	{
		NxErrMsg("Error: Out of memory at %s:%i\n", __FILE__, __LINE__);
		return (HSURFSOURCE)0;
	}
	NX_MEMSET( result, 0, sizeof(struct vrSurfaceSource) );
	
	/* Make context current. */
	EGLBoolean bResult = eglMakeCurrent(pStatics->egl_info.sEGLDisplay, pStatics->default_target[VR_PROGRAM_CVT2UV]->target_pixmap_surface, 
								pStatics->default_target[VR_PROGRAM_CVT2UV]->target_pixmap_surface, pStatics->egl_info.sEGLContext[VR_PROGRAM_CVT2UV]);
	if(bResult == EGL_FALSE)
	{
		EGLint iError = eglGetError();
		NxErrMsg("eglGetError(): %i (0x%.4x)\n", (int)iError, (int)iError);
		NxErrMsg("Error: Failed to make context current at %s:%i\n", __FILE__, __LINE__);
		return 0;	
	}

	EGLNativePixmapType pixmapInput = (EGLNativePixmapType)nxGSurfaceCreatePixmap(uiWidth, uiHeight, hData, VR_FALSE, 32);
	if(pixmapInput == NULL || ((VR_PLATFORM_PIXMAP_STRUCT*)pixmapInput)->data == NULL)
	{
		NxErrMsg("Error: Out of memory at %s:%i\n", __FILE__, __LINE__);\
		NX_FREE(result);
		return (HSURFSOURCE)0;
	}

	//RGB is not supported	
	EGLint imageAttributes[] = {
		EGL_IMAGE_PRESERVED_KHR, EGL_TRUE, 
		EGL_NONE
	};	
	EGLImageKHR eglImage = EGL_CHECK(_NX_eglCreateImageKHR( pStatics->egl_info.sEGLDisplay, EGL_NO_CONTEXT, 
							           EGL_NATIVE_PIXMAP_KHR, (EGLClientBuffer)pixmapInput, imageAttributes));	

	GLuint textureName;
	GL_CHECK(glGenTextures(1, &textureName));
	GL_CHECK(glActiveTexture(GL_TEXTURE0 + VR_INPUT_MODE_TEXTURE1));
	GL_CHECK(glBindTexture(GL_TEXTURE_2D, textureName));
	GL_CHECK(glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR));
	GL_CHECK(glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR));
	GL_CHECK(glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE));
	GL_CHECK(glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE));
	GL_CHECK(_NX_glEGLImageTargetTexture2DOES( GL_TEXTURE_2D, (GLeglImageOES)eglImage));		

	result->width        = uiWidth ;
	result->height       = uiHeight;
	result->texture_name[0] = textureName;
	result->src_native_pixmaps[0]= pixmapInput;
	result->src_pixmap_images[0] = eglImage   ;	

	VR_INFO("", VR_GRAPHIC_DBG_SOURCE, "nxGSurfaceCreateCvt2UVSource done\n"); 		
	return result;
}

void nxGSurfaceDestroyCvt2UVSource ( HSURFSOURCE hSource )
{
	if( !hSource ){ return; }
	Statics *pStatics = vrGetStatics();
	_AUTO_BACKUP_CURRENT_EGL_;
	GL_CHECK(glDeleteTextures(1,&hSource->texture_name[0]));
	EGL_CHECK(_NX_eglDestroyImageKHR(pStatics->egl_info.sEGLDisplay, hSource->src_pixmap_images[0]));
	nxGSurfaceDestroyPixmap(hSource->src_native_pixmaps[0]);
	NX_FREE(hSource);
	VR_INFO("", VR_GRAPHIC_DBG_SOURCE, "nxGSurfaceDestroyCvt2UVSource done\n"); 		
}

HSURFSOURCE nxGSurfaceCreateCvt2RgbaSource  (unsigned int uiWidth, unsigned int uiHeight, NX_MEMORY_HANDLE hDataY , NX_MEMORY_HANDLE hDataU, NX_MEMORY_HANDLE hDataV )
{
	Statics *pStatics = vrGetStatics();
	
	if( !pStatics ){ return (HSURFSOURCE)0; }
	_AUTO_BACKUP_CURRENT_EGL_;
	HSURFSOURCE result = (HSURFSOURCE)NX_MALLOC(sizeof(struct vrSurfaceSource));
	if( !result )
	{
		NxErrMsg("Error: Out of memory at %s:%i\n", __FILE__, __LINE__);
		return (HSURFSOURCE)0;
	}
	NX_MEMSET( result, 0, sizeof(struct vrSurfaceSource) );
	
	/* Make context current. */
	EGLBoolean bResult = eglMakeCurrent(pStatics->egl_info.sEGLDisplay, pStatics->default_target[VR_PROGRAM_CVT2RGBA]->target_pixmap_surface, 
								pStatics->default_target[VR_PROGRAM_CVT2RGBA]->target_pixmap_surface, pStatics->egl_info.sEGLContext[VR_PROGRAM_CVT2RGBA]);
	if(bResult == EGL_FALSE)
	{
		EGLint iError = eglGetError();
		NxErrMsg("eglGetError(): %i (0x%.4x)\n", (int)iError, (int)iError);
		NxErrMsg("Error: Failed to make context current at %s:%i\n", __FILE__, __LINE__);
		return 0;	
	}

	EGLNativePixmapType pixmapInput[VR_INPUT_MODE_YUV_MAX] = {NULL,};

	pixmapInput[VR_INPUT_MODE_Y] = (VR_PLATFORM_PIXMAP_STRUCT*)nxGSurfaceCreatePixmap(uiWidth, uiHeight, hDataY, VR_TRUE, 8);
	if(pixmapInput[VR_INPUT_MODE_Y] == NULL || ((VR_PLATFORM_PIXMAP_STRUCT*)pixmapInput[VR_INPUT_MODE_Y])->data == NULL)
	{
		NxErrMsg("Error: Out of memory at %s:%i\n", __FILE__, __LINE__);
		NX_FREE(result);
		return (HSURFSOURCE)0;
	}
	pixmapInput[VR_INPUT_MODE_U] = (VR_PLATFORM_PIXMAP_STRUCT*)nxGSurfaceCreatePixmap(uiWidth/2, uiHeight/2, hDataU, VR_TRUE, 8);
	if(pixmapInput[VR_INPUT_MODE_U] == NULL || ((VR_PLATFORM_PIXMAP_STRUCT*)pixmapInput[VR_INPUT_MODE_U])->data == NULL)
	{
		NxErrMsg("Error: Out of memory at %s:%i\n", __FILE__, __LINE__);
		NX_FREE(result);
		return (HSURFSOURCE)0;
	}
	pixmapInput[VR_INPUT_MODE_V] = (VR_PLATFORM_PIXMAP_STRUCT*)nxGSurfaceCreatePixmap(uiWidth/2, uiHeight/2, hDataV, VR_TRUE, 8);
	if(pixmapInput[VR_INPUT_MODE_V] == NULL || ((VR_PLATFORM_PIXMAP_STRUCT*)pixmapInput[VR_INPUT_MODE_V])->data == NULL)
	{
		NxErrMsg("Error: Out of memory at %s:%i\n", __FILE__, __LINE__);
		NX_FREE(result);
		return (HSURFSOURCE)0;
	}

	//RGB is not supported	
	EGLint imageAttributes[] = {
		EGL_IMAGE_PRESERVED_KHR, EGL_TRUE, 
		EGL_NONE
	};	
	EGLImageKHR eglImage[VR_INPUT_MODE_YUV_MAX] = {NULL,};
	eglImage[VR_INPUT_MODE_Y] = EGL_CHECK(_NX_eglCreateImageKHR( pStatics->egl_info.sEGLDisplay, EGL_NO_CONTEXT, 
							           EGL_NATIVE_PIXMAP_KHR, (EGLClientBuffer)pixmapInput[VR_INPUT_MODE_Y], imageAttributes));	
	eglImage[VR_INPUT_MODE_U] = EGL_CHECK(_NX_eglCreateImageKHR( pStatics->egl_info.sEGLDisplay, EGL_NO_CONTEXT, 
							           EGL_NATIVE_PIXMAP_KHR, (EGLClientBuffer)pixmapInput[VR_INPUT_MODE_U], imageAttributes));	
	eglImage[VR_INPUT_MODE_V] = EGL_CHECK(_NX_eglCreateImageKHR( pStatics->egl_info.sEGLDisplay, EGL_NO_CONTEXT, 
							           EGL_NATIVE_PIXMAP_KHR, (EGLClientBuffer)pixmapInput[VR_INPUT_MODE_V], imageAttributes));	

	GLuint textureName[VR_INPUT_MODE_YUV_MAX];
	GL_CHECK(glGenTextures(VR_INPUT_MODE_YUV_MAX, textureName));
	
	GL_CHECK(glActiveTexture(GL_TEXTURE0 + VR_INPUT_MODE_Y));
	GL_CHECK(glBindTexture(GL_TEXTURE_2D, textureName[VR_INPUT_MODE_Y]));
	GL_CHECK(glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR));
	GL_CHECK(glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR));
	GL_CHECK(glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE));
	GL_CHECK(glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE));
	GL_CHECK(_NX_glEGLImageTargetTexture2DOES( GL_TEXTURE_2D, (GLeglImageOES)eglImage[VR_INPUT_MODE_Y]));		
	result->texture_name[VR_INPUT_MODE_Y] = textureName[VR_INPUT_MODE_Y];
	result->src_native_pixmaps[VR_INPUT_MODE_Y]= pixmapInput[VR_INPUT_MODE_Y];
	result->src_pixmap_images[VR_INPUT_MODE_Y] = eglImage[VR_INPUT_MODE_Y]   ;				 

	GL_CHECK(glActiveTexture(GL_TEXTURE0 + VR_INPUT_MODE_U));
	GL_CHECK(glBindTexture(GL_TEXTURE_2D, textureName[VR_INPUT_MODE_U]));
	GL_CHECK(glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR));
	GL_CHECK(glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR));
	GL_CHECK(glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE));
	GL_CHECK(glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE));
	GL_CHECK(_NX_glEGLImageTargetTexture2DOES( GL_TEXTURE_2D, (GLeglImageOES)eglImage[VR_INPUT_MODE_U]));		
	result->texture_name[VR_INPUT_MODE_U] = textureName[VR_INPUT_MODE_U];
	result->src_native_pixmaps[VR_INPUT_MODE_U]= pixmapInput[VR_INPUT_MODE_U];
	result->src_pixmap_images[VR_INPUT_MODE_U] = eglImage[VR_INPUT_MODE_U]   ;				 

	GL_CHECK(glActiveTexture(GL_TEXTURE0 + VR_INPUT_MODE_V));
	GL_CHECK(glBindTexture(GL_TEXTURE_2D, textureName[VR_INPUT_MODE_V]));
	GL_CHECK(glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR));
	GL_CHECK(glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR));
	GL_CHECK(glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE));
	GL_CHECK(glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE));
	GL_CHECK(_NX_glEGLImageTargetTexture2DOES( GL_TEXTURE_2D, (GLeglImageOES)eglImage[VR_INPUT_MODE_V]));		
	result->texture_name[VR_INPUT_MODE_V] = textureName[VR_INPUT_MODE_V];
	result->src_native_pixmaps[VR_INPUT_MODE_V]= pixmapInput[VR_INPUT_MODE_V];
	result->src_pixmap_images[VR_INPUT_MODE_V] = eglImage[VR_INPUT_MODE_V]   ;				 
	
	result->width        = uiWidth ;
	result->height       = uiHeight;

	VR_INFO("", VR_GRAPHIC_DBG_SOURCE, "nxGSurfaceCreateCvt2RgbaSource done\n"); 		
	return result;
}

void nxGSurfaceDestroyCvt2RgbaSource ( HSURFSOURCE hSource )
{
	if( !hSource ){ return; }
	Statics *pStatics = vrGetStatics();
	_AUTO_BACKUP_CURRENT_EGL_;
	GL_CHECK(glDeleteTextures(VR_INPUT_MODE_YUV_MAX, hSource->texture_name));
	EGL_CHECK(_NX_eglDestroyImageKHR(pStatics->egl_info.sEGLDisplay, hSource->src_pixmap_images[VR_INPUT_MODE_Y]));
	EGL_CHECK(_NX_eglDestroyImageKHR(pStatics->egl_info.sEGLDisplay, hSource->src_pixmap_images[VR_INPUT_MODE_U]));
	EGL_CHECK(_NX_eglDestroyImageKHR(pStatics->egl_info.sEGLDisplay, hSource->src_pixmap_images[VR_INPUT_MODE_V]));
	nxGSurfaceDestroyPixmap(hSource->src_native_pixmaps[VR_INPUT_MODE_Y]);
	nxGSurfaceDestroyPixmap(hSource->src_native_pixmaps[VR_INPUT_MODE_U]);
	nxGSurfaceDestroyPixmap(hSource->src_native_pixmaps[VR_INPUT_MODE_V]);
	NX_FREE(hSource);
	VR_INFO("", VR_GRAPHIC_DBG_SOURCE, "nxGSurfaceDestroyCvt2RgbaSource done\n"); 		
}

int nxGSurfaceCreateYuvFilterSource  (HSURFSOURCE* psource ,unsigned int uiWidth, unsigned int uiHeight, NX_MEMORY_HANDLE* pData)
{
	Statics *pStatics = vrGetStatics();	
	psource[0] = NULL;
	psource[1] = NULL;
	psource[2] = NULL;

	if( !pStatics )
	{	
		for(int i = 0 ; i < VR_YUV_CNT ; i++){ psource[i] = NULL; }
		return VR_FALSE; 
	}
	_AUTO_BACKUP_CURRENT_EGL_;

	VRImageFormatMode format[3] = { VR_IMAGE_FORMAT_Y, VR_IMAGE_FORMAT_U, VR_IMAGE_FORMAT_V };

	/* Make context current. */
	EGLBoolean bResult = eglMakeCurrent(pStatics->egl_info.sEGLDisplay, pStatics->default_target[VR_PROGRAM_YUVFILTER_Y]->target_pixmap_surface, 
									pStatics->default_target[VR_PROGRAM_YUVFILTER_Y]->target_pixmap_surface, pStatics->egl_info.sEGLContext[VR_PROGRAM_YUVFILTER_Y]);
	if(bResult == EGL_FALSE)
	{
		EGLint iError = eglGetError();
		NxErrMsg("eglGetError(): %i (0x%.4x)\n", (int)iError, (int)iError);
		NxErrMsg("Error: Failed to make context current at %s:%i\n", __FILE__, __LINE__);
		for(int i = 0 ; i < VR_YUV_CNT ; i++){ psource[i] = NULL; }
		return VR_FALSE;	
	}
	
	unsigned int pixmap_width, pixmap_height;
	for(int i = 0 ; i < VR_YUV_CNT ; i++)
	{	
		if(!pData[i])
		{
			VR_ASSERT("Error: Y must exist", i != 0);				
			continue;
		}

		HSURFSOURCE result = (HSURFSOURCE)NX_MALLOC(sizeof(struct vrSurfaceSource));
		if( !result )
		{
			NxErrMsg("Error: Out of memory at %s:%i\n", __FILE__, __LINE__);
			for(int i = 0 ; i < VR_YUV_CNT ; i++){ psource[i] = NULL; }
			return VR_FALSE;
		}
		NX_MEMSET( result, 0, sizeof(struct vrSurfaceSource) );

		if(0 == i)
		{
			//Y case
			pixmap_width = uiWidth;
			pixmap_height = uiHeight;
		}
		else if(1 == i)
		{
			//U case
			pixmap_width = uiWidth/2;
			pixmap_height = uiHeight/2;
		}
		else
		{
			//V case
			pixmap_width = uiWidth/2;
			pixmap_height = uiHeight/2;
		}

		EGLNativePixmapType pixmapInput = (EGLNativePixmapType)nxGSurfaceCreatePixmap(pixmap_width, pixmap_height, pData[i], VR_TRUE, 8);
		if(pixmapInput == NULL || ((VR_PLATFORM_PIXMAP_STRUCT*)pixmapInput)->data == NULL)
		{
			NxErrMsg("Error: Out of memory at %s:%i\n", __FILE__, __LINE__);\
			NX_FREE(result);
			for(int i = 0 ; i < VR_YUV_CNT ; i++){ psource[i] = NULL; }
			return VR_FALSE;
		}

		//RGB is not supported	
		EGLint imageAttributes[] = {
			EGL_IMAGE_PRESERVED_KHR, /*EGL_TRUE*/EGL_FALSE, 
			EGL_NONE
		};	
		EGLImageKHR eglImage = EGL_CHECK(_NX_eglCreateImageKHR( pStatics->egl_info.sEGLDisplay, EGL_NO_CONTEXT, 
								           EGL_NATIVE_PIXMAP_KHR, (EGLClientBuffer)pixmapInput, imageAttributes));	

		GLuint textureName;
		GL_CHECK(glGenTextures(1, &textureName));
		GL_CHECK(glActiveTexture(GL_TEXTURE0 + VR_INPUT_MODE_TEXTURE0));
		GL_CHECK(glBindTexture(GL_TEXTURE_2D, textureName));
		GL_CHECK(glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR));
		GL_CHECK(glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR));
		GL_CHECK(glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE));
		GL_CHECK(glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE));
		GL_CHECK(_NX_glEGLImageTargetTexture2DOES( GL_TEXTURE_2D, (GLeglImageOES)eglImage));		

		result->width        = pixmap_width ;
		result->height       = pixmap_height;
		result->texture_name[0] = textureName;
		result->src_native_pixmaps[0]= pixmapInput;
		result->src_pixmap_images[0] = eglImage   ;		

		psource[i] = result;
		VR_INFO("", VR_GRAPHIC_DBG_SOURCE, "nxGSurfaceCreateYuvFilterSource done(%d)\n", i);		
	}
	return VR_TRUE;
}

void nxGSurfaceDestroyYuvFilterSource ( HSURFSOURCE* psource )
{
	if( !psource ){ return; }
	Statics *pStatics = vrGetStatics();
	_AUTO_BACKUP_CURRENT_EGL_;
	
	for(int i = 0 ; i < VR_YUV_CNT ; i++)
	{
		if(!psource[i])
		{
			VR_ASSERT("Error: Y must exist", i != 0);				
			continue;
		}
		
		GL_CHECK(glDeleteTextures(1,&(psource[i]->texture_name[0])));
		EGL_CHECK(_NX_eglDestroyImageKHR(pStatics->egl_info.sEGLDisplay, psource[i]->src_pixmap_images[0]));
		nxGSurfaceDestroyPixmap(psource[i]->src_native_pixmaps[0]);
		NX_FREE(psource[i]);
		VR_INFO("", VR_GRAPHIC_DBG_SOURCE, "nxGSurfaceDestroyYuvFilterSource done(%d)\n", i);		
	}
}

#else

HSURFSOURCE nxGSurfaceCreateDeinterlaceSource  (unsigned int uiStride, unsigned int uiWidth, unsigned int uiHeight, NX_MEMORY_HANDLE hData)
{
	VR_ASSERT("width must be 8X and height must be 2X", uiWidth && uiHeight && ((uiWidth&0x1) == 0x0) && ((uiHeight&0x1) == 0x0) );
	VR_INFO("", VR_GRAPHIC_DBG_SOURCE, "%s start\n", __FUNCTION__);

	Statics *pStatics = vrGetStatics();
	VRImageFormatMode format[3] = { VR_IMAGE_FORMAT_V, VR_IMAGE_FORMAT_U, VR_IMAGE_FORMAT_Y };

	if( !pStatics || !hData ){ return (HSURFSOURCE)0; }
	_AUTO_BACKUP_CURRENT_EGL_;
	HSURFSOURCE result = (HSURFSOURCE)NX_MALLOC(sizeof(struct vrSurfaceSource));
	if( !result )
	{
		NxErrMsg("Error: Out of memory at %s:%i\n", __FILE__, __LINE__);
		return (HSURFSOURCE)0;
	}
	NX_MEMSET( result, 0, sizeof(struct vrSurfaceSource) );
	result->width		 = uiWidth ;
	result->height		 = uiHeight;
	#ifdef VR_FEATURE_STRIDE_USE
	result->stride		   = uiStride;
	#endif

	/* Make context current. */
	EGLBoolean bResult = eglMakeCurrent(pStatics->egl_info.sEGLDisplay, pStatics->default_target[VR_PROGRAM_DEINTERLACE]->target_pixmap_surfaces[0], 
									pStatics->default_target[VR_PROGRAM_DEINTERLACE]->target_pixmap_surfaces[0], pStatics->egl_info.sEGLContext[VR_PROGRAM_DEINTERLACE]);
	if(bResult == EGL_FALSE)
	{
		EGLint iError = eglGetError();
		NxErrMsg("eglGetError(): %i (0x%.4x)\n", (int)iError, (int)iError);
		NxErrMsg("Error: Failed to make context current at %s:%i\n", __FILE__, __LINE__);
		return 0;	
	}

	unsigned int pixmap_stride, pixmap_width, pixmap_height;
	if(!uiStride){ uiStride = uiWidth; }
	for(int i = 0 ; i < VR_YUV_CNT ; i++)
	{					
		if(2 == i)
		{
			//Y case
			pixmap_stride = uiStride;
			pixmap_width = uiWidth;
			pixmap_height = uiHeight;
			result->total_texture_src_height[i] = pixmap_height;
		}
		else if(1 == i)
		{
			//U case
			pixmap_stride = uiWidth/2 + (uiStride-uiWidth)/2;
			pixmap_width = uiWidth/2;
			pixmap_height = uiHeight*2 + uiHeight/2;
			result->total_texture_src_height[i] = pixmap_height;
			#if (VR_FEATURE_INPUT_HEIGHT_ALIGN_BYTE > 0)
			{
				//add Y align blank offset
				unsigned int y_height, y_align_blank_height = 0;
				y_height = uiHeight;
				if(y_height % VR_FEATURE_INPUT_HEIGHT_ALIGN_BYTE)
				{	
					y_align_blank_height = (VR_FEATURE_INPUT_HEIGHT_ALIGN_BYTE - (y_height % VR_FEATURE_INPUT_HEIGHT_ALIGN_BYTE)) * 2;
				}
				pixmap_height += y_align_blank_height;
				VR_INFO("", VR_GRAPHIC_DBG_HEIGHT_ALIGN, "[%d] y_align_blank_height(%d)\n", i, y_align_blank_height);		
			}
			#endif			
		}
		else
		{
			//V case
			pixmap_stride = uiWidth/2 + (uiStride-uiWidth)/2;
			pixmap_width = uiWidth/2;
			pixmap_height = uiHeight*2 + uiHeight;
			result->total_texture_src_height[i] = pixmap_height;
			#if (VR_FEATURE_INPUT_HEIGHT_ALIGN_BYTE > 0)
			{
				unsigned int y_height, y_align_blank_height = 0;
				unsigned int u_height, u_align_blank_height = 0;
				//add Y align blank offset
				y_height = uiHeight;
				if(y_height % VR_FEATURE_INPUT_HEIGHT_ALIGN_BYTE)
				{	
					y_align_blank_height = (VR_FEATURE_INPUT_HEIGHT_ALIGN_BYTE - (y_height % VR_FEATURE_INPUT_HEIGHT_ALIGN_BYTE)) * 2;
				}
				//add U align blank offset
				u_height = uiHeight/2;
				if(u_height % VR_FEATURE_INPUT_HEIGHT_ALIGN_BYTE)
				{	
					u_align_blank_height = (VR_FEATURE_INPUT_HEIGHT_ALIGN_BYTE - (u_height % VR_FEATURE_INPUT_HEIGHT_ALIGN_BYTE));
				}
				pixmap_height += (y_align_blank_height + u_align_blank_height);
				VR_INFO("", VR_GRAPHIC_DBG_HEIGHT_ALIGN, "[%d] y_align_blank_height(%d), u_align_blank_height(%d)\n", i, y_align_blank_height, u_align_blank_height);		
			}
			#endif						
		}
		#ifdef VR_FEATURE_STRIDE_USE
		pixmap_width = pixmap_stride;
		#endif

		EGLNativePixmapType pixmapInput = (EGLNativePixmapType)nxGSurfaceCreatePixmap(pixmap_width/4, pixmap_height, hData, VR_TRUE, 32);
		if(pixmapInput == NULL || ((VR_PLATFORM_PIXMAP_STRUCT*)pixmapInput)->data == NULL)
		{
			NxErrMsg("Error: Out of memory at %s:%i\n", __FILE__, __LINE__);\
			NX_FREE(result);
			return (HSURFSOURCE)0;
		}

		//RGB is not supported	
		EGLint imageAttributes[] = {
			EGL_IMAGE_PRESERVED_KHR, EGL_TRUE, 
			EGL_NONE
		};	
		EGLImageKHR eglImage = EGL_CHECK(_NX_eglCreateImageKHR( pStatics->egl_info.sEGLDisplay, EGL_NO_CONTEXT, 
								           EGL_NATIVE_PIXMAP_KHR, (EGLClientBuffer)pixmapInput, imageAttributes));	

		GLuint textureName;
		GL_CHECK(glGenTextures(1, &textureName));
		GL_CHECK(glActiveTexture(GL_TEXTURE0 + VR_INPUT_MODE_DEINTERLACE));
		GL_CHECK(glBindTexture(GL_TEXTURE_2D, textureName));
		GL_CHECK(glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST));
		GL_CHECK(glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST));
		GL_CHECK(glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE));
		GL_CHECK(glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE));
		GL_CHECK(_NX_glEGLImageTargetTexture2DOES( GL_TEXTURE_2D, (GLeglImageOES)eglImage));		

		result->texture_name[i] = textureName;
		result->src_native_pixmaps[i]= pixmapInput;
		result->src_pixmap_images[i] = eglImage   ;		
		
		VR_INFO("", VR_GRAPHIC_DBG_SOURCE, "size(%dx%d, %d)\n", pixmap_width, pixmap_height, pixmap_stride);		
	}
	VR_INFO("", VR_GRAPHIC_DBG_SOURCE, "nxGSurfaceCreateDeinterlaceSource done\n"); 		
	return result;
}

void nxGSurfaceDestroyDeinterlaceSource ( HSURFSOURCE hSource )
{
	if( !hSource ){ return; }
	Statics *pStatics = vrGetStatics();
	_AUTO_BACKUP_CURRENT_EGL_;

	for(int i = 0 ; i < VR_YUV_CNT ; i++)
	{	
		if(!hSource->src_pixmap_images[i])
		{
			VR_ASSERT("Error: Y must exist", i != 0);				
			continue;
		}
		GL_CHECK(glDeleteTextures(1,&hSource->texture_name[i]));
		EGL_CHECK(_NX_eglDestroyImageKHR(pStatics->egl_info.sEGLDisplay, hSource->src_pixmap_images[i]));
		nxGSurfaceDestroyPixmap(hSource->src_native_pixmaps[i]);
	}
	NX_FREE(hSource);
	VR_INFO("", VR_GRAPHIC_DBG_SOURCE, "nxGSurfaceDestroyDeinterlaceSource done\n"); 		
}

HSURFSOURCE nxGSurfaceCreateScaleSource  (unsigned int uiStride, unsigned int uiWidth, unsigned int uiHeight, NX_MEMORY_HANDLE hData)
{
	VR_ASSERT("width must be 8X and height must be 2X", uiWidth && uiHeight && ((uiWidth&0x1) == 0x0) && ((uiHeight&0x1) == 0x0) );
	VR_INFO("", VR_GRAPHIC_DBG_SOURCE, "%s start\n", __FUNCTION__);

	Statics *pStatics = vrGetStatics();
	VRImageFormatMode format[3] = { VR_IMAGE_FORMAT_V, VR_IMAGE_FORMAT_U, VR_IMAGE_FORMAT_Y };

	if( !pStatics || !hData ){ return (HSURFSOURCE)0; }
	_AUTO_BACKUP_CURRENT_EGL_;
	HSURFSOURCE result = (HSURFSOURCE)NX_MALLOC(sizeof(struct vrSurfaceSource));
	if( !result )
	{
		NxErrMsg("Error: Out of memory at %s:%i\n", __FILE__, __LINE__);
		return (HSURFSOURCE)0;
	}
	NX_MEMSET( result, 0, sizeof(struct vrSurfaceSource) );
	result->width		 = uiWidth ;
	result->height		 = uiHeight;
	#ifdef VR_FEATURE_STRIDE_USE
	result->stride		   = uiStride;
	#endif

	/* Make context current. */
	EGLBoolean bResult = eglMakeCurrent(pStatics->egl_info.sEGLDisplay, pStatics->default_target[VR_PROGRAM_SCALE]->target_pixmap_surfaces[0], 
									pStatics->default_target[VR_PROGRAM_SCALE]->target_pixmap_surfaces[0], pStatics->egl_info.sEGLContext[VR_PROGRAM_SCALE]);
	if(bResult == EGL_FALSE)
	{
		EGLint iError = eglGetError();
		NxErrMsg("eglGetError(): %i (0x%.4x)\n", (int)iError, (int)iError);
		NxErrMsg("Error: Failed to make context current at %s:%i\n", __FILE__, __LINE__);
		return 0;	
	}

	unsigned int pixmap_stride, pixmap_width, pixmap_height;
	if(!uiStride){ uiStride = uiWidth; }
	for(int i = 0 ; i < VR_YUV_CNT ; i++)
	{			
		if(2 == i)
		{
			//Y case
			pixmap_stride = uiStride;
			pixmap_width = uiWidth;
			pixmap_height = uiHeight;
			result->total_texture_src_height[i] = pixmap_height;
		}
		else if(1 == i)
		{
			//U case
			pixmap_stride = uiWidth/2 + (uiStride-uiWidth)/2;
			pixmap_width = uiWidth/2;
			pixmap_height = uiHeight*2 + uiHeight/2;			
			result->total_texture_src_height[i] = pixmap_height;
			#if (VR_FEATURE_INPUT_HEIGHT_ALIGN_BYTE > 0)
			{
				//add Y align blank offset
				unsigned int y_height, y_align_blank_height = 0;
				y_height = uiHeight;
				if(y_height % VR_FEATURE_INPUT_HEIGHT_ALIGN_BYTE)
				{	
					y_align_blank_height = (VR_FEATURE_INPUT_HEIGHT_ALIGN_BYTE - (y_height % VR_FEATURE_INPUT_HEIGHT_ALIGN_BYTE)) * 2;
				}
				pixmap_height += y_align_blank_height;
				VR_INFO("", VR_GRAPHIC_DBG_HEIGHT_ALIGN, "[%d] y_align_blank_height(%d)\n", i, y_align_blank_height);		
			}
			#endif
		}
		else
		{
			//V case
			pixmap_stride = uiWidth/2 + (uiStride-uiWidth)/2;
			pixmap_width = uiWidth/2;
			pixmap_height = uiHeight*2 + uiHeight;
			result->total_texture_src_height[i] = pixmap_height;
			#if (VR_FEATURE_INPUT_HEIGHT_ALIGN_BYTE > 0)
			{
				unsigned int y_height, y_align_blank_height = 0;
				unsigned int u_height, u_align_blank_height = 0;
				//add Y align blank offset
				y_height = uiHeight;
				if(y_height % VR_FEATURE_INPUT_HEIGHT_ALIGN_BYTE)
				{	
					y_align_blank_height = (VR_FEATURE_INPUT_HEIGHT_ALIGN_BYTE - (y_height % VR_FEATURE_INPUT_HEIGHT_ALIGN_BYTE)) * 2;
				}
				//add U align blank offset
				u_height = uiHeight/2;
				if(u_height % VR_FEATURE_INPUT_HEIGHT_ALIGN_BYTE)
				{	
					u_align_blank_height = (VR_FEATURE_INPUT_HEIGHT_ALIGN_BYTE - (u_height % VR_FEATURE_INPUT_HEIGHT_ALIGN_BYTE));
				}
				pixmap_height += (y_align_blank_height + u_align_blank_height);
				VR_INFO("", VR_GRAPHIC_DBG_HEIGHT_ALIGN, "[%d] y_align_blank_height(%d), u_align_blank_height(%d)\n", i, y_align_blank_height, u_align_blank_height);		
			}
			#endif			
		}
		#ifdef VR_FEATURE_STRIDE_USE
		pixmap_width = pixmap_stride;
		#endif
		EGLNativePixmapType pixmapInput = (EGLNativePixmapType)nxGSurfaceCreatePixmap(pixmap_width, pixmap_height, hData, VR_TRUE, 8);
		if(pixmapInput == NULL || ((VR_PLATFORM_PIXMAP_STRUCT*)pixmapInput)->data == NULL)
		{
			NxErrMsg("Error: Out of memory at %s:%i\n", __FILE__, __LINE__);\
			NX_FREE(result);
			return (HSURFSOURCE)0;
		}

		//RGB is not supported	
		EGLint imageAttributes[] = {
			EGL_IMAGE_PRESERVED_KHR, /*EGL_TRUE*/EGL_FALSE, 
			EGL_NONE
		};	
		EGLImageKHR eglImage = EGL_CHECK(_NX_eglCreateImageKHR( pStatics->egl_info.sEGLDisplay, EGL_NO_CONTEXT, 
								           EGL_NATIVE_PIXMAP_KHR, (EGLClientBuffer)pixmapInput, imageAttributes));	

		GLuint textureName;
		GL_CHECK(glGenTextures(1, &textureName));
		GL_CHECK(glActiveTexture(GL_TEXTURE0 + VR_INPUT_MODE_TEXTURE0));
		GL_CHECK(glBindTexture(GL_TEXTURE_2D, textureName));
		GL_CHECK(glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR));
		GL_CHECK(glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR));
		GL_CHECK(glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE));
		GL_CHECK(glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE));
		GL_CHECK(_NX_glEGLImageTargetTexture2DOES( GL_TEXTURE_2D, (GLeglImageOES)eglImage));		

		result->texture_name[i] = textureName;
		result->src_native_pixmaps[i]= pixmapInput;
		result->src_pixmap_images[i] = eglImage   ;		
		
		VR_INFO("", VR_GRAPHIC_DBG_SOURCE, "size(%dx%d, %d)\n", pixmap_width, pixmap_height, pixmap_stride);		
	}
	VR_INFO("", VR_GRAPHIC_DBG_SOURCE, "nxGSurfaceCreateScaleSource done\n"); 		
	return result;
}

void nxGSurfaceDestroyScaleSource ( HSURFSOURCE hSource )
{
	if( !hSource ){ return; }
	Statics *pStatics = vrGetStatics();
	_AUTO_BACKUP_CURRENT_EGL_;

	for(int i = 0 ; i < VR_YUV_CNT ; i++)
	{	
		if(!hSource->src_pixmap_images[i])
		{
			VR_ASSERT("Error: Y must exist", i != 0);				
			continue;
		}
		GL_CHECK(glDeleteTextures(1,&hSource->texture_name[i]));
		EGL_CHECK(_NX_eglDestroyImageKHR(pStatics->egl_info.sEGLDisplay, hSource->src_pixmap_images[i]));
		nxGSurfaceDestroyPixmap(hSource->src_native_pixmaps[i]);
	}
	NX_FREE(hSource);
	VR_INFO("", VR_GRAPHIC_DBG_SOURCE, "nxGSurfaceDestroyScaleSource done\n"); 		
}

HSURFSOURCE nxGSurfaceCreateCvt2YuvSource  (unsigned int uiStride, unsigned int uiWidth, unsigned int uiHeight, NX_MEMORY_HANDLE hData)
{
	VR_ASSERT("width must be 8X and height must be 2X", uiWidth && uiHeight && ((uiWidth&0x1) == 0x0) && ((uiHeight&0x1) == 0x0) );
	VR_INFO("", VR_GRAPHIC_DBG_SOURCE, "%s start\n", __FUNCTION__);

	Statics *pStatics = vrGetStatics();

	if( !pStatics || !hData ){ return (HSURFSOURCE)0; }
	_AUTO_BACKUP_CURRENT_EGL_;
	HSURFSOURCE result = (HSURFSOURCE)NX_MALLOC(sizeof(struct vrSurfaceSource));
	if( !result )
	{
		NxErrMsg("Error: Out of memory at %s:%i\n", __FILE__, __LINE__);
		return (HSURFSOURCE)0;
	}
	NX_MEMSET( result, 0, sizeof(struct vrSurfaceSource) );
	result->width		 = uiWidth ;
	result->height		 = uiHeight;
	#ifdef VR_FEATURE_STRIDE_USE
	result->stride		   = uiStride;
	#endif

	unsigned int pixmap_stride, pixmap_width, pixmap_height;
	if(!uiStride){ uiStride = uiWidth; }
	pixmap_stride = uiStride, pixmap_width = uiWidth, pixmap_height = uiHeight;
	for(int i = 0 ; i < 2/*Y and UV*/ ; i++)
	{			
		/* Make context current. */
		EGLBoolean bResult = eglMakeCurrent(pStatics->egl_info.sEGLDisplay, pStatics->default_target[VR_PROGRAM_CVT2UV+i]->target_pixmap_surfaces[0], 
										pStatics->default_target[VR_PROGRAM_CVT2UV+i]->target_pixmap_surfaces[0], pStatics->egl_info.sEGLContext[VR_PROGRAM_CVT2UV+i]);
		if(bResult == EGL_FALSE)
		{
			EGLint iError = eglGetError();
			NxErrMsg("eglGetError(): %i (0x%.4x)\n", (int)iError, (int)iError);
			NxErrMsg("Error: Failed to make context current at %s:%i\n", __FILE__, __LINE__);
			return 0;	
		}
		
		#ifdef VR_FEATURE_STRIDE_USE
		pixmap_width = pixmap_stride;
		#endif
		EGLNativePixmapType pixmapInput = (EGLNativePixmapType)nxGSurfaceCreatePixmap(pixmap_width, pixmap_height, hData, VR_TRUE, 32);
		if(pixmapInput == NULL || ((VR_PLATFORM_PIXMAP_STRUCT*)pixmapInput)->data == NULL)
		{
			NxErrMsg("Error: Out of memory at %s:%i\n", __FILE__, __LINE__);\
			NX_FREE(result);
			return (HSURFSOURCE)0;
		}

		//RGB is not supported	
		EGLint imageAttributes[] = {
			EGL_IMAGE_PRESERVED_KHR, /*EGL_TRUE*/EGL_FALSE, 
			EGL_NONE
		};	
		EGLImageKHR eglImage = EGL_CHECK(_NX_eglCreateImageKHR( pStatics->egl_info.sEGLDisplay, EGL_NO_CONTEXT, 
								           EGL_NATIVE_PIXMAP_KHR, (EGLClientBuffer)pixmapInput, imageAttributes));	

		GLuint textureName;
		GL_CHECK(glGenTextures(1, &textureName));
		GL_CHECK(glActiveTexture(GL_TEXTURE0 + VR_INPUT_MODE_TEXTURE0+i));
		GL_CHECK(glBindTexture(GL_TEXTURE_2D, textureName));
		GL_CHECK(glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR));
		GL_CHECK(glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR));
		GL_CHECK(glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE));
		GL_CHECK(glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE));
		GL_CHECK(_NX_glEGLImageTargetTexture2DOES( GL_TEXTURE_2D, (GLeglImageOES)eglImage));		

		result->texture_name[i] = textureName;
		result->src_native_pixmaps[i]= pixmapInput;
		result->src_pixmap_images[i] = eglImage   ;		
		result->total_texture_src_height[i] = pixmap_height;
		
		VR_INFO("", VR_GRAPHIC_DBG_SOURCE, "size(%dx%d, %d)\n", pixmap_width, pixmap_height, pixmap_stride);		
	}
	VR_INFO("", VR_GRAPHIC_DBG_SOURCE, "nxGSurfaceCreateCvt2YuvSource done\n"); 		
	return result;
}

void nxGSurfaceDestroyCvt2YuvSource ( HSURFSOURCE hSource )
{
	if( !hSource ){ return; }
	Statics *pStatics = vrGetStatics();
	_AUTO_BACKUP_CURRENT_EGL_;

	for(int i = 0 ; i < 2/*Y and UV*/ ; i++)
	{	
		if(!hSource->src_pixmap_images[i])
		{
			VR_ASSERT("Error: Y must exist", i != 0);				
			continue;
		}
		GL_CHECK(glDeleteTextures(1,&hSource->texture_name[i]));
		EGL_CHECK(_NX_eglDestroyImageKHR(pStatics->egl_info.sEGLDisplay, hSource->src_pixmap_images[i]));
		nxGSurfaceDestroyPixmap(hSource->src_native_pixmaps[i]);
	}
	NX_FREE(hSource);
	VR_INFO("", VR_GRAPHIC_DBG_SOURCE, "nxGSurfaceDestroyCvt2YuvSource done\n"); 		
}

HSURFSOURCE nxGSurfaceCreateCvt2RgbaSource  (unsigned int uiStride, unsigned int uiWidth, unsigned int uiHeight, NX_MEMORY_HANDLE hData)
{
	VR_ASSERT("width must be 8X and height must be 2X", uiWidth && uiHeight && ((uiWidth&0x1) == 0x0) && ((uiHeight&0x1) == 0x0) );
	VR_INFO("", VR_GRAPHIC_DBG_SOURCE, "%s start\n", __FUNCTION__);

	Statics *pStatics = vrGetStatics();
	VRImageFormatMode format[3] = { VR_IMAGE_FORMAT_Y, VR_IMAGE_FORMAT_U, VR_IMAGE_FORMAT_V };

	if( !pStatics || !hData ){ return (HSURFSOURCE)0; }
	_AUTO_BACKUP_CURRENT_EGL_;
	HSURFSOURCE result = (HSURFSOURCE)NX_MALLOC(sizeof(struct vrSurfaceSource));
	if( !result )
	{
		NxErrMsg("Error: Out of memory at %s:%i\n", __FILE__, __LINE__);
		return (HSURFSOURCE)0;
	}
	NX_MEMSET( result, 0, sizeof(struct vrSurfaceSource) );
	result->width		 = uiWidth ;
	result->height		 = uiHeight;
	#ifdef VR_FEATURE_STRIDE_USE
	result->stride		   = uiStride;
	#endif

	/* Make context current. */
	EGLBoolean bResult = eglMakeCurrent(pStatics->egl_info.sEGLDisplay, pStatics->default_target[VR_PROGRAM_CVT2RGBA]->target_pixmap_surfaces[0], 
									pStatics->default_target[VR_PROGRAM_CVT2RGBA]->target_pixmap_surfaces[0], pStatics->egl_info.sEGLContext[VR_PROGRAM_CVT2RGBA]);

	unsigned int pixmap_stride, pixmap_width, pixmap_height;
	if(!uiStride){ uiStride = uiWidth; }
	for(int i = 0 ; i < VR_YUV_CNT ; i++)
	{			
		if(bResult == EGL_FALSE)
		{
			EGLint iError = eglGetError();
			NxErrMsg("eglGetError(): %i (0x%.4x)\n", (int)iError, (int)iError);
			NxErrMsg("Error: Failed to make context current at %s:%i\n", __FILE__, __LINE__);
			return 0;	
		}

		if(0 == i)
		{
			//Y case
			pixmap_stride = uiStride;
			pixmap_width = uiWidth;
			pixmap_height = uiHeight;
			result->total_texture_src_height[i] = pixmap_height;
		}
		else if(1 == i)
		{
			//U case
			pixmap_stride = uiWidth/2 + (uiStride-uiWidth)/2;
			pixmap_width = uiWidth/2;
			pixmap_height = uiHeight*2 + uiHeight/2;
			result->total_texture_src_height[i] = pixmap_height;
			#if (VR_FEATURE_INPUT_HEIGHT_ALIGN_BYTE > 0)
			{
				//add Y align blank offset
				unsigned int y_height, y_align_blank_height = 0;
				y_height = uiHeight;
				if(y_height % VR_FEATURE_INPUT_HEIGHT_ALIGN_BYTE)
				{	
					y_align_blank_height = (VR_FEATURE_INPUT_HEIGHT_ALIGN_BYTE - (y_height % VR_FEATURE_INPUT_HEIGHT_ALIGN_BYTE)) * 2;
				}
				pixmap_height += y_align_blank_height;
				VR_INFO("", VR_GRAPHIC_DBG_HEIGHT_ALIGN, "[%d] y_align_blank_height(%d)\n", i, y_align_blank_height);		
			}
			#endif			
		}
		else
		{
			//V case
			pixmap_stride = uiWidth/2 + (uiStride-uiWidth)/2;
			pixmap_width = uiWidth/2;
			pixmap_height = uiHeight*2 + uiHeight;
			result->total_texture_src_height[i] = pixmap_height;
			#if (VR_FEATURE_INPUT_HEIGHT_ALIGN_BYTE > 0)
			{
				unsigned int y_height, y_align_blank_height = 0;
				unsigned int u_height, u_align_blank_height = 0;
				//add Y align blank offset
				y_height = uiHeight;
				if(y_height % VR_FEATURE_INPUT_HEIGHT_ALIGN_BYTE)
				{	
					y_align_blank_height = (VR_FEATURE_INPUT_HEIGHT_ALIGN_BYTE - (y_height % VR_FEATURE_INPUT_HEIGHT_ALIGN_BYTE)) * 2;
				}
				//add U align blank offset
				u_height = uiHeight/2;
				if(u_height % VR_FEATURE_INPUT_HEIGHT_ALIGN_BYTE)
				{	
					u_align_blank_height = (VR_FEATURE_INPUT_HEIGHT_ALIGN_BYTE - (u_height % VR_FEATURE_INPUT_HEIGHT_ALIGN_BYTE));
				}
				pixmap_height += (y_align_blank_height + u_align_blank_height);
				VR_INFO("", VR_GRAPHIC_DBG_HEIGHT_ALIGN, "[%d] y_align_blank_height(%d), u_align_blank_height(%d)\n", i, y_align_blank_height, u_align_blank_height);		
			}
			#endif						
		}
		#ifdef VR_FEATURE_STRIDE_USE
		pixmap_width = pixmap_stride;
		#endif

		EGLNativePixmapType pixmapInput = (EGLNativePixmapType)nxGSurfaceCreatePixmap(pixmap_width, pixmap_height, hData, VR_TRUE, 8);
		if(pixmapInput == NULL || ((VR_PLATFORM_PIXMAP_STRUCT*)pixmapInput)->data == NULL)
		{
			NxErrMsg("Error: Out of memory at %s:%i\n", __FILE__, __LINE__);\
			NX_FREE(result);
			return (HSURFSOURCE)0;
		}

		//RGB is not supported	
		EGLint imageAttributes[] = {
			EGL_IMAGE_PRESERVED_KHR, EGL_TRUE, 
			EGL_NONE
		};	
		EGLImageKHR eglImage = EGL_CHECK(_NX_eglCreateImageKHR( pStatics->egl_info.sEGLDisplay, EGL_NO_CONTEXT, 
										   EGL_NATIVE_PIXMAP_KHR, (EGLClientBuffer)pixmapInput, imageAttributes));	

		GLuint textureName;
		GL_CHECK(glGenTextures(1, &textureName));
		GL_CHECK(glActiveTexture(GL_TEXTURE0 + i));
		GL_CHECK(glBindTexture(GL_TEXTURE_2D, textureName));
		GL_CHECK(glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR));
		GL_CHECK(glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR));
		GL_CHECK(glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE));
		GL_CHECK(glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE));
		GL_CHECK(_NX_glEGLImageTargetTexture2DOES( GL_TEXTURE_2D, (GLeglImageOES)eglImage));		

		result->texture_name[i] = textureName;
		result->src_native_pixmaps[i]= pixmapInput;
		result->src_pixmap_images[i] = eglImage   ; 	
		
		VR_INFO("", VR_GRAPHIC_DBG_SOURCE, "size(%dx%d, %d)\n", pixmap_width, pixmap_height, pixmap_stride);		
	}
	VR_INFO("", VR_GRAPHIC_DBG_SOURCE, "nxGSurfaceCreateCvt2RgbaSource done\n");		
	return result;
}

void nxGSurfaceDestroyCvt2RgbaSource ( HSURFSOURCE hSource )
{
	if( !hSource ){ return; }
	Statics *pStatics = vrGetStatics();
	_AUTO_BACKUP_CURRENT_EGL_;
	VR_ASSERT("Error: YUV must exist", hSource->src_pixmap_images[VR_INPUT_MODE_Y] && hSource->src_pixmap_images[VR_INPUT_MODE_U] && hSource->src_pixmap_images[VR_INPUT_MODE_V]);

	GL_CHECK(glDeleteTextures(VR_INPUT_MODE_YUV_MAX, hSource->texture_name));
	for(int i = 0 ; i < VR_YUV_CNT ; i++)
	{
		EGL_CHECK(_NX_eglDestroyImageKHR(pStatics->egl_info.sEGLDisplay, hSource->src_pixmap_images[i]));
		nxGSurfaceDestroyPixmap(hSource->src_native_pixmaps[i]);
		
	}
	NX_FREE(hSource);
	VR_INFO("", VR_GRAPHIC_DBG_SOURCE, "nxGSurfaceDestroyCvt2RgbaSource done\n"); 		
}

HSURFSOURCE nxGSurfaceCreateUserFilterSource  (unsigned int uiStride, unsigned int uiWidth, unsigned int uiHeight, NX_MEMORY_HANDLE hData)
{
	VR_ASSERT("width must be 8X and height must be 2X", uiWidth && uiHeight && ((uiWidth&0x1) == 0x0) && ((uiHeight&0x1) == 0x0) );
	VR_INFO("", VR_GRAPHIC_DBG_SOURCE, "%s start\n", __FUNCTION__);

	Statics *pStatics = vrGetStatics();

	if( !pStatics || !hData ){ return (HSURFSOURCE)0; }
	_AUTO_BACKUP_CURRENT_EGL_;
	HSURFSOURCE result = (HSURFSOURCE)NX_MALLOC(sizeof(struct vrSurfaceSource));
	if( !result )
	{
		NxErrMsg("Error: Out of memory at %s:%i\n", __FILE__, __LINE__);
		return (HSURFSOURCE)0;
	}
	NX_MEMSET( result, 0, sizeof(struct vrSurfaceSource) );
	result->width		 = uiWidth ;
	result->height		 = uiHeight;
	#ifdef VR_FEATURE_STRIDE_USE
	result->stride		   = uiStride;
	#endif

	unsigned int pixmap_stride, pixmap_width, pixmap_height;
	if(!uiStride){ uiStride = uiWidth; }
	pixmap_stride = uiStride, pixmap_width = uiWidth, pixmap_height = uiHeight;

	/* Make context current. */
	EGLBoolean bResult = eglMakeCurrent(pStatics->egl_info.sEGLDisplay, pStatics->default_target[VR_PROGRAM_USERFILTER]->target_pixmap_surfaces[0], 
									pStatics->default_target[VR_PROGRAM_USERFILTER]->target_pixmap_surfaces[0], pStatics->egl_info.sEGLContext[VR_PROGRAM_USERFILTER]);
	if(bResult == EGL_FALSE)
	{
		EGLint iError = eglGetError();
		NxErrMsg("eglGetError(): %i (0x%.4x)\n", (int)iError, (int)iError);
		NxErrMsg("Error: Failed to make context current at %s:%i\n", __FILE__, __LINE__);
		return 0;	
	}
	
	#ifdef VR_FEATURE_STRIDE_USE
	pixmap_width = pixmap_stride;
	#endif
	EGLNativePixmapType pixmapInput = (EGLNativePixmapType)nxGSurfaceCreatePixmap(pixmap_width, pixmap_height, hData, VR_TRUE, 32);
	if(pixmapInput == NULL || ((VR_PLATFORM_PIXMAP_STRUCT*)pixmapInput)->data == NULL)
	{
		NxErrMsg("Error: Out of memory at %s:%i\n", __FILE__, __LINE__);\
		NX_FREE(result);
		return (HSURFSOURCE)0;
	}

	//RGB is not supported	
	EGLint imageAttributes[] = {
		EGL_IMAGE_PRESERVED_KHR, /*EGL_TRUE*/EGL_FALSE, 
		EGL_NONE
	};	
	EGLImageKHR eglImage = EGL_CHECK(_NX_eglCreateImageKHR( pStatics->egl_info.sEGLDisplay, EGL_NO_CONTEXT, 
							           EGL_NATIVE_PIXMAP_KHR, (EGLClientBuffer)pixmapInput, imageAttributes));	

	GLuint textureName;
	GL_CHECK(glGenTextures(1, &textureName));
	GL_CHECK(glActiveTexture(GL_TEXTURE0 + VR_INPUT_MODE_USERFILTER));
	GL_CHECK(glBindTexture(GL_TEXTURE_2D, textureName));
	GL_CHECK(glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST));
	GL_CHECK(glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST));
	GL_CHECK(glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE));
	GL_CHECK(glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE));
	GL_CHECK(_NX_glEGLImageTargetTexture2DOES( GL_TEXTURE_2D, (GLeglImageOES)eglImage));		

	result->texture_name[0] = textureName;
	result->src_native_pixmaps[0]= pixmapInput;
	result->src_pixmap_images[0] = eglImage   ;		
	result->total_texture_src_height[0] = pixmap_height;
	
	VR_INFO("", VR_GRAPHIC_DBG_SOURCE, "size(%dx%d, %d)\n", pixmap_width, pixmap_height, pixmap_stride);		

	VR_INFO("", VR_GRAPHIC_DBG_SOURCE, "nxGSurfaceCreateUserFilterSource done\n"); 		
	return result;
}

void nxGSurfaceDestroyUserFilterSource ( HSURFSOURCE hSource )
{
	if( !hSource ){ return; }
	Statics *pStatics = vrGetStatics();
	_AUTO_BACKUP_CURRENT_EGL_;
	VR_ASSERT("Error: Y must exist", hSource->src_pixmap_images[0]);
	
	GL_CHECK(glDeleteTextures(1,&hSource->texture_name[0]));
	EGL_CHECK(_NX_eglDestroyImageKHR(pStatics->egl_info.sEGLDisplay, hSource->src_pixmap_images[0]));
	nxGSurfaceDestroyPixmap(hSource->src_native_pixmaps[0]);

	NX_FREE(hSource);
	VR_INFO("", VR_GRAPHIC_DBG_SOURCE, "nxGSurfaceDestroyUserFilterSource done\n"); 		
}

HSURFSOURCE nxGSurfaceCreateYuvFilterSource  (unsigned int uiStride, unsigned int uiWidth, unsigned int uiHeight, NX_MEMORY_HANDLE hData)
{
	VR_ASSERT("width must be 8X and height must be 2X", uiWidth && uiHeight && ((uiWidth&0x1) == 0x0) && ((uiHeight&0x1) == 0x0) );
	VR_INFO("", VR_GRAPHIC_DBG_SOURCE, "%s start\n", __FUNCTION__);

	Statics *pStatics = vrGetStatics();
	VRImageFormatMode format[3] = { VR_IMAGE_FORMAT_V, VR_IMAGE_FORMAT_U, VR_IMAGE_FORMAT_Y };

	if( !pStatics || !hData ){ return (HSURFSOURCE)0; }
	_AUTO_BACKUP_CURRENT_EGL_;
	HSURFSOURCE result = (HSURFSOURCE)NX_MALLOC(sizeof(struct vrSurfaceSource));
	if( !result )
	{
		NxErrMsg("Error: Out of memory at %s:%i\n", __FILE__, __LINE__);
		return (HSURFSOURCE)0;
	}
	NX_MEMSET( result, 0, sizeof(struct vrSurfaceSource) );
	result->width		 = uiWidth ;
	result->height		 = uiHeight;
	#ifdef VR_FEATURE_STRIDE_USE
	result->stride		   = uiStride;
	#endif

	/* Make context current. */
	EGLBoolean bResult = eglMakeCurrent(pStatics->egl_info.sEGLDisplay, pStatics->default_target[VR_PROGRAM_YUVFILTER_Y]->target_pixmap_surfaces[0], 
									pStatics->default_target[VR_PROGRAM_YUVFILTER_Y]->target_pixmap_surfaces[0], pStatics->egl_info.sEGLContext[VR_PROGRAM_YUVFILTER_Y]);
	if(bResult == EGL_FALSE)
	{
		EGLint iError = eglGetError();
		NxErrMsg("eglGetError(): %i (0x%.4x)\n", (int)iError, (int)iError);
		NxErrMsg("Error: Failed to make context current at %s:%i\n", __FILE__, __LINE__);
		return 0;	
	}

	unsigned int pixmap_stride, pixmap_width, pixmap_height;
	if(!uiStride){ uiStride = uiWidth; }
	for(int i = 0 ; i < VR_YUV_CNT ; i++)
	{			
		if(2 == i)
		{
			//Y case
			pixmap_stride = uiStride;
			pixmap_width = uiWidth;
			pixmap_height = uiHeight;
			result->total_texture_src_height[i] = pixmap_height;
		}
		else if(1 == i)
		{
			//U case
			pixmap_stride = uiWidth/2 + (uiStride-uiWidth)/2;
			pixmap_width = uiWidth/2;
			pixmap_height = uiHeight*2 + uiHeight/2;			
			result->total_texture_src_height[i] = pixmap_height;
			#if (VR_FEATURE_INPUT_HEIGHT_ALIGN_BYTE > 0)
			{
				//add Y align blank offset
				unsigned int y_height, y_align_blank_height = 0;
				y_height = uiHeight;
				if(y_height % VR_FEATURE_INPUT_HEIGHT_ALIGN_BYTE)
				{	
					y_align_blank_height = (VR_FEATURE_INPUT_HEIGHT_ALIGN_BYTE - (y_height % VR_FEATURE_INPUT_HEIGHT_ALIGN_BYTE)) * 2;
				}
				pixmap_height += y_align_blank_height;
				VR_INFO("", VR_GRAPHIC_DBG_HEIGHT_ALIGN, "[%d] y_align_blank_height(%d)\n", i, y_align_blank_height);		
			}
			#endif
		}
		else
		{
			//V case
			pixmap_stride = uiWidth/2 + (uiStride-uiWidth)/2;
			pixmap_width = uiWidth/2;
			pixmap_height = uiHeight*2 + uiHeight;
			result->total_texture_src_height[i] = pixmap_height;
			#if (VR_FEATURE_INPUT_HEIGHT_ALIGN_BYTE > 0)
			{
				unsigned int y_height, y_align_blank_height = 0;
				unsigned int u_height, u_align_blank_height = 0;
				//add Y align blank offset
				y_height = uiHeight;
				if(y_height % VR_FEATURE_INPUT_HEIGHT_ALIGN_BYTE)
				{	
					y_align_blank_height = (VR_FEATURE_INPUT_HEIGHT_ALIGN_BYTE - (y_height % VR_FEATURE_INPUT_HEIGHT_ALIGN_BYTE)) * 2;
				}
				//add U align blank offset
				u_height = uiHeight/2;
				if(u_height % VR_FEATURE_INPUT_HEIGHT_ALIGN_BYTE)
				{	
					u_align_blank_height = (VR_FEATURE_INPUT_HEIGHT_ALIGN_BYTE - (u_height % VR_FEATURE_INPUT_HEIGHT_ALIGN_BYTE));
				}
				pixmap_height += (y_align_blank_height + u_align_blank_height);
				VR_INFO("", VR_GRAPHIC_DBG_HEIGHT_ALIGN, "[%d] y_align_blank_height(%d), u_align_blank_height(%d)\n", i, y_align_blank_height, u_align_blank_height);		
			}
			#endif			
		}
		#ifdef VR_FEATURE_STRIDE_USE
		pixmap_width = pixmap_stride;
		#endif
		EGLNativePixmapType pixmapInput = (EGLNativePixmapType)nxGSurfaceCreatePixmap(pixmap_width, pixmap_height, hData, VR_TRUE, 8);
		if(pixmapInput == NULL || ((VR_PLATFORM_PIXMAP_STRUCT*)pixmapInput)->data == NULL)
		{
			NxErrMsg("Error: Out of memory at %s:%i\n", __FILE__, __LINE__);\
			NX_FREE(result);
			return (HSURFSOURCE)0;
		}

		//RGB is not supported	
		EGLint imageAttributes[] = {
			EGL_IMAGE_PRESERVED_KHR, /*EGL_TRUE*/EGL_FALSE, 
			EGL_NONE
		};	
		EGLImageKHR eglImage = EGL_CHECK(_NX_eglCreateImageKHR( pStatics->egl_info.sEGLDisplay, EGL_NO_CONTEXT, 
								           EGL_NATIVE_PIXMAP_KHR, (EGLClientBuffer)pixmapInput, imageAttributes));	

		GLuint textureName;
		GL_CHECK(glGenTextures(1, &textureName));
		GL_CHECK(glActiveTexture(GL_TEXTURE0 + VR_INPUT_MODE_TEXTURE0));
		GL_CHECK(glBindTexture(GL_TEXTURE_2D, textureName));
		GL_CHECK(glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR));
		GL_CHECK(glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR));
		GL_CHECK(glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE));
		GL_CHECK(glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE));
		GL_CHECK(_NX_glEGLImageTargetTexture2DOES( GL_TEXTURE_2D, (GLeglImageOES)eglImage));		

		result->texture_name[i] = textureName;
		result->src_native_pixmaps[i]= pixmapInput;
		result->src_pixmap_images[i] = eglImage   ;		
		
		VR_INFO("", VR_GRAPHIC_DBG_SOURCE, "size(%dx%d, %d)\n", pixmap_width, pixmap_height, pixmap_stride);		
	}
	VR_INFO("", VR_GRAPHIC_DBG_SOURCE, "nxGSurfaceCreateYuvFilterSource done\n"); 		
	return result;
}

void nxGSurfaceDestroyYuvFilterSource ( HSURFSOURCE hSource )
{
	if( !hSource ){ return; }
	Statics *pStatics = vrGetStatics();
	_AUTO_BACKUP_CURRENT_EGL_;

	for(int i = 0 ; i < VR_YUV_CNT ; i++)
	{	
		if(!hSource->src_pixmap_images[i])
		{
			VR_ASSERT("Error: Y must exist", i != 0);				
			continue;
		}
		GL_CHECK(glDeleteTextures(1,&hSource->texture_name[i]));
		EGL_CHECK(_NX_eglDestroyImageKHR(pStatics->egl_info.sEGLDisplay, hSource->src_pixmap_images[i]));
		nxGSurfaceDestroyPixmap(hSource->src_native_pixmaps[i]);
	}
	NX_FREE(hSource);
	VR_INFO("", VR_GRAPHIC_DBG_SOURCE, "nxGSurfaceDestroyYuvFilterSource done\n"); 		
}

#endif

HSURFSOURCE nxGSurfaceCreateScaleRGBASource  (unsigned int uiStride, unsigned int uiWidth, unsigned int uiHeight, NX_MEMORY_HANDLE hData)
{
	//VR_ASSERT("width must be 8X and height must be 2X", uiWidth && uiHeight && ((uiWidth&0x1) == 0x0) && ((uiHeight&0x1) == 0x0) );
	VR_INFO("", VR_GRAPHIC_DBG_SOURCE, "%s start\n", __FUNCTION__);

	Statics *pStatics = vrGetStatics();

	if( !pStatics || !hData ){ return (HSURFSOURCE)0; }
	_AUTO_BACKUP_CURRENT_EGL_;
	HSURFSOURCE result = (HSURFSOURCE)NX_MALLOC(sizeof(struct vrSurfaceSource));
	if( !result )
	{
		NxErrMsg("Error: Out of memory at %s:%i\n", __FILE__, __LINE__);
		return (HSURFSOURCE)0;
	}
	NX_MEMSET( result, 0, sizeof(struct vrSurfaceSource) );
	result->width		 = uiWidth ;
	result->height		 = uiHeight;
#ifdef VR_FEATURE_STRIDE_USE
	result->stride		   = uiStride;
#endif

	unsigned int pixmap_stride, pixmap_width, pixmap_height;
	if(!uiStride){ uiStride = uiWidth; }
	pixmap_stride = uiStride, pixmap_width = uiWidth, pixmap_height = uiHeight;

	/* Make context current. */
	#ifdef VR_FEATURE_SEPERATE_FD_USE
	EGLBoolean bResult = eglMakeCurrent(pStatics->egl_info.sEGLDisplay, pStatics->default_target[VR_PROGRAM_SCALE_RGBA]->target_pixmap_surface, 
									pStatics->default_target[VR_PROGRAM_SCALE_RGBA]->target_pixmap_surface, pStatics->egl_info.sEGLContext[VR_PROGRAM_SCALE_RGBA]);
	#else
	EGLBoolean bResult = eglMakeCurrent(pStatics->egl_info.sEGLDisplay, pStatics->default_target[VR_PROGRAM_SCALE_RGBA]->target_pixmap_surfaces[0], 
									pStatics->default_target[VR_PROGRAM_SCALE_RGBA]->target_pixmap_surfaces[0], pStatics->egl_info.sEGLContext[VR_PROGRAM_SCALE_RGBA]);
	#endif								
	if(bResult == EGL_FALSE)
	{
		EGLint iError = eglGetError();
		NxErrMsg("eglGetError(): %i (0x%.4x)\n", (int)iError, (int)iError);
		NxErrMsg("Error: Failed to make context current at %s:%i\n", __FILE__, __LINE__);
		return 0;	
	}
	
#ifdef VR_FEATURE_STRIDE_USE
	pixmap_width = pixmap_stride;
#endif
	EGLNativePixmapType pixmapInput = (EGLNativePixmapType)nxGSurfaceCreatePixmap(pixmap_width, pixmap_height, hData, VR_TRUE, 32);
	if(pixmapInput == NULL || ((VR_PLATFORM_PIXMAP_STRUCT*)pixmapInput)->data == NULL)
	{
		NxErrMsg("Error: Out of memory at %s:%i\n", __FILE__, __LINE__);\
		NX_FREE(result);
		return (HSURFSOURCE)0;
	}

	//RGB is not supported	
	EGLint imageAttributes[] = {
		EGL_IMAGE_PRESERVED_KHR, /*EGL_TRUE*/EGL_FALSE, 
		EGL_NONE
	};	
	EGLImageKHR eglImage = EGL_CHECK(_NX_eglCreateImageKHR( pStatics->egl_info.sEGLDisplay, EGL_NO_CONTEXT, 
									   EGL_NATIVE_PIXMAP_KHR, (EGLClientBuffer)pixmapInput, imageAttributes));	

	GLuint textureName;
	GL_CHECK(glGenTextures(1, &textureName));
	GL_CHECK(glActiveTexture(GL_TEXTURE0 + VR_INPUT_MODE_SCALE_RGBA));
	GL_CHECK(glBindTexture(GL_TEXTURE_2D, textureName));
	GL_CHECK(glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR));
	GL_CHECK(glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR));
	GL_CHECK(glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE));
	GL_CHECK(glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE));
	GL_CHECK(_NX_glEGLImageTargetTexture2DOES( GL_TEXTURE_2D, (GLeglImageOES)eglImage));		

	result->texture_name[0] = textureName;
	result->src_native_pixmaps[0]= pixmapInput;
	result->src_pixmap_images[0] = eglImage   ; 	
	result->total_texture_src_height[0] = pixmap_height;
	
	VR_INFO("", VR_GRAPHIC_DBG_SOURCE, "size(%dx%d, %d)\n", pixmap_width, pixmap_height, pixmap_stride);		

	VR_INFO("", VR_GRAPHIC_DBG_SOURCE, "nxGSurfaceCreateScaleRGBASource done\n");		
	return result;
}

void nxGSurfaceDestroyScaleRGBASource ( HSURFSOURCE hSource )
{
	if( !hSource ){ return; }
	Statics *pStatics = vrGetStatics();
	_AUTO_BACKUP_CURRENT_EGL_;
	VR_ASSERT("Error: src_pixmap_images must exist", hSource->src_pixmap_images[0]);
	
	GL_CHECK(glDeleteTextures(1,&hSource->texture_name[0]));
	EGL_CHECK(_NX_eglDestroyImageKHR(pStatics->egl_info.sEGLDisplay, hSource->src_pixmap_images[0]));
	nxGSurfaceDestroyPixmap(hSource->src_native_pixmaps[0]);

	NX_FREE(hSource);
	VR_INFO("", VR_GRAPHIC_DBG_SOURCE, "nxGSurfaceDestroyScaleRGBASource done\n"); 		
}

#ifdef VR_FEATURE_SEPERATE_FD_USE
void  nxGSurfaceRunDeinterlace( HSURFTARGET hTarget, HSURFSOURCE hSource)
{
	Statics* pStatics = vrGetStatics();
	Shader* pshader = &(vrGetStatics()->shader[VR_PROGRAM_DEINTERLACE]);	
	const float aSquareVertex[] =
	{
		-1.0f,	-1.0f, 0.0f,
		-1.0f, 1.0f, 0.0f,
		 1.0f, 1.0f, 0.0f,
		 1.0f,	-1.0f, 0.0f,
	};	
	const float aSquareTexCoord[] =
	{
		0.0f, 1.0f,
		0.0f, 0.0f,
		1.0f, 0.0f,
		1.0f, 1.0f
	};
	if( NULL == pStatics || NULL == hTarget || NULL == hSource )
	{
		NxErrMsg("Error: NULL output surface at %s:%i\n", __FILE__, __LINE__);
		return;
	}
	_AUTO_BACKUP_CURRENT_EGL_;

	/* Make context current. */
	EGLBoolean bResult = eglMakeCurrent(pStatics->egl_info.sEGLDisplay, hTarget->target_pixmap_surface, 
					hTarget->target_pixmap_surface, pStatics->egl_info.sEGLContext[VR_PROGRAM_DEINTERLACE]);
	if(bResult == EGL_FALSE)
	{
		EGLint iError = eglGetError();
		NxErrMsg("eglGetError(): %i (0x%.4x)\n", (int)iError, (int)iError);
		NxErrMsg("Error: Failed to make context current at %s:%i\n", __FILE__, __LINE__);
		return;	
	}
	GL_CHECK(glUseProgram(pshader->iProgName));
	unsigned int width, height;
	width = ((VR_PLATFORM_PIXMAP_STRUCT *)hTarget->target_native_pixmap)->width;
	height = ((VR_PLATFORM_PIXMAP_STRUCT *)hTarget->target_native_pixmap)->height;
	GL_CHECK(glViewport(0,0,width,height));
	GL_CHECK(glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT)); // To optimize for tile-based renderer
	GL_CHECK(glVertexAttribPointer(pshader->iLocPosition, 3, GL_FLOAT, GL_FALSE, 0, aSquareVertex));
	GL_CHECK(glVertexAttribPointer(pshader->iLocTexCoord[0], 2, GL_FLOAT, GL_FALSE, 0, aSquareTexCoord));

	GL_CHECK(glEnableVertexAttribArray(pshader->iLocPosition));
	GL_CHECK(glEnableVertexAttribArray(pshader->iLocTexCoord[0]));

    GL_CHECK(glUniform1f(pshader->iLocInputHeight, hSource->height));
    //GL_CHECK(glUniform1f(pStatics->shader[program].iLocOutputHeight, output_height));
	GL_CHECK(glUniform1i(pshader->iLocMainTex[0], VR_INPUT_MODE_DEINTERLACE));
	GL_CHECK(glUniform1i(pshader->iLocRefTex, VR_INPUT_MODE_DEINTERLACE_REF));

	GL_CHECK(glActiveTexture(GL_TEXTURE0 + VR_INPUT_MODE_DEINTERLACE));
	GL_CHECK(glBindTexture(GL_TEXTURE_2D, hSource->texture_name[VR_INPUT_MODE_Y]));

	VR_INFO("", VR_GRAPHIC_DBG_RUN, "draw Deinterlace\n" ); 
	GL_CHECK(glDrawArrays(GL_TRIANGLE_FAN, 0, 4));
	vrWaitForDone();
}

void nxGSurfaceRunScale( HSURFTARGET* ptarget, HSURFSOURCE* psource)
{
	Statics* pStatics = vrGetStatics();
	Shader* pshader = &(vrGetStatics()->shader[VR_PROGRAM_SCALE]);	
	const float aSquareVertex[] =
	{
		-1.0f,	-1.0f, 0.0f,
		-1.0f, 1.0f, 0.0f,
		 1.0f, 1.0f, 0.0f,
		 1.0f,	-1.0f, 0.0f,
	};	

	#if 0
	const float aSquareTexCoord_degree0[] = { TPOS1, TPOS2, TPOS3, TPOS4 };
	const float aSquareTexCoord_degree90[] = { TPOS2, TPOS3, TPOS4, TPOS1 };
	const float aSquareTexCoord_degree180[] = { TPOS3, TPOS4, TPOS1, TPOS2 };
	const float aSquareTexCoord_degree270[] = { TPOS4, TPOS1, TPOS2, TPOS3 };
	const float aSquareTexCoord_mirror[] = { TPOS4, TPOS3, TPOS2, TPOS1 };
	const float aSquareTexCoord_vflip[] = { TPOS2, TPOS1, TPOS4, TPOS3 };
	const float* aSquareTexCoord;
	//temp test
	int mode = 1;
	printf("\nscale mode ===> %d\n", mode);
	
	switch(mode)
	{
		case 0 : aSquareTexCoord = aSquareTexCoord_degree0; break;
		case 1 : aSquareTexCoord = aSquareTexCoord_degree90; break;
		case 2 : aSquareTexCoord = aSquareTexCoord_degree180; break;
		case 3 : aSquareTexCoord = aSquareTexCoord_degree270; break;
		case 4 : aSquareTexCoord = aSquareTexCoord_mirror; break;
		case 5 : aSquareTexCoord = aSquareTexCoord_vflip; break;
		default : aSquareTexCoord = aSquareTexCoord_degree0;
	}
	#else
	const float aSquareTexCoord[] = { TPOS1, TPOS2, TPOS3, TPOS4 };
	#endif
	
	if( NULL == pStatics || NULL == pshader || NULL == ptarget || NULL == psource )
	{
		NxErrMsg("Error: NULL output surface at %s:%i\n", __FILE__, __LINE__);
		return;
	}
	_AUTO_BACKUP_CURRENT_EGL_;

	for(int i = 0 ; i < VR_YUV_CNT ; i++)
	{
		if(!ptarget[i] || !psource[i])
		{
			NxErrMsg("Error: ptarget(0x%x) psource(0x%x)\n", (int)ptarget[i], (int)psource[i]);		
			VR_ASSERT("Error: Y must exist", i != 0);				
			continue;
		}			
		
		/* Make context current. */
		EGLBoolean bResult = eglMakeCurrent(pStatics->egl_info.sEGLDisplay, ptarget[i]->target_pixmap_surface, ptarget[i]->target_pixmap_surface, pStatics->egl_info.sEGLContext[VR_PROGRAM_SCALE]);
		if(bResult == EGL_FALSE)
		{
			EGLint iError = eglGetError();
			NxErrMsg("eglGetError(): %i (0x%.4x)\n", (int)iError, (int)iError);
			NxErrMsg("Error: Failed to make context current at %s:%i\n", __FILE__, __LINE__);
			return; 
		}

		GL_CHECK(glUseProgram(pshader->iProgName));
		int x = 0, y = 0, width, height;
		width  = ((VR_PLATFORM_PIXMAP_STRUCT *)ptarget[i]->target_native_pixmap)->width;
		height = ((VR_PLATFORM_PIXMAP_STRUCT *)ptarget[i]->target_native_pixmap)->height;
		/*
		if(1 == i)
		{
			//U case
			y = height * 4;
		}
		else if(2 == i)
		{
			//V case
			y = height * 4;
			y+= height;
		}
		*/
		GL_CHECK(glViewport(x,y,width,height));				
		GL_CHECK(glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT)); // To optimize for tile-based renderer
		GL_CHECK(glVertexAttribPointer(pshader->iLocPosition, 3, GL_FLOAT, GL_FALSE, 0, aSquareVertex));
		GL_CHECK(glVertexAttribPointer(pshader->iLocTexCoord[0], 2, GL_FLOAT, GL_FALSE, 0, aSquareTexCoord));
		GL_CHECK(glEnableVertexAttribArray(pshader->iLocPosition));
		GL_CHECK(glEnableVertexAttribArray(pshader->iLocTexCoord[0]));
		GL_CHECK(glUniform1i(pshader->iLocMainTex[0], VR_INPUT_MODE_TEXTURE0));
		GL_CHECK(glActiveTexture(GL_TEXTURE0 + VR_INPUT_MODE_TEXTURE0));
		GL_CHECK(glBindTexture(GL_TEXTURE_2D, psource[i]->texture_name[0]));
		VR_INFO("", VR_GRAPHIC_DBG_RUN, "draw scaler\n" );

		GL_CHECK(glDrawArrays(GL_TRIANGLE_FAN, 0, 4));
		
		//temp test
		//EGL_CHECK(eglSwapBuffers(pStatics->egl_info.sEGLDisplay, ptarget[i]->target_pixmap_surface));

		vrWaitForDone();	
	}
}

void  nxGSurfaceRunCvt2Y( HSURFTARGET hTarget, HSURFSOURCE hSource)
{
	Statics* pStatics = vrGetStatics();
	Shader* pshader = &(vrGetStatics()->shader[VR_PROGRAM_CVT2Y]);	
	const float aSquareVertex[] =
	{
		-1.0f,	-1.0f, 0.0f,
		-1.0f, 1.0f, 0.0f,
		 1.0f, 1.0f, 0.0f,
		 1.0f,	-1.0f, 0.0f,
	};	
	const float aSquareTexCoord[] =
	{
		0.0f, 1.0f,
		0.0f, 0.0f,
		1.0f, 0.0f,
		1.0f, 1.0f
	};
	if( NULL == pStatics || NULL == hTarget || NULL == hSource )
	{
		NxErrMsg("Error: NULL output surface at %s:%i\n", __FILE__, __LINE__);
		return;
	}
	_AUTO_BACKUP_CURRENT_EGL_;

	/* Make context current. */
	EGLBoolean bResult = eglMakeCurrent(pStatics->egl_info.sEGLDisplay, hTarget->target_pixmap_surface, 
							hTarget->target_pixmap_surface, pStatics->egl_info.sEGLContext[VR_PROGRAM_CVT2Y]);
	if(bResult == EGL_FALSE)
	{
		EGLint iError = eglGetError();
		NxErrMsg("eglGetError(): %i (0x%.4x)\n", (int)iError, (int)iError);
		NxErrMsg("Error: Failed to make context current at %s:%i\n", __FILE__, __LINE__);
		return;	
	}
	GL_CHECK(glUseProgram(pshader->iProgName));
	
	int x=0, y=0, width, height;
	width  = ((VR_PLATFORM_PIXMAP_STRUCT *)hTarget->target_native_pixmap)->width;
	height = ((VR_PLATFORM_PIXMAP_STRUCT *)hTarget->target_native_pixmap)->height;
	GL_CHECK(glViewport(x,y,width,height));
	GL_CHECK(glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT)); // To optimize for tile-based renderer
	GL_CHECK(glVertexAttribPointer(pshader->iLocPosition, 3, GL_FLOAT, GL_FALSE, 0, aSquareVertex));
	GL_CHECK(glVertexAttribPointer(pshader->iLocTexCoord[0], 2, GL_FLOAT, GL_FALSE, 0, aSquareTexCoord));

	GL_CHECK(glEnableVertexAttribArray(pshader->iLocPosition));
	GL_CHECK(glEnableVertexAttribArray(pshader->iLocTexCoord[0]));

	GL_CHECK(glUniform1i(pshader->iLocMainTex[0], VR_INPUT_MODE_TEXTURE0));

	GL_CHECK(glActiveTexture(GL_TEXTURE0 + VR_INPUT_MODE_TEXTURE0));
	GL_CHECK(glBindTexture(GL_TEXTURE_2D, hSource->texture_name[VR_INPUT_MODE_Y]));

	VR_INFO("", VR_GRAPHIC_DBG_RUN, "draw\n" ); 
	GL_CHECK(glDrawArrays(GL_TRIANGLE_FAN, 0, 4));
	vrWaitForDone();
}

void  nxGSurfaceRunCvt2UV( HSURFTARGET hTarget, HSURFSOURCE hSource)
{
	//NxDbgMsg( "nxGSurfaceRunCvt2UV start\n" ); 

	Statics* pStatics = vrGetStatics();
	Shader* pshader = &(vrGetStatics()->shader[VR_PROGRAM_CVT2UV]);	
	const float aSquareVertex[] =
	{
		-1.0f,	-1.0f, 0.0f,
		-1.0f, 1.0f, 0.0f,
		 1.0f, 1.0f, 0.0f,
		 1.0f,	-1.0f, 0.0f,
	};	
	const float aSquareTexCoord[] =
	{
		0.0f, 1.0f,
		0.0f, 0.0f,
		1.0f, 0.0f,
		1.0f, 1.0f
	};
	if( NULL == pStatics || NULL == hTarget || NULL == hSource )
	{
		NxErrMsg("Error: NULL output surface at %s:%i\n", __FILE__, __LINE__);
		return;
	}
	_AUTO_BACKUP_CURRENT_EGL_;

	/* Make context current. */
	EGLBoolean bResult = eglMakeCurrent(pStatics->egl_info.sEGLDisplay, hTarget->target_pixmap_surface, 
							hTarget->target_pixmap_surface, pStatics->egl_info.sEGLContext[VR_PROGRAM_CVT2UV]);
	if(bResult == EGL_FALSE)
	{
		EGLint iError = eglGetError();
		NxErrMsg("eglGetError(): %i (0x%.4x)\n", (int)iError, (int)iError);
		NxErrMsg("Error: Failed to make context current at %s:%i\n", __FILE__, __LINE__);
		return;	
	}
	GL_CHECK(glUseProgram(pshader->iProgName));
	int x=0, y=0, width, height;
	width  = ((VR_PLATFORM_PIXMAP_STRUCT *)hTarget->target_native_pixmap)->width;
	height = ((VR_PLATFORM_PIXMAP_STRUCT *)hTarget->target_native_pixmap)->height;
	GL_CHECK(glViewport(x,y,width,height));
	GL_CHECK(glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT)); // To optimize for tile-based renderer
	GL_CHECK(glVertexAttribPointer(pshader->iLocPosition, 3, GL_FLOAT, GL_FALSE, 0, aSquareVertex));
	GL_CHECK(glVertexAttribPointer(pshader->iLocTexCoord[0], 2, GL_FLOAT, GL_FALSE, 0, aSquareTexCoord));

	GL_CHECK(glEnableVertexAttribArray(pshader->iLocPosition));
	GL_CHECK(glEnableVertexAttribArray(pshader->iLocTexCoord[0]));

	GL_CHECK(glUniform1i(pshader->iLocMainTex[0], VR_INPUT_MODE_TEXTURE1));

	GL_CHECK(glActiveTexture(GL_TEXTURE0 + VR_INPUT_MODE_TEXTURE1));
	GL_CHECK(glBindTexture(GL_TEXTURE_2D, hSource->texture_name[0]));

	VR_INFO("", VR_GRAPHIC_DBG_RUN, "draw(%d) Cvt2Yuv, (%d,%d) %dx%d\n", x, y, width, height );
	GL_CHECK(glDrawArrays(GL_TRIANGLE_FAN, 0, 4));
	vrWaitForDone();
}

void  nxGSurfaceRunCvt2Rgba( HSURFTARGET hTarget, HSURFSOURCE hSource)
{
	Statics* pStatics = vrGetStatics();
	Shader* pshader = &(vrGetStatics()->shader[VR_PROGRAM_CVT2RGBA]);	
	const float aSquareVertex[] =
	{
		-1.0f,	-1.0f, 0.0f,
		-1.0f, 1.0f, 0.0f,
		 1.0f, 1.0f, 0.0f,
		 1.0f,	-1.0f, 0.0f,
	};	
	const float aSquareTexCoord[] =
	{
		0.0f, 1.0f,
		0.0f, 0.0f,
		1.0f, 0.0f,
		1.0f, 1.0f
	};
	if( NULL == pStatics || NULL == hTarget || NULL == hSource )
	{
		NxErrMsg("Error: NULL output surface at %s:%i\n", __FILE__, __LINE__);
		return;
	}
	_AUTO_BACKUP_CURRENT_EGL_;

	/* Make context current. */
	EGLBoolean bResult = eglMakeCurrent(pStatics->egl_info.sEGLDisplay, hTarget->target_pixmap_surface, 
							hTarget->target_pixmap_surface, pStatics->egl_info.sEGLContext[VR_PROGRAM_CVT2RGBA]);
	if(bResult == EGL_FALSE)
	{
		EGLint iError = eglGetError();
		NxErrMsg("eglGetError(): %i (0x%.4x)\n", (int)iError, (int)iError);
		NxErrMsg("Error: Failed to make context current at %s:%i\n", __FILE__, __LINE__);
		return;	
	}
	GL_CHECK(glUseProgram(pshader->iProgName));
	int x=0, y=0, width, height;
	width  = ((VR_PLATFORM_PIXMAP_STRUCT *)hTarget->target_native_pixmap)->width;
	height = ((VR_PLATFORM_PIXMAP_STRUCT *)hTarget->target_native_pixmap)->height;
	GL_CHECK(glViewport(x,y,width,height));
	GL_CHECK(glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT)); // To optimize for tile-based renderer
	GL_CHECK(glVertexAttribPointer(pshader->iLocPosition, 3, GL_FLOAT, GL_FALSE, 0, aSquareVertex));
	GL_CHECK(glEnableVertexAttribArray(pshader->iLocPosition));
	GL_CHECK(glVertexAttribPointer(pshader->iLocTexCoord[0], 2, GL_FLOAT, GL_FALSE, 0, aSquareTexCoord));
	GL_CHECK(glEnableVertexAttribArray(pshader->iLocTexCoord[0]));

    //GL_CHECK(glUniform1f(pStatics->shader[program].iLocOutputHeight, output_height));
	GL_CHECK(glUniform1i(pshader->iLocMainTex[VR_INPUT_MODE_Y], VR_INPUT_MODE_Y));
	GL_CHECK(glUniform1i(pshader->iLocMainTex[VR_INPUT_MODE_U], VR_INPUT_MODE_U));
	GL_CHECK(glUniform1i(pshader->iLocMainTex[VR_INPUT_MODE_V], VR_INPUT_MODE_V));

	GL_CHECK(glActiveTexture(GL_TEXTURE0 + VR_INPUT_MODE_Y));
	GL_CHECK(glBindTexture(GL_TEXTURE_2D, hSource->texture_name[VR_INPUT_MODE_Y]));
	GL_CHECK(glActiveTexture(GL_TEXTURE0 + VR_INPUT_MODE_U));
	GL_CHECK(glBindTexture(GL_TEXTURE_2D, hSource->texture_name[VR_INPUT_MODE_U]));
	GL_CHECK(glActiveTexture(GL_TEXTURE0 + VR_INPUT_MODE_V));
	GL_CHECK(glBindTexture(GL_TEXTURE_2D, hSource->texture_name[VR_INPUT_MODE_V]));

	VR_INFO("", VR_GRAPHIC_DBG_RUN, "draw(%d) Cvt2Rgba, (%d,%d) %dx%d\n", x, y, width, height );
	GL_CHECK(glDrawArrays(GL_TRIANGLE_FAN, 0, 4));
	vrWaitForDone();
}

void nxGSurfaceRunYuvFilter( HSURFTARGET* ptarget, HSURFSOURCE* psource, float edgeParam)
{
	Statics* pStatics = vrGetStatics();
	const float aSquareVertex[] =
	{
		-1.0f,	-1.0f, 0.0f,
		-1.0f, 1.0f, 0.0f,
		 1.0f, 1.0f, 0.0f,
		 1.0f,	-1.0f, 0.0f,
	};	
	float tex_width, tex_height;

	#if 0
	const float aSquareTexCoord_degree0[] = { TPOS1, TPOS2, TPOS3, TPOS4 };
	const float aSquareTexCoord_degree90[] = { TPOS2, TPOS3, TPOS4, TPOS1 };
	const float aSquareTexCoord_degree180[] = { TPOS3, TPOS4, TPOS1, TPOS2 };
	const float aSquareTexCoord_degree270[] = { TPOS4, TPOS1, TPOS2, TPOS3 };
	const float aSquareTexCoord_mirror[] = { TPOS4, TPOS3, TPOS2, TPOS1 };
	const float aSquareTexCoord_vflip[] = { TPOS2, TPOS1, TPOS4, TPOS3 };
	const float* aSquareTexCoord;
	//temp test
	int mode = 1;
	printf("\nscale mode ===> %d\n", mode);
	
	switch(mode)
	{
		case 0 : aSquareTexCoord = aSquareTexCoord_degree0; break;
		case 1 : aSquareTexCoord = aSquareTexCoord_degree90; break;
		case 2 : aSquareTexCoord = aSquareTexCoord_degree180; break;
		case 3 : aSquareTexCoord = aSquareTexCoord_degree270; break;
		case 4 : aSquareTexCoord = aSquareTexCoord_mirror; break;
		case 5 : aSquareTexCoord = aSquareTexCoord_vflip; break;
		default : aSquareTexCoord = aSquareTexCoord_degree0;
	}
	#else
	const float aSquareTexCoord[] = { TPOS1, TPOS2, TPOS3, TPOS4 };
	float aSquareTexCoordArrays[5][8] = 
	{ 
		{ TPOS1, TPOS2, TPOS3, TPOS4 },
		{ TPOS1, TPOS2, TPOS3, TPOS4 },
		{ TPOS1, TPOS2, TPOS3, TPOS4 },
		{ TPOS1, TPOS2, TPOS3, TPOS4 },
		{ TPOS1, TPOS2, TPOS3, TPOS4 }
	};

/*
#define TPOS00_1 	0.0f-1.5f, 1.0f-1.5f
#define TPOS00_2 	0.0f-1.5f, 0.0f-1.5f
#define TPOS00_3 	1.0f-1.5f, 0.0f-1.5f
#define TPOS00_4 	1.0f-1.5f, 1.0f-1.5f
#define TPOS02_1 	0.0f-1.5f, 1.0f+1.5f
#define TPOS02_2 	0.0f-1.5f, 0.0f+1.5f
#define TPOS02_3 	1.0f-1.5f, 0.0f+1.5f
#define TPOS02_4 	1.0f-1.5f, 1.0f+1.5f
#define TPOS11_1 	0.0f, 1.0f
#define TPOS11_2 	0.0f, 0.0f
#define TPOS11_3 	1.0f, 0.0f
#define TPOS11_4 	1.0f, 1.0f
#define TPOS20_1 	0.0f+1.5f, 1.0f-1.5f
#define TPOS20_2 	0.0f+1.5f, 0.0f-1.5f
#define TPOS20_3 	1.0f+1.5f, 0.0f-1.5f
#define TPOS20_4 	1.0f+1.5f, 1.0f-1.5f
#define TPOS22_1 	0.0f+1.5f, 1.0f+1.5f
#define TPOS22_2 	0.0f+1.5f, 0.0f+1.5f
#define TPOS22_3 	1.0f+1.5f, 0.0f+1.5f
#define TPOS22_4 	1.0f+1.5f, 1.0f+1.5f
*/
	tex_width = (float)psource[0]->width;
	tex_height = (float)psource[0]->height;

	float tex_offset_x = 1.f/tex_width;
	float tex_offset_y = 1.f/tex_height;
	
	aSquareTexCoordArrays[0][0] -= tex_offset_x, aSquareTexCoordArrays[0][1] -= tex_offset_y;
	aSquareTexCoordArrays[0][2] -= tex_offset_x, aSquareTexCoordArrays[0][3] -= tex_offset_y;
	aSquareTexCoordArrays[0][4] -= tex_offset_x, aSquareTexCoordArrays[0][5] -= tex_offset_y;
	aSquareTexCoordArrays[0][6] -= tex_offset_x, aSquareTexCoordArrays[0][7] -= tex_offset_y;
	aSquareTexCoordArrays[1][0] -= tex_offset_x, aSquareTexCoordArrays[1][1] += tex_offset_y;
	aSquareTexCoordArrays[1][2] -= tex_offset_x, aSquareTexCoordArrays[1][3] += tex_offset_y;
	aSquareTexCoordArrays[1][4] -= tex_offset_x, aSquareTexCoordArrays[1][5] += tex_offset_y;
	aSquareTexCoordArrays[1][6] -= tex_offset_x, aSquareTexCoordArrays[1][7] += tex_offset_y;
	aSquareTexCoordArrays[3][0] += tex_offset_x, aSquareTexCoordArrays[3][1] -= tex_offset_y;
	aSquareTexCoordArrays[3][2] += tex_offset_x, aSquareTexCoordArrays[3][3] -= tex_offset_y;
	aSquareTexCoordArrays[3][4] += tex_offset_x, aSquareTexCoordArrays[3][5] -= tex_offset_y;
	aSquareTexCoordArrays[3][6] += tex_offset_x, aSquareTexCoordArrays[3][7] -= tex_offset_y;
	aSquareTexCoordArrays[4][0] += tex_offset_x, aSquareTexCoordArrays[4][1] += tex_offset_y;
	aSquareTexCoordArrays[4][2] += tex_offset_x, aSquareTexCoordArrays[4][3] += tex_offset_y;
	aSquareTexCoordArrays[4][4] += tex_offset_x, aSquareTexCoordArrays[4][5] += tex_offset_y;
	aSquareTexCoordArrays[4][6] += tex_offset_x, aSquareTexCoordArrays[4][7] += tex_offset_y;	
	#endif
	
	if( NULL == pStatics || NULL == ptarget || NULL == psource )
	{
		NxErrMsg("Error: NULL output surface at %s:%i\n", __FILE__, __LINE__);
		return;
	}
	_AUTO_BACKUP_CURRENT_EGL_;	
	int program;
	for(int i = 0 ; i < VR_YUV_CNT ; i++)
	{	
		#if 1
		if (0 == i)
			program = VR_PROGRAM_YUVFILTER_Y;
		else
			program = VR_PROGRAM_YUVFILTER_UV;
		#else
		program = VR_PROGRAM_YUVFILTER_Y;
		#endif
		Shader* pshader = &(vrGetStatics()->shader[program]);	
		if( NULL == pshader )
		{
			NxErrMsg("Error: NULL output surface at %s:%i\n", __FILE__, __LINE__);
			return;
		}

		tex_width = (float)psource[i]->width;
		tex_height = (float)psource[i]->height;
	
		if(!ptarget[i] || !psource[i])
		{
			NxErrMsg("Error: ptarget(0x%x) psource(0x%x)\n", (int)ptarget[i], (int)psource[i]);		
			VR_ASSERT("Error: Y must exist", i != 0);				
			continue;
		}			
		
		/* Make context current. */
		EGLBoolean bResult = eglMakeCurrent(pStatics->egl_info.sEGLDisplay, ptarget[i]->target_pixmap_surface, ptarget[i]->target_pixmap_surface, pStatics->egl_info.sEGLContext[VR_PROGRAM_YUVFILTER_Y]);
		if(bResult == EGL_FALSE)
		{
			EGLint iError = eglGetError();
			NxErrMsg("eglGetError(): %i (0x%.4x)\n", (int)iError, (int)iError);
			NxErrMsg("Error: Failed to make context current at %s:%i\n", __FILE__, __LINE__);
			return; 
		}

		GL_CHECK(glUseProgram(pshader->iProgName));
		int x = 0, y = 0, width, height;
		width  = ((VR_PLATFORM_PIXMAP_STRUCT *)ptarget[i]->target_native_pixmap)->width;
		height = ((VR_PLATFORM_PIXMAP_STRUCT *)ptarget[i]->target_native_pixmap)->height;

		VR_INFO("", VR_GRAPHIC_DBG_RUN, "draw scaler. size(%f x %f)\n", tex_width, tex_height);

		#if 1 //org
		GL_CHECK(glViewport(x,y,width,height));	
		#else //temp test
		#endif
		GL_CHECK(glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT)); // To optimize for tile-based renderer
		GL_CHECK(glVertexAttribPointer(pshader->iLocPosition, 3, GL_FLOAT, GL_FALSE, 0, aSquareVertex));
		GL_CHECK(glEnableVertexAttribArray(pshader->iLocPosition));
		GL_CHECK(glEnableVertexAttribArray(pshader->iLocTexCoord[0]));
		GL_CHECK(glUniform1i(pshader->iLocMainTex[0], VR_INPUT_MODE_TEXTURE0));
		if (VR_PROGRAM_YUVFILTER_Y == program)
		{
			GL_CHECK(glUniform2f(pshader->iLocTexInvSize, 1.f/tex_width, 1.f/tex_height));
			GL_CHECK(glUniform1f(pshader->iLocFilterGain, edgeParam/* gain */));
		}
		GL_CHECK(glActiveTexture(GL_TEXTURE0 + VR_INPUT_MODE_TEXTURE0));
		GL_CHECK(glBindTexture(GL_TEXTURE_2D, psource[i]->texture_name[0]));
		#if 1 //org
		GL_CHECK(glVertexAttribPointer(pshader->iLocTexCoord[0], 2, GL_FLOAT, GL_FALSE, 0, aSquareTexCoord));
		GL_CHECK(glDrawArrays(GL_TRIANGLE_FAN, 0, 4));
		#else //temp test
		/* temp test */
		glEnable(GL_BLEND);
		//glBlendFunc(GL_CONSTANT_COLOR, GL_ONE);	
		glBlendFunc(GL_SRC_ALPHA, GL_ONE);	
		glBlendEquation(GL_FUNC_ADD);
		glBlendColor(1.000, 1.000, 0.200, 1.000);
		for(int j = 0 ; j < 5/*5*/ ; j++)
		{
#if 1 //org
			GL_CHECK(glViewport(x,y,width,height)); 		
#else
			if (0 == j)
			{
				GL_CHECK(glViewport(width/4,height/4,width/2,height/2));							
			}
			else if (1 == j)
			{
				GL_CHECK(glViewport(x,y,width/2,height/2)); 		
			}
			else if (2 == j)
			{
				GL_CHECK(glViewport(x+width/2,y,width/2,height/2)); 		
			}
			else if (3 == j)
			{
				GL_CHECK(glViewport(x,y+height/2,width/2,height/2));		
			}
			else if (4 == j)
			{
				GL_CHECK(glViewport(x+width/2,y+height/2,width/2,height/2));		
			}
			
#endif
			GL_CHECK(glVertexAttribPointer(pshader->iLocTexCoord[0], 2, GL_FLOAT, GL_FALSE, 0, aSquareTexCoordArrays[j]));
			GL_CHECK(glDrawArrays(GL_TRIANGLE_FAN, 0, 4));		
		}
		glBlendFunc(GL_ONE, GL_ONE);	
		glBlendEquation(GL_FUNC_SUBTRACT);
		GL_CHECK(glVertexAttribPointer(pshader->iLocTexCoord[0], 2, GL_FLOAT, GL_FALSE, 0, aSquareTexCoordArrays[2]));
		GL_CHECK(glDrawArrays(GL_TRIANGLE_FAN, 0, 4));		
		#endif
		
		vrWaitForDone();	
	}

}

#else

void nxGSurfaceRunDeinterlace( HSURFTARGET hTarget, HSURFSOURCE hSource )
{
	Statics* pStatics = vrGetStatics();
	Shader* pshader = &(vrGetStatics()->shader[VR_PROGRAM_DEINTERLACE]);	
	const float aSquareVertex[] =
	{
		-1.0f,	-1.0f, 0.0f,
		-1.0f, 1.0f, 0.0f,
		 1.0f, 1.0f, 0.0f,
		 1.0f,	-1.0f, 0.0f,
	};	
	float aSquareTexCoord[VR_INPUT_MODE_YUV_MAX][8] =
	{
		{
			0.0f, 1.0f,
			0.0f, 0.0f,
			1.0f, 0.0f,
			1.0f, 1.0f
		},
		{
			0.0f, 1.0f,
			0.0f, 0.0f,
			1.0f, 0.0f,
			1.0f, 1.0f
		},
		{
			0.0f, 1.0f,
			0.0f, 0.0f,
			1.0f, 0.0f,
			1.0f, 1.0f
		}
	};

	VR_INFO("", VR_GRAPHIC_DBG_RUN, "%s start\n", __FUNCTION__);

#if (VR_FEATURE_INPUT_HEIGHT_ALIGN_BYTE > 0)
	float min_y, max_y, tex_height, tex_total_height_pitch; 
	float y_align_blank_height = 0.f, u_align_blank_height = 0.f;
	{
		unsigned int y_height, u_height;
		//add Y align blank offset
		y_height = hSource->height;
		if(hSource->height % VR_FEATURE_INPUT_HEIGHT_ALIGN_BYTE)
		{				
			y_align_blank_height = (float)(VR_FEATURE_INPUT_HEIGHT_ALIGN_BYTE - (y_height % VR_FEATURE_INPUT_HEIGHT_ALIGN_BYTE)) * 2;
		}
		//add U align blank offset
		u_height = hSource->height/2;
		if(u_height % VR_FEATURE_INPUT_HEIGHT_ALIGN_BYTE)
		{	
			u_align_blank_height = (float)(VR_FEATURE_INPUT_HEIGHT_ALIGN_BYTE - (u_height % VR_FEATURE_INPUT_HEIGHT_ALIGN_BYTE));
		}
		VR_INFO("", VR_GRAPHIC_DBG_HEIGHT_ALIGN, "y_align_blank_height(%f), u_align_blank_height(%f)\n", y_align_blank_height, u_align_blank_height);		
	}
	tex_height = (float)hSource->height * 2.f;
	//set V coord
	tex_total_height_pitch = tex_height + tex_height/2 + y_align_blank_height + u_align_blank_height;
	min_y = tex_height + y_align_blank_height + tex_height/4.f + u_align_blank_height;
	VR_INFO("", VR_GRAPHIC_DBG_HEIGHT_ALIGN, "V, min(%f),", min_y);		
	//normalize
	min_y /= tex_total_height_pitch;
	VR_INFO("", VR_GRAPHIC_DBG_HEIGHT_ALIGN, " min nomalize(%f of %f)\n", min_y, tex_total_height_pitch);		
	aSquareTexCoord[0][3] = aSquareTexCoord[0][5] = min_y;
	//aSquareTexCoord[0][1] = aSquareTexCoord[0][7] = 1.f;

	//set U coord
	tex_total_height_pitch = tex_height + y_align_blank_height + tex_height/4;
	min_y = tex_height + y_align_blank_height;
	VR_INFO("", VR_GRAPHIC_DBG_HEIGHT_ALIGN, "U, min(%f),", min_y);		
	//normalize
	min_y /= tex_total_height_pitch;
	VR_INFO("", VR_GRAPHIC_DBG_HEIGHT_ALIGN, " min nomalize(%f)\n", min_y);		
	aSquareTexCoord[1][3] = aSquareTexCoord[1][5] = min_y;
	max_y = tex_height + y_align_blank_height + tex_height/4.f;
	VR_INFO("", VR_GRAPHIC_DBG_HEIGHT_ALIGN, "U, max(%f),", max_y);		
	//normalize
	max_y /= tex_total_height_pitch;
	VR_INFO("", VR_GRAPHIC_DBG_HEIGHT_ALIGN, " max nomalize(%f)\n", max_y);		
	aSquareTexCoord[1][1] = aSquareTexCoord[1][7] = max_y;
	
#else

	//set V coord
	//min y
	aSquareTexCoord[0][3] = aSquareTexCoord[0][5] = 5.f/6.f;
	//max y
	//aSquareTexCoord[0][1] = aSquareTexCoord[0][7] = 1.f;
	
	//set U coord
	//min y
	aSquareTexCoord[1][3] = aSquareTexCoord[1][5] = 4.f/5.f;
	//max y
	//aSquareTexCoord[1][1] = aSquareTexCoord[1][7] = 1.f;
#endif

	#ifdef VR_FEATURE_STRIDE_USE
	if(hSource->stride != hSource->width)
	{
		float max_x = (float)hSource->width/(float)hSource->stride;
		aSquareTexCoord[2][4] = aSquareTexCoord[2][6] = max_x;
		aSquareTexCoord[1][4] = aSquareTexCoord[1][6] = max_x;
		aSquareTexCoord[0][4] = aSquareTexCoord[0][6] = max_x;
	}	
	#endif
	
	if( NULL == pStatics || NULL == hTarget || NULL == hSource )
	{
		NxErrMsg("Error:  hTarget(0x%x), hSource(0x%x) at %s:%i\n", hTarget, hSource, __FILE__, __LINE__);
		return;
	}
	_AUTO_BACKUP_CURRENT_EGL_;

	for(int i = 0 ; i < VR_YUV_CNT ; i++)
	{			
		if(!hTarget->target_pixmap_surfaces[i])
		{
			VR_ASSERT("Error: Y must exist", i != 0);				
			continue;
		}			

		/* Make context current. */
		EGLBoolean bResult = eglMakeCurrent(pStatics->egl_info.sEGLDisplay, 
										hTarget->target_pixmap_surfaces[i], 
										hTarget->target_pixmap_surfaces[i], 
										pStatics->egl_info.sEGLContext[VR_PROGRAM_DEINTERLACE]);
		if(bResult == EGL_FALSE)
		{
			EGLint iError = eglGetError();
			NxErrMsg("eglGetError(): %i (0x%.4x)\n", (int)iError, (int)iError);
			NxErrMsg("Error: Failed to make context current at %s:%i\n", __FILE__, __LINE__);
			return; 
		}

		GL_CHECK(glUseProgram(pshader->iProgName));		
		int x=0, y=0, width, height;
		if(2 == i)
		{
			//Y case
			width  = hTarget->width;
			height = hTarget->height;
		}
		else if(1 == i)
		{
			//U case
			width  = hTarget->width/2;
			height = hTarget->height/2;
		}
		else
		{
			//V case
			width  = hTarget->width/2;
			height = hTarget->height/2;
		}		
		GL_CHECK(glViewport(x,y,width/4,height)); /* input Y 4���� �Ѳ�����*/
		GL_CHECK(glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT)); // To optimize for tile-based renderer
		GL_CHECK(glVertexAttribPointer(pshader->iLocPosition, 3, GL_FLOAT, GL_FALSE, 0, aSquareVertex));
		GL_CHECK(glVertexAttribPointer(pshader->iLocTexCoord[0], 2, GL_FLOAT, GL_FALSE, 0, aSquareTexCoord[i]));

		GL_CHECK(glEnableVertexAttribArray(pshader->iLocPosition));
		GL_CHECK(glEnableVertexAttribArray(pshader->iLocTexCoord[0]));

		GL_CHECK(glUniform1f(pshader->iLocInputHeight, hSource->total_texture_src_height[i]));
		GL_CHECK(glUniform1i(pshader->iLocMainTex[0], VR_INPUT_MODE_DEINTERLACE));
		GL_CHECK(glUniform1i(pshader->iLocRefTex, VR_INPUT_MODE_DEINTERLACE_REF));

		GL_CHECK(glActiveTexture(GL_TEXTURE0 + VR_INPUT_MODE_DEINTERLACE));
		GL_CHECK(glBindTexture(GL_TEXTURE_2D, hSource->texture_name[i]));

		GL_CHECK(glDrawArrays(GL_TRIANGLE_FAN, 0, 4));		

		vrWaitForDone();
		VR_INFO("", VR_GRAPHIC_DBG_RUN, "draw(%d) deinterlace, (%d,%d) %dx%d\n", i, x, y, width, height );
	}	
}

#define TPOS1 	0.0f, 1.0f
#define TPOS2 	0.0f, 0.0f
#define TPOS3 	1.0f, 0.0f
#define TPOS4 	1.0f, 1.0f

void nxGSurfaceRunScale( HSURFTARGET hTarget, HSURFSOURCE hSource )
{
	Statics* pStatics = vrGetStatics();
	Shader* pshader = &(vrGetStatics()->shader[VR_PROGRAM_SCALE]);	
	const float aSquareVertex[] =
	{
		-1.0f,	-1.0f, 0.0f,
		-1.0f, 1.0f, 0.0f,
		 1.0f, 1.0f, 0.0f,
		 1.0f,	-1.0f, 0.0f,
	};	

#if 0
	float aSquareTexCoord_degree0[VR_INPUT_MODE_YUV_MAX][8] = { {TPOS1, TPOS2, TPOS3, TPOS4}, {TPOS1, TPOS2, TPOS3, TPOS4}, {TPOS1, TPOS2, TPOS3, TPOS4} };
	float aSquareTexCoord_degree90[VR_INPUT_MODE_YUV_MAX][8] = { {TPOS2, TPOS3, TPOS4, TPOS1}, {TPOS2, TPOS3, TPOS4, TPOS1}, {TPOS2, TPOS3, TPOS4, TPOS1} };
	float aSquareTexCoord_degree180[VR_INPUT_MODE_YUV_MAX][8] = { {TPOS3, TPOS4, TPOS1, TPOS2}, {TPOS3, TPOS4, TPOS1, TPOS2}, {TPOS3, TPOS4, TPOS1, TPOS2} };
	float aSquareTexCoord_degree270[VR_INPUT_MODE_YUV_MAX][8] = { {TPOS4, TPOS1, TPOS2, TPOS3}, {TPOS4, TPOS1, TPOS2, TPOS3}, {TPOS4, TPOS1, TPOS2, TPOS3} };
	float aSquareTexCoord_mirror[VR_INPUT_MODE_YUV_MAX][8] = { {TPOS4, TPOS3, TPOS2, TPOS1}, {TPOS4, TPOS3, TPOS2, TPOS1}, {TPOS4, TPOS3, TPOS2, TPOS1} };
	float aSquareTexCoord_vflip[VR_INPUT_MODE_YUV_MAX][8] = { {TPOS2, TPOS1, TPOS4, TPOS3}, {TPOS2, TPOS1, TPOS4, TPOS3}, {TPOS2, TPOS1, TPOS4, TPOS3} };
	float aSquareTexCoord[VR_INPUT_MODE_YUV_MAX][8];
	//temp test
	int mode = 2;
	printf("\nscale mode ===> %d\n", mode);
	
	switch(mode)
	{
		case 0 : MEMCPY_FUNC(aSquareTexCoord, aSquareTexCoord_degree0, sizeof(aSquareTexCoord)); break;
		case 1 : MEMCPY_FUNC(aSquareTexCoord, aSquareTexCoord_degree90, sizeof(aSquareTexCoord)); break;
		case 2 : MEMCPY_FUNC(aSquareTexCoord, aSquareTexCoord_degree180, sizeof(aSquareTexCoord)); break;
		case 3 : MEMCPY_FUNC(aSquareTexCoord, aSquareTexCoord_degree270, sizeof(aSquareTexCoord)); break;
		case 4 : MEMCPY_FUNC(aSquareTexCoord, aSquareTexCoord_mirror, sizeof(aSquareTexCoord)); break;
		case 5 : MEMCPY_FUNC(aSquareTexCoord, aSquareTexCoord_vflip, sizeof(aSquareTexCoord)); break;
		default : MEMCPY_FUNC(aSquareTexCoord, aSquareTexCoord_degree0, sizeof(aSquareTexCoord));
	}
#else	
	float aSquareTexCoord[VR_INPUT_MODE_YUV_MAX][8] =
	{
		{
			0.0f, 1.0f,
			0.0f, 0.0f,
			1.0f, 0.0f,
			1.0f, 1.0f
		},
		{
			0.0f, 1.0f,
			0.0f, 0.0f,
			1.0f, 0.0f,
			1.0f, 1.0f
		},
		{
			0.0f, 1.0f,
			0.0f, 0.0f,
			1.0f, 0.0f,
			1.0f, 1.0f
		}
	};
#endif	
	VR_INFO("", VR_GRAPHIC_DBG_RUN, "%s start\n", __FUNCTION__);
	
#if (VR_FEATURE_INPUT_HEIGHT_ALIGN_BYTE > 0)
	float min_y, max_y, tex_height, tex_total_height_pitch; 
	float y_align_blank_height = 0.f, u_align_blank_height = 0.f;
	{
		unsigned int y_height, u_height;
		//add Y align blank offset
		y_height = hSource->height;
		if(hSource->height % VR_FEATURE_INPUT_HEIGHT_ALIGN_BYTE)
		{				
			y_align_blank_height = (float)(VR_FEATURE_INPUT_HEIGHT_ALIGN_BYTE - (y_height % VR_FEATURE_INPUT_HEIGHT_ALIGN_BYTE)) * 2;
		}
		//add U align blank offset
		u_height = hSource->height/2;
		if(u_height % VR_FEATURE_INPUT_HEIGHT_ALIGN_BYTE)
		{	
			u_align_blank_height = (float)(VR_FEATURE_INPUT_HEIGHT_ALIGN_BYTE - (u_height % VR_FEATURE_INPUT_HEIGHT_ALIGN_BYTE));
		}
		VR_INFO("", VR_GRAPHIC_DBG_HEIGHT_ALIGN, "y_align_blank_height(%f), u_align_blank_height(%f)\n", y_align_blank_height, u_align_blank_height);		
	}
	tex_height = (float)hSource->height * 2.f;
	//set V coord
	tex_total_height_pitch = tex_height + tex_height/2 + y_align_blank_height + u_align_blank_height;
	min_y = tex_height + y_align_blank_height + tex_height/4.f + u_align_blank_height;
	VR_INFO("", VR_GRAPHIC_DBG_HEIGHT_ALIGN, "V, min(%f),", min_y);		
	//normalize
	min_y /= tex_total_height_pitch;
	VR_INFO("", VR_GRAPHIC_DBG_HEIGHT_ALIGN, " min nomalize(%f of %f)\n", min_y, tex_total_height_pitch);		
	aSquareTexCoord[0][3] = aSquareTexCoord[0][5] = min_y;
	//aSquareTexCoord[0][1] = aSquareTexCoord[0][7] = 1.f;

	//set U coord
	tex_total_height_pitch = tex_height + y_align_blank_height + tex_height/4;
	min_y = tex_height + y_align_blank_height;
	VR_INFO("", VR_GRAPHIC_DBG_HEIGHT_ALIGN, "U, min(%f),", min_y);		
	//normalize
	min_y /= tex_total_height_pitch;
	VR_INFO("", VR_GRAPHIC_DBG_HEIGHT_ALIGN, " min nomalize(%f)\n", min_y);		
	aSquareTexCoord[1][3] = aSquareTexCoord[1][5] = min_y;
	max_y = tex_height + y_align_blank_height + tex_height/4.f;
	VR_INFO("", VR_GRAPHIC_DBG_HEIGHT_ALIGN, "U, max(%f),", max_y);		
	//normalize
	max_y /= tex_total_height_pitch;
	VR_INFO("", VR_GRAPHIC_DBG_HEIGHT_ALIGN, " max nomalize(%f)\n", max_y);		
	aSquareTexCoord[1][1] = aSquareTexCoord[1][7] = max_y;

#else

	//set V coord
	//min y
	aSquareTexCoord[0][3] = aSquareTexCoord[0][5] = 5.f/6.f;
	//max y
	//aSquareTexCoord[0][1] = aSquareTexCoord[0][7] = 1.f;
	//set U coord
	//min y
	aSquareTexCoord[1][3] = aSquareTexCoord[1][5] = 4.f/5.f;
	//max y
	//aSquareTexCoord[1][1] = aSquareTexCoord[1][7] = 1.f;		
	VR_INFO("", VR_GRAPHIC_DBG_HEIGHT_ALIGN, "U(%f), V(%f)\n", 4.f/6.f, 5.f/6.f);		
#endif

	#ifdef VR_FEATURE_STRIDE_USE
	if(hSource->stride != hSource->width)
	{
		float max_x = (float)hSource->width/(float)hSource->stride;
		aSquareTexCoord[2][4] = aSquareTexCoord[2][6] = max_x;
		aSquareTexCoord[1][4] = aSquareTexCoord[1][6] = max_x;
		aSquareTexCoord[0][4] = aSquareTexCoord[0][6] = max_x;
	}
	#endif
	
	if( NULL == pStatics || NULL == hTarget || NULL == hSource )
	{
		NxErrMsg("Error:  hTarget(0x%x), hSource(0x%x) at %s:%i\n", hTarget, hSource, __FILE__, __LINE__);
		return;
	}
	_AUTO_BACKUP_CURRENT_EGL_;

	for(int i = 0 ; i < VR_YUV_CNT ; i++)
	{		
		if(!hTarget->target_pixmap_surfaces[i])
		{
			VR_ASSERT("Error: Y must exist", i != 0);				
			continue;
		}			

		/* Make context current. */
		EGLBoolean bResult = eglMakeCurrent(pStatics->egl_info.sEGLDisplay, 
										hTarget->target_pixmap_surfaces[i], 
										hTarget->target_pixmap_surfaces[i], 
										pStatics->egl_info.sEGLContext[VR_PROGRAM_SCALE]);
		if(bResult == EGL_FALSE)
		{
			EGLint iError = eglGetError();
			NxErrMsg("eglGetError(): %i (0x%.4x)\n", (int)iError, (int)iError);
			NxErrMsg("Error: Failed to make context current at %s:%i\n", __FILE__, __LINE__);
			return; 
		}

		GL_CHECK(glUseProgram(pshader->iProgName));		
		int x=0, y=0, width, height;
		if(2 == i)
		{
			//Y case
			width  = hTarget->width;
			height = hTarget->height;
		}
		else if(1 == i)
		{
			//U case
			width  = (hTarget->width)/2;
			height = (hTarget->height)/2;
		}
		else
		{
			//V case
			width  = (hTarget->width)/2;
			height = (hTarget->height)/2;
		}		
		GL_CHECK(glViewport(x,y,width,height));		
		GL_CHECK(glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT)); // To optimize for tile-based renderer
		GL_CHECK(glVertexAttribPointer(pshader->iLocPosition, 3, GL_FLOAT, GL_FALSE, 0, aSquareVertex));
		GL_CHECK(glVertexAttribPointer(pshader->iLocTexCoord[0], 2, GL_FLOAT, GL_FALSE, 0, aSquareTexCoord[i]));

		GL_CHECK(glEnableVertexAttribArray(pshader->iLocPosition));
		GL_CHECK(glEnableVertexAttribArray(pshader->iLocTexCoord[0]));

		GL_CHECK(glUniform1i(pshader->iLocMainTex[0], VR_INPUT_MODE_TEXTURE0));

		GL_CHECK(glActiveTexture(GL_TEXTURE0 + VR_INPUT_MODE_TEXTURE0));
		GL_CHECK(glBindTexture(GL_TEXTURE_2D, hSource->texture_name[i]));

		GL_CHECK(glDrawArrays(GL_TRIANGLE_FAN, 0, 4));		

		vrWaitForDone();
		VR_INFO("", VR_GRAPHIC_DBG_RUN, "draw(%d) scaler, (%d,%d) %dx%d\n", i, x, y, width, height );
	}	
}

void  nxGSurfaceRunCvt2Yuv( HSURFTARGET hTarget, HSURFSOURCE hSource)
{
	Statics* pStatics = vrGetStatics();
	const float aSquareVertex[] =
	{
		-1.0f,	-1.0f, 0.0f,
		-1.0f, 1.0f, 0.0f,
		 1.0f, 1.0f, 0.0f,
		 1.0f,	-1.0f, 0.0f,
	};	
	float aSquareTexCoord[] =
	{
		0.0f, 1.0f,
		0.0f, 0.0f,
		1.0f, 0.0f,
		1.0f, 1.0f
	};
	
	if( NULL == pStatics || NULL == hTarget || NULL == hSource )
	{
		NxErrMsg("Error:	hTarget(0x%x), hSource(0x%x) at %s:%i\n", hTarget, hSource, __FILE__, __LINE__);
		return;
	}
	VR_INFO("", VR_GRAPHIC_DBG_RUN, "%s start\n", __FUNCTION__);

	#ifdef VR_FEATURE_STRIDE_USE
	if(hSource->stride != hSource->width)
	{
		float max_x = (float)hSource->width/(float)hSource->stride;
		aSquareTexCoord[4] = aSquareTexCoord[6] = max_x;
	}
	#endif
	
	_AUTO_BACKUP_CURRENT_EGL_;

	for(int i = 0 ; i < VR_Y_UV_CNT ; i++)
	{			
		Shader* pshader = &(vrGetStatics()->shader[VR_PROGRAM_CVT2UV+i]);	
		if(!hTarget->target_pixmap_surfaces[i])
		{
			VR_ASSERT("Error: Y must exist", i != 0);				
			continue;
		}			

		/* Make context current. */
		EGLBoolean bResult = eglMakeCurrent(pStatics->egl_info.sEGLDisplay, 
										hTarget->target_pixmap_surfaces[i], 
										hTarget->target_pixmap_surfaces[i], 
										pStatics->egl_info.sEGLContext[VR_PROGRAM_CVT2UV+i]);
		if(bResult == EGL_FALSE)
		{
			EGLint iError = eglGetError();
			NxErrMsg("eglGetError(): %i (0x%.4x)\n", (int)iError, (int)iError);
			NxErrMsg("Error: Failed to make context current at %s:%i\n", __FILE__, __LINE__);
			return; 
		}

		GL_CHECK(glUseProgram(pshader->iProgName)); 	
		int x=0, y=0, width, height;
		if(1 == i)
		{
			width  = hTarget->width;
			height = hTarget->height;
		}
		else
		{
			//UV case
			width  = (hTarget->width)/2;
			height = (hTarget->height)/2;
		}
		GL_CHECK(glViewport(x,y,width,height));
		GL_CHECK(glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT)); // To optimize for tile-based renderer
		GL_CHECK(glVertexAttribPointer(pshader->iLocPosition, 3, GL_FLOAT, GL_FALSE, 0, aSquareVertex));
		GL_CHECK(glVertexAttribPointer(pshader->iLocTexCoord[0], 2, GL_FLOAT, GL_FALSE, 0, aSquareTexCoord));

		GL_CHECK(glEnableVertexAttribArray(pshader->iLocPosition));
		GL_CHECK(glEnableVertexAttribArray(pshader->iLocTexCoord[0]));

		GL_CHECK(glUniform1i(pshader->iLocMainTex[0], VR_INPUT_MODE_TEXTURE0+i));

		GL_CHECK(glActiveTexture(GL_TEXTURE0 + VR_INPUT_MODE_TEXTURE0+i));
		GL_CHECK(glBindTexture(GL_TEXTURE_2D, hSource->texture_name[i]));

		GL_CHECK(glDrawArrays(GL_TRIANGLE_FAN, 0, 4));		

		vrWaitForDone();
		VR_INFO("", VR_GRAPHIC_DBG_RUN, "draw(%d) Cvt2Yuv, (%d,%d) %dx%d\n", i, x, y, width, height );
	}	
}

void  nxGSurfaceRunCvt2Rgba( HSURFTARGET hTarget, HSURFSOURCE hSource)
{
	Statics* pStatics = vrGetStatics();
	Shader* pshader = &(vrGetStatics()->shader[VR_PROGRAM_CVT2RGBA]);	
	const float aSquareVertex[] =
	{
		-1.0f,	-1.0f, 0.0f,
		-1.0f, 1.0f, 0.0f,
		 1.0f, 1.0f, 0.0f,
		 1.0f,	-1.0f, 0.0f,
	};	
	float aSquareTexCoord[VR_INPUT_MODE_YUV_MAX][8] =
	{
		{
			0.0f, 1.0f,
			0.0f, 0.0f,
			1.0f, 0.0f,
			1.0f, 1.0f
		},
		{
			0.0f, 1.0f,
			0.0f, 0.0f,
			1.0f, 0.0f,
			1.0f, 1.0f
		},
		{
			0.0f, 1.0f,
			0.0f, 0.0f,
			1.0f, 0.0f,
			1.0f, 1.0f
		}
	};

	VR_INFO("", VR_GRAPHIC_DBG_RUN, "%s start\n", __FUNCTION__);

#if (VR_FEATURE_INPUT_HEIGHT_ALIGN_BYTE > 0)
	float min_y, max_y, tex_height, tex_total_height_pitch; 
	float y_align_blank_height = 0.f, u_align_blank_height = 0.f;
	{
		unsigned int y_height, u_height;
		//add Y align blank offset
		y_height = hSource->height;
		if(hSource->height % VR_FEATURE_INPUT_HEIGHT_ALIGN_BYTE)
		{				
			y_align_blank_height = (float)(VR_FEATURE_INPUT_HEIGHT_ALIGN_BYTE - (y_height % VR_FEATURE_INPUT_HEIGHT_ALIGN_BYTE)) * 2;
		}
		//add U align blank offset
		u_height = hSource->height/2;
		if(u_height % VR_FEATURE_INPUT_HEIGHT_ALIGN_BYTE)
		{	
			u_align_blank_height = (float)(VR_FEATURE_INPUT_HEIGHT_ALIGN_BYTE - (u_height % VR_FEATURE_INPUT_HEIGHT_ALIGN_BYTE));
		}
		VR_INFO("", VR_GRAPHIC_DBG_HEIGHT_ALIGN, "y_align_blank_height(%f), u_align_blank_height(%f)\n", y_align_blank_height, u_align_blank_height);		
	}
	tex_height = (float)hSource->height * 2.f;
	//set V coord
	tex_total_height_pitch = tex_height + tex_height/2 + y_align_blank_height + u_align_blank_height;
	min_y = tex_height + y_align_blank_height + tex_height/4.f + u_align_blank_height;
	VR_INFO("", VR_GRAPHIC_DBG_HEIGHT_ALIGN, "V, min(%f),", min_y);		
	//normalize
	min_y /= tex_total_height_pitch;
	VR_INFO("", VR_GRAPHIC_DBG_HEIGHT_ALIGN, " min nomalize(%f of %f)\n", min_y, tex_total_height_pitch);		
	aSquareTexCoord[VR_INPUT_MODE_V][3] = aSquareTexCoord[VR_INPUT_MODE_V][5] = min_y;
	//aSquareTexCoord[VR_INPUT_MODE_V][1] = aSquareTexCoord[VR_INPUT_MODE_V][7] = 1.f;

	//set U coord
	tex_total_height_pitch = tex_height + y_align_blank_height + tex_height/4;
	min_y = tex_height + y_align_blank_height;
	VR_INFO("", VR_GRAPHIC_DBG_HEIGHT_ALIGN, "U, min(%f),", min_y);		
	//normalize
	min_y /= tex_total_height_pitch;
	VR_INFO("", VR_GRAPHIC_DBG_HEIGHT_ALIGN, " min nomalize(%f)\n", min_y);		
	aSquareTexCoord[VR_INPUT_MODE_U][3] = aSquareTexCoord[VR_INPUT_MODE_U][5] = min_y;
	max_y = tex_height + y_align_blank_height + tex_height/4.f;
	VR_INFO("", VR_GRAPHIC_DBG_HEIGHT_ALIGN, "U, max(%f),", max_y);		
	//normalize
	max_y /= tex_total_height_pitch;
	VR_INFO("", VR_GRAPHIC_DBG_HEIGHT_ALIGN, " max nomalize(%f)\n", max_y);		
	aSquareTexCoord[VR_INPUT_MODE_U][1] = aSquareTexCoord[VR_INPUT_MODE_U][7] = max_y;

#else

	//set U coord
	//min y
	aSquareTexCoord[VR_INPUT_MODE_U][3] = aSquareTexCoord[VR_INPUT_MODE_U][5] = 4.f/5.f;
	//max y
	//aSquareTexCoord[VR_INPUT_MODE_U][1] = aSquareTexCoord[VR_INPUT_MODE_U][7] = 1.f;
	//set V coord
	//min y
	aSquareTexCoord[VR_INPUT_MODE_V][3] = aSquareTexCoord[VR_INPUT_MODE_V][5] = 5.f/6.f;
	//max y
	//aSquareTexCoord[VR_INPUT_MODE_V][1] = aSquareTexCoord[VR_INPUT_MODE_V][7] = 1.f;	
#endif

	#ifdef VR_FEATURE_STRIDE_USE
	if(hSource->stride != hSource->width)
	{
		float max_x = (float)hSource->width/(float)hSource->stride;
		aSquareTexCoord[VR_INPUT_MODE_Y][4] = aSquareTexCoord[VR_INPUT_MODE_Y][6] = max_x;
		aSquareTexCoord[VR_INPUT_MODE_U][4] = aSquareTexCoord[VR_INPUT_MODE_U][6] = max_x;
		aSquareTexCoord[VR_INPUT_MODE_V][4] = aSquareTexCoord[VR_INPUT_MODE_V][6] = max_x;
	}
	#endif

	if( NULL == pStatics || NULL == hTarget || NULL == hSource )
	{
		NxErrMsg("Error: NULL output surface at %s:%i\n", __FILE__, __LINE__);
		return;
	}
	_AUTO_BACKUP_CURRENT_EGL_;

	/* Make context current. */
	EGLBoolean bResult = eglMakeCurrent(pStatics->egl_info.sEGLDisplay, hTarget->target_pixmap_surfaces[0], 
							hTarget->target_pixmap_surfaces[0], pStatics->egl_info.sEGLContext[VR_PROGRAM_CVT2RGBA]);
	if(bResult == EGL_FALSE)
	{
		EGLint iError = eglGetError();
		NxErrMsg("eglGetError(): %i (0x%.4x)\n", (int)iError, (int)iError);
		NxErrMsg("Error: Failed to make context current at %s:%i\n", __FILE__, __LINE__);
		return;	
	}
	GL_CHECK(glUseProgram(pshader->iProgName));
	int x=0, y=0, width, height;
	width  = hTarget->width;
	height = hTarget->height;
	GL_CHECK(glViewport(x,y,width,height));
	GL_CHECK(glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT)); // To optimize for tile-based renderer
	GL_CHECK(glVertexAttribPointer(pshader->iLocPosition, 3, GL_FLOAT, GL_FALSE, 0, aSquareVertex));
	GL_CHECK(glEnableVertexAttribArray(pshader->iLocPosition));
	GL_CHECK(glVertexAttribPointer(pshader->iLocTexCoord[VR_INPUT_MODE_Y], 2, GL_FLOAT, GL_FALSE, 0, aSquareTexCoord[VR_INPUT_MODE_Y]));
	GL_CHECK(glVertexAttribPointer(pshader->iLocTexCoord[VR_INPUT_MODE_U], 2, GL_FLOAT, GL_FALSE, 0, aSquareTexCoord[VR_INPUT_MODE_U]));
	GL_CHECK(glVertexAttribPointer(pshader->iLocTexCoord[VR_INPUT_MODE_V], 2, GL_FLOAT, GL_FALSE, 0, aSquareTexCoord[VR_INPUT_MODE_V]));
	GL_CHECK(glEnableVertexAttribArray(pshader->iLocTexCoord[VR_INPUT_MODE_Y]));
	GL_CHECK(glEnableVertexAttribArray(pshader->iLocTexCoord[VR_INPUT_MODE_U]));
	GL_CHECK(glEnableVertexAttribArray(pshader->iLocTexCoord[VR_INPUT_MODE_V]));

    //GL_CHECK(glUniform1f(pStatics->shader[program].iLocOutputHeight, output_height));
	GL_CHECK(glUniform1i(pshader->iLocMainTex[VR_INPUT_MODE_Y], VR_INPUT_MODE_Y));
	GL_CHECK(glUniform1i(pshader->iLocMainTex[VR_INPUT_MODE_U], VR_INPUT_MODE_U));
	GL_CHECK(glUniform1i(pshader->iLocMainTex[VR_INPUT_MODE_V], VR_INPUT_MODE_V));

	GL_CHECK(glActiveTexture(GL_TEXTURE0 + VR_INPUT_MODE_Y));
	GL_CHECK(glBindTexture(GL_TEXTURE_2D, hSource->texture_name[VR_INPUT_MODE_Y]));
	GL_CHECK(glActiveTexture(GL_TEXTURE0 + VR_INPUT_MODE_U));
	GL_CHECK(glBindTexture(GL_TEXTURE_2D, hSource->texture_name[VR_INPUT_MODE_U]));
	GL_CHECK(glActiveTexture(GL_TEXTURE0 + VR_INPUT_MODE_V));
	GL_CHECK(glBindTexture(GL_TEXTURE_2D, hSource->texture_name[VR_INPUT_MODE_V]));

	GL_CHECK(glDrawArrays(GL_TRIANGLE_FAN, 0, 4));
	vrWaitForDone();
	VR_INFO("", VR_GRAPHIC_DBG_RUN, "draw Cvt2Rgba, (%d,%d) %dx%d\n", x, y, width, height );
}

void  nxGSurfaceRunUserFilter( HSURFTARGET hTarget, HSURFSOURCE hSource)
{
	Statics* pStatics = vrGetStatics();
	const float aSquareVertex[] =
	{
		-1.0f,	-1.0f, 0.0f,
		-1.0f, 1.0f, 0.0f,
		 1.0f, 1.0f, 0.0f,
		 1.0f,	-1.0f, 0.0f,
	};	
	float aSquareTexCoord[] =
	{
		0.0f, 1.0f,
		0.0f, 0.0f,
		1.0f, 0.0f,
		1.0f, 1.0f
	};
	
	if( NULL == pStatics || NULL == hTarget || NULL == hSource )
	{
		NxErrMsg("Error:	hTarget(0x%x), hSource(0x%x) at %s:%i\n", hTarget, hSource, __FILE__, __LINE__);
		return;
	}
	VR_INFO("", VR_GRAPHIC_DBG_RUN, "%s start\n", __FUNCTION__);

	#ifdef VR_FEATURE_STRIDE_USE
	if(hSource->stride != hSource->width)
	{
		float max_x = (float)hSource->width/(float)hSource->stride;
		aSquareTexCoord[4] = aSquareTexCoord[6] = max_x;
	}
	#endif
	
	_AUTO_BACKUP_CURRENT_EGL_;

	Shader* pshader = &(vrGetStatics()->shader[VR_PROGRAM_USERFILTER]);	
	VR_ASSERT("Error: target_pixmap_surfaces must exist", hTarget->target_pixmap_surfaces[0]);						

	/* Make context current. */
	EGLBoolean bResult = eglMakeCurrent(pStatics->egl_info.sEGLDisplay, 
									hTarget->target_pixmap_surfaces[0], 
									hTarget->target_pixmap_surfaces[0], 
									pStatics->egl_info.sEGLContext[VR_PROGRAM_USERFILTER]);
	if(bResult == EGL_FALSE)
	{
		EGLint iError = eglGetError();
		NxErrMsg("eglGetError(): %i (0x%.4x)\n", (int)iError, (int)iError);
		NxErrMsg("Error: Failed to make context current at %s:%i\n", __FILE__, __LINE__);
		return; 
	}

	GL_CHECK(glUseProgram(pshader->iProgName)); 	
	int x=0, y=0, width, height;
	width  = hTarget->width;
	height = hTarget->height;
	GL_CHECK(glViewport(x,y,width,height));
	GL_CHECK(glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT)); // To optimize for tile-based renderer
	GL_CHECK(glVertexAttribPointer(pshader->iLocPosition, 3, GL_FLOAT, GL_FALSE, 0, aSquareVertex));
	GL_CHECK(glVertexAttribPointer(pshader->iLocTexCoord[0], 2, GL_FLOAT, GL_FALSE, 0, aSquareTexCoord));

	GL_CHECK(glEnableVertexAttribArray(pshader->iLocPosition));
	GL_CHECK(glEnableVertexAttribArray(pshader->iLocTexCoord[0]));

    GL_CHECK(glUniform1f(pshader->iLocInputWidth, hSource->width));
    GL_CHECK(glUniform1f(pshader->iLocInputHeight, hSource->height));
	GL_CHECK(glUniform1i(pshader->iLocMainTex[0], VR_INPUT_MODE_USERFILTER));

	GL_CHECK(glActiveTexture(GL_TEXTURE0 + VR_INPUT_MODE_USERFILTER));
	GL_CHECK(glBindTexture(GL_TEXTURE_2D, hSource->texture_name[0]));

	GL_CHECK(glDrawArrays(GL_TRIANGLE_FAN, 0, 4));		

	vrWaitForDone();
	VR_INFO("", VR_GRAPHIC_DBG_RUN, "draw UserFilter, (%d,%d) %dx%d\n", x, y, width, height );
}

void nxGSurfaceRunYuvFilter( HSURFTARGET hTarget, HSURFSOURCE hSource, float edgeParam )
{
	Statics* pStatics = vrGetStatics();
	const float aSquareVertex[] =
	{
		-1.0f,	-1.0f, 0.0f,
		-1.0f, 1.0f, 0.0f,
		 1.0f, 1.0f, 0.0f,
		 1.0f,	-1.0f, 0.0f,
	};	

#if 0
	float aSquareTexCoord_degree0[VR_INPUT_MODE_YUV_MAX][8] = { {TPOS1, TPOS2, TPOS3, TPOS4}, {TPOS1, TPOS2, TPOS3, TPOS4}, {TPOS1, TPOS2, TPOS3, TPOS4} };
	float aSquareTexCoord_degree90[VR_INPUT_MODE_YUV_MAX][8] = { {TPOS2, TPOS3, TPOS4, TPOS1}, {TPOS2, TPOS3, TPOS4, TPOS1}, {TPOS2, TPOS3, TPOS4, TPOS1} };
	float aSquareTexCoord_degree180[VR_INPUT_MODE_YUV_MAX][8] = { {TPOS3, TPOS4, TPOS1, TPOS2}, {TPOS3, TPOS4, TPOS1, TPOS2}, {TPOS3, TPOS4, TPOS1, TPOS2} };
	float aSquareTexCoord_degree270[VR_INPUT_MODE_YUV_MAX][8] = { {TPOS4, TPOS1, TPOS2, TPOS3}, {TPOS4, TPOS1, TPOS2, TPOS3}, {TPOS4, TPOS1, TPOS2, TPOS3} };
	float aSquareTexCoord_mirror[VR_INPUT_MODE_YUV_MAX][8] = { {TPOS4, TPOS3, TPOS2, TPOS1}, {TPOS4, TPOS3, TPOS2, TPOS1}, {TPOS4, TPOS3, TPOS2, TPOS1} };
	float aSquareTexCoord_vflip[VR_INPUT_MODE_YUV_MAX][8] = { {TPOS2, TPOS1, TPOS4, TPOS3}, {TPOS2, TPOS1, TPOS4, TPOS3}, {TPOS2, TPOS1, TPOS4, TPOS3} };
	float aSquareTexCoord[VR_INPUT_MODE_YUV_MAX][8];
	//temp test
	int mode = 2;
	printf("\nyuv_filter mode ===> %d\n", mode);
	
	switch(mode)
	{
		case 0 : MEMCPY_FUNC(aSquareTexCoord, aSquareTexCoord_degree0, sizeof(aSquareTexCoord)); break;
		case 1 : MEMCPY_FUNC(aSquareTexCoord, aSquareTexCoord_degree90, sizeof(aSquareTexCoord)); break;
		case 2 : MEMCPY_FUNC(aSquareTexCoord, aSquareTexCoord_degree180, sizeof(aSquareTexCoord)); break;
		case 3 : MEMCPY_FUNC(aSquareTexCoord, aSquareTexCoord_degree270, sizeof(aSquareTexCoord)); break;
		case 4 : MEMCPY_FUNC(aSquareTexCoord, aSquareTexCoord_mirror, sizeof(aSquareTexCoord)); break;
		case 5 : MEMCPY_FUNC(aSquareTexCoord, aSquareTexCoord_vflip, sizeof(aSquareTexCoord)); break;
		default : MEMCPY_FUNC(aSquareTexCoord, aSquareTexCoord_degree0, sizeof(aSquareTexCoord));
	}
#else	
	float aSquareTexCoord[VR_INPUT_MODE_YUV_MAX][8] =
	{
		{
			0.0f, 1.0f,
			0.0f, 0.0f,
			1.0f, 0.0f,
			1.0f, 1.0f
		},
		{
			0.0f, 1.0f,
			0.0f, 0.0f,
			1.0f, 0.0f,
			1.0f, 1.0f
		},
		{
			0.0f, 1.0f,
			0.0f, 0.0f,
			1.0f, 0.0f,
			1.0f, 1.0f
		}
	};
#endif	
	VR_INFO("", VR_GRAPHIC_DBG_RUN, "%s start\n", __FUNCTION__);
	
#if (VR_FEATURE_INPUT_HEIGHT_ALIGN_BYTE > 0)
	float min_y, max_y, tex_height, tex_total_height_pitch; 
	float y_align_blank_height = 0.f, u_align_blank_height = 0.f;
	{
		unsigned int y_height, u_height;
		//add Y align blank offset
		y_height = hSource->height;
		if(hSource->height % VR_FEATURE_INPUT_HEIGHT_ALIGN_BYTE)
		{				
			y_align_blank_height = (float)(VR_FEATURE_INPUT_HEIGHT_ALIGN_BYTE - (y_height % VR_FEATURE_INPUT_HEIGHT_ALIGN_BYTE)) * 2;
		}
		//add U align blank offset
		u_height = hSource->height/2;
		if(u_height % VR_FEATURE_INPUT_HEIGHT_ALIGN_BYTE)
		{	
			u_align_blank_height = (float)(VR_FEATURE_INPUT_HEIGHT_ALIGN_BYTE - (u_height % VR_FEATURE_INPUT_HEIGHT_ALIGN_BYTE));
		}
		VR_INFO("", VR_GRAPHIC_DBG_HEIGHT_ALIGN, "y_align_blank_height(%f), u_align_blank_height(%f)\n", y_align_blank_height, u_align_blank_height);		
	}
	tex_height = (float)hSource->height * 2.f;
	//set V coord
	tex_total_height_pitch = tex_height + tex_height/2 + y_align_blank_height + u_align_blank_height;
	min_y = tex_height + y_align_blank_height + tex_height/4.f + u_align_blank_height;
	VR_INFO("", VR_GRAPHIC_DBG_HEIGHT_ALIGN, "V, min(%f),", min_y);		
	//normalize
	min_y /= tex_total_height_pitch;
	VR_INFO("", VR_GRAPHIC_DBG_HEIGHT_ALIGN, " min nomalize(%f of %f)\n", min_y, tex_total_height_pitch);		
	aSquareTexCoord[0][3] = aSquareTexCoord[0][5] = min_y;
	//aSquareTexCoord[0][1] = aSquareTexCoord[0][7] = 1.f;

	//set U coord
	tex_total_height_pitch = tex_height + y_align_blank_height + tex_height/4;
	min_y = tex_height + y_align_blank_height;
	VR_INFO("", VR_GRAPHIC_DBG_HEIGHT_ALIGN, "U, min(%f),", min_y);		
	//normalize
	min_y /= tex_total_height_pitch;
	VR_INFO("", VR_GRAPHIC_DBG_HEIGHT_ALIGN, " min nomalize(%f)\n", min_y);		
	aSquareTexCoord[1][3] = aSquareTexCoord[1][5] = min_y;
	max_y = tex_height + y_align_blank_height + tex_height/4.f;
	VR_INFO("", VR_GRAPHIC_DBG_HEIGHT_ALIGN, "U, max(%f),", max_y);		
	//normalize
	max_y /= tex_total_height_pitch;
	VR_INFO("", VR_GRAPHIC_DBG_HEIGHT_ALIGN, " max nomalize(%f)\n", max_y);		
	aSquareTexCoord[1][1] = aSquareTexCoord[1][7] = max_y;

#else

	//set V coord
	//min y
	aSquareTexCoord[0][3] = aSquareTexCoord[0][5] = 5.f/6.f;
	//max y
	//aSquareTexCoord[0][1] = aSquareTexCoord[0][7] = 1.f;
	//set U coord
	//min y
	aSquareTexCoord[1][3] = aSquareTexCoord[1][5] = 4.f/5.f;
	//max y
	//aSquareTexCoord[1][1] = aSquareTexCoord[1][7] = 1.f;	
	VR_INFO("", VR_GRAPHIC_DBG_HEIGHT_ALIGN, "U(%f), V(%f)\n", 4.f/6.f, 5.f/6.f);		
#endif	

	#ifdef VR_FEATURE_STRIDE_USE
	if(hSource->stride != hSource->width)
	{
		float max_x = (float)hSource->width/(float)hSource->stride;
		aSquareTexCoord[2][4] = aSquareTexCoord[2][6] = max_x;
		aSquareTexCoord[1][4] = aSquareTexCoord[1][6] = max_x;
		aSquareTexCoord[0][4] = aSquareTexCoord[0][6] = max_x;
	}
	#endif	
	
	if( NULL == pStatics || NULL == hTarget || NULL == hSource )
	{
		NxErrMsg("Error:  hTarget(0x%x), hSource(0x%x) at %s:%i\n", hTarget, hSource, __FILE__, __LINE__);
		return;
	}
	_AUTO_BACKUP_CURRENT_EGL_;

	int program;
	#if 1 //org
	for(int i = 0 ; i < VR_YUV_CNT ; i++)
	#else
	int i = 2;
	#endif
	{			
		#if 1
		if (2 == i)
			program = VR_PROGRAM_YUVFILTER_Y;
		else
			program = VR_PROGRAM_YUVFILTER_UV;
		#else
		program = VR_PROGRAM_YUVFILTER_Y;
		#endif
		Shader* pshader = &(vrGetStatics()->shader[program]);	
		
		if(!hTarget->target_pixmap_surfaces[i])
		{
			VR_ASSERT("Error: Y must exist", i != 0);				
			//continue;
		}			

		/* Make context current. */
		EGLBoolean bResult = eglMakeCurrent(pStatics->egl_info.sEGLDisplay, 
										hTarget->target_pixmap_surfaces[i], 
										hTarget->target_pixmap_surfaces[i], 
										pStatics->egl_info.sEGLContext[VR_PROGRAM_YUVFILTER_Y]);
		if(bResult == EGL_FALSE)
		{
			EGLint iError = eglGetError();
			NxErrMsg("eglGetError(): %i (0x%.4x)\n", (int)iError, (int)iError);
			NxErrMsg("Error: Failed to make context current at %s:%i\n", __FILE__, __LINE__);
			return; 
		}

		GL_CHECK(glUseProgram(pshader->iProgName));		
		int x=0, y=0, width, height;
		if(2 == i)
		{
			//Y case
			width  = hTarget->width;
			height = hTarget->height;
		}
		else if(1 == i)
		{
			//U case
			width  = (hTarget->width)/2;
			height = (hTarget->height)/2;
		}
		else
		{
			//V case
			width  = (hTarget->width)/2;
			height = (hTarget->height)/2;
		}		
		GL_CHECK(glViewport(x,y,width,height));		
		GL_CHECK(glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT)); // To optimize for tile-based renderer
		GL_CHECK(glVertexAttribPointer(pshader->iLocPosition, 3, GL_FLOAT, GL_FALSE, 0, aSquareVertex));
		GL_CHECK(glVertexAttribPointer(pshader->iLocTexCoord[0], 2, GL_FLOAT, GL_FALSE, 0, aSquareTexCoord[i]));
		GL_CHECK(glEnableVertexAttribArray(pshader->iLocPosition));
		GL_CHECK(glEnableVertexAttribArray(pshader->iLocTexCoord[0]));
		GL_CHECK(glUniform1i(pshader->iLocMainTex[0], VR_INPUT_MODE_TEXTURE0));
		if (VR_PROGRAM_YUVFILTER_Y == program)
		{
			GL_CHECK(glUniform2f(pshader->iLocTexInvSize, 1.f/width, 1.f/height));
			GL_CHECK(glUniform1f(pshader->iLocFilterGain, edgeParam/* gain */));
		}
		GL_CHECK(glActiveTexture(GL_TEXTURE0 + VR_INPUT_MODE_TEXTURE0));
		GL_CHECK(glBindTexture(GL_TEXTURE_2D, hSource->texture_name[i]));

		GL_CHECK(glDrawArrays(GL_TRIANGLE_FAN, 0, 4));		

		vrWaitForDone();
		VR_INFO("", VR_GRAPHIC_DBG_RUN, "draw(%d) yuv_filter, (%d,%d) %dx%d\n", i, x, y, width, height );
	}	
}
#endif

void  nxGSurfaceRunScaleRGBA( HSURFTARGET hTarget, HSURFSOURCE hSource)
{
	Statics* pStatics = vrGetStatics();
	const float aSquareVertex[] =
	{
		-1.0f,	-1.0f, 0.0f,
		-1.0f, 1.0f, 0.0f,
		 1.0f, 1.0f, 0.0f,
		 1.0f,	-1.0f, 0.0f,
	};	
	
	#if 1
	float aSquareTexCoord_degree0[] = { TPOS1, TPOS2, TPOS3, TPOS4 };
	float aSquareTexCoord_degree90[] = { TPOS2, TPOS3, TPOS4, TPOS1 };
	float aSquareTexCoord_degree180[] = { TPOS3, TPOS4, TPOS1, TPOS2 };
	float aSquareTexCoord_degree270[] = { TPOS4, TPOS1, TPOS2, TPOS3 };
	float aSquareTexCoord_mirror[] = { TPOS4, TPOS3, TPOS2, TPOS1 };
	float aSquareTexCoord_vflip[] = { TPOS2, TPOS1, TPOS4, TPOS3 };
	float* aSquareTexCoord;
	//temp test
	int mode = VR_ROTATE_0_DEGREE;
	printf("\nscale mode ===> %d\n", mode);
	
	switch(mode)
	{
		case 0 : aSquareTexCoord = aSquareTexCoord_degree0; break;
		case 1 : aSquareTexCoord = aSquareTexCoord_degree90; break;
		case 2 : aSquareTexCoord = aSquareTexCoord_degree180; break;
		case 3 : aSquareTexCoord = aSquareTexCoord_degree270; break;
		case 4 : aSquareTexCoord = aSquareTexCoord_mirror; break;
		case 5 : aSquareTexCoord = aSquareTexCoord_vflip; break;
		default : aSquareTexCoord = aSquareTexCoord_degree0;
	}
	#else
	float aSquareTexCoord[] =
	{
		0.0f, 1.0f,
		0.0f, 0.0f,
		1.0f, 0.0f,
		1.0f, 1.0f
	};
	#endif
	
	if( NULL == pStatics || NULL == hTarget || NULL == hSource )
	{
		NxErrMsg("Error:	hTarget(0x%x), hSource(0x%x) at %s:%i\n", hTarget, hSource, __FILE__, __LINE__);
		return;
	}
	VR_INFO("", VR_GRAPHIC_DBG_RUN, "%s start\n", __FUNCTION__);

#ifdef VR_FEATURE_STRIDE_USE
	if(hSource->stride != hSource->width)
	{
		float max_x = (float)hSource->width/(float)hSource->stride;
		aSquareTexCoord[4] = aSquareTexCoord[6] = max_x;
	}
#endif
	
	_AUTO_BACKUP_CURRENT_EGL_;

	Shader* pshader = &(vrGetStatics()->shader[VR_PROGRAM_SCALE_RGBA]); 

	/* Make context current. */
	#ifdef VR_FEATURE_SEPERATE_FD_USE
	VR_ASSERT("Error: target_pixmap_surfaces must exist", hTarget->target_pixmap_surface);						
	EGLBoolean bResult = eglMakeCurrent(pStatics->egl_info.sEGLDisplay, 
									hTarget->target_pixmap_surface, 
									hTarget->target_pixmap_surface, 
									pStatics->egl_info.sEGLContext[VR_PROGRAM_SCALE_RGBA]);
	#else
	VR_ASSERT("Error: target_pixmap_surfaces must exist", hTarget->target_pixmap_surfaces[0]);						
	EGLBoolean bResult = eglMakeCurrent(pStatics->egl_info.sEGLDisplay, 
									hTarget->target_pixmap_surfaces[0], 
									hTarget->target_pixmap_surfaces[0], 
									pStatics->egl_info.sEGLContext[VR_PROGRAM_SCALE_RGBA]);
	#endif								
	if(bResult == EGL_FALSE)
	{
		EGLint iError = eglGetError();
		NxErrMsg("eglGetError(): %i (0x%.4x)\n", (int)iError, (int)iError);
		NxErrMsg("Error: Failed to make context current at %s:%i\n", __FILE__, __LINE__);
		return; 
	}

	GL_CHECK(glUseProgram(pshader->iProgName)); 	
	int x=0, y=0, width, height;
	width  = hTarget->width;
	height = hTarget->height;
	GL_CHECK(glViewport(x,y,width,height));
	GL_CHECK(glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT)); // To optimize for tile-based renderer
	GL_CHECK(glVertexAttribPointer(pshader->iLocPosition, 3, GL_FLOAT, GL_FALSE, 0, aSquareVertex));
	GL_CHECK(glVertexAttribPointer(pshader->iLocTexCoord[0], 2, GL_FLOAT, GL_FALSE, 0, aSquareTexCoord));

	GL_CHECK(glEnableVertexAttribArray(pshader->iLocPosition));
	GL_CHECK(glEnableVertexAttribArray(pshader->iLocTexCoord[0]));

	#if 0
	GL_CHECK(glUniform1f(pshader->iLocInputWidth, hSource->width));
	GL_CHECK(glUniform1f(pshader->iLocInputHeight, hSource->height));
	#endif
	GL_CHECK(glUniform1i(pshader->iLocMainTex[0], VR_INPUT_MODE_SCALE_RGBA));

	GL_CHECK(glActiveTexture(GL_TEXTURE0 + VR_INPUT_MODE_SCALE_RGBA));
	GL_CHECK(glBindTexture(GL_TEXTURE_2D, hSource->texture_name[0]));

	GL_CHECK(glDrawArrays(GL_TRIANGLE_FAN, 0, 4));		

	vrWaitForDone();
	VR_INFO("", VR_GRAPHIC_DBG_RUN, "draw ScaleRGBA, (%d,%d) %dx%d\n", x, y, width, height );
}

static void  vrWaitForDone( void )
{
	EGL_CHECK(eglWaitGL());
}

#ifdef VR_FEATURE_SHADER_FILE_USE
/* loadShader():	Load the shader hSource into memory.
 *
 * sFilename: String holding filename to load.
 */
static char *loadShader(const char *sFilename)
{
	char *pResult = NULL;
	FILE *pFile = NULL;
	long iLen = 0;

	pFile = fopen(sFilename, "r");
	if(pFile == NULL) {
		NxErrMsg("Error: Cannot read file '%s'\n", sFilename);
		return NULL;
	}
	fseek(pFile, 0, SEEK_END); /* Seek end of file. */
	iLen = ftell(pFile);
	fseek(pFile, 0, SEEK_SET); /* Seek start of file again. */
	pResult = (char*)NX_CALLOC(iLen+1, sizeof(char));
	if(pResult == NULL)
	{
		NxErrMsg("Error: Out of memory at %s:%i\n", __FILE__, __LINE__);
		return NULL;
	}
	fread(pResult, sizeof(char), iLen, pFile);
	pResult[iLen] = '\0';
	fclose(pFile);

	return pResult;
}

/* processShader(): Create shader, load in hSource, compile, dump debug as necessary.
 *
 * pShader: Pointer to return created shader ID.
 * sFilename: Passed-in filename from which to load shader hSource.
 * iShaderType: Passed to GL, e.g. GL_VERTEX_SHADER.
 */
static int processShader(GLuint *pShader, const char *sFilename, GLint iShaderType)
{
	GLint iStatus;
	const char *aStrings[1] = { NULL };

	/* Create shader and load into GL. */
	*pShader = GL_CHECK(glCreateShader(iShaderType));
	aStrings[0] = loadShader(sFilename);
	if(aStrings[0] == NULL)
	{
		NxErrMsg("Error: wrong shader code %s:%i\n", __FILE__, __LINE__);
		return -1;
	}
	GL_CHECK(glShaderSource(*pShader, 1, aStrings, NULL));

	/* Clean up shader hSource. */
	NX_FREE((void *)(aStrings[0]));
	aStrings[0] = NULL;

	/* Try compiling the shader. */
	GL_CHECK(glCompileShader(*pShader));
	GL_CHECK(glGetShaderiv(*pShader, GL_COMPILE_STATUS, &iStatus));

	/* Dump debug info (hSource and log) if compilation failed. */
	if(iStatus != GL_TRUE) {
		GLint iLen;
		char *sDebugSource = NULL;
		char *sErrorLog = NULL;

		/* Get shader hSource. */
		GL_CHECK(glGetShaderiv(*pShader, GL_SHADER_SOURCE_LENGTH, &iLen));
		sDebugSource = (char*)NX_MALLOC(iLen);
		GL_CHECK(glGetShaderSource(*pShader, iLen, NULL, sDebugSource));
		NxDbgMsg("Debug hSource START:\n%s\nDebug hSource END\n\n", sDebugSource);
		NX_FREE(sDebugSource);

		/* Now get the info log. */
		GL_CHECK(glGetShaderiv(*pShader, GL_INFO_LOG_LENGTH, &iLen));
		sErrorLog = (char*)NX_MALLOC(iLen);
		GL_CHECK(glGetShaderInfoLog(*pShader, iLen, NULL, sErrorLog));
		NxDbgMsg("Log START:\n%s\nLog END\n\n", sErrorLog);
		NX_FREE(sErrorLog);

		NxDbgMsg("Compilation FAILED!\n\n");
		return -1;
	}
	return 0;
}
#else
/* processShader(): Create shader, load in hSource, compile, dump debug as necessary.
 *
 * pShader: Pointer to return created shader ID.
 * sFilename: Passed-in filename from which to load shader hSource.
 * iShaderType: Passed to GL, e.g. GL_VERTEX_SHADER.
 */
static int processShader(GLuint *pShader, const char *pString, GLint iShaderType)
{
	GLint iStatus;
	const char *aStrings[1] = { NULL };

	if(pString == NULL)
	{
		NxErrMsg("Error: wrong shader code %s:%i\n", __FILE__, __LINE__);
		return -1;
	}

	/* Create shader and load into GL. */
	*pShader = GL_CHECK(glCreateShader(iShaderType));
	if(pShader == NULL)
	{
		NxErrMsg("Error: wrong shader code %s:%i\n", __FILE__, __LINE__);
		return -1;
	}
	aStrings[0] = pString;
	GL_CHECK(glShaderSource(*pShader, 1, aStrings, NULL));

	/* Clean up shader hSource. */
	aStrings[0] = NULL;

	/* Try compiling the shader. */
	GL_CHECK(glCompileShader(*pShader));
	GL_CHECK(glGetShaderiv(*pShader, GL_COMPILE_STATUS, &iStatus));

	/* Dump debug info (hSource and log) if compilation failed. */
	if(iStatus != GL_TRUE) {
		GLint iLen;
		char *sDebugSource = NULL;
		char *sErrorLog = NULL;

		/* Get shader hSource. */
		GL_CHECK(glGetShaderiv(*pShader, GL_SHADER_SOURCE_LENGTH, &iLen));
		sDebugSource = (char*)NX_MALLOC(iLen);
		GL_CHECK(glGetShaderSource(*pShader, iLen, NULL, sDebugSource));
		NxDbgMsg("Debug hSource START:\n%s\nDebug hSource END\n\n", sDebugSource);
		NX_FREE(sDebugSource);

		/* Now get the info log. */
		GL_CHECK(glGetShaderiv(*pShader, GL_INFO_LOG_LENGTH, &iLen));
		sErrorLog = (char*)NX_MALLOC(iLen);
		GL_CHECK(glGetShaderInfoLog(*pShader, iLen, NULL, sErrorLog));
		NxDbgMsg("Log START:\n%s\nLog END\n\n", sErrorLog);
		NX_FREE(sErrorLog);

		NxDbgMsg("Compilation FAILED!\n\n");
		return -1;
	}
	return 0;
}
#endif

/* For Deinterlace. */
//For 32bit context
static int vrInitializeDeinterlace( HSURFTARGET hTarget)
{
	VR_ASSERT("hTarget must exist", hTarget);

	Statics* pStatics = vrGetStatics();
	unsigned int program = VR_PROGRAM_DEINTERLACE;
	VR_INFO("", VR_GRAPHIC_DBG_CTX, "vrInitializeDeinterlace start\n");	

	if(nxGSurfaceCreateEGLContext(program) != 0)
	{
		NxErrMsg("Error: Fail to create context %s:%i\n", __FILE__, __LINE__);
		return -1;	
	}
	
	/* Make context current. */
	#ifdef VR_FEATURE_SEPERATE_FD_USE
	EGLBoolean bResult = eglMakeCurrent(pStatics->egl_info.sEGLDisplay, hTarget->target_pixmap_surface, 
							hTarget->target_pixmap_surface, pStatics->egl_info.sEGLContext[program]);
	#else
	EGLBoolean bResult = eglMakeCurrent(pStatics->egl_info.sEGLDisplay, hTarget->target_pixmap_surfaces[0], 
							hTarget->target_pixmap_surfaces[0], pStatics->egl_info.sEGLContext[program]);
	#endif
	if(bResult == EGL_FALSE)
	{
		EGLint iError = eglGetError();
		NxErrMsg("eglGetError(): %i (0x%.4x)\n", (int)iError, (int)iError);
		NxErrMsg("Error: Failed to make context current at %s:%i\n", __FILE__, __LINE__);
		return -1;	
	}	
	
	/* Load shaders. */
	if(processShader(&pStatics->shader[program].iVertName, VERTEX_SHADER_SOURCE_DEINTERLACE, GL_VERTEX_SHADER) < 0)
	{
		NxErrMsg("Error: wrong shader %s:%i\n", __FILE__, __LINE__);
		return -1;
	}
	if(processShader(&pStatics->shader[program].iFragName, FRAGMENT_SHADER_SOURCE_DEINTERLACE, GL_FRAGMENT_SHADER) < 0)
	{
		NxErrMsg("Error: wrong shader %s:%i\n", __FILE__, __LINE__);
		return -1;
	}	

	/* Set up shaders. */
	pStatics->shader[program].iProgName = GL_CHECK(glCreateProgram());
	//NxDbgMsg("Deinterlace iProgName(%d)\n", pStatics->shader[program].iProgName);		
	GL_CHECK(glAttachShader(pStatics->shader[program].iProgName, pStatics->shader[program].iVertName));
	GL_CHECK(glAttachShader(pStatics->shader[program].iProgName, pStatics->shader[program].iFragName));
	GL_CHECK(glLinkProgram(pStatics->shader[program].iProgName));
	GL_CHECK(glUseProgram(pStatics->shader[program].iProgName));
	
	/* Vertex positions. */
	pStatics->shader[program].iLocPosition = GL_CHECK(glGetAttribLocation(pStatics->shader[program].iProgName, "a_v4Position"));
	if(pStatics->shader[program].iLocPosition == -1)
	{
		NxErrMsg("Error: Attribute not found at %s:%i\n", __FILE__, __LINE__);
		return -1;
	}
	//else GL_CHECK(glEnableVertexAttribArray(pStatics->shader[program].iLocPosition));

	/* Fill texture. */
	pStatics->shader[program].iLocTexCoord[0] = GL_CHECK(glGetAttribLocation(pStatics->shader[program].iProgName, "a_v2TexCoord"));
	if(pStatics->shader[program].iLocTexCoord[0] == -1)
	{
		NxErrMsg("Warning: Attribute not found at %s:%i\n", __FILE__, __LINE__);
		return -1;
	}
	//else GL_CHECK(glEnableVertexAttribArray(pStatics->shader[program].iLocTexCoord));

    /* Texture Height. */
    pStatics->shader[program].iLocInputHeight = GL_CHECK(glGetUniformLocation(pStatics->shader[program].iProgName, "u_fTexHeight"));
    if(pStatics->shader[program].iLocInputHeight == -1)
    {
		NxErrMsg("Warning: Uniform not found at %s:%i\n", __FILE__, __LINE__);
		//return -1;
    }

    /* diffuse texture. */
    pStatics->shader[program].iLocMainTex[0] = GL_CHECK(glGetUniformLocation(pStatics->shader[program].iProgName, "diffuse"));
    if(pStatics->shader[program].iLocMainTex[0] == -1)
    {
		NxErrMsg("Warning: Uniform not found at %s:%i\n", __FILE__, __LINE__);
		//return -1;
    }
    else 
    {
        //GL_CHECK(glUniform1i(pStatics->shader[program].iLocMainTex[0], VR_INPUT_MODE_DEINTERLACE));
    }	

	/* ref texture. */
    pStatics->shader[program].iLocRefTex = GL_CHECK(glGetUniformLocation(pStatics->shader[program].iProgName, "ref_tex"));
    if(pStatics->shader[program].iLocRefTex == -1)
    {
		NxErrMsg("Warning: Uniform not found at %s:%i\n", __FILE__, __LINE__);
		//return -1;
    }
    else 
    {
        //GL_CHECK(glUniform1i(pStatics->shader[program].iLocRefTex, VR_INPUT_MODE_DEINTERLACE_REF));
    }
	
	//set texture
	GL_CHECK(glGenTextures(1, &pStatics->tex_deinterlace_ref_id));
	GL_CHECK(glActiveTexture(GL_TEXTURE0 + VR_INPUT_MODE_DEINTERLACE_REF));
	GL_CHECK(glBindTexture(GL_TEXTURE_2D, pStatics->tex_deinterlace_ref_id));
	GL_CHECK(glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST));
	GL_CHECK(glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST));
	GL_CHECK(glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT));
	GL_CHECK(glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT)); 
	{
		unsigned int temp_imgbuf[2] = {0x00000000, 0xFFFFFFFF};
		GL_CHECK(glTexImage2D(GL_TEXTURE_2D,0,GL_RGBA,
				 1,2,0,
				 GL_RGBA,GL_UNSIGNED_BYTE,temp_imgbuf));	
	}	

	VR_INFO("", VR_GRAPHIC_DBG_CTX, "vrInitializeDeinterlace end\n"); 	
	return 0;			
}				

/* For Scaler. */
//For 8bit context
static int vrInitializeScaler( HSURFTARGET hTarget)
{
	VR_ASSERT("hTarget must exist", hTarget);

	Statics* pStatics = vrGetStatics();
	unsigned int program = VR_PROGRAM_SCALE;
	VR_INFO("", VR_GRAPHIC_DBG_CTX, "vrInitialize_scale start\n");		

	if(nxGSurfaceCreateEGLContext(program) != 0)
	{
		NxErrMsg("Error: Fail to create context %s:%i\n", __FILE__, __LINE__);
		return -1;	
	}

	/* Make context current. */
	#ifdef VR_FEATURE_SEPERATE_FD_USE
	EGLBoolean bResult = eglMakeCurrent(pStatics->egl_info.sEGLDisplay, hTarget->target_pixmap_surface, 
							hTarget->target_pixmap_surface, pStatics->egl_info.sEGLContext[program]);
	#else
	EGLBoolean bResult = eglMakeCurrent(pStatics->egl_info.sEGLDisplay, hTarget->target_pixmap_surfaces[0], 
							hTarget->target_pixmap_surfaces[0], pStatics->egl_info.sEGLContext[program]);
	#endif
	if(bResult == EGL_FALSE)
	{
		EGLint iError = eglGetError();
		NxErrMsg("eglGetError(): %i (0x%.4x)\n", (int)iError, (int)iError);
		NxErrMsg("Error: Failed to make context current at %s:%i\n", __FILE__, __LINE__);
		return -1;	
	}
	
	/* Load shaders. */
	if(processShader(&pStatics->shader[program].iVertName, VERTEX_SHADER_SOURCE_SCALE, GL_VERTEX_SHADER) < 0)
	{
		NxErrMsg("Error: wrong shader %s:%i\n", __FILE__, __LINE__);
		return -1;
	}
	if(processShader(&pStatics->shader[program].iFragName, FRAGMENT_SHADER_SOURCE_SCALE, GL_FRAGMENT_SHADER) < 0)
	{
		NxErrMsg("Error: wrong shader %s:%i\n", __FILE__, __LINE__);
		return -1;
	}
	
	/* Set up shaders. */
	pStatics->shader[program].iProgName = GL_CHECK(glCreateProgram());
	//NxDbgMsg("Scaler iProgName(%d)\n", pStatics->shader[program].iProgName);
	GL_CHECK(glAttachShader(pStatics->shader[program].iProgName, pStatics->shader[program].iVertName));
	GL_CHECK(glAttachShader(pStatics->shader[program].iProgName, pStatics->shader[program].iFragName));
	GL_CHECK(glLinkProgram(pStatics->shader[program].iProgName));
	GL_CHECK(glUseProgram(pStatics->shader[program].iProgName));

	/* Vertex positions. */
	pStatics->shader[program].iLocPosition = GL_CHECK(glGetAttribLocation(pStatics->shader[program].iProgName, "a_v4Position"));
	if(pStatics->shader[program].iLocPosition == -1)
	{
		NxErrMsg("Error: Attribute not found at %s:%i\n", __FILE__, __LINE__);
		return -1;
	}
	//else GL_CHECK(glEnableVertexAttribArray(pStatics->shader[program].iLocPosition));

	/* Fill texture. */
	pStatics->shader[program].iLocTexCoord[0] = GL_CHECK(glGetAttribLocation(pStatics->shader[program].iProgName, "a_v2TexCoord"));
	if(pStatics->shader[program].iLocTexCoord[0] == -1)
	{
		NxErrMsg("Warning: Attribute not found at %s:%i\n", __FILE__, __LINE__);
		return -1;
	}
	//else GL_CHECK(glEnableVertexAttribArray(pStatics->shader[program].iLocTexCoord));

	/* diffuse texture. */
	pStatics->shader[program].iLocMainTex[0] = GL_CHECK(glGetUniformLocation(pStatics->shader[program].iProgName, "diffuse"));
	if(pStatics->shader[program].iLocMainTex[0] == -1)
	{
		NxErrMsg("Warning: Uniform not found at %s:%i\n", __FILE__, __LINE__);
		//return -1;
	}
	else 
	{
		//GL_CHECK(glUniform1i(pStatics->shader[program].iLocMainTex[0], VR_INPUT_MODE_TEXTURE0));
	}
	VR_INFO("", VR_GRAPHIC_DBG_CTX, "vrInitialize_scale end\n"); 		
	return 0;			
}

/* For Cvt2Y. */
//For 8bit context		
static int vrInitializeCvt2Y( HSURFTARGET hTarget)
{
	VR_ASSERT("hTarget must exist", hTarget);

	Statics* pStatics = vrGetStatics();
	unsigned int program = VR_PROGRAM_CVT2Y;
	VR_INFO("", VR_GRAPHIC_DBG_CTX, "vrInitializeCvt2Y start\n");			

	if(nxGSurfaceCreateEGLContext(program) != 0)
	{
		NxErrMsg("Error: Fail to create context %s:%i\n", __FILE__, __LINE__);
		return -1;	
	}

	/* Make context current. */
	#ifdef VR_FEATURE_SEPERATE_FD_USE
	EGLBoolean bResult = eglMakeCurrent(pStatics->egl_info.sEGLDisplay, hTarget->target_pixmap_surface, 
								hTarget->target_pixmap_surface, pStatics->egl_info.sEGLContext[program]);
	#else
	EGLBoolean bResult = eglMakeCurrent(pStatics->egl_info.sEGLDisplay, hTarget->target_pixmap_surfaces[0], 
								hTarget->target_pixmap_surfaces[0], pStatics->egl_info.sEGLContext[program]);
	#endif
	if(bResult == EGL_FALSE)
	{
		EGLint iError = eglGetError();
		NxErrMsg("eglGetError(): %i (0x%.4x)\n", (int)iError, (int)iError);
		NxErrMsg("Error: Failed to make context current at %s:%i\n", __FILE__, __LINE__);
		return -1;	
	}
	
	/* Load shaders. */
	if(processShader(&pStatics->shader[program].iVertName, VERTEX_SHADER_SOURCE_CVT2Y, GL_VERTEX_SHADER) < 0)
	{
		NxErrMsg("Error: wrong shader %s:%i\n", __FILE__, __LINE__);
		return -1;
	}
	if(processShader(&pStatics->shader[program].iFragName, FRAGMENT_SHADER_SOURCE_CVT2Y, GL_FRAGMENT_SHADER) < 0)
	{
		NxErrMsg("Error: wrong shader %s:%i\n", __FILE__, __LINE__);
		return -1;
	}
	
	/* Set up shaders. */
	pStatics->shader[program].iProgName = GL_CHECK(glCreateProgram());
	//NxDbgMsg("Scaler iProgName(%d)\n", pStatics->shader[program].iProgName);
	GL_CHECK(glAttachShader(pStatics->shader[program].iProgName, pStatics->shader[program].iVertName));
	GL_CHECK(glAttachShader(pStatics->shader[program].iProgName, pStatics->shader[program].iFragName));
	GL_CHECK(glLinkProgram(pStatics->shader[program].iProgName));
	GL_CHECK(glUseProgram(pStatics->shader[program].iProgName));

	/* Vertex positions. */
	pStatics->shader[program].iLocPosition = GL_CHECK(glGetAttribLocation(pStatics->shader[program].iProgName, "a_v4Position"));
	if(pStatics->shader[program].iLocPosition == -1)
	{
		NxErrMsg("Error: Attribute not found at %s:%i\n", __FILE__, __LINE__);
		return -1;
	}
	//else GL_CHECK(glEnableVertexAttribArray(pStatics->shader[program].iLocPosition));

	/* Fill texture. */
	pStatics->shader[program].iLocTexCoord[0] = GL_CHECK(glGetAttribLocation(pStatics->shader[program].iProgName, "a_v2TexCoord"));
	if(pStatics->shader[program].iLocTexCoord[0] == -1)
	{
		NxErrMsg("Warning: Attribute not found at %s:%i\n", __FILE__, __LINE__);
		return -1;
	}
	//else GL_CHECK(glEnableVertexAttribArray(pStatics->shader[program].iLocTexCoord));

	/* diffuse texture. */
	pStatics->shader[program].iLocMainTex[0] = GL_CHECK(glGetUniformLocation(pStatics->shader[program].iProgName, "diffuse"));
	if(pStatics->shader[program].iLocMainTex[0] == -1)
	{
		NxErrMsg("Warning: Uniform not found at %s:%i\n", __FILE__, __LINE__);
		//return -1;
	}
	else 
	{
		//GL_CHECK(glUniform1i(pStatics->shader[program].iLocMainTex[0], VR_INPUT_MODE_TEXTURE0));
	}
	VR_INFO("", VR_GRAPHIC_DBG_CTX, "vrInitializeCvt2Y end\n"); 			
	return 0;
}			


/* For Cvt2UV. */
//For 16bit context
static int vrInitializeCvt2UV( HSURFTARGET hTarget)
{
	VR_ASSERT("hTarget must exist", hTarget);

	Statics* pStatics = vrGetStatics();
	unsigned int program = VR_PROGRAM_CVT2UV;
	VR_INFO("", VR_GRAPHIC_DBG_CTX, "vrInitializeCvt2UV start\n");	

	if(nxGSurfaceCreateEGLContext(program) != 0)
	{
		NxErrMsg("Error: Fail to create context %s:%i\n", __FILE__, __LINE__);
		return -1;	
	}

	/* Make context current. */
#ifdef VR_FEATURE_SEPERATE_FD_USE
	EGLBoolean bResult = eglMakeCurrent(pStatics->egl_info.sEGLDisplay, hTarget->target_pixmap_surface, 
								hTarget->target_pixmap_surface, pStatics->egl_info.sEGLContext[program]);
#else
	EGLBoolean bResult = eglMakeCurrent(pStatics->egl_info.sEGLDisplay, hTarget->target_pixmap_surfaces[0], 
								hTarget->target_pixmap_surfaces[0], pStatics->egl_info.sEGLContext[program]);
#endif
	if(bResult == EGL_FALSE)
	{
		EGLint iError = eglGetError();
		NxErrMsg("eglGetError(): %i (0x%.4x)\n", (int)iError, (int)iError);
		NxErrMsg("Error: Failed to make context current at %s:%i\n", __FILE__, __LINE__);
		return -1;	
	}
	
	/* Load shaders. */
	if(processShader(&pStatics->shader[program].iVertName, VERTEX_SHADER_SOURCE_CVT2UV, GL_VERTEX_SHADER) < 0)
	{
		NxErrMsg("Error: wrong shader %s:%i\n", __FILE__, __LINE__);
		return -1;
	}
	if(processShader(&pStatics->shader[program].iFragName, FRAGMENT_SHADER_SOURCE_CVT2UV, GL_FRAGMENT_SHADER) < 0)
	{
		NxErrMsg("Error: wrong shader %s:%i\n", __FILE__, __LINE__);
		return -1;
	}
	
	/* Set up shaders. */
	pStatics->shader[program].iProgName = GL_CHECK(glCreateProgram());
	//NxDbgMsg("Scaler iProgName(%d)\n", pStatics->shader[program].iProgName);
	GL_CHECK(glAttachShader(pStatics->shader[program].iProgName, pStatics->shader[program].iVertName));
	GL_CHECK(glAttachShader(pStatics->shader[program].iProgName, pStatics->shader[program].iFragName));
	GL_CHECK(glLinkProgram(pStatics->shader[program].iProgName));
	GL_CHECK(glUseProgram(pStatics->shader[program].iProgName));

	/* Vertex positions. */
	pStatics->shader[program].iLocPosition = GL_CHECK(glGetAttribLocation(pStatics->shader[program].iProgName, "a_v4Position"));
	if(pStatics->shader[program].iLocPosition == -1)
	{
		NxErrMsg("Error: Attribute not found at %s:%i\n", __FILE__, __LINE__);
		return -1;
	}
	//else GL_CHECK(glEnableVertexAttribArray(pStatics->shader[program].iLocPosition));

	/* Fill texture. */
	pStatics->shader[program].iLocTexCoord[0] = GL_CHECK(glGetAttribLocation(pStatics->shader[program].iProgName, "a_v2TexCoord"));
	if(pStatics->shader[program].iLocTexCoord[0] == -1)
	{
		NxErrMsg("Warning: Attribute not found at %s:%i\n", __FILE__, __LINE__);
		return -1;
	}
	//else GL_CHECK(glEnableVertexAttribArray(pStatics->shader[program].iLocTexCoord));

	/* diffuse texture. */
	pStatics->shader[program].iLocMainTex[0] = GL_CHECK(glGetUniformLocation(pStatics->shader[program].iProgName, "diffuse"));
	if(pStatics->shader[program].iLocMainTex[0] == -1)
	{
		NxErrMsg("Warning: Uniform not found at %s:%i\n", __FILE__, __LINE__);
		//return -1;
	}
	else 
	{
		//GL_CHECK(glUniform1i(pStatics->shader[program].iLocMainTex[0], VR_INPUT_MODE_TEXTURE0));
	}
	VR_INFO("", VR_GRAPHIC_DBG_CTX, "vrInitializeCvt2UV end\n"); 		
	return 0;
}

/* For Cvt2Rgba. */		
//For 32bit context
static int vrInitializeCvt2Rgba( HSURFTARGET hTarget)
{
	VR_ASSERT("hTarget must exist", hTarget);

	Statics* pStatics = vrGetStatics();
	unsigned int program = VR_PROGRAM_CVT2RGBA;
	VR_INFO("", VR_GRAPHIC_DBG_CTX, "vrInitialize_cvt2rgb start\n");		

	if(nxGSurfaceCreateEGLContext(program) != 0)
	{
		NxErrMsg("Error: Fail to create context %s:%i\n", __FILE__, __LINE__);
		return -1;	
	}

	/* Make context current. */
#ifdef VR_FEATURE_SEPERATE_FD_USE
	EGLBoolean bResult = eglMakeCurrent(pStatics->egl_info.sEGLDisplay, hTarget->target_pixmap_surface, 
								hTarget->target_pixmap_surface, pStatics->egl_info.sEGLContext[program]);
#else
	EGLBoolean bResult = eglMakeCurrent(pStatics->egl_info.sEGLDisplay, hTarget->target_pixmap_surfaces[0], 
								hTarget->target_pixmap_surfaces[0], pStatics->egl_info.sEGLContext[program]);
#endif
	if(bResult == EGL_FALSE)
	{
		EGLint iError = eglGetError();
		NxErrMsg("eglGetError(): %i (0x%.4x)\n", (int)iError, (int)iError);
		NxErrMsg("Error: Failed to make context current at %s:%i\n", __FILE__, __LINE__);
		return -1;	
	}
	
	/* Load shaders. */
	if(processShader(&pStatics->shader[program].iVertName, VERTEX_SHADER_SOURCE_CVT2RGBA, GL_VERTEX_SHADER) < 0)
	{
		NxErrMsg("Error: wrong shader %s:%i\n", __FILE__, __LINE__);
		return -1;
	}
	if(processShader(&pStatics->shader[program].iFragName, FRAGMENT_SHADER_SOURCE_CVT2RGBA, GL_FRAGMENT_SHADER) < 0)
	{
		NxErrMsg("Error: wrong shader %s:%i\n", __FILE__, __LINE__);
		return -1;
	}

	/* Set up shaders. */
	pStatics->shader[program].iProgName = GL_CHECK(glCreateProgram());
	//NxDbgMsg("Deinterlace iProgName(%d)\n", pStatics->shader[program].iProgName);		
	GL_CHECK(glAttachShader(pStatics->shader[program].iProgName, pStatics->shader[program].iVertName));
	GL_CHECK(glAttachShader(pStatics->shader[program].iProgName, pStatics->shader[program].iFragName));
	GL_CHECK(glLinkProgram(pStatics->shader[program].iProgName));
	GL_CHECK(glUseProgram(pStatics->shader[program].iProgName));

	/* Vertex positions. */
	pStatics->shader[program].iLocPosition = GL_CHECK(glGetAttribLocation(pStatics->shader[program].iProgName, "a_v4Position"));
	if(pStatics->shader[program].iLocPosition == -1)
	{
		NxErrMsg("Error: Attribute not found at %s:%i\n", __FILE__, __LINE__);
		return -1;
	}
	//else GL_CHECK(glEnableVertexAttribArray(pStatics->shader[program].iLocPosition));

	/* Fill texture. */
	pStatics->shader[program].iLocTexCoord[VR_INPUT_MODE_Y] = GL_CHECK(glGetAttribLocation(pStatics->shader[program].iProgName, "a_v2TexCoordY"));
	if(pStatics->shader[program].iLocTexCoord[VR_INPUT_MODE_Y] == -1)
	{
		NxErrMsg("Warning: Attribute not found at %s:%i\n", __FILE__, __LINE__);
		return -1;
	}
	//else GL_CHECK(glEnableVertexAttribArray(pStatics->shader[program].iLocTexCoord));
	pStatics->shader[program].iLocTexCoord[VR_INPUT_MODE_U] = GL_CHECK(glGetAttribLocation(pStatics->shader[program].iProgName, "a_v2TexCoordU"));
	if(pStatics->shader[program].iLocTexCoord[VR_INPUT_MODE_U] == -1)
	{
		NxErrMsg("Warning: Attribute not found at %s:%i\n", __FILE__, __LINE__);
		return -1;
	}
	//else GL_CHECK(glEnableVertexAttribArray(pStatics->shader[program].iLocTexCoord));
	pStatics->shader[program].iLocTexCoord[VR_INPUT_MODE_V] = GL_CHECK(glGetAttribLocation(pStatics->shader[program].iProgName, "a_v2TexCoordV"));
	if(pStatics->shader[program].iLocTexCoord[VR_INPUT_MODE_V] == -1)
	{
		NxErrMsg("Warning: Attribute not found at %s:%i\n", __FILE__, __LINE__);
		return -1;
	}
	//else GL_CHECK(glEnableVertexAttribArray(pStatics->shader[program].iLocTexCoord));
		
	/* Y texture. */
	pStatics->shader[program].iLocMainTex[VR_INPUT_MODE_Y] = GL_CHECK(glGetUniformLocation(pStatics->shader[program].iProgName, "diffuseY"));
	if(pStatics->shader[program].iLocMainTex[VR_INPUT_MODE_Y] == -1)
	{
		NxErrMsg("Warning: Uniform not found at %s:%i\n", __FILE__, __LINE__);
		//return -1;
	}
	else 
	{
		//GL_CHECK(glUniform1i(pStatics->shader[program].iLocMainTex[0], VR_INPUT_MODE_DEINTERLACE));
	}
		
	/* U texture. */
	pStatics->shader[program].iLocMainTex[VR_INPUT_MODE_U] = GL_CHECK(glGetUniformLocation(pStatics->shader[program].iProgName, "diffuseU"));
	if(pStatics->shader[program].iLocMainTex[VR_INPUT_MODE_U] == -1)
	{
		NxErrMsg("Warning: Uniform not found at %s:%i\n", __FILE__, __LINE__);
		//return -1;
	}
	else 
	{
		//GL_CHECK(glUniform1i(pStatics->shader[program].iLocMainTex[0], VR_INPUT_MODE_DEINTERLACE));
	}
		
	/* V texture. */
	pStatics->shader[program].iLocMainTex[VR_INPUT_MODE_V] = GL_CHECK(glGetUniformLocation(pStatics->shader[program].iProgName, "diffuseV"));
	if(pStatics->shader[program].iLocMainTex[VR_INPUT_MODE_V] == -1)
	{
		NxErrMsg("Warning: Uniform not found at %s:%i\n", __FILE__, __LINE__);
		//return -1;
	}
	else 
	{
		//GL_CHECK(glUniform1i(pStatics->shader[program].iLocMainTex[0], VR_INPUT_MODE_DEINTERLACE));
	}	
	VR_INFO("", VR_GRAPHIC_DBG_CTX, "vrInitialize_cvt2rgb end\n"); 		
	return 0;
}

/* For UserFilter. */
//For 32bit context
static int vrInitializeUserFilter( HSURFTARGET hTarget)
{
	VR_ASSERT("hTarget must exist", hTarget);

	Statics* pStatics = vrGetStatics();
	unsigned int program = VR_PROGRAM_USERFILTER;
	VR_INFO("", VR_GRAPHIC_DBG_CTX, "vrInitializeUserFilter start\n");		

	if(nxGSurfaceCreateEGLContext(program) != 0)
	{
		NxErrMsg("Error: Fail to create context %s:%i\n", __FILE__, __LINE__);
		return -1;	
	}

	/* Make context current. */
#ifdef VR_FEATURE_SEPERATE_FD_USE
	EGLBoolean bResult = eglMakeCurrent(pStatics->egl_info.sEGLDisplay, hTarget->target_pixmap_surface, 
								hTarget->target_pixmap_surface, pStatics->egl_info.sEGLContext[program]);
#else
	EGLBoolean bResult = eglMakeCurrent(pStatics->egl_info.sEGLDisplay, hTarget->target_pixmap_surfaces[0], 
								hTarget->target_pixmap_surfaces[0], pStatics->egl_info.sEGLContext[program]);
#endif
	if(bResult == EGL_FALSE)
	{
		EGLint iError = eglGetError();
		NxErrMsg("eglGetError(): %i (0x%.4x)\n", (int)iError, (int)iError);
		NxErrMsg("Error: Failed to make context current at %s:%i\n", __FILE__, __LINE__);
		return -1;	
	}
	
	/* Load shaders. */
	if(processShader(&pStatics->shader[program].iVertName, VERTEX_SHADER_SOURCE_USERFILTER, GL_VERTEX_SHADER) < 0)
	{
		NxErrMsg("Error: wrong shader %s:%i\n", __FILE__, __LINE__);
		return -1;
	}
	if(processShader(&pStatics->shader[program].iFragName, FRAGMENT_SHADER_SOURCE_USERFILTER, GL_FRAGMENT_SHADER) < 0)
	{
		NxErrMsg("Error: wrong shader %s:%i\n", __FILE__, __LINE__);
		return -1;
	}

	/* Set up shaders. */
	pStatics->shader[program].iProgName = GL_CHECK(glCreateProgram());
	//NxDbgMsg("Deinterlace iProgName(%d)\n", pStatics->shader[program].iProgName);		
	GL_CHECK(glAttachShader(pStatics->shader[program].iProgName, pStatics->shader[program].iVertName));
	GL_CHECK(glAttachShader(pStatics->shader[program].iProgName, pStatics->shader[program].iFragName));
	GL_CHECK(glLinkProgram(pStatics->shader[program].iProgName));
	GL_CHECK(glUseProgram(pStatics->shader[program].iProgName));
	
	/* Vertex positions. */
	pStatics->shader[program].iLocPosition = GL_CHECK(glGetAttribLocation(pStatics->shader[program].iProgName, "a_v4Position"));
	if(pStatics->shader[program].iLocPosition == -1)
	{
		NxErrMsg("Error: Attribute not found at %s:%i\n", __FILE__, __LINE__);
		return -1;
	}
	//else GL_CHECK(glEnableVertexAttribArray(pStatics->shader[program].iLocPosition));

	/* Fill texture. */
	pStatics->shader[program].iLocTexCoord[0] = GL_CHECK(glGetAttribLocation(pStatics->shader[program].iProgName, "a_v2TexCoord"));
	if(pStatics->shader[program].iLocTexCoord[0] == -1)
	{
		NxErrMsg("Warning: Attribute not found at %s:%i\n", __FILE__, __LINE__);
		return -1;
	}
	//else GL_CHECK(glEnableVertexAttribArray(pStatics->shader[program].iLocTexCoord));

    /* Texture Width. */
    pStatics->shader[program].iLocInputWidth = GL_CHECK(glGetUniformLocation(pStatics->shader[program].iProgName, "u_fTexWidth"));
    if(pStatics->shader[program].iLocInputWidth == -1)
    {
		NxErrMsg("Warning: Uniform not found at %s:%i\n", __FILE__, __LINE__);
		//return -1;
    }
    
    /* Texture Height. */
    pStatics->shader[program].iLocInputHeight = GL_CHECK(glGetUniformLocation(pStatics->shader[program].iProgName, "u_fTexHeight"));
    if(pStatics->shader[program].iLocInputHeight == -1)
    {
		NxErrMsg("Warning: Uniform not found at %s:%i\n", __FILE__, __LINE__);
		//return -1;
    }

    /* diffuse texture. */
    pStatics->shader[program].iLocMainTex[0] = GL_CHECK(glGetUniformLocation(pStatics->shader[program].iProgName, "diffuse"));
    if(pStatics->shader[program].iLocMainTex[0] == -1)
    {
		NxErrMsg("Warning: Uniform not found at %s:%i\n", __FILE__, __LINE__);
		//return -1;
    }
    else 
    {
        //GL_CHECK(glUniform1i(pStatics->shader[program].iLocMainTex[0], VR_INPUT_MODE_DEINTERLACE));
    }	
	VR_INFO("", VR_GRAPHIC_DBG_CTX, "vrInitializeUserFilter end\n"); 		
	return 0;
}

/* For YuvFilter. */
//For 16bit context
static int vrInitializeYuvFilter( HSURFTARGET hTarget)
{
	VR_ASSERT("hTarget must exist", hTarget);

	Statics* pStatics = vrGetStatics();
	unsigned int program = VR_PROGRAM_YUVFILTER_Y;
	VR_INFO("", VR_GRAPHIC_DBG_CTX, "vrInitializeYuvFilter start\n");	

	if(nxGSurfaceCreateEGLContext(program) != 0)
	{
		NxErrMsg("Error: Fail to create context %s:%i\n", __FILE__, __LINE__);
		return -1;	
	}

	/* Make context current. */
#ifdef VR_FEATURE_SEPERATE_FD_USE
	EGLBoolean bResult = eglMakeCurrent(pStatics->egl_info.sEGLDisplay, hTarget->target_pixmap_surface, 
								hTarget->target_pixmap_surface, pStatics->egl_info.sEGLContext[program]);
#else
	EGLBoolean bResult = eglMakeCurrent(pStatics->egl_info.sEGLDisplay, hTarget->target_pixmap_surfaces[0], 
								hTarget->target_pixmap_surfaces[0], pStatics->egl_info.sEGLContext[program]);
#endif
	if(bResult == EGL_FALSE)
	{
		EGLint iError = eglGetError();
		NxErrMsg("eglGetError(): %i (0x%.4x)\n", (int)iError, (int)iError);
		NxErrMsg("Error: Failed to make context current at %s:%i\n", __FILE__, __LINE__);
		return -1;	
	}

	/* Y */
	{
		/* Load shaders. */
		if(processShader(&pStatics->shader[program].iVertName, VERTEX_SHADER_SOURCE_YUVFILTER_Y, GL_VERTEX_SHADER) < 0)
		{
			NxErrMsg("Error: wrong shader %s:%i\n", __FILE__, __LINE__);
			return -1;
		}
		if(processShader(&pStatics->shader[program].iFragName, FRAGMENT_SHADER_SOURCE_YUVFILTER_Y, GL_FRAGMENT_SHADER) < 0)
		{
			NxErrMsg("Error: wrong shader %s:%i\n", __FILE__, __LINE__);
			return -1;
		}

		/* Set up shaders. */
		pStatics->shader[program].iProgName = GL_CHECK(glCreateProgram());
		//NxDbgMsg("Scaler iProgName(%d)\n", pStatics->shader[program].iProgName);
		GL_CHECK(glAttachShader(pStatics->shader[program].iProgName, pStatics->shader[program].iVertName));
		GL_CHECK(glAttachShader(pStatics->shader[program].iProgName, pStatics->shader[program].iFragName));
		GL_CHECK(glLinkProgram(pStatics->shader[program].iProgName));
		GL_CHECK(glUseProgram(pStatics->shader[program].iProgName));

		/* Vertex positions. */
		pStatics->shader[program].iLocPosition = GL_CHECK(glGetAttribLocation(pStatics->shader[program].iProgName, "a_v4Position"));
		if(pStatics->shader[program].iLocPosition == -1)
		{
			NxErrMsg("Error: Attribute not found at %s:%i\n", __FILE__, __LINE__);
			return -1;
		}
		//else GL_CHECK(glEnableVertexAttribArray(pStatics->shader[program].iLocPosition));

		/* Fill texture. */
		pStatics->shader[program].iLocTexCoord[0] = GL_CHECK(glGetAttribLocation(pStatics->shader[program].iProgName, "a_v2TexCoord"));
		if(pStatics->shader[program].iLocTexCoord[0] == -1)
		{
			NxErrMsg("Warning: Attribute not found at %s:%i\n", __FILE__, __LINE__);
			return -1;
		}
		//else GL_CHECK(glEnableVertexAttribArray(pStatics->shader[program].iLocTexCoord));

		/* diffuse texture. */
		pStatics->shader[program].iLocMainTex[0] = GL_CHECK(glGetUniformLocation(pStatics->shader[program].iProgName, "diffuse"));
		if(pStatics->shader[program].iLocMainTex[0] == -1)
		{
			NxErrMsg("Warning: Uniform not found at %s:%i\n", __FILE__, __LINE__);
			//return -1;
		}
		else 
		{
			//GL_CHECK(glUniform1i(pStatics->shader[program].iLocMainTex[0], VR_INPUT_MODE_TEXTURE0));
		}
		
		/* texture inverse size */
		pStatics->shader[program].iLocTexInvSize = GL_CHECK(glGetUniformLocation(pStatics->shader[program].iProgName, "Tinvsize"));
		if(pStatics->shader[program].iLocTexInvSize == -1)
		{
			NxErrMsg("Warning: Uniform not found at %s:%i\n", __FILE__, __LINE__);
			//return -1;
		}

		/* texture filter gain */
		pStatics->shader[program].iLocFilterGain = GL_CHECK(glGetUniformLocation(pStatics->shader[program].iProgName, "Tgain"));
		if(pStatics->shader[program].iLocFilterGain == -1)
		{
			NxErrMsg("Warning: Uniform not found at %s:%i\n", __FILE__, __LINE__);
			//return -1;
		}
	}

	#if 1
	program = VR_PROGRAM_YUVFILTER_UV;
	/* UV */
	{
		/* Load shaders. */
		if(processShader(&pStatics->shader[program].iVertName, VERTEX_SHADER_SOURCE_YUVFILTER_UV, GL_VERTEX_SHADER) < 0)
		{
			NxErrMsg("Error: wrong shader %s:%i\n", __FILE__, __LINE__);
			return -1;
		}
		if(processShader(&pStatics->shader[program].iFragName, FRAGMENT_SHADER_SOURCE_YUVFILTER_UV, GL_FRAGMENT_SHADER) < 0)
		{
			NxErrMsg("Error: wrong shader %s:%i\n", __FILE__, __LINE__);
			return -1;
		}

		/* Set up shaders. */
		pStatics->shader[program].iProgName = GL_CHECK(glCreateProgram());
		//NxDbgMsg("Scaler iProgName(%d)\n", pStatics->shader[program].iProgName);
		GL_CHECK(glAttachShader(pStatics->shader[program].iProgName, pStatics->shader[program].iVertName));
		GL_CHECK(glAttachShader(pStatics->shader[program].iProgName, pStatics->shader[program].iFragName));
		GL_CHECK(glLinkProgram(pStatics->shader[program].iProgName));
		GL_CHECK(glUseProgram(pStatics->shader[program].iProgName));

		/* Vertex positions. */
		pStatics->shader[program].iLocPosition = GL_CHECK(glGetAttribLocation(pStatics->shader[program].iProgName, "a_v4Position"));
		if(pStatics->shader[program].iLocPosition == -1)
		{
			NxErrMsg("Error: Attribute not found at %s:%i\n", __FILE__, __LINE__);
			return -1;
		}
		//else GL_CHECK(glEnableVertexAttribArray(pStatics->shader[program].iLocPosition));

		/* Fill texture. */
		pStatics->shader[program].iLocTexCoord[0] = GL_CHECK(glGetAttribLocation(pStatics->shader[program].iProgName, "a_v2TexCoord"));
		if(pStatics->shader[program].iLocTexCoord[0] == -1)
		{
			NxErrMsg("Warning: Attribute not found at %s:%i\n", __FILE__, __LINE__);
			return -1;
		}
		//else GL_CHECK(glEnableVertexAttribArray(pStatics->shader[program].iLocTexCoord));

		/* diffuse texture. */
		pStatics->shader[program].iLocMainTex[0] = GL_CHECK(glGetUniformLocation(pStatics->shader[program].iProgName, "diffuse"));
		if(pStatics->shader[program].iLocMainTex[0] == -1)
		{
			NxErrMsg("Warning: Uniform not found at %s:%i\n", __FILE__, __LINE__);
			//return -1;
		}
		else 
		{
			//GL_CHECK(glUniform1i(pStatics->shader[program].iLocMainTex[0], VR_INPUT_MODE_TEXTURE0));
		}		
	}
	#endif
	
	VR_INFO("", VR_GRAPHIC_DBG_CTX, "vrInitializeYuvFilter end\n"); 		
	return 0;
}

static int vrInitializeScaleRGBA( HSURFTARGET hTarget)
{
	VR_ASSERT("hTarget must exist", hTarget);

	Statics* pStatics = vrGetStatics();
	unsigned int program = VR_PROGRAM_SCALE_RGBA;
	VR_INFO("", VR_GRAPHIC_DBG_CTX, "vrInitializeScaleRGBA start\n");		

	if(nxGSurfaceCreateEGLContext(program) != 0)
	{
		NxErrMsg("Error: Fail to create context %s:%i\n", __FILE__, __LINE__);
		return -1;	
	}

	/* Make context current. */
#ifdef VR_FEATURE_SEPERATE_FD_USE
	EGLBoolean bResult = eglMakeCurrent(pStatics->egl_info.sEGLDisplay, hTarget->target_pixmap_surface, 
								hTarget->target_pixmap_surface, pStatics->egl_info.sEGLContext[program]);
#else
	EGLBoolean bResult = eglMakeCurrent(pStatics->egl_info.sEGLDisplay, hTarget->target_pixmap_surfaces[0], 
								hTarget->target_pixmap_surfaces[0], pStatics->egl_info.sEGLContext[program]);
#endif
	if(bResult == EGL_FALSE)
	{
		EGLint iError = eglGetError();
		NxErrMsg("eglGetError(): %i (0x%.4x)\n", (int)iError, (int)iError);
		NxErrMsg("Error: Failed to make context current at %s:%i\n", __FILE__, __LINE__);
		return -1;	
	}
	
	/* Load shaders. */
	if(processShader(&pStatics->shader[program].iVertName, VERTEX_SHADER_SOURCE_SCALE_RGBA, GL_VERTEX_SHADER) < 0)
	{
		NxErrMsg("Error: wrong shader %s:%i\n", __FILE__, __LINE__);
		return -1;
	}
	if(processShader(&pStatics->shader[program].iFragName, FRAGMENT_SHADER_SOURCE_SCALE_RGBA, GL_FRAGMENT_SHADER) < 0)
	{
		NxErrMsg("Error: wrong shader %s:%i\n", __FILE__, __LINE__);
		return -1;
	}

	/* Set up shaders. */
	pStatics->shader[program].iProgName = GL_CHECK(glCreateProgram());
	//NxDbgMsg("Deinterlace iProgName(%d)\n", pStatics->shader[program].iProgName);		
	GL_CHECK(glAttachShader(pStatics->shader[program].iProgName, pStatics->shader[program].iVertName));
	GL_CHECK(glAttachShader(pStatics->shader[program].iProgName, pStatics->shader[program].iFragName));
	GL_CHECK(glLinkProgram(pStatics->shader[program].iProgName));
	GL_CHECK(glUseProgram(pStatics->shader[program].iProgName));
	
	/* Vertex positions. */
	pStatics->shader[program].iLocPosition = GL_CHECK(glGetAttribLocation(pStatics->shader[program].iProgName, "a_v4Position"));
	if(pStatics->shader[program].iLocPosition == -1)
	{
		NxErrMsg("Error: Attribute not found at %s:%i\n", __FILE__, __LINE__);
		return -1;
	}
	//else GL_CHECK(glEnableVertexAttribArray(pStatics->shader[program].iLocPosition));

	/* Fill texture. */
	pStatics->shader[program].iLocTexCoord[0] = GL_CHECK(glGetAttribLocation(pStatics->shader[program].iProgName, "a_v2TexCoord"));
	if(pStatics->shader[program].iLocTexCoord[0] == -1)
	{
		NxErrMsg("Warning: Attribute not found at %s:%i\n", __FILE__, __LINE__);
		return -1;
	}
	//else GL_CHECK(glEnableVertexAttribArray(pStatics->shader[program].iLocTexCoord));

	#if 0
	/* Texture Width. */
	pStatics->shader[program].iLocInputWidth = GL_CHECK(glGetUniformLocation(pStatics->shader[program].iProgName, "u_fTexWidth"));
	if(pStatics->shader[program].iLocInputWidth == -1)
	{
		NxErrMsg("Warning: Uniform not found at %s:%i\n", __FILE__, __LINE__);
		//return -1;
	}
	
	/* Texture Height. */
	pStatics->shader[program].iLocInputHeight = GL_CHECK(glGetUniformLocation(pStatics->shader[program].iProgName, "u_fTexHeight"));
	if(pStatics->shader[program].iLocInputHeight == -1)
	{
		NxErrMsg("Warning: Uniform not found at %s:%i\n", __FILE__, __LINE__);
		//return -1;
	}
	#endif

	/* diffuse texture. */
	pStatics->shader[program].iLocMainTex[0] = GL_CHECK(glGetUniformLocation(pStatics->shader[program].iProgName, "diffuse"));
	if(pStatics->shader[program].iLocMainTex[0] == -1)
	{
		NxErrMsg("Warning: Uniform not found at %s:%i\n", __FILE__, __LINE__);
		//return -1;
	}
	else 
	{
		//GL_CHECK(glUniform1i(pStatics->shader[program].iLocMainTex[0], VR_INPUT_MODE_DEINTERLACE));
	}	
	VR_INFO("", VR_GRAPHIC_DBG_CTX, "vrInitializeScaleRGBA end\n");		
	return 0;
}


static int vrDeinitializeDeinterlace( HSURFTARGET hTarget)
{
	Statics* pStatics = vrGetStatics();
	int ret = 0;

#ifdef VR_FEATURE_SEPERATE_FD_USE
	EGLBoolean bResult = eglMakeCurrent(pStatics->egl_info.sEGLDisplay, hTarget->target_pixmap_surface, 
								hTarget->target_pixmap_surface, pStatics->egl_info.sEGLContext[VR_PROGRAM_DEINTERLACE]);
#else
	EGLBoolean bResult = eglMakeCurrent(pStatics->egl_info.sEGLDisplay, hTarget->target_pixmap_surfaces[0], 
								hTarget->target_pixmap_surfaces[0], pStatics->egl_info.sEGLContext[VR_PROGRAM_DEINTERLACE]);
#endif
	if(bResult == EGL_FALSE)
	{
		EGLint iError = eglGetError();
		NxErrMsg("eglGetError(): %i (0x%.4x)\n", (int)iError, (int)iError);
		NxErrMsg("Error: Failed to make context current at %s:%i\n", __FILE__, __LINE__);
		return -1; 
	}

	GL_CHECK(glDeleteTextures(1,&pStatics->tex_deinterlace_ref_id));
	GL_CHECK(glDeleteShader(pStatics->shader[VR_PROGRAM_DEINTERLACE].iVertName	));
	GL_CHECK(glDeleteShader(pStatics->shader[VR_PROGRAM_DEINTERLACE].iFragName	));
	GL_CHECK(glDeleteProgram(pStatics->shader[VR_PROGRAM_DEINTERLACE].iProgName ));
	
	VR_INFO("", VR_GRAPHIC_DBG_TARGET, "Deinterlace eglDestroyContext start, 32ctx\n");		
	vrResetShaderInfo(&pStatics->shader[VR_PROGRAM_DEINTERLACE]);

	ret = nxGSurfaceDestroyEGLContext(VR_PROGRAM_DEINTERLACE);	
	return ret;
}

static int vrDeinitializeScaler( HSURFTARGET hTarget)
{
	Statics* pStatics = vrGetStatics();
	int ret = 0;

#ifdef VR_FEATURE_SEPERATE_FD_USE
	EGLBoolean bResult = eglMakeCurrent(pStatics->egl_info.sEGLDisplay, hTarget->target_pixmap_surface, 
								hTarget->target_pixmap_surface, pStatics->egl_info.sEGLContext[VR_PROGRAM_SCALE]);
#else
	EGLBoolean bResult = eglMakeCurrent(pStatics->egl_info.sEGLDisplay, hTarget->target_pixmap_surfaces[0], 
								hTarget->target_pixmap_surfaces[0], pStatics->egl_info.sEGLContext[VR_PROGRAM_SCALE]);
#endif	
	if(bResult == EGL_FALSE)
	{
		EGLint iError = eglGetError();
		NxErrMsg("eglGetError(): %i (0x%.4x)\n", (int)iError, (int)iError);
		NxErrMsg("Error: Failed to make context current at %s:%i\n", __FILE__, __LINE__);
		return -1; 
	}

	GL_CHECK(glDeleteShader(pStatics->shader[VR_PROGRAM_SCALE].iVertName ));
	GL_CHECK(glDeleteShader(pStatics->shader[VR_PROGRAM_SCALE].iFragName ));
	GL_CHECK(glDeleteProgram(pStatics->shader[VR_PROGRAM_SCALE].iProgName ));
	
	VR_INFO("", VR_GRAPHIC_DBG_TARGET, "Scale eglDestroyContext start, 8ctx\n");		
	vrResetShaderInfo(&pStatics->shader[VR_PROGRAM_SCALE]);
	
	ret = nxGSurfaceDestroyEGLContext(VR_PROGRAM_SCALE);	
	return ret;
}

static int vrDeinitializeCvt2Y( HSURFTARGET hTarget)
{
	Statics* pStatics = vrGetStatics();
	int ret = 0;

#ifdef VR_FEATURE_SEPERATE_FD_USE
	EGLBoolean bResult = eglMakeCurrent(pStatics->egl_info.sEGLDisplay, hTarget->target_pixmap_surface, 
								hTarget->target_pixmap_surface, pStatics->egl_info.sEGLContext[VR_PROGRAM_CVT2Y]);
#else
	EGLBoolean bResult = eglMakeCurrent(pStatics->egl_info.sEGLDisplay, hTarget->target_pixmap_surfaces[0], 
								hTarget->target_pixmap_surfaces[0], pStatics->egl_info.sEGLContext[VR_PROGRAM_CVT2Y]);
#endif		
	if(bResult == EGL_FALSE)
	{
		EGLint iError = eglGetError();
		NxErrMsg("eglGetError(): %i (0x%.4x)\n", (int)iError, (int)iError);
		NxErrMsg("Error: Failed to make context current at %s:%i\n", __FILE__, __LINE__);
		return -1; 
	}

	GL_CHECK(glDeleteShader(pStatics->shader[VR_PROGRAM_CVT2Y].iVertName	));
	GL_CHECK(glDeleteShader(pStatics->shader[VR_PROGRAM_CVT2Y].iFragName	));
	GL_CHECK(glDeleteProgram(pStatics->shader[VR_PROGRAM_CVT2Y].iProgName ));
	
	VR_INFO("", VR_GRAPHIC_DBG_TARGET, "Cvt2Y eglDestroyContext start, 8ctx\n");	
	vrResetShaderInfo(&pStatics->shader[VR_PROGRAM_CVT2Y]);
	
	ret = nxGSurfaceDestroyEGLContext(VR_PROGRAM_CVT2Y);	
	return ret;
}

static int vrDeinitializeCvt2UV( HSURFTARGET hTarget)
{
	Statics* pStatics = vrGetStatics();
	int ret = 0;

#ifdef VR_FEATURE_SEPERATE_FD_USE
	EGLBoolean bResult = eglMakeCurrent(pStatics->egl_info.sEGLDisplay, hTarget->target_pixmap_surface, 
								hTarget->target_pixmap_surface, pStatics->egl_info.sEGLContext[VR_PROGRAM_CVT2UV]);
#else
	EGLBoolean bResult = eglMakeCurrent(pStatics->egl_info.sEGLDisplay, hTarget->target_pixmap_surfaces[0], 
								hTarget->target_pixmap_surfaces[0], pStatics->egl_info.sEGLContext[VR_PROGRAM_CVT2UV]);
#endif			
	if(bResult == EGL_FALSE)
	{
		EGLint iError = eglGetError();
		NxErrMsg("eglGetError(): %i (0x%.4x)\n", (int)iError, (int)iError);
		NxErrMsg("Error: Failed to make context current at %s:%i\n", __FILE__, __LINE__);
		return -1; 
	}
	
	GL_CHECK(glDeleteShader(pStatics->shader[VR_PROGRAM_CVT2UV].iVertName	));
	GL_CHECK(glDeleteShader(pStatics->shader[VR_PROGRAM_CVT2UV].iFragName	));
	GL_CHECK(glDeleteProgram(pStatics->shader[VR_PROGRAM_CVT2UV].iProgName ));
	
	VR_INFO("", VR_GRAPHIC_DBG_TARGET, "Cvt2UV eglDestroyContext start, 16ctx\n");	
	vrResetShaderInfo(&pStatics->shader[VR_PROGRAM_CVT2UV]);

	ret = nxGSurfaceDestroyEGLContext(VR_PROGRAM_CVT2UV);	
	return ret;
}

static int vrDeinitializeCvt2Rgba( HSURFTARGET hTarget)
{
	Statics* pStatics = vrGetStatics();
	int ret = 0;
	
#ifdef VR_FEATURE_SEPERATE_FD_USE
	EGLBoolean bResult = eglMakeCurrent(pStatics->egl_info.sEGLDisplay, hTarget->target_pixmap_surface, 
								hTarget->target_pixmap_surface, pStatics->egl_info.sEGLContext[VR_PROGRAM_CVT2RGBA]);
#else
	EGLBoolean bResult = eglMakeCurrent(pStatics->egl_info.sEGLDisplay, hTarget->target_pixmap_surfaces[0], 
								hTarget->target_pixmap_surfaces[0], pStatics->egl_info.sEGLContext[VR_PROGRAM_CVT2RGBA]);
#endif			
	if(bResult == EGL_FALSE)
	{
		EGLint iError = eglGetError();
		NxErrMsg("eglGetError(): %i (0x%.4x)\n", (int)iError, (int)iError);
		NxErrMsg("Error: Failed to make context current at %s:%i\n", __FILE__, __LINE__);
		return -1; 
	}
	
	GL_CHECK(glDeleteShader(pStatics->shader[VR_PROGRAM_CVT2RGBA].iVertName	));
	GL_CHECK(glDeleteShader(pStatics->shader[VR_PROGRAM_CVT2RGBA].iFragName	));
	GL_CHECK(glDeleteProgram(pStatics->shader[VR_PROGRAM_CVT2RGBA].iProgName ));
	
	VR_INFO("", VR_GRAPHIC_DBG_TARGET, "Cvt2Rgba eglDestroyContext start, 32ctx\n");
	vrResetShaderInfo(&pStatics->shader[VR_PROGRAM_CVT2RGBA]);

	ret = nxGSurfaceDestroyEGLContext(VR_PROGRAM_CVT2RGBA);	
	return ret;
}

static int vrDeinitializeUserFilter( HSURFTARGET hTarget)
{
	Statics* pStatics = vrGetStatics();
	int ret = 0;
	
#ifdef VR_FEATURE_SEPERATE_FD_USE
	EGLBoolean bResult = eglMakeCurrent(pStatics->egl_info.sEGLDisplay, hTarget->target_pixmap_surface, 
								hTarget->target_pixmap_surface, pStatics->egl_info.sEGLContext[VR_PROGRAM_USERFILTER]);
#else
	EGLBoolean bResult = eglMakeCurrent(pStatics->egl_info.sEGLDisplay, hTarget->target_pixmap_surfaces[0], 
								hTarget->target_pixmap_surfaces[0], pStatics->egl_info.sEGLContext[VR_PROGRAM_USERFILTER]);
#endif			
	if(bResult == EGL_FALSE)
	{
		EGLint iError = eglGetError();
		NxErrMsg("eglGetError(): %i (0x%.4x)\n", (int)iError, (int)iError);
		NxErrMsg("Error: Failed to make context current at %s:%i\n", __FILE__, __LINE__);
		return -1; 
	}
	
	GL_CHECK(glDeleteShader(pStatics->shader[VR_PROGRAM_USERFILTER].iVertName	));
	GL_CHECK(glDeleteShader(pStatics->shader[VR_PROGRAM_USERFILTER].iFragName	));
	GL_CHECK(glDeleteProgram(pStatics->shader[VR_PROGRAM_USERFILTER].iProgName ));
	
	VR_INFO("", VR_GRAPHIC_DBG_TARGET, "UserFilter eglDestroyContext start, 32ctx\n");
	vrResetShaderInfo(&pStatics->shader[VR_PROGRAM_USERFILTER]);

	ret = nxGSurfaceDestroyEGLContext(VR_PROGRAM_USERFILTER);	
	return ret;
}

static int vrDeinitializeYuvFilter( HSURFTARGET hTarget)
{
	Statics* pStatics = vrGetStatics();
	int ret = 0;

#ifdef VR_FEATURE_SEPERATE_FD_USE
	EGLBoolean bResult = eglMakeCurrent(pStatics->egl_info.sEGLDisplay, hTarget->target_pixmap_surface, 
								hTarget->target_pixmap_surface, pStatics->egl_info.sEGLContext[VR_PROGRAM_YUVFILTER_Y]);
#else
	EGLBoolean bResult = eglMakeCurrent(pStatics->egl_info.sEGLDisplay, hTarget->target_pixmap_surfaces[0], 
								hTarget->target_pixmap_surfaces[0], pStatics->egl_info.sEGLContext[VR_PROGRAM_YUVFILTER_Y]);
#endif	
	if(bResult == EGL_FALSE)
	{
		EGLint iError = eglGetError();
		NxErrMsg("eglGetError(): %i (0x%.4x)\n", (int)iError, (int)iError);
		NxErrMsg("Error: Failed to make context current at %s:%i\n", __FILE__, __LINE__);
		return -1; 
	}

	int program = VR_PROGRAM_YUVFILTER_Y;
	GL_CHECK(glDeleteShader(pStatics->shader[program].iVertName ));
	GL_CHECK(glDeleteShader(pStatics->shader[program].iFragName ));
	GL_CHECK(glDeleteProgram(pStatics->shader[program].iProgName ));
	vrResetShaderInfo(&pStatics->shader[VR_PROGRAM_YUVFILTER_Y]);

	#if 1
	program = VR_PROGRAM_YUVFILTER_UV;
	GL_CHECK(glDeleteShader(pStatics->shader[program].iVertName ));
	GL_CHECK(glDeleteShader(pStatics->shader[program].iFragName ));
	GL_CHECK(glDeleteProgram(pStatics->shader[program].iProgName ));
	vrResetShaderInfo(&pStatics->shader[VR_PROGRAM_YUVFILTER_UV]);
	#endif
	
	VR_INFO("", VR_GRAPHIC_DBG_TARGET, "Scale eglDestroyContext start, 8ctx\n");		
	
	ret = nxGSurfaceDestroyEGLContext(VR_PROGRAM_YUVFILTER_Y);	
	return ret;
}

static int vrDeinitializeScaleRGBA( HSURFTARGET hTarget)
{
	Statics* pStatics = vrGetStatics();
	int ret = 0;
	
#ifdef VR_FEATURE_SEPERATE_FD_USE
	EGLBoolean bResult = eglMakeCurrent(pStatics->egl_info.sEGLDisplay, hTarget->target_pixmap_surface, 
								hTarget->target_pixmap_surface, pStatics->egl_info.sEGLContext[VR_PROGRAM_SCALE_RGBA]);
#else
	EGLBoolean bResult = eglMakeCurrent(pStatics->egl_info.sEGLDisplay, hTarget->target_pixmap_surfaces[0], 
								hTarget->target_pixmap_surfaces[0], pStatics->egl_info.sEGLContext[VR_PROGRAM_SCALE_RGBA]);
#endif			
	if(bResult == EGL_FALSE)
	{
		EGLint iError = eglGetError();
		NxErrMsg("eglGetError(): %i (0x%.4x)\n", (int)iError, (int)iError);
		NxErrMsg("Error: Failed to make context current at %s:%i\n", __FILE__, __LINE__);
		return -1; 
	}
	
	GL_CHECK(glDeleteShader(pStatics->shader[VR_PROGRAM_SCALE_RGBA].iVertName	));
	GL_CHECK(glDeleteShader(pStatics->shader[VR_PROGRAM_SCALE_RGBA].iFragName	));
	GL_CHECK(glDeleteProgram(pStatics->shader[VR_PROGRAM_SCALE_RGBA].iProgName ));
	
	VR_INFO("", VR_GRAPHIC_DBG_TARGET, "ScaleRGBA eglDestroyContext start, 32ctx\n");
	vrResetShaderInfo(&pStatics->shader[VR_PROGRAM_SCALE_RGBA]);

	ret = nxGSurfaceDestroyEGLContext(VR_PROGRAM_SCALE_RGBA);	
	return ret;
}

