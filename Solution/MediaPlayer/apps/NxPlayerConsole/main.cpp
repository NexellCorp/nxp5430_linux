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

#include <uevent.h>

#include <nx_dsp.h>
#include <NX_MoviePlay.h>

#include "NX_CCmdQueue.h"

#define DISPLAY_WIDTH		1024
#define DISPLAY_HEIGHT		600

// #define DUAL_DISPLAY

typedef struct AppDataStruct {
	MP_HANDLE 		hPlayer;
	MP_MEDIA_INFO	MediaInfo;

	int32_t			bThreadExit;

	NX_CCmdQueue	*pCmdQueue;

	pthread_t		hHdmiThread;
	pthread_t		hConsoleThread;
	struct termios	oldt;

	int32_t			bHdmi;
	int32_t			iVolume;

	int32_t			iVideoIndex;
	int32_t			iAudioIndex;
	char 			Uri[2048];

	int32_t 			bExit;

	// Player Status
	int64_t			iPositionSave;
	int32_t			bPause;
	int32_t			iShellMode;

	//Thread
	pthread_t	hCmdThread;
	int32_t		bCmdThreadExit;
	int32_t		bHdmiThreadExit;
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
			printf("SIGINT..\n");	break;
		case SIGTERM :
			printf("SIGTERM..\n");	break;
		case SIGABRT :
			printf("SIGABRT..\n");	break;
		// case SIGSEGV :
		// 	printf("SIGSEGV..\n");	break;
		default :					break;
	}

	NX_MPStop( AppData.hPlayer );
	NX_MPClose( AppData.hPlayer );
		
	AppData.bThreadExit = true;
	if(AppData.iShellMode)		
	{
		AppData.pCmdQueue->Deinit();
		delete AppData.pCmdQueue;
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
			if(pAppData->iShellMode)		
			{
				pAppData->pCmdQueue->Deinit();
				delete pAppData->pCmdQueue;
			}
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
			if(pAppData->iShellMode)		
			{
				pAppData->pCmdQueue->Deinit();
				delete pAppData->pCmdQueue;
			}
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

////////////////////////////////////////////////////////////////////////////////
//
//	Application
//
void print_usage( const char *appName )
{
	printf(" Usage: %s [options]\n", appName);
	printf("   -f [filename]    : Input FileName.\n");
}

//Shell Mode

static void shell_help( void )
{
	//               1         2         3         4         5         6         7
	//      12345678901234567890123456789012345678901234567890123456789012345678901234567890
	printf("\n");
	printf("================================================================================\n");
	printf("                            Play Control Commands								\n");
	printf("--------------------------------------------------------------------------------\n");
	printf("    open( or o)                            : open 		                        \n");
	printf("    seturi [filename]                      : player for specific file		    \n");
	printf("    addtrack [track no]                    : audio/video track index		    \n");
	printf("    close                                  : close player						\n");
	printf("    info                                   : show file information				\n");
	printf("    play                                   : play								\n");
	printf("    pause                                  : pause playing						\n");
	printf("    stop                                   : stop playing						\n");
	printf("    seek (or s) [milli seconds]            : seek								\n");
	printf("    status (or st)                         : display current player status		\n");
	printf("    p                                      : toggle play & pause				\n");
	printf("--------------------------------------------------------------------------------\n");
	printf("                            Other Control Commands								\n");
	printf("--------------------------------------------------------------------------------\n");
	printf("    pos [module] [port] [x] [y] [w] [h]    : change display position			\n");
	printf("    vol [gain(%%), 0~100]                  : change volume(0:mute)				\n");
	printf("================================================================================\n");
	printf("\n");
}


#define	NX_SHELL_MAX_ARG	16
#define	NX_SHELL_MAX_STR	512
static int GetArgument( char *pSrc, char arg[][NX_SHELL_MAX_STR] )
{
	int	i, j;
	// Reset all arguments
  	for( i=0 ; i<NX_SHELL_MAX_ARG ; i++ ) 
  	{
  		arg[i][0] = 0;
  	}

  	for( i=0 ; i<NX_SHELL_MAX_ARG ; i++ )
	{
		// Remove space char
  		while( *pSrc == ' ' ) 	pSrc++;

		// check end of string.
	  	if( *pSrc == 0 )  		break;

	 	j=0;
		while( (*pSrc != ' ') && (*pSrc != 0) )
		{
		 	arg[i][j] = *pSrc++;
		 	j++;
		 	if( j > (NX_SHELL_MAX_STR-1) ) 	j = NX_SHELL_MAX_STR-1;
	  	}
	  	
	  	arg[i][j] = 0;
  	}

	return i;
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
			case CMD_TYPE_OPEN:
				if(pAppData->hPlayer)
				{
					NX_MPClose( pAppData->hPlayer );
				}
				else
				{
					iRet = NX_MPOpen( &pAppData->hPlayer, &cbEventCallback, pAppData );
					pAppData->bPause = false;
				}
				if(iRet)
					printf("\nError: iRet = %d: NX_MPOpen Error!!!", iRet);
				break;
			case CMD_TYPE_SETURI:			
				if(pAppData->hPlayer)
				{
					if( 0 > (iRet = NX_MPSetUri( pAppData->hPlayer, pAppData->Uri )) )
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
						NX_MPClose( pAppData->hPlayer );
					}
					else
					{
						memset( &pAppData->MediaInfo, 0x00, sizeof(MP_MEDIA_INFO) );
						NX_MPGetMediaInfo( pAppData->hPlayer, &pAppData->MediaInfo );
						PrintMediaInfo( &pAppData->MediaInfo );
					}
				}
				else
				{
					printf("\nMediaPlayer is not opened !!!");
				}
				break;
			case CMD_TYPE_ADDTRACK:
				if(pAppData->hPlayer)
				{
					int32_t iTrackIndex = msg.Param[0];
					int32_t iFindTrackType = -1;
					MP_MEDIA_INFO *pMediaInfo = &pAppData->MediaInfo;

					for( int32_t i = 0; i < pMediaInfo->iProgramNum; i++ )
					{
						if( 0 < pMediaInfo->ProgramInfo[i].iVideoNum )
						{
							for( int32_t j = 0; j < pMediaInfo->ProgramInfo[i].iVideoNum + pMediaInfo->ProgramInfo[i].iAudioNum; j++ )
							{
								MP_TRACK_INFO *pTrackInfo = &pMediaInfo->ProgramInfo[i].TrackInfo[j];
								
								if( MP_TRACK_VIDEO == pTrackInfo->iTrackType ) 
								{
									if(iTrackIndex == pTrackInfo->iTrackIndex)
									{
										iFindTrackType = MP_TRACK_VIDEO;
									}
								}
							}
						}

						if( 0 < pMediaInfo->ProgramInfo[i].iAudioNum )
						{
							for( int32_t j = 0; j < pMediaInfo->ProgramInfo[i].iVideoNum + pMediaInfo->ProgramInfo[i].iAudioNum; j++ )
							{
								MP_TRACK_INFO *pTrackInfo = &pMediaInfo->ProgramInfo[i].TrackInfo[j];

								if( MP_TRACK_AUDIO == pTrackInfo->iTrackType ) 
								{
									if(iTrackIndex == pTrackInfo->iTrackIndex)
									{
										iFindTrackType = MP_TRACK_AUDIO;
									}
								}
							}
						}
					}

					if(iFindTrackType == MP_TRACK_VIDEO)					
					{
						MP_DSP_CONFIG dspConf;
						int32_t x, y, width, height;

						memset( &dspConf, 0x00, sizeof(dspConf) );

						dspConf.iPort				= !pAppData->bHdmi ? DISPLAY_PORT_LCD : DISPLAY_PORT_HDMI;
						dspConf.iModule				= !pAppData->bHdmi ? DISPLAY_MODULE_MLC0 : DISPLAY_MODULE_MLC1;
		
						GetVideoPosition( dspConf.iPort, 0, &x, &y, &width, &height );

						dspConf.dstRect.iX			= x;
						dspConf.dstRect.iY			= y;
						dspConf.dstRect.iWidth		= width;
						dspConf.dstRect.iHeight		= height;

						NX_DspVideoSetPriority(DISPLAY_MODULE_MLC0 , 0);
						NX_MPAddTrack( pAppData->hPlayer, iTrackIndex, &dspConf );
					}
					else if(iFindTrackType == MP_TRACK_AUDIO)					
					{
						NX_MPAddTrack( pAppData->hPlayer, iTrackIndex, NULL, pAppData->bHdmi ? true : false );
					}
					else
					{
						printf("\nError: Not Find Track !!!");			
					}

				}
				else
				{
					printf("\nMediaPlayer is not opened !!!");	
				}
				break;
			case CMD_TYPE_PLAY:
				if(pAppData->hPlayer)
				{
					NX_MPPlay( pAppData->hPlayer );
					pAppData->bPause = false;
				}
				else
				{
					printf("\nMediaPlayer is not opened !!!");	
				}

				break;
			case CMD_TYPE_STOP:
				if(pAppData->hPlayer)
				{
					NX_MPStop( pAppData->hPlayer );
					pAppData->bPause = false;
				}
				else
				{
					printf("\nMediaPlayer is not opened !!!");	
				}
				break;
			case CMD_TYPE_CLOSE:
				if(pAppData->hPlayer)
				{
					NX_MPClose( pAppData->hPlayer );
					pAppData->bPause = false;
					pAppData->hPlayer = NULL;
				}
				else
				{
					printf("\nMediaPlayer is not opened !!!");	
				}
				break;
			case CMD_TYPE_PAUSE:
				if(pAppData->hPlayer)
				{
					NX_MPPause( pAppData->hPlayer );
					pAppData->bPause = true;
				}
				else
				{
					printf("\nMediaPlayer is not opened !!!");	
				}
				break;
			case CMD_TYPE_P:
				if(pAppData->hPlayer)
				{
					if(pAppData->bPause == true)
					{
						NX_MPPlay( pAppData->hPlayer );
						pAppData->bPause = false;
					}
					else
					{
						NX_MPPause( pAppData->hPlayer );
						pAppData->bPause = true;
					}
				}
				else
				{
					printf("\nMediaPlayer is not opened !!!");	
				}
				break;
			case CMD_TYPE_SEEK:
				if(pAppData->hPlayer)
				{
					int64_t iSeekTime = 0;
					iSeekTime = (int64_t)msg.Param[0];
					NX_MPSeek( pAppData->hPlayer, iSeekTime );
				}
				else
				{
					printf("\nMediaPlayer is not opened !!!");	
				}
				break;
			case CMD_TYPE_VOLUME:
				if(pAppData->hPlayer)
				{
					int32_t iVolume = 0;
					iVolume = msg.Param[0];
					NX_MPSetVolume( pAppData->hPlayer, iVolume );
				}
				else
				{
					printf("\nMediaPlayer is not opened !!!");	
				}
				break;
			case CMD_TYPE_MEDIA_INFO:
				if(pAppData->hPlayer)
				{
					MP_MEDIA_INFO mMediaInfo;
					memset(&mMediaInfo, 0, sizeof(MP_MEDIA_INFO));
					NX_MPGetMediaInfo( pAppData->hPlayer, &mMediaInfo );
					PrintMediaInfo( &mMediaInfo );
				}
				else
				{
					printf("\nMediaPlayer is not opened !!!");	
				}
				break;
			case CMD_TYPE_STATUS:
				if(pAppData->hPlayer)
				{
					int64_t iDuration = 0;
					int64_t iPosition = 0;
					NX_MPGetDuration( pAppData->hPlayer, &iDuration );
					NX_MPGetPosition( pAppData->hPlayer, &iPosition );
					printf("Postion : %lld msec / %lld msec    \n", iPosition, iDuration);
				}
				else
				{
					printf("\nMediaPlayer is not opened !!!");	
				}
				break;
			case CMD_TYPE_DISPLAY_POSITION:
				if(pAppData->hPlayer)
				{
					MP_DSP_RECT dsp_rect;
					memset( &dsp_rect, 0x00, sizeof(dsp_rect) );

//					mDspRect.iPort 			= msg.Param[0];	
//					mDspRect.iModule 		= msg.Param[1];
					dsp_rect.iX 	= msg.Param[2];
					dsp_rect.iY 	= msg.Param[3];
					dsp_rect.iWidth = msg.Param[4];
					dsp_rect.iHeight = msg.Param[5];

					int32_t iVideoIndex = GetTrackIndex( MP_TRACK_VIDEO, 0 );

					iRet =	NX_MPSetDspPosition( pAppData->hPlayer, iVideoIndex, &dsp_rect );
					if(MP_ERR_NONE != iRet)
						printf("\nError: NX_MPSetDspPosition !!!");			
				}
				else
				{
					printf("\nMediaPlayer is not opened !!!");	
				}
				break;
			case CMD_TYPE_EXIT:
					printf("~~~ Bye !!!");	
					exit(1);
				break;
			case CMD_TYPE_HDMI_INSERT:
				if(pAppData->hPlayer)
				{
					if( !pAppData->bHdmi )
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
						NX_MPAddSubDisplay( pAppData->hPlayer, GetTrackIndex(MP_TRACK_VIDEO, 0), &dspConf );
#else
						NX_MPGetPosition( pAppData->hPlayer, &pAppData->iPositionSave );
						NX_MPStop( pAppData->hPlayer );
						NX_MPClearTrack( pAppData->hPlayer );
						NX_MPClose( pAppData->hPlayer );

						NX_MPOpen( &pAppData->hPlayer, &cbEventCallback, &AppData );
						NX_MPSetUri( pAppData->hPlayer, pAppData->Uri );
						NX_MPGetMediaInfo( pAppData->hPlayer, &pAppData->MediaInfo );

						if( pAppData->MediaInfo.iVideoTrackNum > 0 )
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
							NX_MPAddTrack( pAppData->hPlayer, pAppData->iVideoIndex, &dspConf );
						}

						if( pAppData->MediaInfo.iAudioTrackNum > 0 )
						{
							NX_MPAddTrack( pAppData->hPlayer, pAppData->iAudioIndex, NULL, true);
							NX_MPSetVolume( pAppData->hPlayer, pAppData->iVolume );
						}

						NX_MPPlay( pAppData->hPlayer );
						NX_MPSeek( pAppData->hPlayer, pAppData->iPositionSave );
#endif					
						pAppData->bHdmi = true;
					}
				}
				else
				{
					printf("\nMediaPlayer is not opened !!!");	
				}
				break;
			case CMD_TYPE_HDMI_REMOVE:
				if(pAppData->hPlayer)
				{
					if( pAppData->bHdmi )
					{
						printf("Remove HDMI.\n");
#ifdef DUAL_DISPLAY
						NX_MPClearSubDisplay( pAppData->hPlayer, pAppData->iVideoIndex );
#else
						NX_MPGetPosition( pAppData->hPlayer, &pAppData->iPositionSave );
						NX_MPStop( pAppData->hPlayer );
						NX_MPClearTrack( pAppData->hPlayer );
						NX_MPClose( pAppData->hPlayer );

						NX_MPOpen( &pAppData->hPlayer, &cbEventCallback, &AppData );
						NX_MPSetUri( pAppData->hPlayer, pAppData->Uri );
						NX_MPGetMediaInfo( pAppData->hPlayer, &pAppData->MediaInfo );

						if( pAppData->MediaInfo.iVideoTrackNum > 0 )
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
							NX_MPAddTrack( pAppData->hPlayer, pAppData->iVideoIndex, &dspConf );
						}

						if( pAppData->MediaInfo.iAudioTrackNum > 0 )
						{
							NX_MPAddTrack( pAppData->hPlayer, pAppData->iAudioIndex, NULL );
							NX_MPSetVolume( pAppData->hPlayer, pAppData->iVolume );
						}

						NX_MPPlay( pAppData->hPlayer );
						NX_MPSeek( pAppData->hPlayer, pAppData->iPositionSave );
#endif					
						pAppData->bHdmi = false;
					}
				}
				else
				{
					printf("\nMediaPlayer is not opened !!!");	
				}
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

