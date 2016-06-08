#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include <nx_fourcc.h>

#include <nx_vip.h>			//	VIP
#include <nx_dsp.h>		//	Display
#include "nx_video_api.h"	//	Video En/Decoder
#include "NX_Queue.h"
#include "Util.h"

#define	MAX_JPEG_HEADER_SIZE	(4*1024)

#define	MAX_ENC_BUFFER			8

//#define	DISPLAY_ONLY

#define	IMAGE_WIDTH		800*2
#define	IMAGE_HEIGHT	600*2
//#define	IMAGE_STRIDE	(((IMAGE_WIDTH+63)>>6)<<6)		//	Interlace Camera
#define	IMAGE_STRIDE	(((IMAGE_WIDTH+15)>>4)<<4)			//	Encoder Needs 16 Aligned Memory


//int32_t VpuJpgMain( const char *pOutFile, int32_t quality, int32_t angle )
int32_t VpuJpgMain( CODEC_APP_DATA *pAppData )
{
	int32_t i;
	int32_t inWidth, inHeight;				//	Sensor Input Image Width & Height
	int32_t cropX, cropY, cropW, cropH;		//	Clipper Output Information
	int32_t frameCnt = 0;
	FILE *fdOut = NULL;
	int32_t quality=pAppData->qp, angle = pAppData->angle;

	//	VIP
	VIP_HANDLE hVip;
	VIP_INFO vipInfo;
	//	Memory
	NX_VID_MEMORY_HANDLE hMem[MAX_ENC_BUFFER];
	//	Display
	DISPLAY_HANDLE hDsp;
	NX_QUEUE memQueue;
	DISPLAY_INFO dspInfo;
	//	Previous Displayed Memory
	NX_VID_MEMORY_INFO *pPrevDsp = NULL;
	//	Current Vip Buffer
	NX_VID_MEMORY_INFO *pCurCapturedBuf = NULL;
	NX_VID_MEMORY_INFO *pTmpMem = NULL;

	//	Encoder Parameters
	NX_VID_ENC_INIT_PARAM encInitParam;
	int32_t captureFrameNum = 10;
	unsigned char *jpgHeader = (unsigned char *)malloc( MAX_JPEG_HEADER_SIZE );
	NX_VID_ENC_HANDLE hEnc;
	NX_VID_ENC_OUT encOut;
	int32_t instanceIdx;

	long long vipTime;

	if( !pAppData->outFileName )
	{
		printf("Invalid output file name !!!\n");
		return -1;
	}

	//	Set Image & Clipper Information
	inWidth = IMAGE_WIDTH;
	inHeight = IMAGE_HEIGHT;
	cropX = 0;
	cropY = 0;
	cropW = IMAGE_WIDTH;
	cropH = IMAGE_HEIGHT;

	//	Initialze Memory Queue
	NX_InitQueue( &memQueue, MAX_ENC_BUFFER );
	//	Allocate Memory
	for( i=0; i<MAX_ENC_BUFFER ; i++ )
	{
		hMem[i] = NX_VideoAllocateMemory( 4096, IMAGE_STRIDE, IMAGE_WIDTH, NX_MEM_MAP_LINEAR, FOURCC_MVS0 );
		NX_PushQueue( &memQueue, hMem[i] );
	}

	memset( &vipInfo, 0, sizeof(vipInfo) );

	vipInfo.port = 0;
	vipInfo.mode = VIP_MODE_CLIPPER;

	//	Sensor Input Size
	vipInfo.width = inWidth;
	vipInfo.height = inHeight;
	vipInfo.numPlane = 1;

	//	Clipper Setting
	vipInfo.cropX = cropX;
	vipInfo.cropY = cropY;
	vipInfo.cropWidth  = cropW;
	vipInfo.cropHeight = cropH;

	//	Fps
	vipInfo.fpsNum = 30;
	vipInfo.fpsDen = 1;


	fdOut = fopen( pAppData->outFileName, "wb" );

	//	Initailize VIP & Display
	dspInfo.port = DISPLAY_PORT_LCD;
//	dspInfo.port = DISPLAY_PORT_HDMI;
	dspInfo.module = 0;
	dspInfo.width = cropW;
	dspInfo.height = cropH;
	dspInfo.numPlane = 1;

	dspInfo.dspSrcRect.left = 0;
	dspInfo.dspSrcRect.top = 0;
	dspInfo.dspSrcRect.right = cropW;
	dspInfo.dspSrcRect.bottom = cropH;

	dspInfo.dspDstRect.left = 0;
	dspInfo.dspDstRect.top = 0;
	dspInfo.dspDstRect.right = cropW;
	dspInfo.dspDstRect.bottom = cropH;

	hDsp = NX_DspInit( &dspInfo );
	hVip = NX_VipInit( &vipInfo );

#ifndef	DISPLAY_ONLY
	//	Initialize Encoder
	hEnc = NX_VidEncOpen( NX_JPEG_ENC, &instanceIdx );

	memset( &encInitParam, 0, sizeof(encInitParam) );
	encInitParam.width = cropW;
	encInitParam.height = cropH;
	encInitParam.rotAngle = angle;
	encInitParam.mirDirection = 0;
	encInitParam.jpgQuality = quality;

	if( NX_VidEncInit( hEnc, &encInitParam ) != 0 )
	{
		printf("NX_VidEncInit(failed)!!!");
		exit(-1);
	}

	if( fdOut )
	{
		int size;
		//	Write Sequence Data
		NX_VidEncJpegGetHeader( hEnc, jpgHeader, &size );
		if( size > 0 )
		{
			fwrite( jpgHeader, 1, size, fdOut );
		}
	}
#endif	//	DISPLAY_ONLY

	//	PopQueue
	NX_PopQueue( &memQueue, (void**)&pTmpMem );
	NX_VipQueueBuffer( hVip, pTmpMem );

	while(1)
	{
		NX_PopQueue( &memQueue, (void**)&pTmpMem );
		NX_VipQueueBuffer( hVip, pTmpMem );

		NX_VipDequeueBuffer( hVip, &pCurCapturedBuf, &vipTime );

		NX_DspQueueBuffer( hDsp, pCurCapturedBuf );

		if( pPrevDsp )
		{
			NX_DspDequeueBuffer( hDsp );

			printf("frameCnt = %d\n", frameCnt);

#ifndef	DISPLAY_ONLY
			if( frameCnt == captureFrameNum )
			{
				printf("NX_VidEncJpegRunFrame ++\n");
				NX_VidEncJpegRunFrame( hEnc, pPrevDsp, &encOut );
				printf("NX_VidEncJpegRunFrame --\n");
				if( fdOut && encOut.bufSize>0 )
				{
					printf("encOut.bufSize = %d\n", encOut.bufSize);
					//	Write Sequence Data
					fwrite( encOut.outBuf, 1, encOut.bufSize, fdOut );
				}
				break;
			}
#endif	//	DISPLAY_ONLY

			NX_PushQueue( &memQueue, pPrevDsp );
		}
		pPrevDsp = pCurCapturedBuf;
		frameCnt ++;
	}

	if( fdOut )
	{
		printf("Close File Done\n");
		fclose( fdOut );
	}

#ifndef	DISPLAY_ONLY
	NX_DspClose( hDsp );
#endif	//	DISPLAY_ONLY

	NX_VipClose( hVip );

	return 0;
}

