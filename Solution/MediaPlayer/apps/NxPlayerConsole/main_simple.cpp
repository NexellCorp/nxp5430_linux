//------------------------------------------------------------------------------
//
//	Copyright (C) 2015 Nexell Co. All Rights Reserved
//	Nexell Co. Proprietary & Confidential
//
//	NEXELL INFORMS THAT THIS CODE AND INFORMATION IS PROVIDED "AS IS" BASE
//  AND	WITHOUT WARRANTY OF ANY KIND, EITHER EXPRESSED OR IMPLIED, INCLUDING
//  BUT NOT LIMITED TO THE IMPLIED WARRANTIES OF MERCHANTABILITY AND/OR FITNESS
//  FOR A PARTICULAR PURPOSE.
//
//	Module		: 
//	File		: 
//	Description	: 
//	Author		: 
//	Export		: 
//	History		: 
//
//------------------------------------------------------------------------------

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>

#include <poll.h>
#include <fcntl.h>
#include <signal.h>
#include <termios.h>
#include <linux/fb.h>
#include <sys/ioctl.h>

#ifdef ANDROID
#include <cutils/log.h>
#include <gui/Surface.h>
#include <gui/SurfaceComposerClient.h>
using namespace android;
#else
#include <uevent.h>
#endif

#include <nx_dsp.h>
#include <NX_MoviePlay.h>

#include "NX_CCmdQueue.h"

#define DISPLAY_WIDTH		1024
#define DISPLAY_HEIGHT		600

// #define DUAL_DISPLAY

typedef struct AppData {
	MP_HANDLE 		hPlayer;
	MP_MEDIA_INFO	MediaInfo;

	int32_t			bThreadExit;

	NX_CCmdQueue	*pCmdQueue;

	char			*pUri;
	int32_t			iVolume;
	int32_t			iVideoIndex;
	int32_t			iAudioIndex;
} AppData;

AppData gAppData;

////////////////////////////////////////////////////////////////////////////////
//
//	Signal Handler
//
static void signal_handler( int sig )
{
	printf("Aborted by signal %s (%d)...\n", (char*)strsignal(sig), sig);

	switch(sig)
	{
		case SIGINT :
			printf("SIGINT..\n");	break;
		case SIGTERM :
			printf("SIGTERM..\n");	break;
		case SIGABRT :
			printf("SIGABRT..\n");	break;
		case SIGSEGV :
			printf("SIGSEGV..\n");	break;
		default :					break;
	}

	NX_DspVideoSetPriority(DISPLAY_MODULE_MLC0 , 1);

	if( gAppData.pUri )
	{
		free( gAppData.pUri );
	}

	if( gAppData.pCmdQueue )
	{
		delete gAppData.pCmdQueue;
	}

	if( gAppData.hPlayer )
	{
		NX_MPStop( gAppData.hPlayer );
		NX_MPClose( gAppData.hPlayer );
		
		gAppData.bThreadExit = true;
		gAppData.hPlayer = NULL;
	}

	exit( EXIT_FAILURE );
}

static void register_signal(void)
{
	signal( SIGINT, signal_handler );
	signal( SIGTERM, signal_handler );
	signal( SIGABRT, signal_handler );
	signal( SIGSEGV, signal_handler );
}


////////////////////////////////////////////////////////////////////////////////
//
//	Tools
//
static int32_t GetTrackIndex( int32_t trackType, int32_t trackNum )
{
	int32_t index = -1, trackOrder = 0;

	for( int32_t i = 0; i < gAppData.MediaInfo.iProgramNum; i++)
	{
		for( int32_t j = 0; j < gAppData.MediaInfo.ProgramInfo[i].iVideoNum + gAppData.MediaInfo.ProgramInfo[i].iAudioNum; j++ )
		{
			if( trackType == gAppData.MediaInfo.ProgramInfo[i].TrackInfo[j].iTrackType )
			{
				if( trackNum == trackOrder )
				{
					index = gAppData.MediaInfo.ProgramInfo[i].TrackInfo[j].iTrackIndex;
					printf( "[%s] Require Track( %d ), Stream Index( %d )\n", (trackType == MP_TRACK_VIDEO) ? "VIDEO" : "AUDIO", trackNum, index );
					return index;
				}
				trackOrder++;
			}
		}
	}

	return index;
}

static void GetScreenResolution( int32_t *width, int32_t *height )
{
	int32_t fb;
	struct	fb_var_screeninfo  fbvar;

	*width = *height = 0;

	fb = open( "/dev/fb0", O_RDWR);
	ioctl( fb, FBIOGET_VSCREENINFO, &fbvar);

	*width	= fbvar.xres;
	*height	= fbvar.yres;
	if( fb ) close( fb );

	printf("Screen Width( %d ) x Height( %d )\n", *width, *height );
}

