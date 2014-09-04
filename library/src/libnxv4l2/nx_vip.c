//------------------------------------------------------------------------------
//
//  Copyright (C) 2013 Nexell Co. All Rights Reserved
//  Nexell Co. Proprietary & Confidential
//
//  NEXELL INFORMS THAT THIS CODE AND INFORMATION IS PROVIDED "AS IS" BASE
//  AND WITHOUT WARRANTY OF ANY KIND, EITHER EXPRESSED OR IMPLIED, INCLUDING
//  BUT NOT LIMITED TO THE IMPLIED WARRANTIES OF MERCHANTABILITY AND/OR FITNESS
//  FOR A PARTICULAR PURPOSE.
//
//  Module      :
//  File        :
//  Description :
//  Author      : 
//  Export      :
//  History     :
//
//------------------------------------------------------------------------------

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <sys/time.h>

#include <linux/media.h>
#include <linux/v4l2-subdev.h>
#include <linux/v4l2-mediabus.h>
#include <linux/videodev2.h>
#include <linux/videodev2_nxp_media.h>

#include <nx_alloc_mem.h>
#include <nxp-v4l2-media.h>

#include "nx_vip.h"

#define V4L2_CID_CAMERA_MODE_CHANGE     (V4L2_CTRL_CLASS_CAMERA | 0x1003)


typedef struct VIP_HANDLE_INFO	VIP_HANDLE_INFO;

struct VIP_HANDLE_INFO {
	V4L2_PRIVATE_HANDLE	hPrivate;		//  private handle
	
	//	Setting Values
	VIP_INFO			vipInfo;		//	Input Information
	int32_t				mode;			//	same as Video Input Type' mode
	int32_t				cliperId;		//	
	int32_t				sensorId;		//	
	int32_t				decimatorId;	//	

	int32_t				numPlane;		//	Input Image's Plane Number

	//	Buffer Control Informatons
	NX_VID_MEMORY_INFO *pMgmtMem[VIP_MAX_BUF_SIZE];
	int32_t				curQueuedSize;

	bool				streamOnFlag;	//	on/off flag
	pthread_mutex_t		hMutex;
};

//#define DISPLAY_FPS

#ifdef DISPLAY_FPS
unsigned long long int GetSystemTime(void)
{
	struct timeval tv;
	gettimeofday( &tv, NULL );
	return ((unsigned long long int)tv.tv_sec)*1000 + tv.tv_usec / 1000;
}
#endif

