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

#include "stdafx.h"
#include "MemSubPic.h"
#include "color_conv_table.h"

#define AVERAGE_4_PIX_INTRINSICS(m128_1, m128_2) \
    m128_1 = _mm_avg_epu8(m128_1, m128_2); \
    m128_2 = _mm_slli_epi16(m128_1, 8); \
    m128_1 = _mm_srli_epi16(m128_1, 8); \
    m128_2 = _mm_srli_epi16(m128_2, 8); \
    m128_1 = _mm_avg_epu8(m128_1, m128_2);

static __forceinline void pix_alpha_blend_yv12_luma_sse2(byte* dst, const byte* alpha, const byte* sub)
{
    __m128i dst128 = _mm_load_si128( reinterpret_cast<const __m128i*>(dst) );
    __m128i alpha128 = _mm_load_si128( reinterpret_cast<const __m128i*>(alpha) );
    __m128i sub128 = _mm_load_si128( reinterpret_cast<const __m128i*>(sub) );
    __m128i zero = _mm_setzero_si128();

    __m128i ones = _mm_cmpeq_epi32(ones,ones);
    ones = _mm_cmpeq_epi8(ones,alpha128);

    __m128i dst_lo128 = _mm_unpacklo_epi8(dst128, zero);
    __m128i alpha_lo128 = _mm_unpacklo_epi8(alpha128, zero);

    __m128i ones2 = _mm_unpacklo_epi8(ones, zero);    

    dst_lo128 = _mm_mullo_epi16(dst_lo128, alpha_lo128);
    dst_lo128 = _mm_adds_epu16(dst_lo128, ones2);
    dst_lo128 = _mm_srli_epi16(dst_lo128, 8);

    dst128 = _mm_unpackhi_epi8(dst128, zero);
    alpha128 = _mm_unpackhi_epi8(alpha128, zero);

    ones2 = _mm_unpackhi_epi8(ones, zero);

    dst128 = _mm_mullo_epi16(dst128, alpha128);
    dst128 = _mm_adds_epu16(dst128, ones2);
    dst128 = _mm_srli_epi16(dst128, 8);
    dst_lo128 = _mm_packus_epi16(dst_lo128, dst128);

    dst_lo128 = _mm_adds_epu8(dst_lo128, sub128);
    _mm_store_si128( reinterpret_cast<__m128i*>(dst), dst_lo128 );
}

/***
 * output not exactly identical to pix_alpha_blend_yv12_chroma
 */
static __forceinline void pix_alpha_blend_yv12_chroma_sse2(byte* dst, const byte* src, const byte* alpha, int src_pitch)
{
    __m128i zero = _mm_setzero_si128();
    __m128i alpha128_1 = _mm_load_si128( reinterpret_cast<const __m128i*>(alpha) );
    __m128i alpha128_2 = _mm_load_si128( reinterpret_cast<const __m128i*>(alpha+src_pitch) );
    __m128i dst128 = _mm_loadl_epi64( reinterpret_cast<const __m128i*>(dst) );

    __m128i sub128_1 = _mm_load_si128( reinterpret_cast<const __m128i*>(src) );
    __m128i sub128_2 = _mm_load_si128( reinterpret_cast<const __m128i*>(src+src_pitch) );

    AVERAGE_4_PIX_INTRINSICS(alpha128_1, alpha128_2);

    __m128i ones = _mm_cmpeq_epi32(ones, ones);
    ones = _mm_cmpeq_epi8(ones, alpha128_1);
    
    dst128 = _mm_unpacklo_epi8(dst128, zero);
    __m128i dst128_2 = _mm_and_si128(dst128, ones);
    
    dst128 = _mm_mullo_epi16(dst128, alpha128_1);
    dst128 = _mm_adds_epu16(dst128, dst128_2);
    
    dst128 = _mm_srli_epi16(dst128, 8);
        
    AVERAGE_4_PIX_INTRINSICS(sub128_1, sub128_2);

    dst128 = _mm_adds_epi16(dst128, sub128_1);    
    dst128 = _mm_packus_epi16(dst128, dst128);
    
    _mm_storel_epi64( reinterpret_cast<__m128i*>(dst), dst128 );
}

static __forceinline void pix_alpha_blend_yv12_chroma(byte* dst, const byte* src, const byte* alpha, int src_pitch)
{
    unsigned int ia = (alpha[0]+alpha[1]+
        alpha[0+src_pitch]+alpha[1+src_pitch])>>2;
    if(ia!=0xff)
    {
        *dst= (((*dst)*ia)>>8) + ((src[0]        +src[1]+
            src[src_pitch]+src[1+src_pitch] )>>2);
    }    
}

static void AlphaBltYv12Luma(byte* dst, int dst_pitch,
    int w, int h,
    const byte* sub, const byte* alpha, int sub_pitch)
{
    if( ((reinterpret_cast<intptr_t>(alpha) | static_cast<intptr_t>(sub_pitch) |
        reinterpret_cast<intptr_t>(dst) | static_cast<intptr_t>(dst_pitch) ) & 15 )==0 )
    {
        for(int i=0; i<h; i++, dst += dst_pitch, alpha += sub_pitch, sub += sub_pitch)
        {
            const BYTE* sa = alpha;
            const BYTE* s2 = sub;
            const BYTE* s2end_mod16 = s2 + (w&~15);
            const BYTE* s2end = s2 + w;
            BYTE* d2 = dst;

            for(; s2 < s2end_mod16; s2+=16, sa+=16, d2+=16)
            {
                pix_alpha_blend_yv12_luma_sse2(d2, sa, s2);                        
            }
            for(; s2 < s2end; s2++, sa++, d2++)
            {
                if(sa[0] < 0xff)
                {                    
                    d2[0] = ((d2[0]*sa[0])>>8) + s2[0];
                }
            }
        }
    }
    else //fix me: only a workaround for non-mod-16 size video
    {
        for(int i=0; i<h; i++, dst += dst_pitch, alpha += sub_pitch, sub += sub_pitch)
        {
            const BYTE* sa = alpha;
            const BYTE* s2 = sub;
            const BYTE* s2end_mod16 = s2 + (w&~15);
            const BYTE* s2end = s2 + w;
            BYTE* d2 = dst;
            for(; s2 < s2end; s2+=1, sa+=1, d2+=1)
            {
                if(sa[0] < 0xff)
                {
                    //					d2[0] = (((d2[0]-0x10)*s2[3])>>8) + s2[1];  
                    d2[0] = ((d2[0]*sa[0])>>8) + s2[0];
                }
            }
        }
    }
}

