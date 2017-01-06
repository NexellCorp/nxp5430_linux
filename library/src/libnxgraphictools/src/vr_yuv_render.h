#ifndef __VR_DEINTERACE__
#define __VR_DEINTERACE__

#include "vr_common_inc.h"

#define VERTEX_SHADER_SOURCE_DEINTERLACE		deinterace_vertex_shader
#define FRAGMENT_SHADER_SOURCE_DEINTERLACE		deinterace_frag_shader
#define VERTEX_SHADER_SOURCE_SCALE				scaler_vertex_shader
#define FRAGMENT_SHADER_SOURCE_SCALE			scaler_frag_shader
#define VERTEX_SHADER_SOURCE_CVT2Y				cvt2y_vertex_shader
#define FRAGMENT_SHADER_SOURCE_CVT2Y			cvt2y_frag_shader
#define VERTEX_SHADER_SOURCE_CVT2UV				cvt2uv_vertex_shader
#define FRAGMENT_SHADER_SOURCE_CVT2UV			cvt2uv_frag_shader
#define VERTEX_SHADER_SOURCE_CVT2RGBA			cvt2rgba_vertex_shader
#define FRAGMENT_SHADER_SOURCE_CVT2RGBA			cvt2rgba_frag_shader
#define VERTEX_SHADER_SOURCE_USERFILTER			userfilter_vertex_shader
#define FRAGMENT_SHADER_SOURCE_USERFILTER		userfilter_frag_shader
#define VERTEX_SHADER_SOURCE_DISPLAY			display_vertex_shader
#define FRAGMENT_SHADER_SOURCE_DISPLAY			display_frag_shader
#define VERTEX_SHADER_SOURCE_SCALE_RGBA			scale_rgba_vertex_shader
#define FRAGMENT_SHADER_SOURCE_SCALE_RGBA		scale_rgba_frag_shader
#define VERTEX_SHADER_SOURCE_YUVFILTER_Y		yuvfilter_vertex_shader_y
#define FRAGMENT_SHADER_SOURCE_YUVFILTER_Y		yuvfilter_frag_shader_y
#define VERTEX_SHADER_SOURCE_YUVFILTER_UV		yuvfilter_vertex_shader_uv
#define FRAGMENT_SHADER_SOURCE_YUVFILTER_UV		yuvfilter_frag_shader_uv

extern const char deinterace_vertex_shader[];
extern const char deinterace_frag_shader[];
extern const char scaler_vertex_shader[];
extern const char scaler_frag_shader[];
extern const char cvt2y_vertex_shader[];
extern const char cvt2y_frag_shader[];
extern const char cvt2uv_vertex_shader[];
extern const char cvt2uv_frag_shader[];
extern const char cvt2rgba_vertex_shader[];
extern const char cvt2rgba_frag_shader[];
extern const char userfilter_vertex_shader[];
extern const char userfilter_frag_shader[];
extern const char yuvfilter_vertex_shader_y[];
extern const char yuvfilter_frag_shader_y[];
extern const char yuvfilter_vertex_shader_uv[];
extern const char yuvfilter_frag_shader_uv[];
extern const char display_vertex_shader[];
extern const char display_frag_shader[];
extern const char scale_rgba_vertex_shader[];
extern const char scale_rgba_frag_shader[];

typedef struct vrSurfaceTarget* HSURFTARGET;
typedef struct vrSurfaceSource* HSURFSOURCE;
typedef struct vrSurfaceBoundTarget* HSURFBOUNDTARGET;
/*
	input   : Y, Y4���� �Ѳ�����
	output : Y
*/
#ifdef VR_FEATURE_SEPERATE_FD_USE
HSURFTARGET nxGSurfaceCreateDeinterlaceTarget  (unsigned int uiWidth, unsigned int uiHeight, NX_MEMORY_HANDLE hData, int iIsDefault );
HSURFSOURCE nxGSurfaceCreateDeinterlaceSource  (unsigned int uiWidth, unsigned int uiHeight, NX_MEMORY_HANDLE hData);
void        nxGSurfaceRunDeinterlace           ( HSURFTARGET hTarget, HSURFSOURCE hSource);
void        nxGSurfaceDestroyDeinterlaceTarget ( HSURFTARGET hTarget, int iIsDefault );
void        nxGSurfaceDestroyDeinterlaceSource ( HSURFSOURCE hSource );
#else
HSURFTARGET nxGSurfaceCreateDeinterlaceTarget  (unsigned int uiStride, unsigned int uiWidth, unsigned int uiHeight, NX_MEMORY_HANDLE hData, int iIsDefault );
HSURFSOURCE nxGSurfaceCreateDeinterlaceSource  (unsigned int uiStride, unsigned int uiWidth, unsigned int uiHeight, NX_MEMORY_HANDLE hData);
void        nxGSurfaceRunDeinterlace           ( HSURFTARGET hTarget, HSURFSOURCE hSource);
void        nxGSurfaceDestroyDeinterlaceTarget ( HSURFTARGET hTarget, int iIsDefault );
void        nxGSurfaceDestroyDeinterlaceSource ( HSURFSOURCE hSource );
#endif

