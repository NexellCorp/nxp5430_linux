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

#include <stdlib.h>
#include <string.h>

#include "NX_CCmdQueue.h"

//------------------------------------------------------------------------------
NX_CCmdQueue::NX_CCmdQueue()
{
	//m_pSem		= new NX_CSemaphore( MAX_QUEUE_COUNT, 0 );
	m_pQueue	= new NX_CQueue();
}

//------------------------------------------------------------------------------
NX_CCmdQueue::~NX_CCmdQueue()
{
	Deinit();

	//delete m_pSem;
	delete m_pQueue;
}

//------------------------------------------------------------------------------
int32_t NX_CCmdQueue::Init( void )
{
	//m_pSem->Init();
	m_pQueue->Reset();

	return 0;
}

//------------------------------------------------------------------------------
int32_t NX_CCmdQueue::Deinit( void )
{
	//m_pSem->ResetSignal();
	m_pQueue->Reset();

	return 0;
}

//------------------------------------------------------------------------------
int32_t NX_CCmdQueue::PushCommand( CMD_MESSAGE *pMessage )
{
	CMD_MESSAGE *pMsg = (CMD_MESSAGE*)malloc( sizeof( CMD_MESSAGE) );
	memcpy( pMsg, pMessage, sizeof(CMD_MESSAGE) );
	m_pQueue->Push( (void*)pMsg );
	//m_pSem->Post();
	
	return 0;
}

//------------------------------------------------------------------------------
int32_t NX_CCmdQueue::PopCommand( CMD_MESSAGE *pMessage )
{
	// if( 0 > m_pSem->Pend() )
	// 	return -1;

	CMD_MESSAGE *pMsg = NULL;
	if( 0 > m_pQueue->Pop( (void**)&pMsg ) )
		return -1;

	memcpy( pMessage, pMsg, sizeof(CMD_MESSAGE) );
	free( pMsg );

	return 0;
}