/*
 *	Copyright (C) 2003-2006 Gabest
 *	http://www.gabest.org
 *
 *  This Program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2, or (at your option)
 *  any later version.
 *
 *  This Program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with GNU Make; see the file COPYING.  If not, write to
 *  the Free Software Foundation, 675 Mass Ave, Cambridge, MA 02139, USA.
 *  http://www.gnu.org/copyleft/gpl.html
 *
 */

#include "stdafx.h"
#include "ISubPic.h"
#include "..\DSUtil\DSUtil.h"
#include "CRect2.h"
#include "MemSubPic.h"

#define CSUBPICQUEUE_THREAD_PROC_WAIT_TIMEOUT	20
#define CSUBPICQUEUE_LOOKUP_WAIT_TIMEOUT		40
#define CSUBPICQUEUE_UPDATEQUEUE_WAIT_TIMEOUT	100

//
// ISubPicQueueImpl
//

CSubPicQueueImpl::CSubPicQueueImpl(ISubPicAllocator* pAllocator, HRESULT* phr)
	: CUnknown(NAME("ISubPicQueueImpl"), NULL)
	, m_pAllocator(pAllocator)
	, m_rtNow(0)
	, m_fps(25.0)
{
	if(phr) *phr = S_OK;

	if(!m_pAllocator)
	{
		if(phr) *phr = E_FAIL;
		return;
	}
}

CSubPicQueueImpl::~CSubPicQueueImpl()
{
}

STDMETHODIMP CSubPicQueueImpl::NonDelegatingQueryInterface(REFIID riid, void** ppv)
{
	return
		QI(ISubPicQueue)
		__super::NonDelegatingQueryInterface(riid, ppv);
}

// ISubPicQueue

STDMETHODIMP CSubPicQueueImpl::SetSubPicProvider(ISubPicProvider* pSubPicProvider)
{
	DbgLog((LOG_TRACE, 3, "%s(%d), %s", __FILE__, __LINE__, __FUNCTION__));
//	if(m_pSubPicProvider != pSubPicProvider)
	{
		CAutoLock cAutoLock(&m_csSubPicProvider);
		m_pSubPicProvider = pSubPicProvider;
	}
	Invalidate();

	return S_OK;
}

STDMETHODIMP CSubPicQueueImpl::GetSubPicProvider(ISubPicProvider** pSubPicProvider)
{
	if(!pSubPicProvider)
		return E_POINTER;

	CAutoLock cAutoLock(&m_csSubPicProvider);

	if(m_pSubPicProvider)
		(*pSubPicProvider = m_pSubPicProvider)->AddRef();

	return !!*pSubPicProvider ? S_OK : E_FAIL;
}

STDMETHODIMP CSubPicQueueImpl::SetFPS(double fps)
{
	m_fps = fps;

	return S_OK;
}

STDMETHODIMP CSubPicQueueImpl::SetTime(REFERENCE_TIME rtNow)
{
	m_rtNow = rtNow;

	return S_OK;
}

// private

HRESULT CSubPicQueueImpl::RenderTo(ISubPic* pSubPic, REFERENCE_TIME rtStart, REFERENCE_TIME rtStop, double fps)
{
	HRESULT hr = E_FAIL;

	if(!pSubPic)
		return hr;

	CComPtr<ISubPicProvider> pSubPicProvider;
	if(FAILED(GetSubPicProvider(&pSubPicProvider)) || !pSubPicProvider)
		return hr;

	if(FAILED(pSubPicProvider->Lock()))
		return hr;

	SubPicDesc spd;
	if(SUCCEEDED(pSubPic->Lock(spd)))
	{
		DWORD color = 0xFF000000;
//		if(spd.type == MSP_YUY2 || spd.type == MSP_YV12 || spd.type == MSP_IYUV)
//		{
//			color = 0xFF801080;
//			//color = 0xFF000000;
//		}
//		else if(spd.type == MSP_AYUV)
//		{
//			color = 0xFF108080;
//		}
//		else
//		{
//			color = 0xFF000000;
//		}
		if(SUCCEEDED(pSubPic->ClearDirtyRect(color)))
		{
			CAtlList<CRect> rectList;

			hr = pSubPicProvider->Render(spd, rtStart, fps, rectList);

			DbgLog((LOG_TRACE, 3, "ISubPicQueueImpl::RenderTo => GetStartPosition"));
			POSITION pos = pSubPicProvider->GetStartPosition(rtStart, fps);

			DbgLog((LOG_TRACE, 3, "ISubPicQueueImpl::RenderTo => GetStartStop rtStart:%lu", (ULONG)rtStart/10000));
			pSubPicProvider->GetStartStop(pos, fps, rtStart, rtStop);
			DbgLog((LOG_TRACE, 3, "rtStart:%lu", (ULONG)rtStart/10000));

			//DbgLog((LOG_TRACE, 3, TEXT("rtStart=%lu rtStop=%lu pos:%x fps:%f"), (ULONG)rtStart/10000, (ULONG)rtStop/10000, pos, fps));

			pSubPic->SetStart(rtStart);
			pSubPic->SetStop(rtStop);

			pSubPic->Unlock(&rectList);
		}
	}

	pSubPicProvider->Unlock();

	return hr;
}