static void AlphaBltYv12Chroma(byte* dst, int dst_pitch,
    int w, int chroma_h,
    const byte* sub_chroma, const byte* alpha, int sub_pitch)
{
    if( ((reinterpret_cast<intptr_t>(sub_chroma) |
        //reinterpret_cast<intptr_t>(dst) | 
        reinterpret_cast<intptr_t>(alpha) | static_cast<intptr_t>(sub_pitch) 
        //| (static_cast<intptr_t>(dst_pitch)&7) 
        ) & 15 )==0 )
    {
        int pitch = sub_pitch;
        for(int j = 0; j < chroma_h; j++, sub_chroma += sub_pitch*2, alpha += sub_pitch*2, dst += dst_pitch)
        {
            const BYTE* s2 = sub_chroma;
            const BYTE* sa2 = alpha;
            const BYTE* s2end_mod16 = s2 + (w&~15);
            const BYTE* s2end = s2 + w;
            BYTE* d2 = dst;

            for(; s2 < s2end_mod16; s2 += 16, sa2 += 16, d2+=8)
            {
                pix_alpha_blend_yv12_chroma_sse2(d2, s2, sa2, sub_pitch);
            }
            for(; s2 < s2end; s2+=2, sa2+=2, d2++)
            {
                pix_alpha_blend_yv12_chroma(d2, s2, sa2, sub_pitch);
            }
        }
    }
    else//fix me: only a workaround for non-mod-16 size video
    {
        for(int j = 0; j < chroma_h; j++, sub_chroma += sub_pitch*2, alpha += sub_pitch*2, dst += dst_pitch)
        {
            const BYTE* s2 = sub_chroma;
            const BYTE* sa2 = alpha;
            const BYTE* s2end_mod16 = s2 + (w&~15);
            const BYTE* s2end = s2 + w;
            BYTE* d2 = dst;
            for(; s2 < s2end; s2 += 2, sa2 += 2, d2++)
            {                            
                pix_alpha_blend_yv12_chroma(d2, s2, sa2, sub_pitch);
            }
        }
    }
}

//
// CMemSubPic
//

CMemSubPic::CMemSubPic(SubPicDesc& spd, int alpha_blt_dst_type)
    : m_spd(spd), m_alpha_blt_dst_type(alpha_blt_dst_type)
{
    m_maxsize.SetSize(spd.w, spd.h);
    //	m_rcDirty.SetRect(0, 0, spd.w, spd.h);
    CRect allSpd(0,0,spd.w, spd.h);
    m_rectListDirty.AddTail(allSpd);
}

CMemSubPic::~CMemSubPic()
{
    delete [] m_spd.bits, m_spd.bits = NULL;
}

// ISubPic

STDMETHODIMP_(void*) CMemSubPic::GetObject() const
{
    return (void*)&m_spd;
}

STDMETHODIMP CMemSubPic::GetDesc(SubPicDesc& spd) const
{
    spd.type = m_spd.type;
    spd.w = m_size.cx;
    spd.h = m_size.cy;
    spd.bpp = m_spd.bpp;
    spd.pitch = m_spd.pitch;
    spd.bits = m_spd.bits;
    spd.bitsU = m_spd.bitsU;
    spd.bitsV = m_spd.bitsV;
    spd.vidrect = m_vidrect;
    return S_OK;
}

STDMETHODIMP CMemSubPic::CopyTo(ISubPicEx* pSubPic)
{
    HRESULT hr;
	if(FAILED(hr = __super::CopyTo(pSubPic))) {
        return hr;
	}

	SubPicDesc src, dst;
	if(FAILED(GetDesc(src)) || FAILED(pSubPic->GetDesc(dst))) {
        return E_FAIL;
    }
    while(!m_rectListDirty.IsEmpty())
    {
        CRect& cRect = m_rectListDirty.GetHead();
        int w = cRect.Width(), h = cRect.Height();
        BYTE* s = (BYTE*)src.bits + src.pitch*cRect.top + cRect.left*4;
        BYTE* d = (BYTE*)dst.bits + dst.pitch*cRect.top + cRect.left*4;
        for(int j = 0; j < h; j++, s += src.pitch, d += dst.pitch)
            memcpy(d, s, w*4);
    }
    return S_OK;
}

STDMETHODIMP CMemSubPic::ClearDirtyRect(DWORD color)
{
    if(m_rectListDirty.IsEmpty()) {
        return S_OK;
	}
    while(!m_rectListDirty.IsEmpty())
    {
        //pDirtyRect = m_rectListDirty.RemoveHead();
        CRect& dirtyRect = m_rectListDirty.RemoveTail();
        BYTE* p = (BYTE*)m_spd.bits + m_spd.pitch*(dirtyRect.top) + dirtyRect.left*(m_spd.bpp>>3);
        int w = dirtyRect.Width();
        if(m_spd.type!=MSP_AY11)
        {
            for(int j = 0, h = dirtyRect.Height(); j < h; j++, p += m_spd.pitch)
            {
#ifdef _WIN64
				memsetd(p, color, w*4); // nya
#else
                __asm
                {
                        mov eax, color
                        mov ecx, w
                        mov edi, p
                        cld
                        rep stosd
                }
				
#endif
            }
        }
        else
        {
            ///TODO:
            ///FIX ME
            for(int j = 0, h = dirtyRect.Height(); j < h; j++, p += m_spd.pitch)
            {
                //        memsetd(p, 0, m_rcDirty.Width());
                //DbgLog((LOG_TRACE, 3, "w:%d", w));
                //w = pDirtyRect->Width();
                memset(p, 0xFF, w);
                memset(p+m_spd.h*m_spd.pitch, 0, w);
                memset(p+m_spd.h*m_spd.pitch*2, 0, w);
                memset(p+m_spd.h*m_spd.pitch*3, 0, w);
            }
        }
    }
	m_rectListDirty.RemoveAll();
    return S_OK;
}

