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
	int32_t			bPause;

	NX_CCmdQueue	*pCmd;

	pthread_t		hHdmiThread;
	pthread_t		hConsoleThread;
	struct termios	oldt;

	int32_t			bHdmi;
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
		// case SIGSEGV :
		// 	printf("SIGSEGV..\n");	break;
		default :					break;
	}

	if( SIGSEGV == sig ) {
		tcsetattr( STDIN_FILENO, TCSANOW, &gAppData.oldt );
	}
	else {
		tcsetattr( STDIN_FILENO, TCSANOW, &gAppData.oldt );

		NX_MPStop( gAppData.hPlayer );
		NX_MPClose( gAppData.hPlayer );
		
		gAppData.bThreadExit = true;
		gAppData.pCmd->Deinit();
		delete gAppData.pCmd;
	}

	exit( EXIT_FAILURE );
}

static void register_signal(void)
{
	signal( SIGINT, signal_handler );
	signal( SIGTERM, signal_handler );
	signal( SIGABRT, signal_handler );
	// signal( SIGSEGV, signal_handler );
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

	if( dspPort == DISPLAY_PORT_LCD )
	{
#ifdef ANDROID
	scrWidth	= DISPLAY_WIDTH;
	scrHeight	= DISPLAY_HEIGHT;
#else
		GetScreenResolution( &scrWidth, &scrHeight );
#endif		
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
			pAppData->bThreadExit = true;
			pAppData->pCmd->Deinit();
		}
	}
	else if( EventType == MP_MSG_DEMUX_ERR )
	{
		printf("%s(): Cannot play contents.\n", __func__);
		if( pAppData )
		{
			pAppData->bThreadExit = true;
			pAppData->pCmd->Deinit();
		}
	}
}


////////////////////////////////////////////////////////////////////////////////
//
//	HDMI Detect Thread
//
#ifdef ANDROID
#else
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
	AppData *pAppData = (AppData*)arg;
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
					pAppData->pCmd->PushCommand( &cmd );
				}
			}
		}
		else if( err == 0 )
		{
			//printf("poll() timeout!\n");
		}
		else if( err == -1 )
		{
			//printf("poll() error!\n");
		}
	}

	return (void*)0xDEADDEAD;
}

int32_t StartHdmiDetect( void )
{
	if( gAppData.hHdmiThread ) {
		return -1;
	}
	
	if( 0 != pthread_create( &gAppData.hHdmiThread, NULL, HdmiDetectThread, (void*)&gAppData ) ) {
		printf("Fail, Create Thread.\n");
		return -1;
	}

	return 0;
}

int32_t StopHdmiDetect( void )
{
	if( !gAppData.hHdmiThread ) {
		return -1;
	}

	pthread_join( gAppData.hHdmiThread, NULL );
	return 0;
}
#endif

static int kbhit(void)
{
	struct termios oldt, newt;
	int ch;
	tcgetattr( STDIN_FILENO, &oldt );
	newt = oldt;
	newt.c_lflag &= ~( ICANON | ECHO );
	tcsetattr( STDIN_FILENO, TCSANOW, &newt );
	ch = getchar();
	tcsetattr( STDIN_FILENO, TCSANOW, &oldt );
	return ch;
}

void *ConsoleThread( void *arg )
{
	AppData *pAppData = (AppData*)arg;
	tcgetattr( STDIN_FILENO, &pAppData->oldt );

	CMD_MESSAGE cmd;

	while( !pAppData->bThreadExit )
	{
		int32_t ch = kbhit();
		memset( &cmd, 0x00, sizeof(cmd) );

		switch( ch )
		{
		case '6':
			cmd.iCmdType = CMD_TYPE_FORWARD_5SEC;
			pAppData->pCmd->PushCommand( &cmd );
			break;

		case '4':
			cmd.iCmdType = CMD_TYPE_BACKWARD_5SEC;
			pAppData->pCmd->PushCommand( &cmd );
			break;

		case '8':
			cmd.iCmdType = CMD_TYPE_VOLUME_UP;
			pAppData->pCmd->PushCommand( &cmd );
			break;

		case '2':
			cmd.iCmdType = CMD_TYPE_VOLUME_DOWN;
			pAppData->pCmd->PushCommand( &cmd );
			break;
		
		case 'p':
			if( pAppData->bPause ) {
				pAppData->bPause = false;
				cmd.iCmdType = CMD_TYPE_PLAY;
			}
			else {
				pAppData->bPause = true;
				cmd.iCmdType = CMD_TYPE_PAUSE;
			}
			pAppData->pCmd->PushCommand( &cmd );
			break;
		
		case 'q':
			gAppData.bThreadExit = true;
			cmd.iCmdType = CMD_TYPE_EXIT;
			pAppData->pCmd->PushCommand( &cmd );
			break;
		}
	}

	return (void*)0xDEADDEAD;
}

