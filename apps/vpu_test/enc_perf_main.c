#include <stdio.h>
#include <string.h>
#include <unistd.h>		//	getopt & optarg
#include <stdlib.h>		//	atoi
#include <sys/time.h>	//	gettimeofday
#include <math.h>

#include <nx_fourcc.h>
#include <nx_dsp.h>		//	Display

#include "nx_video_api.h"	//	Video En/Decoder
#include "queue.h"


#define	MAX_SEQ_BUF_SIZE		(4*1024)
#define	MAX_ENC_BUFFER			2

#define	ENABLE_NV12				1

//#define TEST_CHG_PARA


//	Encoder Application Data
typedef struct tENC_APP_DATA {
	//	Input Options
	char *inFileName;			//	Input File Name
	int32_t	width;				//	Input YUV Image Width
	int32_t	height;				//	Input YUV Image Height
	int32_t fpsNum;				//	Input Image Fps Number
	int32_t fpsDen;				//	Input Image Fps Density

	//	Output Options
	char *outFileName;			//	Output File Name
	char *outLogFileName;		//	Output Log File Name
	char *outImgName;			//	Output Reconstructed Image File Name

	int32_t kbitrate;			//	kilo Bitrate
	int32_t gop;				//	GoP
	int32_t codec;				//	0:H.264, 1:Mp4v, 2:H.263, 3:JPEG (def:H.264)
	int32_t qp;					//	Fixed Qp
	int32_t vbv;
	int32_t maxQp;
	int32_t RCAlgorithm;

	//	Preview Options
	int32_t dspX;				//	Display X Axis Offset
	int32_t dspY;				//	Display Y Axis Offset
	int32_t	dspWidth;			//	Display Width
	int32_t dspHeight;			//	Dispplay Height
} ENC_APP_DATA;


static uint64_t NX_GetTickCount( void )
{
	uint64_t ret;
	struct timeval	tv;
	struct timezone	zv;
	gettimeofday( &tv, &zv );
	ret = ((uint64_t)tv.tv_sec)*1000000 + tv.tv_usec;
	return ret;
}

