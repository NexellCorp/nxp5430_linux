//
//	Nexel Video En/Decoder API
//
#include <stdlib.h>		//	malloc & free
#include <string.h>		//	memset
#include <unistd.h>		//	close

#include <sys/ioctl.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>


#include <nx_video_api.h>

#include <nx_fourcc.h>
#include <vpu_drv_ioctl.h>		//	Device Driver IOCTL
#include <nx_alloc_mem.h>		//	Memory Allocation Information


#define	WORK_BUF_SIZE		(  80*1024)
#define	STREAM_BUF_SIZE		(1024*1024*4)
#define	PS_SAVE_SIZE		( 320*1024)


//
//	Debug Message Configuration
//
#define	NX_DTAG		"[VPU|API] "		//
#include "api_osapi.h"
#define	DBG_BUF_ALLOC		0
#define	DBG_ENC_OUT			1
#define DBG_BUF_INFO		0
#define	DBG_VBS				0
#define	DBG_WARNING			1
#define	DBG_USER_DATA		0

#define	DEV_NAME		"/dev/nx_vpu"
#define	RECON_CHROMA_INTERLEAVED	0

#define SLICE_SAVE_SIZE                 (MAX_DEC_PIC_WIDTH*MAX_DEC_PIC_HEIGHT*3/4)

//----------------------------------------------------------------------------
//	define static functions
static int32_t AllocateEncoderMemory( NX_VID_ENC_HANDLE hEnc );
static int32_t FreeEncoderMemory( NX_VID_ENC_HANDLE hEnc );
static int32_t AllocateDecoderMemory( NX_VID_DEC_HANDLE hDec);
static int32_t FreeDecoderMemory( NX_VID_DEC_HANDLE hDec );
static void DecoderFlushTimeStamp( NX_VID_DEC_HANDLE hDec );
static void DecoderPutTimeStamp( NX_VID_DEC_HANDLE hDec, int32_t iIndex, VPU_DEC_DEC_FRAME_ARG *pDecArg, uint64_t lTimeStamp );
static uint64_t DecoderGetTimeStamp( NX_VID_DEC_HANDLE hDec, int32_t iIndex, int32_t *piPicType );


//////////////////////////////////////////////////////////////////////////////
//
//		Video Encoder APIs
//

struct NX_VIDEO_ENC_INFO
{
	//	open information
	int32_t hEncDrv;		                    //	Device Driver Handle
	int32_t codecMode;		                    //	(AVC_ENC = 0x10 / MP4_ENC = 0x12 / NX_JPEG_ENC=0x20 )
	int32_t instIndex;		                    //	Instance Index

	NX_MEMORY_HANDLE hInstanceBuf;				//	Encoder Instance Memory Buffer

	//	Frame Buffer Information ( for Initialization )
	int32_t refChromaInterleave;				//	Reconstruct & Referernce Buffer Chroma Interleaved
	NX_VID_MEMORY_HANDLE hRefReconBuf[2];		//	Reconstruct & Referernce Buffer Information
	NX_MEMORY_HANDLE hSubSampleBuf[2];			//	Sub Sample Buffer Address
	NX_MEMORY_HANDLE hBitStreamBuf;				//	Bit Stream Buffer
	int32_t isInitialized;

	//	Initialize Output Informations
	VPU_ENC_GET_HEADER_ARG seqInfo;

	//	Encoder Options ( Default CBR Mode )
	int32_t width;
	int32_t height;
	int32_t gopSize;							//	Group Of Pictures' Size
	int32_t frameRateNum;						//	Framerate numerator
	int32_t frameRateDen;						//	Framerate denominator
	int32_t bitRate;							//	BitRate
	int32_t enableSkip;							//	Enable skip frame

	int32_t userQScale;							//	Default User Qunatization Scale

	//	JPEG Specific
	uint32_t frameIndex;
	int32_t rstIntval;
};

NX_VID_ENC_HANDLE NX_VidEncOpen( int32_t codecType, int32_t *piInstanceIdx )
{
	VPU_OPEN_ARG openArg;
	int32_t ret;

	//	Create Context
	NX_VID_ENC_HANDLE hEnc = (NX_VID_ENC_HANDLE)malloc( sizeof(struct NX_VIDEO_ENC_INFO) );
	memset( hEnc, 0, sizeof(struct NX_VIDEO_ENC_INFO) );
	memset( &openArg, 0, sizeof(openArg) );

	FUNC_IN();

	//	Open Device Driver
	hEnc->hEncDrv = open(DEV_NAME, O_RDWR);
	if( hEnc->hEncDrv < 0 )
	{
		NX_ErrMsg( ("Cannot open device(%s)!!!\n", DEV_NAME) );
		goto ERROR_EXIT;
	}

	switch( codecType )
	{
		case NX_MP4_ENC:
			openArg.codecStd = CODEC_STD_MPEG4;
			break;
		case NX_AVC_ENC:
			openArg.codecStd = CODEC_STD_AVC;
			break;
		case NX_JPEG_ENC:
			openArg.codecStd = CODEC_STD_MJPG;
			break;
		case NX_H263_ENC:
			openArg.codecStd = CODEC_STD_H263;
			break;
		default:
			NX_ErrMsg( ("Invalid codec type (%d)!!!\n", codecType) );
			goto ERROR_EXIT;
	}

	//	Allocate Instance Memory & Stream Buffer
	hEnc->hInstanceBuf =  NX_AllocateMemory( WORK_BUF_SIZE, 4096 );		//	x16 aligned
	if( 0 == hEnc->hInstanceBuf ){
		NX_ErrMsg(("hInstanceBuf allocation failed.\n"));
		goto ERROR_EXIT;
	}

	openArg.instIndex = -1;
	openArg.isEncoder = 1;
	openArg.instanceBuf = *hEnc->hInstanceBuf;

	ret = ioctl( hEnc->hEncDrv, IOCTL_VPU_OPEN_INSTANCE, &openArg );
	if( ret < 0 )
	{
		NX_ErrMsg( ("NX_VidEncOpen() : IOCTL_VPU_OPEN_INSTANCE ioctl failed!!!\n") );
		goto ERROR_EXIT;
	}

	hEnc->instIndex = openArg.instIndex;
	hEnc->codecMode = codecType;
	hEnc->refChromaInterleave = RECON_CHROMA_INTERLEAVED;

	if ( piInstanceIdx )
		*piInstanceIdx = hEnc->instIndex;

	FUNC_OUT();
	return hEnc;

ERROR_EXIT:
	if( hEnc )
	{
		if( hEnc->hEncDrv > 0 )
		{
			close( hEnc->hEncDrv );
		}
		free( hEnc );
	}
	return NULL;
}

VID_ERROR_E NX_VidEncClose( NX_VID_ENC_HANDLE hEnc )
{
	int32_t ret;
	FUNC_IN();
	if( !hEnc )
	{
		NX_ErrMsg( ("Invalid encoder handle or driver handle!!!\n") );
		return VID_ERR_PARAM;
	}

	if( hEnc->hEncDrv <= 0 )
	{
		NX_ErrMsg( ("Invalid encoder handle or driver handle!!!\n") );
		free( hEnc );
		return VID_ERR_PARAM;
	}

	ret = ioctl( hEnc->hEncDrv, IOCTL_VPU_CLOSE_INSTANCE, 0 );
	if( ret < 0 )
	{
		ret = VID_ERR_FAIL;
		NX_ErrMsg( ("NX_VidEncClose() : IOCTL_VPU_CLOSE_INSTANCE ioctl failed!!!\n") );
	}

	FreeEncoderMemory( hEnc );
	close( hEnc->hEncDrv );
	free( hEnc );

	FUNC_OUT();
	return VID_ERR_NONE;
}

