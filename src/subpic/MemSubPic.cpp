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

#define MIX_4_PIX_YV12(dst, zero_128i, c_128i, a_128i) \
{ \
    __m128i d_128i = _mm_cvtsi32_si128(*dst);    \
	_MIX_4_PIX_YV12(d_128i, zero_128i, c_128i, a_128i) \
    *dst = (DWORD)_mm_cvtsi128_si32(d_128i); \
}

#define _MIX_4_PIX_YV12(dst_128i, zero_128i, c_128i, a_128i) \
{ \
	dst_128i = _mm_unpacklo_epi8(dst_128i, zero_128i);    \
	dst_128i = _mm_unpacklo_epi16(dst_128i, c_128i); \
	dst_128i = _mm_madd_epi16(dst_128i, a_128i);   \
	dst_128i = _mm_srli_epi32(dst_128i, 8);   \
	dst_128i = _mm_packs_epi32(dst_128i, dst_128i);  \
	dst_128i = _mm_packus_epi16(dst_128i, dst_128i); \
}

#define AVERAGE_4_PIX(a,b)      \
    __asm    pavgb       a, b   \
    __asm    movaps      b, a   \
    __asm    psrlw       a, 8   \
    __asm    psllw       b, 8   \
    __asm    psrlw       b, 8   \
    __asm    pavgw       a, b

#define SSE2_ALPHA_BLT_UV(dst, alpha_mask, src, src_pitch) \
    __asm    mov         eax,src_pitch                  \
                                                        \
    __asm    xorps       XMM0,XMM0                      \
    __asm    mov         esi, alpha_mask                \
    __asm    movaps      XMM1,[esi]                     \
    __asm    add         esi, eax                       \
    __asm    movaps      XMM2,[esi]                     \
                                                        \
    __asm    AVERAGE_4_PIX(XMM1, XMM2)                  \
    __asm    mov         edi, dst                       \
    __asm    movlps      XMM3,[edi]                     \
    __asm    punpcklbw   XMM3,XMM0                      \
    __asm    pmullw      XMM3,XMM1                      \
    __asm    psrlw       XMM3,8                         \
                                                        \
    __asm    mov         esi, src                       \
    __asm    movaps      XMM1,[esi]                     \
    __asm    add         esi, eax                       \
    __asm    movaps      XMM2,[esi]                     \
    __asm    AVERAGE_4_PIX(XMM1, XMM2)                  \
                                                        \
    __asm    paddw       XMM3,XMM1                      \
    __asm    packuswb    XMM3,XMM0                      \
                                                        \
    __asm    movdq2q     MM0, XMM3                      \
    __asm    movq        [edi],MM0

//
// CMemSubPic
//

CMemSubPic::CMemSubPic(SubPicDesc& spd)
    : m_spd(spd)
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

