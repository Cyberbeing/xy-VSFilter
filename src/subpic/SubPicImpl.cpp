/*
 *  $Id: SubPicImpl.cpp 2786 2010-12-17 16:42:55Z XhmikosR $
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

#include "stdafx.h"
#include "SubPicImpl.h"
#include "CRect2.h"
#include "../DSUtil/DSUtil.h"
#include "../DSUtil/xy_utils.h"

//
// CSubPicImplHelper
//

CSubPicImplHelper::CSubPicImplHelper()
    : m_rtStart(0), m_rtStop(0)
	, m_rtSegmentStart(0), m_rtSegmentStop(0)
	, m_rcDirty(0, 0, 0, 0), m_maxsize(0, 0), m_size(0, 0), m_vidrect(0, 0, 0, 0)
	, m_VirtualTextureSize(0, 0), m_VirtualTextureTopLeft (0, 0)
{
}

// ISubPic

STDMETHODIMP_(REFERENCE_TIME) CSubPicImplHelper::GetStart() const
{
    return(m_rtStart);
}

STDMETHODIMP_(REFERENCE_TIME) CSubPicImplHelper::GetStop() const
{
    return(m_rtStop);
}

STDMETHODIMP_(REFERENCE_TIME) CSubPicImplHelper::GetSegmentStart()
{
	if (m_rtSegmentStart) {
		return(m_rtSegmentStart);
	}
	return(m_rtStart);
}

STDMETHODIMP_(REFERENCE_TIME) CSubPicImplHelper::GetSegmentStop()
{
	if (m_rtSegmentStop) {
		return(m_rtSegmentStop);
	}
	return(m_rtStop);
}

STDMETHODIMP_(void) CSubPicImplHelper::SetSegmentStart(REFERENCE_TIME rtStart)
{
	m_rtSegmentStart = rtStart;
}

STDMETHODIMP_(void) CSubPicImplHelper::SetSegmentStop(REFERENCE_TIME rtStop)
{
	m_rtSegmentStop = rtStop;
}

STDMETHODIMP_(void) CSubPicImplHelper::SetStart(REFERENCE_TIME rtStart)
{
    m_rtStart = rtStart;
}

STDMETHODIMP_(void) CSubPicImplHelper::SetStop(REFERENCE_TIME rtStop)
{
    m_rtStop = rtStop;
}

STDMETHODIMP CSubPicImplHelper::CopyTo(ISubPic* pSubPic)
{
    if(!pSubPic)
        return E_POINTER;

    pSubPic->SetStart(m_rtStart);
    pSubPic->SetStop(m_rtStop);
	pSubPic->SetSegmentStart(m_rtSegmentStart);
	pSubPic->SetSegmentStop(m_rtSegmentStop);
    pSubPic->SetDirtyRect(m_rcDirty);
    pSubPic->SetSize(m_size, m_vidrect);
	pSubPic->SetVirtualTextureSize(m_VirtualTextureSize, m_VirtualTextureTopLeft);

    return S_OK;
}

STDMETHODIMP CSubPicImplHelper::GetDirtyRect(RECT* pDirtyRect) const
{
	return pDirtyRect ? *pDirtyRect = m_rcDirty, S_OK : E_POINTER;
}

STDMETHODIMP CSubPicImplHelper::GetSourceAndDest(SIZE* pSize, RECT* pRcSource, RECT* pRcDest)
{
	CheckPointer (pRcSource, E_POINTER);
	CheckPointer (pRcDest,	 E_POINTER);

	if(m_size.cx > 0 && m_size.cy > 0) {
		CRect		rcTemp = m_rcDirty;

		// FIXME
		rcTemp.DeflateRect(1, 1);

		*pRcSource = rcTemp;

		rcTemp.OffsetRect (m_VirtualTextureTopLeft);
		*pRcDest = CRect (rcTemp.left   * pSize->cx / m_VirtualTextureSize.cx,
						  rcTemp.top    * pSize->cy / m_VirtualTextureSize.cy,
						  rcTemp.right  * pSize->cx / m_VirtualTextureSize.cx,
						  rcTemp.bottom * pSize->cy / m_VirtualTextureSize.cy);

		return S_OK;
	} else {
		return E_INVALIDARG;
	}
}

STDMETHODIMP CSubPicImplHelper::SetDirtyRect(RECT* pDirtyRect)
{
	return pDirtyRect ? m_rcDirty = *pDirtyRect, S_OK : E_POINTER;
}

STDMETHODIMP CSubPicImplHelper::GetMaxSize(SIZE* pMaxSize) const
{
    return pMaxSize ? *pMaxSize = m_maxsize, S_OK : E_POINTER;
}

STDMETHODIMP CSubPicImplHelper::SetSize(SIZE size, RECT vidrect)
{
	m_size = size;
	m_vidrect = vidrect;

	if(m_size.cx > m_maxsize.cx) {
		m_size.cy = MulDiv(m_size.cy, m_maxsize.cx, m_size.cx);
		m_size.cx = m_maxsize.cx;
	}

	if(m_size.cy > m_maxsize.cy) {
		m_size.cx = MulDiv(m_size.cx, m_maxsize.cy, m_size.cy);
		m_size.cy = m_maxsize.cy;
	}

	if(m_size.cx != size.cx || m_size.cy != size.cy) {
        m_vidrect.top    = MulDiv(m_vidrect.top, m_size.cx, size.cx);
        m_vidrect.bottom = MulDiv(m_vidrect.bottom, m_size.cx, size.cx);
        m_vidrect.left   = MulDiv(m_vidrect.left, m_size.cy, size.cy);
        m_vidrect.right  = MulDiv(m_vidrect.right, m_size.cy, size.cy);
	}
	m_VirtualTextureSize = m_size;

	return S_OK;
}

STDMETHODIMP CSubPicImplHelper::SetVirtualTextureSize (const SIZE pSize, const POINT pTopLeft)
{
	m_VirtualTextureSize.SetSize (pSize.cx, pSize.cy);
	m_VirtualTextureTopLeft.SetPoint (pTopLeft.x, pTopLeft.y);

	return S_OK;
}

//
// CSubPicImpl
//

CSubPicImpl::CSubPicImpl()
    :CUnknown(NAME("CSubPicImpl"), NULL)
    ,CSubPicImplHelper()
{

}

STDMETHODIMP CSubPicImpl::NonDelegatingQueryInterface( REFIID riid, void** ppv )
{
    return
        QI(ISubPic)
        __super::NonDelegatingQueryInterface(riid, ppv);
}

//
// CSubPicExImpl
//

CSubPicExImpl::CSubPicExImpl()
    :CUnknown(NAME("CSubPicExImpl"),NULL),CSubPicImplHelper()
{

}

STDMETHODIMP CSubPicExImpl::NonDelegatingQueryInterface( REFIID riid, void** ppv )
{
    return
        QI(ISubPicEx)
        (riid == __uuidof(ISubPic)) ? GetInterface( static_cast<ISubPicEx*>(this), ppv) :
        __super::NonDelegatingQueryInterface(riid, ppv);
}

STDMETHODIMP CSubPicExImpl::CopyTo( ISubPicEx* pSubPicEx )
{
    if(!pSubPicEx)
        return E_POINTER;

    pSubPicEx->SetStart(m_rtStart);
    pSubPicEx->SetStop(m_rtStop);
    pSubPicEx->SetSegmentStart(m_rtSegmentStart);
    pSubPicEx->SetSegmentStop(m_rtSegmentStop);
    pSubPicEx->SetDirtyRectEx(&m_rectListDirty);
    pSubPicEx->SetSize(m_size, m_vidrect);
    pSubPicEx->SetVirtualTextureSize(m_VirtualTextureSize, m_VirtualTextureTopLeft);

    return S_OK;
}

STDMETHODIMP CSubPicExImpl::SetDirtyRect(RECT* pDirtyRect)
{
    HRESULT hr = CSubPicImplHelper::SetDirtyRect(pDirtyRect);
    if(hr && pDirtyRect)
    {
        m_rectListDirty.RemoveAll();
        m_rectListDirty.AddTail(*pDirtyRect);
        return S_OK;
    }
    else
    {
	    return E_POINTER;
    }
}

STDMETHODIMP CSubPicExImpl::SetDirtyRectEx(CAtlList<CRect>* dirtyRectList)
{
    if(dirtyRectList!=NULL)
    {
        m_rectListDirty.RemoveAll();
        m_rcDirty.SetRectEmpty();

        POSITION tagPos = NULL;
        POSITION pos = dirtyRectList->GetHeadPosition();
        while(pos!=NULL)
        {
            m_rcDirty |= dirtyRectList->GetNext(pos);
        }
        MergeRects(*dirtyRectList, &m_rectListDirty);
        return S_OK;
    }
    return E_POINTER;
}

STDMETHODIMP CSubPicExImpl::GetDirtyRects( CAtlList<const CRect>& dirtyRectList /*[out]*/ ) const
{
    POSITION pos = m_rectListDirty.GetHeadPosition();
    while(pos!=NULL)
    {
        dirtyRectList.AddTail(m_rectListDirty.GetNext(pos));
    }
    return S_OK;
}