VID_ERROR_E NX_VidEncInit( NX_VID_ENC_HANDLE hEnc, NX_VID_ENC_INIT_PARAM *pstParam )
{
	int32_t ret;
	VPU_ENC_SEQ_ARG seqArg;
	VPU_ENC_SET_FRAME_ARG frameArg;
	VPU_ENC_GET_HEADER_ARG *pHdrArg = &hEnc->seqInfo;

	FUNC_IN();

	if( !hEnc )
	{
		NX_ErrMsg( ("Invalid encoder handle or driver handle!!!\n") );
		return VID_ERR_PARAM;
	}

	if( hEnc->hEncDrv <= 0 )
	{
		NX_ErrMsg( ("Invalid encoder handle or driver handle!!!\n") );
		return VID_ERR_PARAM;
	}

	memset( &seqArg, 0, sizeof( seqArg ) );
	memset( &frameArg, 0, sizeof( frameArg ) );
	memset( pHdrArg, 0, sizeof(VPU_ENC_GET_HEADER_ARG) );

	//	Initialize Encoder
	if( hEnc->isInitialized  )
	{
		NX_ErrMsg( ("Already initialized\n") );
		return VID_ERR_FAIL;
	}

	hEnc->width = pstParam->width;
	hEnc->height = pstParam->height;

	if( 0 != AllocateEncoderMemory( hEnc ) )
	{
		NX_ErrMsg( ("AllocateEncoderMemory() failed!!\n") );
		return VID_ERR_NOT_ALLOC_BUFF;
	}

	seqArg.srcWidth = pstParam->width;
	seqArg.srcHeight = pstParam->height;

	seqArg.chromaInterleave = pstParam->chromaInterleave;
	seqArg.refChromaInterleave = hEnc->refChromaInterleave;

	seqArg.rotAngle = pstParam->rotAngle;
	seqArg.mirDirection = pstParam->mirDirection;

	seqArg.strmBufPhyAddr = hEnc->hBitStreamBuf->phyAddr;
	seqArg.strmBufVirAddr = hEnc->hBitStreamBuf->virAddr;
	seqArg.strmBufSize = hEnc->hBitStreamBuf->size;

	if( hEnc->codecMode != NX_JPEG_ENC )
	{
		seqArg.gopSize = pstParam->gopSize;
		seqArg.frameRateNum = pstParam->fpsNum;
		seqArg.frameRateDen = pstParam->fpsDen;

		//	Rate Control
		seqArg.enableRC = pstParam->enableRC;
		if ( seqArg.enableRC )
		{
    		seqArg.bitrate = pstParam->bitrate;
    		seqArg.rcAutoSkip = pstParam->enableSkip;
			seqArg.initialDelay = pstParam->RCDelay;
			seqArg.vbvBufferSize = pstParam->rcVbvSize;
			seqArg.gammaFactor = ( pstParam->gammaFactor ) ? ( pstParam->gammaFactor ) : ((int)(0.75 * 32768));
		}
		else
		  hEnc->userQScale = (pstParam->userQScale == 0) ? (23) : (pstParam->userQScale);

		seqArg.searchRange = pstParam->searchRange;       // ME Search Range
		seqArg.intraRefreshMbs = pstParam->numIntraRefreshMbs;

    	if( hEnc->codecMode == NX_AVC_ENC )
		{
			if ( pstParam->enableAUDelimiter != 0 )
    			seqArg.enableAUDelimiter = 1;
			seqArg.maxQScale = ( pstParam->maxQScale > 0 ) ? ( pstParam->maxQScale ) : (51);
    	}
		else
		{
			seqArg.maxQScale = ( pstParam->maxQScale > 0 ) ? ( pstParam->maxQScale ) : (31);
		}
	}
	else
	{
		seqArg.frameRateNum = 1;
		seqArg.frameRateDen = 1;
		seqArg.gopSize = 1;
		seqArg.quality = pstParam->jpgQuality;
	}

	ret = ioctl( hEnc->hEncDrv, IOCTL_VPU_ENC_SET_SEQ_PARAM, &seqArg );
	if( ret < 0 )
	{
		NX_ErrMsg( ("IOCTL_VPU_ENC_SET_SEQ_PARAM ioctl failed!!!\n") );
		goto ERROR_EXIT;
	}

	if( hEnc->codecMode != NX_JPEG_ENC )
	{
		frameArg.numFrameBuffer = 2;		//	We use always 2 frame
		frameArg.frameBuffer[0] = *hEnc->hRefReconBuf[0];
		frameArg.frameBuffer[1] = *hEnc->hRefReconBuf[1];
		frameArg.subSampleBuffer[0] = *hEnc->hSubSampleBuf[0];
		frameArg.subSampleBuffer[1] = *hEnc->hSubSampleBuf[1];

		//	data partition mode always disabled ( for MPEG4 )
		frameArg.dataPartitionBuffer.phyAddr = 0;
		frameArg.dataPartitionBuffer.virAddr = 0;

		ret = ioctl( hEnc->hEncDrv, IOCTL_VPU_ENC_SET_FRAME_BUF, &frameArg );
		if( ret < 0 )
		{
			ret = VID_ERR_INIT;
			NX_ErrMsg( ("IOCTL_VPU_ENC_SET_FRAME_BUF ioctl failed!!!\n") );
			goto ERROR_EXIT;
		}

		ret = ioctl( hEnc->hEncDrv, IOCTL_VPU_ENC_GET_HEADER, pHdrArg );
		if( ret < 0 )
		{
			NX_ErrMsg( ("IOCTL_VPU_ENC_GET_HEADER ioctl failed!!!\n") );
			goto ERROR_EXIT;
		}
	}
	else
	{
		frameArg.numFrameBuffer = 0;
	}

	hEnc->isInitialized = 1;
	ret = VID_ERR_NONE;
	FUNC_OUT();

ERROR_EXIT:
	return ret;
}

VID_ERROR_E NX_VidEncGetSeqInfo( NX_VID_ENC_HANDLE hEnc, uint8_t *pbySeqBuf, int32_t *piSeqBufSize )
{
	FUNC_IN();
	if( !hEnc )
	{
		NX_ErrMsg( ("Invalid encoder handle or driver handle!!!\n") );
		return VID_ERR_PARAM;
	}

	if( hEnc->hEncDrv <= 0 )
	{
		NX_ErrMsg( ("Invalid encoder handle or driver handle!!!\n") );
		return VID_ERR_PARAM;
	}

	if( !hEnc->isInitialized )
	{
		NX_ErrMsg( ("Invalid encoder operation initialize first!!!\n") );
		return VID_ERR_INIT;
	}

	if( hEnc->codecMode == NX_JPEG_ENC || hEnc->codecMode == NX_H263_ENC )
	{
		*piSeqBufSize = 0;
		return VID_ERR_NONE;
	}

	memcpy( pbySeqBuf, hEnc->seqInfo.avcHeader.spsData, hEnc->seqInfo.avcHeader.spsSize );
	memcpy( pbySeqBuf+hEnc->seqInfo.avcHeader.spsSize, hEnc->seqInfo.avcHeader.ppsData, hEnc->seqInfo.avcHeader.ppsSize );
	*piSeqBufSize = hEnc->seqInfo.avcHeader.spsSize + hEnc->seqInfo.avcHeader.ppsSize;
	FUNC_OUT();
	return VID_ERR_NONE;
}

