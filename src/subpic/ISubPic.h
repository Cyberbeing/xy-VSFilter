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

#pragma once

#include <atlbase.h>
#include <atlcoll.h>
#include "CoordGeom.h"

#pragma pack(push, 1)
struct SubPicDesc
{
	int type;
	int w, h, bpp, pitch, pitchUV;
	void* bits;
	BYTE* bitsU;
	BYTE* bitsV;
	RECT vidrect; // video rectangle

	struct SubPicDesc() {type = 0; w = h = bpp = pitch = pitchUV = 0; bits = NULL; bitsU = bitsV = NULL;}
};
#pragma pack(pop)

//
// ISubPic
//

[uuid("449E11F3-52D1-4a27-AA61-E2733AC92CC0")]
interface ISubPic : public IUnknown
{
	STDMETHOD_(void*, GetObject) () const PURE;

	STDMETHOD_(REFERENCE_TIME, GetStart) () const PURE;
	STDMETHOD_(REFERENCE_TIME, GetStop) () const PURE;
	STDMETHOD_(void, SetStart) (REFERENCE_TIME rtStart) PURE;
	STDMETHOD_(void, SetStop) (REFERENCE_TIME rtStop) PURE;

	STDMETHOD (GetDesc) (SubPicDesc& spd /*[out]*/) const PURE;
	STDMETHOD (CopyTo) (ISubPic* pSubPic /*[in]*/) PURE;

	STDMETHOD (ClearDirtyRect) (DWORD color /*[in]*/) PURE;
	STDMETHOD (GetDirtyRect) (RECT* pDirtyRect /*[out]*/) const PURE;
	STDMETHOD (GetDirtyRects) (CAtlList<const CRect>& dirtyRectList /*[out]*/) const PURE;
	STDMETHOD (SetDirtyRect) (CAtlList<CRect>* dirtyRectList /*[in]*/) PURE;

	STDMETHOD (GetMaxSize) (SIZE* pMaxSize /*[out]*/) const PURE;
	STDMETHOD (SetSize) (SIZE pSize /*[in]*/, RECT vidrect /*[in]*/) PURE;

	STDMETHOD (Lock) (SubPicDesc& spd /*[out]*/) PURE;
	STDMETHOD (Unlock) (CAtlList<CRect>* dirtyRectList /*[in]*/) PURE;

	STDMETHOD (AlphaBlt) (const RECT* pSrc, const RECT* pDst, SubPicDesc* pTarget = NULL /*[in]*/) PURE;
};

//
// ISubPicAllocator
//

[uuid("CF7C3C23-6392-4a42-9E72-0736CFF793CB")]
interface ISubPicAllocator : public IUnknown
{
	STDMETHOD (SetCurSize) (SIZE size /*[in]*/) PURE;
	STDMETHOD (SetCurVidRect) (RECT curvidrect) PURE;

	STDMETHOD (GetStatic) (ISubPic** ppSubPic /*[out]*/) PURE;
	STDMETHOD (AllocDynamic) (ISubPic** ppSubPic /*[out]*/) PURE;

	STDMETHOD_(bool, IsDynamicWriteOnly) () PURE;

	STDMETHOD (ChangeDevice) (IUnknown* pDev) PURE;
};

//
// ISubPicProvider
//

[uuid("D62B9A1A-879A-42db-AB04-88AA8F243CFD")]
interface ISubPicProvider : public IUnknown
{
	STDMETHOD (Lock) () PURE;
	STDMETHOD (Unlock) () PURE;

	STDMETHOD_(POSITION, GetStartPosition) (REFERENCE_TIME rt, double fps) PURE;
	STDMETHOD_(POSITION, GetNext) (POSITION pos) PURE;

	STDMETHOD_(REFERENCE_TIME, GetStart) (POSITION pos, double fps) PURE;
	STDMETHOD_(REFERENCE_TIME, GetStop) (POSITION pos, double fps) PURE;
	STDMETHOD_(VOID, GetStartStop) (POSITION pos, double fps, /*out*/REFERENCE_TIME& start, /*out*/REFERENCE_TIME& stop) PURE;

	STDMETHOD_(bool, IsAnimated) (POSITION pos) PURE;

	STDMETHOD (Render) (SubPicDesc& spd, REFERENCE_TIME rt, double fps, RECT& bbox) PURE;
	STDMETHOD (Render) (SubPicDesc& spd, REFERENCE_TIME rt, double fps, CAtlList<CRect>& rectList) PURE;
};

