#include <stdio.h>
#include <string.h>
#include <unistd.h>		//	getopt & optarg
#include <stdlib.h>		//	atoi
#include <sys/time.h>	//	gettimeofday

#include <nx_fourcc.h>
#include <nx_dsp.h>		//	Display

#include "nx_video_api.h"	//	Video En/Decoder
#include "queue.h"

#define	MAX_SEQ_BUF_SIZE		(4*1024)
#define	MAX_ENC_BUFFER			2

#define	ENABLE_NV12				1

static uint64_t NX_GetTickCount( void )
{
	uint64_t ret;
	struct timeval	tv;
	struct timezone	zv;
	gettimeofday( &tv, &zv );
	ret = ((uint64_t)tv.tv_sec)*1000000 + tv.tv_usec;
	return ret;
}

static void dumpdata( void *data, int32_t len, const char *msg )
{
	int32_t i=0;
	uint8_t *byte = (uint8_t *)data;
	printf("Dump Data : %s", msg);
	for( i=0 ; i<len ; i ++ )
	{
		if( i!=0 && i%16 == 0 )	printf("\n\t");
		printf("%.2x", byte[i] );
		if( i%4 == 3 ) printf(" ");
	}
	printf("\n");
}

//	Encoder Application Data
typedef struct tENC_APP_DATA {
	//	Input Options
	char *inFileName;			//	Input File Name
	int32_t	width;				//	Input YUV Image Width
	int32_t	height;				//	Input YUV Image Height
	int32_t fps;				//	Input Image Fps

	//	Output Options
	char *outFileName;			//	Output File Name
	char *outLogFileName;		//	Output Log File Name
	int32_t bitrate;			//	Bitrate
	int32_t gop;				//	GoP
	int32_t codec;				//	0:H.264, 1:Mp4v, 2:H.263, 3:JPEG (def:H.264)
	int32_t qp;					//	Fixed Qp

	//	Preview Options
	int32_t dspX;				//	Display X Axis Offset
	int32_t dspY;				//	Display Y Axis Offset
	int32_t	dspWidth;			//	Display Width
	int32_t dspHeight;			//	Dispplay Height
} ENC_APP_DATA;

//
//	Coda960 Performance Test Application
//
//	Application Sequence :
//
//	Step 1. Prepare Parameter
//	Step 2. Load YUV Image & Copy to Encoding Buffer
//	Step 3. Write Encoded Bitstream
//


//
//	pSrc : Y + U(Cb) + V(Cr) (IYUV format)
//
static int32_t LoadImage( uint8_t *pSrc, int32_t w, int32_t h, NX_VID_MEMORY_INFO *pImg )
{
	int32_t i, j;
	uint8_t *pDst, *pCb, *pCr;

	//	Copy Lu
	pDst = (uint8_t*)pImg->luVirAddr;
	for( i=0 ; i<h ; i++ )
	{
		memcpy(pDst, pSrc, w);
		pDst += pImg->luStride;
		pSrc += w;
	}

	pCb = pSrc;
	pCr = pSrc + w*h/4;


	switch( pImg->fourCC )
	{
		case FOURCC_NV12:
		{
			uint8_t *pCbCr;
			pDst = (uint8_t*)pImg->cbVirAddr;
			for( i=0 ; i<h/2 ; i++ )
			{
				pCbCr = pDst + pImg->cbStride*i;
				for( j=0 ; j<w/2 ; j++ )
				{
					*pCbCr++ = *pCb++;
					*pCbCr++ = *pCr++;
				}
			}
			break;
		}
		case FOURCC_NV21:
		{
			uint8_t *pCrCb;
			pDst = (uint8_t*)pImg->cbVirAddr;
			for( i=0 ; i<h/2 ; i++ )
			{
				pCrCb = pDst + pImg->cbStride*i;
				for( j=0 ; j<w/2 ; j++ )
				{
					*pCrCb++ = *pCr++;
					*pCrCb++ = *pCb++;
				}
			}
			break;
		}
		case FOURCC_MVS0:
		case FOURCC_YV12:
		case FOURCC_IYUV:
		{
			//	Cb
			pDst = (uint8_t*)pImg->cbVirAddr;
			for( i=0 ; i<h/2 ; i++ )
			{
				memcpy(pDst, pCb, w/2);
				pDst += pImg->cbStride;
				pCb += w/2;
			}

			//	Cr
			pDst = (uint8_t*)pImg->crVirAddr;
			for( i=0 ; i<h/2 ; i++ )
			{
				memcpy(pDst, pCr, w/2);
				pDst += pImg->crStride;
				pCr += w/2;
			}
			break;
		}
	}
	return 0;
}