int32_t StartConsole( void )
{
	if( gAppData.hConsoleThread ) {
		return -1;
	}

	if( 0 != pthread_create( &gAppData.hConsoleThread, NULL, ConsoleThread, (void*)&gAppData) ) {
		printf("Fail, Create Thread.\n");
		return -1;
	}

	return 0;
}

int32_t StopConsole( void )
{
	if( !gAppData.hConsoleThread ) {
		return -1;
	}

	pthread_join( gAppData.hConsoleThread, NULL );
	return 0;
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
	static char uri[2048];

#ifdef ANDROID
	sp<SurfaceComposerClient> spSurfaceComposerClient;	
	sp<SurfaceControl> spSurfaceControl;
#endif

	int64_t duration = 0, position = 0;

	if( 2 > argc )
	{
		print_usage( argv[0] );
		return 0;
	}

	register_signal();

	while( -1 != (opt=getopt(argc, argv, "hf:")) )
	{
		switch( opt )
		{
			case 'h':	print_usage(argv[0]);	return 0;
			case 'f':	strcpy(uri, optarg);	break;
			default:							break;
		}
	}	
	
	memset( &gAppData, 0x00, sizeof(AppData) );
	gAppData.pCmd = new NX_CCmdQueue();

	int32_t iVersion		= NX_MPGetVersion();
	int32_t iMajorVersion 	= (iVersion & 0XFF000000) >> 24;
	int32_t iMinorVersion 	= (iVersion & 0x00FF0000) >> 16;
	int32_t iRevisionVersion= (iVersion & 0x0000FF00) >> 8;

	printf("############################## STARTING APPLICATION ##############################\n");
	printf(" Player based Filter                             \n");
	printf(" -. Library Version : %d.%d.%d                   \n", iMajorVersion, iMinorVersion, iRevisionVersion);
	printf(" -. Build Time      : %s, %s                     \n", __TIME__, __DATE__);
	printf(" -. Author          : SW3 Team.                  \n");
	printf("##################################################################################\n");

	NX_MPOpen( &gAppData.hPlayer, &cbEventCallback, &gAppData );

	if( 0 > (iRet = NX_MPSetUri( gAppData.hPlayer, uri )) )
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
#ifdef ANDROID
		spSurfaceComposerClient = new SurfaceComposerClient();
		spSurfaceControl = spSurfaceComposerClient->createSurface( String8("YUV Surface"), DISPLAY_WIDTH, DISPLAY_HEIGHT, HAL_PIXEL_FORMAT_YV12, ISurfaceComposerClient::eFXSurfaceNormal );
		if( spSurfaceControl == NULL )
		{
			printf("Fail, Create YUV Surface.\n");
			exit( -1 );
		}

		int32_t x, y, width, height;
		GetVideoPosition( DISPLAY_PORT_LCD, 0, &x, &y, &width, &height );
		spSurfaceControl->setPosition( x, y );
		spSurfaceControl->setSize( width, height );

		SurfaceComposerClient::openGlobalTransaction();
		spSurfaceControl->show();
		spSurfaceControl->setLayer( 99999 );
		SurfaceComposerClient::closeGlobalTransaction();

		sp<ANativeWindow> spNativeWindow( spSurfaceControl->getSurface() );
		NX_MPAddTrack( gAppData.hPlayer, gAppData.iVideoIndex, spNativeWindow.get(), NULL );
#else
		gAppData.bHdmi = GetHdmiStatus();

#ifdef DUAL_DISPLAY
		MP_DSP_CONFIG dspConf;
		memset( &dspConf, 0x00, sizeof(dspConf) );

		int32_t x, y, width, height;

		dspConf.iPort	= DISPLAY_PORT_LCD;	
		dspConf.iModule	= DISPLAY_MODULE_MLC0;

		GetVideoPosition( dspConf.iPort, 0, &x, &y, &width, &height );
		
		dspConf.dstRect.iX			= x;
		dspConf.dstRect.iY			= y;
		dspConf.dstRect.iWidth		= width;
		dspConf.dstRect.iHeight		= height;
		
		NX_DspVideoSetPriority(DISPLAY_MODULE_MLC0, 0);
		NX_MPAddTrack( gAppData.hPlayer, gAppData.iVideoIndex, &dspConf );

		if( gAppData.bHdmi )
		{
			dspConf.iPort	= DISPLAY_PORT_HDMI;
			dspConf.iModule	= DISPLAY_MODULE_MLC1;

			GetVideoPosition( dspConf.iPort, 0, &x, &y, &width, &height );

			dspConf.dstRect.iX			= x;
			dspConf.dstRect.iY			= y;
			dspConf.dstRect.iWidth		= width;
			dspConf.dstRect.iHeight		= height;

			NX_DspVideoSetPriority(DISPLAY_MODULE_MLC1, 0);
			NX_MPAddSubDisplay( gAppData.hPlayer, gAppData.iVideoIndex, &dspConf );
		}
#else
		MP_DSP_CONFIG dspConf;
		int32_t x, y, width, height;

		memset( &dspConf, 0x00, sizeof(dspConf) );

		dspConf.iPort				= !gAppData.bHdmi ? DISPLAY_PORT_LCD : DISPLAY_PORT_HDMI;
		dspConf.iModule				= !gAppData.bHdmi ? DISPLAY_MODULE_MLC0 : DISPLAY_MODULE_MLC1;
		
		GetVideoPosition( dspConf.iPort, 0, &x, &y, &width, &height );

		dspConf.dstRect.iX			= x;
		dspConf.dstRect.iY			= y;
		dspConf.dstRect.iWidth		= width;
		dspConf.dstRect.iHeight		= height;

		NX_DspVideoSetPriority(DISPLAY_MODULE_MLC0 , 0);
		NX_MPAddTrack( gAppData.hPlayer, gAppData.iVideoIndex, &dspConf );
#endif
#endif
	}

	if( gAppData.MediaInfo.iAudioTrackNum > 0 )
	{
#ifdef ANDROID
		NX_MPAddTrack( gAppData.hPlayer, gAppData.iAudioIndex, NULL, NULL );
#else	
#ifdef DUAL_DISPLAY
		NX_MPAddTrack( gAppData.hPlayer, gAppData.iAudioIndex, NULL );
#else
		NX_MPAddTrack( gAppData.hPlayer, gAppData.iAudioIndex, NULL, gAppData.bHdmi ? true : false );
#endif

		gAppData.iVolume = 10;
		NX_MPSetVolume( gAppData.hPlayer, gAppData.iVolume );
#endif
	}

	gAppData.bThreadExit= false;
	gAppData.bPause		= false;
	
	NX_MPPlay( gAppData.hPlayer );
	NX_MPGetDuration( gAppData.hPlayer, &duration );
	
	NX_CCmdQueue *pCmd = gAppData.pCmd;
	CMD_MESSAGE msg;

	StartConsole();