STDMETHODIMP CSubPicExImpl::Unlock( RECT* pDirtyRect /*[in]*/ )
{
    if(pDirtyRect)
    {
        CAtlList<CRect> tmp;
        tmp.AddTail(*pDirtyRect);
        return UnlockEx(&tmp);
    }
    else
    {
        return UnlockEx( reinterpret_cast<CAtlList<CRect>*>(NULL) );
    }
}


//
// CSubPicAllocatorImpl
//

CSubPicAllocatorImpl::CSubPicAllocatorImpl(SIZE cursize, bool fDynamicWriteOnly, bool fPow2Textures )
    : CUnknown(NAME("ISubPicAllocatorImpl"), NULL)
    , m_cursize(cursize)
    , m_fDynamicWriteOnly(fDynamicWriteOnly)
    , m_fPow2Textures(fPow2Textures)
{
    m_curvidrect = CRect(CPoint(0,0), m_cursize);
}

STDMETHODIMP CSubPicAllocatorImpl::NonDelegatingQueryInterface(REFIID riid, void** ppv)
{
    return
        QI(ISubPicAllocator)
        __super::NonDelegatingQueryInterface(riid, ppv);
}

// ISubPicAllocator

STDMETHODIMP CSubPicAllocatorImpl::SetCurSize(SIZE cursize)
{
    m_cursize = cursize;
    return S_OK;
}