//
// CSubPicQueue
//

CSubPicQueue::CSubPicQueue(int nMaxSubPic, ISubPicAllocator* pAllocator, HRESULT* phr)
	: CSubPicQueueImpl(pAllocator, phr)
	, m_nMaxSubPic(nMaxSubPic)
	, m_rtQueueStart(0)
{
	//InitTracer;
	DbgLog((LOG_TRACE, 3, "CSubPicQueue::CSubPicQueue"));
	//Trace(_T("SubPicQueue constructing\n"));

	if(phr && FAILED(*phr))
		return;

	if(m_nMaxSubPic < 1)
		{if(phr) *phr = E_INVALIDARG; return;}

	m_fBreakBuffering = false;
	for(int i = 0; i < EVENT_COUNT; i++)
		m_ThreadEvents[i] = CreateEvent(NULL, FALSE, FALSE, NULL);
	for(int i=0;i<QueueEvents_COUNT; i++)
		m_QueueEvents[i] = CreateEvent(NULL, FALSE, FALSE, NULL);
	if(m_nMaxSubPic > 0)
	{
		SetEvent(m_QueueEvents[QueueEvents_NOTFULL]);
	}


	//m_subPos = NULL;

	CAMThread::Create();
}

CSubPicQueue::~CSubPicQueue()
{
	m_fBreakBuffering = true;
	//SetEvent(m_ThreadEvents[EVENT_EXIT]);
	CAMThread::CallWorker(EVENT_EXIT);
	CAMThread::Close();
	for(int i=0;i<QueueEvents_COUNT; i++)
		CloseHandle(m_QueueEvents[i]);
	for(int i = 0; i < EVENT_COUNT; i++)
		CloseHandle(m_ThreadEvents[i]);
	DbgLog((LOG_TRACE, 3, "CSubPicQueue::~CSubPicQueue"));
	//ReleaseTracer;
}

// ISubPicQueue

STDMETHODIMP CSubPicQueue::SetFPS(double fps)
{
	HRESULT hr = __super::SetFPS(fps);
	if(FAILED(hr)) return hr;

	//SetEvent(m_ThreadEvents[EVENT_TIME]);
	//CallWorker(EVENT_TIME);
	UpdateQueue();

	return S_OK;
}

STDMETHODIMP CSubPicQueue::SetTime(REFERENCE_TIME rtNow)
{
	HRESULT hr = __super::SetTime(rtNow);
	if(FAILED(hr)) return hr;

	//SetEvent(m_ThreadEvents[EVENT_TIME]);
	//DbgLog((LOG_LOCKING, 3, "SetTime::CallWorker(EVENT_TIME)"));
	//CallWorker(EVENT_TIME);
	UpdateQueue();

	return S_OK;
}

STDMETHODIMP CSubPicQueue::Invalidate(REFERENCE_TIME rtInvalidate)
{
	{
//		CAutoLock cQueueLock(&m_csQueueLock);
//		RemoveAll();

		m_rtInvalidate = rtInvalidate;
		m_fBreakBuffering = true;
		UpdateQueue();
	}

	return S_OK;
}

