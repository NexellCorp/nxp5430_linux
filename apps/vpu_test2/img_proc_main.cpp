#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/time.h>	//	gettimeofday

#include <ui/GraphicBuffer.h>
#include <gui/Surface.h>
#include <gui/ISurfaceComposer.h>
#include <gui/SurfaceComposerClient.h>

#include <cutils/log.h>

#include <nxp-v4l2.h>
#include <gralloc_priv.h>
#include <ion-private.h>
#include <nexell_format.h>
#include <nx_fourcc.h>
#include <nx_alloc_mem.h>

#include <nx_video_api.h>			//	For Video Decoding
#include <nx_graphictools.h>		//	For Deinterlace

#include "media_reader.h"
#include "codec_info.h"
#include "queue.h"
#include "NX_Semaphore.h"
#include "img_proc_main.h"

using namespace android;

#define	WINDOW_WIDTH		1280
#define	WINDOW_HEIGHT		800


//
//	Main Processing Flow :
//		Decoding Thread  -->  Image Processing Thread  -->  Encoding Thread
//
//

CImageProcessing::CImageProcessing()
	: m_pMediaReader(NULL)
	, m_hDec(0)
	, m_hDecThread(0)
	, m_hImgProcThread(0)
	, m_bExitDecThread(true)
	, m_bExitImgProcThread(true)
	, m_bStarted(false)
{
	m_pSemDec = NX_CreateSem(0, 1024);
	m_pSemImgProc = NX_CreateSem(0, 1024);

	NX_InitQueue( &m_tDecRelQueue, 64 );
	NX_InitQueue( &m_tImgInQueue, 64);
	NX_InitQueue( &m_tImgOutQueue, 64);

	pthread_mutex_init(&m_hCtrlMutex, NULL);
}

CImageProcessing::~CImageProcessing()
{
	if( m_pSemDec )
	{
		NX_DestroySem(m_pSemDec);
	}
	if( m_pSemImgProc )
	{
		NX_DestroySem(m_pSemImgProc);
	}
	pthread_mutex_destroy(&m_hCtrlMutex);
}


bool CImageProcessing::Start( const char *inFileName )
{
	pthread_mutex_lock(&m_hCtrlMutex);
	if( m_bStarted )
		goto ErrorExit;

	m_pMediaReader = new CMediaReader();
	if( !m_pMediaReader->OpenFile( inFileName ) )
	{
		printf("Cannot open media file(%s)\n", inFileName );
		exit(-1);
	}
	m_pMediaReader->GetVideoResolution(&m_InWidth, &m_InHeight);


	m_bExitDecThread = false;
	m_bExitImgProcThread = false;

	if( pthread_create( &m_hImgProcThread, NULL, ImgProcThreadStub, this ) < 0 )
	{
		printf("Cannot Create Image Processing Thread!!!\n");
		m_hImgProcThread = 0;
		goto ErrorExit;
	}
	
	if( pthread_create( &m_hDecThread, NULL, DecThreadStub, this ) < 0 )
	{
		printf("Cannot Create Decoding Thread!!!\n");
		m_hDecThread = 0;
		goto ErrorExit;
	}

	m_bStarted = true;
	pthread_mutex_unlock(&m_hCtrlMutex);
	return true;
ErrorExit:
	pthread_mutex_unlock(&m_hCtrlMutex);
	return false;
}


bool CImageProcessing::Stop()
{
	pthread_mutex_lock(&m_hCtrlMutex);
	if( m_bStarted )
	{
		if( 0 != m_hDecThread )
		{
			m_bExitDecThread = true;
			NX_PostSem(m_pSemDec);

			pthread_join( m_hDecThread, NULL );
			m_hDecThread = 0;
		}
		if( 0 != m_hImgProcThread )
		{
			m_bExitImgProcThread = true;
			NX_PostSem(m_pSemImgProc);
			pthread_join( m_hImgProcThread, NULL );
			m_hImgProcThread = 0;
		}
		m_bStarted = false;
	}

	if( m_pMediaReader )
	{
		delete m_pMediaReader;
		m_pMediaReader = NULL;
	}

	pthread_mutex_unlock(&m_hCtrlMutex);
	return true;
}

