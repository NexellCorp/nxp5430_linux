#include <unistd.h>


//  FFMPEG Headers
#ifdef __cplusplus
 #define __STDC_CONSTANT_MACROS
 #ifdef _STDINT_H
  #undef _STDINT_H
 #endif
 # include <stdint.h>
#endif

#ifdef __cplusplus
extern "C"{
#endif
#include "libavformat/avformat.h"
#include "libavcodec/avcodec.h"
#include "libavcodec/opt.h"
#include "libavutil/pixdesc.h"
#include "libavdevice/avdevice.h"
#ifdef __cplusplus
}
#endif



#include "nx_video_api.h"
#include "display.h"

unsigned char extraData[1024*2];
int extraDataSize = 0;
unsigned char streamBuffer[1024*1024*4];
int streamBufferSize = 0;

static void dumpdata( void *data, int len )
{
	int i=0;
	unsigned char *byte = (unsigned char *)data;
	printf("Dump Data : ");
	for( i=0 ; i<len ; i ++ )
	{
		if( i%16 == 0 )	printf("\n\t");
		printf("%.2x", byte[i] );
		if( i%4 == 3 ) printf(" ");
	}
	printf("\n");
}

int isIdrFrame( unsigned char *buf, int size )
{
	int i;
	int isIdr=0;

	for( i=0 ; i<size-4; i++ )
	{
		if( buf[i]==0 && buf[i+1]==0 && ((buf[i+2]&0x1F)==0x05) )	//	Check Nal Start Code & Nal Type
		{
			isIdr = 1;
			break;
		}
	}

	return isIdr;
}


#define	NX_MAX_NUM_SPS		3
#define	NX_MAX_SPS_SIZE		1024
#define	NX_MAX_NUM_PPS		3
#define	NX_MAX_PPS_SIZE		1024

typedef struct {
	int				version;
	int				profile_indication;
	int				compatible_profile;
	int				level_indication;
	int				nal_length_size;
	int				num_sps;
	int				sps_length[NX_MAX_NUM_SPS];
	unsigned char	sps_data  [NX_MAX_NUM_SPS][NX_MAX_SPS_SIZE];
	int				num_pps;
	int				pps_length[NX_MAX_NUM_PPS];
	unsigned char	pps_data  [NX_MAX_NUM_PPS][NX_MAX_PPS_SIZE];
} NX_AVCC_TYPE;


static int NX_ParseSpsPpsFromAVCC( unsigned char *extraData, int extraDataSize, NX_AVCC_TYPE *avcCInfo )
{
	int pos = 0;
	int i;
	int length;
	if( 1!=extraData[0] || 11>extraDataSize ){
		printf( "Error : Invalid \"avcC\" data\n" );
		return -1;
	}

	//	Parser "avcC" format data
	avcCInfo->version				= (int)extraData[pos];			pos++;
	avcCInfo->profile_indication		= (int)extraData[pos];			pos++;
	avcCInfo->compatible_profile		= (int)extraData[pos];			pos++;
	avcCInfo->level_indication		= (int)extraData[pos];			pos++;
	avcCInfo->nal_length_size		= (int)(extraData[pos]&0x03)+1;	pos++;
	//	parser SPSs
	avcCInfo->num_sps				= (int)(extraData[pos]&0x1f);	pos++;
	for( i=0 ; i<avcCInfo->num_sps ; i++){
		length = avcCInfo->sps_length[i] = (int)(extraData[pos]<<8)|extraData[pos+1];
		pos+=2;
		if( (pos+length) > extraDataSize ){
			printf("Error : extraData size too small(SPS)\n" );
			return -1;
		}
		memcpy( avcCInfo->sps_data[i], extraData+pos, length );
		pos += length;
	}

	//	parse PPSs
	avcCInfo->num_pps				= (int)extraData[pos];			pos++;
	for( i=0 ; i<avcCInfo->num_pps ; i++ ){
		length = avcCInfo->pps_length[i] = (int)(extraData[pos]<<8)|extraData[pos+1];
		pos+=2;
		if( (pos+length) > extraDataSize ){
			printf( "Error : extraData size too small(PPS)\n");
			return -1;
		}
		memcpy( avcCInfo->pps_data[i], extraData+pos, length );
		pos += length;
	}
	return 0;
}

