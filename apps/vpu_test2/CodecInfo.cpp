#include <ctype.h>
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
#include "libavutil/pixdesc.h"
#include "libavdevice/avdevice.h"
#ifdef __cplusplus
}
#endif

#include <nx_video_api.h>

typedef struct {
    int codStd;
	int mp4Class;
    int codec_id;
	unsigned int fourcc;
} CodStdTab;

#ifndef MKTAG
#define MKTAG(a,b,c,d) (a | (b << 8) | (c << 16) | (d << 24))
#endif

const CodStdTab codstd_tab[] = {
    { NX_AVC_DEC,	0, CODEC_ID_H264,		MKTAG('H', '2', '6', '4') },
    { NX_AVC_DEC,	0, CODEC_ID_H264,		MKTAG('X', '2', '6', '4') },
    { NX_AVC_DEC,	0, CODEC_ID_H264,		MKTAG('A', 'V', 'C', '1') },
    { NX_AVC_DEC,	0, CODEC_ID_H264,		MKTAG('V', 'S', 'S', 'H') },
    { NX_H263_DEC,	0, CODEC_ID_H263,		MKTAG('H', '2', '6', '3') },
    { NX_H263_DEC,	0, CODEC_ID_H263,		MKTAG('X', '2', '6', '3') },
    { NX_H263_DEC,	0, CODEC_ID_H263,		MKTAG('T', '2', '6', '3') },
    { NX_H263_DEC,	0, CODEC_ID_H263,		MKTAG('L', '2', '6', '3') },
    { NX_H263_DEC,	0, CODEC_ID_H263,		MKTAG('V', 'X', '1', 'K') },
    { NX_H263_DEC,	0, CODEC_ID_H263,		MKTAG('Z', 'y', 'G', 'o') },
    { NX_H263_DEC,	0, CODEC_ID_H263,		MKTAG('H', '2', '6', '3') },
    { NX_H263_DEC,	0, CODEC_ID_H263,		MKTAG('I', '2', '6', '3') },	/* intel h263 */
    { NX_H263_DEC,	0, CODEC_ID_H263,		MKTAG('H', '2', '6', '1') },
    { NX_H263_DEC,	0, CODEC_ID_H263,		MKTAG('U', '2', '6', '3') },
    { NX_H263_DEC,	0, CODEC_ID_H263,		MKTAG('V', 'I', 'V', '1') },
    { NX_MP4_DEC,	0, CODEC_ID_MPEG4,		MKTAG('F', 'M', 'P', '4') },
    { NX_MP4_DEC,	5, CODEC_ID_MPEG4,		MKTAG('D', 'I', 'V', 'X') },	// DivX 4
    { NX_MP4_DEC,	1, CODEC_ID_MPEG4,		MKTAG('D', 'X', '5', '0') },
    { NX_MP4_DEC,	2, CODEC_ID_MPEG4,		MKTAG('X', 'V', 'I', 'D') },
    { NX_MP4_DEC,	0, CODEC_ID_MPEG4,		MKTAG('M', 'P', '4', 'S') },
    { NX_MP4_DEC,	0, CODEC_ID_MPEG4,		MKTAG('M', '4', 'S', '2') },	//MPEG-4 version 2 simple profile
    { NX_MP4_DEC,	0, CODEC_ID_MPEG4,		MKTAG( 4 ,  0 ,  0 ,  0 ) },	/* some broken avi use this */
    { NX_MP4_DEC,	0, CODEC_ID_MPEG4,		MKTAG('D', 'I', 'V', '1') },
    { NX_MP4_DEC,	0, CODEC_ID_MPEG4,		MKTAG('B', 'L', 'Z', '0') },
    { NX_MP4_DEC,	0, CODEC_ID_MPEG4,		MKTAG('M', 'P', '4', 'V') },
    { NX_MP4_DEC,	0, CODEC_ID_MPEG4,		MKTAG('U', 'M', 'P', '4') },
    { NX_MP4_DEC,	0, CODEC_ID_MPEG4,		MKTAG('W', 'V', '1', 'F') },
    { NX_MP4_DEC,	0, CODEC_ID_MPEG4,		MKTAG('S', 'E', 'D', 'G') },
    { NX_MP4_DEC,	0, CODEC_ID_MPEG4,		MKTAG('R', 'M', 'P', '4') },
    { NX_MP4_DEC,	0, CODEC_ID_MPEG4,		MKTAG('3', 'I', 'V', '2') },
    { NX_MP4_DEC,	0, CODEC_ID_MPEG4,		MKTAG('F', 'F', 'D', 'S') },
    { NX_MP4_DEC,	0, CODEC_ID_MPEG4,		MKTAG('F', 'V', 'F', 'W') },
    { NX_MP4_DEC,	0, CODEC_ID_MPEG4,		MKTAG('D', 'C', 'O', 'D') },
    { NX_MP4_DEC,	0, CODEC_ID_MPEG4,		MKTAG('M', 'V', 'X', 'M') },
    { NX_MP4_DEC,	0, CODEC_ID_MPEG4,		MKTAG('P', 'M', '4', 'V') },
    { NX_MP4_DEC,	0, CODEC_ID_MPEG4,		MKTAG('S', 'M', 'P', '4') },
    { NX_MP4_DEC,	0, CODEC_ID_MPEG4,		MKTAG('D', 'X', 'G', 'M') },
    { NX_MP4_DEC,	0, CODEC_ID_MPEG4,		MKTAG('V', 'I', 'D', 'M') },
    { NX_MP4_DEC,	0, CODEC_ID_MPEG4,		MKTAG('M', '4', 'T', '3') },
    { NX_MP4_DEC,	0, CODEC_ID_MPEG4,		MKTAG('G', 'E', 'O', 'X') },
    { NX_MP4_DEC,	0, CODEC_ID_MPEG4,		MKTAG('H', 'D', 'X', '4') }, /* flipped video */
    { NX_MP4_DEC,	0, CODEC_ID_MPEG4,		MKTAG('D', 'M', 'K', '2') },
    { NX_MP4_DEC,	0, CODEC_ID_MPEG4,		MKTAG('D', 'I', 'G', 'I') },
    { NX_MP4_DEC,	0, CODEC_ID_MPEG4,		MKTAG('I', 'N', 'M', 'C') },
    { NX_MP4_DEC,	0, CODEC_ID_MPEG4,		MKTAG('E', 'P', 'H', 'V') }, /* Ephv MPEG-4 */
    { NX_MP4_DEC,	0, CODEC_ID_MPEG4,		MKTAG('E', 'M', '4', 'A') },
    { NX_MP4_DEC,	0, CODEC_ID_MPEG4,		MKTAG('M', '4', 'C', 'C') }, /* Divio MPEG-4 */
    { NX_MP4_DEC,	0, CODEC_ID_MPEG4,		MKTAG('S', 'N', '4', '0') },
    { NX_MP4_DEC,	0, CODEC_ID_MPEG4,		MKTAG('V', 'S', 'P', 'X') },
    { NX_MP4_DEC,	0, CODEC_ID_MPEG4,		MKTAG('U', 'L', 'D', 'X') },
    { NX_MP4_DEC,	0, CODEC_ID_MPEG4,		MKTAG('G', 'E', 'O', 'V') },
    { NX_MP4_DEC,	0, CODEC_ID_MPEG4,		MKTAG('S', 'I', 'P', 'P') }, /* Samsung SHR-6040 */
    { NX_DIV3_DEC,	0, CODEC_ID_MSMPEG4V3,	MKTAG('D', 'I', 'V', '3') }, /* default signature when using MSMPEG4 */
	{ NX_DIV3_DEC,	0, CODEC_ID_MSMPEG4V3,	MKTAG('M', 'P', '4', '3') },
    { NX_DIV3_DEC,	0, CODEC_ID_MSMPEG4V3,	MKTAG('M', 'P', 'G', '3') },
    { NX_MP4_DEC,	1, CODEC_ID_MSMPEG4V3,	MKTAG('D', 'I', 'V', '5') },
    { NX_MP4_DEC,	1, CODEC_ID_MSMPEG4V3,	MKTAG('D', 'I', 'V', '6') },
    { NX_MP4_DEC,	5, CODEC_ID_MSMPEG4V3,	MKTAG('D', 'I', 'V', '4') },
	{ NX_DIV3_DEC,	0, CODEC_ID_MSMPEG4V3,	MKTAG('D', 'V', 'X', '3') },
    { NX_DIV3_DEC,	0, CODEC_ID_MSMPEG4V3,	MKTAG('A', 'P', '4', '1') },	//Another hacked version of Microsoft's MP43 codec.
    { NX_MP4_DEC,	0, CODEC_ID_MSMPEG4V3,	MKTAG('C', 'O', 'L', '1') },
    { NX_MP4_DEC,	0, CODEC_ID_MSMPEG4V3,	MKTAG('C', 'O', 'L', '0') },	// not support ms mpeg4 v1, 2
	{ NX_MP4_DEC, 256, CODEC_ID_FLV1,		MKTAG('F', 'L', 'V', '1') }, /* Sorenson spark */
    { NX_VC1_DEC,	0, CODEC_ID_WMV1,		MKTAG('W', 'M', 'V', '1') },
    { NX_VC1_DEC,	0, CODEC_ID_WMV2,		MKTAG('W', 'M', 'V', '2') },
	{ NX_MP2_DEC,	0, CODEC_ID_MPEG1VIDEO,	MKTAG('M', 'P', 'G', '1') },
    { NX_MP2_DEC,	0, CODEC_ID_MPEG1VIDEO,	MKTAG('M', 'P', 'G', '2') },
    { NX_MP2_DEC,	0, CODEC_ID_MPEG2VIDEO,	MKTAG('M', 'P', 'G', '2') },
    { NX_MP2_DEC,	0, CODEC_ID_MPEG2VIDEO,	MKTAG('M', 'P', 'E', 'G') },
	{ NX_MP2_DEC,	0, CODEC_ID_MPEG1VIDEO,	MKTAG('M', 'P', '2', 'V') },
    { NX_MP2_DEC,	0, CODEC_ID_MPEG1VIDEO,	MKTAG('P', 'I', 'M', '1') },
    { NX_MP2_DEC,	0, CODEC_ID_MPEG2VIDEO,	MKTAG('P', 'I', 'M', '2') },
    { NX_MP2_DEC,	0, CODEC_ID_MPEG1VIDEO,	MKTAG('V', 'C', 'R', '2') },
    { NX_MP2_DEC,	0, CODEC_ID_MPEG1VIDEO,	MKTAG( 1 ,  0 ,  0 ,  16) },
    { NX_MP2_DEC,	0, CODEC_ID_MPEG2VIDEO,	MKTAG( 2 ,  0 ,  0 ,  16) },
    { NX_MP4_DEC,	0, CODEC_ID_MPEG4,		MKTAG( 4 ,  0 ,  0 ,  16) },
    { NX_MP2_DEC,	0, CODEC_ID_MPEG2VIDEO,	MKTAG('D', 'V', 'R', ' ') },
    { NX_MP2_DEC,	0, CODEC_ID_MPEG2VIDEO,	MKTAG('M', 'M', 'E', 'S') },
    { NX_MP2_DEC,	0, CODEC_ID_MPEG2VIDEO,	MKTAG('L', 'M', 'P', '2') }, /* Lead MPEG2 in avi */
    { NX_MP2_DEC,	0, CODEC_ID_MPEG2VIDEO,	MKTAG('S', 'L', 'I', 'F') },
    { NX_MP2_DEC,	0, CODEC_ID_MPEG2VIDEO,	MKTAG('E', 'M', '2', 'V') },
    { NX_VC1_DEC,	0, CODEC_ID_WMV3,		MKTAG('W', 'M', 'V', '3') },
    { NX_VC1_DEC,	0, CODEC_ID_VC1,		MKTAG('W', 'V', 'C', '1') },
    { NX_VC1_DEC,	0, CODEC_ID_VC1,		MKTAG('W', 'M', 'V', 'A') },

    { NX_RV_DEC,	0, CODEC_ID_RV30,		MKTAG('R', 'V', '3', '0') },
    { NX_RV_DEC,	0, CODEC_ID_RV40,		MKTAG('R', 'V', '4', '0') },
    { NX_THEORA_DEC,0, CODEC_ID_THEORA,     MKTAG('T', 'H', 'E', 'O') },
    { NX_VP8_DEC,   0, CODEC_ID_VP8,        MKTAG('V', 'P', '8', '0') }
#if 0
	{ STD_AVS,		0, CODEC_ID_CAVS,		MKTAG('C','A','V','S') },
	{ STD_AVS,		0, CODEC_ID_AVS,		MKTAG('A','V','S','2') },
    { STD_VP3,		0, CODEC_ID_VP3,		MKTAG('V', 'P', '3', '0') },
    { STD_VP3,		0, CODEC_ID_VP3,		MKTAG('V', 'P', '3', '1') },
#endif
};

