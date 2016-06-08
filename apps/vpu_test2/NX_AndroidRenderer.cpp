#include <gui/Surface.h>
#include <gui/SurfaceComposerClient.h>
#include <cutils/log.h>

#include <gralloc_priv.h>	//	private_handle_t
#include <ion/ion.h>		//	ion_open
#include <ion-private.h>	//	ion_get_phys
#include <nexell_format.h>	//	ALIGN

#include "NX_AndroidRenderer.h"

static int32_t ConvertPrivateHandleVideoMemory( struct private_handle_t const *handle, NX_VID_MEMORY_INFO *memInfo )
{
	int ion_fd = ion_open();
    int ret = ion_get_phys(ion_fd, handle->share_fd, (long unsigned int *)&memInfo->luPhyAddr);
	int vstride = ALIGN(handle->height, 16);

	((NX_MEMORY_HANDLE)memInfo->privateDesc[0])->privateDesc = (void*)handle->share_fd;
	memInfo->fourCC    = FOURCC_MVS0;
	memInfo->imgWidth  = handle->width;
	memInfo->imgHeight = handle->height;
	memInfo->cbPhyAddr = memInfo->luPhyAddr + handle->stride * vstride;
	memInfo->crPhyAddr = memInfo->cbPhyAddr + ALIGN(handle->stride>>1,16) * ALIGN(vstride>>1,16);
	memInfo->luStride  = handle->stride;
	memInfo->cbStride  =
	memInfo->crStride  = handle->stride >> 1;;

	memInfo->luVirAddr = (unsigned int)mmap(NULL, handle->stride * vstride * 3 / 2, PROT_READ|PROT_WRITE, MAP_SHARED, handle->share_fd, 0);
	memInfo->cbVirAddr = memInfo->luVirAddr + handle->stride * vstride;
	memInfo->crVirAddr = memInfo->cbVirAddr + ALIGN(handle->stride>>1,16) * ALIGN(vstride>>1,16);

	close( ion_fd );
	return 0;
}

CNX_AndroidRenderer::CNX_AndroidRenderer( int32_t iWndWidth, int32_t iWndHeight )
	: m_WndWidth(0), m_WndHeight(0)
	, m_ImgWidth(0), m_ImgHeight(0)
	, m_bFirst(false)
{
	int32_t err;
	sp<SurfaceComposerClient> surfComClient = new SurfaceComposerClient();
	sp<SurfaceControl> yuvSurfaceControl =
		surfComClient->createSurface(String8("YUV Surface"), iWndWidth, iWndHeight, HAL_PIXEL_FORMAT_YV12, ISurfaceComposerClient::eFXSurfaceNormal);
	if (yuvSurfaceControl == NULL) {
		printf("failed to create yuv surface!!!");
		exit(-1);
	}
	SurfaceComposerClient::openGlobalTransaction();
	yuvSurfaceControl->show();
	yuvSurfaceControl->setLayer(99999);
	SurfaceComposerClient::closeGlobalTransaction();

	m_NumBuffers = 0;
	for( int32_t i=0 ; i<MAX_NUMBER_MEMORY ; i++ )
	{
		m_pYuvANBuffer[i] = NULL;
		m_MemoryHandles[i] = NULL;
	}

	m_SurfComClient = surfComClient;
	m_YuvSurfaceControl = yuvSurfaceControl;
}

CNX_AndroidRenderer::~CNX_AndroidRenderer()
{
}

int32_t CNX_AndroidRenderer::GetBuffers( int32_t iNumBuf, int32_t iImgWidth, int32_t iImgHeight, NX_VID_MEMORY_HANDLE **pMemHandle )
{
	int32_t err;
	sp<ANativeWindow> yuvWindow(m_YuvSurfaceControl->getSurface());
	err = native_window_set_buffer_count(yuvWindow.get(), iNumBuf);
	if (err) {
		printf("failed to yuv native_window_set_buffer_count!!!\n");
		return -1;
	}
	err = native_window_set_scaling_mode(yuvWindow.get(), NATIVE_WINDOW_SCALING_MODE_SCALE_TO_WINDOW);
	if (err) {
		printf("failed to yuv native_window_set_scaling_mode!!!\n");
		return -1;
	}
	err = native_window_set_usage(yuvWindow.get(), GRALLOC_USAGE_HW_CAMERA_WRITE);
	if (err) {
		printf("failed to yuv native_window_set_usage!!!\n");
		return -1;
	}
	err = native_window_set_buffers_geometry(yuvWindow.get(), iImgWidth, iImgHeight, HAL_PIXEL_FORMAT_YV12);
	if (err) {
		printf("failed to yuv native_window_set_buffers_geometry!!!\n");
		return -1;
	}

	for (int i = 0; i < iNumBuf; i++)
	{
		err = native_window_dequeue_buffer_and_wait(yuvWindow.get(), &m_pYuvANBuffer[i]);
		if (err) {
			printf("failed to yuv dequeue buffer..\n");
			return -1;
		}

		private_handle_t const *yuvHandle = reinterpret_cast<private_handle_t const *>(m_pYuvANBuffer[i]->handle);

		//	Make Dummy NX_VID_MEMORY_HANDLE
		{
			m_MemoryHandles[i] = (NX_VID_MEMORY_HANDLE)calloc(1, sizeof(NX_VID_MEMORY_INFO));
			m_MemoryHandles[i]->privateDesc[0] = calloc(1, sizeof(NX_MEMORY_INFO));
			m_MemoryHandles[i]->privateDesc[1] = calloc(1, sizeof(NX_MEMORY_INFO));
			m_MemoryHandles[i]->privateDesc[2] = calloc(1, sizeof(NX_MEMORY_INFO));

		}
		ConvertPrivateHandleVideoMemory(yuvHandle, m_MemoryHandles[i] );
	}
	*pMemHandle = &m_MemoryHandles[0];
	m_YuvWindow = yuvWindow;
	return 0;
}

int32_t CNX_AndroidRenderer::DspQueueBuffer( NX_VID_MEMORY_HANDLE hHandle, int32_t index )
{
	(void) hHandle;
	m_YuvWindow->queueBuffer(m_YuvWindow.get(), m_pYuvANBuffer[index], -1);
	return 0;
}

int32_t CNX_AndroidRenderer::DspDequeueBuffer( NX_VID_MEMORY_HANDLE *hHandle, int32_t *index )
{
    ANativeWindowBuffer *yuvTempBuffer;
	(void) hHandle;
	(void) index;
	native_window_dequeue_buffer_and_wait(m_YuvWindow.get(), &yuvTempBuffer);
	return 0;
}