static void shell_main( void )
{
	int cmdcnt;
	static char cmdStr[1024];
	static char cmd[16][512];

	AppData.pCmdQueue 	= new NX_CCmdQueue();

	CMD_MESSAGE CmdM;

	shell_help();
	StartCmdThread();

	while(!AppData.bExit)
	{
 		printf(" cmd > ");
 		gets(cmdStr);

 		cmdcnt = GetArgument( cmdStr, cmd );
 		memset( &CmdM, 0x00, sizeof(CmdM) );

 		if( (0 == strcasecmp( cmd[0], "open" )) | (0 == strcasecmp( cmd[0], "o" )) ) 
 		{
 			printf("[Cmd] %s.\n",cmd[0]);
  			CmdM.iCmdType = CMD_TYPE_OPEN;
  			CmdM.iParamLen = 0; 
 			AppData.pCmdQueue->PushCommand( &CmdM );
 		}
 		else if ( !strcasecmp( cmd[0], "seturi" ) )
 		{
 			printf("[Cmd] %s.\n",cmd[0]);
 			if(cmdcnt < 2)
 			{
// 				printf(" ~~~ No Uri, Input Uri !!!\n");
 //				continue;
 				CmdM.iCmdType = CMD_TYPE_SETURI;
 				CmdM.iParamLen = 0; 
 				AppData.pCmdQueue->PushCommand( &CmdM );
 			}
 			else
 			{
 				CmdM.iCmdType = CMD_TYPE_SETURI;
 				strcpy(AppData.Uri, cmd[1]);
 				CmdM.iParamLen = 1; 
 				AppData.pCmdQueue->PushCommand( &CmdM );
 			}
 		}
 		else if ( !strcasecmp( cmd[0], "close" ) )
 		{
 			printf("[Cmd] %s.\n",cmd[0]);
 			CmdM.iCmdType = CMD_TYPE_CLOSE;
 			CmdM.iParamLen = 0; 
 			AppData.pCmdQueue->PushCommand( &CmdM );
 		}
 		else if ( !strcasecmp( cmd[0], "info" ) )
 		{
 			printf("[Cmd] %s.\n",cmd[0]);
 			CmdM.iCmdType = CMD_TYPE_MEDIA_INFO;
 			CmdM.iParamLen = 0; 
 			AppData.pCmdQueue->PushCommand( &CmdM );
 		}
 		else if ( !strcasecmp( cmd[0], "play" ) )
 		{
 			printf("[Cmd] %s.\n",cmd[0]);
 			CmdM.iCmdType = CMD_TYPE_PLAY;
 			CmdM.iParamLen = 0; 
 			AppData.pCmdQueue->PushCommand( &CmdM );
 		}
 		else if ( !strcasecmp( cmd[0], "pause" ) )
 		{
 			printf("[Cmd] %s.\n",cmd[0]);
 			CmdM.iCmdType = CMD_TYPE_PAUSE;
 			CmdM.iParamLen = 0; 
 			AppData.pCmdQueue->PushCommand( &CmdM );
 		}
 		else if ( !strcasecmp( cmd[0], "p" ) )
 		{
 			printf("[Cmd] %s.\n",cmd[0]);
 			CmdM.iCmdType = CMD_TYPE_P;
 			CmdM.iParamLen = 0; 
 			AppData.pCmdQueue->PushCommand( &CmdM );
 		}
 		else if ( !strcasecmp( cmd[0], "stop" ) )
 		{
 			printf("[Cmd] %s.\n",cmd[0]);
 			CmdM.iCmdType = CMD_TYPE_STOP;
 			CmdM.iParamLen = 0; 
 			AppData.pCmdQueue->PushCommand( &CmdM );
 		}
 		else if( (0 == strcasecmp( cmd[0], "seek" )) || (0 == strcasecmp( cmd[0], "s" )) )
 		{
 			printf("[Cmd] %s.\n",cmd[0]);
 			if(cmdcnt < 2)
 			{
 				printf(" ~~~ Input Seek Value !!!\n");
 				continue;
 			}
 			CmdM.iCmdType = CMD_TYPE_SEEK;
 			CmdM.Param[0] = atoi( cmd[1] );
			CmdM.iParamLen = 1; 
 			AppData.pCmdQueue->PushCommand( &CmdM );
 		}
 		else if( (0 == strcasecmp( cmd[0], "status" )) || (0 == strcasecmp( cmd[0], "st" )) )
 		{
 			printf("[Cmd] %s.\n",cmd[0]);
 			CmdM.iCmdType = CMD_TYPE_STATUS;
 			CmdM.iParamLen = 0; 
 			AppData.pCmdQueue->PushCommand( &CmdM );
 		}
 		else if ( !strcasecmp( cmd[0], "pos" ) )
 		{
 			printf("[Cmd] %s.\n",cmd[0]);
			if( cmdcnt < 7) 
			{
				printf("\nError : Invalid argument, Usage : pos [module] [port] [x] [y] [w] [h]\n");
				continue;
			}

 			CmdM.iCmdType = CMD_TYPE_DISPLAY_POSITION;
 			CmdM.Param[0] = atoi(cmd[1]);		// module
			CmdM.Param[1] = atoi(cmd[2]);		// port
			CmdM.Param[2] = atoi(cmd[3]);		// x
			CmdM.Param[3] = atoi(cmd[4]);		// y
			CmdM.Param[4] = atoi(cmd[5]);		// w
			CmdM.Param[5] = atoi(cmd[6]);		// h
			CmdM.iParamLen = 6; 
 			AppData.pCmdQueue->PushCommand( &CmdM );
 		}
 		else if ( !strcasecmp( cmd[0], "vol" ) )
 		{
 			printf("[Cmd] %s.\n",cmd[0]);
 			if(cmdcnt < 2)
 			{
 				printf(" ~~~ Input Volume Value !!!\n");
 				continue;
 			}
 			CmdM.iCmdType = CMD_TYPE_VOLUME;
 			CmdM.Param[0] = atoi( cmd[1] );
 			if( (0 > CmdM.Param[0]) || (100 < CmdM.Param[0]) )
 			{
 				printf("Error:  100 =< Volume  > 0, Volume = %d\n",CmdM.Param[0]);
 			}
			CmdM.iParamLen = 1; 
 			AppData.pCmdQueue->PushCommand( &CmdM );
 		}
 		else if ( !strcasecmp( cmd[0], "addtrack" ) )
 		{
 			printf("[Cmd] %s.\n",cmd[0]);
 			if(cmdcnt < 2)
 			{
 				printf(" ~~~ Input AddTrack Number !!!\n");
 				continue;
 			}
 			CmdM.iCmdType = CMD_TYPE_ADDTRACK;
 			CmdM.Param[0] = atoi( cmd[1] );
			CmdM.iParamLen = 1; 
 			AppData.pCmdQueue->PushCommand( &CmdM );
 		}
 		else if ( !strcasecmp( cmd[0], "h") || !strcasecmp( cmd[0], "?") || !strcasecmp( cmd[0], "help") )
 		{
 			shell_help();
 		}
 		else if ( !strcasecmp( cmd[0], "Q" ) )
 		{
 			CmdM.iCmdType = CMD_TYPE_EXIT;
 			AppData.pCmdQueue->PushCommand( &CmdM );
 		}
 		else
 		{
 			if(cmdcnt > 0 )
 			{
 				printf("unknown command : %s\n", cmd[0]);
 			}
 		}
	}

	printf("Dmr_Shell End\n");

	StopCmdThread();
}
//

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
	printf(" ShellMode ==>  ./NxPlayerConsole -f test_file.mp4 -s\n");
	printf("######################################################################\n");
	memset( &AppData, 0x00, sizeof(AppDataStruct) );

	while( -1 != (opt=getopt(argc, argv, "hsf:")) )
	{
		switch( opt )
		{
			case 'h':	print_usage(argv[0]);	return 0;
			case 'f':	strcpy(AppData.Uri, optarg);	break;
			case 's':	AppData.iShellMode = 1;			break;
			default:							break;
		}
	}	

	if( (argc >1) && (1 == AppData.iShellMode) )
	{
		shell_main();
		return 0;
	}
	
