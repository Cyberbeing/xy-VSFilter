/*
 *  $Id: SubPicImpl.h 2786 2010-12-17 16:42:55Z XhmikosR $
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

class CSubPicImpl : public CUnknown, public ISubPic
{
protected:
    REFERENCE_TIME m_rtStart, m_rtStop;
    REFERENCE_TIME m_rtSegmentStart, m_rtSegmentStop;
    CAtlList<CRect> m_rectListDirty;

    CSize m_maxsize, m_size;
    CRect m_vidrect;
	CSize	m_VirtualTextureSize;
	CPoint	m_VirtualTextureTopLeft;

	/*

	                          Texture
	        +-------+---------------------------------+
	        |       .                                 |   .
	        |       .             m_maxsize           |       .
	 TextureTopLeft .<=============================== |======>    .           Video
	        | . . . +-------------------------------- | -----+       +-----------------------------------+
	        |       |                         .       |      |       |  m_vidrect                        |
	        |       |                         .       |      |       |                                   |
	        |       |                         .       |      |       |                                   |
	        |       |        +-----------+    .       |      |       |                                   |
	        |       |        | m_rcDirty |    .       |      |       |                                   |
	        |       |        |           |    .       |      |       |                                   |
	        |       |        +-----------+    .       |      |       |                                   |
	        |       +-------------------------------- | -----+       |                                   |
	        |                    m_size               |              |                                   |
	        |       <=========================>       |              |                                   |
	        |                                         |              |                                   |
	        |                                         |              +-----------------------------------+
	        |                                         |          .
	        |                                         |      .
	        |                                         |   .
	        +-----------------------------------------+
	                   m_VirtualTextureSize
	        <=========================================>

	*/


public:
    static const int DIRTY_RECT_MERGE_EMPTY_AREA = 100;

    CSubPicImpl();

    DECLARE_IUNKNOWN;
    STDMETHODIMP NonDelegatingQueryInterface(REFIID riid, void** ppv);

    // ISubPic

    STDMETHODIMP_(REFERENCE_TIME) GetStart() const;
    STDMETHODIMP_(REFERENCE_TIME) GetStop() const;
    STDMETHODIMP_(void) SetStart(REFERENCE_TIME rtStart);
    STDMETHODIMP_(void) SetStop(REFERENCE_TIME rtStop);

    STDMETHODIMP GetDesc(SubPicDesc& spd) const = 0;
    STDMETHODIMP CopyTo(ISubPic* pSubPic);

    STDMETHODIMP ClearDirtyRect(DWORD color) = 0;
    STDMETHODIMP GetDirtyRect(RECT* pDirtyRect) const;
    STDMETHODIMP GetDirtyRects(CAtlList<const CRect>& dirtyRectList /*[out]*/) const;
    STDMETHODIMP SetDirtyRect(CAtlList<CRect>* dirtyRectList /*[in]*/);

    STDMETHODIMP GetMaxSize(SIZE* pMaxSize) const;
    STDMETHODIMP SetSize(SIZE size, RECT vidrect);

    STDMETHODIMP Lock(SubPicDesc& spd) = 0;
    STDMETHODIMP Unlock(CAtlList<CRect>* dirtyRectList /*[in]*/) = 0;

    STDMETHODIMP AlphaBlt(const RECT* pSrc, const RECT* pDst, SubPicDesc* pTarget) = 0;

	STDMETHODIMP SetVirtualTextureSize (const SIZE pSize, const POINT pTopLeft);
	STDMETHODIMP GetSourceAndDest(SIZE* pSize, RECT* pRcSource, RECT* pRcDest);

	STDMETHODIMP_(REFERENCE_TIME) GetSegmentStart();
	STDMETHODIMP_(REFERENCE_TIME) GetSegmentStop();
	STDMETHODIMP_(void) SetSegmentStart(REFERENCE_TIME rtStart);
	STDMETHODIMP_(void) SetSegmentStop(REFERENCE_TIME rtStop);

};


class CSubPicAllocatorImpl : public CUnknown, public ISubPicAllocator
{
    CComPtr<ISubPic> m_pStatic;

private:
    CSize m_cursize;
    CRect m_curvidrect;
    bool m_fDynamicWriteOnly;

    virtual bool Alloc(bool fStatic, ISubPic** ppSubPic) = 0;

protected:
    bool m_fPow2Textures;

public:
    CSubPicAllocatorImpl(SIZE cursize, bool fDynamicWriteOnly, bool fPow2Textures);

    DECLARE_IUNKNOWN;
    STDMETHODIMP NonDelegatingQueryInterface(REFIID riid, void** ppv);

    // ISubPicAllocator

    STDMETHODIMP SetCurSize(SIZE cursize);
    STDMETHODIMP SetCurVidRect(RECT curvidrect);
    STDMETHODIMP GetStatic(ISubPic** ppSubPic);
    STDMETHODIMP AllocDynamic(ISubPic** ppSubPic);
    STDMETHODIMP_(bool) IsDynamicWriteOnly();
    STDMETHODIMP ChangeDevice(IUnknown* pDev);
    STDMETHODIMP SetMaxTextureSize(SIZE MaxTextureSize) {
        return E_NOTIMPL;
    };
};