VID_ERROR_E NX_VidEncEncodeFrame( NX_VID_ENC_HANDLE hEnc, NX_VID_ENC_IN *pstEncIn, NX_VID_ENC_OUT *pstEncOut )
{
	int32_t ret;
	VPU_ENC_RUN_FRAME_ARG runArg;
	FUNC_IN();
	if( !hEnc )
	{
		NX_ErrMsg( ("Invalid encoder handle or driver handle!!!\n") );
		return VID_ERR_PARAM;
	}

	if( hEnc->hEncDrv <= 0 )
	{
		NX_ErrMsg( ("Invalid encoder handle or driver handle!!!\n") );
		return VID_ERR_PARAM;
	}

	memset( &runArg, 0, sizeof(runArg) );

	runArg.inImgBuffer = *(pstEncIn->pImage);
	//runArg.changeFlag = 0;
	//runArg.enableRc = 1;					//	N/A
	runArg.forceIPicture = pstEncIn->forcedIFrame;
	runArg.quantParam = pstEncIn->quantParam;
	runArg.skipPicture = pstEncIn->forcedSkipFrame;
	//pstEncIn->timeStamp;

	ret = ioctl( hEnc->hEncDrv, IOCTL_VPU_ENC_RUN_FRAME, &runArg );
	if( ret < 0 )
	{
		NX_ErrMsg( ("IOCTL_VPU_ENC_RUN_FRAME ioctl failed!!!\n") );
		return ret;
	}

	pstEncOut->width = hEnc->width;
	pstEncOut->height = hEnc->height;
	pstEncOut->frameType = runArg.frameType;
	pstEncOut->bufSize = runArg.outStreamSize;
	pstEncOut->outBuf = runArg.outStreamAddr;

//	NX_DbgMsg( DBG_ENC_OUT, ("Encoder Output : Success(outputSize = %d, isKey=%d)\n", pEncOut->bufSize, pEncOut->isKey) );
	FUNC_OUT();
	return 0;
}

VID_ERROR_E NX_VidEncChangeParameter( NX_VID_ENC_HANDLE hEnc, NX_VID_ENC_CHG_PARAM *pstChgParam )
{
	int32_t ret;
	VPU_ENC_CHG_PARA_ARG chgArg;
	FUNC_IN();
	if( !hEnc )
	{
		NX_ErrMsg( ("Invalid encoder handle or driver handle!!!\n") );
		return VID_ERR_PARAM;
	}

	if( hEnc->hEncDrv <= 0 )
	{
		NX_ErrMsg( ("Invalid encoder handle or driver handle!!!\n") );
		return VID_ERR_PARAM;
	}

	memset( &chgArg, 0, sizeof(chgArg) );

	chgArg.chgFlg = pstChgParam->chgFlg;
	chgArg.gopSize = pstChgParam->gopSize;
	chgArg.bitrate = pstChgParam->bitrate;
	chgArg.frameRateNum = pstChgParam->fpsNum;
	chgArg.frameRateDen = pstChgParam->fpsDen;
	chgArg.intraRefreshMbs = pstChgParam->numIntraRefreshMbs;

	ret = ioctl( hEnc->hEncDrv, IOCTL_VPU_ENC_CHG_PARAM, &chgArg );
	if( ret < 0 )
	{
		NX_ErrMsg( ("IOCTL_VPU_ENC_CHG_PARAM ioctl failed!!!\n") );
		return ret;
	}

	FUNC_OUT();
	return VID_ERR_NONE;
}

//
//		End of Encoder APIs
//
//////////////////////////////////////////////////////////////////////////////



//////////////////////////////////////////////////////////////////////////////
//
//	Jpeg Encoder APIs
//
VID_ERROR_E NX_VidEncJpegGetHeader( NX_VID_ENC_HANDLE hEnc, uint8_t *pbyJpgHeader, int32_t *piHeaderSize )
{
	int32_t ret;
	VPU_ENC_GET_HEADER_ARG *pHdrArg = (VPU_ENC_GET_HEADER_ARG *)calloc(sizeof(VPU_ENC_GET_HEADER_ARG), 1);
	FUNC_IN();
	ret = ioctl( hEnc->hEncDrv, IOCTL_VPU_JPG_GET_HEADER, pHdrArg );
	printf("pHdrArg->jpgHeader.headerSize = %d\n", pHdrArg->jpgHeader.headerSize);
	if( ret < 0 )
	{
		NX_ErrMsg( ("IOCTL_VPU_JPG_GET_HEADER ioctl failed!!!\n") );
	}
	else
	{
		memcpy( pbyJpgHeader, pHdrArg->jpgHeader.jpegHeader, pHdrArg->jpgHeader.headerSize );
		*piHeaderSize = pHdrArg->jpgHeader.headerSize;
	}
	FUNC_OUT();
	return ret;
}

VID_ERROR_E NX_VidEncJpegRunFrame( NX_VID_ENC_HANDLE hEnc, NX_VID_MEMORY_HANDLE hInImage, NX_VID_ENC_OUT *pstEncOut )
{
	int32_t ret;
	VPU_ENC_RUN_FRAME_ARG runArg;
	FUNC_IN();
	if( !hEnc )
	{
		NX_ErrMsg( ("Invalid encoder handle or driver handle!!!\n") );
		return -1;
	}

	if( hEnc->hEncDrv <= 0 )
	{
		NX_ErrMsg( ("Invalid encoder handle or driver handle!!!\n") );
		return -1;
	}

	memset( &runArg, 0, sizeof(runArg) );
	runArg.inImgBuffer = *hInImage;

	ret = ioctl( hEnc->hEncDrv, IOCTL_VPU_JPG_RUN_FRAME, &runArg );
	if( ret < 0 )
	{
		NX_ErrMsg( ("IOCTL_VPU_JPG_RUN_FRAME ioctl failed!!!\n") );
		return -1;
	}

	pstEncOut->width = hEnc->width;
	pstEncOut->height = hEnc->height;
	pstEncOut->bufSize = runArg.outStreamSize;
	pstEncOut->outBuf = runArg.outStreamAddr;
	FUNC_OUT();
	return 0;
}

//
//	Jpeg Encoder APIs
//
//////////////////////////////////////////////////////////////////////////////



//////////////////////////////////////////////////////////////////////////////
//
//	Video Decoder APIs
//

#define	PIC_FLAG_KEY		0x0001
#define	PIC_FLAG_INTERLACE	0x0010

struct NX_VIDEO_DEC_INFO
{
	//	open information
	int32_t hDecDrv;		//	Device Driver Handle
	int32_t codecStd;		//	NX_VPU_CODEC_MODE 	( AVC_DEC = 0, MP2_DEC = 2, MP4_DEC = 3, DV3_DEC = 3, RV_DEC = 4  )
	int32_t instIndex;		//	Instance Index

	int32_t width;
	int32_t height;

	//	Frame Buffer Information ( for Initialization )
	int32_t numFrameBuffers;
	NX_MEMORY_HANDLE hInstanceBuf;				//	Decoder Instance Memory Buffer
	NX_MEMORY_HANDLE hBitStreamBuf;				//	Bit Stream Buffer
	NX_VID_MEMORY_HANDLE hFrameBuffer[MAX_DEC_FRAME_BUFFERS];		//	Reconstruct & Referernce Buffer Information
	NX_MEMORY_HANDLE hColMvBuffer;				//	All Codecs
	NX_MEMORY_HANDLE hSliceBuffer;				//	AVC codec
	NX_MEMORY_HANDLE hPvbSliceBuffer;			//	PVX codec

