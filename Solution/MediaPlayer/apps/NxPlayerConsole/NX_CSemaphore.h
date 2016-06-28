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

#ifndef __NX_CSEMAPHORE_H__
#define __NX_CSEMAPHORE_H__

class NX_CSemaphore
{
public:
	NX_CSemaphore( int32_t iMax, int32_t iInit );
	~NX_CSemaphore();

public:
	void	Init( void );
	void	ResetSignal( void );

	int32_t	Post( void );
	int32_t	Pend( void );

private:
	enum {	MAX_SEM_VALUE = 1024	};

	pthread_cond_t  m_hCond;
	pthread_mutex_t m_hLock;
	int32_t			m_iValue;
	int32_t			m_iMax;
	int32_t			m_iInit;
	int32_t			m_bReset;

private:
	NX_CSemaphore (const NX_CSemaphore &Ref);
	NX_CSemaphore &operator=(const NX_CSemaphore &Ref);		
};

#endif	// __NX_CSEMAPHORE_H__