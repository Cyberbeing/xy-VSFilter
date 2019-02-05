#include "stdafx.h"
#include "SimpleSubpicImpl.h"
#include "ISimpleSubPic.h"
#include "xy_intrinsics.h"
#include "../subtitles/xy_malloc.h"
#include "MemSubPic.h"

//////////////////////////////////////////////////////////////////////////
//
// SimpleSubpic
//

SimpleSubpic::SimpleSubpic( IXySubRenderFrame*sub_render_frame, int alpha_blt_dst_type )
    : CUnknown(NAME("SimpleSubpic"), NULL)
    , m_sub_render_frame(sub_render_frame)
    , m_alpha_blt_dst_type(alpha_blt_dst_type)
{
    ConvertColorSpace();
}

SimpleSubpic::~SimpleSubpic()
{
    for(unsigned i=0;i<m_buffers.GetCount();i++)
        xy_free(m_buffers.GetAt(i));
}

STDMETHODIMP SimpleSubpic::NonDelegatingQueryInterface( REFIID riid, void** ppv )
{
    return
        QI(ISimpleSubPic)
        __super::NonDelegatingQueryInterface(riid, ppv); 
}

STDMETHODIMP SimpleSubpic::AlphaBlt( SubPicDesc* target )
{
    ASSERT(target!=NULL);
    HRESULT hr = S_FALSE;
    int count = m_bitmap.GetCount();
    for(int i=0;i<count;i++)
    {
        switch(target->type)
        {
        case MSP_NV12:
        case MSP_NV21:
            hr = AlphaBltAnv12_Nv12(target, m_bitmap.GetAt(i));
            break;
        case MSP_P010:
        case MSP_P016:
            hr = AlphaBltAnv12_P010(target, m_bitmap.GetAt(i));
            break;
        default:
            hr = AlphaBlt(target, m_bitmap.GetAt(i));
            break;
        }
        
        if (FAILED(hr))
        {
            return hr;
        }
    }

    return hr;
}

HRESULT SimpleSubpic::AlphaBltAnv12_P010( SubPicDesc* target, const Bitmap& src )
{
    //fix me: check colorspace and log error
    SubPicDesc dst = *target; // copy, because we might modify it

    CRect rd(src.pos, src.size);
    if(dst.h < 0)
    {
        dst.h = -dst.h;
        rd.bottom = dst.h - rd.bottom;
        rd.top = dst.h - rd.top;
    }

    int w = src.size.cx, h = src.size.cy;
    bool bottom_down = rd.top > rd.bottom;

    BYTE* d = NULL;
    BYTE* dUV = NULL;
    if(!bottom_down)
    {
        d = reinterpret_cast<BYTE*>(dst.bits) + dst.pitch*rd.top + rd.left*2;
        dUV = reinterpret_cast<BYTE*>(dst.bits) + dst.pitch*dst.h + dst.pitch*rd.top/2 + rd.left*2;
    }
    else
    {
        d = reinterpret_cast<BYTE*>(dst.bits) + dst.pitch*(rd.top-1) + rd.left*2;
        dUV = reinterpret_cast<BYTE*>(dst.bits) + dst.pitch*dst.h + dst.pitch*(rd.top/2-1) + rd.left*2;
        dst.pitch = -dst.pitch;
    }
    ASSERT(dst.pitchUV==0 || dst.pitchUV==abs(dst.pitch));

    enum PLANS{A=0,Y,UV};
    const BYTE* sa = reinterpret_cast<const BYTE*>(src.extra.plans[A]);
    const BYTE* sy = reinterpret_cast<const BYTE*>(src.extra.plans[Y]);
    const BYTE* s_uv = reinterpret_cast<const BYTE*>(src.extra.plans[UV]);
    return CMemSubPic::AlphaBltAnv12_P010(sa, sy, s_uv, src.pitch, d, dUV, dst.pitch, w, h);
}