/*
	input   : YUV
	output : YUV
*/
#ifdef VR_FEATURE_SEPERATE_FD_USE
int 	    nxGSurfaceCreateScaleTarget  ( HSURFTARGET* ptarget, unsigned int uiWidth, unsigned int uiHeight, NX_MEMORY_HANDLE* pData, int iIsDefault);
int         nxGSurfaceCreateScaleSource  ( HSURFSOURCE* psource, unsigned int uiWidth, unsigned int uiHeight, NX_MEMORY_HANDLE* pData );
void        nxGSurfaceRunScale           	( HSURFTARGET* ptarget, HSURFSOURCE* psource );
void        nxGSurfaceDestroyScaleTarget ( HSURFTARGET* ptarget, int iIsDefault );
void        nxGSurfaceDestroyScaleSource ( HSURFSOURCE* psource );
#else
HSURFTARGET nxGSurfaceCreateScaleTarget  (unsigned int uiStride, unsigned int uiWidth, unsigned int uiHeight, NX_MEMORY_HANDLE hData, int iIsDefault );
HSURFSOURCE nxGSurfaceCreateScaleSource  (unsigned int uiStride, unsigned int uiWidth, unsigned int uiHeight, NX_MEMORY_HANDLE hData );
void        nxGSurfaceRunScale           	      ( HSURFTARGET hTarget, HSURFSOURCE hSource );
void        nxGSurfaceDestroyScaleTarget ( HSURFTARGET hTarget, int iIsDefault );
void        nxGSurfaceDestroyScaleSource ( HSURFSOURCE hSource );
#endif

#ifdef VR_FEATURE_SEPERATE_FD_USE
HSURFTARGET nxGSurfaceCreateCvt2YTarget  (unsigned int uiWidth, unsigned int uiHeight, NX_MEMORY_HANDLE hData, int iIsDefault);
HSURFSOURCE nxGSurfaceCreateCvt2YSource  (unsigned int uiWidth, unsigned int uiHeight, NX_MEMORY_HANDLE hData);
void        nxGSurfaceRunCvt2Y           	      ( HSURFTARGET hTarget, HSURFSOURCE hSource);
void        nxGSurfaceDestroyCvt2YTarget ( HSURFTARGET hTarget, int iIsDefault );
void        nxGSurfaceDestroyCvt2YSource ( HSURFSOURCE hSource );

HSURFTARGET nxGSurfaceCreateCvt2UVTarget  (unsigned int uiWidth, unsigned int uiHeight, NX_MEMORY_HANDLE hData, int iIsDefault);
HSURFSOURCE nxGSurfaceCreateCvt2UVSource  (unsigned int uiWidth, unsigned int uiHeight, NX_MEMORY_HANDLE hData);
void        nxGSurfaceRunCvt2UV           	      ( HSURFTARGET hTarget, HSURFSOURCE hSource);
void        nxGSurfaceDestroyCvt2UVTarget ( HSURFTARGET hTarget, int iIsDefault );
void        nxGSurfaceDestroyCvt2UVSource ( HSURFSOURCE hSource );
#else
HSURFTARGET nxGSurfaceCreateCvt2YuvTarget  (unsigned int uiStride, unsigned int uiWidth, unsigned int uiHeight, NX_MEMORY_HANDLE hData, int iIsDefault);
HSURFSOURCE nxGSurfaceCreateCvt2YuvSource  (unsigned int uiStride, unsigned int uiWidth, unsigned int uiHeight, NX_MEMORY_HANDLE hData);
void        nxGSurfaceRunCvt2Yuv           	      ( HSURFTARGET hTarget, HSURFSOURCE hSource);
void        nxGSurfaceDestroyCvt2YuvTarget ( HSURFTARGET hTarget, int iIsDefault );
void        nxGSurfaceDestroyCvt2YuvSource ( HSURFSOURCE hSource );
#endif

#ifdef VR_FEATURE_SEPERATE_FD_USE
HSURFTARGET nxGSurfaceCreateCvt2RgbaTarget  (unsigned int uiWidth, unsigned int uiHeight, NX_MEMORY_HANDLE hData, int iIsDefault);
HSURFSOURCE nxGSurfaceCreateCvt2RgbaSource  (unsigned int uiWidth, unsigned int uiHeight, NX_MEMORY_HANDLE DataY , NX_MEMORY_HANDLE DataU, NX_MEMORY_HANDLE DataV );
#else
HSURFTARGET nxGSurfaceCreateCvt2RgbaTarget  (unsigned int uiStride, unsigned int uiWidth, unsigned int uiHeight, NX_MEMORY_HANDLE hData, int iIsDefault);
HSURFSOURCE nxGSurfaceCreateCvt2RgbaSource  (unsigned int uiStride, unsigned int uiWidth, unsigned int uiHeight, NX_MEMORY_HANDLE hData);
#endif
void        nxGSurfaceRunCvt2Rgba           	( HSURFTARGET hTarget, HSURFSOURCE hSource);
void        nxGSurfaceDestroyCvt2RgbaTarget ( HSURFTARGET hTarget, int iIsDefault );
void        nxGSurfaceDestroyCvt2RgbaSource ( HSURFSOURCE hSource );

