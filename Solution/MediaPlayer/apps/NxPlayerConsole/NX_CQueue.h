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

#ifndef __NX_CQUEUE_H__
#define __NX_CQUEUE_H__

class NX_CQueue
{
public:
	NX_CQueue();
	~NX_CQueue();

public:
	int32_t	Push( void *pData );
	int32_t	Pop( void **ppData );
	
	int32_t	IsReady( void );
	void	Reset( void );

private:
	enum { MAX_QUEUE_COUNT = 128 };
	
	void *m_pBuffer[MAX_QUEUE_COUNT];
	int32_t	m_iHead, m_iTail, m_iCount;
	int32_t m_iDepth;
	pthread_mutex_t m_hLock;

private:
	NX_CQueue (const NX_CQueue &Ref);
	NX_CQueue &operator=(const NX_CQueue &Ref);	
};

#endif	// __NX_CQUEUE_H__