int fourCCToMp4Class(unsigned int fourcc)
{
	unsigned int i;
	int mp4Class = -1;
	char str[5];

	str[0] = toupper((char)fourcc);
	str[1] = toupper((char)(fourcc>>8));
	str[2] = toupper((char)(fourcc>>16));
	str[3] = toupper((char)(fourcc>>24));
	str[4] = '\0';

	for(i=0; i<sizeof(codstd_tab)/sizeof(codstd_tab[0]); i++)
	{
		if (codstd_tab[i].fourcc == (unsigned int)MKTAG(str[0], str[1], str[2], str[3]) )
		{
			mp4Class = codstd_tab[i].mp4Class;
			break;
		}
	}

	return mp4Class;
}

int fourCCToCodStd(unsigned int fourcc)
{
	int codStd = -1;
	unsigned int i;

	char str[5];

	str[0] = toupper((char)fourcc);
	str[1] = toupper((char)(fourcc>>8));
	str[2] = toupper((char)(fourcc>>16));
	str[3] = toupper((char)(fourcc>>24));
	str[4] = '\0';

	for(i=0; i<sizeof(codstd_tab)/sizeof(codstd_tab[0]); i++)
	{
		if (codstd_tab[i].fourcc == (unsigned int)MKTAG(str[0], str[1], str[2], str[3]))
		{
			codStd = codstd_tab[i].codStd;
			break;
		}
	}

	return codStd;
}