STDMETHODIMP CMemSubPic::Lock(SubPicDesc& spd)
{
    return GetDesc(spd);
}

STDMETHODIMP CMemSubPic::Unlock( CAtlList<CRect>* dirtyRectList )
{
    int src_type = m_spd.type;
    int dst_type = m_alpha_blt_dst_type;
    if( (src_type==MSP_RGBA && (dst_type == MSP_RGB32 ||
                                dst_type == MSP_RGB24 ||
                                dst_type == MSP_RGB16 ||
                                dst_type == MSP_RGB15)) 
        ||
        (src_type==MSP_AUYV &&  dst_type == MSP_YUY2)//ToDo: fix me MSP_AYUV
        ||
        (src_type==MSP_AYUV &&  dst_type == MSP_AYUV) 
        ||
        (src_type==MSP_AY11 && (dst_type == MSP_IYUV ||
                                dst_type == MSP_YV12 ||
                                dst_type == MSP_P010 ||
                                dst_type == MSP_P016)))
    {
        return UnlockOther(dirtyRectList);        
    }
    else if(src_type==MSP_RGBA && (dst_type == MSP_YUY2 ||  
                                   dst_type == MSP_AYUV || //ToDo: fix me MSP_AYUV
                                   dst_type == MSP_IYUV ||
                                   dst_type == MSP_YV12))
    {
        return UnlockRGBA_YUV(dirtyRectList);
    }
    return E_NOTIMPL;
}

STDMETHODIMP CMemSubPic::UnlockOther(CAtlList<CRect>* dirtyRectList)
{
    SetDirtyRectEx(dirtyRectList);
    if(m_rectListDirty.IsEmpty()) {
        return S_OK;
    }

    POSITION pos = m_rectListDirty.GetHeadPosition();
    while(pos!=NULL)
    {
        const CRect& cRect = m_rectListDirty.GetNext(pos);
        int w = cRect.Width(), h = cRect.Height();
        BYTE* top = (BYTE*)m_spd.bits + m_spd.pitch*(cRect.top) + cRect.left*4;
        BYTE* bottom = top + m_spd.pitch*h;
        if(m_alpha_blt_dst_type == MSP_RGB16)
        {
            for(; top < bottom ; top += m_spd.pitch)
            {
                DWORD* s = (DWORD*)top;
                DWORD* e = s + w;
                for(; s < e; s++)
                {
                    *s = ((*s>>3)&0x1f000000)|((*s>>8)&0xf800)|((*s>>5)&0x07e0)|((*s>>3)&0x001f);
                    //				*s = (*s&0xff000000)|((*s>>8)&0xf800)|((*s>>5)&0x07e0)|((*s>>3)&0x001f);
                }
            }
        }
        else if(m_alpha_blt_dst_type == MSP_RGB15)
        {
            for(; top < bottom; top += m_spd.pitch)
            {
                DWORD* s = (DWORD*)top;
                DWORD* e = s + w;
                for(; s < e; s++)
                {
                    *s = ((*s>>3)&0x1f000000)|((*s>>9)&0x7c00)|((*s>>6)&0x03e0)|((*s>>3)&0x001f);
                    //				*s = (*s&0xff000000)|((*s>>9)&0x7c00)|((*s>>6)&0x03e0)|((*s>>3)&0x001f);
                }
            }
        }
        else if(m_alpha_blt_dst_type == MSP_YUY2)
        {
            XY_DO_ONCE( xy_logger::write_file("G:\\b1_ul", top, m_spd.pitch*(h-1)) );

            for(BYTE* tempTop=top; tempTop < bottom ; tempTop += m_spd.pitch)
            {
                BYTE* s = tempTop;
                BYTE* e = s + w*4;
                for(; s < e; s+=8) // AUYV AUYV -> AxYU AxYV
                {
                    s[4] = (s[0] + s[4])>>1;
                    s[0] = (s[2] + s[6])>>1;
                }
            }

            XY_DO_ONCE( xy_logger::write_file("G:\\a1_ul", top, m_spd.pitch*(h-1)) );
        }
        else if(m_alpha_blt_dst_type == MSP_YV12 || m_alpha_blt_dst_type == MSP_IYUV || m_alpha_blt_dst_type == MSP_YV12 
             || m_alpha_blt_dst_type == MSP_P010 || m_alpha_blt_dst_type == MSP_P016)
        {
            //nothing to do
        }
    }
    return S_OK;
}