	int32_t enableUserData;							//	User Data Mode Enable/Disable
	NX_MEMORY_HANDLE hUserDataBuffer;			//	User Data ( MPEG2 Only )

	int32_t isInitialized;

	int32_t useExternalFrameBuffer;
	int32_t numBufferableBuffers;

	//	Initialize Output Informations
	uint8_t	pSeqData[2048];				//	SPS PPS (H.264) or Decoder Specific Information(for MPEG4)
	int32_t seqDataSize;

	uint64_t timeStamp[MAX_DEC_FRAME_BUFFERS];
	int32_t picType[MAX_DEC_FRAME_BUFFERS];
	int32_t picFlag[MAX_DEC_FRAME_BUFFERS];
};


NX_VID_DEC_HANDLE NX_VidDecOpen( int32_t iCodecType, uint32_t uMp4Class, int32_t iOptions, int32_t *piInstanceIdx )
{
	int32_t ret;
	VPU_OPEN_ARG openArg;
	int32_t workBufSize = WORK_BUF_SIZE;
	FUNC_IN();
	//	Create Context
	NX_VID_DEC_HANDLE hDec = (NX_VID_DEC_HANDLE)malloc( sizeof(struct NX_VIDEO_DEC_INFO) );

	memset( hDec, 0, sizeof(struct NX_VIDEO_DEC_INFO) );
	memset( &openArg, 0, sizeof(openArg) );

	//	Open Device Driver
	hDec->hDecDrv = open(DEV_NAME, O_RDWR);
	if( hDec->hDecDrv < 0 )
	{
		NX_ErrMsg(("Cannot open device(%s)!!!\n", DEV_NAME));
		goto ERROR_EXIT;
	}

	if( iCodecType == NX_AVC_DEC || iCodecType == NX_AVC_ENC )
	{
		workBufSize += PS_SAVE_SIZE;
	}
	hDec->hBitStreamBuf = NX_AllocateMemory( STREAM_BUF_SIZE, 4096 );	//	x16 aligned
	if( 0 == hDec->hBitStreamBuf ){
		NX_ErrMsg(("hBitStreamBuf allocation failed.\n"));
		goto ERROR_EXIT;
	}

	//	Allocate Instance Memory & Stream Buffer
	hDec->hInstanceBuf =  NX_AllocateMemory( workBufSize, 4096 );		//	x16 aligned
	if( 0 == hDec->hInstanceBuf ){
		NX_ErrMsg(("hInstanceBuf allocation failed.\n"));
		goto ERROR_EXIT;
	}

	switch( iCodecType )
	{
		case NX_AVC_DEC:
			openArg.codecStd = CODEC_STD_AVC;
			break;
		case NX_MP2_DEC:
			openArg.codecStd = CODEC_STD_MPEG2;
			break;
		case NX_MP4_DEC:
			openArg.codecStd = CODEC_STD_MPEG4;
			openArg.mp4Class = uMp4Class;
			break;
		case NX_H263_DEC:	//
			openArg.codecStd = CODEC_STD_H263;
			break;
		case NX_DIV3_DEC:	//
			openArg.codecStd = CODEC_STD_DIV3;
			break;
		case NX_RV_DEC:		// Real Video
			openArg.codecStd = CODEC_STD_RV;
			break;
		case NX_VC1_DEC:	//	WMV
			openArg.codecStd = CODEC_STD_VC1;
			break;
		case NX_VPX_THEORA:	//	Theora
			openArg.codecStd = CODEC_STD_THO;
			break;
		case NX_VPX_VP8:	//	VP8
			openArg.codecStd = CODEC_STD_VP8;
			break;
		default:
			NX_ErrMsg( ("IOCTL_VPU_OPEN_INSTANCE codec Type\n") );
			goto ERROR_EXIT;
	}

	openArg.instIndex = -1;
	openArg.instanceBuf = *hDec->hInstanceBuf;
	openArg.streamBuf = *hDec->hBitStreamBuf;

	if( iOptions && DEC_OPT_CHROMA_INTERLEAVE )
	{
		openArg.chromaInterleave = 1;
	}

	ret = ioctl( hDec->hDecDrv, IOCTL_VPU_OPEN_INSTANCE, &openArg );
	if( ret < 0 )
	{
		NX_ErrMsg( ("IOCTL_VPU_OPEN_INSTANCE ioctl failed!!!\n") );
		goto ERROR_EXIT;
	}
	hDec->instIndex = openArg.instIndex;
	hDec->codecStd = openArg.codecStd;

	if ( piInstanceIdx )
		*piInstanceIdx = hDec->instIndex;

	DecoderFlushTimeStamp(hDec);

	FUNC_OUT();
	return hDec;

ERROR_EXIT:
	if( hDec->hDecDrv > 0 )
	{
		if( hDec->hInstanceBuf )
		{
			NX_FreeMemory(hDec->hInstanceBuf);
		}
		if( hDec->hBitStreamBuf )
		{
			NX_FreeMemory(hDec->hBitStreamBuf);
		}
		close( hDec->hDecDrv );
		free( hDec );
	}
	return 0;
}


VID_ERROR_E NX_VidDecClose( NX_VID_DEC_HANDLE hDec )
{
	int32_t ret;
	FUNC_IN();
	if( !hDec )
	{
		NX_ErrMsg( ("Invalid encoder handle or driver handle!!!\n") );
		return -1;
	}

	if( hDec->hDecDrv <= 0 )
	{
		NX_ErrMsg( ("Invalid encoder handle or driver handle!!!\n") );
		return -1;
	}

	ret = ioctl( hDec->hDecDrv, IOCTL_VPU_CLOSE_INSTANCE, 0 );
	if( ret < 0 )
	{
		NX_ErrMsg( ("IOCTL_VPU_CLOSE_INSTANCE ioctl failed!!!\n") );
	}

	FreeDecoderMemory( hDec );

	close( hDec->hDecDrv );

	free( hDec );

	FUNC_OUT();
	return 0;
}

VID_ERROR_E NX_VidDecParseVideoCfg(NX_VID_SEQ_IN *pstSeqIn, NX_VID_SEQ_OUT *pstSeqOut)
{
	return 0;
}