#ifdef ANDROID
#else
	StartHdmiDetect();
#endif
	
	while( !gAppData.bThreadExit )
	{
		int32_t ret = pCmd->PopCommand( &msg );
		usleep(300000);

		NX_MPGetPosition( gAppData.hPlayer, &position );
		fprintf(stdout, "Postion : %lld msec / %lld msec    \n", position, duration);
		fflush(stdout);

		if( 0 > ret )
			continue;

		switch( msg.iCmdType )
		{
			case CMD_TYPE_EXIT:
				printf("\nBye Bye~\n");
				break;
			
			case CMD_TYPE_PLAY:
				printf("Play.\n");
				NX_MPPlay( gAppData.hPlayer );
				break;

			case CMD_TYPE_PAUSE:
				printf("Pause.\n");
				NX_MPPause( gAppData.hPlayer );
				break;

			case CMD_TYPE_FORWARD_5SEC:
				NX_MPGetPosition( gAppData.hPlayer, &position );
				position += 5000;
				if( position > duration)
					position = duration;

				printf("Seek Position : %lld msec\n", position);
				NX_MPSeek( gAppData.hPlayer, position );
				break;
			
			case CMD_TYPE_BACKWARD_5SEC:
				NX_MPGetPosition( gAppData.hPlayer, &position );
				position -= 5000;
				if( position < 0 )
					position = 0;

				printf("Seek Position : %lld msec\n", position);
				NX_MPSeek( gAppData.hPlayer, position );
				break;

#ifdef ANDROID
#else
			case CMD_TYPE_VOLUME_UP:
				gAppData.iVolume += 5;
				if( gAppData.iVolume > 99 )
					gAppData.iVolume = 99;

				printf("Change Volume. ( %d )\n", gAppData.iVolume);
				NX_MPSetVolume( gAppData.hPlayer, gAppData.iVolume );
				break;

			case CMD_TYPE_VOLUME_DOWN:
				gAppData.iVolume -= 5;
				if( gAppData.iVolume <= 0 )
					gAppData.iVolume = 0;

				printf("Change Volume. ( %d )\n", gAppData.iVolume);
				NX_MPSetVolume( gAppData.hPlayer, gAppData.iVolume );
				break;

			case CMD_TYPE_HDMI_INSERT:
				if( !gAppData.bHdmi )
				{
					printf("Insert HDMI.\n");

#ifdef DUAL_DISPLAY
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
					NX_MPAddSubDisplay( gAppData.hPlayer, GetTrackIndex(MP_TRACK_VIDEO, 0), &dspConf );
#else
					NX_MPGetPosition( gAppData.hPlayer, &position );
					NX_MPStop( gAppData.hPlayer );
					NX_MPClearTrack( gAppData.hPlayer );
					NX_MPClose( gAppData.hPlayer );

					NX_MPOpen( &gAppData.hPlayer, &cbEventCallback, &gAppData );
					NX_MPSetUri( gAppData.hPlayer, uri );
					NX_MPGetMediaInfo( gAppData.hPlayer, &gAppData.MediaInfo );

					if( gAppData.MediaInfo.iVideoTrackNum > 0 )
					{
						MP_DSP_CONFIG dspConf;
						memset( &dspConf, 0x00, sizeof(dspConf) );

						dspConf.iPort				= DISPLAY_PORT_HDMI;
						dspConf.iModule				= DISPLAY_MODULE_MLC1;
						
						int32_t x, y, width, height;
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
						NX_MPAddTrack( gAppData.hPlayer, gAppData.iAudioIndex, NULL, true);
						NX_MPSetVolume( gAppData.hPlayer, gAppData.iVolume );
					}

					NX_MPPlay( gAppData.hPlayer );
					NX_MPGetDuration( gAppData.hPlayer, &duration );
					NX_MPSeek( gAppData.hPlayer, position );
#endif					
					gAppData.bHdmi = true;
				}
				break;

			case CMD_TYPE_HDMI_REMOVE:
				if( gAppData.bHdmi )
				{
					printf("Remove HDMI.\n");
#ifdef DUAL_DISPLAY
					NX_MPClearSubDisplay( gAppData.hPlayer, gAppData.iVideoIndex );
#else
					NX_MPGetPosition( gAppData.hPlayer, &position );
					NX_MPStop( gAppData.hPlayer );
					NX_MPClearTrack( gAppData.hPlayer );
					NX_MPClose( gAppData.hPlayer );

					NX_MPOpen( &gAppData.hPlayer, &cbEventCallback, &gAppData );
					NX_MPSetUri( gAppData.hPlayer, uri );
					NX_MPGetMediaInfo( gAppData.hPlayer, &gAppData.MediaInfo );

					if( gAppData.MediaInfo.iVideoTrackNum > 0 )
					{
						MP_DSP_CONFIG dspConf;
						memset( &dspConf, 0x00, sizeof(dspConf) );

						dspConf.iPort				= DISPLAY_PORT_LCD;
						dspConf.iModule				= DISPLAY_MODULE_MLC0;
						
						int32_t x, y, width, height;
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
						NX_MPAddTrack( gAppData.hPlayer, gAppData.iAudioIndex, NULL );
						NX_MPSetVolume( gAppData.hPlayer, gAppData.iVolume );
					}

					NX_MPPlay( gAppData.hPlayer );
					NX_MPGetDuration( gAppData.hPlayer, &duration );
					NX_MPSeek( gAppData.hPlayer, position );
#endif					
					gAppData.bHdmi = false;
				}
				break;
#endif
			default:
				break;
		}
	}

	StopConsole();

#ifdef ANDROID
#else
	StopHdmiDetect();
#endif

	NX_MPStop( gAppData.hPlayer );
	NX_MPClose( gAppData.hPlayer );

	delete gAppData.pCmd;

	return 0;
}
