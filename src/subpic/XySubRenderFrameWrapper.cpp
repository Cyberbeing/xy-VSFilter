#include "stdafx.h"
#include "XySubRenderFrameWrapper.h"

XySubRenderFrameWrapper::XySubRenderFrameWrapper( CMemSubPic * inner_obj, const CRect& output_rect
    , const CRect& clip_rect, ULONGLONG id, HRESULT *phr/*=NULL*/ )
    : CUnknown(NAME("XySubRenderFrameWrapper"), NULL, phr)
    , m_inner_obj(inner_obj)
    , m_output_rect(output_rect)
    , m_clip_rect(clip_rect)
    , m_id(id)
{
    ASSERT(m_inner_obj);
    if (!m_inner_obj && phr)
    {
        *phr = E_FAIL;
    }
}

STDMETHODIMP XySubRenderFrameWrapper::NonDelegatingQueryInterface( REFIID riid, void** ppv )
{
    return
        QI(IXySubRenderFrame)
        __super::NonDelegatingQueryInterface(riid, ppv);
}

STDMETHODIMP XySubRenderFrameWrapper::GetOutputRect( RECT *outputRect )
{
    if (!outputRect)
    {
        return E_INVALIDARG;
    }
    *outputRect = m_output_rect;
    return S_OK;
}

STDMETHODIMP XySubRenderFrameWrapper::GetClipRect( RECT *clipRect )
{
    if (!clipRect)
    {
        return E_INVALIDARG;
    }
    *clipRect = m_clip_rect;
    return S_OK;
}

STDMETHODIMP XySubRenderFrameWrapper::GetXyColorSpace( int *xyColorSpace )
{
    if (!xyColorSpace)
    {
        return S_FALSE;
    }
    *xyColorSpace = XY_CS_ARGB_F;
    return S_OK;
}

STDMETHODIMP XySubRenderFrameWrapper::GetBitmapCount( int *count )
{
    if (!count)
    {
        return S_FALSE;
    }
    *count = 1;
    return S_OK;
}

STDMETHODIMP XySubRenderFrameWrapper::GetBitmap( int index, ULONGLONG *id, POINT *position, SIZE *size
    , LPCVOID *pixels, int *pitch )
{
    if (index!=0)
    {
        return E_INVALIDARG;
    }
    if (!id && !position && !size && !pixels && !pitch)
    {
        return S_FALSE;
    }
    if (id)
    {
        *id = m_id;
    }
    CRect dirty_rect;
    HRESULT hr = m_inner_obj->GetDirtyRect(&dirty_rect);
    if (FAILED(hr))
    {
        return hr;
    }
    if (position)
    {
        *position = dirty_rect.TopLeft();
    }
    if (size)
    {
        *size = dirty_rect.Size();
    }
    if (pitch)
    {
        *pitch = m_inner_obj->m_spd.pitch;
    }
    if (pixels)
    {
        ASSERT(m_inner_obj->m_spd.bpp==32);
        *pixels = (BYTE*)(m_inner_obj->m_spd.bits) + dirty_rect.top * m_inner_obj->m_spd.pitch 
            + dirty_rect.left * 32;
    }
    return S_OK;
}

STDMETHODIMP XySubRenderFrameWrapper::GetBitmapExtra( int index, LPVOID extra_info )
{
    if (index!=0)
    {
        return E_INVALIDARG;
    }
    return E_NOTIMPL;
}