STDMETHODIMP CMemSubPic::CopyTo(ISubPic* pSubPic)
{
    HRESULT hr;
    if(FAILED(hr = __super::CopyTo(pSubPic)))
        return hr;
    SubPicDesc src, dst;
    if(FAILED(GetDesc(src)) || FAILED(pSubPic->GetDesc(dst)))
        return E_FAIL;
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
    if(m_rectListDirty.IsEmpty())
        return S_FALSE;
    while(!m_rectListDirty.IsEmpty())
    {
        //pDirtyRect = m_rectListDirty.RemoveHead();
        CRect& dirtyRect = m_rectListDirty.RemoveTail();
        BYTE* p = (BYTE*)m_spd.bits + m_spd.pitch*(dirtyRect.top) + dirtyRect.left*(m_spd.bpp>>3);
        int w = dirtyRect.Width();
        if(m_spd.type!=MSP_YV12 && m_spd.type!=MSP_IYUV)
        {
            for(int j = 0, h = dirtyRect.Height(); j < h; j++, p += m_spd.pitch)
            {
                //        memsetd(p, 0, m_rcDirty.Width());
                //DbgLog((LOG_TRACE, 3, "w:%d", w));
                //w = pDirtyRect->Width();
                __asm
                {
                        mov eax, color
                        mov ecx, w
                        mov edi, p
                        cld
                        rep stosd
                }
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
    return S_OK;
}

STDMETHODIMP CMemSubPic::Lock(SubPicDesc& spd)
{
    return GetDesc(spd);
}

STDMETHODIMP CMemSubPic::Unlock(CAtlList<CRect>* dirtyRectList)
{
    POSITION pos;
    SetDirtyRect(dirtyRectList);
    if(m_rectListDirty.IsEmpty())
        return S_OK;

    pos = m_rectListDirty.GetHeadPosition();
    while(pos!=NULL)
    {
        CRect cRect = m_rectListDirty.GetNext(pos);
        int w = cRect.Width(), h = cRect.Height();
        BYTE* top = (BYTE*)m_spd.bits + m_spd.pitch*(cRect.top) + cRect.left*4;
        BYTE* bottom = top + m_spd.pitch*h;
        if(m_spd.type == MSP_RGB16)
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
        else if(m_spd.type == MSP_RGB15)
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
        else if(m_spd.type == MSP_YUY2)
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
        else if(m_spd.type == MSP_YV12 || m_spd.type == MSP_IYUV)
        {
            //nothing to do
        }
    }
    return S_OK;
}

STDMETHODIMP CMemSubPic::AlphaBlt(const RECT* pSrc, const RECT* pDst, SubPicDesc* pTarget)
{
    ASSERT(pTarget);
    if(!pSrc || !pDst || !pTarget)
        return E_POINTER;
    const SubPicDesc& src = m_spd;
    SubPicDesc dst = *pTarget; // copy, because we might modify it
    if(src.type != dst.type)
        return E_INVALIDARG;
    CRect rs(*pSrc), rd(*pDst);
    if(dst.h < 0)
    {
        dst.h = -dst.h;
        rd.bottom = dst.h - rd.bottom;
        rd.top = dst.h - rd.top;
    }
    if(rs.Width() != rd.Width() || rs.Height() != abs(rd.Height()))
        return E_INVALIDARG;
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
    case MSP_AYUV:
        for(int j = 0; j < h; j++, s += src.pitch, d += dst.pitch)
        {
            BYTE* s2 = s;
            BYTE* s2end = s2 + w*4;
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
                    /*
                    y1 = (BYTE)(((((*d2&0xff)-0x10)*s2[3])>>8) + s2[1]); // + y1;
                    y2 = (BYTE)((((((*d2>>16)&0xff)-0x10)*s2[7])>>8) + s2[5]); // + y2;
                    u = (BYTE)((((((*d2>>8)&0xff)-0x80)*ia)>>8) + s2[0]); // + u;
                    v = (BYTE)((((((*d2>>24)&0xff)-0x80)*ia)>>8) + s2[4]); // + v;

                    *d2 = (v<<24)|(y2<<16)|(u<<8)|y1;
                    */
                    //static const __int64 _8181 = 0x0080001000800010i64;
                    ia = (ia<<24)|(s2[7]<<16)|(ia<<8)|s2[3];
                    c = (s2[4]<<24)|(s2[5]<<16)|(s2[0]<<8)|s2[1]; // (v<<24)|(y2<<16)|(u<<8)|y1;
                    __asm
                    {
                            mov			esi, s2
                            mov			edi, d2
                            pxor		mm0, mm0
                            //movq		mm1, _8181
                            movd		mm2, c
                            punpcklbw	mm2, mm0
                            movd		mm3, [edi]
                            punpcklbw	mm3, mm0
                            movd		mm4, ia
                            punpcklbw	mm4, mm0
                            psrlw		mm4, 1
                            //psubsw		mm3, mm1
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
            BYTE *s = src_origin;

            BYTE* ss[2];
            ss[0] = src_origin + src.pitch*src.h*2;//U
            ss[1] = src_origin + src.pitch*src.h*3;//V

            for(int i=0; i<h; i++, s += src.pitch, d += dst.pitch)
            {
                BYTE* sa = s;
                BYTE* s2 = s + src.pitch*src.h;
                BYTE* s2end = s2 + w;
                BYTE* d2 = d;
//                for(; s2 < s2end; s2+=1, sa+=1, d2+=1)
//                {
//                //if(s2[3] < 0xff)
//                    {
//                        //					d2[0] = (((d2[0]-0x10)*s2[3])>>8) + s2[1];
//                        d2[0] = (((d2[0])*sa[0])>>8) + s2[0];
//                    }
//                }
                for(; s2 < s2end; s2+=16, sa+=16, d2+=16)
                {
                    __asm
                    {
                        //important!
                        mov			edi, d2
                        mov         esi, sa

                        movaps      XMM3,[edi]
                        xorps       XMM0,XMM0
                        movaps      XMM4,XMM3
                        punpcklbw   XMM4,XMM0

                        movaps      XMM1,[esi]
                        movaps      XMM5,XMM1
                        punpcklbw   XMM5,XMM0
                        pmullw      XMM4,XMM5
                        psrlw       XMM4,8

                        punpckhbw   XMM1,XMM0
                        punpckhbw   XMM3,XMM0
                        pmullw      XMM1,XMM3
                        psrlw       XMM1,8

                        packuswb    XMM4,XMM1
                        mov         esi, s2
                        movaps      XMM3,[esi]
                        paddusb     XMM4,XMM3
                        movntps     [edi],XMM4
                    }
                }
            }
            for(int i = 0; i < 2; i++)
            {
                BYTE* s_uv = ss[i];
                BYTE* sa = src_origin;
                d = dd[i];
                int pitch = src.pitch;
                for(int j = 0; j < h2; j++, s_uv += src.pitch*2, sa += src.pitch*2, d += dst.pitchUV)
                {
                    BYTE* s2 = s_uv;
                    BYTE* sa2 = sa;
                    BYTE* s2end = s2 + w;
                    BYTE* d2 = d;
//                    for(; s2 < s2end; s2 += 2, sa2 += 2, d2++)
//                    {
//                        unsigned int ia = (sa2[0]+         +sa2[1]+
//                                           sa2[0+src.pitch]+sa2[1+src.pitch])>>2;
//                      //if(ia < 0xff)
//                        {
//                            //                        *d2 = (((*d2-0x80)*ia)>>8) + ((s2[0]        +s2[1]
//                            //                                                       s2[src.pitch]+s2[1+src.pitch] )>>2);
//                            *d2 = (((*d2)*ia)>>8) + ((s2[0]        +s2[1]+
//                                                      s2[src.pitch]+s2[1+src.pitch] )>>2);
//                        }
//                    }
                    for(; s2 < s2end; s2 += 16, sa2 += 16, d2+=8)
                    {
                        SSE2_ALPHA_BLT_UV(d2, sa2, s2, pitch)
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

STDMETHODIMP CMemSubPic::SetDirtyRect(CAtlList<CRect>* dirtyRectList )
{
    //if(m_spd.type == MSP_YUY2 || m_spd.type == MSP_YV12 || m_spd.type == MSP_IYUV || m_spd.type == MSP_AYUV)
    if(dirtyRectList!=NULL)
    {
        POSITION pos = dirtyRectList->GetHeadPosition();
        if(m_spd.type == MSP_YV12 || m_spd.type == MSP_IYUV)
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
        else if(m_spd.type == MSP_YUY2)
        {
            while(pos!=NULL)
            {
                CRect& cRectSrc = dirtyRectList->GetNext(pos);
                cRectSrc.left &= ~1;
                cRectSrc.right = (cRectSrc.right+1)&~1;
            }
        }
    }
    return __super::SetDirtyRect(dirtyRectList);
}
//
// CMemSubPicAllocator
//

CMemSubPicAllocator::CMemSubPicAllocator(int type, SIZE maxsize)
    : ISubPicAllocatorImpl(maxsize, false, false)
    , m_type(type)
    , m_maxsize(maxsize)
{
}

// ISubPicAllocatorImpl

bool CMemSubPicAllocator::Alloc(bool fStatic, ISubPic** ppSubPic)
{
    if(!ppSubPic)
        return(false);
    SubPicDesc spd;
    spd.w = m_maxsize.cx;
    spd.h = m_maxsize.cy;
    spd.bpp = 32;
    spd.pitch = (spd.w*spd.bpp)>>3;
    spd.type = m_type;
    if(!(spd.bits = new BYTE[spd.pitch*spd.h]))
        return(false);
    if(!(*ppSubPic = new CMemSubPic(spd)))
        return(false);
    (*ppSubPic)->AddRef();
    return(true);
}