static void NX_MakeH264StreamAVCCtoANNEXB( NX_AVCC_TYPE *avcc, unsigned char *pBuf, int *size )
{
	int i;
	int pos = 0;
	for( i=0 ; i<avcc->num_sps ; i++ )
	{
		pBuf[pos++] = 0x00;
		pBuf[pos++] = 0x00;
		pBuf[pos++] = 0x00;
		pBuf[pos++] = 0x01;
		memcpy( pBuf + pos, avcc->sps_data[i], avcc->sps_length[i] );
		pos += avcc->sps_length[i];
	}
	for( i=0 ; i<avcc->num_pps ; i++ )
	{
		pBuf[pos++] = 0x00;
		pBuf[pos++] = 0x00;
		pBuf[pos++] = 0x00;
		pBuf[pos++] = 0x01;
		memcpy( pBuf + pos, avcc->pps_data[i], avcc->pps_length[i] );
		pos += avcc->pps_length[i];
	}
	*size = pos;
}


static void play_video( AVFormatContext *fmt_ctx )
{
	int video_stream_index=0xffffffff;
	int ret, got_picture;
	AVCodec *video_codec = NULL;
	AVStream *video_stream = NULL;
	AVPacket packet, *pkt = &packet;
    AVFrame *dec_out_frame= avcodec_alloc_frame();
	AVCodecContext *avctx;
	struct timeval start, end;
	int i=0;
	int frame_count=0, out_count = 0;
	DISPLAY_HANDLE hDsp;
	int prevIdx = -1;
	NX_VID_RET vidRet;
	int vpuCodecType;
	static NX_AVCC_TYPE avcC_info;

	if( !fmt_ctx )
		return;

	//	Video Codec Binding
	for( i=0; i<(int)fmt_ctx->nb_streams ; i++ )
	{
		AVStream *stream = fmt_ctx->streams[i];
		if( stream->codec->codec_type == AVMEDIA_TYPE_VIDEO ){
			if( !(video_codec=avcodec_find_decoder(stream->codec->codec_id)) ){
				printf( "Unsupported codec (id=%d) for input stream %d\n", stream->codec->codec_id, stream->index );
			}else if( avcodec_open(stream->codec, video_codec)<0 ){
				printf( "Error while opening codec for input stream %d\n", stream->index );
			}else{
				//	find first video stream
				video_stream = stream;
				video_stream_index = i;
				break;
			}
		}
	}

	avctx = video_stream->codec;
    avctx->debug_mv = 0;
    avctx->debug = 0;
    avctx->workaround_bugs = 1;
    avctx->lowres = 0;
	avctx->flags2 |= CODEC_FLAG2_FAST;
	avctx->skip_frame = AVDISCARD_NONE;
    avctx->skip_idct= AVDISCARD_NONE;
    avctx->skip_loop_filter= AVDISCARD_NONE;

	if( video_stream->codec->codec_id == CODEC_ID_MPEG4 )
	{
		vpuCodecType = NX_MP4_DEC;
	}
	else if( video_stream->codec->codec_id == CODEC_ID_H264 )
	{
		vpuCodecType = NX_AVC_DEC;
	}
	else if( video_stream->codec->codec_id == CODEC_ID_MPEG2VIDEO )
	{
		vpuCodecType = NX_MP2_DEC;
	}
	else
	{
		printf("Cannot supprot codecid(%d)\n", video_stream->codec->codec_id);
		exit(-1);
	}

	hDsp = InitDisplay( video_stream->codec->coded_width, video_stream->codec->coded_height, 1 );
	if( hDsp == NULL )
	{
		printf("Display Failed!!!\n");
		exit(-1);
	}

	if( video_stream->codec->codec_id == CODEC_ID_H264 && avctx->extradata_size > 0 && avctx->extradata[0]==1 ){
		if( 0 == NX_ParseSpsPpsFromAVCC( avctx->extradata, avctx->extradata_size, &avcC_info ) )
		{
			NX_MakeH264StreamAVCCtoANNEXB( &avcC_info, extraData, &extraDataSize );
		}
	}

	streamBufferSize = 0;

	if( video_codec && video_stream )
	{
		NX_VID_DEC_HANDLE handle;
		NX_VID_DEC_OUT decOut;

		printf( "<<< Ray >>> Resolution : %dx%d\n"
		        "            Extrea Data Size : %d\n",
				video_stream->codec->coded_width,
				video_stream->codec->coded_height,
				video_stream->codec->extradata_size );

//		dumpdata(video_stream->codec->extradata, video_stream->codec->extradata_size);
		handle = NX_VidDecOpen(vpuCodecType);

		av_init_packet(pkt);
		frame_count = 0;
		for( ;; )
		{
			ret = av_read_frame( fmt_ctx, pkt );
			if( ret<0 )
			{
				break;
			}
			//getchar();
			if( pkt->stream_index == video_stream_index )
			{
				if(frame_count==0 )
				{
					//if( !isIdrFrame( pkt->data, pkt->size ) )
					//{
					//	printf("is not idr\n");
					//	continue;
					//}
					//memcpy( streamBuffer, video_stream->codec->extradata, video_stream->codec->extradata_size);
					//streamBufferSize = video_stream->codec->extradata_size;
					memcpy( streamBuffer + streamBufferSize, pkt->data, pkt->size );
					streamBufferSize += pkt->size;
					if( NX_VidDecInit( handle, streamBuffer, streamBufferSize, video_stream->codec->coded_width, video_stream->codec->coded_height ) < 0 )
					{
						printf("NX_VidDecInit()\n");
						exit(-1);
					}
					frame_count ++;
				}
				else
				{
					vidRet = NX_VidDecDecodeFrame( handle, pkt->data, pkt->size, &decOut );
					if( vidRet == VID_NEED_MORE_BUF )
					{
						frame_count ++;
						continue;
					}
					if( vidRet < 0 )
					{
						printf("Decoding Error!!!\n");
						exit(-2);
					}
					if( decOut.outImgIdx >= 0  )
					{
						NX_VidDecClrDspFlag( handle, &decOut.outImg, decOut.outImgIdx );
						if( out_count != 0 )
						{
							DisplayDequeueBuffer( hDsp );
						}
						DisplayQueueBuffer( hDsp, &decOut.outImg );
						out_count ++;
					}
					usleep(10000);
				}
				av_free_packet(pkt);
				frame_count ++;
			}
		}
	}
}