int codecIdToMp4Class(int codec_id)
{
	int mp4Class = -1;
	unsigned int i;

	for(i=0; i<sizeof(codstd_tab)/sizeof(codstd_tab[0]); i++)
	{
		if (codstd_tab[i].codec_id == codec_id)
		{
			mp4Class = codstd_tab[i].mp4Class;
			break;
		}
	}

	return mp4Class;

}
int codecIdToCodStd(int codec_id)
{
	int codStd = -1;
	unsigned int i;

	for(i=0; i<sizeof(codstd_tab)/sizeof(codstd_tab[0]); i++)
	{
		if (codstd_tab[i].codec_id == codec_id)
		{
			codStd = codstd_tab[i].codStd;
			break;
		}
	}
	return codStd;
}

int codecIdToFourcc(int codec_id)
{
	int fourcc = 0;
	unsigned int i;

	for(i=0; i<sizeof(codstd_tab)/sizeof(codstd_tab[0]); i++)
	{
		if (codstd_tab[i].codec_id == codec_id)
		{
			fourcc = codstd_tab[i].fourcc;
			break;
		}
	}
	return fourcc;
}

VID_TYPE_E CodecIdToVpuType( int codecId, unsigned int fourcc )
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
	else if( codecId == CODEC_ID_THEORA )
	{
		vpuCodecType = NX_THEORA_DEC;
	}
	else if( codecId == CODEC_ID_VP8 )
	{
		vpuCodecType = NX_VP8_DEC;
	}
	else
	{
		printf("Cannot support codecid(%d)\n", codecId);
		exit(-1);
	}
	return (VID_TYPE_E)vpuCodecType;
}