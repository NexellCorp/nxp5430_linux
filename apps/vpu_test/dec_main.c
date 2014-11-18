#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>		//	getopt & optarg
#include <sys/time.h>	//	gettimeofday


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
#include <libavformat/avformat.h>
#include <libavcodec/avcodec.h>
#include <libavcodec/opt.h>
#include <libavutil/pixdesc.h>
#include <libavdevice/avdevice.h>
#ifdef __cplusplus
}
#endif

#define	ENABLE_THEORA		1


#if	(ENABLE_THEORA)
#include "theoraparser/include/theora_parser.h"
//Theora specific display information
typedef struct {
	int frameWidth;
	int frameHeight;
	int picWidth;
	int picHeight;
	int picOffsetX;
	int picOffsetY;
} ThoScaleInfo;
#endif

#include <nx_dsp.h>
#include "nx_video_api.h"
#include "codec_info.h"


//#define DUMP_FILE_FORMAT
#define	ENABLE_DISPLAY
#ifdef	ENABLE_DISPLAY
#define	ENABLE_MLC_SCALER
#endif

//	Drone
#define	LCD_WIDTH	1024
#define	LCD_HEIGHT	600
//	Pyxis
// #define	LCD_WIDTH	800
// #define	LCD_HEIGHT	1280
//	Lynx
//#define	LCD_WIDTH	1280
//#define	LCD_HEIGHT	800

#define	RCV_V2

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


typedef struct tFFMPEG_STREAM_READER {
	AVFormatContext		*fmt_ctx;

	//	Video Stream Information
	int					video_stream_idx;
	AVStream			*video_stream;
	AVCodec				*video_codec;

	NX_AVCC_TYPE		h264AvcCHeader;		//	for h.264
	tho_parser_t*		theoraParser;		//	for theora

	//	Audio Stream Information
	int					audio_stream_idx;
	AVStream			*audio_stream;
	AVCodec				*audio_codec;
} FFMPEG_STREAM_READER;


unsigned char streamBuffer[8*1024*1024];
unsigned char seqData[1024*4];


//==============================================================================
static int NX_ParseSpsPpsFromAVCC( unsigned char *extraData, int extraDataSize, NX_AVCC_TYPE *avcCInfo );
static void NX_MakeH264StreamAVCCtoANNEXB( NX_AVCC_TYPE *avcc, unsigned char *pBuf, int *size );
static void dumpdata( void *data, int len, const char *msg );



#ifndef MKTAG
#define MKTAG(a,b,c,d) (a | (b << 8) | (c << 16) | (d << 24))
#endif

#define PUT_LE32(_p, _var) \
	*_p++ = (unsigned char)((_var)>>0);  \
	*_p++ = (unsigned char)((_var)>>8);  \
	*_p++ = (unsigned char)((_var)>>16); \
	*_p++ = (unsigned char)((_var)>>24);

#define PUT_BE32(_p, _var) \
	*_p++ = (unsigned char)((_var)>>24);  \
	*_p++ = (unsigned char)((_var)>>16);  \
	*_p++ = (unsigned char)((_var)>>8); \
	*_p++ = (unsigned char)((_var)>>0);

#define PUT_LE16(_p, _var) \
	*_p++ = (unsigned char)((_var)>>0);  \
	*_p++ = (unsigned char)((_var)>>8);


#define PUT_BE16(_p, _var) \
	*_p++ = (unsigned char)((_var)>>8);  \
	*_p++ = (unsigned char)((_var)>>0);


static uint64_t NX_GetTickCount( void )
{
	uint64_t ret;
	struct timeval	tv;
	struct timezone	zv;
	gettimeofday( &tv, &zv );
	ret = ((uint64_t)tv.tv_sec)*1000000 + tv.tv_usec;
	return ret;
}

FFMPEG_STREAM_READER *OpenMediaFile( const char *fileName )
{
	FFMPEG_STREAM_READER *streamReader = NULL;

	AVFormatContext *fmt_ctx = NULL;
	AVInputFormat *iformat = NULL;

	AVCodec *video_codec = NULL;
	AVCodec *audio_codec = NULL;
	AVStream *video_stream = NULL;
	AVStream *audio_stream = NULL;
	int video_stream_idx = -1;
	int audio_stream_idx = -1;
	int i;

	fmt_ctx = avformat_alloc_context();

	fmt_ctx->flags |= CODEC_FLAG_TRUNCATED;

	if ( avformat_open_input(&fmt_ctx, fileName, iformat, NULL) < 0 )
	{
		printf("avformat_open_input() Error \n");
		return NULL;
	}

	/* fill the streams in the format context */
	if ( av_find_stream_info(fmt_ctx) < 0)
	{
		printf("av_find_stream_info() Error \n");
		av_close_input_file( fmt_ctx );
		return NULL;
	}

#ifdef	DUMP_FILE_FORMAT
	av_dump_format(fmt_ctx, 0, fileName, 0);
#endif

	//	Video Codec Binding
	for( i=0; i<(int)fmt_ctx->nb_streams ; i++ )
	{
		AVStream *stream = fmt_ctx->streams[i];

		if( stream->codec->codec_type == AVMEDIA_TYPE_VIDEO )
		{
			if( !(video_codec=avcodec_find_decoder(stream->codec->codec_id)) )
			{
				printf( "Unsupported codec (id=%d) for input stream %d\n", stream->codec->codec_id, stream->index );
				goto ErrorExit;
			}

			if( avcodec_open(stream->codec, video_codec)<0 )
			{
				printf( "Error while opening codec for input stream %d\n", stream->index );
				goto ErrorExit;
			}
			else
			{
				if( video_stream_idx == -1 )
				{
					video_stream_idx = i;
					video_stream = stream;;
				}
				else
				{
					avcodec_close( stream->codec );
				}
			}
		}
		else if( stream->codec->codec_type == AVMEDIA_TYPE_AUDIO )
		{
			if( !(audio_codec=avcodec_find_decoder(stream->codec->codec_id)) )
			{
				printf( "Unsupported codec (id=%d) for input stream %d\n", stream->codec->codec_id, stream->index );
				goto ErrorExit;
			}

			if( avcodec_open(stream->codec, audio_codec)<0 )
			{
				printf( "Error while opening codec for input stream %d\n", stream->index );
				goto ErrorExit;
			}
			else
			{
				if( audio_stream_idx == -1 )
				{
					audio_stream_idx = i;
					audio_stream = stream;;
				}
				else
				{
					avcodec_close( stream->codec );
				}
			}
		}
	}

	streamReader = (FFMPEG_STREAM_READER *)malloc(sizeof(FFMPEG_STREAM_READER));
	memset( streamReader, 0, sizeof(FFMPEG_STREAM_READER) );
	streamReader->fmt_ctx = fmt_ctx;

	streamReader->video_stream_idx = video_stream_idx;
	streamReader->video_stream     = video_stream;
	streamReader->video_codec      = video_codec;

	streamReader->audio_stream_idx = audio_stream_idx;
	streamReader->audio_stream     = audio_stream;
	streamReader->audio_codec      = audio_codec;

	return streamReader;

ErrorExit:
	if( streamReader )
	{
		free( streamReader );
	}
	if( fmt_ctx )
	{
		av_close_input_file( fmt_ctx );
	}
	return NULL;

}

 void CloseFile( FFMPEG_STREAM_READER *pStreamReader )
{
	if( pStreamReader )
	{
		av_close_input_file( (AVFormatContext*)((void*)pStreamReader->fmt_ctx) );
		free( pStreamReader );
	}
}