HRESULT SimpleSubpic::AlphaBltAnv12_Nv12( SubPicDesc* target, const Bitmap& src )
{
    //fix me: check colorspace and log error
    SubPicDesc dst = *target; // copy, because we might modify it

    CRect rd(src.pos, src.size);
    if(dst.h < 0)
    {
        dst.h = -dst.h;
        rd.bottom = dst.h - rd.bottom;
        rd.top = dst.h - rd.top;
    }

    int w = src.size.cx, h = src.size.cy;
    bool bottom_down = rd.top > rd.bottom;

    BYTE* d = NULL;
    BYTE* dUV = NULL;
    if (!bottom_down)
    {
        d = reinterpret_cast<BYTE*>(dst.bits) + dst.pitch*rd.top + rd.left;
        dUV = reinterpret_cast<BYTE*>(dst.bits) + dst.pitch*dst.h + dst.pitch*rd.top/2 + rd.left;
    }
    else
    {
        d = reinterpret_cast<BYTE*>(dst.bits) + dst.pitch*(rd.top-1) + rd.left;
        dUV = reinterpret_cast<BYTE*>(dst.bits) + dst.pitch*dst.h + dst.pitch*(rd.top/2-1) + rd.left;
        dst.pitch = -dst.pitch;
    }
    ASSERT(dst.pitchUV==0 || dst.pitchUV==abs(dst.pitch));

    enum PLANS{A=0,Y,UV};
    const BYTE* sa = reinterpret_cast<const BYTE*>(src.extra.plans[A]);
    const BYTE* sy = reinterpret_cast<const BYTE*>(src.extra.plans[Y]);
    const BYTE* s_uv = reinterpret_cast<const BYTE*>(src.extra.plans[UV]);
    return CMemSubPic::AlphaBltAnv12_Nv12(sa, sy, s_uv, src.pitch, d, dUV, dst.pitch, w, h);
}

