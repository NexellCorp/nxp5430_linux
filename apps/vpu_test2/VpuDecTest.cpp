//#define _ERROR_AND_SEEK_
//#define _DUMP_ES_
//#define DEINTERLACE_ENABLE

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/time.h>	//	gettimeofday

#include <nx_fourcc.h>
#include <nx_alloc_mem.h>
#include <nx_video_api.h>
#include "MediaExtractor.h"
#include "CodecInfo.h"
#include "Util.h"

#ifdef ANDROID
#include "NX_AndroidRenderer.h"
#else
#include "nx_dsp.h"
#endif

#ifdef DEINTERLACE_ENABLE
#include <nx_deinterlace.h>
#include <nx_graphictools.h>

#define DEINTERLACE_BUFFER_NUM	4
#endif

//	Display Window Screen Size
#define	WINDOW_WIDTH			1024
#define	WINDOW_HEIGHT			600
#define	NUMBER_OF_BUFFER		12


unsigned char streamBuffer[4*1024*1024];
unsigned char seqData[1024*4];


int32_t VpuDecMain( CODEC_APP_DATA *pAppData )
{
	VID_TYPE_E vpuCodecType;
	int vidRet;
	NX_VID_SEQ_IN seqIn;
	NX_VID_SEQ_OUT seqOut;
	NX_VID_DEC_HANDLE hDec;
	NX_VID_DEC_IN decIn;
	NX_VID_DEC_OUT decOut;
	int32_t seqSize = 0;
	int32_t bInit=0;
	int32_t outCount=0, frameCount=0;
	int32_t prevIdx = -1;
	int32_t size, key = 0;
	int64_t timeStamp = -1;
	int32_t mp4Class=0;
	int32_t codecTag=-1, codecId=-1;
	int32_t imgWidth=-1, imgHeight=-1;
	int32_t instanceIdx;
	int32_t brokenB = 0;
	int32_t iPrevIdx = -1;
	uint64_t startTime, endTime, totalTime = 0;
	FILE *fpOut = NULL;
#ifdef _DUMP_ES_
	FILE *fpES = fopen("enc.bit", "wb");
#endif

#ifdef DEINTERLACE_ENABLE
	void *hDeinterlace = NULL;
#ifndef ANDROID
	NX_VID_MEMORY_HANDLE hVideoMemory[DEINTERLACE_BUFFER_NUM];		// For DeInterlaced Frame
#endif
#endif

	CMediaReader *pMediaReader = new CMediaReader();
	if( !pMediaReader->OpenFile( pAppData->inFileName ) )
	{
		printf("Cannot open media file(%s)\n", pAppData->inFileName);
		exit(-1);
	}
	pMediaReader->GetVideoResolution(&imgWidth, &imgHeight);

	if( pAppData->outFileName )
	{
		fpOut = fopen( pAppData->outFileName, "wb" );
	}

#ifdef ANDROID
	CNX_AndroidRenderer *pAndRender = new CNX_AndroidRenderer(WINDOW_WIDTH, WINDOW_HEIGHT);
	NX_VID_MEMORY_HANDLE *pMemHandle;
	pAndRender->GetBuffers(NUMBER_OF_BUFFER, imgWidth, imgHeight, &pMemHandle );
	NX_VID_MEMORY_HANDLE hVideoMemory[NUMBER_OF_BUFFER];
	for( int32_t i=0 ; i<NUMBER_OF_BUFFER ; i++ )
	{
		hVideoMemory[i] = pMemHandle[i];
	}
#else
	//	Linux
	DISPLAY_HANDLE hDsp;
	DISPLAY_INFO dspInfo;
	dspInfo.port = 0;
	dspInfo.module = 0;
	dspInfo.width = imgWidth;
	dspInfo.height = imgHeight;
	dspInfo.numPlane = 1;
	dspInfo.dspSrcRect.left = 0;
	dspInfo.dspSrcRect.top = 0;
	dspInfo.dspSrcRect.right = imgWidth;
	dspInfo.dspSrcRect.bottom = imgHeight;
	dspInfo.dspDstRect.left = pAppData->dspX;
	dspInfo.dspDstRect.top = pAppData->dspY;
	dspInfo.dspDstRect.right = pAppData->dspX + pAppData->dspWidth;
	dspInfo.dspDstRect.bottom = pAppData->dspY + pAppData->dspHeight;
	hDsp = NX_DspInit( &dspInfo );
	NX_DspVideoSetPriority(0, 0);
	if( hDsp == NULL )
	{
		printf("Display Failed!!!\n");
		exit(-1);
	}
#endif

	pMediaReader->GetCodecTagId( AVMEDIA_TYPE_VIDEO, &codecTag, &codecId  );
	vpuCodecType = (VID_TYPE_E)(CodecIdToVpuType(codecId, codecTag));
	mp4Class = fourCCToMp4Class( codecTag );
	if( mp4Class == -1 )
		mp4Class = codecIdToMp4Class( codecId );
	printf("vpuCodecType = %d, mp4Class = %d\n", vpuCodecType, mp4Class );

	if( (hDec = NX_VidDecOpen(vpuCodecType, mp4Class, 0, &instanceIdx)) == NULL )
	{
		printf("NX_VidDecOpen(%d) failed!!!\n", vpuCodecType);
		return -1;
	}

	seqSize = pMediaReader->GetVideoSeqInfo( streamBuffer );

	while( 1 )
	{
		//	ReadStream
		if( pMediaReader->ReadStream( CMediaReader::MEDIA_TYPE_VIDEO, streamBuffer+seqSize, &size, &key, &timeStamp ) != 0 )
		{
			size = 0;
		}

		if( !bInit && !key )
		{
			continue;
		}

		if( !bInit )
		{
			memset( &seqIn, 0, sizeof(seqIn) );
			seqIn.addNumBuffers = 4;
			seqIn.enablePostFilter = 0;
			seqIn.seqInfo = streamBuffer;
			seqIn.seqSize = seqSize + size;
			seqIn.enableUserData = 0;
			seqIn.disableOutReorder = 0;
			seqIn.width = imgWidth;			// optional
			seqIn.height = imgHeight;
#if defined (ANDROID) && !defined (DEINTERLACE_ENABLE)
			//	Use External Video Memory
			seqIn.numBuffers = NUMBER_OF_BUFFER;
			seqIn.pMemHandle = &hVideoMemory[0];
#endif

#ifdef _DUMP_ES_
			fwrite(seqIn.seqInfo, 1, seqIn.seqSize, fpES);
#endif

			vidRet = NX_VidDecParseVideoCfg(hDec, &seqIn, &seqOut);
			if ( vidRet != VID_ERR_NONE )
			{
				printf("Parser Fail(%d, %d) \n", vidRet, seqOut.unsupportedFeature );
				exit(-1);
			}

			seqIn.width = seqOut.width;
			seqIn.height = seqOut.height;
			vidRet = NX_VidDecInit( hDec, &seqIn );
			if( vidRet == VID_NEED_STREAM )
			{
				printf("VPU Initialize Failed!!!\n");
				break;
			}

#ifdef DEINTERLACE_ENABLE
			if ( seqOut.isInterlace )
			{
				pAppData->deinterlaceMode = ( pAppData->deinterlaceMode ) ? ( pAppData->deinterlaceMode ) : ( DEINTERLACE_BOB );
				if ( pAppData->deinterlaceMode < 10 )
					hDeinterlace = NX_DeInterlaceOpen( (DEINTERLACE_MODE)pAppData->deinterlaceMode );
				else if ( pAppData->deinterlaceMode == 10 )
					hDeinterlace = (void *)NX_GTDeintOpen( seqOut.width, seqOut.height, MAX_GRAPHIC_BUF_SIZE );
				else if ( pAppData->deinterlaceMode == 20 );

				if ( hDeinterlace == NULL )
					printf("DeInterlace Init function is failed!!!\n");

#ifndef ANDROID
				for( int32_t i=0 ; i<DEINTERLACE_BUFFER_NUM ; i++ )
				{
					hVideoMemory[i] = NX_VideoAllocateMemory( 4096, seqOut.width, seqOut.height, NX_MEM_MAP_LINEAR, FOURCC_MVS0 );
					if( 0 == hVideoMemory[i] )
					{
						printf("NX_VideoAllocateMemory(64, %d, %d,..) failed.(i=%d)\n", seqOut.width, seqOut.height, i);
						return -1;
					}
				}
#endif
				printf("Deinterlace Initialization is Success!!\n");
			}
			else
				pAppData->deinterlaceMode = 0;
#endif

			printf("<<<<<<<<<<< Init In >>>>>>>>>>>>>> \n");
			printf("seqInfo = %x \n", seqIn.seqInfo);
			printf("seqSize = %d(0x%x) \n", seqIn.seqSize, seqIn.seqSize);
			{
				int32_t i;
				for (i=0 ; i<seqIn.seqSize ; i++)
				{
					printf("%2x ", seqIn.seqInfo[i]);
					if (i > 16) break;
					//if ( i > 128 ) break;
					//if (i % 32 == 0) printf("\n");
				}
				printf("\n");
			}
			printf("width = %d \n", seqIn.width);
			printf("heigh = %d \n", seqIn.height);
			printf("pMemHandle = %x \n", seqIn.pMemHandle);
			printf("numBuffers = %d \n", seqIn.numBuffers);
			printf("addNumBuffers = %d \n", seqIn.addNumBuffers);
			printf("disableOutReorder = %d \n", seqIn.disableOutReorder);
			printf("enablePostFilter = %d \n", seqIn.enablePostFilter);
			printf("enableUserData = %d \n", seqIn.enableUserData);

			printf("<<<<<<<<<<< Init Out >>>>>>>>>>>>>> \n");
			printf("minBuffers = %d \n", seqOut.minBuffers);
			printf("numBuffers = %d \n", seqOut.numBuffers);
			printf("width = %d \n", seqOut.width);
			printf("height = %d \n", seqOut.height);
			printf("frameBufDelay = %d \n", seqOut.frameBufDelay);
			printf("isInterace = %d \n", seqOut.isInterlace);
			printf("userDataNum = %d \n", seqOut.userDataNum);
			printf("userDataSize = %d \n", seqOut.userDataSize);
			printf("userDataBufFull = %d \n", seqOut.userDataBufFull);
			printf("frameRateNum = %d \n", seqOut.frameRateNum);
			printf("frameRateDen = %d \n", seqOut.frameRateDen);
			printf("vp8ScaleWidth = %d \n", seqOut.vp8ScaleWidth);
			printf("vp8ScaleHeight = %d \n", seqOut.vp8ScaleHeight);
			printf("unsupportedFeature = %d \n", seqOut.unsupportedFeature);

			bInit = 1;
			seqSize = 0;

			if( vpuCodecType != NX_HEVC_DEC )
				size = 0;
			if ( (vpuCodecType == NX_VC1_DEC) && (streamBuffer[0] == 0) && (streamBuffer[1] == 0) && (streamBuffer[2] == 1) )
				continue;
		}

		memset(&decIn, 0, sizeof(decIn));

		decIn.strmBuf = streamBuffer;
		decIn.strmSize = size;
		decIn.timeStamp = timeStamp;
		decIn.eos = ( decIn.strmSize > 0 || frameCount == 0 ) ? (0) : (1);

#ifdef _ERROR_AND_SEEK_
		if ( (decOut.outFrmReliable_0_100[DEC_DECODED_FRAME] != 100) && (decOut.outDecIdx >= 0) && (decIn.strmSize > 0) && (frameCount > 0) )
		{
			int32_t iFrameType;
			int32_t iCheckType = ( vpuCodecType == NX_AVC_DEC ) ? ( PIC_TYPE_IDR ) : ( PIC_TYPE_I );

			NX_VidDecGetFrameType( vpuCodecType, &decIn, &iFrameType );
			printf("[%d] Get Type = %d, size = %d \n", frameCount, iFrameType, size );

			if ( iFrameType != iCheckType )
			{
				size = 0;
				frameCount++;
				continue;
			}
			else
			{
				brokenB = 0;
				iPrevIdx = -1;
			}
		}

		if ( brokenB < 2 )
		{
			int32_t iFrameType;

			NX_VidDecGetFrameType( vpuCodecType, &decIn, &iFrameType );
			if ( iFrameType == PIC_TYPE_B )
			{
				size = 0;
				frameCount++;
				continue;
			}
		}
#endif

#ifdef _DUMP_ES_
		fwrite(decIn.strmBuf, 1, decIn.strmSize, fpES);
#endif

		startTime = NX_GetTickCount();
		vidRet = NX_VidDecDecodeFrame( hDec, &decIn, &decOut );
		endTime = NX_GetTickCount();
		totalTime += (endTime - startTime);

		printf("Frame[%5d]: size=%6d, DecIdx=%2d, DspIdx=%2d, InTimeStamp=%7lld, outTimeStamp=%7lld, %7lld, time=%6lld, interlace=%1d(%2d), Reliable=%3d, %3d, type = %d, %d, Rd = %6x, Wd = %6x, %d \n",
			frameCount, decIn.strmSize, decOut.outDecIdx, decOut.outImgIdx, decIn.timeStamp, decOut.timeStamp[FIRST_FIELD], decOut.timeStamp[SECOND_FIELD], (endTime-startTime), decOut.isInterlace, decOut.topFieldFirst,
			decOut.outFrmReliable_0_100[DECODED_FRAME], decOut.outFrmReliable_0_100[DISPLAY_FRAME], decOut.picType[DECODED_FRAME], decOut.picType[DISPLAY_FRAME], decOut.strmReadPos, decOut.strmWritePos, decOut.strmWritePos - decOut.strmReadPos );

		//printf("Frame[%5d]: size=%6d, DspIdx=%2d, DecIdx=%2d, InTimeStamp=%7lld, outTimeStamp=%7lld, %7lld, time=%6lld, interlace=%d(%d), Reliable=%3d, %3d, type = %d, %d, MultiResol=%d, upW=%d, upH=%d\n",
		//	frameCount, decIn.strmSize, decOut.outImgIdx, decOut.outDecIdx, decIn.timeStamp, decOut.timeStamp[FIRST_FIELD], decOut.timeStamp[SECOND_FIELD], (endTime-startTime), decOut.isInterlace, decOut.topFieldFirst,
		//	decOut.outFrmReliable_0_100, decOut.outFrmReliable_0_100[DISPLAY_FRAME], decOut.picType[DECODED_FRAME], decOut.picType[DISPLAY_FRAME], decOut.multiResolution, decOut.upSampledWidth, decOut.upSampledHeight);
		//printf("(%2x %2x %2x %2x %2x %2x %2x %2x %2x %2x %2x %2x %2x %2x %2x %2x)\n", decIn.strmBuf[0], decIn.strmBuf[1], decIn.strmBuf[2], decIn.strmBuf[3], decIn.strmBuf[4], decIn.strmBuf[5], decIn.strmBuf[6], decIn.strmBuf[7],
		//	decIn.strmBuf[8], decIn.strmBuf[9], decIn.strmBuf[10], decIn.strmBuf[11], decIn.strmBuf[12], decIn.strmBuf[13], decIn.strmBuf[14], decIn.strmBuf[15]);

		if( vidRet < 0 )
		{
			printf("Decoding Error!!!\n");
			if ( vidRet == VID_ERR_TIMEOUT )
				NX_VidDecFlush( hDec );
			//exit(-2);
		}

#ifdef _ERROR_AND_SEEK_
		if ( (brokenB < 2) && (decOut.outDecIdx >= 0) )
		{
			int32_t iFrameType;

			NX_VidDecGetFrameType( vpuCodecType, &decIn, &iFrameType );
			if ( (iFrameType == PIC_TYPE_I) || (iFrameType == PIC_TYPE_P) || (iFrameType == PIC_TYPE_IDR) )
			{
				if ( iPrevIdx != decOut.outDecIdx )
				{
					brokenB++;
				}
				iPrevIdx = decOut.outDecIdx;
			}
		}
#endif

		if( decOut.outImgIdx >= 0  )
		{
			NX_VID_MEMORY_HANDLE hDispBuff;
			int32_t OutImgIdx;

#ifdef DEINTERLACE_ENABLE
			if ( hDeinterlace )
			{
#ifdef ANDROID
				OutImgIdx = outCount % NUMBER_OF_BUFFER;
#else
				OutImgIdx = outCount % DEINTERLACE_BUFFER_NUM;
#endif
				hDispBuff = hVideoMemory[OutImgIdx];

				startTime = NX_GetTickCount();
				if ( pAppData->deinterlaceMode < 10 )
					vidRet = NX_DeInterlaceFrame( hDeinterlace, &decOut.outImg, decOut.topFieldFirst, hDispBuff );
				else if ( pAppData->deinterlaceMode == 10 )
					vidRet = NX_GTDeintDoDeinterlace( (NX_GT_DEINT_HANDLE)hDeinterlace, &decOut.outImg, hDispBuff );
				endTime = NX_GetTickCount();

				if ( vidRet < 0 )
				{
					printf( "%d Frame is failed in DeInterlace function!!!\n", frameCount );
					exit(-2);
				}
				printf("[%3d Frm]DeInterlace Timer = %6lld \n", outCount, endTime - startTime );
			}
#else
			OutImgIdx = decOut.outImgIdx;
			hDispBuff = &decOut.outImg;
#endif

#ifdef ANDROID
			pAndRender->DspQueueBuffer( NULL, OutImgIdx );
			if( prevIdx != -1 )
			{
				pAndRender->DspDequeueBuffer(NULL, NULL);
			}
#else
			NX_DspQueueBuffer( hDsp, hDispBuff );
			if( outCount != 0 )
			{
				NX_DspDequeueBuffer( hDsp );
			}
#endif

			if ( fpOut )
			{
				int32_t h;
				uint8_t *pbyImg = (uint8_t *)(hDispBuff->luVirAddr);

				for(h=0 ; h<hDispBuff->imgHeight ; h++)
				{
					fwrite(pbyImg, 1, h<hDispBuff->imgWidth, fpOut);
					pbyImg += hDispBuff->luStride;
				}

				pbyImg = (uint8_t *)(hDispBuff->cbVirAddr);
				for(h=0 ; h<hDispBuff->imgHeight/2 ; h++)
				{
					fwrite(pbyImg, 1, hDispBuff->imgWidth/2, fpOut);
					pbyImg += hDispBuff->cbStride;
				}

				pbyImg = (uint8_t *)(hDispBuff->crVirAddr);
				for(h=0 ; h<hDispBuff->imgHeight/2 ; h++)
				{
					fwrite(pbyImg, 1, hDispBuff->imgWidth/2, fpOut);
					pbyImg += hDispBuff->crStride;
				}
			}

			if( prevIdx != -1 )
			{
				NX_VidDecClrDspFlag( hDec, &decOut.outImg, prevIdx );
			}
			prevIdx = decOut.outImgIdx;
			outCount ++;
		}
		else if ( decIn.eos == 1)		break;

#ifdef _ERROR_AND_SEEK_
		if ( (decOut.outFrmReliable_0_100[DEC_DECODED_FRAME] != 100) && (decOut.outDecIdx >= 0) )
		{
			NX_VidDecFlush( hDec );
		}
#endif

		frameCount++;
	}

	NX_VidDecClose( hDec );

#ifdef ANDROID
	if( pAndRender )
		delete pAndRender;
#else
	if( hDsp )
		NX_DspClose(hDsp);

	printf("Avg Time = %6lld (%6lld / %d) \n", totalTime / frameCount, totalTime, frameCount);
#endif
	if( pMediaReader )
		delete pMediaReader;
	if ( fpOut )
		fclose(fpOut);
#ifdef _DUMP_ES_
	if ( fpES )
		fclose(fpES);
#endif
#ifdef DEINTERLACE_ENABLE
	if ( hDeinterlace )
	{
		if ( pAppData->deinterlaceMode < 10 )
			NX_DeInterlaceClose( hDeinterlace );
		else if ( pAppData->deinterlaceMode == 10 )
			NX_GTDeintClose( (NX_GT_DEINT_HANDLE)hDeinterlace );
	}
#endif

	return 0;
}