int GetSequenceInformation( FFMPEG_STREAM_READER *streamReader, AVStream *stream, unsigned char *buffer, int size )
{
	unsigned char *pbHeader = buffer;
	enum CodecID codecId = stream->codec->codec_id;
	int fourcc;
	int frameRate = 0;
	int nMetaData = stream->codec->extradata_size;
	unsigned char *pbMetaData = stream->codec->extradata;
	int retSize = 0;
	unsigned int tag = stream->codec->codec_tag;

	if (stream->avg_frame_rate.den && stream->avg_frame_rate.num)
		frameRate = (int)((double)stream->avg_frame_rate.num/(double)stream->avg_frame_rate.den);
	if (!frameRate && stream->r_frame_rate.den && stream->r_frame_rate.num)
		frameRate = (int)((double)stream->r_frame_rate.num/(double)stream->r_frame_rate.den);

	if( (codecId == CODEC_ID_H264) && (stream->codec->extradata_size>0) )
	{
		if( stream->codec->extradata[0] == 0x1 )
		{
			NX_ParseSpsPpsFromAVCC( pbMetaData, nMetaData, &streamReader->h264AvcCHeader );
			NX_MakeH264StreamAVCCtoANNEXB(&streamReader->h264AvcCHeader, buffer, &retSize );
			return retSize;
		}
	}
    else if ( (codecId == CODEC_ID_VC1) )
    {
		retSize = nMetaData;
        memcpy(pbHeader, pbMetaData, retSize);
		//if there is no seq startcode in pbMetatData. VPU will be failed at seq_init stage.
		return retSize;
	}
    else if ( (codecId == CODEC_ID_MSMPEG4V3) )
	{
		switch( tag )
		{
			case MKTAG('D','I','V','3'):
			case MKTAG('M','P','4','3'):
			case MKTAG('M','P','G','3'):
			case MKTAG('D','V','X','3'):
			case MKTAG('A','P','4','1'):
				if( !nMetaData )
				{
					PUT_LE32(pbHeader, MKTAG('C', 'N', 'M', 'V'));	//signature 'CNMV'
					PUT_LE16(pbHeader, 0x00);						//version
					PUT_LE16(pbHeader, 0x20);						//length of header in bytes
					PUT_LE32(pbHeader, MKTAG('D', 'I', 'V', '3'));	//codec FourCC
					PUT_LE16(pbHeader, stream->codec->width);		//width
					PUT_LE16(pbHeader, stream->codec->height);		//height
					PUT_LE32(pbHeader, stream->r_frame_rate.num);	//frame rate
					PUT_LE32(pbHeader, stream->r_frame_rate.den);	//time scale(?)
					PUT_LE32(pbHeader, stream->nb_index_entries);	//number of frames in file
					PUT_LE32(pbHeader, 0);							//unused
					retSize += 32;
				}
				else
				{
					PUT_BE32(pbHeader, nMetaData);
					retSize += 4;
					memcpy(pbHeader, pbMetaData, nMetaData);
					retSize += nMetaData;
				}
				return retSize;
			default:
				break;

		}
	}
	else if(  (codecId == CODEC_ID_WMV1) || (codecId == CODEC_ID_WMV2) || (codecId == CODEC_ID_WMV3) )
	{
#ifdef	RCV_V2	//	RCV_V2
        PUT_LE32(pbHeader, ((0xC5 << 24)|0));
        retSize += 4; //version
        PUT_LE32(pbHeader, nMetaData);
        retSize += 4;

        memcpy(pbHeader, pbMetaData, nMetaData);
		pbHeader += nMetaData;
        retSize += nMetaData;

        PUT_LE32(pbHeader, stream->codec->height);
        retSize += 4;
        PUT_LE32(pbHeader, stream->codec->width);
        retSize += 4;
        PUT_LE32(pbHeader, 12);
        retSize += 4;
        PUT_LE32(pbHeader, 2 << 29 | 1 << 28 | 0x80 << 24 | 1 << 0);
        retSize += 4; // STRUCT_B_FRIST (LEVEL:3|CBR:1:RESERVE:4:HRD_BUFFER|24)
        PUT_LE32(pbHeader, stream->codec->bit_rate);
        retSize += 4; // hrd_rate
		PUT_LE32(pbHeader, frameRate);
        retSize += 4; // frameRate
#else	//RCV_V1
        PUT_LE32(pbHeader, (0x85 << 24) | 0x00);
        retSize += 4; //frames count will be here
        PUT_LE32(pbHeader, nMetaData);
        retSize += 4;
        memcpy(pbHeader, pbMetaData, nMetaData);
		pbHeader += nMetaData;
        retSize += nMetaData;
        PUT_LE32(pbHeader, stream->codec->height);
        retSize += 4;
        PUT_LE32(pbHeader, stream->codec->width);
        retSize += 4;
#endif
		return retSize;
	}
	else if( (stream->codec->codec_id == CODEC_ID_RV40 || stream->codec->codec_id == CODEC_ID_RV30 )
		&& (stream->codec->extradata_size>0) )
	{
		if( CODEC_ID_RV40 == stream->codec->codec_id )
		{
			fourcc = MKTAG('R','V','4','0');
		}
		else
		{
			fourcc = MKTAG('R','V','3','0');
		}
        retSize = 26 + nMetaData;
        PUT_BE32(pbHeader, retSize); //Length
        PUT_LE32(pbHeader, MKTAG('V', 'I', 'D', 'O')); //MOFTag
		PUT_LE32(pbHeader, fourcc); //SubMOFTagl
        PUT_BE16(pbHeader, stream->codec->width);
        PUT_BE16(pbHeader, stream->codec->height);
        PUT_BE16(pbHeader, 0x0c); //BitCount;
        PUT_BE16(pbHeader, 0x00); //PadWidth;
        PUT_BE16(pbHeader, 0x00); //PadHeight;
        PUT_LE32(pbHeader, frameRate);
        memcpy(pbHeader, pbMetaData, nMetaData);
		return retSize;
	}
	else if( stream->codec->codec_id == CODEC_ID_VP8 )
	{
		PUT_LE32(pbHeader, MKTAG('D', 'K', 'I', 'F'));	//signature 'DKIF'
		PUT_LE16(pbHeader, 0x00);						//version
		PUT_LE16(pbHeader, 0x20);						//length of header in bytes
		PUT_LE32(pbHeader, MKTAG('V', 'P', '8', '0'));	//codec FourCC
		PUT_LE16(pbHeader, stream->codec->width);		//width
		PUT_LE16(pbHeader, stream->codec->height);		//height
		PUT_LE32(pbHeader, stream->r_frame_rate.num);	//frame rate
		PUT_LE32(pbHeader, stream->r_frame_rate.den);	//time scale(?)
		PUT_LE32(pbHeader, stream->nb_index_entries);	//number of frames in file
		PUT_LE32(pbHeader, 0);							//unused
		retSize += 32;
		return retSize;
	}
#if	(ENABLE_THEORA)
	else if( stream->codec->codec_id == CODEC_ID_THEORA )
	{
		ThoScaleInfo thoScaleInfo;
		tho_parser_t *thoParser = streamReader->theoraParser;
		thoParser->open(thoParser->handle, stream->codec->extradata, stream->codec->extradata_size, (int *)&thoScaleInfo);
		retSize = theora_make_stream((void *)streamReader->theoraParser->handle, buffer, 1);
		return retSize;
	}
#endif

	memcpy( buffer, stream->codec->extradata, stream->codec->extradata_size );

	return stream->codec->extradata_size;
}