int32_t performance_test( ENC_APP_DATA *pAppData )
{
	int32_t i;
	int32_t frameCnt = 0;
	FILE *fdOut = NULL, *fdIn=NULL, *fdLog=NULL;
	int inWidth, inHeight;				//	Sensor Input Image Width & Height
	uint8_t *pSrcBuf = (uint8_t*)malloc(pAppData->width*pAppData->height*3/2);
	uint64_t startTime, endTime, totalTime;
	int32_t instanceIdx;

	//	Memory
	NX_VID_MEMORY_HANDLE hMem[MAX_ENC_BUFFER];
	//	Display
	DISPLAY_HANDLE hDsp;
	DISPLAY_INFO dspInfo;
	//	Previous Displayed Memory
	NX_VID_MEMORY_INFO *pPrevDsp = NULL;

	//	Encoder Parameters
	NX_VID_ENC_INIT_PARAM encInitParam = {0,};
	uint8_t *seqBuffer = (uint8_t *)malloc( MAX_SEQ_BUF_SIZE );
	NX_VID_ENC_HANDLE hEnc;
	NX_VID_ENC_IN encIn;
	NX_VID_ENC_OUT encOut;

	long long totalSize = 0;
	double bitRate = 0.;
	uint64_t StrmTotalSize = 0;

	//	Apply Default Value
	inWidth = pAppData->width;
	inHeight = pAppData->height;
	pAppData->fps = (pAppData->fps)?pAppData->fps:30;
	pAppData->bitrate = (pAppData->bitrate)?pAppData->bitrate:10000000;
	pAppData->gop = (pAppData->gop)?pAppData->gop:30;

	if ( pAppData->codec == 0) pAppData->codec = NX_AVC_ENC;
	else if (pAppData->codec == 1) pAppData->codec = NX_MP4_ENC;
	else if (pAppData->codec == 2) pAppData->codec = NX_H263_ENC;
	else if (pAppData->codec == 3) pAppData->codec = NX_JPEG_ENC;

	//	Allocate Memory
	for( i=0; i<MAX_ENC_BUFFER ; i++ )
	{
		if ( pAppData->codec != NX_JPEG_ENC )
		{
#if ENABLE_NV12
			hMem[i] = NX_VideoAllocateMemory( 4096, inWidth, inHeight, NX_MEM_MAP_LINEAR, FOURCC_NV12 );
#else
			hMem[i] = NX_VideoAllocateMemory( 4096, inWidth, inHeight, NX_MEM_MAP_LINEAR, /*FOURCC_NV12*/FOURCC_MVS0 );
#endif
		}
		else
			hMem[i] = NX_VideoAllocateMemory( 4096, inWidth, inHeight, NX_MEM_MAP_LINEAR, /*FOURCC_NV12*/FOURCC_MVS0 );
	}

	fdIn = fopen( pAppData->inFileName, "rb" );
	fdOut = fopen( pAppData->outFileName, "wb" );
	fdLog = fopen( pAppData->outLogFileName, "w" );

	//	Initailize Display
	dspInfo.port = 0;
	dspInfo.module = 0;
	dspInfo.width = inWidth;
	dspInfo.height = inHeight;
	dspInfo.numPlane = 1;
	dspInfo.dspSrcRect.left = 0;
	dspInfo.dspSrcRect.top = 0;
	dspInfo.dspSrcRect.right = inWidth;
	dspInfo.dspSrcRect.bottom = inHeight;
	dspInfo.dspDstRect.left = pAppData->dspX;
	dspInfo.dspDstRect.top = pAppData->dspY;
	dspInfo.dspDstRect.right = pAppData->dspX + pAppData->width;
	dspInfo.dspDstRect.bottom = pAppData->dspY + pAppData->height;
	hDsp = NX_DspInit( &dspInfo );
	NX_DspVideoSetPriority(dspInfo.module, 0);

	//	Initialize Encoder
	hEnc = NX_VidEncOpen( pAppData->codec, &instanceIdx );

	encInitParam.width = inWidth;
	encInitParam.height = inHeight;
	encInitParam.gopSize = pAppData->gop;
	encInitParam.bitrate = pAppData->bitrate;
	encInitParam.fpsNum = pAppData->fps;
	encInitParam.fpsDen = 1;
#if ENABLE_NV12
	encInitParam.chromaInterleave = 1;
#else
	encInitParam.chromaInterleave = 0;
#endif

	//	Rate Control
	encInitParam.enableRC = (pAppData->qp == 0) ? (1) : (0);		//	Enable Rate Control
	encInitParam.enableSkip = 0;	//	Enable Skip
	encInitParam.maxQScale = 0;		//	Max Qunatization Scale
	encInitParam.userQScale = (pAppData->qp == 0) ? (10) : (pAppData->qp);	//	Default Encoder API ( enableRC == 0 )
	encInitParam.enableAUDelimiter = 0;	//	Enable / Disable AU Delimiter

	if ( pAppData->codec == NX_JPEG_ENC )
	{
		encInitParam.chromaInterleave = 0;
		encInitParam.jpgQuality = (pAppData->qp == 0) ? (90) : (pAppData->qp);
	}

	if (NX_VidEncInit( hEnc, &encInitParam ) != VID_ERR_NONE)
	{
		printf("NX_VidEncInit() failed \n");
		exit(-1);
	}

	if( fdOut )
	{
		int size;
		//	Write Sequence Data
		if ( pAppData->codec != NX_JPEG_ENC )
			NX_VidEncGetSeqInfo( hEnc, seqBuffer, &size );
		else
			NX_VidEncJpegGetHeader( hEnc, seqBuffer, &size );

		fwrite( seqBuffer, 1, size, fdOut );
		dumpdata( seqBuffer, size, "sps pps" );
		printf("Encoder Out Size = %d\n", size);

		StrmTotalSize += size;
	}

	if( fdLog )
	{
		fprintf(fdLog, "Frame Count\tFrame Size\tEncoding Time\tIs Key\n");
	}

	totalTime = 0;

	while(1)
	{
#if 0
		if (frameCnt == 50)
		{
			NX_VID_ENC_CHG_PARAM stChgParam = {0,};;
			stChgParam.chgFlg = VID_CHG_GOP;
			stChgParam.gopSize = 10;
			NX_VidEncChangeParameter( hEnc, &stChgParam );
		}
		else if (frameCnt == 100)
		{
			NX_VID_ENC_CHG_PARAM stChgParam = {0,};
			stChgParam.chgFlg = VID_CHG_BITRATE | VID_CHG_GOP;
			stChgParam.bitrate = 5000000;
			stChgParam.gopSize = 30;
			NX_VidEncChangeParameter( hEnc, &stChgParam );
		}
		else if (frameCnt == 150)
		{
			NX_VID_ENC_CHG_PARAM stChgParam = {0,};
			stChgParam.chgFlg = VID_CHG_FRAMERATE;
			stChgParam.fpsNum = 15;
			stChgParam.fpsDen = 1;
			NX_VidEncChangeParameter( hEnc, &stChgParam );
		}
		else if (frameCnt == 150)
		{
			NX_VID_ENC_CHG_PARAM stChgParam = {0,};
			stChgParam.chgFlg = VID_CHG_FRAMERATE;
			stChgParam.fpsNum = 15;
			stChgParam.fpsDen = 1;
			NX_VidEncChangeParameter( hEnc, &stChgParam );
		}
		else if (frameCnt == 200)
		{
			NX_VID_ENC_CHG_PARAM stChgParam = {0,};
			stChgParam.chgFlg = VID_CHG_BITRATE | VID_CHG_GOP | VID_CHG_FRAMERATE;
			stChgParam.bitrate = 30000000;
			stChgParam.gopSize = 10;
			stChgParam.fpsNum = 30;
			stChgParam.fpsDen = 1;
			NX_VidEncChangeParameter( hEnc, &stChgParam );
		}
		else if (frameCnt % 35 == 7)
			encIn.forcedIFrame = 1;
		else if (frameCnt % 35 == 20)
			encIn.forcedSkipFrame = 1;
#endif

		encIn.pImage = hMem[frameCnt%MAX_ENC_BUFFER];

		if( fdIn )
		{
			int32_t readSize = fread(pSrcBuf, 1, inWidth*inHeight*3/2, fdIn);

			if( readSize != inWidth*inHeight*3/2 || readSize == 0 )
			{
				printf("End of Stream!!!\n");
				break;
			}
		}

		//	Load Image
		LoadImage( pSrcBuf, inWidth, inHeight, encIn.pImage );

		if ( pAppData->codec != NX_JPEG_ENC )
		{
			encIn.timeStamp = 0;
			encIn.forcedIFrame = 0;
			encIn.forcedSkipFrame = 0;
			encIn.quantParam = ( pAppData->qp ) ? ( pAppData->qp ) : (24);

			//	Encode Image
			startTime = NX_GetTickCount();
			NX_VidEncEncodeFrame( hEnc, &encIn, &encOut );
		}
		else
		{
			startTime = NX_GetTickCount();
			NX_VidEncJpegRunFrame( hEnc, encIn.pImage, &encOut );
		}

		endTime = NX_GetTickCount();
		totalTime += (endTime-startTime);

		//	Display Image
		NX_DspQueueBuffer( hDsp, encIn.pImage );

		if( pPrevDsp )
		{
			NX_DspDequeueBuffer( hDsp );
		}
		pPrevDsp = encIn.pImage;

		if( fdOut && encOut.bufSize>0 )
		{
			totalSize += encOut.bufSize;
			bitRate = (double)totalSize/(double)frameCnt*.8;

			//	Write Sequence Data
			fwrite( encOut.outBuf, 1, encOut.bufSize, fdOut );
			printf("[%4d]FrameType = %d, size = %8d, ", frameCnt, encOut.frameType, encOut.bufSize);
			//dumpdata( encOut.outBuf, 16, "" );
			printf("bitRate = %6.3f kbps, Qp = %2d, time=%6lld\n", bitRate*pAppData->fps/1000., encIn.quantParam,(endTime-startTime));
			StrmTotalSize += encOut.bufSize;

			//	Frame Size, Encoding Time, Is Key
			if( fdLog )
			{
				fprintf(fdLog, "%d\t%d\t%lld\t%d\n", frameCnt, encOut.bufSize, (endTime-startTime), encOut.frameType);
				fflush(fdLog);
			}
		}
		frameCnt ++;
	}

	printf("Total Bitrate = %.3f, Frame Count = %d \n", (float)((StrmTotalSize * 8 * frameCnt) / (pAppData->fps * 1024 * 1024)), frameCnt );

	if( fdLog )
	{
		fclose(fdLog);
	}

	if( fdIn )
	{
		fclose( fdIn );
	}

	if( fdOut )
	{
		fclose( fdOut );
	}

	NX_DspClose( hDsp );

	return 0;
}