STDMETHODIMP CSubPicAllocatorImpl::SetCurVidRect(RECT curvidrect)
{
    m_curvidrect = curvidrect;
    return S_OK;
}

STDMETHODIMP CSubPicAllocatorImpl::GetStatic(ISubPic** ppSubPic)
{
	if(!ppSubPic) {
		return E_POINTER;
	}

	if(!m_pStatic) {
		if(!Alloc(true, &m_pStatic) || !m_pStatic) {
			return E_OUTOFMEMORY;
		}
	}

	m_pStatic->SetSize(m_cursize, m_curvidrect);

	(*ppSubPic = m_pStatic)->AddRef();

	return S_OK;
}

STDMETHODIMP CSubPicAllocatorImpl::AllocDynamic(ISubPic** ppSubPic)
{
	if(!ppSubPic) {
		return E_POINTER;
	}

	if(!Alloc(false, ppSubPic) || !*ppSubPic) {
		return E_OUTOFMEMORY;
	}

	(*ppSubPic)->SetSize(m_cursize, m_curvidrect);

	return S_OK;
}

STDMETHODIMP_(bool) CSubPicAllocatorImpl::IsDynamicWriteOnly()
{
    return(m_fDynamicWriteOnly);
}

STDMETHODIMP CSubPicAllocatorImpl::ChangeDevice(IUnknown* pDev)
{
    m_pStatic = NULL;
    return S_OK;
}

