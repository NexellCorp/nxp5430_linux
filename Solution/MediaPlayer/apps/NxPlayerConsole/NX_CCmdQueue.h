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

#include <stdint.h>
#include <stdbool.h>
#include <pthread.h>

#include "NX_CQueue.h"
#include "NX_CSemaphore.h"

enum {
	CMD_TYPE_EXIT			= 0x0000,

	CMD_TYPE_PLAY			= 0x1000,
	CMD_TYPE_PAUSE			= 0x1001,
	CMD_TYPE_FORWARD_5SEC	= 0x1002,
	CMD_TYPE_BACKWARD_5SEC	= 0x1003,
	CMD_TYPE_VOLUME_UP		= 0x1004,
	CMD_TYPE_VOLUME_DOWN	= 0x1005,

	CMD_TYPE_HDMI_INSERT	= 0x2000,
	CMD_TYPE_HDMI_REMOVE	= 0x2001,
};

typedef struct _CMD_MESSAGE {
	int32_t 	iCmdType;
	int32_t		iCmdData[255];
	int32_t		iCmdSize;
} CMD_MESSAGE;

class NX_CCmdQueue
{
public:
	NX_CCmdQueue();
	~NX_CCmdQueue();

public:
	int32_t Init( void );
	int32_t Deinit( void );

	int32_t PushCommand( CMD_MESSAGE *pMessage );
	int32_t PopCommand( CMD_MESSAGE *pMesage );

private:
	enum { MAX_QUEUE_COUNT = 128 };

	NX_CSemaphore 	*m_pSem;
	NX_CQueue		*m_pQueue;

private:
	NX_CCmdQueue (const NX_CCmdQueue &Ref);
	NX_CCmdQueue &operator=(const NX_CCmdQueue &Ref);	
};