static void dump_video( AVFormatContext *fmt_ctx, const char *outFileName )
{
	int video_stream_index=0xffffffff;
	int ret, got_picture;
	AVCodec *video_codec = NULL;
	AVStream *video_stream = NULL;
	AVPacket packet, *pkt = &packet;
    AVFrame *dec_out_frame= avcodec_alloc_frame();
	AVCodecContext *avctx;
	struct timeval start, end;
	int i=0;
	int frame_count=0, out_count = 0;
	int prevIdx = -1;
	NX_VID_RET vidRet;
	int vpuCodecType;
	static NX_AVCC_TYPE avcC_info;
	FILE *outFd = fopen(outFileName, "wb");

	if( !fmt_ctx )
		return;

	//	Video Codec Binding
	for( i=0; i<(int)fmt_ctx->nb_streams ; i++ )
	{
		AVStream *stream = fmt_ctx->streams[i];
		if( stream->codec->codec_type == AVMEDIA_TYPE_VIDEO ){
			if( !(video_codec=avcodec_find_decoder(stream->codec->codec_id)) ){
				printf( "Unsupported codec (id=%d) for input stream %d\n", stream->codec->codec_id, stream->index );
			}else if( avcodec_open(stream->codec, video_codec)<0 ){
				printf( "Error while opening codec for input stream %d\n", stream->index );
			}else{
				//	find first video stream
				video_stream = stream;
				video_stream_index = i;
				break;
			}
		}
	}

	avctx = video_stream->codec;
    avctx->debug_mv = 0;
    avctx->debug = 0;
    avctx->workaround_bugs = 1;
    avctx->lowres = 0;
	avctx->flags2 |= CODEC_FLAG2_FAST;
	avctx->skip_frame = AVDISCARD_NONE;
    avctx->skip_idct= AVDISCARD_NONE;
    avctx->skip_loop_filter= AVDISCARD_NONE;

	if( video_stream->codec->codec_id == CODEC_ID_H264 && avctx->extradata_size > 0 && avctx->extradata[0]==1 ){
		if( 0 == NX_ParseSpsPpsFromAVCC( avctx->extradata, avctx->extradata_size, &avcC_info ) )
		{
			NX_MakeH264StreamAVCCtoANNEXB( &avcC_info, extraData, &extraDataSize );
		}
	}

	if( video_codec && video_stream )
	{
		av_init_packet(pkt);
		frame_count = 0;
		for( ;; )
		{
			ret = av_read_frame( fmt_ctx, pkt );
			if( ret<0 )
			{
				break;
			}
			//getchar();
			if( pkt->stream_index == video_stream_index )
			{
				if(frame_count==0 )
				{
					if( outFd && video_stream->codec->extradata_size > 0 )
					{
						fwrite( video_stream->codec->extradata, 1, video_stream->codec->extradata_size, outFd );
					}
				}
				if( outFd )
				{
					fwrite( pkt->data, 1, pkt->size, outFd );
				}
				frame_count ++;
			}
			av_free_packet(pkt);
		}
	}
	if( outFd )
	{
		fclose( outFd );
	}
}

