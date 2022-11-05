#include "stdafx.h"
#include "xy_bitmap.h"
#include "xy_malloc.h"
#include "../SubPic/ISubPic.h"
#include "../subpic/color_conv_table.h"

XyBitmap::~XyBitmap()
{
    xy_free(bits);
}

XyBitmap * XyBitmap::CreateBitmap( const CRect& target_rect, MemLayout layout )
{
    XyBitmap *result = new XyBitmap();
    if (result==NULL)
    {
        ASSERT(0);
        return result;
    }
    result->type = layout;
    result->x = target_rect.left;
    result->y = target_rect.top;
    result->w = target_rect.Width();
    result->h = target_rect.Height();    
    int w16 = (result->w + 15) & ~15;
        
    switch (result->type)
    {
    case PACK:
        result->pitch = w16*4;
        result->bits = xy_malloc(4*w16*result->h, (result->x*4)&15);
        result->plans[0] = reinterpret_cast<BYTE*>(result->bits);
        break;
    case PLANNA:
        result->bits = xy_malloc(4*w16*result->h, result->x&15);
        result->pitch = w16;
        result->plans[0] = reinterpret_cast<BYTE*>(result->bits);
        result->plans[1] = result->plans[0] + result->pitch * result->h;
        result->plans[2] = result->plans[1] + result->pitch * result->h;
        result->plans[3] = result->plans[2] + result->pitch * result->h;        
        break;
    default:
        ASSERT(0);
        result->bits = NULL;
        break;
    }
    ClearBitmap(result);
    return result;
}

void XyBitmap::ClearBitmap( XyBitmap *bitmap )
{
    if (!bitmap)
        return;
    if (bitmap->type==XyBitmap::PLANNA)
    {
        memset(bitmap->plans[0], 0xFF, bitmap->h * bitmap->pitch);
        memset(bitmap->plans[1], 0, bitmap->h * bitmap->pitch * 3);//assuming the other 2 plans lied right after plan 1
    }
    else
    {
        BYTE * p = bitmap->plans[0];
        for (int i=0;i<bitmap->h;i++, p+=bitmap->pitch)
        {
            memsetd(p, 0xFF000000, bitmap->w*4);
        }        
    }
}

void XyBitmap::AlphaBltPack( SubPicDesc& spd, POINT pos, SIZE size, LPCVOID pixels, int pitch )
{
    ASSERT( spd.type!=MSP_AYUV_PLANAR );
    CRect r(0, 0, spd.w, spd.h);

    int x = pos.x;
    int y = pos.y;
    int w = size.cx;
    int h = size.cy;
    int x_src = 0, y_src = 0;

    if(x < r.left) {x_src = r.left-x; w -= r.left-x; x = r.left;}
    if(y < r.top) {y_src = r.top-y; h -= r.top-y; y = r.top;}
    if(x+w > r.right) w = r.right-x;
    if(y+h > r.bottom) h = r.bottom-y;

    const BYTE* src = reinterpret_cast<const BYTE*>(pixels) + y_src*pitch + x_src*4;

    BYTE* dst = reinterpret_cast<BYTE*>(spd.bits) + spd.pitch * y + ((x*spd.bpp)>>3);

    for(int i=0;i<h;i++, src += pitch, dst += spd.pitch)
    {
        const BYTE* s2 = src;
        const BYTE* s2end = s2 + w*4;
        DWORD* d2 = (DWORD*)dst;
        for(; s2 < s2end; s2 += 4, d2++)
        {
            int tmp = s2[3]+1;
            *d2 = (((((*d2&0x00ff00ff)*tmp)>>8) + (*((DWORD*)s2)&0x00ff00ff))&0x00ff00ff)
                | (((((*d2&0x0000ff00)*tmp)>>8) + (*((DWORD*)s2)&0x0000ff00))&0x0000ff00);
        }
    }
}