VID_ERROR_E NX_VidDecInit(NX_VID_DEC_HANDLE hDec, NX_VID_SEQ_IN *pstSeqIn, NX_VID_SEQ_OUT *pstSeqOut)
{
	int32_t i, ret=-1;
	VPU_DEC_SEQ_INIT_ARG seqArg;
	VPU_DEC_REG_FRAME_ARG frameArg;

	FUNC_IN();
	memset( &seqArg, 0, sizeof(seqArg) );
	memset( &frameArg, 0, sizeof(frameArg) );

	if( !hDec )
	{
		NX_ErrMsg( ("Invalid encoder handle or driver handle!!!\n") );
		goto ERROR_EXIT;
	}
	if( hDec->hDecDrv <= 0 )
	{
		NX_ErrMsg( ("Invalid encoder handle or driver handle!!!\n") );
		goto ERROR_EXIT;
	}
	//	Initialize Encoder
	if( hDec->isInitialized  )
	{
		NX_ErrMsg( ("Already initialized\n") );
		goto ERROR_EXIT;
	}

	seqArg.seqData        	= pstSeqIn->seqInfo;
	seqArg.seqDataSize    	= pstSeqIn->seqSize;
	seqArg.outWidth 		= pstSeqIn->width;
	seqArg.outHeight 		= pstSeqIn->height;

	if( pstSeqIn->disableOutReorder )
	{
		NX_DbgMsg( DBG_WARNING, ("Diable Reordering!!!!\n") );
		seqArg.disableOutReorder = 1;
	}

	seqArg.enablePostFilter = pstSeqIn->enablePostFilter;
	seqArg.enableUserData   = pstSeqIn->enableUserData && (hDec->codecStd == CODEC_STD_MPEG2);

	if( seqArg.enableUserData )
	{
		NX_DbgMsg(DBG_USER_DATA, ("Enabled user data\n"));
		hDec->enableUserData = 1;
		hDec->hUserDataBuffer = NX_AllocateMemory( 0x10000, 4096 );		//	x16 aligned
		if( 0 == hDec->hUserDataBuffer ){
			NX_ErrMsg(("hUserDataBuffer allocation failed.(size=%d,align=%d)\n", 0x10000, 4096));
			goto ERROR_EXIT;
		}
		seqArg.userDataBuffer = *hDec->hUserDataBuffer;
	}

	ret = ioctl( hDec->hDecDrv, IOCTL_VPU_DEC_SET_SEQ_INFO, &seqArg );
	if( ret == VID_NEED_STREAM )
		goto ERROR_EXIT;
	if( ret < 0 )
	{
		NX_ErrMsg( ("IOCTL_VPU_DEC_SET_SEQ_INFO ioctl failed!!!\n") );
		goto ERROR_EXIT;
	}

	if( seqArg.minFrameBufCnt < 1 || seqArg.minFrameBufCnt > MAX_DEC_FRAME_BUFFERS )
	{
		NX_ErrMsg( ("IOCTL_VPU_DEC_SET_SEQ_INFO ioctl failed(nimFrameBufCnt = %d)!!!\n", seqArg.minFrameBufCnt) );
		goto ERROR_EXIT;
	}

	if( pstSeqIn->numBuffers > 0 )
	{
		hDec->useExternalFrameBuffer = 1;
		hDec->numFrameBuffers = pstSeqIn->numBuffers;
	}
	else
	{
		hDec->numFrameBuffers = seqArg.minFrameBufCnt + pstSeqIn->addNumBuffers;
	}

	//	Allocation & Save Parameter in the decoder handle.
	hDec->width  = seqArg.outWidth;
	hDec->height = seqArg.outHeight;

	if( 0 != AllocateDecoderMemory( hDec ) )
	{
		ret = VID_ERR_NOT_ALLOC_BUFF;
		NX_ErrMsg(("AllocateDecoderMemory() Failed!!!\n"));
		goto ERROR_EXIT;
	}

	//	Set Frame Argement Valiable
	frameArg.numFrameBuffer = hDec->numFrameBuffers;
	for( i=0 ; i< hDec->numFrameBuffers ; i++ )
	{
		if( hDec->useExternalFrameBuffer )
			hDec->hFrameBuffer[i] = pstSeqIn->pMemHandle[i];
		frameArg.frameBuffer[i] = *hDec->hFrameBuffer[i];
	}
	if( hDec->hSliceBuffer )
		frameArg.sliceBuffer = *hDec->hSliceBuffer;
	if( hDec->hColMvBuffer)
		frameArg.colMvBuffer = *hDec->hColMvBuffer;

	ret = ioctl( hDec->hDecDrv, IOCTL_VPU_DEC_REG_FRAME_BUF, &frameArg );
	if( ret < 0 )
	{
		NX_ErrMsg( ("IOCTL_VPU_DEC_REG_FRAME_BUF ioctl failed!!!\n") );
		goto ERROR_EXIT;
	}

	pstSeqOut->minBuffers   = seqArg.minFrameBufCnt;
	pstSeqOut->numBuffers   = hDec->numFrameBuffers;
	if( (pstSeqOut->numBuffers - pstSeqOut->minBuffers < 2) && hDec->useExternalFrameBuffer )
	{
		NX_DbgMsg( DBG_WARNING, ("[Warning] External Buffer too small.(min=%d, buffers=%d)\n", pstSeqOut->minBuffers, hDec->numFrameBuffers) );
	}

	pstSeqOut->width        = seqArg.outWidth;
	pstSeqOut->height       = seqArg.outHeight;
	pstSeqOut->frameBufDelay= seqArg.frameBufDelay;
	pstSeqOut->isInterlace 	= seqArg.interlace;

	pstSeqOut->frameRateNum = seqArg.frameRateNum;	//	Frame Rate Numerator
	pstSeqOut->frameRateDen = seqArg.frameRateDen;	//	Frame Rate Denominator
	NX_DbgMsg(0, ("frameRateNum = %d, frameRateDen = %d\n", pstSeqOut->frameRateNum, pstSeqOut->frameRateDen));

	if (seqArg.vp8HScaleFactor == 0)		pstSeqOut->vp8ScaleWidth  = 0;
	else if (seqArg.vp8HScaleFactor == 1) 	pstSeqOut->vp8ScaleWidth  = seqArg.vp8ScaleWidth * 5 / 4;
	else if (seqArg.vp8HScaleFactor == 2) 	pstSeqOut->vp8ScaleWidth  = seqArg.vp8ScaleWidth * 5 / 3;
	else if (seqArg.vp8HScaleFactor == 3) 	pstSeqOut->vp8ScaleWidth  = seqArg.vp8ScaleWidth * 2;

	if (seqArg.vp8VScaleFactor == 0)		pstSeqOut->vp8ScaleHeight  = 0;
	else if (seqArg.vp8VScaleFactor == 1) 	pstSeqOut->vp8ScaleHeight  = seqArg.vp8ScaleHeight * 5 / 4;
	else if (seqArg.vp8VScaleFactor == 2) 	pstSeqOut->vp8ScaleHeight  = seqArg.vp8ScaleHeight * 5 / 3;
	else if (seqArg.vp8VScaleFactor == 3) 	pstSeqOut->vp8ScaleHeight  = seqArg.vp8ScaleHeight * 2;

	// TBD.
	pstSeqOut->userDataNum = 0;
	pstSeqOut->userDataSize = 0;
	pstSeqOut->userDataBufFull = 0;

	pstSeqOut->unsupportedFeature = 0;

	hDec->isInitialized = 1;
	FUNC_OUT();
	return ret;

ERROR_EXIT:
	return ret;
}


static void DecoderFlushTimeStamp( NX_VID_DEC_HANDLE hDec )
{
	int32_t i;
	for( i=0 ; i<MAX_DEC_FRAME_BUFFERS ;i++ )
	{
		hDec->timeStamp[i] = -1;
	}
}

static void DecoderPutTimeStamp( NX_VID_DEC_HANDLE hDec, int32_t iIndex, VPU_DEC_DEC_FRAME_ARG *pDecArg, uint64_t lTimeStamp )
{
	hDec->timeStamp[ iIndex ] = lTimeStamp;
	hDec->picType[ iIndex ] = pDecArg->picType;

	if( pDecArg->isInterace )
		hDec->picFlag[ iIndex ] |= PIC_FLAG_INTERLACE;

	if( hDec->codecStd == CODEC_STD_AVC )
	{
		if( pDecArg->picTypeFirst == 5 || pDecArg->picType == 0 || pDecArg->picType == 5 )
		{
			hDec->picFlag[ iIndex ] |= PIC_FLAG_KEY;
		}
	}
}

