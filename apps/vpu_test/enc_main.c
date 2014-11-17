#include <stdio.h>
#include <string.h>

#include <nx_fourcc.h>

#include <nx_vip.h>			//	VIP
#include <nx_dsp.h>		//	Display
#include "nx_video_api.h"	//	Video En/Decoder
#include "queue.h"

#define	MAX_SEQ_BUF_SIZE		(4*1024)

#define	MAX_ENC_BUFFER			8

#define	NV12_MEM_TEST

#include <sys/time.h>
static uint64_t NX_GetTickCount( void )
{
	uint64_t ret;
	struct timeval	tv;
	struct timezone	zv;
	gettimeofday( &tv, &zv );
	ret = ((uint64_t)tv.tv_sec)*1000 + tv.tv_usec/1000;
	return ret;
}

static void dumpdata( void *data, int len, const char *msg )
{
	int i=0;
	unsigned char *byte = (unsigned char *)data;
	printf("Dump Data : %s", msg);
	for( i=0 ; i<len ; i ++ )
	{
		if( i!=0 && i%16 == 0 )	printf("\n\t");
		printf("%.2x", byte[i] );
		if( i%4 == 3 ) printf(" ");
	}
	printf("\n");
}


int main( int argc, char *argv[] )
{
	int i;
	int inWidth, inHeight;				//	Sensor Input Image Width & Height
	int cropX=0, cropY=0, cropW, cropH;		//	Clipper Output Information
	int frameCnt = 0;
	FILE *fdOut = NULL;

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
#ifdef NV12_MEM_TEST
	NX_VID_MEMORY_INFO *pNV12Mem = NULL;
#endif
	//	Current Vip Buffer
	NX_VID_MEMORY_INFO *pCurCapturedBuf = NULL;
	NX_VID_MEMORY_INFO *pTmpMem = NULL;

	//	Encoder Parameters
	NX_VID_ENC_INIT_PARAM encInitParam;
	unsigned char *seqBuffer = (unsigned char *)malloc( MAX_SEQ_BUF_SIZE );
	NX_VID_ENC_HANDLE hEnc;
	NX_VID_ENC_IN encIn;
	NX_VID_ENC_OUT encOut;

	long long totalSize = 0;
	double bitRate = 0.;
	long long vipTimeStamp;

	int instanceIdx;

	//	Set Image & Clipper Information
	inWidth = 1024;
	inHeight = 768;
	cropX = 0;
	cropY = 0;
	cropW = 1024;
	cropH = 768;


	//	Initialze Memory Queue
	NX_InitQueue( &memQueue, MAX_ENC_BUFFER );
	//	Allocate Memory
	for( i=0; i<MAX_ENC_BUFFER ; i++ )
	{
		hMem[i] = NX_VideoAllocateMemory( 4096, cropW, cropH, NX_MEM_MAP_LINEAR, FOURCC_MVS0 );
		NX_PushQueue( &memQueue, hMem[i] );
	}

	memset( &vipInfo, 0, sizeof(vipInfo) );

	vipInfo.port = 2;
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


	//	Output
	if( argc > 1 )
	{
		fdOut = fopen( argv[1], "wb" );
	}


	//	Initailize VIP & Display
	dspInfo.port = 0;
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
	NX_DspVideoSetPriority(hDsp, 0);
	hVip = NX_VipInit(&vipInfo);

	//	Initialize Encoder
	hEnc = NX_VidEncOpen( NX_AVC_ENC,  &instanceIdx);

	memset( &encInitParam, 0, sizeof(encInitParam) );
	encInitParam.width = cropW;
	encInitParam.height = cropH;
	encInitParam.gopSize = 30/2;
	encInitParam.bitrate = 3000000;
	encInitParam.fpsNum = 30;
	encInitParam.fpsDen = 1;

#ifdef NV12_MEM_TEST
	encInitParam.chromaInterleave = 1;
#else
	encInitParam.chromaInterleave = 0;
#endif

	//	Rate Control
	encInitParam.enableRC = 1;		//	Enable Rate Control
	encInitParam.disableSkip = 0;	//	Enable Skip
	encInitParam.maximumQp = 51;	//	Max Qunatization Scale
	encInitParam.initialQp = 10;	//	Default Encoder API ( enableRC == 0 )
	encInitParam.enableAUDelimiter = 1;	//	Enable / Disable AU Delimiter

	NX_VidEncInit( hEnc, &encInitParam );

	if( fdOut )
	{
		int size;
		//	Write Sequence Data
		NX_VidEncGetSeqInfo( hEnc, seqBuffer, &size );
		fwrite( seqBuffer, 1, size, fdOut );
		dumpdata( seqBuffer, size, "sps pps" );
		printf("Encoder Out Size = %d\n", size);
	}

#ifdef NV12_MEM_TEST
	pNV12Mem = NX_VideoAllocateMemory( 4096, cropW, cropH, NX_MEM_MAP_LINEAR, FOURCC_NV12 );
#endif

	//	PopQueue
	NX_PopQueue( &memQueue, (void**)&pTmpMem );
	NX_VipQueueBuffer( hVip, pTmpMem );

	while(1)
	{
		NX_PopQueue( &memQueue, (void**)&pTmpMem );
		NX_VipQueueBuffer( hVip, pTmpMem );

		NX_VipDequeueBuffer( hVip, &pCurCapturedBuf, &vipTimeStamp );
		NX_DspQueueBuffer( hDsp, pCurCapturedBuf );

		if( pPrevDsp )
		{
			NX_DspDequeueBuffer( hDsp );

#ifdef NV12_MEM_TEST
			if( pNV12Mem )
			{
				int j;
				unsigned char *cbcr =(unsigned char*)pNV12Mem->cbVirAddr;
				unsigned char *cb   =(unsigned char*)pPrevDsp->cbVirAddr;
				unsigned char *cr   =(unsigned char*)pPrevDsp->crVirAddr;
				//	Copy
				memcpy( (unsigned char*)pNV12Mem->luVirAddr, (unsigned char*)pPrevDsp->luVirAddr, cropW*cropH );
				for( i=0 ; i<cropH/2 ; i++ )
				{
					for( j=0 ; j<cropW/2 ; j++ )
					{
						*cbcr++ = *cb++;
						*cbcr++ = *cr++;
					}
				}

				encIn.pImage = pNV12Mem;
			}
			else
#endif
			{
				encIn.pImage = pPrevDsp;
			}

			encIn.timeStamp = 0;
			encIn.forcedIFrame = 0;
			encIn.forcedSkipFrame = 0;
			encIn.quantParam = 25;

			NX_VidEncEncodeFrame( hEnc, &encIn, &encOut );

			if( fdOut && encOut.bufSize>0 )
			{
				//	Write Sequence Data
				fwrite( encOut.outBuf, 1, encOut.bufSize, fdOut );
				printf("FrameType = %d, size = %8d, ", encOut.frameType, encOut.bufSize);
				dumpdata( encOut.outBuf, 16, "" );
				totalSize += encOut.bufSize;
				bitRate = (double)totalSize/(double)frameCnt*.8;
				printf("bitRate = %4.3f kbps\n", bitRate*30/1024.);
			}

			//if( frameCnt > 1000 )
			//{
			//	break;
			//}

			NX_PushQueue( &memQueue, pPrevDsp );
		}
		pPrevDsp = pCurCapturedBuf;
		frameCnt ++;
	}

	if( fdOut )
	{
		fclose( fdOut );
	}

	NX_DspClose( hDsp );
	NX_VipClose( hVip );

	return 0;
}