void XyBitmap::AlphaBltPlannar( SubPicDesc& spd, POINT pos, SIZE size, const XyPlannerFormatExtra& pixels, int pitch )
{
    ASSERT( spd.type==MSP_AYUV_PLANAR );

    CRect r(0, 0, spd.w, spd.h);

    int x = pos.x;
    int y = pos.y;
    int w = size.cx;
    int h = size.cy;
    int x_src = 0, y_src = 0;

    if(x < r.left) {x_src = r.left-x; w -= r.left-x; x = r.left;}
    if(y < r.top) {y_src = r.top-y; h -= r.top-y; y = r.top;}
    if(x+w > r.right) w = r.right-x;
    if(y+h > r.bottom) h = r.bottom-y;

    BYTE* dst = reinterpret_cast<BYTE*>(spd.bits) + spd.pitch * y + ((x*spd.bpp)>>3);

    const BYTE* src_A = reinterpret_cast<const BYTE*>(pixels.plans[0]) + y_src*pitch + x_src;
    const BYTE* src_Y = reinterpret_cast<const BYTE*>(pixels.plans[1]) + y_src*pitch + x_src;
    const BYTE* src_U = reinterpret_cast<const BYTE*>(pixels.plans[2]) + y_src*pitch + x_src;
    const BYTE* src_V = reinterpret_cast<const BYTE*>(pixels.plans[3]) + y_src*pitch + x_src;

    BYTE* dst_A = dst;
    BYTE* dst_Y = dst_A + spd.pitch*spd.h;
    BYTE* dst_U = dst_Y + spd.pitch*spd.h;
    BYTE* dst_V = dst_U + spd.pitch*spd.h;

    const BYTE*  src_A1 = src_A;
    for (int i=0;i<h;i++, src_A += pitch, dst_A += spd.pitch)
    {
        for (int j=0;j<w;j++)
        {
            dst_A[j] = (dst_A[j]*(src_A[j]+1))>>8;
        }
    }

    src_A = src_A1;
    for (int i=0;i<h;i++, src_A += pitch, src_Y += pitch, dst_Y += spd.pitch)
    {
        for (int j=0;j<w;j++)
        {
            dst_Y[j] = ((dst_Y[j]*(src_A[j]+1))>>8) + src_Y[j];
        }
    }
    src_A = src_A1;
    for (int i=0;i<h;i++, src_A += pitch, src_U += pitch, dst_U += spd.pitch)
    {
        for (int j=0;j<w;j++)
        {
            dst_U[j] = ((dst_U[j]*(src_A[j]+1))>>8) + src_U[j];
        }
    }
    src_A = src_A1;
    for (int i=0;i<h;i++, src_A += pitch, src_V += pitch, dst_V += spd.pitch)
    {
        for (int j=0;j<w;j++)
        {
            dst_V[j] = ((dst_V[j]*(src_A[j]+1))>>8) + src_V[j];
        }
    }
}

//////////////////////////////////////////////////////////////////////////
//
// SubRenderFrame
// 

XySubRenderFrame::XySubRenderFrame()
    : CUnknown(NAME("XySubRenderFrameWrapper"), NULL)
{

}

XySubRenderFrame::~XySubRenderFrame()
{
}

STDMETHODIMP XySubRenderFrame::NonDelegatingQueryInterface( REFIID riid, void** ppv )
{
    return
        QI(IXySubRenderFrame)
        __super::NonDelegatingQueryInterface(riid, ppv);
}

STDMETHODIMP XySubRenderFrame::GetOutputRect( RECT *outputRect )
{
    if (!outputRect)
    {
        return E_POINTER;
    }
    *outputRect = m_output_rect;
    return S_OK;
}

STDMETHODIMP XySubRenderFrame::GetClipRect( RECT *clipRect )
{
    if (!clipRect)
    {
        return E_POINTER;
    }
    *clipRect = m_clip_rect;
    return S_OK;
}

STDMETHODIMP XySubRenderFrame::GetXyColorSpace( int *xyColorSpace )
{
    if (!xyColorSpace)
    {
        return E_POINTER;
    }
    *xyColorSpace = m_xy_color_space;
    return S_OK;
}

STDMETHODIMP XySubRenderFrame::GetBitmapCount( int *count )
{
    if (!count)
    {
        return E_POINTER;
    }
    *count = m_bitmaps.GetCount();
    return S_OK;
}