VIP_HANDLE NX_VipInit( VIP_INFO *pVipInfo )
{
	VIP_HANDLE hVip = NULL;
	V4L2_PRIVATE_HANDLE hPrivate = NULL;

	int32_t cliperId, sensorId, mipiId;
	int32_t width, height, format;
	int32_t cropX, cropY, cropWidth, cropHeight;

	struct V4l2UsageScheme s;
	memset(&s, 0, sizeof(s));

	// Configuration
	if( pVipInfo->port == VIP_PORT_0 )
	{
		s.useClipper0 = true;
		s.useDecimator0 = true;

		cliperId = nxp_v4l2_clipper0;
		sensorId = nxp_v4l2_sensor0;
	}
	else if( pVipInfo->port == VIP_PORT_1 )
	{
		s.useClipper1 = true;
		s.useDecimator1 = true;

		cliperId = nxp_v4l2_clipper1;
		sensorId = nxp_v4l2_sensor1;
	}
	else if( pVipInfo->port == VIP_PORT_MIPI )
	{
		s.useClipper1 = true;
		s.useDecimator1 = true;

		cliperId = nxp_v4l2_clipper1;
		sensorId = nxp_v4l2_sensor1;
		mipiId	 = nxp_v4l2_mipicsi;
	}
	else
	{
		printf("Invalid port number = %d\n", pVipInfo->port );
		return NULL;
	}


	if( !(hPrivate = v4l2_init(&s)) )
	{
		printf("Error : v4l2_init() failed!!!\n");
		return NULL;
	}

	//v4l2_set_format(hPrivate, nxp_v4l2_clipper1, width, height, V4L2_PIX_FMT_YUV420M);
	//v4l2_set_crop(hPrivate, nxp_v4l2_clipper1, 0, 0, width, height);
	//v4l2_set_format(hPrivate, nxp_v4l2_sensor1, width, height, V4L2_MBUS_FMT_YUYV8_2X8);
	//v4l2_set_format(hPrivate, nxp_v4l2_mipicsi, width, height, V4L2_MBUS_FMT_YUYV8_2X8);
	//v4l2_reqbuf(hPrivate, nxp_v4l2_clipper1, VIP_MAX_BUF_SIZE);

	width = pVipInfo->width;
	height = pVipInfo->height;

	if( pVipInfo->port != VIP_PORT_MIPI )
		v4l2_set_format(hPrivate, sensorId, width, height, PIXCODE_YUV420_PLANAR);
	else {
		v4l2_set_format(hPrivate, nxp_v4l2_sensor1, width, height, V4L2_MBUS_FMT_YUYV8_2X8);
		v4l2_set_format(hPrivate, nxp_v4l2_mipicsi, width, height, V4L2_MBUS_FMT_YUYV8_2X8);
	}

	if (width > 1024 || height > 768)
		v4l2_set_ctrl(hPrivate, sensorId, V4L2_CID_CAMERA_MODE_CHANGE, 1);
	else
		v4l2_set_ctrl(hPrivate, sensorId, V4L2_CID_CAMERA_MODE_CHANGE, 0);

	if( pVipInfo->mode == VIP_MODE_CLIPPER ) {		// Cliper

		if( pVipInfo->numPlane == 3 )
			v4l2_set_format(hPrivate, cliperId, width, height, PIXFORMAT_YUV420_PLANAR);
		else
			v4l2_set_format(hPrivate, cliperId, width, height, PIXFORMAT_YUV420_YV12);
		v4l2_set_crop(hPrivate, cliperId, pVipInfo->cropX, pVipInfo->cropY, pVipInfo->cropWidth, pVipInfo->cropHeight);
		v4l2_reqbuf(hPrivate, cliperId, VIP_MAX_BUF_SIZE);
	}
	else {		// Decimator (Not support yet..)
		printf("Decimator not implementation yet.\n");
		return NULL;
	}

	hVip = (VIP_HANDLE)malloc(sizeof(VIP_HANDLE_INFO));
	memset( hVip, 0, sizeof(VIP_HANDLE_INFO) );
	memcpy( &hVip->vipInfo, pVipInfo, sizeof(hVip->vipInfo) );

	hVip->hPrivate	= hPrivate;
	hVip->mode		= pVipInfo->mode;
	hVip->numPlane  = pVipInfo->numPlane;
	hVip->cliperId	= cliperId;
	hVip->sensorId	= sensorId;

	pthread_mutex_init( &hVip->hMutex, NULL );

	return hVip;
}

void NX_VipClose( VIP_HANDLE hVip )
{
	if( hVip )
	{
		if( hVip->streamOnFlag )
		{
			v4l2_streamoff( hVip->hPrivate, hVip->cliperId );
			hVip->streamOnFlag = 0;
		}
		v4l2_exit(hVip->hPrivate);

		if( hVip )
			free( hVip );
	}
}

int32_t NX_VipStreamControl( VIP_HANDLE hVip, bool enable )
{
	printf("[%s] Not Implemetation..\n", __func__);
	return 0;
	
	if( hVip )
	{
		if( 1 == enable ) {
			if( 0 == hVip->streamOnFlag ) {
				hVip->streamOnFlag = 1;
				v4l2_streamon( hVip->hPrivate, hVip->cliperId );
			} else {
				printf("Already Stream On..\n");
			}
		}  else {
			if( 1 == hVip->streamOnFlag ) {
				hVip->streamOnFlag = 0;
				v4l2_streamoff( hVip->hPrivate, hVip->cliperId );
			} else {
				printf("Already Stream Off..\n");
			}
		}
	} else {
		printf("hVip is NULL..\n");
	}
}