static uint64_t DecoderGetTimeStamp( NX_VID_DEC_HANDLE hDec, int32_t iIndex, int32_t *piPicType )
{
	*piPicType = PIC_TYPE_UNKNOWN;
	if( iIndex < 0 )
		return -1;

	if( hDec->picFlag[ iIndex ] & PIC_FLAG_KEY )
	{
		*piPicType = 0;
	}
	else
	{
		*piPicType = hDec->picType[ iIndex ];
	}

	hDec->picFlag[ iIndex ] = 0;		//	Clear Flag

	return hDec->timeStamp[ iIndex ];
}


VID_ERROR_E NX_VidDecDecodeFrame( NX_VID_DEC_HANDLE hDec, NX_VID_DEC_IN *pstDecIn, NX_VID_DEC_OUT *pstDecOut )
{
	int32_t ret;
	VPU_DEC_DEC_FRAME_ARG decArg;

	FUNC_IN();
	//	Initialize Encoder
	if( !hDec->isInitialized  )
	{
		NX_ErrMsg( ("%s Line(%d) : Not initialized!!!\n", __func__, __LINE__));
		return -1;
	}

	memset( &decArg, 0, sizeof(decArg) );
	decArg.strmData = pstDecIn->strmBuf;
	decArg.strmDataSize = pstDecIn->strmSize;
	decArg.eos = pstDecIn->eos;
	decArg.iFrameSearchEnable = 0;
	decArg.skipFrameMode = 0;
	decArg.decSkipFrameNum = 0;
	pstDecOut->outImgIdx = -1;

	ret = ioctl( hDec->hDecDrv, IOCTL_VPU_DEC_RUN_FRAME, &decArg );
	if( ret < 0 )
	{
		NX_ErrMsg( ("IOCTL_VPU_DEC_RUN_FRAME ioctl failed!!!\n") );
		return -1;
	}

	if( ret > 0 )
		return ret;

	pstDecOut->outImgIdx = decArg.indexFrameDisplay;
	pstDecOut->outDecIdx = decArg.indexFrameDecoded;
	pstDecOut->width     = decArg.outWidth;
	pstDecOut->height    = decArg.outHeight;
	pstDecOut->picType   = PIC_TYPE_UNKNOWN;

	pstDecOut->strmReadPos  = decArg.strmReadPos;
	pstDecOut->strmWritePos = decArg.strmWritePos;

	pstDecOut->isInterlace 	= decArg.isInterace;
	pstDecOut->topFieldFirst= decArg.topFieldFirst;

	if (decArg.numOfErrMBs == 0)
		pstDecOut->outFrmReliable_0_100 = 100;
	else
	{
		int TotalMbNum = ((decArg.outWidth+15)>>4) * ((decArg.outHeight+15)>>4);
		pstDecOut->outFrmReliable_0_100 = (TotalMbNum - decArg.numOfErrMBs) * 100 / TotalMbNum;
	}

	pstDecOut->multiResolution = decArg.multiRes;

	if (hDec->codecStd == CODEC_STD_VP8)
	{
		if (decArg.vp8ScaleInfo.hScaleFactor == 0) 		pstDecOut->upSampledWidth = 0;
		else if (decArg.vp8ScaleInfo.hScaleFactor == 1)	pstDecOut->upSampledWidth = decArg.vp8ScaleInfo.picWidth * 5 / 4;
		else if (decArg.vp8ScaleInfo.hScaleFactor == 2)	pstDecOut->upSampledWidth = decArg.vp8ScaleInfo.picWidth * 5 / 3;
		else if (decArg.vp8ScaleInfo.hScaleFactor == 3)	pstDecOut->upSampledWidth = decArg.vp8ScaleInfo.picWidth * 2;

		if (decArg.vp8ScaleInfo.vScaleFactor == 0) 		pstDecOut->upSampledHeight = 0;
		else if (decArg.vp8ScaleInfo.vScaleFactor == 1)	pstDecOut->upSampledHeight = decArg.vp8ScaleInfo.picHeight * 5 / 4;
		else if (decArg.vp8ScaleInfo.vScaleFactor == 2)	pstDecOut->upSampledHeight = decArg.vp8ScaleInfo.picHeight * 5 / 3;
		else if (decArg.vp8ScaleInfo.vScaleFactor == 3)	pstDecOut->upSampledHeight = decArg.vp8ScaleInfo.picHeight * 2;
	}
	else
	{
		pstDecOut->upSampledWidth  = 0;
		pstDecOut->upSampledHeight = 0;
	}

	DecoderPutTimeStamp( hDec, pstDecOut->outDecIdx, &decArg, pstDecIn->timeStamp );

	if( pstDecOut->outImgIdx >= 0 && hDec->numFrameBuffers > pstDecOut->outImgIdx )
	{
		int32_t picType;
		pstDecOut->outImg = *hDec->hFrameBuffer[decArg.indexFrameDisplay];
		pstDecOut->timeStamp = DecoderGetTimeStamp( hDec, decArg.indexFrameDisplay, &picType );
		pstDecOut->picType = picType;
#if DBG_BUF_INFO
		// {
		// 	int32_t j=0;
		// 	NX_MEMORY_INFO *memInfo;
		// 	for( j=0 ; j<3 ; j++ )
		// 	{
		// 		memInfo = (NX_MEMORY_INFO *)pDecOut->outImg.privateDesc[j];
		// 		NX_DbgMsg( DBG_BUF_INFO, ("privateDesc = 0x%.8x\n", memInfo->privateDesc ) );
		// 		NX_DbgMsg( DBG_BUF_INFO, ("align       = 0x%.8x\n", memInfo->align       ) );
		// 		NX_DbgMsg( DBG_BUF_INFO, ("size        = 0x%.8x\n", memInfo->size        ) );
		// 		NX_DbgMsg( DBG_BUF_INFO, ("virAddr     = 0x%.8x\n", memInfo->virAddr     ) );
		// 		NX_DbgMsg( DBG_BUF_INFO, ("phyAddr     = 0x%.8x\n", memInfo->phyAddr     ) );
		// 	}
		// }
		{
			NX_VID_MEMORY_INFO *memInfo = &pDecOut->outImg;
			NX_DbgMsg( DBG_BUF_INFO, ("Phy(0x%08x,0x%08x,0x%08x), Vir(0x%08x,0x%08x,0x%08x), Stride(0x%08x,0x%08x,0x%08x)\n",
				memInfo->luPhyAddr, memInfo->cbPhyAddr, memInfo->crPhyAddr,
				memInfo->luVirAddr, memInfo->cbVirAddr, memInfo->crVirAddr,
				memInfo->luStride , memInfo->cbStride , memInfo->crStride) );
		}
#endif
	}
	else
	{
		pstDecOut->outImgIdx = -1;
		pstDecOut->timeStamp = -1;
	}

	NX_RelMsg( 0, ("NX_VidDecDecodeFrame() Resol:%dx%d, picType=%d, imgIdx = %d\n", pstDecOut->width, pstDecOut->height, pstDecOut->picType, pstDecOut->outImgIdx) );
	FUNC_OUT();
	return 0;
}

VID_ERROR_E NX_VidDecFlush( NX_VID_DEC_HANDLE hDec )
{
	int32_t ret;
	FUNC_IN();
	if( !hDec->isInitialized  )
	{
		NX_ErrMsg( ("%s Line(%d) : Not initialized!!!\n", __func__, __LINE__));
		return -1;
	}
	ret = ioctl( hDec->hDecDrv, IOCTL_VPU_DEC_FLUSH, NULL );
	if( ret < 0 )
	{
		NX_ErrMsg( ("IOCTL_VPU_DEC_FLUSH ioctl failed!!!\n") );
		return -1;
	}

	DecoderFlushTimeStamp( hDec );

	FUNC_OUT();
	return 0;
}

