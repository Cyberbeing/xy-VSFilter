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

//
// CSubPicImpl
//

CSubPicImpl::CSubPicImpl()
    : CUnknown(NAME("CSubPicImpl"), NULL)
    , m_rtStart(0), m_rtStop(0)
    , m_maxsize(0, 0), m_size(0, 0), m_vidrect(0, 0, 0, 0)
{
}

STDMETHODIMP CSubPicImpl::NonDelegatingQueryInterface(REFIID riid, void** ppv)
{
    return
        QI(ISubPic)
        __super::NonDelegatingQueryInterface(riid, ppv);
}

// ISubPic

STDMETHODIMP_(REFERENCE_TIME) CSubPicImpl::GetStart() const
{
    return(m_rtStart);
}

STDMETHODIMP_(REFERENCE_TIME) CSubPicImpl::GetStop() const
{
    return(m_rtStop);
}

STDMETHODIMP_(void) CSubPicImpl::SetStart(REFERENCE_TIME rtStart)
{
    m_rtStart = rtStart;
}

STDMETHODIMP_(void) CSubPicImpl::SetStop(REFERENCE_TIME rtStop)
{
    m_rtStop = rtStop;
}

STDMETHODIMP CSubPicImpl::CopyTo(ISubPic* pSubPic)
{
    if(!pSubPic)
        return E_POINTER;

    pSubPic->SetStart(m_rtStart);
    pSubPic->SetStop(m_rtStop);
    pSubPic->SetDirtyRect(&m_rectListDirty);
    pSubPic->SetSize(m_size, m_vidrect);

    return S_OK;
}

STDMETHODIMP CSubPicImpl::GetDirtyRect(RECT* pDirtyRect) const
{
    if(pDirtyRect!=NULL)
    {
        CRect result = CRect(0,0,0,0);
        POSITION pos = m_rectListDirty.GetHeadPosition();
        while(pos!=NULL)
        {
            result |= m_rectListDirty.GetNext(pos);
        }
        *pDirtyRect = result;
        return S_OK;
    }
    return E_POINTER;
}

STDMETHODIMP CSubPicImpl::SetDirtyRect(CAtlList<CRect>* dirtyRectList)
{
    m_rectListDirty.RemoveAll();
    if(dirtyRectList!=NULL)
    {
        POSITION tagPos = NULL;
        POSITION pos = dirtyRectList->GetHeadPosition();
        while(pos!=NULL)
        {
            CRect crectSrc = dirtyRectList->GetNext(pos);
            tagPos = m_rectListDirty.GetHeadPosition();
            while(tagPos!=NULL)
            {
                CRect crectTag = m_rectListDirty.GetAt(tagPos);
                if(CRect2::IsIntersect(&crectTag, &crectSrc))
                {
                    crectSrc |= crectTag;
                    m_rectListDirty.RemoveAt(tagPos);
                    tagPos = m_rectListDirty.GetHeadPosition();
                }
                else
                {
                    CRect temp;
                    temp.UnionRect(crectSrc, crectTag);
                    if(CRect2::Area(temp) - CRect2::Area(crectSrc) - CRect2::Area(crectTag) > DIRTY_RECT_MERGE_EMPTY_AREA)
                    {
                        m_rectListDirty.GetNext(tagPos);
                    }
                    else
                    {
                        crectSrc |= crectTag;
                        m_rectListDirty.RemoveAt(tagPos);
                        tagPos = m_rectListDirty.GetHeadPosition();
                    }
                }
            }
            m_rectListDirty.AddTail(crectSrc);
        }
    }
    return S_OK;
}

STDMETHODIMP CSubPicImpl::GetMaxSize(SIZE* pMaxSize) const
{
    return pMaxSize ? *pMaxSize = m_maxsize, S_OK : E_POINTER;
}

STDMETHODIMP CSubPicImpl::SetSize(SIZE size, RECT vidrect)
{
    m_size = size;
    m_vidrect = vidrect;

    if(m_size.cx > m_maxsize.cx)
    {
        m_size.cy = MulDiv(m_size.cy, m_maxsize.cx, m_size.cx);
        m_size.cx = m_maxsize.cx;
    }

    if(m_size.cy > m_maxsize.cy)
    {
        m_size.cx = MulDiv(m_size.cx, m_maxsize.cy, m_size.cy);
        m_size.cy = m_maxsize.cy;
    }

    if(m_size.cx != size.cx || m_size.cy != size.cy)
    {
        m_vidrect.top = MulDiv(m_vidrect.top, m_size.cx, size.cx);
        m_vidrect.bottom = MulDiv(m_vidrect.bottom, m_size.cx, size.cx);
        m_vidrect.left = MulDiv(m_vidrect.left, m_size.cy, size.cy);
        m_vidrect.right = MulDiv(m_vidrect.right, m_size.cy, size.cy);
    }

    return S_OK;
}

STDMETHODIMP CSubPicImpl::GetDirtyRects( CAtlList<const CRect>& dirtyRectList /*[out]*/ ) const
{
    POSITION pos = m_rectListDirty.GetHeadPosition();
    while(pos!=NULL)
    {
        dirtyRectList.AddTail(m_rectListDirty.GetNext(pos));
    }
    return S_OK;
}


//
// CSubPicAllocatorImpl
//

CSubPicAllocatorImpl::CSubPicAllocatorImpl(SIZE cursize, bool fDynamicWriteOnly, bool fPow2Textures)
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
    if(!ppSubPic)
        return E_POINTER;

    if(!m_pStatic)
    {
        if(!Alloc(true, &m_pStatic) || !m_pStatic)
            return E_OUTOFMEMORY;
    }

    m_pStatic->SetSize(m_cursize, m_curvidrect);

    (*ppSubPic = m_pStatic)->AddRef();

    return S_OK;
}

STDMETHODIMP CSubPicAllocatorImpl::AllocDynamic(ISubPic** ppSubPic)
{
    if(!ppSubPic)
        return E_POINTER;

    if(!Alloc(false, ppSubPic) || !*ppSubPic)
        return E_OUTOFMEMORY;

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