STDMETHODIMP_(bool) CSubPicQueue::LookupSubPic(REFERENCE_TIME rtNow, ISubPic** ppSubPic)
{
	CComPtr<ISubPicProvider> pSubPicProvider;	
	double fps = m_fps;
	*ppSubPic = NULL;
	//if(SUCCEEDED(GetSubPicProvider(&pSubPicProvider)) && pSubPicProvider)
	{
	//	if(pSubPicProvider->GetStartPosition(rtNow, fps))
		{
			CAutoLock cQueueLock(&m_csQueueLock);
			int count = GetCount();
			if(count>0)
			if(WaitForSingleObject(m_QueueEvents[QueueEvents_NOTEMPTY], CSUBPICQUEUE_LOOKUP_WAIT_TIMEOUT)==WAIT_OBJECT_0)
			{
				POSITION pos = GetHeadPosition();
				while(pos)
				{
					CComPtr<ISubPic> pSubPic = GetNext(pos);
					//DbgLog((LOG_TRACE, 3, "picStart:%lu picStop:%lu rtNow:%lu", (ULONG)pSubPic->GetStart()/10000, (ULONG)pSubPic->GetStop()/10000, (ULONG)rtNow/10000));
					if(pSubPic->GetStart() > rtNow)
						break;
					else if(rtNow < pSubPic->GetStop())
					{
						*ppSubPic = pSubPic.Detach();
						break;
					}
				}
				int count = GetCount();
				if(count>0)
					SetEvent(m_QueueEvents[QueueEvents_NOTEMPTY]);
				else//important
					ResetEvent(m_QueueEvents[QueueEvents_NOTEMPTY]);
				if(count<m_nMaxSubPic)
					SetEvent(m_QueueEvents[QueueEvents_NOTFULL]);
				else//important
					ResetEvent(m_QueueEvents[QueueEvents_NOTFULL]);

				//DbgLog((LOG_TRACE, 3, "CSubPicQueue LookupSubPic return"));
			}
		}
	}

	return(!!*ppSubPic);
}

STDMETHODIMP CSubPicQueue::GetStats(int& nSubPics, REFERENCE_TIME& rtNow, REFERENCE_TIME& rtStart, REFERENCE_TIME& rtStop)
{
	CAutoLock cQueueLock(&m_csQueueLock);

	nSubPics = GetCount();
	rtNow = m_rtNow;
	rtStart = m_rtQueueStart;
	rtStop = GetCount() > 0 ? GetTail()->GetStop() : rtStart;

	return S_OK;
}

STDMETHODIMP CSubPicQueue::GetStats(int nSubPic, REFERENCE_TIME& rtStart, REFERENCE_TIME& rtStop)
{
	CAutoLock cQueueLock(&m_csQueueLock);

	rtStart = rtStop = -1;

	if(nSubPic >= 0 && nSubPic < (int)GetCount())
	{
		if(POSITION pos = FindIndex(nSubPic))
		{
			rtStart = GetAt(pos)->GetStart();
			rtStop = GetAt(pos)->GetStop();
		}
	}
	else
	{
		return E_INVALIDARG;
	}

	return S_OK;
}

// private

REFERENCE_TIME CSubPicQueue::UpdateQueue()
{
	//DbgLog((LOG_TRACE, 3, "CSubPicQueue UpdateQueue"));
	REFERENCE_TIME rtNow=0;


	//if(WaitForMultipleObjects(QueueEvents_COUNT, m_QueueEvents, true, CSUBPICQUEUE_UPDATEQUEUE_WAIT_TIMEOUT))
	{
		CAutoLock cQueueLock(&m_csQueueLock);
		rtNow = m_rtNow;
		int count = GetCount();
		if(rtNow < m_rtQueueStart)
		{
			RemoveAll();
			count = 0;
		}
		else
		{
			while(count>0 && rtNow >= GetHead()->GetStop())
			{
				RemoveHead();
				count--;
			}
		}
		if(count>0)
			rtNow = GetTail()->GetStop();
		m_rtQueueStart = m_rtNow;
		if(count>0)
			SetEvent(m_QueueEvents[QueueEvents_NOTEMPTY]);
		else//important
			ResetEvent(m_QueueEvents[QueueEvents_NOTEMPTY]);
		if(count<m_nMaxSubPic)
			SetEvent(m_QueueEvents[QueueEvents_NOTFULL]);
		else//important
			ResetEvent(m_QueueEvents[QueueEvents_NOTFULL]);
	}
	return(rtNow);
}

void CSubPicQueue::AppendQueue(ISubPic* pSubPic)
{
	CAutoLock cQueueLock(&m_csQueueLock);

	AddTail(pSubPic);
}

// overrides