STDMETHODIMP CMemSubPic::UnlockRGBA_YUV(CAtlList<CRect>* dirtyRectList)
{
    SetDirtyRectEx(dirtyRectList);
    if(m_rectListDirty.IsEmpty()) {
        return S_OK;
    }
    
    const ColorConvTable *conv_table = ColorConvTable::GetDefaultColorConvTable();
    const int *c2y_yb = conv_table->c2y_yb;
    const int *c2y_yg = conv_table->c2y_yg;
    const int *c2y_yr = conv_table->c2y_yr;
    const int cy_cy2 = conv_table->cy_cy2;
    const int c2y_cu = conv_table->c2y_cu;
    const int c2y_cv = conv_table->c2y_cv;
    const int cy_cy = conv_table->cy_cy;
    const unsigned char* Clip = conv_table->Clip;

    POSITION pos = m_rectListDirty.GetHeadPosition();
    while(pos!=NULL)
    {
        const CRect& cRect = m_rectListDirty.GetNext(pos);
        int w = cRect.Width(), h = cRect.Height();

        BYTE* top = (BYTE*)m_spd.bits + m_spd.pitch*cRect.top + cRect.left*4;
        BYTE* bottom = top + m_spd.pitch*h;

        if(m_alpha_blt_dst_type == MSP_YUY2 || m_alpha_blt_dst_type == MSP_YV12 || m_alpha_blt_dst_type == MSP_IYUV) {
            for(; top < bottom ; top += m_spd.pitch) {
                BYTE* s = top;
                BYTE* e = s + w*4;
                for(; s < e; s+=8) { // ARGB ARGB -> AxYU AxYV
                    if((s[3]+s[7]) < 0x1fe) {
                        int a = 0x200 - (s[3]+s[7]);
                        a <<= 7;
                        // 0 <= a <= 0x10000
                        s[1] = (c2y_yb[s[0]] + c2y_yg[s[1]] + c2y_yr[s[2]] + 0x10*a  + 0x8000) >> 16;
                        s[5] = (c2y_yb[s[4]] + c2y_yg[s[5]] + c2y_yr[s[6]] + 0x10*a  + 0x8000) >> 16;

                        int scaled_y = (s[1]+s[5]-32) * cy_cy2;

                        s[0] = Clip[(((((s[0]+s[4])<<15) - scaled_y) >> 10) * c2y_cu + 0x80*a + 0x8000) >> 16];
                        s[4] = Clip[(((((s[2]+s[6])<<15) - scaled_y) >> 10) * c2y_cv + 0x80*a + 0x8000) >> 16];
                    } else {
                        s[1] = s[5] = 0;
                        s[0] = s[4] = 0;
                    }
                }
            }
        } 
        else if(m_alpha_blt_dst_type == MSP_AYUV) {
            for(; top < bottom ; top += m_spd.pitch) {
                BYTE* s = top;
                BYTE* e = s + w*4;
                for(; s < e; s+=4) { // ARGB -> AYUV
                    if(s[3] < 0xff) {
                        int a = 0x100 - s[3];
                        a <<= 8;
                        // 0 <= a <= 0x10000

                        int y = (c2y_yb[s[0]] + c2y_yg[s[1]] + c2y_yr[s[2]] + 0x10*a + 0x8000) >> 16;
                        int scaled_y = (y-32) * cy_cy;
                        s[1] = Clip[((((s[0]<<16) - scaled_y) >> 10) * c2y_cu + 0x80*a + 0x8000) >> 16];
                        s[0] = Clip[((((s[2]<<16) - scaled_y) >> 10) * c2y_cv + 0x80*a + 0x8000) >> 16];
                        s[2] = y;
                    } else {
                        s[0] = s[1] = 0;
                        s[2] = 0;
                    }
                }
            }
        }
    }
    return S_OK;
}

STDMETHODIMP CMemSubPic::AlphaBlt( const RECT* pSrc, const RECT* pDst, SubPicDesc* pTarget )
{
    if(!pSrc || !pDst || !pTarget) {
        return E_POINTER;
    }
    int src_type = m_spd.type;
    int dst_type = pTarget->type;

    if( (src_type==MSP_RGBA && (dst_type == MSP_RGB32 ||
                                dst_type == MSP_RGB24 ||
                                dst_type == MSP_RGB16 ||
                                dst_type == MSP_RGB15 ||
                                dst_type == MSP_RGBA ||
                                dst_type == MSP_YUY2 ||//ToDo: fix me MSP_RGBA changed into AxYU AxYV after unlock, may be confusing
                                dst_type == MSP_AYUV )) 
        ||
        (src_type==MSP_AUYV &&  dst_type == MSP_YUY2)//ToDo: fix me MSP_AYUV
        ||
        (src_type==MSP_AYUV &&  dst_type == MSP_AYUV) 
        ||
        (src_type==MSP_AY11 && (dst_type == MSP_IYUV ||
                                dst_type == MSP_YV12 ||
                                dst_type == MSP_P010 ||
                                dst_type == MSP_P016 )) )
    {
        return AlphaBltOther(pSrc, pDst, pTarget);        
    }
    else if( src_type==MSP_RGBA && (dst_type == MSP_IYUV ||
                                    dst_type == MSP_YV12)) 
    {
        return AlphaBltAyuv_Yv12(pSrc, pDst, pTarget);
    }
    return E_NOTIMPL;
}

