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

class CSubPicImplHelper
{
protected:
    REFERENCE_TIME m_rtStart, m_rtStop;
    REFERENCE_TIME m_rtSegmentStart, m_rtSegmentStop;

    CRect m_rcDirty;

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

    CSubPicImplHelper();
    
    // ISubPic

    STDMETHODIMP_(REFERENCE_TIME) GetStart() const;
    STDMETHODIMP_(REFERENCE_TIME) GetStop() const;
    STDMETHODIMP_(void) SetStart(REFERENCE_TIME rtStart);
    STDMETHODIMP_(void) SetStop(REFERENCE_TIME rtStop);

    //STDMETHODIMP GetDesc(SubPicDesc& spd) const = 0;
    STDMETHODIMP CopyTo(ISubPic* pSubPic);

    //STDMETHODIMP ClearDirtyRect(DWORD color) = 0;
    STDMETHODIMP GetDirtyRect(RECT* pDirtyRect) const;
    STDMETHODIMP SetDirtyRect(RECT* pDirtyRect /*[in]*/);

    STDMETHODIMP GetMaxSize(SIZE* pMaxSize) const;
    STDMETHODIMP SetSize(SIZE size, RECT vidrect);

    //STDMETHODIMP Lock(SubPicDesc& spd) = 0;
    //STDMETHODIMP Unlock(RECT* pDirtyRect /*[in]*/) = 0;

    //STDMETHODIMP AlphaBlt(const RECT* pSrc, const RECT* pDst, SubPicDesc* pTarget) = 0;

	STDMETHODIMP SetVirtualTextureSize (const SIZE pSize, const POINT pTopLeft);
	STDMETHODIMP GetSourceAndDest(SIZE* pSize, RECT* pRcSource, RECT* pRcDest);

	STDMETHODIMP_(REFERENCE_TIME) GetSegmentStart();
	STDMETHODIMP_(REFERENCE_TIME) GetSegmentStop();
	STDMETHODIMP_(void) SetSegmentStart(REFERENCE_TIME rtStart);
	STDMETHODIMP_(void) SetSegmentStop(REFERENCE_TIME rtStop);
};

class CSubPicImpl : public CUnknown, public CSubPicImplHelper, public ISubPic
{
public:
    CSubPicImpl();

    DECLARE_IUNKNOWN;
    STDMETHODIMP NonDelegatingQueryInterface(REFIID riid, void** ppv);

    // ISubPic

    STDMETHODIMP_(REFERENCE_TIME) GetStart() const
    {
        return CSubPicImplHelper::GetStart();
    }
    STDMETHODIMP_(REFERENCE_TIME) GetStop() const
    {
        return CSubPicImplHelper::GetStop();
    }
    STDMETHODIMP_(void) SetStart(REFERENCE_TIME rtStart)
    {
        return CSubPicImplHelper::SetStart(rtStart);
    }
    STDMETHODIMP_(void) SetStop(REFERENCE_TIME rtStop)
    {
        return CSubPicImplHelper::SetStop(rtStop);
    }

    STDMETHODIMP GetDesc(SubPicDesc& spd) const = 0;
    STDMETHODIMP CopyTo(ISubPic* pSubPic)
    {
        return CSubPicImplHelper::CopyTo(pSubPic);
    }

    STDMETHODIMP ClearDirtyRect(DWORD color) = 0;
    STDMETHODIMP GetDirtyRect(RECT* pDirtyRect) const
    {
        return CSubPicImplHelper::GetDirtyRect(pDirtyRect);
    }
    STDMETHODIMP SetDirtyRect(RECT* pDirtyRect /*[in]*/)
    {
        return CSubPicImplHelper::SetDirtyRect(pDirtyRect);
    }

    STDMETHODIMP GetMaxSize(SIZE* pMaxSize) const
    {
        return CSubPicImplHelper::GetMaxSize(pMaxSize);
    }
    STDMETHODIMP SetSize(SIZE size, RECT vidrect)
    {
        return CSubPicImplHelper::SetSize(size, vidrect);
    }

    STDMETHODIMP Lock(SubPicDesc& spd) = 0;
    STDMETHODIMP Unlock(RECT* pDirtyRect /*[in]*/) = 0;

    STDMETHODIMP AlphaBlt(const RECT* pSrc, const RECT* pDst, SubPicDesc* pTarget) = 0;

	STDMETHODIMP SetVirtualTextureSize (const SIZE pSize, const POINT pTopLeft)
    {
        return CSubPicImplHelper::SetVirtualTextureSize(pSize, pTopLeft);
    }
	STDMETHODIMP GetSourceAndDest(SIZE* pSize, RECT* pRcSource, RECT* pRcDest)
    {
        return CSubPicImplHelper::GetSourceAndDest(pSize, pRcSource, pRcDest);
    }