class CSubPicProviderImpl : public CUnknown, public ISubPicProvider
{
protected:
	CCritSec* m_pLock;

public:
	CSubPicProviderImpl(CCritSec* pLock);
	virtual ~CSubPicProviderImpl();

	DECLARE_IUNKNOWN;
	STDMETHODIMP NonDelegatingQueryInterface(REFIID riid, void** ppv);

	// ISubPicProvider

	STDMETHODIMP Lock();
	STDMETHODIMP Unlock();

	STDMETHODIMP_(POSITION) GetStartPosition(REFERENCE_TIME rt, double fps) = 0;
	STDMETHODIMP_(POSITION) GetNext(POSITION pos) = 0;

	STDMETHODIMP_(REFERENCE_TIME) GetStart(POSITION pos, double fps) = 0;
	STDMETHODIMP_(REFERENCE_TIME) GetStop(POSITION pos, double fps) = 0;
	STDMETHODIMP_(VOID) GetStartStop(POSITION pos, double fps, /*out*/REFERENCE_TIME& start, /*out*/REFERENCE_TIME& stop);

	STDMETHODIMP Render(SubPicDesc& spd, REFERENCE_TIME rt, double fps, RECT& bbox) = 0;
	STDMETHODIMP Render(SubPicDesc& spd, REFERENCE_TIME rt, double fps, CAtlList<CRect>& rectList);
};

//
// ISubPicQueue
//

[uuid("C8334466-CD1E-4ad1-9D2D-8EE8519BD180")]
interface ISubPicQueue : public IUnknown
{
	STDMETHOD (SetSubPicProvider) (ISubPicProvider* pSubPicProvider /*[in]*/) PURE;
	STDMETHOD (GetSubPicProvider) (ISubPicProvider** pSubPicProvider /*[out]*/) PURE;

	STDMETHOD (SetFPS) (double fps /*[in]*/) PURE;
	STDMETHOD (SetTime) (REFERENCE_TIME rtNow /*[in]*/) PURE;

	STDMETHOD (Invalidate) (REFERENCE_TIME rtInvalidate = -1) PURE;
	STDMETHOD_(bool, LookupSubPic) (REFERENCE_TIME rtNow /*[in]*/, ISubPic** ppSubPic /*[out]*/) PURE;

	STDMETHOD (GetStats) (int& nSubPics, REFERENCE_TIME& rtNow, REFERENCE_TIME& rtStart, REFERENCE_TIME& rtStop /*[out]*/) PURE;
	STDMETHOD (GetStats) (int nSubPic /*[in]*/, REFERENCE_TIME& rtStart, REFERENCE_TIME& rtStop /*[out]*/) PURE;
};

class CSubPicQueueImpl : public CUnknown, public ISubPicQueue
{
	CCritSec m_csSubPicProvider;
	CComPtr<ISubPicProvider> m_pSubPicProvider;

protected:
	double m_fps;
	REFERENCE_TIME m_rtNow;

	CComPtr<ISubPicAllocator> m_pAllocator;

	HRESULT RenderTo(ISubPic* pSubPic, REFERENCE_TIME rtStart, REFERENCE_TIME rtStop, double fps);

public:
	CSubPicQueueImpl(ISubPicAllocator* pAllocator, HRESULT* phr);
	virtual ~CSubPicQueueImpl();

	DECLARE_IUNKNOWN;
	STDMETHODIMP NonDelegatingQueryInterface(REFIID riid, void** ppv);

	// ISubPicQueue

	STDMETHODIMP SetSubPicProvider(ISubPicProvider* pSubPicProvider);
	STDMETHODIMP GetSubPicProvider(ISubPicProvider** pSubPicProvider);

	STDMETHODIMP SetFPS(double fps);
	STDMETHODIMP SetTime(REFERENCE_TIME rtNow);
/*
	STDMETHODIMP Invalidate(REFERENCE_TIME rtInvalidate = -1) = 0;
	STDMETHODIMP_(bool) LookupSubPic(REFERENCE_TIME rtNow, ISubPic** ppSubPic) = 0;

	STDMETHODIMP GetStats(int& nSubPics, REFERENCE_TIME& rtNow, REFERENCE_TIME& rtStart, REFERENCE_TIME& rtStop) = 0;
	STDMETHODIMP GetStats(int nSubPics, REFERENCE_TIME& rtStart, REFERENCE_TIME& rtStop) = 0;
*/
};

class CSubPicQueue : public CSubPicQueueImpl, private CInterfaceList<ISubPic>, private CAMThread
{
	int m_nMaxSubPic;

	CCritSec m_csQueueLock; // for protecting CInterfaceList<ISubPic>