VID_ERROR_E NX_VidDecClrDspFlag( NX_VID_DEC_HANDLE hDec, NX_VID_MEMORY_HANDLE hFrameBuf, int32_t iFrameIdx )
{
	int32_t ret;
	FUNC_IN();
	VPU_DEC_CLR_DSP_FLAG_ARG clrFlagArg;
	if( !hDec->isInitialized  )
	{
		return -1;
	}
	clrFlagArg.indexFrameDisplay = iFrameIdx;
	if( NULL != hFrameBuf )
	{
		//	Optional
		clrFlagArg.frameBuffer = *hFrameBuf;
	}
	else
	{
		memset( &clrFlagArg.frameBuffer, 0, sizeof(clrFlagArg.frameBuffer) );
	}

	ret = ioctl( hDec->hDecDrv, IOCTL_VPU_DEC_CLR_DSP_FLAG, &clrFlagArg );
	if( ret < 0 )
	{
		NX_ErrMsg( ("IOCTL_VPU_DEC_CLR_DSP_FLAG ioctl failed!!!\n") );
		return -1;
	}

	FUNC_OUT();
	return 0;
}


//
//	Video Decoder APIs
//
//////////////////////////////////////////////////////////////////////////////





//////////////////////////////////////////////////////////////////////////////
//
//	Static Internal Functions
//

static int32_t AllocateEncoderMemory( NX_VID_ENC_HANDLE hEnc )
{
	int32_t width, height;

	if( !hEnc || hEnc->hEncDrv<=0 )
	{
		NX_ErrMsg(("invalid encoder handle or driver handle!!!\n"));
		return -1;
	}

	//	Make alligned x16
	width  = ((hEnc->width  + 15) >> 4)<<4;
	height = ((hEnc->height + 15) >> 4)<<4;

	if( hEnc->codecMode == NX_JPEG_ENC )
	{
		int32_t jpegStreamBufSize = width * height * 1.5;
		hEnc->hRefReconBuf[0] = NULL;
		hEnc->hRefReconBuf[1] = NULL;
		hEnc->hSubSampleBuf[0] = NULL;
		hEnc->hSubSampleBuf[1] = NULL;
		hEnc->hBitStreamBuf = NX_AllocateMemory( jpegStreamBufSize, 16 );		//	x16 aligned
		if( 0 == hEnc->hBitStreamBuf ){
			NX_ErrMsg(("hBitStreamBuf allocation failed.(size=%d,align=%d)\n", ENC_BITSTREAM_BUFFER, 16));
			goto ERROR_EXIT;
		}
	}
	else
	{
		int32_t fourcc = FOURCC_MVS0;
		if( hEnc->refChromaInterleave ){
			fourcc = FOURCC_NV12;	//	2 Planar 420( Luminunce Plane + Cb/Cr Interleaved Plane )
		}
		else
		{
			fourcc = FOURCC_MVS0;	//	3 Planar 420( Luminunce plane + Cb Plane + Cr Plane )
		}

		hEnc->hRefReconBuf[0] = NX_VideoAllocateMemory( 64, width, height, NX_MEM_MAP_LINEAR, fourcc );
		if( 0 == hEnc->hRefReconBuf[0] ){
			NX_ErrMsg(("NX_VideoAllocateMemory(64,%d,%d,..) failed.(recon0)\n", width, height));
			goto ERROR_EXIT;
		}

		hEnc->hRefReconBuf[1] = NX_VideoAllocateMemory( 64, width, height, NX_MEM_MAP_LINEAR, fourcc );
		if( 0 == hEnc->hRefReconBuf[1] ){
			NX_ErrMsg(("NX_VideoAllocateMemory(64,%d,%d,..) failed.(recon1)\n", width, height));
			goto ERROR_EXIT;
		}

		hEnc->hSubSampleBuf[0] = NX_AllocateMemory( width*height/4, 16 );	//	x16 aligned
		if( 0 == hEnc->hSubSampleBuf[0] ){
			NX_ErrMsg(("hSubSampleBuf allocation failed.(size=%d,align=%d)\n", width*height, 16));
			goto ERROR_EXIT;
		}

		hEnc->hSubSampleBuf[1] = NX_AllocateMemory( width*height/4, 16 );	//	x16 aligned
		if( 0 == hEnc->hSubSampleBuf[1] ){
			NX_ErrMsg(("hSubSampleBuf allocation failed.(size=%d,align=%d)\n", width*height, 16));
			goto ERROR_EXIT;
		}

		hEnc->hBitStreamBuf = NX_AllocateMemory( ENC_BITSTREAM_BUFFER, 16 );		//	x16 aligned
		if( 0 == hEnc->hBitStreamBuf ){
			NX_ErrMsg(("hBitStreamBuf allocation failed.(size=%d,align=%d)\n", ENC_BITSTREAM_BUFFER, 16));
			goto ERROR_EXIT;
		}
	}

#if (DBG_BUF_ALLOC)
	NX_DbgMsg( DBG_BUF_ALLOC, ("Allocate Encoder Memory\n") );
	NX_DbgMsg( DBG_BUF_ALLOC, ("    hRefReconBuf[0]  : LuPhy(0x%08x), CbPhy(0x%08x), CrPhy(0x%08x)\n", hEnc->hRefReconBuf[0]->luPhyAddr, hEnc->hRefReconBuf[0]->cbPhyAddr, hEnc->hRefReconBuf[0]->crPhyAddr) );
	NX_DbgMsg( DBG_BUF_ALLOC, ("    hRefReconBuf[1]  : LuPhy(0x%08x), CbPhy(0x%08x), CrPhy(0x%08x)\n", hEnc->hRefReconBuf[1]->luPhyAddr, hEnc->hRefReconBuf[1]->cbPhyAddr, hEnc->hRefReconBuf[1]->crPhyAddr) );
	NX_DbgMsg( DBG_BUF_ALLOC, ("    hSubSampleBuf[0] : PhyAddr(0x%08x), VirAddr(0x%08x)\n", hEnc->hSubSampleBuf[0]->phyAddr, hEnc->hSubSampleBuf[0]->virAddr) );
	NX_DbgMsg( DBG_BUF_ALLOC, ("    hSubSampleBuf[1] : PhyAddr(0x%08x), VirAddr(0x%08x)\n", hEnc->hSubSampleBuf[1]->phyAddr, hEnc->hSubSampleBuf[1]->virAddr) );
	NX_DbgMsg( DBG_BUF_ALLOC, ("    hBitStreamBuf    : PhyAddr(0x%08x), VirAddr(0x%08x)\n", hEnc->hBitStreamBuf->phyAddr, hEnc->hBitStreamBuf->virAddr) );
#endif	//	DBG_BUF_ALLOC

	return 0;

ERROR_EXIT:
	FreeEncoderMemory( hEnc );
	return -1;
}