//
//	Decoding
void CImageProcessing::DecThread()
{
	int vpuCodecType;
	NX_VID_RET vidRet;
	NX_VID_SEQ_IN seqIn;
	NX_VID_SEQ_OUT seqOut;
	NX_VID_DEC_IN decIn;
	NX_VID_DEC_OUT decOut;
	int32_t seqSize = 0;
	int32_t bInit=0;
	int32_t readSize, frameCount = 0, outCount=0;
	int32_t prevIdx = -1;
	int32_t size, key = 0;
	long long timeStamp = -1, outTimeStamp = -1;
	int32_t needKey = 1;
	int32_t mp4Class=0;
	int32_t seqNeedMoreBuffer = 0;
	int32_t tmpSize;
	int32_t codecTag=-1, codecId=-1;
	int32_t isFirst = 1;
	int32_t imgWidth=-1, imgHeight=-1;
	NX_VID_DEC_OUT  decOutInfo[30];

	m_pMediaReader->GetCodecTagId( AVMEDIA_TYPE_VIDEO, &codecTag, &codecId  );
	vpuCodecType = CodecIdToVpuType(codecId, codecTag);
	mp4Class = fourCCToMp4Class( codecTag );
	if( mp4Class == -1 )
		mp4Class = codecIdToMp4Class( codecId );
	mp4Class = 0;
	printf("vpuCodecType = %d, mp4Class = %d\n", vpuCodecType, mp4Class );

	if( NULL == (m_hDec = NX_VidDecOpen(vpuCodecType, mp4Class, 0)) )
	{
		printf("NX_VidDecOpen(%d) failed!!!\n", vpuCodecType);
		exit(-1);
		return;
	}

	NX_PostSem(m_pSemDec);	//	Need Decoding
	seqSize = m_pMediaReader->GetVideoSeqInfo( m_StreamBuffer );

	while( !m_bExitDecThread )
	{
		//	Wait Buffer Status Change
		NX_PendSem(m_pSemDec);

		//	Check Output Buffer
		//	ReadStream
		if( 0 != m_pMediaReader->ReadStream( CMediaReader::MEDIA_TYPE_VIDEO, m_StreamBuffer+seqSize, &size, &key, &timeStamp ) )
		{
			break;
		}

		if( !bInit && !key )
		{
			NX_PostSem(m_pSemDec);
			continue;
		}

		if( !bInit )
		{
			memset( &seqIn, 0, sizeof(seqIn) );
			seqIn.seqInfo = m_StreamBuffer;
			seqIn.seqSize = seqSize + size;
			seqIn.enableUserData = 0;
			seqIn.disableOutReorder = 0;

			vidRet = NX_VidDecInit( m_hDec, &seqIn, &seqOut );
			if( vidRet == VID_NEED_MORE_BUF )
			{
				printf("VPU Initialize Failed!!!\n");
				break;
			}

			int outBufferableCnt = seqOut.numBuffers - seqOut.nimBuffers;
			for( int j=0; j<outBufferableCnt ; j++ )
			{
				NX_PostSem(m_pSemDec);
			}

			seqSize = 0;
			bInit = 1;
			size = 0;
		}

		memset(&decIn, 0, sizeof(decIn));
		decIn.strmBuf = m_StreamBuffer;
		decIn.strmSize = size;
		decIn.timeStamp = timeStamp;
		decIn.eos = 0;


		//	Clear All Display Memory
		vidRet = NX_VidDecDecodeFrame( m_hDec, &decIn, &decOut );
		if( vidRet == VID_NEED_MORE_BUF )
		{
			printf("VID_NEED_MORE_BUF NX_VidDecDecodeFrame\n");
			NX_PostSem(m_pSemDec);
			continue;
		}
		if( vidRet < 0 )
		{
			printf("Decoding Error!!!\n");
			break;
		}

		if( decOut.outImgIdx >= 0  )
		{
			if( prevIdx != -1 )
			{
			//	NX_VidDecClrDspFlag( m_hDec, &decOut.outImg, prevIdx );
			}
			prevIdx = decOut.outImgIdx;
			decOutInfo[decOut.outImgIdx] = decOut;

			//	Send To Image Process Queue
			NX_PushQueue(&m_tImgInQueue, (void*)&decOutInfo[decOut.outImgIdx]);		//	Input Index
			NX_PostSem(m_pSemImgProc);
		}
	}

	NX_VidDecClose(m_hDec);
	printf("Exit Decoding Thread Loop!!!\n");
}