STDMETHODIMP CMemSubPic::AlphaBltOther(const RECT* pSrc, const RECT* pDst, SubPicDesc* pTarget)
{
    const SubPicDesc& src = m_spd;
    SubPicDesc dst = *pTarget; // copy, because we might modify it

    CRect rs(*pSrc), rd(*pDst);
    if(dst.h < 0)
    {
        dst.h = -dst.h;
        rd.bottom = dst.h - rd.bottom;
        rd.top = dst.h - rd.top;
    }
	if(rs.Width() != rd.Width() || rs.Height() != abs(rd.Height())) {
        return E_INVALIDARG;
	}
    int w = rs.Width(), h = rs.Height();
    BYTE* s = (BYTE*)src.bits + src.pitch*rs.top + ((rs.left*src.bpp)>>3);//rs.left*4
    BYTE* d = (BYTE*)dst.bits + dst.pitch*rd.top + ((rd.left*dst.bpp)>>3);
    if(rd.top > rd.bottom)
    {
        if(dst.type == MSP_RGB32 || dst.type == MSP_RGB24
            || dst.type == MSP_RGB16 || dst.type == MSP_RGB15
            || dst.type == MSP_YUY2 || dst.type == MSP_AYUV)
        {
            d = (BYTE*)dst.bits + dst.pitch*(rd.top-1) + (rd.left*dst.bpp>>3);
        }
        else if(dst.type == MSP_YV12 || dst.type == MSP_IYUV)
        {
            d = (BYTE*)dst.bits + dst.pitch*(rd.top-1) + (rd.left*8>>3);
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
            BYTE* s2 = s;
            BYTE* s2end = s2 + w*4;
            DWORD* d2 = (DWORD*)d;
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
    case MSP_AYUV: //ToDo: fix me MSP_VUYA indeed?
        for(int j = 0; j < h; j++, s += src.pitch, d += dst.pitch)
        {
            BYTE* s2 = s;
            BYTE* s2end = s2 + w*4;
            DWORD* d2 = (DWORD*)d;
            for(; s2 < s2end; s2 += 4, d2++)
            {
#ifdef _WIN64
							DWORD ia = 256-s2[3];
							if(s2[3] < 0xff) {
								*d2 = ((((*d2&0x00ff00ff)*s2[3])>>8) + (((*((DWORD*)s2)&0x00ff00ff)*ia)>>8)&0x00ff00ff)
									  | ((((*d2&0x0000ff00)*s2[3])>>8) + (((*((DWORD*)s2)&0x0000ff00)*ia)>>8)&0x0000ff00);
							}
#else
                if(s2[3] < 0xff)
                {
                    *d2 = (((((*d2&0x00ff00ff)*s2[3])>>8) + (*((DWORD*)s2)&0x00ff00ff))&0x00ff00ff)
                        | (((((*d2&0x0000ff00)*s2[3])>>8) + (*((DWORD*)s2)&0x0000ff00))&0x0000ff00);
                }
#endif
            }
        }
        break;
    case MSP_RGB24:
        for(int j = 0; j < h; j++, s += src.pitch, d += dst.pitch)
        {
            BYTE* s2 = s;
            BYTE* s2end = s2 + w*4;
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
            BYTE* s2 = s;
            BYTE* s2end = s2 + w*4;
            WORD* d2 = (WORD*)d;
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
            BYTE* s2 = s;
            BYTE* s2end = s2 + w*4;
            WORD* d2 = (WORD*)d;
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
        for(int j = 0; j < h; j++, s += src.pitch, d += dst.pitch)
        {
            unsigned int ia, c;
            BYTE* s2 = s;
            BYTE* s2end = s2 + w*4;
            DWORD* d2 = (DWORD*)d;
            for(; s2 < s2end; s2 += 8, d2++)
            {
                ia = (s2[3]+s2[7])>>1;
                if(ia < 0xff)
                {
                    //int y1 = (BYTE)(((((*d2&0xff))*s2[3])>>8) + s2[1]); // + y1;
                    //int u = (BYTE)((((((*d2>>8)&0xff))*ia)>>8) + s2[0]); // + u;
                    //int y2 = (BYTE)((((((*d2>>16)&0xff))*s2[7])>>8) + s2[5]); // + y2;                    
                    //int v = (BYTE)((((((*d2>>24)&0xff))*ia)>>8) + s2[4]); // + v;
                    //*d2 = (v<<24)|(y2<<16)|(u<<8)|y1;
                    
                    ia = (ia<<24)|(s2[7]<<16)|(ia<<8)|s2[3];
                    c = (s2[4]<<24)|(s2[5]<<16)|(s2[0]<<8)|s2[1]; // (v<<24)|(y2<<16)|(u<<8)|y1;
                    __asm
                    {
                            mov			edi, d2
                            pxor		mm0, mm0
                            movd		mm2, c
                            punpcklbw	mm2, mm0
                            movd		mm3, [edi]
                            punpcklbw	mm3, mm0
                            movd		mm4, ia
                            punpcklbw	mm4, mm0
                            psraw		mm4, 1          //or else, overflow because psraw shift in sign bit
                            pmullw		mm3, mm4
                            psraw		mm3, 7
                            paddsw		mm3, mm2
                            packuswb	mm3, mm3
                            movd		[edi], mm3
                    };
                }
            }
        }
        __asm emms;
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
                dst.bitsU = (BYTE*)dst.bits + abs(dst.pitch)*dst.h;
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

            BYTE* src_origin= (BYTE*)src.bits + src.pitch*rs.top + rs.left;            

            BYTE* ss[2];
            ss[0] = src_origin + src.pitch*src.h*2;//U
            ss[1] = src_origin + src.pitch*src.h*3;//V

            AlphaBltYv12Luma( d, dst.pitch, w, h, src_origin + src.pitch*src.h, src_origin, src.pitch );

            AlphaBltYv12Chroma( dd[0], dst.pitchUV, w, h2, ss[0], src_origin, src.pitch);
            AlphaBltYv12Chroma( dd[1], dst.pitchUV, w, h2, ss[1], src_origin, src.pitch);

            __asm emms;
        }
        break;
    case MSP_P010:
    case MSP_P016:
        {
            //dst.pitch = abs(dst.pitch);
            int h2 = h/2;
            if(!dst.pitchUV)
            {
                dst.pitchUV = abs(dst.pitch);
            }
            if(!dst.bitsU || !dst.bitsV)
            {
                dst.bitsU = (BYTE*)dst.bits + abs(dst.pitch)*dst.h;
                dst.bitsV = dst.bitsU + 2;
            }
            BYTE* ddUV = dst.bitsU + dst.pitchUV*rd.top/2 + rd.left*2;
            if(rd.top > rd.bottom)
            {
                ddUV = dst.bitsU + dst.pitchUV*(rd.top/2-1) + rd.left*2;
                dst.pitchUV = -dst.pitchUV;
            }

            BYTE* src_origin= (BYTE*)src.bits + src.pitch*rs.top + rs.left;
            BYTE *s = src_origin;

            BYTE* ss[2];
            ss[0] = src_origin + src.pitch*src.h*2;//U
            ss[1] = src_origin + src.pitch*src.h*3;//V

            // equivalent:                
            //   if( (reinterpret_cast<intptr_t>(s2)&15)==0 && (reinterpret_cast<intptr_t>(sa)&15)==0 
            //     && (reinterpret_cast<intptr_t>(d2)&15)==0 )
            if( ((reinterpret_cast<intptr_t>(s) | static_cast<intptr_t>(src.pitch) |
                  reinterpret_cast<intptr_t>(d) | static_cast<intptr_t>(dst.pitch) ) & 15 )==0 )
            {
                for(int i=0; i<h; i++, s += src.pitch, d += dst.pitch)
                {
                    BYTE* sa = s;
                    BYTE* s2 = s + src.pitch*src.h;
                    BYTE* s2end_mod16 = s2 + (w&~15);
                    BYTE* s2end = s2 + w;
                    BYTE* d2 = d;
                                        
                    for(; s2 < s2end_mod16; s2+=16, sa+=16, d2+=16)
                    {
                        //important!
                        __m128i alpha = _mm_load_si128( reinterpret_cast<const __m128i*>(sa) );
                        __m128i src_y = _mm_load_si128( reinterpret_cast<const __m128i*>(s2) );
                        __m128i dst_y = _mm_load_si128( reinterpret_cast<const __m128i*>(d2) );                        

                        __m128i alpha_ff = _mm_cmpeq_epi32(alpha_ff,alpha_ff);
                        alpha_ff = _mm_cmpeq_epi8(alpha_ff, alpha);                                           

                        __m128i lo = _mm_unpacklo_epi8(alpha_ff, alpha);//(alpha<<8)+0x100 will overflow
                                                                        //so we do it another way
                                                                        //first, (alpha<<8)+0xff
                        __m128i ones = _mm_setzero_si128();
                        ones = _mm_cmpeq_epi16(dst_y, ones);
                        
                        __m128i ones2 = _mm_cmpeq_epi32(ones2,ones2);
                        ones = _mm_xor_si128(ones, ones2);                            
                        ones = _mm_srli_epi16(ones, 15);
                        ones = _mm_and_si128(ones, lo);

                        dst_y = _mm_mulhi_epu16(dst_y, lo);
                        dst_y = _mm_adds_epu16(dst_y, ones);//then add one if necessary

                        lo = _mm_setzero_si128();
                        lo = _mm_unpacklo_epi8(lo, src_y);
                        dst_y = _mm_adds_epu16(dst_y, lo);                        
                        _mm_store_si128( reinterpret_cast<__m128i*>(d2), dst_y );

                        d2 += 16;
                        dst_y = _mm_load_si128( reinterpret_cast<const __m128i*>(d2) );

                        lo = _mm_unpackhi_epi8(alpha_ff, alpha);

                        ones = _mm_setzero_si128();
                        ones = _mm_cmpeq_epi16(dst_y, ones);
                        ones = _mm_xor_si128(ones, ones2);  
                        ones = _mm_srli_epi16(ones, 15);
                        ones = _mm_and_si128(ones, lo);    

                        dst_y = _mm_mulhi_epu16(dst_y, lo); 
                        dst_y = _mm_adds_epu16(dst_y, ones);

                        lo = _mm_setzero_si128();
                        lo = _mm_unpackhi_epi8(lo, src_y);
                        dst_y = _mm_adds_epu16(dst_y, lo);
                        _mm_store_si128( reinterpret_cast<__m128i*>(d2), dst_y );
                    }
                    for( WORD* d3=reinterpret_cast<WORD*>(d2); s2 < s2end; s2++, sa++, d3++)
                    {
                        if(sa[0] < 0xff)
                        {                            
                            d2[0] = ((d2[0]*sa[0])>>8) + (s2[0]<<8);
                        }
                    }
                }
            }
            else //fix me: only a workaround for non-mod-16 size video
            {
                for(int i=0; i<h; i++, s += src.pitch, d += dst.pitch)
                {
                    BYTE* sa = s;
                    BYTE* s2 = s + src.pitch*src.h;
                    BYTE* s2end_mod16 = s2 + (w&~15);
                    BYTE* s2end = s2 + w;
                    WORD* d2 = reinterpret_cast<WORD*>(d);
                    for(; s2 < s2end; s2+=1, sa+=1, d2+=1)
                    {
                        if(sa[0] < 0xff)
                        {                            
                            d2[0] = ((d2[0]*sa[0])>>8) + (s2[0]<<8);
                        }
                    }
                }
            }
//            // equivalent:
//            //   if( (reinterpret_cast<intptr_t>(s2)&15)==0 && (reinterpret_cast<intptr_t>(sa2)&15)==0 
//            //       && (reinterpret_cast<intptr_t>(d2)&7)==0 )
//            if( ((reinterpret_cast<intptr_t>(ss[0]) | reinterpret_cast<intptr_t>(ss[1]) | 
//                reinterpret_cast<intptr_t>(ddUV) | 
//                reinterpret_cast<intptr_t>(src_origin) | static_cast<intptr_t>(src.pitch) |
//                (static_cast<intptr_t>(dst.pitchUV)&7) ) & 15 )==0 )
//            {
//                BYTE* s_u = ss[0];
//                BYTE* s_v = ss[1];
//                BYTE* sa = src_origin;
//                BYTE* d = ddUV;
//                int pitch = src.pitch;
//                for(int j = 0; j < h2; j++, s_u += src.pitch*2, s_v += src.pitch*2, sa += src.pitch*2, d += dst.pitchUV)
//                {
//                    BYTE* s_u2 = s_u;
//                    BYTE* sa2 = sa;
//                    BYTE* s_u2end_mod16 = s_u2 + (w&~15);
//                    BYTE* s_u2end = s_u2 + w;
//                    BYTE* d2 = d;
//                    BYTE* s_v2 = s_v;
//                                                           
//                    for(; s_u2 < s_u2end_mod16; s_u2 += 8, s_v2+=8, sa2 += 8, d2+=16)
//                    {
//                        __m128i dst = _mm_load_si128( reinterpret_cast<const __m128i*>(d2) );
//                        __m128i alpha1 = _mm_load_si128( reinterpret_cast<const __m128i*>(sa2) );
//                        __m128i alpha2 = _mm_load_si128( reinterpret_cast<const __m128i*>(sa2+src.pitch) );
//
//                        __m128i temp1 = _mm_setzero_si128();
//                        temp1 = _mm_unpacklo_epi8(alpha1, temp1);
//                        __m128i temp2 = _mm_setzero_si128();
//                        temp2 = _mm_unpacklo_epi8(alpha1, temp2);
//
//                        temp1 = _mm_adds_epu16(temp1, temp2);
//
//                        temp2 = _mm_srai_epi32(temp1, 16);
//                        temp1 = _mm_adds_epu16(temp1, temp2);
//                        temp1 = _mm_srli_epi32(temp1, 22);
//                        temp2 = _mm_srai_epi32(temp1, 16);
//                        temp1 = _mm_adds_epu16(temp1, temp2);
//                                                
//                        dst = _mm_mulhi_epu16(dst, temp1);
//
//
//                        __m128i su1 = _mm_load_si128( reinterpret_cast<const __m128i*>(s_u2) );
//                        __m128i su2 = _mm_load_si128( reinterpret_cast<const __m128i*>(s_u2+src.pitch) );
//                        __m128i sv1 = _mm_load_si128( reinterpret_cast<const __m128i*>(s_v2) );
//                        __m128i sv2 = _mm_load_si128( reinterpret_cast<const __m128i*>(s_u2+src.pitch) );
//
///*
//                        su1 = _mm_unpacklo_epi8(su1, zero);
//                        su2 = _mm_unpacklo_epi8(su2, zero);
//                        sv1 = _mm_unpacklo_epi8(sv1, zero);
//                        sv2 = _mm_unpacklo_epi8(sv2, zero);
//                        alpha = _mm_unpacklo_epi8(alpha, zero);
//                        alpha2 = _mm_unpacklo_epi8(alpha2, zero);
//                        
//                        su1 = _mm_adds_epu16(su1, su2);                        
//                        sv1 = _mm_adds_epu16(sv1, sv2);
//                        alpha = _mm_adds_epu16(alpha, alpha2);
//
//                        su2 = _mm_srli_epi32(su1, 16);
//                        sv2 = _mm_srli_epi32(sv1, 16);
//                        alpha2 = _mm_srli_epi32(alpha, 16);
//
//                        su1 = _mm_adds_epu16(su1,su2);
//                        sv1 = _mm_adds_epu16(sv1,sv2);
//                        alpha = _mm_adds_epu16(alpha,alpha2);
//
//                        su1 = _mm_srai_epi32(su1, 16);
//                        sv1 = _mm_srai_epi32(sv1, 16);
//                        sv1 = _mm_srli_epi32(sv1, 16);
//
//                        su1 = _mm_add_epi32(su1,sv1);
//
//                        alpha2 = _mm_srai_epi32(alpha, 16);
//                        alpha = _mm_srli_epi32(alpha2, 16);
//                        alpha = _mm_add_epi32(alpha,alpha2);
//                        alpha = _mm_srli_epi16(alpha, 6);
//
//                        dst = _mm_mulhi_epu16(dst, alpha);
//                        dst = _mm_adds_epu16(dst, su1);
//                        _mm_store_si128( reinterpret_cast<__m128i*>(d2), dst );     */                   
//                    }
//                    for( WORD* d3=reinterpret_cast<WORD*>(d2); s_u2 < s_u2end; s_u2+=2, s_v2+=2, sa2+=2, d3++)
//                    {
//                        unsigned int ia = ( sa2[0]+          sa2[1]+
//                                            sa2[0+src.pitch]+sa2[1+src.pitch]);
//                        *d3 = (((*d3)*ia)>>8) + ((s_u2[0] +       s_u2[1]+
//                                                  s_u2[src.pitch]+s_u2[1+src.pitch] ));
//                        d3++;
//                        *d3 = (((*d3)*ia)>>8) + ((s_v2[0] +       s_v2[1]+
//                                                  s_v2[src.pitch]+s_v2[1+src.pitch] ));
//                    }
//                }
//            }
//            else//fix me: only a workaround for non-mod-16 size video
            {
                BYTE* s_u = ss[0];
                BYTE* s_v = ss[1];
                BYTE* sa = src_origin;
                BYTE* d = ddUV;
                int pitch = src.pitch;
                for(int j = 0; j < h2; j++, s_u += src.pitch*2, s_v += src.pitch*2, sa += src.pitch*2, d += dst.pitchUV)
                {
                    BYTE* s_u2 = s_u;
                    BYTE* sa2 = sa;
                    BYTE* s_u2end_mod16 = s_u2 + (w&~15);
                    BYTE* s_u2end = s_u2 + w;
                    BYTE* d2 = d;
                    BYTE* s_v2 = s_v;

                    for( WORD* d3=reinterpret_cast<WORD*>(d2); s_u2 < s_u2end; s_u2+=2, s_v2+=2, sa2+=2, d3+=2)
                    {
                        unsigned int ia = ( 
                            sa2[0]+          sa2[1]+
                            sa2[0+src.pitch]+sa2[1+src.pitch]);
                        if( ia!=0xFF*4 )
                        {
                            *d3 = (((*d3)*ia)>>10) + ((
                                s_u2[0] +       s_u2[1]+
                                s_u2[src.pitch]+s_u2[1+src.pitch] )<<6);                            
                            d3[1] = (((d3[1])*ia)>>10) + ((
                                s_v2[0] +       s_v2[1]+
                                s_v2[src.pitch]+s_v2[1+src.pitch] )<<6);
                        }
                    }
                }
            }
            __asm emms;
        }
        break;
    default:
        return E_NOTIMPL;
        break;
    }

    //emms要40个cpu周期
    //__asm emms;
    return S_OK;
}

STDMETHODIMP CMemSubPic::AlphaBltAyuv_Yv12(const RECT* pSrc, const RECT* pDst, SubPicDesc* pTarget)
{
    const SubPicDesc& src = m_spd;
    SubPicDesc dst = *pTarget; // copy, because we might modify it

    CRect rs(*pSrc), rd(*pDst);

    if(dst.h < 0) {
        dst.h = -dst.h;
        rd.bottom = dst.h - rd.bottom;
        rd.top = dst.h - rd.top;
    }

    if(rs.Width() != rd.Width() || rs.Height() != abs(rd.Height())) {
        return E_INVALIDARG;
    }

    int w = rs.Width(), h = rs.Height();

    BYTE* s = (BYTE*)src.bits + src.pitch*rs.top + ((rs.left*src.bpp)>>3);
    BYTE* d = (BYTE*)dst.bits + dst.pitch*rd.top + rd.left;

    if(rd.top > rd.bottom) {
        d = (BYTE*)dst.bits + dst.pitch*(rd.top-1) + rd.left;

        dst.pitch = -dst.pitch;
    }

    for(ptrdiff_t j = 0; j < h; j++, s += src.pitch, d += dst.pitch) {
        BYTE* s2 = s;
        BYTE* s2end = s2 + w*4;
        BYTE* d2 = d;
        for(; s2 < s2end; s2 += 4, d2++) {
            if(s2[3] < 0xff) {
                d2[0] = ((d2[0]*s2[3])>>8) + s2[1];
            }
        }
    }
    dst.pitch = abs(dst.pitch);

    int h2 = h/2;

    if(!dst.pitchUV) {
        dst.pitchUV = dst.pitch/2;
    }

    BYTE* ss[2];
    ss[0] = (BYTE*)src.bits + src.pitch*rs.top + rs.left*4;
    ss[1] = ss[0] + 4;

    if(!dst.bitsU || !dst.bitsV) {
        dst.bitsU = (BYTE*)dst.bits + dst.pitch*dst.h;
        dst.bitsV = dst.bitsU + dst.pitchUV*dst.h/2;

        if(dst.type == MSP_YV12) {
            BYTE* p = dst.bitsU;
            dst.bitsU = dst.bitsV;
            dst.bitsV = p;
        }
    }

    BYTE* dd[2];
    dd[0] = dst.bitsU + dst.pitchUV*rd.top/2 + rd.left/2;
    dd[1] = dst.bitsV + dst.pitchUV*rd.top/2 + rd.left/2;

    if(rd.top > rd.bottom) {
        dd[0] = dst.bitsU + dst.pitchUV*(rd.top/2-1) + rd.left/2;
        dd[1] = dst.bitsV + dst.pitchUV*(rd.top/2-1) + rd.left/2;
        dst.pitchUV = -dst.pitchUV;
    }

    for(ptrdiff_t i = 0; i < 2; i++) {
        s = ss[i];
        d = dd[i];
        BYTE* is = ss[1-i];
        for(ptrdiff_t j = 0; j < h2; j++, s += src.pitch*2, d += dst.pitchUV, is += src.pitch*2) {
            BYTE* s2 = s;
            BYTE* s2end = s2 + w*4;
            BYTE* d2 = d;
            BYTE* is2 = is;
            for(; s2 < s2end; s2 += 8, d2++, is2 += 8) {
                unsigned int ia = (s2[3]+s2[3+src.pitch]+is2[3]+is2[3+src.pitch])>>2;
                if(ia < 0xff) {
                    *d2 = ((*d2*ia)>>8) + ((s2[0]+s2[src.pitch])>>1);
                }
            }
        }
    }

    return S_OK;
}

STDMETHODIMP CMemSubPic::SetDirtyRectEx(CAtlList<CRect>* dirtyRectList )
{
    //if(m_spd.type == MSP_YUY2 || m_spd.type == MSP_YV12 || m_spd.type == MSP_IYUV || m_spd.type == MSP_AYUV)
    if(dirtyRectList!=NULL)
    {
        POSITION pos = dirtyRectList->GetHeadPosition();
        if(m_spd.type == MSP_AY11 || m_alpha_blt_dst_type==MSP_IYUV || m_alpha_blt_dst_type==MSP_YV12 
            || m_alpha_blt_dst_type==MSP_P010 || m_alpha_blt_dst_type==MSP_P016)
        {
            while(pos!=NULL)
            {
                CRect& cRectSrc = dirtyRectList->GetNext(pos);
                cRectSrc.left &= ~15;
                cRectSrc.right = (cRectSrc.right+15)&~15;
                cRectSrc.top &= ~1;
                cRectSrc.bottom = (cRectSrc.bottom+1)&~1;
            }
        }
        else if(m_spd.type == MSP_AUYV || m_alpha_blt_dst_type==MSP_YUY2)
        {
            while(pos!=NULL)
            {
                CRect& cRectSrc = dirtyRectList->GetNext(pos);
                cRectSrc.left &= ~3;
                cRectSrc.right = (cRectSrc.right+3)&~3;
            }
        }
    }
    return __super::SetDirtyRectEx(dirtyRectList);
}

//
// CMemSubPicAllocator
//

CMemSubPicAllocator::CMemSubPicAllocator(int alpha_blt_dst_type, SIZE maxsize, int type/*=-1*/)
    : CSubPicExAllocatorImpl(maxsize, false, false)
    , m_alpha_blt_dst_type(alpha_blt_dst_type)
    , m_maxsize(maxsize)
    , m_type(type)
{
    if(m_type==-1)
    {
        switch(alpha_blt_dst_type)
        {
        case MSP_YUY2:
            m_type = MSP_AUYV;
            break;
        case MSP_AYUV:
            m_type = MSP_AYUV;
            break;
        case MSP_IYUV:
        case MSP_YV12:
            m_type = MSP_AY11;
            break;
        default:
            m_type = MSP_RGBA;
            break;
        }
    }
}

// ISubPicAllocatorImpl

bool CMemSubPicAllocator::AllocEx(bool fStatic, ISubPicEx** ppSubPic)
{
	if(!ppSubPic) {
		return false;
	}
    SubPicDesc spd;
    spd.w = m_maxsize.cx;
    spd.h = m_maxsize.cy;
    spd.bpp = 32;
    spd.pitch = (spd.w*spd.bpp)>>3;
    spd.type = m_type;
	spd.bits = DNew BYTE[spd.pitch*spd.h];
	if(!spd.bits) {
		return false;
	}    
	*ppSubPic = DNew CMemSubPic(spd, m_alpha_blt_dst_type);
	if(!(*ppSubPic)) {
		return false;
	}
    (*ppSubPic)->AddRef();
	return true;
}



