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
//------------------------------------------------------------------------------

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>

#include <poll.h>
#include <fcntl.h>
#include <signal.h>
#include <linux/fb.h>
#include <sys/ioctl.h>

#include <uevent.h>

#include <nx_dsp.h>
#include <NX_MoviePlay.h>

#include "NX_CCmdQueue.h"

typedef struct AppDataStruct {
	MP_HANDLE 		hPlayer;
	MP_MEDIA_INFO	MediaInfo;

	int32_t			bThreadExit;

	NX_CCmdQueue	*pCmdQueue;

	pthread_t		hHdmiThread;
	pthread_t		hConsoleThread;

	int32_t			bHdmi;
	int32_t			iVolume;

	int32_t			iVideoIndex;
	int32_t			iAudioIndex;
	int32_t			iAudioPath;
	char 			Uri[2048];

	int32_t 			bExit;

	// Player Status
	int64_t			iPositionSave;
	int32_t			bPause;

	//Thread
	pthread_t		hCmdThread;
	int32_t			bCmdThreadExit;
	int32_t			bHdmiThreadExit;

} AppDataStruct;

AppDataStruct AppData;

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
			printf("SIGINT..\n");
			break;
		case SIGTERM :
			printf("SIGTERM..\n");
			break;
		case SIGABRT :
			printf("SIGABRT..\n");
			break;
		case SIGSEGV :
			printf("SIGSEGV..\n");
			break;
		default :
			break;
	}

	NX_MPStop( AppData.hPlayer );
	NX_MPClose( AppData.hPlayer );
		
	AppData.bThreadExit = true;
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

	for( int32_t i = 0; i < AppData.MediaInfo.iProgramNum; i++)
	{
		for( int32_t j = 0; j < AppData.MediaInfo.ProgramInfo[i].iVideoNum + AppData.MediaInfo.ProgramInfo[i].iAudioNum; j++ )
		{
			if( trackType == AppData.MediaInfo.ProgramInfo[i].TrackInfo[j].iTrackType )
			{
				if( trackNum == trackOrder )
				{
					index = AppData.MediaInfo.ProgramInfo[i].TrackInfo[j].iTrackIndex;
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

	for( int32_t i = 0; i < AppData.MediaInfo.iProgramNum; i++)
	{
		for( int32_t j = 0; AppData.MediaInfo.ProgramInfo[i].iVideoNum + AppData.MediaInfo.ProgramInfo[i].iAudioNum; j++ )
		{
			if( MP_TRACK_VIDEO == AppData.MediaInfo.ProgramInfo[i].TrackInfo[j].iTrackType )
			{
				if( trackNum == trackOrder )
				{
					*width	= AppData.MediaInfo.ProgramInfo[i].TrackInfo[j].iWidth;
					*height	= AppData.MediaInfo.ProgramInfo[i].TrackInfo[j].iHeight;
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

	if( dspPort == DISPLAY_PORT_LCD )
	{
		GetScreenResolution( &scrWidth, &scrHeight );
	}
	else
	{
		scrWidth	= 1920;
		scrHeight	= 1080;
	}

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

	//usleep(1000000);
	return 0;
}


////////////////////////////////////////////////////////////////////////////////
//
//	Event Callback
//
static void cbEventCallback( void *privateDesc, unsigned int EventType, unsigned int /*EventData*/, unsigned int /*param*/ )
{
	AppDataStruct *pAppData = (AppDataStruct*)privateDesc;

	if( EventType == MP_MSG_EOS )
	{
		printf("%s(): End of stream. ( privateDesc = 0x%08x )\n", __func__, (int32_t)pAppData );
		if( pAppData )
		{
			NX_MPStop( pAppData->hPlayer );
			NX_MPClose( pAppData->hPlayer );
		
			pAppData->bThreadExit = true;
			exit(1);
		}
	}
	else if( EventType == MP_MSG_DEMUX_ERR )
	{
		printf("%s(): Cannot play contents.\n", __func__);
		if( pAppData )
		{
			NX_MPStop( pAppData->hPlayer );
			NX_MPClose( pAppData->hPlayer );
		
			pAppData->bThreadExit = true;
			exit(1);
		}
	}
}


////////////////////////////////////////////////////////////////////////////////
//
//	HDMI Detect Thread
//
static int32_t GetHdmiStatus( void )
{
	int32_t fd;
	char value;

	if( 0 > (fd = open( "/sys/class/switch/hdmi/state", O_RDONLY )) ) {
		close(fd);
		return -1;
	}

	if( 0 >= read( fd, (char*)&value, 1 ) ) {
		close(fd);
		return -1;
	}

	return atoi(&value);
}

void *HdmiDetectThread( void *arg )
{
	AppDataStruct *pAppData = (AppDataStruct*)arg;
	CMD_MESSAGE cmd;

	struct pollfd fds;
	uint8_t desc[1024];

	uevent_init();
	fds.fd		= uevent_get_fd();
	fds.events	= POLLIN;
	
	while( !pAppData->bThreadExit )
	{
		int32_t err = poll( &fds, 1, 1000 );

		if( err > 0 )
		{ 
			if( fds.revents & POLLIN )
			{
				uevent_next_event((char *)desc, sizeof(desc));
				desc[ sizeof(desc)-1 ] = 0x00;
				
				if( !strcmp((const char *)desc, (const char *)"change@/devices/virtual/switch/hdmi") )
				{
					memset( &cmd, 0x00, sizeof(cmd) );
					cmd.iCmdType = GetHdmiStatus() ? CMD_TYPE_HDMI_INSERT : CMD_TYPE_HDMI_REMOVE;
					pAppData->pCmdQueue->PushCommand( &cmd );
				}
			}
		}
	}

	return (void*)0xDEADDEAD;
}

int32_t StartHdmiDetect( void )
{
	if( AppData.hHdmiThread ) {
		return -1;
	}
	
	if( 0 != pthread_create( &AppData.hHdmiThread, NULL, HdmiDetectThread, (void*)&AppData ) ) {
		printf("Fail, Create Thread.\n");
		return -1;
	}

	return 0;
}

int32_t StopHdmiDetect( void )
{
	if( !AppData.hHdmiThread ) {
		return -1;
	}

	pthread_join( AppData.hHdmiThread, NULL );
	return 0;
}

////////////////////////////////////////////////////////////////////////////////
//
//	Application
//
void print_usage( const char *appName )
{
	printf(" [NxDualDisplayPlayer]\n");
	printf(" Usage: %s [options]\n", appName);
	printf("   -f [filename]    : Input FileName.\n");
	printf("   -a [Audio #]     : Input Audio Number. (I2S : 0 or HDMI : 1)\n");
}

//--------------- CmdThread ----------------------------------
static void *CmdThread( void *arg )
{
	AppDataStruct *pAppData = (AppDataStruct *)arg;
	pAppData->hPlayer = NULL;
	CMD_MESSAGE msg;
	while( !pAppData->bCmdThreadExit )
	{
		int32_t iRet = pAppData->pCmdQueue->PopCommand( &msg );
		usleep(300000);

		if( 0 > iRet )
			continue;

		switch( msg.iCmdType )
		{
			case CMD_TYPE_HDMI_INSERT:
				if(pAppData->hPlayer)
				{
					if( !pAppData->bHdmi )
					{
						printf("Insert HDMI.\n");
#if 0
						NX_MPClearSubDisplay( pAppData->hPlayer, pAppData->iVideoIndex );

						MP_DSP_CONFIG dspConf;
						memset( &dspConf, 0x00, sizeof(dspConf) );
						
						int32_t x, y, width, height;

						dspConf.iPort	= DISPLAY_PORT_HDMI;
						dspConf.iModule	= DISPLAY_MODULE_MLC1;

						GetVideoPosition( dspConf.iPort, 0, &x, &y, &width, &height );

						dspConf.dstRect.iX			= x;
						dspConf.dstRect.iY			= y;
						dspConf.dstRect.iWidth		= width;
						dspConf.dstRect.iHeight		= height;

						NX_DspVideoSetPriority(DISPLAY_MODULE_MLC1, 0);
						NX_MPAddSubDisplay( pAppData->hPlayer, GetTrackIndex(MP_TRACK_VIDEO, 0), &dspConf );
#endif
						pAppData->bHdmi = true;
					}
				}
				break;
			case CMD_TYPE_HDMI_REMOVE:
				if(pAppData->hPlayer)
				{
					if( pAppData->bHdmi )
					{
						printf("Remove HDMI.\n");
#if 0
						NX_MPClearSubDisplay( pAppData->hPlayer, pAppData->iVideoIndex );
#endif
						pAppData->bHdmi = false;
					}
				}
				break;
			default:
				break;
		}
	}		

	return (void*)0xDEADDEAD;
}

static int32_t StartCmdThread( void )
{

	if( AppData.hCmdThread )
	{
		return -1;
	}
	
	if( 0 != pthread_create( &AppData.hCmdThread, NULL, CmdThread, (void*)&AppData ) ) 
	{	
		printf("Fail, Create Thread.\n");
		return -1;
	}

	return 0;
}

static int32_t StopCmdThread( void )
{
	if( !AppData.hCmdThread)
	{
		return -1;
	}

	AppData.bCmdThreadExit = true;

	pthread_join( AppData.hCmdThread, NULL );
	return 0;
}

int32_t main( int32_t argc, char *argv[] )
{
	int32_t iRet = 0, opt = 0;

	int64_t duration = 0, position = 0;

	if( 2 > argc )
	{
		print_usage( argv[0] );
		return 0;
	}

	register_signal();

	printf("############################## Example ##############################\n");
	printf(" Default   ==>  ./NxPlayerConsole -f test_file.mp4\n");
    printf(" Set Audio Path\n");
	printf("  - I2S Audio  ==>  ./NxPlayerConsole -f test_file.mp4 -s 0\n");
    printf("  - HDMI Audio ==>  ./NxPlayerConsole -f test_file.mp4 -s 1\n");
	printf("######################################################################\n");
	memset( &AppData, 0x00, sizeof(AppDataStruct) );

	while( -1 != (opt=getopt(argc, argv, "haf:")) )
	{
		switch( opt )
		{
			case 'h':	print_usage(argv[0]);
				return 0;
			case 'f':	strcpy(AppData.Uri, optarg);
				break;
			case 'a':   AppData.iAudioPath = atoi(argv[4]);
				break;
			default:
				break;
		}
	}	

	AppData.pCmdQueue 	= new NX_CCmdQueue();
	StartCmdThread();
	
	int32_t iVersion		= NX_MPGetVersion();
	int32_t iMajorVersion 	= (iVersion & 0XFF000000) >> 24;
	int32_t iMinorVersion 	= (iVersion & 0x00FF0000) >> 16;
	int32_t iRevisionVersion= (iVersion & 0x0000FF00) >> 8;

	printf("############################## STARTING APPLICATION ##############################\n");
	printf(" Player based Filter                             \n");
	printf(" -. Library Version : %d.%d.%d                   \n", iMajorVersion, iMinorVersion, iRevisionVersion);
	printf(" -. Build Time      : %s, %s                     \n", __TIME__, __DATE__);
	printf(" -. Author          : SW2 Team.                  \n");
	printf("##################################################################################\n");

	NX_MPOpen( &AppData.hPlayer, &cbEventCallback, &AppData );

	if( 0 > (iRet = NX_MPSetUri( AppData.hPlayer, AppData.Uri )) )
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

		NX_MPClose( AppData.hPlayer );
		return -1;
	}
	
	memset( &AppData.MediaInfo, 0x00, sizeof(MP_MEDIA_INFO) );
	NX_MPGetMediaInfo( AppData.hPlayer, &AppData.MediaInfo );
	PrintMediaInfo( &AppData.MediaInfo );

	AppData.iVideoIndex = GetTrackIndex( MP_TRACK_VIDEO, 0 );
	AppData.iAudioIndex = GetTrackIndex( MP_TRACK_AUDIO, 0 );

	if( AppData.MediaInfo.iVideoTrackNum > 0 )
	{
		AppData.bHdmi = GetHdmiStatus();

		MP_DSP_CONFIG dspConf;
		memset( &dspConf, 0x00, sizeof(dspConf) );

		int32_t x, y, width, height;

		// Primary Display
		dspConf.iPort	= DISPLAY_PORT_LCD;	
		dspConf.iModule	= DISPLAY_MODULE_MLC0;

		GetVideoPosition( dspConf.iPort, 0, &x, &y, &width, &height );
		
		dspConf.dstRect.iX			= x;
		dspConf.dstRect.iY			= y;
		dspConf.dstRect.iWidth		= width;
		dspConf.dstRect.iHeight		= height;
		
		NX_DspVideoSetPriority(DISPLAY_MODULE_MLC0, 0);
		NX_MPAddTrack( AppData.hPlayer, AppData.iVideoIndex, &dspConf );

		// Secondary Display
		dspConf.iPort	= DISPLAY_PORT_HDMI;
		dspConf.iModule	= DISPLAY_MODULE_MLC1;

		GetVideoPosition( dspConf.iPort, 0, &x, &y, &width, &height );

		dspConf.dstRect.iX			= x;
		dspConf.dstRect.iY			= y;
		dspConf.dstRect.iWidth		= width;
		dspConf.dstRect.iHeight		= height;

		NX_DspVideoSetPriority(DISPLAY_MODULE_MLC1, 0);
		NX_MPAddSubDisplay( AppData.hPlayer, AppData.iVideoIndex, &dspConf );
	}

	if( AppData.MediaInfo.iAudioTrackNum > 0 )
	{

        if (AppData.iAudioPath == 0)
            NX_MPAddTrack( AppData.hPlayer, AppData.iAudioIndex, NULL );
        else if (AppData.iAudioPath == 1)
            NX_MPAddTrack( AppData.hPlayer, AppData.iAudioIndex, NULL, true );
        else
            NX_MPAddTrack( AppData.hPlayer, AppData.iAudioIndex, NULL );

		AppData.iVolume = 10;
		NX_MPSetVolume( AppData.hPlayer, AppData.iVolume );
	}

	AppData.bThreadExit = false;
	AppData.bPause		= false;

	NX_MPPlay( AppData.hPlayer );
	NX_MPGetDuration( AppData.hPlayer, &duration );
	
	StartHdmiDetect();

	while( !AppData.bThreadExit )
	{
		usleep(1000000);	// 1 sec
		NX_MPGetPosition( AppData.hPlayer, &position );
		printf("Postion : %lld msec / %lld msec    \n", position, duration);
	}

	StopHdmiDetect();

	NX_MPStop( AppData.hPlayer );
	NX_MPClose( AppData.hPlayer );

	StopCmdThread();

	printf("Exit player\n");
	return 0;
}