static AVFormatContext *open_file(const char *filename)
{
	AVFormatContext *fmt_ctx;
	AVInputFormat *iformat = NULL;

	fmt_ctx = avformat_alloc_context();

	if ( avformat_open_input(&fmt_ctx, filename, iformat, NULL) < 0) {
		return NULL;
	}

	/* fill the streams in the format context */
	if ( av_find_stream_info(fmt_ctx) < 0) {
		av_close_input_file( fmt_ctx );
		return NULL;
	}
	av_dump_format(fmt_ctx, 0, filename, 0);
	return fmt_ctx;
}

static void close_file( AVFormatContext *fmt_ctx )
{
	if( fmt_ctx ){
		av_close_input_file( fmt_ctx );
	}
}


int play_simple_file( const char *fileName )
{
	FILE *fd = fopen( fileName, "rb" );
	unsigned char * readBuffer = malloc(1024*1024);
	size_t rSize;
	NX_VID_DEC_HANDLE handle = NX_VidDecOpen(NX_MP2_DEC);
	int frameCnt=0;
	int ret, needStream=1;

	if( fd )
	{
		while( 1 )
		{
			if( needStream == 1 )
			{
				rSize = fread( readBuffer, 1, 1024*1024, fd );
				needStream = 0;
			}
			if( frameCnt == 0 )
			{
//				getchar();
				ret = NX_VidDecInit( handle, readBuffer, rSize, 0 , 0 );
				//getchar();
				if( ret < 0 )
				{
					break;
				}
			}
		}
	}
	return 0;
}



int main( int argc, char *argv[] )
{
	int dumpMode=0;
	AVFormatContext *fmtCtx;

	av_register_all();
	if( argc<2 )
	{
		printf("\nUsage : vpu_test [file_name]\n\n");
		return -1;
	}
	if( argc>2 )
	{
		printf("Dump Mode\n");
		dumpMode = 1;
	}


	//return play_simple_file( argv[1] );


	fmtCtx = open_file( argv[1] );
	if( fmtCtx )
	{
		if( !dumpMode )
		{
			play_video( fmtCtx );
		}
		else
		{
			dump_video( fmtCtx, argv[2] );
		}
		close_file( fmtCtx );
	}

	return 0;
}