HSURFTARGET nxGSurfaceCreateUserFilterTarget  (unsigned int uiStride, unsigned int uiWidth, unsigned int uiHeight, NX_MEMORY_HANDLE hData, int iIsDefault );
HSURFSOURCE nxGSurfaceCreateUserFilterSource  (unsigned int uiStride, unsigned int uiWidth, unsigned int uiHeight, NX_MEMORY_HANDLE hData);
void        nxGSurfaceRunUserFilter           ( HSURFTARGET hTarget, HSURFSOURCE hSource);
void        nxGSurfaceDestroyUserFilterTarget ( HSURFTARGET hTarget, int iIsDefault );
void        nxGSurfaceDestroyUserFilterSource ( HSURFSOURCE hSource );

#ifdef VR_FEATURE_SEPERATE_FD_USE
int 	    nxGSurfaceCreateYuvFilterTarget  ( HSURFTARGET* ptarget, unsigned int uiWidth, unsigned int uiHeight, NX_MEMORY_HANDLE* pData, int iIsDefault);
int         nxGSurfaceCreateYuvFilterSource  ( HSURFSOURCE* psource, unsigned int uiWidth, unsigned int uiHeight, NX_MEMORY_HANDLE* pData );
void        nxGSurfaceRunYuvFilter           	( HSURFTARGET* ptarget, HSURFSOURCE* psource, float edgeParam );
void        nxGSurfaceDestroyYuvFilterTarget ( HSURFTARGET* ptarget, int iIsDefault );
void        nxGSurfaceDestroyYuvFilterSource ( HSURFSOURCE* psource );
#else
HSURFTARGET nxGSurfaceCreateYuvFilterTarget  (unsigned int uiStride, unsigned int uiWidth, unsigned int uiHeight, NX_MEMORY_HANDLE hData, int iIsDefault );
HSURFSOURCE nxGSurfaceCreateYuvFilterSource  (unsigned int uiStride, unsigned int uiWidth, unsigned int uiHeight, NX_MEMORY_HANDLE hData );
void        nxGSurfaceRunYuvFilter           	     ( HSURFTARGET hTarget, HSURFSOURCE hSource, float edgeParam );
void        nxGSurfaceDestroyYuvFilterTarget ( HSURFTARGET hTarget, int iIsDefault );
void        nxGSurfaceDestroyYuvFilterSource ( HSURFSOURCE hSource );
#endif

HSURFTARGET nxGSurfaceCreateScaleRGBATarget  (unsigned int uiStride, unsigned int uiWidth, unsigned int uiHeight, NX_MEMORY_HANDLE hData, int iIsDefault );
HSURFSOURCE nxGSurfaceCreateScaleRGBASource  (unsigned int uiStride, unsigned int uiWidth, unsigned int uiHeight, NX_MEMORY_HANDLE hData);
void        nxGSurfaceRunScaleRGBA           ( HSURFTARGET hTarget, HSURFSOURCE hSource);
void        nxGSurfaceDestroyScaleRGBATarget ( HSURFTARGET hTarget, int iIsDefault );
void        nxGSurfaceDestroyScaleRGBASource ( HSURFSOURCE hSource );


/* for display */
int 		vrGSurfaceInitDisplay(unsigned int uiWidth, unsigned int uiHeight);
void 		nxGSurfaceRunDisplay(unsigned int uiDstX, unsigned int uiDstY, unsigned int uiDstWidth, unsigned int uiDstHeight, 
						unsigned int uiSrcWidth, unsigned int uiSrcHeight, void* pdata);
void 		nxGSurfaceRunStatusBar(int status_value, float start_x, float end_x);						
void 		vrGSurfaceSwapDisplay(void);
void 		vrGSurfaceDeinitDisplay(void);


EGLBoolean nxGSurfaceCreateGraphicSurface( EGLDisplay display );
void  nxGSurfaceDestroyGraphicSurface( void );

/* for native : User implementation Functions */
EGLNativeDisplayType vrPlatformGetNativeDispay(void);
EGLNativeWindowType vrPlatformCreateNativeWindow(unsigned int uiWidth, unsigned int uiHeight);
void vrPlatformDestroyNativeWindow(EGLNativeWindowType platform_win);

#endif  /* __VR_DEINTERACE__ */