static void GetVideoResolution( int32_t trackNum, int32_t *width, int32_t *height )
{
	int32_t trackOrder = 0;

	*width = *height = 0;

	for( int32_t i = 0; i < gAppData.MediaInfo.iProgramNum; i++)
	{
		for( int32_t j = 0; gAppData.MediaInfo.ProgramInfo[i].iVideoNum + gAppData.MediaInfo.ProgramInfo[i].iAudioNum; j++ )
		{
			if( MP_TRACK_VIDEO == gAppData.MediaInfo.ProgramInfo[i].TrackInfo[j].iTrackType )
			{
				if( trackNum == trackOrder )
				{
					*width	= gAppData.MediaInfo.ProgramInfo[i].TrackInfo[j].iWidth;
					*height	= gAppData.MediaInfo.ProgramInfo[i].TrackInfo[j].iHeight;
					return;
				}
				trackOrder++;
			}
		}
	}
}

static void GetVideoPosition( int32_t dspPort, int32_t trackNum, int32_t *x, int32_t *y, int32_t *width, int32_t *height )
{
	double xRatio, yRatio;

	int32_t scrWidth = 0, scrHeight = 0;
	int32_t vidWidth = 0, vidHeight = 0;
	
	*x = *y = *width = *height = 0;

	GetVideoResolution( trackNum, &vidWidth, &vidHeight );
	GetScreenResolution( &scrWidth, &scrHeight );

	xRatio = (double)scrWidth / (double)vidWidth;
	yRatio = (double)scrHeight / (double)vidHeight;

	if( xRatio > yRatio )
	{
		*width = vidWidth * yRatio;
		*height = scrHeight;
		
		*x = abs( scrWidth - *width ) / 2;
		*y = 0;
	}
	else
	{
		*width = scrWidth;
		*height = vidHeight * xRatio;
		
		*x = 0;
		*y = abs( scrHeight - *height ) / 2;
	}

	printf("VideoPosition( %d, %d, %d, %d )\n", *x, *y, *width, *height );
}

static int32_t PrintMediaInfo( MP_MEDIA_INFO *pMediaInfo )
{
	printf("--------------------------------------------------------------------------------\n");
	printf("* Media Information\n" );
	printf(" -. ProgramNum( %d ), VideoTrack( %d ), AudioTrack( %d ), SubTitleTrack( %d ), DataTrack( %d )\n\n", 
		pMediaInfo->iProgramNum, pMediaInfo->iVideoTrackNum, pMediaInfo->iAudioTrackNum, pMediaInfo->iSubTitleTrackNum, pMediaInfo->iDataTrackNum );

	for( int32_t i = 0; i < pMediaInfo->iProgramNum; i++ )
	{
		printf("--------------------------------------------------------------------------------\n");
		printf("[ PROGRAM #%d ]\n", i );
		printf(" -. VideoNum( %d ), AudioNum( %d ), SubTitleNum( %d ), DataNum( %d ), Duration( %lld )\n\n", 
			pMediaInfo->ProgramInfo[i].iVideoNum, pMediaInfo->ProgramInfo[i].iAudioNum, pMediaInfo->ProgramInfo[i].iSubTitleNum, pMediaInfo->ProgramInfo[i].iDataNum, pMediaInfo->ProgramInfo[i].iDuration);
		
		if( 0 < pMediaInfo->ProgramInfo[i].iVideoNum )
		{
			int32_t num = 0;
			printf("[ Video Information ]\n" );

			for( int32_t j = 0; j < pMediaInfo->ProgramInfo[i].iVideoNum + pMediaInfo->ProgramInfo[i].iAudioNum; j++ )
			{
				MP_TRACK_INFO *pTrackInfo = &pMediaInfo->ProgramInfo[i].TrackInfo[j];
				
				if( MP_TRACK_VIDEO == pTrackInfo->iTrackType ) {
					printf(" Video Track #%d\n", num++);
					printf("  -. Track Index : %d\n", pTrackInfo->iTrackIndex);
					printf("  -. Codec Type  : %d\n", (int32_t)pTrackInfo->iCodecId);
					printf("  -. Resolution  : %d x %d\n", pTrackInfo->iWidth, pTrackInfo->iHeight);
					if( 0 > pTrackInfo->iDuration )
						printf("  -. Duration    : Unknown\n\n");
					else
						printf("  -. Duration    : %lld ms\n\n", pTrackInfo->iDuration);
				}
			}
		}

		if( 0 < pMediaInfo->ProgramInfo[i].iAudioNum )
		{
			int32_t num = 0;
			printf("[ Audio Information ]\n" );

			for( int32_t j = 0; j < pMediaInfo->ProgramInfo[i].iVideoNum + pMediaInfo->ProgramInfo[i].iAudioNum; j++ )
			{
				MP_TRACK_INFO *pTrackInfo = &pMediaInfo->ProgramInfo[i].TrackInfo[j];

				if( MP_TRACK_AUDIO == pTrackInfo->iTrackType ) {
					printf(" Audio Track #%d\n", num++ );
					printf("  -. Track Index : %d\n", pTrackInfo->iTrackIndex );
					printf("  -. Codec Type  : %d\n", (int32_t)pTrackInfo->iCodecId );
					printf("  -. Channels    : %d\n", pTrackInfo->iChannels );
					printf("  -. SampleRate  : %d Hz\n", pTrackInfo->iSampleRate );
					printf("  -. Bitrate     : %d bps\n", pTrackInfo->iBitrate );
					if( 0 > pTrackInfo->iDuration )
						printf("  -. Duration    : Unknown\n\n" );
					else
						printf("  -. Duration    : %lld ms\n\n", pTrackInfo->iDuration );
				}
			}
		}
	}
	printf("--------------------------------------------------------------------------------\n");

	usleep(1000000);
	return 0;
}


