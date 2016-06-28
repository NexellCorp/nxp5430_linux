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

#include "NX_CQueue.h"

//------------------------------------------------------------------------------
NX_CQueue::NX_CQueue()
	: m_iHead( 0 )
	, m_iTail( 0 )
	, m_iCount( 0 )
	, m_iDepth( MAX_QUEUE_COUNT )
{
	pthread_mutex_init( &m_hLock, NULL );
}

//------------------------------------------------------------------------------
NX_CQueue::~NX_CQueue()
{
	pthread_mutex_destroy( &m_hLock );
}

//------------------------------------------------------------------------------
int32_t NX_CQueue::Push( void *pData )
{
	pthread_mutex_lock( &m_hLock );

	if( m_iCount >= m_iDepth ) {
		pthread_mutex_unlock( &m_hLock );
		return -1;
	}

	m_pBuffer[m_iTail] = pData;
	m_iTail = (m_iTail+1) % m_iDepth;
	m_iCount++;

	pthread_mutex_unlock( &m_hLock );
	return 0;
}

//------------------------------------------------------------------------------
int32_t NX_CQueue::Pop( void **ppData )
{
	pthread_mutex_lock( &m_hLock );

	if( m_iCount <= 0 ) {
		pthread_mutex_unlock( &m_hLock );
		return -1;
	}

	*ppData = m_pBuffer[m_iHead];
	m_iHead = (m_iHead+1) % m_iDepth;
	m_iCount--;

	pthread_mutex_unlock( &m_hLock );
	return 0;
}

//------------------------------------------------------------------------------
int32_t NX_CQueue::IsReady( void )
{
	pthread_mutex_lock( &m_hLock );
	if( m_iCount > 0 ) {
		pthread_mutex_unlock( &m_hLock );
		return true;
	}
	pthread_mutex_unlock( &m_hLock );
	return false;
}

//------------------------------------------------------------------------------
void NX_CQueue::Reset( void )
{
	pthread_mutex_lock( &m_hLock );
	m_iHead 	= 0;
	m_iTail 	= 0;
	m_iCount 	= 0;
	pthread_mutex_unlock( &m_hLock );
}