static float GetPSNR (uint8_t *pbyOrg, uint8_t *pbyRecon, int32_t iWidth, int32_t iHeight, int32_t iStride)
{
    int32_t  i, j;
    float    fPSNR_L = 0;

	for (i = 0; i < iHeight ; i++) {
		for (j = 0; j < iWidth ; j++) {
			fPSNR_L += (*(pbyOrg + j) - *(pbyRecon + j)) * (*(pbyOrg + j) - *(pbyRecon + j));
		}
		pbyOrg   += iStride;
		pbyRecon += iWidth;
	}

    // L
    fPSNR_L = (float) fPSNR_L / (float) (iWidth * iHeight);
    fPSNR_L = (fPSNR_L)? 10 * (float) log10 ((float) (255 * 255) / fPSNR_L): (float) 99.99;

	return fPSNR_L;
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

static void TestchangeParameter( ENC_APP_DATA *pAppData, NX_VID_ENC_HANDLE hEnc, int32_t frameCnt )
{
	NX_VID_ENC_CHG_PARAM stChgParam = {0,};

	if (frameCnt == 0)
	{
		printf(" <<< Test Change Parameter >>> \n");
	}
	else if (frameCnt == 200)
	{
		stChgParam.chgFlg = VID_CHG_GOP;
		stChgParam.gopSize = pAppData->gop >> 1;
		printf("Change From 200Frm : GOP Size is half (%d -> %d) \n", pAppData->gop, stChgParam.gopSize );
		NX_VidEncChangeParameter( hEnc, &stChgParam );
	}
	else if (frameCnt == 400)
	{
		stChgParam.chgFlg = VID_CHG_BITRATE | VID_CHG_GOP | VID_CHG_VBV;
		stChgParam.bitrate = ( pAppData->kbitrate >> 1 ) * 1024;
		stChgParam.gopSize = pAppData->gop;
		stChgParam.rcVbvSize = 0;
		printf("Change From 400Frm : BPS is half (%d -> %d) \n", pAppData->kbitrate, stChgParam.bitrate );
		NX_VidEncChangeParameter( hEnc, &stChgParam );
	}
	else if (frameCnt == 600)
	{
		stChgParam.chgFlg = VID_CHG_FRAMERATE | VID_CHG_BITRATE | VID_CHG_VBV;
		stChgParam.bitrate = pAppData->kbitrate * 1024;
		stChgParam.fpsNum = pAppData->fpsNum >> 1;
		stChgParam.fpsDen = pAppData->fpsDen;
		stChgParam.rcVbvSize = 0;
		printf("Change From 600Frm : FPS is half (%d, %d) \n", pAppData->fpsNum, stChgParam.fpsNum );
		NX_VidEncChangeParameter( hEnc, &stChgParam );
	}
	else if (frameCnt == 800)
	{
		stChgParam.chgFlg = VID_CHG_BITRATE | VID_CHG_GOP | VID_CHG_FRAMERATE | VID_CHG_VBV;
		stChgParam.bitrate = ( pAppData->kbitrate << 2 ) * 1024;
		stChgParam.gopSize = pAppData->gop >> 2;
		stChgParam.fpsNum = pAppData->fpsNum;
		stChgParam.fpsDen = pAppData->fpsDen;
		stChgParam.rcVbvSize = 0;
		printf("Change From 800Frm : BPS is quadruple & gop is quarter (%d -> %d, %d -> %d) \n", pAppData->kbitrate, stChgParam.bitrate, pAppData->gop, stChgParam.gopSize );
		NX_VidEncChangeParameter( hEnc, &stChgParam );
	}
}

int32_t performance_test( ENC_APP_DATA *pAppData )
{
	DISPLAY_HANDLE hDsp;				// Display Handle
	NX_VID_ENC_HANDLE hEnc;				// Encoder Handle
	uint64_t StrmTotalSize = 0;
	float    PSNRSum = 0;

	//	Input Image Width & Height
	int32_t inWidth  = pAppData->width;
	int32_t inHeight = pAppData->height;

	FILE *fdIn  = fopen( pAppData->inFileName, "rb" );
	FILE *fdOut = fopen( pAppData->outFileName, "wb" );
	FILE *fdLog = ( pAppData->outLogFileName ) ? fopen( pAppData->outLogFileName, "w" ) : NULL;
	FILE *fdRecon = ( pAppData->outImgName ) ? fopen( pAppData->outImgName, "wb" ) : NULL;

	if ( fdIn == NULL || fdOut == NULL )
	{
		printf("input file/out stream file open error!! \n");
		exit(-1);
	}

	//==============================================================================
	// INITIALIZATION
	//==============================================================================
	{
		DISPLAY_INFO dspInfo = {0,};
		NX_VID_ENC_INIT_PARAM encInitParam = {0,};		//	Encoder Parameters
		uint8_t *seqBuffer = (uint8_t *)malloc( MAX_SEQ_BUF_SIZE );

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
		if ( pAppData->codec == 0) pAppData->codec = NX_AVC_ENC;
		else if (pAppData->codec == 1) pAppData->codec = NX_MP4_ENC;
		else if (pAppData->codec == 2) pAppData->codec = NX_H263_ENC;
		else if (pAppData->codec == 3) pAppData->codec = NX_JPEG_ENC;
		hEnc = NX_VidEncOpen( pAppData->codec, NULL );

		pAppData->fpsNum = ( pAppData->fpsNum ) ? ( pAppData->fpsNum ) : ( 30 );
		pAppData->fpsDen = ( pAppData->fpsDen ) ? ( pAppData->fpsDen ) : ( 1 );
		pAppData->gop = ( pAppData->gop ) ? ( pAppData->gop ) : ( pAppData->fpsNum / pAppData->fpsDen );

		encInitParam.width = inWidth;
		encInitParam.height = inHeight;
		encInitParam.fpsNum = pAppData->fpsNum;
		encInitParam.fpsDen = pAppData->fpsDen;
		encInitParam.gopSize = pAppData->gop;
		encInitParam.bitrate = pAppData->kbitrate * 1024;
		encInitParam.chromaInterleave = ENABLE_NV12;
		encInitParam.enableAUDelimiter = 0;			// Enable / Disable AU Delimiter
		encInitParam.searchRange = 0;
		if ( pAppData->codec == NX_JPEG_ENC )
		{
			encInitParam.chromaInterleave = 0;
			encInitParam.jpgQuality = (pAppData->qp == 0) ? (90) : (pAppData->qp);
		}

		//	Rate Control
		encInitParam.maximumQp = pAppData->maxQp;
		encInitParam.disableSkip = 0;
		encInitParam.initialQp = pAppData->qp;
		encInitParam.enableRC = ( encInitParam.bitrate ) ? ( 1 ) : ( 0 );
		encInitParam.RCAlgorithm = ( pAppData->RCAlgorithm == 0 ) ? ( 1 ) : ( 0 );
		encInitParam.rcVbvSize = ( pAppData->vbv ) ? (pAppData->vbv) : (encInitParam.bitrate * 2 / 8);

		if (NX_VidEncInit( hEnc, &encInitParam ) != VID_ERR_NONE)
		{
			printf("NX_VidEncInit() failed \n");
			exit(-1);
		}
		printf("NX_VidEncInit() success \n");

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
			StrmTotalSize += size;
			printf("Encoder Header Size = %d\n", size);
		}

		if( fdLog )
		{
			fprintf(fdLog, "Frame Count\tFrame Size\tEncoding Time\tIs Key\n");
		}
	}

	//==============================================================================
	// ENCODE PROCESS UNIT
	//==============================================================================
	{
		NX_VID_MEMORY_HANDLE hMem[MAX_ENC_BUFFER];		// Memory
		NX_VID_MEMORY_INFO *pPrevDsp = NULL;			// Previous Displayed Memory

		NX_VID_ENC_IN encIn;
		NX_VID_ENC_OUT encOut;

		long long totalSize = 0;
		double bitRate = 0.;
		int32_t frameCnt = 0, i, readSize;
		uint64_t startTime, endTime, totalTime = 0;

		uint8_t *pSrcBuf = (uint8_t*)malloc(inWidth*inHeight*3/2);

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

		while(1)
		{
#ifdef TEST_CHG_PARA
			TestchangeParameter( pAppData, hEnc, frameCnt );
#endif
			//if (frameCnt % 35 == 7)
			//	encIn.forcedIFrame = 1;
			//else if (frameCnt % 35 == 20)
			//	encIn.forcedSkipFrame = 1;

			encIn.pImage = hMem[frameCnt%MAX_ENC_BUFFER];

			readSize = fread(pSrcBuf, 1, inWidth*inHeight*3/2, fdIn);
			if( readSize != inWidth*inHeight*3/2 || readSize == 0 )
			{
				printf("End of Stream!!!\n");
				break;
			}

			LoadImage( pSrcBuf, inWidth, inHeight, encIn.pImage );

			if ( pAppData->codec != NX_JPEG_ENC )
			{
				encIn.forcedIFrame = 0;
				encIn.forcedSkipFrame = 0;
				encIn.quantParam = pAppData->qp;
				encIn.timeStamp = 0;

				//	Encode Image
				startTime = NX_GetTickCount();
				NX_VidEncEncodeFrame( hEnc, &encIn, &encOut );
				endTime = NX_GetTickCount();
			}
			else
			{
				startTime = NX_GetTickCount();
				NX_VidEncJpegRunFrame( hEnc, encIn.pImage, &encOut );
				endTime = NX_GetTickCount();
			}

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
				float PSNR = GetPSNR(encIn.pImage->luVirAddr, encOut.ReconImg.luVirAddr, encOut.width, encOut.height, encIn.pImage->luStride);

				totalSize += encOut.bufSize;
				bitRate = (double)totalSize*8/(double)frameCnt;

				//	Write Sequence Data
				fwrite( encOut.outBuf, 1, encOut.bufSize, fdOut );
				printf("[%4d]FrameType = %d, size = %8d, ", frameCnt, encOut.frameType, encOut.bufSize);
				//dumpdata( encOut.outBuf, 16, "" );
				printf("bitRate = %6.3f kbps, Qp = %2d, PSNR = %f, time=%6lld\n", bitRate*pAppData->fpsNum/pAppData->fpsDen/1000., encIn.quantParam, PSNR, (endTime-startTime) );
				StrmTotalSize += encOut.bufSize;
				PSNRSum += PSNR;

				//	Frame Size, Encoding Time, Is Key
				if( fdLog )
				{
					fprintf(fdLog, "%5d\t%7d\t%2d\t%lld\t%d\n", frameCnt, encOut.bufSize, encIn.quantParam, (endTime-startTime), encOut.frameType);
					fflush(fdLog);
				}

				if ( fdRecon )
				{
					fwrite( encOut.ReconImg.luVirAddr, 1, encOut.width * encOut.height, fdRecon );
					fwrite( encOut.ReconImg.cbVirAddr, 1, encOut.width * encOut.height / 4, fdRecon );
					fwrite( encOut.ReconImg.crVirAddr, 1, encOut.width * encOut.height / 4, fdRecon );
				}
			}
			//if (frameCnt > 5) break;
			frameCnt++;
		}

		{
			float TotalBps = (float)((StrmTotalSize * 8 * pAppData->fpsNum / pAppData->fpsDen) / (frameCnt * 1024));
			printf("[Summary]Bitrate = %.3fKBps(%.2f%), PSNR = %.3fdB, Frame Count = %d \n", TotalBps, TotalBps * 100 / pAppData->kbitrate, (PSNRSum / frameCnt), frameCnt );
		}
	}

	//==============================================================================
	// TERMINATION
	//==============================================================================
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

	if ( fdRecon )
	{
		fclose( fdRecon );
	}

	if ( hEnc )
	{
		NX_VidEncClose( hEnc );
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
	printf("    -o [output file name]       [O] : Output file name (def : enc.bit)\n");
	printf("    -r [recon file name]        [O] : Output reconstruced image file name\n");
	printf("    -l [output log file name]   [O] : Output log file name\n");
	printf("    -w [width]                  [M] : Input image's width\n");
	printf("    -h [height]                 [M] : Input image's height\n");
	printf("    -f [fps Num] [fps Den]      [O] : Input image's frame rate Number (def:30 / 1)\n");
	printf("    -b [Kbitrate]               [M] : Out Bitstream's Kilo bitrate \n");
	printf("    -g [gop size]               [O] : Out Bitstream's GoP size (def:frame rate)\n");
	printf("    -c [codec]                  [o] : 0:H.264, 1:Mp4v, 2:H.263, 3:JPEG (def:H.264)\n");
	printf("    -q [QP]                     [O] : Quantization Parameter \n");
	printf("    -v [VBV]                    [O] : VBV Size (def:2Sec)\n");
	printf("    -m [Max Qp]                 [O] : Maximum QP \n");
	printf("    -a [RC Algorithm]           [O] : Rate Control Algorithm (0 : Nexell, 1 : CnM) \n");
	printf("    -d [x] [y] [width] [height] [O] : Display image position\n");
	printf("------------------------------------------------------------------\n");
	printf("  [M] = mandatory, [O] = Optional\n");
	printf("==================================================================\n\n");
}