static int32_t FreeEncoderMemory( NX_VID_ENC_HANDLE hEnc )
{
	if( !hEnc )
	{
		NX_ErrMsg(("invalid encoder handle!!!\n"));
		return -1;
	}

	//	Free Reconstruct Buffer & Reference Buffer
	if( hEnc->hRefReconBuf[0] )
	{
		NX_FreeVideoMemory( hEnc->hRefReconBuf[0] );
		hEnc->hRefReconBuf[0] = 0;
	}
	if( hEnc->hRefReconBuf[1] )
	{
		NX_FreeVideoMemory( hEnc->hRefReconBuf[1] );
		hEnc->hRefReconBuf[1] = 0;
	}

	//	Free SubSampleb Buffer
	if( hEnc->hSubSampleBuf[0] )
	{
		NX_FreeMemory( hEnc->hSubSampleBuf[0] );
		hEnc->hSubSampleBuf[0] = 0;
	}
	if( hEnc->hSubSampleBuf[1] )
	{
		NX_FreeMemory( hEnc->hSubSampleBuf[1] );
		hEnc->hSubSampleBuf[1] = 0;
	}

	//	Free Bitstream Buffer
	if( hEnc->hBitStreamBuf )
	{
		NX_FreeMemory( hEnc->hBitStreamBuf );
		hEnc->hBitStreamBuf = 0;
	}

	if( hEnc->hInstanceBuf )
	{
		NX_FreeMemory(hEnc->hInstanceBuf);
		hEnc->hInstanceBuf = 0;
	}

	return 0;
}


static int32_t AllocateDecoderMemory( NX_VID_DEC_HANDLE hDec )
{
	int32_t i, width, height, mvSize;

	if( !hDec || !hDec->hDecDrv )
	{
		NX_ErrMsg(("invalid encoder handle or driver handle!!!\n"));
		return -1;
	}

	//	Make alligned x16
	width  = ((hDec->width  + 15) >> 4)<<4;
	height = ((hDec->height + 15) >> 4)<<4;

	//
	mvSize = ((hDec->width+31)&~31)*((hDec->height+31)&~31);
	mvSize = (mvSize*3)/2;
	mvSize = (mvSize+4) / 5;
	mvSize = ((mvSize+7)/ 8) * 8;
	mvSize = ((mvSize + 4096-1)/4096) * 4096;

	if( width==0 || height==0 || mvSize==0 )
	{
		NX_ErrMsg(("Invalid memory parameters!!!(width=%d, height=%d, mvSize=%d)\n", width, height, mvSize));
		return -1;
	}

	if( !hDec->useExternalFrameBuffer )
	{
		NX_RelMsg( 1, ( "resole : %dx%d, numFrameBuffers=%d\n", width, height, hDec->numFrameBuffers ));
		for( i=0 ; i<hDec->numFrameBuffers ; i++ )
		{
			hDec->hFrameBuffer[i] = NX_VideoAllocateMemory( 4096, width, height, NX_MEM_MAP_LINEAR, FOURCC_MVS0 );	//	Planar Lu/Cb/Cr
			if( 0 == hDec->hFrameBuffer[i] ){
				NX_ErrMsg(("NX_VideoAllocateMemory(64,%d,%d,..) failed.(i=%d)\n", width, height, i));
				goto ERROR_EXIT;
			}
		}
	}


	hDec->hColMvBuffer = NX_AllocateMemory( mvSize*hDec->numFrameBuffers, 4096 );	//	Planar Lu/Cb/Cr
	if( 0 == hDec->hColMvBuffer ){
		NX_ErrMsg(("hColMvBuffer allocation failed(size=%d, align=%d)\n", mvSize*hDec->numFrameBuffers, 4096));
		goto ERROR_EXIT;
	}

	if( hDec->codecStd == CODEC_STD_AVC )
	{
		hDec->hSliceBuffer = NX_AllocateMemory( 2048*2048*3/4, 4096 );		//	x16 aligned
		if( 0 == hDec->hSliceBuffer ){
			NX_ErrMsg(("hSliceBuffer allocation failed.(size=%d,align=%d)\n",  2048*2048*3/4, 4096));
			goto ERROR_EXIT;
		}
	}

	if( hDec->codecStd == CODEC_STD_THO || hDec->codecStd == CODEC_STD_VP3 || hDec->codecStd == CODEC_STD_VP8 )
	{
		hDec->hPvbSliceBuffer = NX_AllocateMemory( 17*4*(2048*2048/256), 4096 );		//	x16 aligned
		if( 0 == hDec->hPvbSliceBuffer ){
			NX_ErrMsg(("hPvbSliceBuffer allocation failed.(size=%d,align=%d)\n", 17*4*(2048*2048/256), 4096));
			goto ERROR_EXIT;
		}
	}

#if DBG_BUF_ALLOC
	NX_DbgMsg( DBG_BUF_ALLOC, ("Allocate Decoder Memory\n") );
	for( i=0 ; i<hDec->numFrameBuffers ; i++ )
	{
		NX_DbgMsg( DBG_BUF_ALLOC, ("    hFrameBuffer[%d]  : LuPhy(0x%08x), CbPhy(0x%08x), CrPhy(0x%08x)\n", i, hDec->hFrameBuffer[i]->luPhyAddr, hDec->hFrameBuffer[i]->cbPhyAddr, hDec->hFrameBuffer[i]->crPhyAddr) );
		NX_DbgMsg( DBG_BUF_ALLOC, ("    hFrameBuffer[%d]  : LuVir(0x%08x), CbVir(0x%08x), CrVir(0x%08x)\n", i, hDec->hFrameBuffer[i]->luVirAddr, hDec->hFrameBuffer[i]->cbVirAddr, hDec->hFrameBuffer[i]->crVirAddr) );
	}
	NX_DbgMsg( DBG_BUF_ALLOC, ("    hBitStreamBuf    : PhyAddr(0x%08x), VirAddr(0x%08x)\n", hDec->hBitStreamBuf->phyAddr, hDec->hBitStreamBuf->virAddr) );
#endif	//	DBG_BUF_ALLOC

	return 0;

ERROR_EXIT:
	FreeDecoderMemory( hDec );
	return -1;
}

static int32_t FreeDecoderMemory( NX_VID_DEC_HANDLE hDec )
{
	int32_t i;
	if( !hDec )
	{
		NX_ErrMsg(("invalid encoder handle!!!\n"));
		return -1;
	}

	if( !hDec->useExternalFrameBuffer )
	{
		//	Free Frame Buffer
		for( i=0 ; i<hDec->numFrameBuffers ; i++ )
		{
			if( hDec->hFrameBuffer[i] )
			{
				NX_FreeVideoMemory( hDec->hFrameBuffer[i] );
				hDec->hFrameBuffer[i] = 0;
			}
		}
	}

	if( hDec->hColMvBuffer )
	{
		NX_FreeMemory( hDec->hColMvBuffer );
		hDec->hColMvBuffer = 0;
	}

	if( hDec->hSliceBuffer )
	{
		NX_FreeMemory( hDec->hSliceBuffer );
		hDec->hSliceBuffer = 0;
	}

	//	Allocate Instance Memory & Stream Buffer
	if( hDec->hInstanceBuf )
	{
		NX_FreeMemory( hDec->hInstanceBuf );
		hDec->hInstanceBuf = 0;
	}

	//	Free Bitstream Buffer
	if( hDec->hBitStreamBuf )
	{
		NX_FreeMemory( hDec->hBitStreamBuf );
		hDec->hBitStreamBuf = 0;
	}

	//	Free USerdata Buffer
	if( hDec->hUserDataBuffer )
	{
		NX_FreeMemory( hDec->hUserDataBuffer );
		hDec->hUserDataBuffer = 0;
	}

	return 0;
}

//
//	End of Static Functions
//
//////////////////////////////////////////////////////////////////////////////
