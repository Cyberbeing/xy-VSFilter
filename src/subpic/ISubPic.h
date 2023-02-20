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

enum ColorType {    
    MSP_RGB32,
    MSP_RGB24,
    MSP_RGB16,
    MSP_RGB15,
    MSP_YUY2,
    MSP_YV12,
    MSP_IYUV,
    MSP_AYUV,
    MSP_RGBA,
    MSP_AYUV_PLANAR, //AYUV in planar form
    MSP_XY_AUYV,
    MSP_P010,
    MSP_P016,
    MSP_NV12,
    MSP_NV21
};

#pragma pack(push, 1)
struct SubPicDesc {
	int type;
	int w, h, bpp, pitch, pitchUV;
	void* bits;
	BYTE* bitsU;
	BYTE* bitsV;
	RECT vidrect; // video rectangle

	struct SubPicDesc() {
		type = 0;
		w = h = bpp = pitch = pitchUV = 0;
		bits = NULL;
		bitsU = bitsV = NULL;
	}
};
#pragma pack(pop)

//
// ISubPic
//

interface __declspec(uuid("449E11F3-52D1-4a27-AA61-E2733AC92CC0"))
ISubPic :
public IUnknown {
	STDMETHOD_(void*, GetObject) () const PURE;

	STDMETHOD_(REFERENCE_TIME, GetStart) () const PURE;
	STDMETHOD_(REFERENCE_TIME, GetStop) () const PURE;
	STDMETHOD_(void, SetStart) (REFERENCE_TIME rtStart) PURE;
	STDMETHOD_(void, SetStop) (REFERENCE_TIME rtStop) PURE;

	STDMETHOD (GetDesc) (SubPicDesc& spd /*[out]*/) const PURE;
	STDMETHOD (CopyTo) (ISubPic* pSubPic /*[in]*/) PURE;

	STDMETHOD (ClearDirtyRect) (DWORD color /*[in]*/) PURE;
	STDMETHOD (GetDirtyRect) (RECT* pDirtyRect /*[out]*/) const PURE;
	STDMETHOD (SetDirtyRect) (RECT* pDirtyRect /*[in]*/) PURE;

	STDMETHOD (GetMaxSize) (SIZE* pMaxSize /*[out]*/) const PURE;
	STDMETHOD (SetSize) (SIZE pSize /*[in]*/, RECT vidrect /*[in]*/) PURE;

	STDMETHOD (Lock) (SubPicDesc& spd /*[out]*/) PURE;
	STDMETHOD (Unlock) (RECT* pDirtyRect /*[in]*/) PURE;

	STDMETHOD (AlphaBlt) (const RECT* pSrc, const RECT* pDst, SubPicDesc* pTarget = NULL /*[in]*/) PURE;
	STDMETHOD (GetSourceAndDest) (SIZE* pSize /*[in]*/, RECT* pRcSource /*[out]*/, RECT* pRcDest /*[out]*/) PURE;
	STDMETHOD (SetVirtualTextureSize) (const SIZE pSize, const POINT pTopLeft) PURE;

	STDMETHOD_(REFERENCE_TIME, GetSegmentStart) () PURE;
	STDMETHOD_(REFERENCE_TIME, GetSegmentStop) () PURE;
	STDMETHOD_(void, SetSegmentStart) (REFERENCE_TIME rtStart) PURE;
	STDMETHOD_(void, SetSegmentStop) (REFERENCE_TIME rtStop) PURE;
};

interface __declspec(uuid("728B3CDC-B0B2-4DA8-8426-AEDE9C90674E"))
ISubPicEx :
public ISubPic {
    STDMETHOD (CopyTo) (ISubPicEx* pSubPicEx /*[in]*/) PURE;

    STDMETHOD (GetDirtyRects) (CAtlList<const CRect>& dirtyRectList /*[out]*/) const PURE;
    STDMETHOD (SetDirtyRectEx) (CAtlList<CRect>* dirtyRectList /*[in]*/) PURE;
    
	STDMETHOD (Unlock) (CAtlList<CRect>* dirtyRectList /*[in]*/) PURE;
};

//
// ISubPicAllocator
//

interface __declspec(uuid("CF7C3C23-6392-4a42-9E72-0736CFF793CB"))
ISubPicAllocator :
public IUnknown {
	STDMETHOD (SetCurSize) (SIZE size /*[in]*/) PURE;
	STDMETHOD (SetCurVidRect) (RECT curvidrect) PURE;

	STDMETHOD (GetStatic) (ISubPic** ppSubPic /*[out]*/) PURE;
	STDMETHOD (AllocDynamic) (ISubPic** ppSubPic /*[out]*/) PURE;

	STDMETHOD_(bool, IsDynamicWriteOnly) () PURE;

	STDMETHOD (ChangeDevice) (IUnknown* pDev) PURE;
	STDMETHOD (SetMaxTextureSize) (SIZE MaxTextureSize) PURE;
};

