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

#include "NX_CSemaphore.h"

//------------------------------------------------------------------------------
NX_CSemaphore::NX_CSemaphore( int32_t iMax, int32_t iInit )
	: m_iValue	( iInit )
	, m_iMax	( iMax )
	, m_iInit	( iInit )
	, m_bReset	( false )
{
	pthread_cond_init ( &m_hCond,  NULL );
	pthread_mutex_init( &m_hLock, NULL );
}

//------------------------------------------------------------------------------
NX_CSemaphore::~NX_CSemaphore()
{
	ResetSignal();
	pthread_cond_destroy( &m_hCond );
	pthread_mutex_destroy( &m_hLock );
}

//------------------------------------------------------------------------------
void NX_CSemaphore::Init( void )
{
	pthread_mutex_lock( &m_hLock );
	m_iValue = m_iInit;
	m_bReset = false;
	pthread_mutex_unlock( &m_hLock );
}

//------------------------------------------------------------------------------
void NX_CSemaphore::ResetSignal( void )
{
	pthread_mutex_lock( &m_hLock );
	m_bReset = true;
	for( int32_t i = 0; i < m_iMax; i++ )
		pthread_cond_signal( &m_hCond );
	pthread_mutex_unlock( &m_hLock );
}

//------------------------------------------------------------------------------
int32_t NX_CSemaphore::Post( void )
{
	int32_t iRet = 0;
	pthread_mutex_lock( &m_hLock );
	if( m_iValue >= m_iMax ) {
		pthread_mutex_unlock( &m_hLock );
		return -1;
	}

	m_iValue++;
	pthread_cond_signal ( &m_hCond );
	if( m_bReset || m_iValue <= 0 ) {
		iRet = -1;
	}
	pthread_mutex_unlock( &m_hLock );
	return iRet;
}

//------------------------------------------------------------------------------
int32_t NX_CSemaphore::Pend( void )
{
	int32_t iRet = 0;
	pthread_mutex_lock( &m_hLock );
	if( m_iValue == 0 && !m_bReset ) {
		iRet = pthread_cond_wait( &m_hCond, &m_hLock );
		m_iValue--;
	}
	else if( m_iValue < 0 || m_bReset ) {
		iRet = -1;
	}
	else {
		m_iValue--;
		iRet = 0;
	}
	pthread_mutex_unlock( &m_hLock );
	return iRet;
}