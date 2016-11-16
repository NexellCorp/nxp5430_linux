#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <pthread.h>
#include <poll.h>
#include <errno.h>

#include <linux/fb.h>	//	for FB
#include <sys/types.h>	//	for open
#include <sys/stat.h>	//	for open
#include <fcntl.h>		//	for open
#include <sys/mman.h>	//	for mmap
#include <sys/ioctl.h>	//	for _IOWR
#include <sys/time.h>	//	for gettimeofday

#include <nx_fourcc.h>
#include <nx_alloc_mem.h>
#include <nx_graphictools.h>
#include <nx_video_api.h>

#define	ALIGNED16(X)	(((X+15)>>4)<<4)

static uint64_t time_get_usec(void)
{
	int32_t result;
	struct timeval tod;
	result = gettimeofday(&tod, NULL);

	/* gettimeofday returns non-null on error*/
	if (0 != result) return 0;
	return ((uint64_t)tod.tv_sec) * 1000000ULL + tod.tv_usec;
}

void print_usage(const char *appName)
{
	printf("\nUsage : %s [options]\n", appName);
	printf("  -h                  : help\n");
	printf("  -f [file name]      : Input file name(mandatory)\n");
	printf("  -o [file name]      : Output file name(mandatory)\n");
	printf("  -s [width,height]   : Source file image size(mandatory)\n");
	printf("  -d [width,height]   : Destination file image size(optional)\n");
	printf("-----------------------------------------------------------------------\n");
	printf(" example) \n");
	printf("  #> %s -f input.yuv -o output.yuv -s 1920,1080 -d 1280,720\n", appName);
}


void ClearVideoMemory(NX_VID_MEMORY_HANDLE hMem)
{
	memset((void*)hMem->luVirAddr, 0, hMem->luStride*hMem->imgHeight);
	memset((void*)hMem->cbVirAddr, 0, hMem->cbStride*hMem->imgHeight/2);
	memset((void*)hMem->crVirAddr, 0, hMem->crStride*hMem->imgHeight/2);
}


//----------------------------------------------------------------------------------------------------
//
//	Signal Handler
//
static int32_t g_ExitLoop = false;
static void signal_handler( int32_t sig )
{
	printf("Aborted by signal %s (%d)..\n", (char*)strsignal(sig), sig);

	switch( sig )
	{
		case SIGINT :
			printf("SIGINT..\n"); 	break;
		case SIGTERM :
			printf("SIGTERM..\n");	break;
		case SIGABRT :
			printf("SIGABRT..\n");	break;
		default :
			break;
	}

	g_ExitLoop = true;

	exit(EXIT_FAILURE);
}

static void register_signal( void )
{
	signal( SIGINT,  signal_handler );
	signal( SIGTERM, signal_handler );
	signal( SIGABRT, signal_handler );
}


static uint8_t gst_pSeqBuffer[1024];