int32_t NX_VipQueueBuffer( VIP_HANDLE hVip, NX_VID_MEMORY_INFO *pInfo )
{
	NX_VID_MEMORY_INFO *pMemSlot;
	int32_t slotIndex, i;

	pthread_mutex_lock( &hVip->hMutex );
	if( hVip->curQueuedSize >= VIP_MAX_BUF_SIZE ) {
		pthread_mutex_unlock( &hVip->hMutex );
		return -1;
	}

	//	Find Empty Slot & index
	for( i = 0; i < VIP_MAX_BUF_SIZE; i++ ) {
		if( hVip->pMgmtMem[i] == NULL ) {
			slotIndex = i;
			hVip->pMgmtMem[i] = pInfo;
			break;
		}
	}

	if( i == VIP_MAX_BUF_SIZE ) {
		printf("Have no empty slot\n");
		pthread_mutex_unlock( &hVip->hMutex );
		return -1;
	}
	hVip->curQueuedSize ++;

	pthread_mutex_unlock( &hVip->hMutex );

	if( 1 == hVip->mode )		//	Clipper Only
	{
		struct nxp_vid_buffer vipBuffer;
		if( hVip->numPlane == 1 )
		{
			NX_MEMORY_INFO *vidMem;
			for( i = 0; i<hVip->numPlane ; i++ ) {
				vidMem				= (NX_MEMORY_INFO *)pInfo->privateDesc[i];
				vipBuffer.fds[i]	= (int)vidMem->privateDesc;
				vipBuffer.virt[i]	= (char*)vidMem->virAddr;
				vipBuffer.phys[i]	= vidMem->phyAddr;
				vipBuffer.sizes[i]	= vidMem->size;
			}
		}
		else
		{
			NX_MEMORY_INFO *vidMem;
			for( i = 0; i < 3; i++ ) {
				vidMem				= (NX_MEMORY_INFO *)pInfo->privateDesc[i];
				vipBuffer.fds[i]	= (int)vidMem->privateDesc;
				vipBuffer.virt[i]	= (char*)vidMem->virAddr;
				vipBuffer.phys[i]	= vidMem->phyAddr;
				vipBuffer.sizes[i]	= vidMem->size;
			}
		}
		v4l2_qbuf( hVip->hPrivate, hVip->cliperId , 3, slotIndex, &vipBuffer, -1, NULL);
	}
	else  {
		//	Not Support Yet
	}

	if( !hVip->streamOnFlag )
	{
		hVip->streamOnFlag = 1;
		v4l2_streamon( hVip->hPrivate, hVip->cliperId );
	}

	return 0;
}


int32_t NX_VipDequeueBuffer( VIP_HANDLE hVip, NX_VID_MEMORY_INFO **ppInfo, long long *timeStamp )
{
	int32_t ret = 0;
	int32_t caputreIdx;

	pthread_mutex_lock( &hVip->hMutex );
	if( hVip->curQueuedSize < 2 )
	{
		pthread_mutex_unlock( &hVip->hMutex );
		return -1;
	}
	pthread_mutex_unlock( &hVip->hMutex );
	
	if( 1 == hVip->mode )
	{
		v4l2_dqbuf(hVip->hPrivate, hVip->cliperId, 3, &caputreIdx, NULL);
		v4l2_get_timestamp(hVip->hPrivate, hVip->cliperId, timeStamp);
		*ppInfo = hVip->pMgmtMem[caputreIdx];
		hVip->pMgmtMem[caputreIdx] = NULL;
		if( *ppInfo == NULL )
		{
			printf("Error : buffering problem!!!!\n");
			ret = -1;
		}
	}
	else
	{
		//	Not Support Yet
	}

	pthread_mutex_lock( &hVip->hMutex );
	hVip->curQueuedSize--;
	pthread_mutex_unlock( &hVip->hMutex );

#ifdef DISPLAY_FPS
	{
		static uint64_t startTime = 0, endTime = 0;
		static uint32_t captureCnt = 0;

		endTime = GetSystemTime();
		captureCnt++;
		if( (endTime - startTime) >= 1000) {
			printf("instance [ %p ] frame [ %03.1f fps ]\n", hVip, (double)(captureCnt / ((endTime - startTime) / 1000.) ));
			startTime = endTime;
			captureCnt = 0;
		}
	}
#endif

	return ret;
}


int32_t NX_VipGetCurrentBufCount( VIP_HANDLE hVip, int32_t *maxSize )
{
	int32_t ret;
	pthread_mutex_lock( &hVip->hMutex );
	*maxSize = VIP_MAX_BUF_SIZE;
	ret = hVip->curQueuedSize;
	pthread_mutex_unlock( &hVip->hMutex );
	return ret;
}