DWORD CSubPicQueue::ThreadProc()
{
	SetThreadPriority(m_hThread, THREAD_PRIORITY_LOWEST/*THREAD_PRIORITY_BELOW_NORMAL*/);

	//Trace(_T("CSubPicQueue Thread Start\n"));
	DbgLog((LOG_TRACE, 3, "CSubPicQueue Thread Start"));
	//while((WaitForMultipleObjects(EVENT_COUNT, m_ThreadEvents, FALSE, INFINITE) - WAIT_OBJECT_0) == EVENT_TIME)
	DWORD request;
	while(true)
	{
		if(CheckRequest(&request))
		{
			if(request==EVENT_EXIT)
				break;

		}
		if(WaitForSingleObject(m_QueueEvents[QueueEvents_NOTFULL], CSUBPICQUEUE_THREAD_PROC_WAIT_TIMEOUT)!=WAIT_OBJECT_0)
			continue;

		bool failed = true;
		bool reachEnd = false;
		CComPtr<ISubPic> pSubPic;
		CComPtr<ISubPic> pStatic;
		CComPtr<ISubPicProvider> pSubPicProvider;

		double fps = m_fps;
		int nMaxSubPic = m_nMaxSubPic;

		m_csQueueLock.Lock();
		REFERENCE_TIME rtNow = 0;
		if(GetCount()>0)
			rtNow = GetTail()->GetStop();
		if(rtNow < m_rtNow)
			rtNow = m_rtNow;
		CComPtr<ISubPicAllocator> pAllocator = m_pAllocator;
		m_csQueueLock.Unlock();

		//for(int i=0;i<1;i++)
		{
			if(FAILED(pAllocator->AllocDynamic(&pSubPic))
				|| (pAllocator->IsDynamicWriteOnly() && FAILED(pAllocator->GetStatic(&pStatic))))
				break;

			if(SUCCEEDED(GetSubPicProvider(&pSubPicProvider)) && pSubPicProvider
				&& SUCCEEDED(pSubPicProvider->Lock()))
			{
				REFERENCE_TIME rtStart, rtStop;

				//DbgLog((LOG_TRACE, 3, "CSubPicQueue::ThreadProc => GetStartPosition"));
				POSITION pos = pSubPicProvider->GetStartPosition(rtNow, fps);

				//DbgLog((LOG_TRACE, 3, "pos:%x, m_fBreakBuffering:%d GetCount():%d nMaxSubPic:%d", pos, m_fBreakBuffering, GetCount(), nMaxSubPic));

				if(pos!=NULL)//!m_fBreakBuffering
				{
					reachEnd = false;
					ASSERT(GetCount() < (size_t)nMaxSubPic);

					//DbgLog((LOG_TRACE, 3, "CSubPicQueue::ThreadProc => GetStartStop"));
					pSubPicProvider->GetStartStop(pos, fps, rtStart, rtStop);

					//DbgLog((LOG_TRACE, 3, "rtStart=%lu rtStop=%lu fps=%f rtNow=%lu m_rtNow=%lu", (ULONG)rtStart/10000, (ULONG)rtStop/10000,
					//	fps, (ULONG)rtNow/10000, (ULONG)m_rtNow/10000));

					if(m_rtNow >= rtStop)
						break;
					//if(rtStart >= m_rtNow + 60*10000000i64) // we are already one minute ahead, this should be enough
					//	break;

					if(rtNow < rtStop)
					{
						if(pAllocator->IsDynamicWriteOnly())
							//if(true)
						{
							HRESULT hr = RenderTo(pStatic, rtStart, rtStop, fps);
							if(FAILED(hr)
								|| S_OK != hr// subpic was probably empty
								|| FAILED(pStatic->CopyTo(pSubPic))
								)
								break;
						}
						else
						{
							HRESULT hr = RenderTo(pSubPic, rtStart, rtStop, fps);
							if(FAILED(hr)
								|| S_OK != hr// subpic was probably empty
								)
								break;
						}
						/*DbgLog((LOG_TRACE, 3, "picStart:%lu picStop:%lu seg:%d idx:%d pos:%x",
							(ULONG)pSubPic->GetStart()/10000,
							(ULONG)pSubPic->GetStop()/10000,
							(int)pos >> 16, (int)pos & ((1<<16)-1)));*/
						failed = false;
					}
				}
				else
					reachEnd = true;
				pSubPicProvider->Unlock();
				pSubPicProvider = NULL;
			}
		}

		m_csQueueLock.Lock();
		if(!failed)
			AddTail(pSubPic);
		int count = GetCount();
		if(count>0)
			SetEvent(m_QueueEvents[QueueEvents_NOTEMPTY]);
		else//important
			ResetEvent(m_QueueEvents[QueueEvents_NOTEMPTY]);
		if(count<m_nMaxSubPic)
			SetEvent(m_QueueEvents[QueueEvents_NOTFULL]);
		else//important
			ResetEvent(m_QueueEvents[QueueEvents_NOTFULL]);
		m_csQueueLock.Unlock();

		if(reachEnd)
		    Sleep(CSUBPICQUEUE_THREAD_PROC_WAIT_TIMEOUT);

		//if(failed)
		//{
		//	if(pSubPicProvider!=NULL)
		//		pSubPicProvider->Unlock();
		//	ReleaseSemaphore(m_semNotFull, 1, NULL);
		//}
		//else
		//	ReleaseSemaphore(m_semNotEmpty, 1, NULL);

		//DbgLog((LOG_LOCKING, 3, "ReleaseMutex m_mtxResetNotFull"));
		//ReleaseMutex(m_mtxResetNotFull);
	}

	//Trace(_T("CSubPicQueue Thread Return\n"));
	DbgLog((LOG_TRACE, 3, "CSubPicQueue Thread Return"));
	Reply(EVENT_EXIT);
	return(0);
}