//	AppData.pCmdQueue = new NX_CCmdQueue();

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
		NX_MPAddTrack( AppData.hPlayer, AppData.iVideoIndex, &dspConf );

		if( AppData.bHdmi )
		{
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
#else
		MP_DSP_CONFIG dspConf;
		int32_t x, y, width, height;

		memset( &dspConf, 0x00, sizeof(dspConf) );

		dspConf.iPort				= !AppData.bHdmi ? DISPLAY_PORT_LCD : DISPLAY_PORT_HDMI;
		dspConf.iModule				= !AppData.bHdmi ? DISPLAY_MODULE_MLC0 : DISPLAY_MODULE_MLC1;
		
		GetVideoPosition( dspConf.iPort, 0, &x, &y, &width, &height );

		dspConf.dstRect.iX			= x;
		dspConf.dstRect.iY			= y;
		dspConf.dstRect.iWidth		= width;
		dspConf.dstRect.iHeight		= height;

		NX_DspVideoSetPriority(DISPLAY_MODULE_MLC0 , 0);
		NX_MPAddTrack( AppData.hPlayer, AppData.iVideoIndex, &dspConf );
#endif
	}

	if( AppData.MediaInfo.iAudioTrackNum > 0 )
	{
#ifdef DUAL_DISPLAY
		NX_MPAddTrack( AppData.hPlayer, AppData.iAudioIndex, NULL );
#else
		NX_MPAddTrack( AppData.hPlayer, AppData.iAudioIndex, NULL, AppData.bHdmi ? true : false );
#endif

		AppData.iVolume = 10;
		NX_MPSetVolume( AppData.hPlayer, AppData.iVolume );
	}

	AppData.bThreadExit= false;
	AppData.bPause		= false;

	NX_MPPlay( AppData.hPlayer );
	NX_MPGetDuration( AppData.hPlayer, &duration );
	
	StartHdmiDetect();

	while( !AppData.bThreadExit )
	{
		usleep(300000);
		NX_MPGetPosition( AppData.hPlayer, &position );
		printf("Postion : %lld msec / %lld msec    \n", position, duration);
	}

	StopHdmiDetect();

	NX_MPStop( AppData.hPlayer );
	NX_MPClose( AppData.hPlayer );

	return 0;
}