HRESULT SimpleSubpic::AlphaBlt( SubPicDesc* target, const Bitmap& src )
{
    SubPicDesc dst = *target; // copy, because we might modify it

    CRect rd(src.pos, src.size);
    if(dst.h < 0)
    {
        dst.h = -dst.h;
        rd.bottom = dst.h - rd.bottom;
        rd.top = dst.h - rd.top;
    }

    int w = src.size.cx, h = src.size.cy;
    const BYTE* s = reinterpret_cast<const BYTE*>(src.pixels);
    BYTE* d = reinterpret_cast<BYTE*>(dst.bits) + dst.pitch*rd.top + ((rd.left*dst.bpp)>>3);

    if(rd.top > rd.bottom)
    {
        if(dst.type == MSP_RGB32 || dst.type == MSP_RGB24
            || dst.type == MSP_RGB16 || dst.type == MSP_RGB15
            || dst.type == MSP_YUY2 || dst.type == MSP_AYUV)
        {
            d = reinterpret_cast<BYTE*>(dst.bits) + dst.pitch*(rd.top-1) + (rd.left*dst.bpp>>3);
        }
        else if(dst.type == MSP_YV12 || dst.type == MSP_IYUV)
        {
            d = reinterpret_cast<BYTE*>(dst.bits) + dst.pitch*(rd.top-1) + (rd.left*8>>3);
        }
        else
        {
            return E_NOTIMPL;
        }
        dst.pitch = -dst.pitch;
    }
    DbgLog((LOG_TRACE, 5, TEXT("w=%d h=%d"), w, h));
    switch(dst.type)
    {
    case MSP_RGBA:
        for(int j = 0; j < h; j++, s += src.pitch, d += dst.pitch)
        {
            const BYTE* s2 = s;
            const BYTE* s2end = s2 + w*4;
            DWORD* d2 = reinterpret_cast<DWORD*>(d);
            for(; s2 < s2end; s2 += 4, d2++)
            {
                if(s2[3] < 0xff)
                {
                    DWORD bd =0x00000100 -( (DWORD) s2[3]);
                    DWORD B = ((*((DWORD*)s2)&0x000000ff)<<8)/bd;
                    DWORD V = ((*((DWORD*)s2)&0x0000ff00)/bd)<<8;
                    DWORD R = (((*((DWORD*)s2)&0x00ff0000)>>8)/bd)<<16;
                    *d2 = B | V | R
                        | (0xff000000-(*((DWORD*)s2)&0xff000000))&0xff000000;
                }
            }
        }
        break;
    case MSP_RGB32:
        for(int j = 0; j < h; j++, s += src.pitch, d += dst.pitch)
        {
            const BYTE* s2 = s;
            const BYTE* s2end = s2 + w*4;
            DWORD* d2 = (DWORD*)d;
            for(; s2 < s2end; s2 += 4, d2++)
            {
                if(s2[3] < 0xff)
                {
                    *d2 = (((((*d2&0x00ff00ff)*s2[3])>>8) + (*((DWORD*)s2)&0x00ff00ff))&0x00ff00ff)
                        | (((((*d2&0x0000ff00)*s2[3])>>8) + (*((DWORD*)s2)&0x0000ff00))&0x0000ff00);
                }
            }
        }
        break;
    case MSP_AYUV:
        for(int j = 0; j < h; j++, s += src.pitch, d += dst.pitch)
        {
            const BYTE* s2 = s;
            const BYTE* s2end = s2 + w*4;
            DWORD* d2 = (DWORD*)d;
            for(; s2 < s2end; s2 += 4, d2++)
            {
                if(s2[3] < 0xff)
                {
                    *d2 = (((((*d2&0x00ff00ff)*s2[3])>>8) + (*((DWORD*)s2)&0x00ff00ff))&0x00ff00ff)
                        | (((((*d2&0x0000ff00)*s2[3])>>8) + (*((DWORD*)s2)&0x0000ff00))&0x0000ff00)
                        | (*d2&0xff000000);
                }
            }
        }
        break;
    case MSP_RGB24:
        for(int j = 0; j < h; j++, s += src.pitch, d += dst.pitch)
        {
            const BYTE* s2 = s;
            const BYTE* s2end = s2 + w*4;
            BYTE* d2 = d;
            for(; s2 < s2end; s2 += 4, d2 += 3)
            {
                if(s2[3] < 0xff)
                {
                    d2[0] = ((d2[0]*s2[3])>>8) + s2[0];
                    d2[1] = ((d2[1]*s2[3])>>8) + s2[1];
                    d2[2] = ((d2[2]*s2[3])>>8) + s2[2];
                }
            }
        }
        break;
    case MSP_RGB16:
        for(int j = 0; j < h; j++, s += src.pitch, d += dst.pitch)
        {
            const BYTE* s2 = s;
            const BYTE* s2end = s2 + w*4;
            WORD* d2 = reinterpret_cast<WORD*>(d);
            for(; s2 < s2end; s2 += 4, d2++)
            {
                if(s2[3] < 0x1f)
                {
                    *d2 = (WORD)((((((*d2&0xf81f)*s2[3])>>5) + (*(DWORD*)s2&0xf81f))&0xf81f)
                        | (((((*d2&0x07e0)*s2[3])>>5) + (*(DWORD*)s2&0x07e0))&0x07e0));
                    /*					*d2 = (WORD)((((((*d2&0xf800)*s2[3])>>8) + (*(DWORD*)s2&0xf800))&0xf800)
                    | (((((*d2&0x07e0)*s2[3])>>8) + (*(DWORD*)s2&0x07e0))&0x07e0)
                    | (((((*d2&0x001f)*s2[3])>>8) + (*(DWORD*)s2&0x001f))&0x001f));
                    */
                }
            }
        }
        break;
    case MSP_RGB15:
        for(int j = 0; j < h; j++, s += src.pitch, d += dst.pitch)
        {
            const BYTE* s2 = s;
            const BYTE* s2end = s2 + w*4;
            WORD* d2 = reinterpret_cast<WORD*>(d);
            for(; s2 < s2end; s2 += 4, d2++)
            {
                if(s2[3] < 0x1f)
                {
                    *d2 = (WORD)((((((*d2&0x7c1f)*s2[3])>>5) + (*(DWORD*)s2&0x7c1f))&0x7c1f)
                        | (((((*d2&0x03e0)*s2[3])>>5) + (*(DWORD*)s2&0x03e0))&0x03e0));
                    /*					*d2 = (WORD)((((((*d2&0x7c00)*s2[3])>>8) + (*(DWORD*)s2&0x7c00))&0x7c00)
                    | (((((*d2&0x03e0)*s2[3])>>8) + (*(DWORD*)s2&0x03e0))&0x03e0)
                    | (((((*d2&0x001f)*s2[3])>>8) + (*(DWORD*)s2&0x001f))&0x001f));
                    */
                }
            }
        }
        break;
    case MSP_YUY2:
        CMemSubPic::AlphaBlt_YUY2(w,h,d,dst.pitch,s, src.pitch);
        break;
    case MSP_YV12:
    case MSP_IYUV:
        {
            //dst.pitch = abs(dst.pitch);
            int h2 = h/2;
            if(!dst.pitchUV)
            {
                dst.pitchUV = abs(dst.pitch)/2;
            }
            if(!dst.bitsU || !dst.bitsV)
            {
                dst.bitsU = reinterpret_cast<BYTE*>(dst.bits) + abs(dst.pitch)*dst.h;
                dst.bitsV = dst.bitsU + dst.pitchUV*dst.h/2;
                if(dst.type == MSP_YV12)
                {
                    BYTE* p = dst.bitsU;
                    dst.bitsU = dst.bitsV;
                    dst.bitsV = p;
                }
            }
            BYTE* dd[2];
            dd[0] = dst.bitsU + dst.pitchUV*rd.top/2 + rd.left/2;
            dd[1] = dst.bitsV + dst.pitchUV*rd.top/2 + rd.left/2;
            if(rd.top > rd.bottom)
            {
                dd[0] = dst.bitsU + dst.pitchUV*(rd.top/2-1) + rd.left/2;
                dd[1] = dst.bitsV + dst.pitchUV*(rd.top/2-1) + rd.left/2;
                dst.pitchUV = -dst.pitchUV;
            }

            enum PLANS{A=0,Y,U,V};
            const BYTE* sa = reinterpret_cast<const BYTE*>(src.extra.plans[A]);
            const BYTE* sy = reinterpret_cast<const BYTE*>(src.extra.plans[Y]);
            const BYTE* su = reinterpret_cast<const BYTE*>(src.extra.plans[U]);
            const BYTE* sv = reinterpret_cast<const BYTE*>(src.extra.plans[V]);
            CMemSubPic::AlphaBltYv12Luma( d, dst.pitch, w, h, sy, sa, src.pitch );
            CMemSubPic::AlphaBltYv12Chroma( dd[0], dst.pitchUV, w, h2, su, sa, src.pitch);
            CMemSubPic::AlphaBltYv12Chroma( dd[1], dst.pitchUV, w, h2, sv, sa, src.pitch);
#ifndef _WIN64
            // TODOX64 : fixme!
            _mm_empty();
#endif
        }
        break;
    default:
        return E_NOTIMPL;
        break;
    }

    //emmsҪ40��cpu����
    //__asm emms;
    return S_OK;
}

