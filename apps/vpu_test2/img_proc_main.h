#ifndef __IMG_PROC_MAIN_H__
#define	__IMG_PROC_MAIN_H__

#include <pthread.h>

class CImageProcessing
{
public:
	CImageProcessing();
	~CImageProcessing();

	bool Start( const char *inFileName );
	bool Stop();

	//
	//	Threads
	//
private:
	//	Decoding Thread
	static void *DecThreadStub( void *arg ){
		CImageProcessing *pObj = (CImageProcessing *)arg;
		pObj->DecThread();
		return (void*)0xDEADDEAD;
	}
	void DecThread();

	//	Image Processing Thread
	static void *ImgProcThreadStub( void *arg ){
		CImageProcessing *pObj = (CImageProcessing *)arg;
		pObj->ImgProcThread();
		return (void*)0xDEADDEAD;
	}
	void ImgProcThread();


	//	Private Variables
private:
	CMediaReader	*m_pMediaReader;		//	Stream Reader
	int32_t			m_InWidth, m_InHeight;	//	Input Stream's Resolution	
	
	NX_VID_DEC_HANDLE m_hDec;

	pthread_t		m_hDecThread;			//	Decode Loop Thread Handler
	pthread_t		m_hImgProcThread;		//	Image Processing Loop Thread Handler

	bool			m_bExitDecThread;
	bool			m_bExitImgProcThread;

	NX_QUEUE		m_tDecRelQueue;
	NX_QUEUE		m_tImgInQueue;
	NX_QUEUE		m_tImgOutQueue;

	NX_SEMAPHORE	*m_pSemDec;
	NX_SEMAPHORE	*m_pSemImgProc;

	pthread_mutex_t	m_hCtrlMutex;

	bool			m_bStarted;

	uint8_t			m_StreamBuffer[4*1024*1024];
	uint8_t			m_SeqData[1024*4];

};

#endif	//	__IMG_PROC_MAIN_H__