int32_t main( int32_t argc, char *argv[] )
{
	int32_t opt;
	char *inFileName = NULL;
	char *outFileName = NULL;
	int32_t srcWidth = 1920, srcHeight = 1080;
	int32_t dstWidth = 1920, dstHeight = 1080;
	int32_t seqSize, param = 3.0;
	NX_VID_ENC_HANDLE hEnc;
	NX_VID_ENC_IN encIn;
	NX_VID_ENC_OUT encOut;
	uint32_t frameCnt = 0;

	uint32_t totalSize = 0;

	register_signal();

	while( -1 != (opt=getopt(argc, argv, "hf:o:s:d:p:")) )
	{
		switch( opt )
		{
			case 'h':
				print_usage( argv[0] );
				return 0;
			case 'f':
				inFileName = strdup(optarg);
				break;
			case 'o':
				outFileName = strdup(optarg);
				break;
			case 's':
				sscanf( optarg, "%d,%d", &srcWidth, &srcHeight );
				break;
			case 'p':
				param = atoi( optarg );
				break;
			case 'd':
				sscanf( optarg, "%d,%d", &dstWidth, &dstHeight );
				break;
			default:
				break;
		}
	}

	if( dstWidth==0 && dstHeight==0 )
	{
		dstWidth = srcWidth;
		dstHeight = srcHeight;
	}

	if( outFileName == NULL )
	{
		outFileName = (char*)malloc( strlen(inFileName) + 5 );
		strcpy( outFileName, inFileName );
		strcat( outFileName, ".out.yuv" );
		outFileName[ strlen(outFileName)+1 ] = 0;
	}

	//	Print In/Out Informations
	printf("\n====================================================\n");
	printf("  Source filename   : %s\n", inFileName);
	printf("  Source Image Size : %d, %d\n", srcWidth, srcHeight);
	printf("  Output filename   : %s\n", outFileName);
	printf("  Output Image Size : %d, %d\n", dstWidth, dstHeight);
	printf("====================================================\n");

	//
	//	Load Input Image to Input Buffer
	//	 : Allocation Memory --> Load Image
	//
	NX_GT_NRFILTER_HANDLE hNrFilter = NULL;
	NX_VID_MEMORY_HANDLE inBuffer = NULL; 	//	Allocate 3 plane memory for YUV
	NX_VID_MEMORY_HANDLE outBuffer = NULL;
	
	inBuffer = NX_VideoAllocateMemory( 4096, srcWidth, srcHeight, NX_MEM_MAP_LINEAR, FOURCC_MVS0 );
	outBuffer = NX_VideoAllocateMemory( 4096, dstWidth, dstHeight, NX_MEM_MAP_LINEAR, FOURCC_MVS0 );

	hNrFilter = NX_GTNRFilterOpen( srcWidth, srcHeight, dstWidth, dstHeight, 1 );

	//	Load Image to Allocated Memory
	FILE *inFd = fopen( inFileName, "rb");
	FILE *outFd = fopen(outFileName, "wb");


	//	Allocate Input Buffer for YUV
	size_t allocSize = (srcWidth * srcHeight * 3) / 2;
	unsigned char *inImgBuffer = (unsigned char *)malloc(allocSize);
	size_t readSize = 0;


	NX_VID_ENC_INIT_PARAM encInitParam;											// Encoder Parameters

	// Initialize Encoder
	hEnc = NX_VidEncOpen( NX_AVC_ENC,  NULL );
	memset( &encInitParam, 0, sizeof(encInitParam) );
	encInitParam.width = dstWidth;
	encInitParam.height = dstHeight;
	encInitParam.fpsNum = 30;
	encInitParam.fpsDen = 1;
	encInitParam.gopSize = 30;
	encInitParam.bitrate = 1024 * 1024 * 12;		// 10Mbps
	encInitParam.chromaInterleave = 0;
	encInitParam.enableAUDelimiter = 0;	//	Enable / Disable AU Delimiter
	encInitParam.searchRange = 0;

	//	Rate Control
	encInitParam.maximumQp = 40;
	encInitParam.disableSkip = 1;
	encInitParam.initialQp = 0;
	encInitParam.enableRC = 1;
	encInitParam.RCAlgorithm = 1;
	encInitParam.rcVbvSize = 0;

	if (NX_VidEncInit( hEnc, &encInitParam ) != VID_ERR_NONE )
	{
		printf("NX_VidEncInit() failed \n");
		exit(-1);
	}

	//	Write Seqeunce Data
	if( outFd )
	{
		//
		//	Read Sequence Header
		//
		NX_VidEncGetSeqInfo( hEnc, gst_pSeqBuffer, &seqSize );
		fwrite(gst_pSeqBuffer, 1, seqSize, outFd);
	}

	while( (!g_ExitLoop) && ((readSize = fread( inImgBuffer, 1, allocSize, inFd )) == allocSize) )
	{
		//
		//	Copy Input Image to Encoder Input Memory
		//
		unsigned char *tmpSrc;
		unsigned char *tmpDst;
		tmpSrc = inImgBuffer;
		printf("frameCnt = %d\n", frameCnt++);
		//	Load Lu
		tmpDst = (unsigned char *)inBuffer->luVirAddr;
		for( int i=0 ; i<srcHeight ; i ++ )
		{
			memcpy( tmpDst, tmpSrc, srcWidth );
			tmpDst += inBuffer->luStride;
			tmpSrc += srcWidth;
		}
		//	Load Cb
		tmpDst = (unsigned char *)inBuffer->cbVirAddr;
		for( int i=0 ; i<srcHeight/2 ; i ++ )
		{
			memcpy( tmpDst, tmpSrc, srcWidth/2 );
			tmpDst += inBuffer->cbStride;
			tmpSrc += srcWidth/2;
		}
		//	Load Cr
		tmpDst = (unsigned char *)inBuffer->crVirAddr;
		for( int i=0 ; i<srcHeight/2 ; i ++ )
		{
			memcpy( tmpDst, tmpSrc, srcWidth/2 );
			tmpDst += inBuffer->crStride;
			tmpSrc += srcWidth/2;
		}

		//
		//	3D NR Filtering
		//
		NX_GTNRFilterDoFiltering( hNrFilter, inBuffer, outBuffer, (float)param );


		//
		//	Encoding
		//
		encIn.pImage = outBuffer;
		encIn.forcedIFrame = 0;
		encIn.forcedSkipFrame = 0;
		encIn.quantParam = 0;
		encIn.timeStamp = 0;

		NX_VidEncEncodeFrame( hEnc, &encIn, &encOut );

		if( outFd && encOut.bufSize>0 )
		{
			fwrite( encOut.outBuf, 1, encOut.bufSize, outFd );
			totalSize += encOut.bufSize;
		}
	}

	printf("Total Size = %d, FrameCnt = %d\n", totalSize, frameCnt);

	fclose( inFd );
	fclose( outFd );

	if( inFileName )
		free( inFileName );

	if( outFileName )
		free( outFileName );

	if( outBuffer )
		NX_FreeVideoMemory(outBuffer);

	if( hNrFilter != NULL )
	{
		NX_GTNRFilterClose(hNrFilter);
	}

	return 0;
}
