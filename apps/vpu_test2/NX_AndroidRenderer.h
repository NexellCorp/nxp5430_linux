#ifndef __NX_ANDROIDRENDERER_H__
#define	__NX_ANDROIDRENDERER_H__

#include <nx_fourcc.h>
#include <nx_alloc_mem.h>

#include <gui/Surface.h>
#include <gui/SurfaceComposerClient.h>
#include <cutils/log.h>

#define	MAX_NUMBER_MEMORY	32

using namespace android;

class CNX_AndroidRenderer
{
public:
	CNX_AndroidRenderer( int32_t iWndWidth, int32_t iWndHeight );
	virtual ~CNX_AndroidRenderer();

	int32_t GetBuffers( int32_t iNumBuf, int32_t iImgWidth, int32_t iImgHeight, int32_t iDspWidth, int32_t iDspHeight, NX_VID_MEMORY_HANDLE **pMemHandle );
	int32_t DspQueueBuffer( NX_VID_MEMORY_HANDLE hHandle, int32_t index );
	int32_t DspDequeueBuffer( NX_VID_MEMORY_HANDLE *hHandle, int32_t *index );

private:
	sp<SurfaceComposerClient> 	m_SurfComClient;		//	Surface Composer Client
	sp<SurfaceControl> 			m_YuvSurfaceControl;	//	YUV Surface Controller
	sp<ANativeWindow>			m_YuvWindow;			//	YUV Window

	int32_t						m_WndWidth, m_WndHeight;	//	Window's width & height.
	int32_t						m_ImgWidth, m_ImgHeight;	//	YUV Image's width & height.
	int32_t						m_NumBuffers;				//	Number of Buffers.

	ANativeWindowBuffer			*m_pYuvANBuffer[MAX_NUMBER_MEMORY];
	NX_VID_MEMORY_HANDLE		m_MemoryHandles[MAX_NUMBER_MEMORY];

	bool						m_bFirst;
};

#endif	// __NX_ANDROIDRENDERER_H__