int32_t main( int32_t argc, char *argv[] )
{
	int32_t opt;
	int32_t dspX=-1, dspY=-1, dspWidth=-1, dspHeight=-1;	//	Display Position
	ENC_APP_DATA appData;

	memset( &appData, 0, sizeof(appData) );

	while( -1 != (opt=getopt(argc, argv, "ui:o:r:l:w:h:f:b:g:c:q:v:m:a:d:")))
	{
		switch( opt ){
			case 'u':
				print_usage(argv[0]);
				return 0;
			case 'i':
				appData.inFileName = strdup( optarg );
				break;
			case 'o':
				appData.outFileName = strdup( optarg );
				break;
			case 'r':
				appData.outImgName = strdup( optarg );
				break;
			case 'l':
				appData.outLogFileName = strdup( optarg );
				break;
			case 'w':
				appData.width = atoi( optarg );
				break;
			case 'h':
				appData.height = atoi( optarg );
				break;
			case 'f':
				sscanf( optarg, "%d,%d", &appData.fpsNum, &appData.fpsDen );
				break;
			case 'b':
				appData.kbitrate = atoi( optarg );
				break;
			case 'g':
				appData.gop = atoi( optarg );
				break;
			case 'c':
				appData.codec = atoi( optarg );
				break;
			case 'q':
				appData.qp = atoi( optarg );
				break;
			case 'v':
				appData.vbv = atoi( optarg );
				break;
			case 'm':
				appData.maxQp = atoi( optarg );
				break;
			case 'a':
				appData.RCAlgorithm = atoi( optarg );
				break;
			case 'd':
				sscanf( optarg, "%d,%d,%d,%d", &dspX, &dspY, &dspWidth, &dspHeight );
				printf("dspX = %d, dspY=%d, dspWidth=%d, dspHeight=%d\n", dspX, dspY, dspWidth, dspHeight );
				printf("optarg = %s\n", optarg);
				break;
			default:
				break;
		}
	}

	//	Check Parameters
#if 1
	if( appData.width <= 0 ||
		appData.height <= 0 ||
		appData.inFileName == 0 )
	{
		printf("Error : invalid arguments!!!\n");
		printf("   Input File Name & Input Width/Height is mandatory !!\n");
		print_usage( argv[0] );
		return -1;
	}
#else
	if( appData.width <= 0 ) appData.width = 176;
	if( appData.height <= 0 ) appData.height = 144;
	if(	appData.inFileName == 0 ) appData.inFileName = "SD/images/fm_15fps_150_qcif.yuv";
	if(	appData.outFileName == 0) appData.outFileName = "out.264";
	if( appData.fps == 0 ) appData.fps = 15;
	if( appData.bitrate == 0) appData.bitrate = 64000;
	if( appData.gop == 0 ) appData.gop = 30;
#endif

	appData.dspX = dspX;
	appData.dspY = dspX;
	appData.dspWidth = dspWidth;
	appData.dspHeight = dspHeight;

	if( appData.outFileName == NULL )
		appData.outFileName = "enc.bit";

	/*if( appData.outLogFileName == NULL )
	{
		appData.outLogFileName = (uint8_t*)malloc(strlen(appData.outFileName) + 5);
		strcpy(appData.outLogFileName, appData.outFileName);
		strcat(appData.outLogFileName, ".log");
	}*/

	return performance_test(&appData);
}