interface __declspec(uuid("379DD04B-F132-475E-9901-AB02FF4351A7"))
ISubPicExAllocator :
public ISubPicAllocator {
    STDMETHOD_(bool, IsSpdColorTypeSupported) (int type) PURE;
    STDMETHOD_(int, SetSpdColorType) (int color_type) PURE;

    STDMETHOD (GetStaticEx) (ISubPicEx** ppSubPic /*[out]*/) PURE;
    STDMETHOD (AllocDynamicEx) (ISubPicEx** ppSubPic /*[out]*/) PURE;
};

//
// ISubPicProvider
//

interface __declspec(uuid("D62B9A1A-879A-42db-AB04-88AA8F243CFD"))
ISubPicProvider :
public IUnknown {
	STDMETHOD (Lock) () PURE;
	STDMETHOD (Unlock) () PURE;

	STDMETHOD_(POSITION, GetStartPosition) (REFERENCE_TIME rt, double fps) PURE;
	STDMETHOD_(POSITION, GetNext) (POSITION pos) PURE;

	STDMETHOD_(REFERENCE_TIME, GetStart) (POSITION pos, double fps) PURE;
	STDMETHOD_(REFERENCE_TIME, GetStop) (POSITION pos, double fps) PURE;

	STDMETHOD_(bool, IsAnimated) (POSITION pos) PURE;

	STDMETHOD (Render) (SubPicDesc& spd, REFERENCE_TIME rt, double fps, RECT& bbox) PURE;
    STDMETHOD (GetTextureSize) (POSITION pos, SIZE& MaxTextureSize, SIZE& VirtualSize, POINT& VirtualTopLeft) PURE;
};

interface __declspec(uuid("F8B6C39E-4188-41E0-9CCF-79579C376A40"))
ISubPicProviderEx :
public ISubPicProvider {
	STDMETHOD_(VOID, GetStartStop) (POSITION pos, double fps, /*out*/REFERENCE_TIME& start, /*out*/REFERENCE_TIME& stop) PURE;
    STDMETHOD (RenderEx) (SubPicDesc& spd, REFERENCE_TIME rt, double fps, CAtlList<CRect>& rectList) PURE;

    STDMETHOD_(bool, IsColorTypeSupported) (int type) PURE;
};

//
// ISubPicQueue
//

interface __declspec(uuid("C8334466-CD1E-4ad1-9D2D-8EE8519BD180"))
ISubPicQueue :
public IUnknown {
	STDMETHOD (SetSubPicProvider) (ISubPicProvider* pSubPicProvider /*[in]*/) PURE;
	STDMETHOD (GetSubPicProvider) (ISubPicProvider** pSubPicProvider /*[out]*/) PURE;

	STDMETHOD (SetFPS) (double fps /*[in]*/) PURE;
	STDMETHOD (SetTime) (REFERENCE_TIME rtNow /*[in]*/) PURE;

	STDMETHOD (Invalidate) (REFERENCE_TIME rtInvalidate = -1) PURE;
	STDMETHOD_(bool, LookupSubPic) (REFERENCE_TIME rtNow /*[in]*/, ISubPic** ppSubPic /*[out]*/) PURE;

	STDMETHOD (GetStats) (int& nSubPics, REFERENCE_TIME& rtNow, REFERENCE_TIME& rtStart, REFERENCE_TIME& rtStop /*[out]*/) PURE;
	STDMETHOD (GetStats) (int nSubPic /*[in]*/, REFERENCE_TIME& rtStart, REFERENCE_TIME& rtStop /*[out]*/) PURE;
};

//
// ISubPicAllocatorPresenter
//

interface __declspec(uuid("CF75B1F0-535C-4074-8869-B15F177F944E"))
ISubPicAllocatorPresenter :
public IUnknown {
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

	STDMETHOD_(bool, ResetDevice) () PURE;

	STDMETHOD_(bool, DisplayChange) () PURE;
};

interface __declspec(uuid("767AEBA8-A084-488a-89C8-F6B74E53A90F"))
ISubPicAllocatorPresenter2 :
public ISubPicAllocatorPresenter {
	STDMETHOD (SetPixelShader2) (LPCSTR pSrcData, LPCSTR pTarget, bool bScreenSpace) PURE;
	STDMETHOD_(SIZE, GetVisibleVideoSize) () PURE;
};


//
// ISubStream
//

interface __declspec(uuid("DE11E2FB-02D3-45e4-A174-6B7CE2783BDB"))
ISubStream :
public IPersist {
	STDMETHOD_(int, GetStreamCount) () PURE;
	STDMETHOD (GetStreamInfo) (int i, WCHAR** ppName, LCID* pLCID) PURE;
	STDMETHOD_(int, GetStream) () PURE;
	STDMETHOD (SetStream) (int iStream) PURE;
	STDMETHOD (Reload) () PURE;

	// TODO: get rid of IPersist to identify type and use only
	// interface functions to modify the settings of the substream
};