static int PasreAVCStream( AVPacket *pkt, int nalLengthSize, unsigned char *buffer, int outBufSize )
{
	int nalLength;

	//	input
	unsigned char *inBuf = pkt->data;
	int inSize = pkt->size;
	int pos=0;

	//	'avcC' format
	do{
		nalLength = 0;

		if( nalLengthSize == 2 )
		{
			nalLength = inBuf[0]<< 8 | inBuf[1];
		}
		else if( nalLengthSize == 3 )
		{
			nalLength = inBuf[0]<<16 | inBuf[1]<<8  | inBuf[2];
		}
		else if( nalLengthSize == 4 )
		{
			nalLength = inBuf[0]<<24 | inBuf[1]<<16 | inBuf[2]<<8 | inBuf[3];
		}
		else if( nalLengthSize == 1 )
		{
			nalLength = inBuf[0];
		}

		inBuf  += nalLengthSize;
		inSize -= nalLengthSize;

		if( 0==nalLength || inSize<(int)nalLength )
		{
			printf("Error : avcC type nal length error (nalLength = %d, inSize=%d, nalLengthSize=%d)\n", nalLength, inSize, nalLengthSize);
			return -1;
		}

		//	put nal start code
		buffer[pos + 0] = 0x00;
		buffer[pos + 1] = 0x00;
		buffer[pos + 2] = 0x00;
		buffer[pos + 3] = 0x01;
		pos += 4;

		memcpy( buffer + pos, inBuf, nalLength );
		pos += nalLength;

		inSize -= nalLength;
		inBuf += nalLength;
	}while( 2<inSize );
	return pos;
}

static int MakeRvStream( AVPacket *pkt, AVStream *stream, unsigned char *buffer, int outBufSize )
{
	unsigned char *p = pkt->data;
    int cSlice, nSlice;
    int i, val, offset;
//	int has_st_code = 0;
	int size;

	cSlice = p[0] + 1;
	nSlice =  pkt->size - 1 - (cSlice * 8);
	size = 20 + (cSlice*8);

	PUT_BE32(buffer, nSlice);
	if (AV_NOPTS_VALUE == (unsigned long long)pkt->pts)
	{
		PUT_LE32(buffer, 0);
	}
	else
	{
		PUT_LE32(buffer, (int)((double)(pkt->pts/stream->time_base.den))); // milli_sec
	}
	PUT_BE16(buffer, stream->codec->frame_number);
	PUT_BE16(buffer, 0x02); //Flags
	PUT_BE32(buffer, 0x00); //LastPacket
	PUT_BE32(buffer, cSlice); //NumSegments
	offset = 1;
	for (i = 0; i < (int) cSlice; i++)
	{
		val = (p[offset+3] << 24) | (p[offset+2] << 16) | (p[offset+1] << 8) | p[offset];
		PUT_BE32(buffer, val); //isValid
		offset += 4;
		val = (p[offset+3] << 24) | (p[offset+2] << 16) | (p[offset+1] << 8) | p[offset];
		PUT_BE32(buffer, val); //Offset
		offset += 4;
	}

	memcpy(buffer, pkt->data+(1+(cSlice*8)), nSlice);
	size += nSlice;

	//printf("size = %6d, nSlice = %6d, cSlice = %4d, pkt->size=%6d, frameNumber=%d\n", size, nSlice, cSlice, pkt->size, stream->codec->frame_number );
	return size;
}

