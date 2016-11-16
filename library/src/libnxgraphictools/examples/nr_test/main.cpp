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
	printf("  -o [file name]      : Output file name(optional)\n");
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

int main( int argc, char *argv[] )
{
	int opt;
	char *inFileName = NULL;
	char *outFileName = NULL;
	int srcWidth = 1920, srcHeight = 1080;
	int dstWidth = 1920, dstHeight = 1080;
	int mode = 0;
	int32_t param = 7;

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
	NX_VID_MEMORY_HANDLE inBuffer = NULL; 	//	Allocate 3 plane memory for YUV
	inBuffer = NX_VideoAllocateMemory( 4096, srcWidth, srcHeight, NX_MEM_MAP_LINEAR, FOURCC_MVS0 );

	//	Load Image to Allocated Memory
	FILE *inFd = fopen( inFileName, "rb");
	if( inFd == NULL )
	{
		printf("Cannot open input file(%s)\n", inFileName);
		exit(-1);
	}
	else
	{
		size_t allocSize = (srcWidth * srcHeight * 3) / 2;
		size_t readSize = 0;
		unsigned char *inImgBuffer = (unsigned char *)malloc(allocSize);
		unsigned char *tmpSrc;
		unsigned char *tmpDst;
		readSize = fread( inImgBuffer, 1, allocSize, inFd );

		printf("ReadSize = %d\n", readSize);

		if( readSize != allocSize )
		{
			printf("[Warning] Input file information is not valid.!!\n");
		}
		tmpSrc = inImgBuffer;
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
		fclose( inFd );
	}

	//
	//	Deinter / Scale / Deinterlace & Scale
	//
	NX_VID_MEMORY_HANDLE outBuffer = NX_VideoAllocateMemory( 4096, dstWidth, dstHeight, NX_MEM_MAP_LINEAR, FOURCC_MVS0 );
	NX_GT_NRFILTER_HANDLE hNrFilter = NULL;

	hNrFilter = NX_GTNRFilterOpen( srcWidth, srcHeight, dstWidth, dstHeight, 1 );
	if( NULL == hNrFilter )
	{
		printf("NX_GTNRFilterOpen() failed!!!\n");
		exit(1);
	}
	ClearVideoMemory(outBuffer);

	NX_GTNRFilterDoFiltering( hNrFilter, inBuffer, outBuffer, param );

	//
	//	Write to Output File
	//
	FILE *outFd = fopen(outFileName, "wb");

	if( outFd != NULL )
	{
		for( int i=0 ; i<dstHeight ; i++ )
		{
			fwrite( (unsigned char*)(outBuffer->luVirAddr + outBuffer->luStride*i), 1, dstWidth, outFd );
		}
		for( int i=0 ; i<dstHeight/2 ; i++ )
		{
			fwrite( (unsigned char*)(outBuffer->cbVirAddr + i * outBuffer->luStride/2), 1, dstWidth/2, outFd );
		}
		for( int i=0 ; i<dstHeight/2 ; i++ )
		{
			fwrite( (unsigned char*)(outBuffer->crVirAddr + i * outBuffer->luStride/2), 1, dstWidth/2, outFd );
		}
		fclose(outFd);
	}
	else
	{
		printf("Cannot open output file(%s)\n", outFileName);
		exit(-1);
	}

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
