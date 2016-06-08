#ifndef	__CODEC_INFO_H__
#define	__CODEC_INFO_H__

int fourCCToMp4Class(unsigned int fourcc);
int fourCCToCodStd(unsigned int fourcc);
int codecIdToMp4Class(int codec_id);
int codecIdToCodStd(int codec_id);
int codecIdToFourcc(int codec_id);
int CodecIdToVpuType( int codecId, unsigned int fourcc );

#endif	//	__CODEC_INFO_H__