STDMETHODIMP XySubRenderFrame::GetBitmap( int index, ULONGLONG *id, POINT *position, SIZE *size, LPCVOID *pixels, int *pitch )
{
    if (index<0 || index>=(int)m_bitmaps.GetCount())
    {
        return E_INVALIDARG;
    }
    if (id)
    {
        *id = m_bitmap_ids.GetAt(index);
    }
    const XyBitmap& bitmap = *(m_bitmaps.GetAt(index));
    if (position)
    {
        position->x = bitmap.x;
        position->y = bitmap.y;
    }
    if (size)
    {
        size->cx = bitmap.w;
        size->cy = bitmap.h;
    }
    if (pixels)
    {
        *pixels = bitmap.plans[0];
    }
    if (pitch)
    {
        *pitch = bitmap.pitch;
    }
    return S_OK;
}

STDMETHODIMP XySubRenderFrame::GetBitmapExtra( int index, LPVOID extra_info )
{
    if (index<0 || index>=(int)m_bitmaps.GetCount())
    {
        return E_INVALIDARG;
    }
    if (extra_info && m_xy_color_space == XY_CS_AYUV_PLANAR)
    {
        const XyBitmap& bitmap = *(m_bitmaps.GetAt(index));
        XyPlannerFormatExtra *output = reinterpret_cast<XyPlannerFormatExtra*>(extra_info);

        output->plans[0] = bitmap.plans[0];
        output->plans[1] = bitmap.plans[1];
        output->plans[2] = bitmap.plans[2];
        output->plans[3] = bitmap.plans[3];
    }
    return S_OK;
}

//////////////////////////////////////////////////////////////////////////
//
// XySubRenderFrameCreater
// 

XySubRenderFrameCreater* XySubRenderFrameCreater::GetDefaultCreater()
{
    static XySubRenderFrameCreater s_default_creater;
    return &s_default_creater;
}

HRESULT XySubRenderFrameCreater::SetOutputRect( const RECT& output_rect )
{
    m_output_rect = output_rect;
    return S_OK;
}

HRESULT XySubRenderFrameCreater::SetClipRect( const RECT& clip_rect )
{
    m_clip_rect = clip_rect;
    return S_OK;
}

HRESULT XySubRenderFrameCreater::SetColorSpace( XyColorSpace color_space )
{
    m_xy_color_space = color_space;
    switch (m_xy_color_space)
    {
    case XY_CS_AYUV_PLANAR:
        m_bitmap_layout = XyBitmap::PLANNA;
        break;
    default:
        m_bitmap_layout = XyBitmap::PACK;
        break;
    }
    return S_OK;
}

HRESULT XySubRenderFrameCreater::GetOutputRect( RECT *output_rect )
{
    if (!output_rect)
    {
        return S_FALSE;
    }
    *output_rect = m_output_rect;
    return S_OK;
}

HRESULT XySubRenderFrameCreater::GetClipRect( RECT *clip_rect )
{
    if (!clip_rect)
    {
        return S_FALSE;
    }
    *clip_rect = m_clip_rect;
    return S_OK;
}

HRESULT XySubRenderFrameCreater::GetColorSpace( XyColorSpace *color_space )
{
    if (!color_space)
    {
        return S_FALSE;
    }
    *color_space = m_xy_color_space;
    return S_OK;
}

XySubRenderFrame* XySubRenderFrameCreater::NewXySubRenderFrame( UINT bitmap_count )
{
    XySubRenderFrame *result = new XySubRenderFrame();
    ASSERT(result);
    result->m_output_rect = m_output_rect;
    result->m_clip_rect = m_clip_rect;
    result->m_xy_color_space = m_xy_color_space;
    result->m_bitmap_ids.SetCount(bitmap_count);
    result->m_bitmaps.SetCount(bitmap_count);
    return result;
}

XyBitmap* XySubRenderFrameCreater::CreateBitmap( const RECT& target_rect )
{
    return XyBitmap::CreateBitmap(target_rect, m_bitmap_layout);
}

DWORD XySubRenderFrameCreater::TransColor( DWORD argb )
{
    switch(m_xy_color_space)
    {
    case XY_CS_AYUV_PLANAR:
    case XY_CS_AYUV:
        return ColorConvTable::Argb2Ayuv(argb);
        break;
    case XY_CS_AUYV:
        return ColorConvTable::Argb2Auyv(argb);
        break;
    case XY_CS_ARGB:
        return argb;
        break;
    }
    return argb;
}