static int MakeVC1Stream( AVPacket *pkt, AVStream *stream, unsigned char *buffer, int outBufSize )
{
	int size=0;
	unsigned char *p = pkt->data;

	if( stream->codec->codec_id == CODEC_ID_VC1 )
	{
		if (p[0] != 0 || p[1] != 0 || p[2] != 1) // check start code as prefix (0x00, 0x00, 0x01)
		{
			*buffer++ = 0x00;
			*buffer++ = 0x00;
			*buffer++ = 0x01;
			*buffer++ = 0x0D;
			size = 4;
			memcpy(buffer, pkt->data, pkt->size);
			size += pkt->size;
		}
		else
		{
			memcpy(buffer, pkt->data, pkt->size);
			size = pkt->size; // no extra header size, there is start code in input stream.
		}
	}
	else
	{
		PUT_LE32(buffer, pkt->size | ((pkt->flags & AV_PKT_FLAG_KEY) ? 0x80000000 : 0));
		size += 4;
#ifdef RCV_V2	//	RCV_V2
		if (AV_NOPTS_VALUE == (unsigned long long)pkt->pts)
		{
			PUT_LE32(buffer, 0);
		}
		else
		{
			PUT_LE32(buffer, (int)((double)(pkt->pts/stream->time_base.den))); // milli_sec
		}
		size += 4;
#endif
		memcpy(buffer, pkt->data, pkt->size);
		size += pkt->size;
	}
	return size;
}


static int MakeDIVX3Stream( AVPacket *pkt, AVStream *stream, unsigned char *buffer, int outBufSize )
{
	int size = pkt->size;
	unsigned int tag = stream->codec->codec_tag;
	if( tag == MKTAG('D', 'I', 'V', '3') || tag == MKTAG('M', 'P', '4', '3') ||
		tag == MKTAG('M', 'P', 'G', '3') || tag == MKTAG('D', 'V', 'X', '3') || tag == MKTAG('A', 'P', '4', '1') )
	{
 		PUT_LE32(buffer,pkt->size);
 		PUT_LE32(buffer,0);
 		PUT_LE32(buffer,0);
 		size += 12;
	}
	memcpy( buffer, pkt->data, pkt->size );
	return size;
}

static int MakeVP8Stream( AVPacket *pkt, AVStream *stream, unsigned char *buffer, int outBufSize )
{
	PUT_LE32(buffer,pkt->size);		//	frame_chunk_len
	PUT_LE32(buffer,0);				//	time stamp
	PUT_LE32(buffer,0);
	memcpy( buffer, pkt->data, pkt->size );
	return ( pkt->size + 12 );
}

int ReadStream( FFMPEG_STREAM_READER *streamReader, AVStream *stream, unsigned char *buffer, int *size, int *isKey, long long *timeStamp )
{
	int ret;
	AVPacket pkt;
	enum CodecID codecId = stream->codec->codec_id;
	double timeStampRatio = (double)stream->time_base.num*1000./(double)stream->time_base.den;
	*size = 0;
	do{
		ret = av_read_frame( streamReader->fmt_ctx, &pkt );
		if( ret < 0 )
			return -1;
		if( pkt.stream_index == stream->index )
		{
			//	check codec type
			if( codecId == CODEC_ID_H264 && stream->codec->extradata_size > 0 && stream->codec->extradata[0]==1 )
			{
				*size = PasreAVCStream( &pkt, streamReader->h264AvcCHeader.nal_length_size, buffer, 0 );
				*isKey = (pkt.flags & AV_PKT_FLAG_KEY)?1:0;
				if( (unsigned long long)pkt.pts != AV_NOPTS_VALUE )
					*timeStamp = pkt.pts*timeStampRatio;
				else
					*timeStamp = -1;
				av_free_packet( &pkt );
				return 0;
			}
			else if(  (codecId == CODEC_ID_VC1) || (codecId == CODEC_ID_WMV1) || (codecId == CODEC_ID_WMV2) || (codecId == CODEC_ID_WMV3) )
			{
				*size = MakeVC1Stream( &pkt, stream, buffer, 0 );
				*isKey = (pkt.flags & AV_PKT_FLAG_KEY)?1:0;
				if( (unsigned long long)pkt.pts != AV_NOPTS_VALUE )
					*timeStamp = pkt.pts*timeStampRatio;
				else
					*timeStamp = -1;
				av_free_packet( &pkt );
				return 0;
			}
			else if( codecId == CODEC_ID_RV30 || codecId == CODEC_ID_RV40 )
			{
				*size = MakeRvStream( &pkt, stream, buffer, 0 );
				*isKey = (pkt.flags & AV_PKT_FLAG_KEY)?1:0;
				if( (unsigned long long)pkt.pts != AV_NOPTS_VALUE )
					*timeStamp = pkt.pts*timeStampRatio;
				else
					*timeStamp = -1;
				av_free_packet( &pkt );
				return 0;
			}
			else if( codecId == CODEC_ID_MSMPEG4V3 )
			{
				*size = MakeDIVX3Stream( &pkt, stream, buffer, 0 );
				*isKey = (pkt.flags & AV_PKT_FLAG_KEY)?1:0;
				if( (unsigned long long)pkt.pts != AV_NOPTS_VALUE )
					*timeStamp = pkt.pts*timeStampRatio;
				else
					*timeStamp = -1;
				av_free_packet( &pkt );
				return 0;
			}
#if	(ENABLE_THEORA)
			else if( codecId == CODEC_ID_THEORA )
			{
				tho_parser_t *thoParser = streamReader->theoraParser;
				if( thoParser->read_frame( thoParser->handle, pkt.data, pkt.size ) < 0 )
				{
					printf("Theora Read Frame Failed!!!\n");
					exit(1);
				}
				*size = theora_make_stream((void *)thoParser->handle, buffer, 3 /*Theora Picture Run*/);
				*isKey = (pkt.flags & AV_PKT_FLAG_KEY)?1:0;
				if( (unsigned long long)pkt.pts != AV_NOPTS_VALUE )
					*timeStamp = pkt.pts*timeStampRatio;
				else
					*timeStamp = -1;
				av_free_packet( &pkt );
				return 0;
			}
#endif
			else if( codecId == CODEC_ID_VP8 )
			{
				*size = MakeVP8Stream( &pkt, stream, buffer, 0 );
				*isKey = (pkt.flags & AV_PKT_FLAG_KEY)?1:0;
				if( (unsigned long long)pkt.pts != AV_NOPTS_VALUE )
					*timeStamp = pkt.pts*timeStampRatio;
				else
					*timeStamp = -1;
				av_free_packet( &pkt );
				return 0;
			}
			else
			{
				memcpy(buffer, pkt.data, pkt.size );
				*size = pkt.size;
				*isKey = (pkt.flags & AV_PKT_FLAG_KEY)?1:0;
				if( (unsigned long long)pkt.pts != AV_NOPTS_VALUE )
					*timeStamp = pkt.pts*timeStampRatio;
				else
					*timeStamp = -1;
				av_free_packet( &pkt );
				return 0;
			}
		}
		av_free_packet( &pkt );
	}while(1);
	return -1;
}