//
// CSubPicExAllocatorImpl
// 

CSubPicExAllocatorImpl::CSubPicExAllocatorImpl( SIZE cursize, bool fDynamicWriteOnly, bool fPow2Textures )
    :CUnknown(NAME("CSubPicExAllocatorImpl"), NULL)
    , m_cursize(cursize)
    , m_fDynamicWriteOnly(fDynamicWriteOnly)
    , m_fPow2Textures(fPow2Textures)
{

}

STDMETHODIMP CSubPicExAllocatorImpl::NonDelegatingQueryInterface(REFIID riid, void** ppv)
{
    return
        QI(ISubPicExAllocator)
        QI(ISubPicAllocator)
        __super::NonDelegatingQueryInterface(riid, ppv);
}

// ISubPicAllocator

STDMETHODIMP CSubPicExAllocatorImpl::SetCurSize(SIZE cursize)
{
    m_cursize = cursize;
    return S_OK;
}

STDMETHODIMP CSubPicExAllocatorImpl::SetCurVidRect(RECT curvidrect)
{
    m_curvidrect = curvidrect;
    return S_OK;
}

STDMETHODIMP CSubPicExAllocatorImpl::GetStatic(ISubPic** ppSubPic)
{
    if( !ppSubPic )
    {
        return GetStaticEx(NULL);
    }
    else
    {
        ISubPicEx *temp;
        HRESULT result = GetStaticEx(&temp);
        *ppSubPic = temp;
        return result;
    }
}

STDMETHODIMP CSubPicExAllocatorImpl::AllocDynamic(ISubPic** ppSubPic)
{
    if( !ppSubPic )
    {
        return AllocDynamicEx(NULL);
    }
    else
    {
        ISubPicEx *temp;
        HRESULT result = AllocDynamicEx(&temp);
        *ppSubPic = temp;
        return result;
    }
}

STDMETHODIMP_(bool) CSubPicExAllocatorImpl::IsDynamicWriteOnly()
{
    return(m_fDynamicWriteOnly);
}

STDMETHODIMP CSubPicExAllocatorImpl::ChangeDevice(IUnknown* pDev)
{
    m_pStatic = NULL;
    return S_OK;
}

// ISubPicExAllocator

STDMETHODIMP CSubPicExAllocatorImpl::GetStaticEx(ISubPicEx** ppSubPic)
{
    if(!ppSubPic) {
        return E_POINTER;
    }

    if(!m_pStatic) {
        if(!AllocEx(true, &m_pStatic) || !m_pStatic) {
            return E_OUTOFMEMORY;
        }
    }

    m_pStatic->SetSize(m_cursize, m_curvidrect);

    (*ppSubPic = m_pStatic)->AddRef();

    return S_OK;
}

STDMETHODIMP CSubPicExAllocatorImpl::AllocDynamicEx(ISubPicEx** ppSubPic)
{
    if(!ppSubPic) {
        return E_POINTER;
    }

    if(!AllocEx(false, ppSubPic) || !*ppSubPic) {
        return E_OUTOFMEMORY;
    }

    (*ppSubPic)->SetSize(m_cursize, m_curvidrect);

    return S_OK;
}

bool CSubPicExAllocatorImpl::Alloc( bool fStatic, ISubPic** ppSubPic )
{
    if(ppSubPic==NULL)
    {
        return AllocEx(fStatic, NULL);
    }
    else
    {
        ISubPicEx *temp;
        bool result = AllocEx(fStatic, &temp);
        *ppSubPic = temp;
        return result;
    }
}

STDMETHODIMP_(int) CSubPicExAllocatorImpl::SetSpdColorType( int color_type )
{
    (color_type);
    return MSP_RGBA;
}

STDMETHODIMP_(bool) CSubPicExAllocatorImpl::IsSpdColorTypeSupported( int type )
{
    return MSP_RGBA==type;
}