void print_usage(const char *appName)
{
	printf("\n==================================================================\n");
	printf("\nUsage : %s [options]\n", appName);
	printf("------------------------------------------------------------------\n");
	printf("    -u                              : usage \n");
	printf("    -i [input file name]        [M] : Input file name\n");
	printf("    -w [width]                  [M] : Input image's width\n");
	printf("    -h [height]                 [M] : Input image's height\n");
	printf("    -f [frame rate]             [O] : Input image's frame rate(def:30)\n");
	printf("    -o [output file name]       [M] : Output file name\n");
	printf("    -l [output log file name]   [O] : Output log file name\n");
	printf("    -g [gop size]               [O] : Out Bitstream's GoP size(def:30)\n");
	printf("    -b [bitrate]                [O] : Out Bitstream's bitrate(def:10M)\n");
	printf("    -d [x] [y] [width] [height] [O] : Display image position\n");
	printf("    -c [codec]                  [o] : 0:H.264, 1:Mp4v, 2:H.263, 3:JPEG (def:H.264)\n");
	printf("    -q [QP]                     [o] : Quantization Parameter\n");
	printf("------------------------------------------------------------------\n");
	printf("  [M] = mandatory, [O] = Optional\n");
	printf("==================================================================\n\n");
}

/*
typedef struct tENC_APP_DATA {
	//	Input Options
	char *inFileName;			//	Input File Name
	int32_t	width;				//	Input YUV Image Width
	int32_t	height;				//	Input YUV Image Height

	//	Output Options
	char *outFileName;			//	Output File Name
	char *outLogFileName;		//	Output Log File Name

	//	Preview Options
	int32_t dspX;				//	Display X Axis Offset
	int32_t dspY;				//	Display Y Axis Offset
	int32_t	dspWidth;			//	Display Width
	int32_t dspHeight;			//	Dispplay Height
} ENC_APP_DATA;
*/
int32_t main( int32_t argc, char *argv[] )
{
	int32_t opt;
	int32_t dspX=-1, dspY=-1, dspWidth=-1, dspHeight=-1;	//	Display Position
	ENC_APP_DATA appData;

	memset( &appData, 0, sizeof(appData) );

	while( -1 != (opt=getopt(argc, argv, "ui:w:h:o:l:d:f:g:b:c:q:")))
	{
		switch( opt ){
			case 'u':
				print_usage(argv[0]);
				return 0;
			case 'i':
				appData.inFileName = strdup( optarg );
				break;
			case 'w':
				appData.width = atoi( optarg );
				break;
			case 'h':
				appData.height = atoi( optarg );
				break;
			case 'f':
				appData.fps = atoi( optarg );
				break;
			case 'o':
				appData.outFileName = strdup( optarg );
				break;
			case 'l':
				appData.outLogFileName = strdup( optarg );
				break;
			case 'b':
				appData.bitrate = atoi( optarg );
				break;
			case 'g':
				appData.gop = atoi( optarg );
				break;
			case 'd':
				sscanf( optarg, "%d,%d,%d,%d", &dspX, &dspY, &dspWidth, &dspHeight );
				printf("dspX = %d, dspY=%d, dspWidth=%d, dspHeight=%d\n", dspX, dspY, dspWidth, dspHeight );
				printf("optarg = %s\n", optarg);
				break;
			case 'c':
				appData.codec = atoi( optarg );
				break;
			case 'q':
				appData.qp = atoi( optarg );
				break;
			default:
				break;
		}
	}

	//	Check Parameters
	if( appData.width <= 0 ||
		appData.height <= 0 ||
		appData.inFileName == 0 ||
		appData.outFileName == 0 )
	{
		printf("Error : invalid arguments!!!\n");
		printf("   In/Out File Name & Input Width/Height is mandatory !!\n");
		print_usage( argv[0] );
		return -1;
	}

	appData.dspX = dspX;
	appData.dspY = dspX;
	appData.dspWidth = dspWidth;
	appData.dspHeight = dspHeight;

	if( appData.outLogFileName == NULL )
	{
		appData.outLogFileName = (uint8_t*)malloc(strlen(appData.outFileName) + 5);
		strcpy(appData.outLogFileName, appData.outFileName);
		strcat(appData.outLogFileName, ".log");
	}

	return performance_test(&appData);
}