	STDMETHODIMP_(REFERENCE_TIME) GetSegmentStart()
    {
        return CSubPicImplHelper::GetSegmentStart();
    }
	STDMETHODIMP_(REFERENCE_TIME) GetSegmentStop()
    {
        return CSubPicImplHelper::GetSegmentStop();
    }
	STDMETHODIMP_(void) SetSegmentStart(REFERENCE_TIME rtStart)
    {
        return CSubPicImplHelper::SetSegmentStart(rtStart);
    }
	STDMETHODIMP_(void) SetSegmentStop(REFERENCE_TIME rtStop)
    {
        return CSubPicImplHelper::SetSegmentStop(rtStop);
    }
};

class CSubPicExImpl : public CUnknown, public CSubPicImplHelper, public ISubPicEx
{
protected:
    CAtlList<CRect> m_rectListDirty;

public:	
    CSubPicExImpl();

    DECLARE_IUNKNOWN;
    STDMETHODIMP NonDelegatingQueryInterface(REFIID riid, void** ppv);
    
    // ISubPic

    STDMETHODIMP_(REFERENCE_TIME) GetStart() const
    {
        return CSubPicImplHelper::GetStart();
    }
    STDMETHODIMP_(REFERENCE_TIME) GetStop() const
    {
        return CSubPicImplHelper::GetStop();
    }
    STDMETHODIMP_(void) SetStart(REFERENCE_TIME rtStart)
    {
        return CSubPicImplHelper::SetStart(rtStart);
    }
    STDMETHODIMP_(void) SetStop(REFERENCE_TIME rtStop)
    {
        return CSubPicImplHelper::SetStop(rtStop);
    }

    STDMETHODIMP GetDesc(SubPicDesc& spd) const = 0;
    STDMETHODIMP CopyTo(ISubPic* pSubPic)
    {
        return CSubPicImplHelper::CopyTo(pSubPic);
    }

    STDMETHODIMP ClearDirtyRect(DWORD color) = 0;
    STDMETHODIMP GetDirtyRect(RECT* pDirtyRect) const
    {
        return CSubPicImplHelper::GetDirtyRect(pDirtyRect);
    }
    STDMETHODIMP SetDirtyRect(RECT* pDirtyRect /*[in]*/);

    STDMETHODIMP GetMaxSize(SIZE* pMaxSize) const
    {
        return CSubPicImplHelper::GetMaxSize(pMaxSize);
    }
    STDMETHODIMP SetSize(SIZE size, RECT vidrect)
    {
        return CSubPicImplHelper::SetSize(size, vidrect);
    }

    STDMETHODIMP Lock(SubPicDesc& spd) = 0;
    STDMETHODIMP Unlock(RECT* pDirtyRect /*[in]*/);

    STDMETHODIMP AlphaBlt(const RECT* pSrc, const RECT* pDst, SubPicDesc* pTarget) = 0;

    STDMETHODIMP SetVirtualTextureSize (const SIZE pSize, const POINT pTopLeft)
    {
        return CSubPicImplHelper::SetVirtualTextureSize(pSize, pTopLeft);
    }
    STDMETHODIMP GetSourceAndDest(SIZE* pSize, RECT* pRcSource, RECT* pRcDest)
    {
        return CSubPicImplHelper::GetSourceAndDest(pSize, pRcSource, pRcDest);
    }

    STDMETHODIMP_(REFERENCE_TIME) GetSegmentStart()
    {
        return CSubPicImplHelper::GetSegmentStart();
    }
    STDMETHODIMP_(REFERENCE_TIME) GetSegmentStop()
    {
        return CSubPicImplHelper::GetSegmentStop();
    }
    STDMETHODIMP_(void) SetSegmentStart(REFERENCE_TIME rtStart)
    {
        return CSubPicImplHelper::SetSegmentStart(rtStart);
    }
    STDMETHODIMP_(void) SetSegmentStop(REFERENCE_TIME rtStop)
    {
        return CSubPicImplHelper::SetSegmentStop(rtStop);
    }

    // ISubPicEx
    STDMETHODIMP CopyTo(ISubPicEx* pSubPicEx);

    STDMETHODIMP GetDirtyRects(CAtlList<const CRect>& dirtyRectList /*[out]*/) const;
    STDMETHODIMP SetDirtyRectEx(CAtlList<CRect>* dirtyRectList /*[in]*/);

    STDMETHODIMP UnlockEx(CAtlList<CRect>* dirtyRectList /*[in]*/) = 0;
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

class CSubPicExAllocatorImpl : public CUnknown, public ISubPicExAllocator
{
private:
    CSize m_cursize;
    CRect m_curvidrect;
    bool m_fDynamicWriteOnly;
    
    bool Alloc(bool fStatic, ISubPic** ppSubPic);
    
    virtual bool AllocEx(bool fStatic, ISubPicEx** ppSubPic) = 0;
protected:
    CComPtr<ISubPicEx> m_pStatic;
    bool m_fPow2Textures;
public:
    CSubPicExAllocatorImpl(SIZE cursize, bool fDynamicWriteOnly, bool fPow2Textures);

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

    // ISubPicExAllocator
    STDMETHODIMP_(bool) IsSpdColorTypeSupported(int type);
    STDMETHODIMP_(int) SetSpdColorType(int color_type);

    STDMETHODIMP GetStaticEx(ISubPicEx** ppSubPic);
    STDMETHODIMP AllocDynamicEx(ISubPicEx** ppSubPic);
};