	POSITION m_subPos;//use to pass to the SubPic provider to produce the next SubPic

	REFERENCE_TIME UpdateQueue();
	void AppendQueue(ISubPic* pSubPic);

	REFERENCE_TIME m_rtQueueStart, m_rtInvalidate;

	// CAMThread

	bool m_fBreakBuffering;
	enum {EVENT_EXIT, EVENT_TIME, EVENT_COUNT}; // IMPORTANT: _EXIT must come before _TIME if we want to exit fast from the destructor
	HANDLE m_ThreadEvents[EVENT_COUNT];
	enum {QueueEvents_NOTEMPTY, QueueEvents_NOTFULL, QueueEvents_COUNT};
	HANDLE m_QueueEvents[QueueEvents_COUNT];
    DWORD ThreadProc();

public:
	CSubPicQueue(int nMaxSubPic, ISubPicAllocator* pAllocator, HRESULT* phr);
	virtual ~CSubPicQueue();

	// ISubPicQueue

	STDMETHODIMP SetFPS(double fps);
	STDMETHODIMP SetTime(REFERENCE_TIME rtNow);

	STDMETHODIMP Invalidate(REFERENCE_TIME rtInvalidate = -1);
	STDMETHODIMP_(bool) LookupSubPic(REFERENCE_TIME rtNow, ISubPic** ppSubPic);

	STDMETHODIMP GetStats(int& nSubPics, REFERENCE_TIME& rtNow, REFERENCE_TIME& rtStart, REFERENCE_TIME& rtStop);
	STDMETHODIMP GetStats(int nSubPic, REFERENCE_TIME& rtStart, REFERENCE_TIME& rtStop);
};

class CSubPicQueueNoThread : public CSubPicQueueImpl
{
	CCritSec m_csLock;
	CComPtr<ISubPic> m_pSubPic;

public:
	CSubPicQueueNoThread(ISubPicAllocator* pAllocator, HRESULT* phr);
	virtual ~CSubPicQueueNoThread();

	// ISubPicQueue

	STDMETHODIMP Invalidate(REFERENCE_TIME rtInvalidate = -1);
	STDMETHODIMP_(bool) LookupSubPic(REFERENCE_TIME rtNow, ISubPic** ppSubPic);

	STDMETHODIMP GetStats(int& nSubPics, REFERENCE_TIME& rtNow, REFERENCE_TIME& rtStart, REFERENCE_TIME& rtStop);
	STDMETHODIMP GetStats(int nSubPic, REFERENCE_TIME& rtStart, REFERENCE_TIME& rtStop);
};

//
// ISubPicAllocatorPresenter
//

[uuid("CF75B1F0-535C-4074-8869-B15F177F944E")]
interface ISubPicAllocatorPresenter : public IUnknown
{
	STDMETHOD (CreateRenderer) (IUnknown** ppRenderer) PURE;

	STDMETHOD_(SIZE, GetVideoSize) (bool fCorrectAR = true) PURE;
	STDMETHOD_(void, SetPosition) (RECT w, RECT v) PURE;
	STDMETHOD_(bool, Paint) (bool fAll) PURE;

	STDMETHOD_(void, SetTime) (REFERENCE_TIME rtNow) PURE;
	STDMETHOD_(void, SetSubtitleDelay) (int delay_ms) PURE;
	STDMETHOD_(int, GetSubtitleDelay) () PURE;
	STDMETHOD_(double, GetFPS) () PURE;

	STDMETHOD_(void, SetSubPicProvider) (ISubPicProvider* pSubPicProvider) PURE;
	STDMETHOD_(void, Invalidate) (REFERENCE_TIME rtInvalidate = -1) PURE;

	STDMETHOD (GetDIB) (BYTE* lpDib, DWORD* size) PURE;

	STDMETHOD (SetVideoAngle) (Vector v, bool fRepaint = true) PURE;
	STDMETHOD (SetPixelShader) (LPCSTR pSrcData, LPCSTR pTarget) PURE;
};

//
// ISubStream
//

[uuid("DE11E2FB-02D3-45e4-A174-6B7CE2783BDB")]
interface ISubStream : public IPersist
{
	STDMETHOD_(int, GetStreamCount) () PURE;
	STDMETHOD (GetStreamInfo) (int i, WCHAR** ppName, LCID* pLCID) PURE;
	STDMETHOD_(int, GetStream) () PURE;
	STDMETHOD (SetStream) (int iStream) PURE;
	STDMETHOD (Reload) () PURE;

	// TODO: get rid of IPersist to identify type and use only
	// interface functions to modify the settings of the substream
};