static void dumpdata( void *data, int len, const char *msg )
{
	int i=0;
	unsigned char *byte = (unsigned char *)data;
	printf("Dump Data : %s", msg);
	for( i=0 ; i<len ; i ++ )
	{
		if( i%32 == 0 )	printf("\n\t");
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
	avcCInfo->profile_indication	= (int)extraData[pos];			pos++;
	avcCInfo->compatible_profile	= (int)extraData[pos];			pos++;
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



static void dump_video( FFMPEG_STREAM_READER *pReader, const char *outFileName )
{
	int readSize;
	//unsigned char extraData[2048];
	//int extraDataSize;
	int frame_count = 0;
	//int video_stream_index = pReader->video_stream_idx;
	AVStream *video_stream = pReader->video_stream;
	FILE *outFd = fopen(outFileName, "wb");
	long long timeStamp;
	int isKey=0;

	frame_count = 0;
	for( ;; )
	{
		if(frame_count==0 )
		{
			readSize = GetSequenceInformation( pReader, video_stream, streamBuffer, sizeof(streamBuffer) );
			printf("Get Seqdata = %d\n", readSize );
			if( outFd && readSize >0 )
			{
				fwrite( streamBuffer, 1, readSize, outFd );
			}
		}

		if( ReadStream( pReader, video_stream, streamBuffer, &readSize, &isKey, &timeStamp ) != 0 )
		{
			break;
		}
		if( outFd )
		{
			fwrite( streamBuffer, 1, readSize, outFd );
		}
		frame_count ++;
	}
	if( outFd )
	{
		fclose( outFd );
	}
}


static int CodecIdToVpuType( int codecId, unsigned int fourcc )
{
	int vpuCodecType =-1;
	//printf("codecId = %d, fourcc=%c%c%c%c\n", codecId, fourcc, fourcc>>8, fourcc>>16, fourcc>>24);
	if( codecId == CODEC_ID_MPEG4 || codecId == CODEC_ID_FLV1 )
	{
		vpuCodecType = NX_MP4_DEC;
	}
	else if( codecId == CODEC_ID_MSMPEG4V3 )
	{
		switch( fourcc )
		{
			case MKTAG('D','I','V','3'):
			case MKTAG('M','P','4','3'):
			case MKTAG('M','P','G','3'):
			case MKTAG('D','V','X','3'):
			case MKTAG('A','P','4','1'):
				vpuCodecType = NX_DIV3_DEC;
				break;
			default:
				vpuCodecType = NX_MP4_DEC;
				break;
		}
	}
	else if( codecId == CODEC_ID_H263 || codecId == CODEC_ID_H263P || codecId == CODEC_ID_H263I )
	{
		vpuCodecType = NX_H263_DEC;
	}
	else if( codecId == CODEC_ID_H264 )
	{
		vpuCodecType = NX_AVC_DEC;
	}
	else if( codecId == CODEC_ID_MPEG2VIDEO )
	{
		vpuCodecType = NX_MP2_DEC;
	}
	else if( (codecId == CODEC_ID_WMV3) || (codecId == CODEC_ID_VC1) )
	{
		vpuCodecType = NX_VC1_DEC;
	}
	else if( (codecId == CODEC_ID_RV30) || (codecId == CODEC_ID_RV40) )
	{
		vpuCodecType = NX_RV_DEC;
	}
#if (ENABLE_THEORA)
	else if( codecId == CODEC_ID_THEORA )
	{
		vpuCodecType = NX_THEORA_DEC;
	}
#endif
	else if( codecId == CODEC_ID_VP8 )
	{
		vpuCodecType = NX_VP8_DEC;
	}
	else
	{
		printf("Cannot support codecid(%d)\n", codecId);
		exit(-1);
	}
	return vpuCodecType;
}


long long GetCurrentTime( void )
{
	long long msec;
	struct timeval tv;
	gettimeofday( &tv, NULL );

	msec = (long long)(tv.tv_sec*1000) + (long long)tv.tv_usec/1000;
	return msec;
}

void print_usage(void)
{
	printf("\nUsage : dec_test [options]\n");
	printf("    -h                           : help \n");
	printf("    -f [input file name]         : Input file name\n");
	printf("    -p [x] [y] [width] [height]  : Display image position\n");
	printf("    -d [dump file name]          : Dump file name\n\n");
}


int dec_main( int argc, char *argv[] )
{
	int opt;
	int dumpMode=0;
	FFMPEG_STREAM_READER *pReader;
#ifdef ENABLE_DISPLAY
	DISPLAY_HANDLE hDsp;
	DISPLAY_INFO dspInfo;
#endif
	int vpuCodecType;
	VID_ERROR_E vidRet;
	NX_VID_SEQ_IN seqIn;
	NX_VID_SEQ_OUT seqOut;
	NX_VID_DEC_HANDLE hDec;
	NX_VID_DEC_IN decIn;
	NX_VID_DEC_OUT decOut;
	int seqSize = 0;
	int bInit=0, pos=0;
	int readSize, frameCount = 0, outCount=0;
	int prevIdx = -1;
	int isKey = 0;
	long long timeStamp = -1, outTimeStamp = -1;
	int needKey = 1;
	int dspWidth=0, dspHeight=0, dspX=0, dspY=0;
	double xRatio, yRatio;
	int mp4Class=0;
	int seqNeedMoreBuffer = 0;
	char *fileName = NULL;
	char *dumpFileName;
	char *outFileName = NULL;
	int tmpSize;
	int instanceIdx;
	uint64_t startTime, endTime, totalTime = 0;
	FILE *fpOut = NULL;

	av_register_all();

	while( -1 != (opt=getopt(argc, argv, "hf:p:d:o:")))
	{
		switch( opt ){
			case 'h':
				print_usage();
				return 0;
			case 'f':
				fileName = strdup( optarg );
				break;
			case 'p':
				sscanf( optarg, "%d,%d,%d,%d", &dspX, &dspY, &dspWidth, &dspHeight );
				printf("dspX = %d, dspY=%d, dspWidth=%d, dspHeight=%d\n", dspX, dspY, dspWidth, dspHeight );
				printf("optarg = %s\n", optarg);
				break;
			case 'd':
				dumpMode = 1;
				dumpFileName = strdup(optarg);
				break;
			case 'o':
				outFileName = strdup(optarg);
				break;
			default:
				break;
		}
	}

	if( fileName == NULL )
	{
		printf("Need input file!!!\n");
		print_usage();
		return -1;
	}
	if( dumpMode && dumpFileName==NULL )
	{
		printf("Need dump file\n");
		print_usage();
		return -1;
	}

	pReader = OpenMediaFile( fileName );
	if( !pReader )
	{
		printf("Cannot open file!!!\n");
		return -1;
	}

	if( pReader->video_stream_idx == -1 )
	{
		printf("Cannot found video stream!!!\n");
		return -1;
	}

	if( dumpMode )
	{
		dump_video( pReader, dumpFileName );
		return 0;
	}

	fpOut = fopen( outFileName, "wb" );

	vpuCodecType = CodecIdToVpuType( pReader->video_stream->codec->codec_id, pReader->video_stream->codec->codec_tag );
	if( vpuCodecType < 0 )
	{
		return -1;
	}

	mp4Class = fourCCToMp4Class( pReader->video_stream->codec->codec_tag );
	if( mp4Class == -1 )
		mp4Class = codecIdToMp4Class( pReader->video_stream->codec->codec_id );

	printf("vpuCodecType = %d, mp4Class = %d\n", vpuCodecType, mp4Class );

	hDec = NX_VidDecOpen(vpuCodecType, mp4Class, 0, &instanceIdx);

	if( hDec == NULL )
	{
		printf("NX_VidDecOpen(%d) failed!!!\n", vpuCodecType);
		return -1;
	}

#if (ENABLE_THEORA)
	//	Intialize Theora Parser
	if( vpuCodecType == NX_THEORA_DEC )
	{
		theora_parser_init((void**)&pReader->theoraParser);
	}
#endif
	seqSize = GetSequenceInformation( pReader, pReader->video_stream, seqData, sizeof(seqData) );


	if( seqSize == 0 )
	{
		printf("Have no SeqData!!!(vpuCodecType=%d)\n", vpuCodecType);
		//return -1;
	}

#ifdef ENABLE_DISPLAY
	if( dspX==0 && dspY==0 && dspWidth==0 && dspHeight==0 )
	{
		dspX = 0;
		dspY = 0;
		dspWidth  = pReader->video_stream->codec->coded_width;
		dspHeight = pReader->video_stream->codec->coded_height;
	}

#ifdef ENABLE_MLC_SCALER
	xRatio = (double)LCD_WIDTH / (double)pReader->video_stream->codec->coded_width;
	yRatio = (double)LCD_HEIGHT / (double)pReader->video_stream->codec->coded_height;

	if( xRatio > yRatio )		//	Heigth
	{
		dspWidth = pReader->video_stream->codec->coded_width * yRatio;
		dspHeight = LCD_HEIGHT;
		dspX = abs(LCD_WIDTH - dspWidth)/2;
	}
	else
	{
		dspWidth = LCD_WIDTH;
		dspHeight = pReader->video_stream->codec->coded_height * xRatio;
		dspY = abs(LCD_HEIGHT - dspHeight)/2;
	}
#endif	//	ENABLE_MLC_SCALER

	dspInfo.port = 0;
	dspInfo.module = 0;
	dspInfo.width = pReader->video_stream->codec->coded_width;
	dspInfo.height = pReader->video_stream->codec->coded_height;
	dspInfo.numPlane = 1;
	dspInfo.dspSrcRect.left = 0;
	dspInfo.dspSrcRect.top = 0;
	dspInfo.dspSrcRect.right = dspInfo.width;
	dspInfo.dspSrcRect.bottom = dspInfo.height;
	dspInfo.dspDstRect.left = dspX;
	dspInfo.dspDstRect.top = dspY;
	dspInfo.dspDstRect.right = dspWidth+dspX;
	dspInfo.dspDstRect.bottom = dspHeight+dspY;
	hDsp = NX_DspInit( &dspInfo );
	NX_DspVideoSetPriority(hDsp, 0);
	if( hDsp == NULL )
	{
		printf("Display Failed!!!\n");
		exit(-1);
	}
#endif	//	ENABLE_DISPLAY

	while( 1 )
	{
		if( 0 == bInit )
		{
			if( seqNeedMoreBuffer == 0 )
			{
				if( seqSize > 0 )
					memcpy( streamBuffer, seqData, seqSize );

				if( 0 != ReadStream( pReader, pReader->video_stream,  (streamBuffer+seqSize), &readSize, &isKey, &timeStamp ) )
				{
					break;
				}

				if( needKey )
				{
					if( !isKey )
					{
						continue;
					}
					needKey = 0;
				}
				memset( &seqIn, 0, sizeof(seqIn) );
				seqIn.addNumBuffers = 4;
				seqIn.enablePostFilter = 0;
				seqIn.seqInfo = streamBuffer;
				seqIn.seqSize = readSize+seqSize;
				seqIn.enableUserData = 0;
				seqIn.disableOutReorder = 0;
				vidRet = NX_VidDecInit( hDec, &seqIn, &seqOut );
				tmpSize = readSize+seqSize;
			}
			else
			{
				if( 0 != ReadStream( pReader, pReader->video_stream, streamBuffer, &readSize, &isKey, &timeStamp ) )
				{
					break;
				}
				memset( &seqIn, 0, sizeof(seqIn) );
				seqIn.addNumBuffers = 4;
				seqIn.enablePostFilter = 0;
				seqIn.seqInfo = streamBuffer;
				seqIn.seqSize = readSize;
				seqIn.enableUserData = 0;
				seqIn.disableOutReorder = 0;
				vidRet = NX_VidDecInit( hDec, &seqIn, &seqOut );
				tmpSize = readSize;
 			}

			if( vidRet == 1 )
			{
				seqNeedMoreBuffer = 1;
				continue;
			}

			if( 0 != vidRet )
			{
				printf("Initialize Failed!!!\n");
				return -1;
			}

			printf("<<<<<<<<<<< Init_Out >>>>>>>>>>>>>> \n");
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

			pos = 0;
			bInit = 1;
		}
		else
		{
			if( 0 != ReadStream( pReader, pReader->video_stream,  streamBuffer+pos, &readSize, &isKey, &timeStamp ) )
			{
				break;
			}
			pos += readSize;
			tmpSize = pos;
		}

		memset(&decIn, 0, sizeof(decIn));
		decIn.strmBuf = streamBuffer;
		decIn.strmSize = pos;
		decIn.timeStamp = timeStamp;
		decIn.eos = 0;

		startTime = NX_GetTickCount();
		vidRet = NX_VidDecDecodeFrame( hDec, &decIn, &decOut );
		endTime = NX_GetTickCount();

		pos = 0;
		if( vidRet == VID_NEED_STREAM )
		{
			printf("VID_NEED_MORE_BUF NX_VidDecDecodeFrame\n");
			continue;
		}
		if( vidRet < 0 )
		{
			printf("Decoding Error!!!\n");
			exit(-2);
		}

		printf("%2x %2x %2x %2x %2x %2x %2x %2x %2x %2x %2x %2x %2x %2x %2x %2x \n", decIn.strmBuf[0], decIn.strmBuf[1], decIn.strmBuf[2], decIn.strmBuf[3], decIn.strmBuf[4], decIn.strmBuf[5], decIn.strmBuf[6], decIn.strmBuf[7],
			decIn.strmBuf[8], decIn.strmBuf[9], decIn.strmBuf[10], decIn.strmBuf[11], decIn.strmBuf[12], decIn.strmBuf[13], decIn.strmBuf[14], decIn.strmBuf[15]);
		printf("Frame[%5d]: size=%6d, DspIdx=%2d, DecIdx=%2d, InTimeStamp=%7lld, outTimeStamp=%7lld, time=%6lld \n", frameCount, tmpSize, decOut.outImgIdx, decOut.outDecIdx, timeStamp, decOut.timeStamp, (endTime-startTime));
		//printf("interlace = %d(%d), Reliable = %d, MultiResel = %d, upW = %d, upH = %d\n", decOut.isInterlace, decOut.topFieldFirst, decOut.outFrmReliable_0_100, decOut.multiResolution, decOut.upSampledWidth, decOut.upSampledHeight);

		totalTime += (endTime-startTime);
		frameCount ++;

		if( decOut.outImgIdx >= 0  )
		{
//			printf("OutFrame[%5d]: Type = %d, DspIdx=%2d, timeStamp=%7lld\n", outCount, decOut.picType, decOut.outImgIdx, decOut.timeStamp );
#ifdef ENABLE_DISPLAY
			NX_DspQueueBuffer( hDsp, &decOut.outImg );
			if( outCount != 0 )
			{
				NX_DspDequeueBuffer( hDsp );
			}
#endif	//	ENABLE_DISPLAY

			if( fpOut )
			{
				int h;
				unsigned char *pbyImg = (unsigned char *)(decOut.outImg.luVirAddr);

				for(h=0 ; h<decOut.height ; h++)
				{
					fwrite( pbyImg, 1, decOut.width, fpOut );
					pbyImg += decOut.outImg.luStride;
				}

				pbyImg = (unsigned char *)(decOut.outImg.cbVirAddr);
				for(h=0 ; h<decOut.height/2 ; h++)
				{
					fwrite( pbyImg, 1, decOut.width/2, fpOut );
					pbyImg += decOut.outImg.cbStride;
				}

				pbyImg = (unsigned char *)(decOut.outImg.crVirAddr);
				for(h=0 ; h<decOut.height/2 ; h++)
				{
					fwrite( pbyImg, 1, decOut.width/2, fpOut );
					pbyImg += decOut.outImg.crStride;
				}
			}

			outCount ++;
			if( prevIdx != -1 )
			{
				NX_VidDecClrDspFlag( hDec, &decOut.outImg, prevIdx );
			}
			prevIdx = decOut.outImgIdx;
		}
	}
	NX_VidDecClose( hDec );

	printf("Avg Time = %6lld (%6lld / %d) \n", totalTime / frameCount, totalTime, frameCount);

#if (ENABLE_THEORA)
	//	Intialize Theora Parser
	if( vpuCodecType == NX_THEORA_DEC )
	{
		theora_parser_end(pReader->theoraParser);
	}
#endif

#ifdef ENABLE_DISPLAY
	NX_DspClose(hDsp);
#endif

	if ( fpOut )
		fclose(fpOut);

	return 0;
}



int aging_main( int argc, char *argv[] )
{
	FFMPEG_STREAM_READER *pReader;
	int vpuCodecType;
	VID_ERROR_E vidRet;
	NX_VID_SEQ_IN seqIn;
	NX_VID_SEQ_OUT seqOut;
	NX_VID_DEC_HANDLE hDec;
	NX_VID_DEC_IN decIn;
	NX_VID_DEC_OUT decOut;
	int readSize;
	int instanceIdx;
	av_register_all();

	while (1)
	{
		int seqSize = 0;
		int bInit=0, pos=0;
		int frameCount = 0, outCount=0;
		int prevIdx = -1;
		int isKey = 0;
		int needKey = 1;
		int mp4Class=0;
		int seqNeedMoreBuffer = 0;
		long long timeStamp = -1;

		pReader = OpenMediaFile( argv[1] );
		if( !pReader )
		{
			printf("Cannot open file!!!\n");
			return -1;
		}
		if( pReader->video_stream_idx == -1 )
		{
			printf("Cannot found video stream!!!\n");
			return -1;
		}
		vpuCodecType = CodecIdToVpuType( pReader->video_stream->codec->codec_id, pReader->video_stream->codec->codec_tag );
		if( vpuCodecType < 0 )
		{
			return -1;
		}
		mp4Class = fourCCToMp4Class( pReader->video_stream->codec->codec_tag );
		if( mp4Class == -1 )
			mp4Class = codecIdToMp4Class( pReader->video_stream->codec->codec_id );

		hDec = NX_VidDecOpen(vpuCodecType, mp4Class, 0, &instanceIdx);
		if( hDec == NULL )
		{
			printf("NX_VidDecOpen(%d) failed!!!\n", vpuCodecType);
			return -1;
		}
		seqSize = GetSequenceInformation( pReader, pReader->video_stream, seqData, sizeof(seqData) );
		if( seqSize == 0 )
		{
			printf("Have no SeqData!!!(vpuCodecType=%d)\n", vpuCodecType);
		}

		while( 1 )
		{
			if( 0 == bInit )
			{
				if( seqNeedMoreBuffer == 0 )
				{
					if( seqSize > 0 )
						memcpy( streamBuffer, seqData, seqSize );

					if( 0 != ReadStream( pReader, pReader->video_stream,  (streamBuffer+seqSize), &readSize, &isKey, &timeStamp ) )
					{
						break;
					}

					if( needKey )
					{
						if( !isKey )
						{
							continue;
						}
						needKey = 0;
					}
					memset( &seqIn, 0, sizeof(seqIn) );
					seqIn.seqInfo = streamBuffer;
					seqIn.seqSize = readSize+seqSize;
					seqIn.enableUserData = 0;
					vidRet = NX_VidDecInit( hDec, &seqIn, &seqOut );
				}
				else
				{
					if( 0 != ReadStream( pReader, pReader->video_stream, streamBuffer, &readSize, &isKey, &timeStamp ) )
					{
						break;
					}
					memset( &seqIn, 0, sizeof(seqIn) );
					seqIn.seqInfo = streamBuffer;
					seqIn.seqSize = readSize;
					seqIn.enableUserData = 0;
					vidRet = NX_VidDecInit( hDec, &seqIn, &seqOut );
				}

				if( vidRet == 1 )
				{
					seqNeedMoreBuffer = 1;
					continue;
				}

				if( 0 != vidRet )
				{
					printf("Initialize Failed!!!\n");
					return -1;
				}

				pos = 0;
				bInit = 1;
			}

			if( 0 != ReadStream( pReader, pReader->video_stream,  streamBuffer+pos, &readSize, &isKey, &timeStamp ) )
			{
				break;
			}
			pos += readSize;

			decIn.strmBuf = streamBuffer;
			decIn.strmSize = pos;
			decIn.timeStamp = timeStamp;
			decIn.eos = 0;
			vidRet = NX_VidDecDecodeFrame( hDec, &decIn, &decOut );
			pos = 0;
			if( vidRet == VID_NEED_STREAM )
			{
				frameCount ++;
				printf("VID_NEED_MORE_BUF NX_VidDecDecodeFrame\n");
				continue;
			}
			if( vidRet < 0 )
			{
				printf("Decoding Error!!!\n");
				exit(-2);
			}
			if( decOut.outImgIdx >= 0  )
			{
				outCount ++;

				if( prevIdx != -1 )
				{
					NX_VidDecClrDspFlag( hDec, &decOut.outImg, prevIdx );
				}
				prevIdx = decOut.outImgIdx;
			}
			if( outCount > 10 )
				break;
		}
		NX_VidDecClose( hDec );

		CloseFile( pReader );
		system("free");
	}
	return 0;
}





int main( int argc, char *argv[] )
{
//	return aging_main( argc, argv );
	return dec_main( argc, argv );
}