HRESULT SimpleSubpic::ConvertColorSpace()
{
    int count = 0;
    HRESULT hr = m_sub_render_frame->GetBitmapCount(&count);
    if (FAILED(hr) || count==0)
    {
        return hr;
    }
    int xy_color_space = 0;
    hr = m_sub_render_frame->GetXyColorSpace(&xy_color_space);
    if (FAILED(hr))
    {
        return hr;
    }
    m_bitmap.SetCount(count);
    m_buffers.SetCount(count);
    for (int i=0;i<count;i++)
    {
        m_buffers.GetAt(i) = NULL;//safe

        Bitmap &bitmap = m_bitmap.GetAt(i);
        hr = m_sub_render_frame->GetBitmap(i, &bitmap.id, &bitmap.pos, &bitmap.size, &bitmap.pixels, &bitmap.pitch);
        if (FAILED(hr))
        {
            return hr;
        }
        if (xy_color_space==XY_CS_AYUV_PLANAR)
        {
            hr = m_sub_render_frame->GetBitmapExtra(i, &bitmap.extra);
            if (FAILED(hr))
            {
                return hr;
            }
        }

        int w = bitmap.size.cx, h = bitmap.size.cy;
        if (w<=0 || h<=0)
        {
            continue;
        }

        const BYTE* top = reinterpret_cast<const BYTE*>(bitmap.pixels);
        const BYTE* bottom = top + bitmap.pitch*h;
        if(m_alpha_blt_dst_type == MSP_RGB16)
        {
            ASSERT(xy_color_space==XY_CS_ARGB);

            BYTE* dst = reinterpret_cast<BYTE*>(xy_malloc(bitmap.pitch*h, (bitmap.pos.x*4)&15));
            m_buffers.GetAt(i) = dst;
            bitmap.pixels = dst;
            for(; top < bottom ; top += bitmap.pitch, dst += bitmap.pitch)
            {
                const DWORD* s = reinterpret_cast<const DWORD*>(top);
                const DWORD* e = s + w;
                DWORD* dst2 = reinterpret_cast<DWORD*>(dst);
                for(; s < e; s++, dst2++)
                {
                    *dst2 = ((*s>>3)&0x1f000000)|((*s>>8)&0xf800)|((*s>>5)&0x07e0)|((*s>>3)&0x001f);
                    //				*s = (*s&0xff000000)|((*s>>8)&0xf800)|((*s>>5)&0x07e0)|((*s>>3)&0x001f);
                }
            }            
        }
        else if(m_alpha_blt_dst_type == MSP_RGB15)
        {
            ASSERT(xy_color_space==XY_CS_ARGB);
            
            BYTE* dst = reinterpret_cast<BYTE*>(xy_malloc(bitmap.pitch*h, (bitmap.pos.x*4)&15));
            m_buffers.GetAt(i) = dst;
            bitmap.pixels = dst;
            for(; top < bottom; top += bitmap.pitch, dst += bitmap.pitch)
            {
                const DWORD* s = reinterpret_cast<const DWORD*>(top);
                const DWORD* e = s + w;
                DWORD* dst2 = reinterpret_cast<DWORD*>(dst);
                for(; s < e; s++, dst2++)
                {
                    *dst2 = ((*s>>3)&0x1f000000)|((*s>>9)&0x7c00)|((*s>>6)&0x03e0)|((*s>>3)&0x001f);
                    //				*s = (*s&0xff000000)|((*s>>9)&0x7c00)|((*s>>6)&0x03e0)|((*s>>3)&0x001f);
                }
            }            
        }
        else if(m_alpha_blt_dst_type == MSP_YUY2)
        {
            ASSERT(xy_color_space==XY_CS_AUYV);

            BYTE* dst = reinterpret_cast<BYTE*>(xy_malloc(bitmap.pitch*h, (bitmap.pos.x*4)&15));
            m_buffers.GetAt(i) = dst;            
            memcpy(dst, bitmap.pixels, bitmap.pitch*h);
            bitmap.pixels = dst;
            for(BYTE* tempTop=dst; tempTop < dst+bitmap.pitch*h ; tempTop += bitmap.pitch)
            {
                BYTE* s = tempTop;
                BYTE* e = s + w*4;
                BYTE last_v = s[0], last_u=s[2];
                for(; s < e; s+=8) // AUYV AUYV -> AxYU AxYV
                {
                    BYTE tmp = s[4];
                    s[4] = (last_v + 2*s[0] + s[4] + 2)>>2;
                    last_v = tmp;

                    s[0] = (last_u + 2*s[2] + s[6] + 2)>>2;                    
                    last_u = s[6];
                }
            }
        }
        else if(m_alpha_blt_dst_type == MSP_YV12 || m_alpha_blt_dst_type == MSP_IYUV )
        {
            ASSERT(xy_color_space==XY_CS_AYUV_PLANAR);
            //nothing to do
        }
        else if ( m_alpha_blt_dst_type == MSP_P010 || m_alpha_blt_dst_type == MSP_P016 
            || m_alpha_blt_dst_type == MSP_NV12 )
        {
            ASSERT(xy_color_space==XY_CS_AYUV_PLANAR);
            SubsampleAndInterlace(i, &bitmap, true);
        }
        else if( m_alpha_blt_dst_type == MSP_NV21 )
        {
            ASSERT(xy_color_space==XY_CS_AYUV_PLANAR);
            SubsampleAndInterlace(i, &bitmap, false);
        }
    }
    return S_OK;
}