////////////////////////////////////////////////////////////////////////////////
//
//	Event Callback
//
static void cbEventCallback( void *privateDesc, unsigned int EventType, unsigned int /*EventData*/, unsigned int /*param*/ )
{
	AppData *pAppData = (AppData*)privateDesc;

	if( EventType == MP_MSG_EOS )
	{
		printf("%s(): End of stream. ( privateDesc = 0x%08x )\n", __func__, (int32_t)pAppData );
		if( pAppData )
		{
			CMD_MESSAGE cmd;
			cmd.iCmdType = CMD_TYPE_PLAY;
			pAppData->pCmdQueue->PushCommand( &cmd );
		}
	}
	else if( EventType == MP_MSG_DEMUX_ERR )
	{
		printf("%s(): Cannot play contents.\n", __func__);
		if( pAppData )
		{
			pAppData->bThreadExit = true;
		}
	}
}


////////////////////////////////////////////////////////////////////////////////
//
//	Application
//
void print_usage( const char *appName )
{
	printf(" Usage: %s [options]\n", appName);
	printf("   -f [filename]    : Input FileName.\n");
}

int32_t main( int32_t argc, char *argv[] )
{
	int32_t iRet = 0, opt = 0;

	if( 2 > argc )
	{
		print_usage( argv[0] );
		return 0;
	}

	register_signal();
	memset( &gAppData, 0x00, sizeof(AppData) );

	while( -1 != (opt=getopt(argc, argv, "hf:")) )
	{
		switch( opt )
		{
		case 'h':
			print_usage(argv[0]);
			return 0;
		case 'f':
			gAppData.pUri = strdup(optarg);
			break;
		default:
			break;
		}
	}	
	
	int32_t iVersion		= NX_MPGetVersion();
	int32_t iMajorVersion 	= (iVersion & 0XFF000000) >> 24;
	int32_t iMinorVersion 	= (iVersion & 0x00FF0000) >> 16;
	int32_t iRevisionVersion= (iVersion & 0x0000FF00) >> 8;

	printf("############################## STARTING APPLICATION ##############################\n");
	printf(" Simpe Player based Filter                       \n");
	printf(" -. Library Version : %d.%d.%d                   \n", iMajorVersion, iMinorVersion, iRevisionVersion);
	printf(" -. Build Time      : %s, %s                     \n", __TIME__, __DATE__);
	printf(" -. Author          : SW2 Team.                  \n");
	printf("##################################################################################\n");

	gAppData.pCmdQueue = new NX_CCmdQueue();

	NX_MPOpen( &gAppData.hPlayer, &cbEventCallback, &gAppData );

	if( 0 > (iRet = NX_MPSetUri( gAppData.hPlayer, gAppData.pUri )) )
	{
		switch( iRet )
		{
		case MP_NOT_SUPPORT_AUDIOCODEC:
			printf("Fail, Not Support Audio Codec.\n");
			break;

		case MP_NOT_SUPPORT_VIDEOCODEC:
			printf("Fail, Not Support Video Codec.\n");
			break;

		case MP_NOT_SUPPORT_VIDEOWIDTH:
		case MP_NOT_SUPPORT_VIDEOHEIGHT:
			printf("Fail, Not Support Video Size.\n");
			break;

		default:
			printf("Fail, NX_MPSetUri().\n");
			break;
		}

		NX_MPClose( gAppData.hPlayer );
		return -1;
	}
	
	memset( &gAppData.MediaInfo, 0x00, sizeof(MP_MEDIA_INFO) );
	NX_MPGetMediaInfo( gAppData.hPlayer, &gAppData.MediaInfo );
	PrintMediaInfo( &gAppData.MediaInfo );

	gAppData.iVideoIndex = GetTrackIndex( MP_TRACK_VIDEO, 0 );
	gAppData.iAudioIndex = GetTrackIndex( MP_TRACK_AUDIO, 0 );

	if( gAppData.MediaInfo.iVideoTrackNum > 0 )
	{
		MP_DSP_CONFIG dspConf;
		int32_t x, y, width, height;

		memset( &dspConf, 0x00, sizeof(dspConf) );

		dspConf.iPort				= DISPLAY_PORT_LCD;
		dspConf.iModule				= DISPLAY_MODULE_MLC0;
		
		GetVideoPosition( dspConf.iPort, 0, &x, &y, &width, &height );

		dspConf.dstRect.iX			= x;
		dspConf.dstRect.iY			= y;
		dspConf.dstRect.iWidth		= width;
		dspConf.dstRect.iHeight		= height;

		NX_DspVideoSetPriority(DISPLAY_MODULE_MLC0 , 0);
		NX_MPAddTrack( gAppData.hPlayer, gAppData.iVideoIndex, &dspConf );
	}

	if( gAppData.MediaInfo.iAudioTrackNum > 0 )
	{
		NX_MPAddTrack( gAppData.hPlayer, gAppData.iAudioIndex, NULL, false );

		gAppData.iVolume = 100;
		NX_MPSetVolume( gAppData.hPlayer, gAppData.iVolume );
	}

	NX_MPPlay( gAppData.hPlayer );
	
	CMD_MESSAGE msg;

	while( !gAppData.bThreadExit )
	{
		int32_t ret = gAppData.pCmdQueue->PopCommand( &msg );
		usleep(300000);

		if( 0 > ret )
			continue;

		switch( msg.iCmdType )
		{
		case CMD_TYPE_PLAY:
			NX_MPStop( gAppData.hPlayer );
			NX_MPClose( gAppData.hPlayer );

			NX_MPOpen( &gAppData.hPlayer, &cbEventCallback, &gAppData );
			NX_MPSetUri( gAppData.hPlayer, gAppData.pUri );

			memset( &gAppData.MediaInfo, 0x00, sizeof(MP_MEDIA_INFO) );
			NX_MPGetMediaInfo( gAppData.hPlayer, &gAppData.MediaInfo );
			PrintMediaInfo( &gAppData.MediaInfo );

			gAppData.iVideoIndex = GetTrackIndex( MP_TRACK_VIDEO, 0 );
			gAppData.iAudioIndex = GetTrackIndex( MP_TRACK_AUDIO, 0 );

			MP_DSP_CONFIG dspConf;
			int32_t x, y, width, height;

			memset( &dspConf, 0x00, sizeof(dspConf) );

			dspConf.iPort				= DISPLAY_PORT_LCD;
			dspConf.iModule				= DISPLAY_MODULE_MLC0;
			
			GetVideoPosition( dspConf.iPort, 0, &x, &y, &width, &height );

			dspConf.dstRect.iX			= x;
			dspConf.dstRect.iY			= y;
			dspConf.dstRect.iWidth		= width;
			dspConf.dstRect.iHeight		= height;

			NX_MPAddTrack( gAppData.hPlayer, gAppData.iVideoIndex, &dspConf );
			NX_MPAddTrack( gAppData.hPlayer, gAppData.iAudioIndex, NULL, false );

			gAppData.iVolume = 100;
			NX_MPSetVolume( gAppData.hPlayer, gAppData.iVolume );
			NX_MPPlay( gAppData.hPlayer );
			break;

		default:
			break;
		}
	}

	if( gAppData.pUri )
	{
		free( gAppData.pUri );
	}

	if( gAppData.pCmdQueue )
	{
		delete gAppData.pCmdQueue;
	}

	if( gAppData.hPlayer )
	{
		NX_MPStop( gAppData.hPlayer );
		NX_MPClose( gAppData.hPlayer );
		
		gAppData.bThreadExit = true;
		gAppData.hPlayer = NULL;
	}

	return 0;
}