//
//	Image Porcessing : Deinterlacing
//
#include <linux/media.h>
#include <linux/v4l2-subdev.h>
#include <linux/v4l2-mediabus.h>
#include <linux/videodev2.h>
#include <linux/videodev2_nxp_media.h>
#include <ion/ion.h>
#define	NUMBER_OF_OUT_BUFFER	10
void CImageProcessing::ImgProcThread()
{
	//
	//	Allocate Output YUV Buffer for Display
	//
	int32_t err;
    sp<SurfaceComposerClient> client = new SurfaceComposerClient();
    sp<SurfaceControl> yuvSurfaceControl = 
		client->createSurface(String8("YUV Surface"), WINDOW_WIDTH, WINDOW_HEIGHT, HAL_PIXEL_FORMAT_YV12, ISurfaceComposerClient::eFXSurfaceNormal);
    if (yuvSurfaceControl == NULL) {
        printf("failed to create yuv surface!!!");
        exit(-1);
    }
    SurfaceComposerClient::openGlobalTransaction();
    yuvSurfaceControl->show();
    yuvSurfaceControl->setLayer(99999);
    SurfaceComposerClient::closeGlobalTransaction();

	sp<ANativeWindow> yuvWindow(yuvSurfaceControl->getSurface());
    err = native_window_set_buffer_count(yuvWindow.get(), NUMBER_OF_OUT_BUFFER);
    if (err) {
        printf("failed to yuv native_window_set_buffer_count!!!\n");
        exit(-1);
    }
    err = native_window_set_scaling_mode(yuvWindow.get(), NATIVE_WINDOW_SCALING_MODE_SCALE_TO_WINDOW);
    if (err) {
        printf("failed to yuv native_window_set_scaling_mode!!!\n");
        exit(-1);
    }
    err = native_window_set_usage(yuvWindow.get(), GRALLOC_USAGE_HW_CAMERA_WRITE);
    if (err) {
        printf("failed to yuv native_window_set_usage!!!\n");
        exit(-1);
    }
    err = native_window_set_buffers_geometry(yuvWindow.get(), m_InWidth, m_InHeight, HAL_PIXEL_FORMAT_YV12);
    if (err) {
        printf("failed to yuv native_window_set_buffers_geometry!!!\n");
        exit(-1);
    }

    ANativeWindowBuffer *yuvANBuffer[NUMBER_OF_OUT_BUFFER];
	NX_VID_MEMORY_INFO	outMemoryInfo[NUMBER_OF_OUT_BUFFER];
	NX_MEMORY_INFO      memInfo[NUMBER_OF_OUT_BUFFER];
	memset(&outMemoryInfo[0], 0, sizeof(outMemoryInfo));
	memset(&memInfo[0], 0, sizeof(memInfo));

	//	Mapping Android Native Window Buffer to NX_VID_MEMORY_INFO
    for (int32_t i = 0; i < NUMBER_OF_OUT_BUFFER; i++)
    {
        err = native_window_dequeue_buffer_and_wait(yuvWindow.get(), &yuvANBuffer[i]);
        if (err) {
            printf("failed to yuv dequeue buffer..\n");
            exit(-1);
        }

		private_handle_t const *yuvHandle = reinterpret_cast<private_handle_t const *>(yuvANBuffer[i]->handle);
		int ion_fd = ion_open();
		int ret = ion_get_phys(ion_fd, yuvHandle->share_fd, (long unsigned int *)&outMemoryInfo[i].luPhyAddr);
		int vstride = ALIGN(yuvHandle->height, 16);

		memInfo[i].privateDesc = (void *)yuvHandle->share_fd;
		outMemoryInfo[i].privateDesc[0] = &memInfo[i];
		outMemoryInfo[i].fourCC    = FOURCC_MVS0;
		outMemoryInfo[i].imgWidth  = yuvHandle->width;
		outMemoryInfo[i].imgHeight = yuvHandle->height;
		outMemoryInfo[i].cbPhyAddr = outMemoryInfo[i].luPhyAddr + yuvHandle->stride * vstride;
		outMemoryInfo[i].crPhyAddr = outMemoryInfo[i].cbPhyAddr + ALIGN(yuvHandle->stride>>1,16) * ALIGN(vstride>>1,16);
		outMemoryInfo[i].luStride  = yuvHandle->stride;
		outMemoryInfo[i].cbStride  = 
		outMemoryInfo[i].crStride  = yuvHandle->stride >> 1;;
		close( ion_fd );
		NX_PushQueue( &m_tImgOutQueue, (void*)i );
    }


	//
	//	Main Deinterlace Operation
	//
	int32_t outImgIdx = -1;
	NX_VID_DEC_OUT *decOut;
	NX_GT_DEINT_HANDLE hDeint = NX_GTDeintOpen( ALIGN(m_InWidth,16), ALIGN(m_InHeight,16), 10 );
	int32_t pPrevDspIdx = -1;

	while( !m_bExitImgProcThread )
	{
		//	Wait Buffer Status Change
		NX_PendSem(m_pSemImgProc);

		ALOGD("In Count = %d, Out Count = %d\n", NX_GetQueueCnt(&m_tImgInQueue), NX_GetQueueCnt(&m_tImgOutQueue) );

		//	Check In/Output Buffer
		if( (NX_GetQueueCnt(&m_tImgInQueue) > 0) && (NX_GetQueueCnt(&m_tImgOutQueue) > 0) )
		{
			NX_PopQueue(&m_tImgInQueue, (void**)&decOut);		//	Input Index
			NX_PopQueue(&m_tImgOutQueue, (void**)&outImgIdx);	//	Output Index
			//	Image Processing
			NX_GTDeintDoDeinterlace( hDeint, &decOut->outImg, &outMemoryInfo[outImgIdx] );
			//	Return back deinterlace input buffer to video decoder.
			NX_VidDecClrDspFlag( m_hDec, &decOut->outImg, decOut->outImgIdx );
		}
		else
		{
			printf("Continue !!!!!, In Count = %d, out Count = %d\n", NX_GetQueueCnt(&m_tImgInQueue), NX_GetQueueCnt(&m_tImgOutQueue) );
			continue;
		}

		//	Display Buffer
		yuvWindow->queueBuffer(yuvWindow.get(), yuvANBuffer[outImgIdx], -1);
		if( pPrevDspIdx != -1 )
		{
			ANativeWindowBuffer *yuvTempBuffer;
#if 1
			int fenceFd;
			yuvWindow->dequeueBuffer(yuvWindow.get(), &yuvTempBuffer, &fenceFd);
#else
			native_window_dequeue_buffer_and_wait(yuvWindow.get(), &yuvTempBuffer);
#endif
			NX_PushQueue( &m_tImgOutQueue, (void*)pPrevDspIdx);
		}
		pPrevDspIdx = outImgIdx;
		NX_PostSem(m_pSemDec);	//	Send Semaphore to Decoding Thread
	}
	NX_GTDeintClose( hDeint );
	printf("Exit Image Processing Thread Loop!!!\n");
}


int img_proc_main( const char *fileName )
{
	CImageProcessing *pImgProc = new CImageProcessing;

	pImgProc->Start( fileName );

	while(1)
		usleep(10000000);

	pImgProc->Stop();
	return 0;
}
