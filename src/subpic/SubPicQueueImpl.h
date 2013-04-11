/*
 *  $Id: SubPicQueueImpl.h 2786 2010-12-17 16:42:55Z XhmikosR $
 *
 *  (C) 2003-2006 Gabest
 *  (C) 2006-2010 see AUTHORS
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

#pragma once

#include "ISubPic.h"
#include "ISimpleSubPic.h"


class CSubPicQueueImpl : public CUnknown, public ISubPicQueue, public ISimpleSubPicProvider
{
    CAtlList<int> m_prefered_colortype;

	CCritSec m_csSubPicProvider;
	CComPtr<ISubPicProviderEx> m_pSubPicProviderEx;

protected:
	double m_fps;
	REFERENCE_TIME m_rtNow;
	REFERENCE_TIME m_rtNowLast;

	CComPtr<ISubPicExAllocator> m_pAllocator;

	HRESULT RenderTo(ISubPicEx* pSubPic, REFERENCE_TIME rtStart, REFERENCE_TIME rtStop, double fps, BOOL bIsAnimated);

    HRESULT SetSubPicProviderEx(ISubPicProviderEx* pSubPicProviderEx);
    HRESULT GetSubPicProviderEx(ISubPicProviderEx** pSubPicProviderEx);
public:
	CSubPicQueueImpl(ISubPicExAllocator* pAllocator, HRESULT* phr, const int *prefered_colortype=NULL, int prefered_colortype_num=0);
	virtual ~CSubPicQueueImpl();

	DECLARE_IUNKNOWN;
	STDMETHODIMP NonDelegatingQueryInterface(REFIID riid, void** ppv);

	// ISubPicQueue

	STDMETHODIMP SetSubPicProvider(ISubPicProvider* pSubPicProvider);
	STDMETHODIMP GetSubPicProvider(ISubPicProvider** pSubPicProvider);

	STDMETHODIMP SetFPS(double fps);
	STDMETHODIMP SetTime(REFERENCE_TIME rtNow);

    // ISimpleSubPicProvider

    STDMETHODIMP SetSubPicProvider(IUnknown* subpic_provider);
    STDMETHODIMP GetSubPicProvider(IUnknown** subpic_provider);

	STDMETHODIMP Invalidate(REFERENCE_TIME rtInvalidate = -1) = 0;
/*
	STDMETHODIMP_(bool) LookupSubPic(REFERENCE_TIME rtNow, ISubPic** ppSubPic) = 0;

	STDMETHODIMP GetStats(int& nSubPics, REFERENCE_TIME& rtNow, REFERENCE_TIME& rtStart, REFERENCE_TIME& rtStop) = 0;
	STDMETHODIMP GetStats(int nSubPics, REFERENCE_TIME& rtStart, REFERENCE_TIME& rtStop) = 0;
*/
};

class CSubPicQueue : public CSubPicQueueImpl, private CAMThread
{
	int m_nMaxSubPic;
	BOOL m_bDisableAnim;

	CInterfaceList<ISubPic> m_Queue;

	CCritSec m_csQueueLock; // for protecting CInterfaceList<ISubPic>


	POSITION m_subPos;//use to pass to the SubPic provider to produce the next SubPic

	REFERENCE_TIME UpdateQueue();
	void AppendQueue(ISubPic* pSubPic);
	int GetQueueCount();

	REFERENCE_TIME m_rtQueueMin;
	REFERENCE_TIME m_rtQueueMax;
	REFERENCE_TIME m_rtQueueStart, m_rtInvalidate;

	// CAMThread

	bool m_fBreakBuffering;
	enum {EVENT_EXIT, EVENT_TIME, EVENT_COUNT}; // IMPORTANT: _EXIT must come before _TIME if we want to exit fast from the destructor
	HANDLE m_ThreadEvents[EVENT_COUNT];
	enum {QueueEvents_NOTEMPTY, QueueEvents_NOTFULL, QueueEvents_COUNT};
	HANDLE m_QueueEvents[QueueEvents_COUNT];
    DWORD ThreadProc();

public:
	CSubPicQueue(int nMaxSubPic, BOOL bDisableAnim, ISubPicExAllocator* pAllocator, HRESULT* phr);
	virtual ~CSubPicQueue();

	// ISubPicQueue

	STDMETHODIMP SetFPS(double fps);
	STDMETHODIMP SetTime(REFERENCE_TIME rtNow);

	STDMETHODIMP Invalidate(REFERENCE_TIME rtInvalidate = -1);
	STDMETHODIMP_(bool) LookupSubPic(REFERENCE_TIME rtNow, ISubPic** ppSubPic);

	STDMETHODIMP GetStats(int& nSubPics, REFERENCE_TIME& rtNow, REFERENCE_TIME& rtStart, REFERENCE_TIME& rtStop);
	STDMETHODIMP GetStats(int nSubPic, REFERENCE_TIME& rtStart, REFERENCE_TIME& rtStop);

    // ISimpleSubPicProvider
    STDMETHODIMP_(bool) LookupSubPic(REFERENCE_TIME now /*[in]*/, ISimpleSubPic** output_subpic/*[out]*/);
};

class CSubPicQueueNoThread : public CSubPicQueueImpl
{
	CCritSec m_csLock;
	CComPtr<ISubPicEx> m_pSubPic;

    CAtlList<int> m_prefered_colortype;
    
    bool LookupSubPicEx(REFERENCE_TIME rtNow, ISubPicEx** ppSubPic);
public:
	CSubPicQueueNoThread(ISubPicExAllocator* pAllocator, HRESULT* phr, const int *prefered_colortype=NULL, int prefered_colortype_num=0);
	virtual ~CSubPicQueueNoThread();

	// ISubPicQueue
	STDMETHODIMP Invalidate(REFERENCE_TIME rtInvalidate = -1);
	STDMETHODIMP_(bool) LookupSubPic(REFERENCE_TIME rtNow, ISubPic** ppSubPic);
    
	STDMETHODIMP GetStats(int& nSubPics, REFERENCE_TIME& rtNow, REFERENCE_TIME& rtStart, REFERENCE_TIME& rtStop);
	STDMETHODIMP GetStats(int nSubPic, REFERENCE_TIME& rtStart, REFERENCE_TIME& rtStop);

    // ISimpleSubPicProvider
    STDMETHODIMP_(bool) LookupSubPic(REFERENCE_TIME now /*[in]*/, ISimpleSubPic** output_subpic/*[out]*/);
};