//
// CSubPicQueueNoThread
//

CSubPicQueueNoThread::CSubPicQueueNoThread(ISubPicAllocator* pAllocator, HRESULT* phr)
	: CSubPicQueueImpl(pAllocator, phr)
{
}

CSubPicQueueNoThread::~CSubPicQueueNoThread()
{
}

// ISubPicQueue

STDMETHODIMP CSubPicQueueNoThread::Invalidate(REFERENCE_TIME rtInvalidate)
{
	CAutoLock cQueueLock(&m_csLock);

	m_pSubPic = NULL;

	return S_OK;
}

STDMETHODIMP_(bool) CSubPicQueueNoThread::LookupSubPic(REFERENCE_TIME rtNow, ISubPic** ppSubPic)
{
	if(!ppSubPic)
		return(false);

	*ppSubPic = NULL;

	CComPtr<ISubPic> pSubPic;

	{
		CAutoLock cAutoLock(&m_csLock);

		if(!m_pSubPic)
		{
			if(FAILED(m_pAllocator->AllocDynamic(&m_pSubPic)))
				return(false);
		}

		pSubPic = m_pSubPic;
	}

	if(pSubPic->GetStart() <= rtNow && rtNow < pSubPic->GetStop())
	{
		(*ppSubPic = pSubPic)->AddRef();
	}
	else
	{
		CComPtr<ISubPicProvider> pSubPicProvider;
		if(SUCCEEDED(GetSubPicProvider(&pSubPicProvider)) && pSubPicProvider
		&& SUCCEEDED(pSubPicProvider->Lock()))
		{
			double fps = m_fps;

			if(POSITION pos = pSubPicProvider->GetStartPosition(rtNow, fps))
			{
				REFERENCE_TIME rtStart = pSubPicProvider->GetStart(pos, fps);
				REFERENCE_TIME rtStop = pSubPicProvider->GetStop(pos, fps);

				if(pSubPicProvider->IsAnimated(pos))
				{
					rtStart = rtNow;
					rtStop = rtNow+1;
				}

				if(rtStart <= rtNow && rtNow < rtStop)
				{
					if(m_pAllocator->IsDynamicWriteOnly())
					{
						CComPtr<ISubPic> pStatic;
						if(SUCCEEDED(m_pAllocator->GetStatic(&pStatic))
						&& SUCCEEDED(RenderTo(pStatic, rtStart, rtStop, fps))
						&& SUCCEEDED(pStatic->CopyTo(pSubPic)))
							(*ppSubPic = pSubPic)->AddRef();
					}
					else
					{
						if(SUCCEEDED(RenderTo(m_pSubPic, rtStart, rtStop, fps)))
							(*ppSubPic = pSubPic)->AddRef();
					}
				}
			}

			pSubPicProvider->Unlock();

			if(*ppSubPic)
			{
				CAutoLock cAutoLock(&m_csLock);

				m_pSubPic = *ppSubPic;
			}
		}
	}

	return(!!*ppSubPic);
}

STDMETHODIMP CSubPicQueueNoThread::GetStats(int& nSubPics, REFERENCE_TIME& rtNow, REFERENCE_TIME& rtStart, REFERENCE_TIME& rtStop)
{
	CAutoLock cAutoLock(&m_csLock);

	nSubPics = 0;
	rtNow = m_rtNow;
	rtStart = rtStop = 0;

	if(m_pSubPic)
	{
		nSubPics = 1;
		rtStart = m_pSubPic->GetStart();
		rtStop = m_pSubPic->GetStop();
	}

	return S_OK;
}

STDMETHODIMP CSubPicQueueNoThread::GetStats(int nSubPic, REFERENCE_TIME& rtStart, REFERENCE_TIME& rtStop)
{
	CAutoLock cAutoLock(&m_csLock);

	if(!m_pSubPic || nSubPic != 0)
		return E_INVALIDARG;

	rtStart = m_pSubPic->GetStart();
	rtStop = m_pSubPic->GetStop();

	return S_OK;
}