void SimpleSubpic::SubsampleAndInterlace( int index, Bitmap*bitmap, bool u_first )
{
    ASSERT(bitmap!=NULL);
    //fix me: check alignment and log error
    int w = bitmap->size.cx, h = bitmap->size.cy;
    ASSERT(h%2==0);
    const BYTE* u_start = reinterpret_cast<const BYTE*>(bitmap->extra.plans[2]);
    const BYTE* v_start = reinterpret_cast<const BYTE*>(bitmap->extra.plans[3]);

    BYTE* dst = reinterpret_cast<BYTE*>(xy_malloc(bitmap->pitch*h/2, bitmap->pos.x&15));
    m_buffers.GetAt(index) = dst;
    bitmap->extra.plans[2] = dst;

    if(!u_first)
    {
        const BYTE* tmp = v_start;
        v_start = u_start;
        u_start = tmp;
    }

    //Todo: fix me. 
    //Walkarround for alignment
    if ( ((bitmap->pitch | (intptr_t)u_start | (intptr_t)v_start)&15) == 0 )
    {
        for (int i=0;i<h;i+=2)
        {
            int w16 = w&~15;
            hleft_vmid_subsample_and_interlace_2_line_sse2(dst, u_start, v_start, w16, bitmap->pitch);
            ASSERT(w>0);
            hleft_vmid_subsample_and_interlace_2_line_c(dst+w16, u_start+w16, v_start+w16, w&15, bitmap->pitch, -1);
            u_start += 2*bitmap->pitch;
            v_start += 2*bitmap->pitch;
            dst += bitmap->pitch;
        }
    }
    else
    {
        for (int i=0;i<h;i+=2)
        {
            hleft_vmid_subsample_and_interlace_2_line_c(dst, u_start, v_start, w, bitmap->pitch);
            u_start += 2*bitmap->pitch;
            v_start += 2*bitmap->pitch;
            dst += bitmap->pitch;
        }
    }
}
