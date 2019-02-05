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

#ifdef SPD_DUMP_FILE
#include <fstream>
//
// debug functions
// 
static void SaveRect2File(const CRect& cRect, const char * filename)
{
    std::ofstream os(filename);
    os<<cRect.left<<","<<cRect.top<<","<<cRect.right<<","<<cRect.bottom;
}
static void SaveAxxx2File(SubPicDesc& spd, const CRect& cRect, const char * filename)
{
    std::ofstream axxx(filename);
    int w = cRect.Width(), h = cRect.Height();

    BYTE* top = (BYTE*)spd.bits + spd.pitch*cRect.top + cRect.left*4;
    BYTE* bottom = top + spd.pitch*h;

    for(; top < bottom ; top += spd.pitch) {
        BYTE* s = top;
        BYTE* e = s + w*4;
        for(; s < e; s+=4) { // ARGB ARGB -> AxYU AxYV
            axxx<<(int)s[0]<<","<<(int)s[1]<<","<<(int)s[2]<<","<<(int)s[3];
            if(s+4>=e)
            {
                axxx<<std::endl;
            }
            else
            {
                axxx<<",";
            }
        }
    }
    axxx.close();
}
static void SaveArgb2File(SubPicDesc& spd, const CRect& cRect, const char * filename)
{
    SaveAxxx2File(spd, cRect, filename);
}
static void SaveAyuv2File(SubPicDesc& spd, const CRect& cRect, const char * filename)
{
    SaveAxxx2File(spd, cRect, filename);
}
static void SaveNvxx2File(SubPicDesc& spd, const CRect& cRect, const char * filename)
{
    std::ofstream os(filename);
    int w = cRect.Width(), h = cRect.Height();

    BYTE* top = (BYTE*)spd.bits;
    BYTE* bottom = top + spd.pitch*h;

    for(; top < bottom ; top += spd.pitch) {
        BYTE* s = top;
        BYTE* e = s + w;

        BYTE* sY = s + spd.pitch*spd.h;
        BYTE* sU = sY + spd.pitch*spd.h;
        BYTE* sV = sU + 1;
        for(; s < e; s++, sY++, sU+=2,sV+=2) {
            os<<(int)s[0]<<","<<(int)sY[0]<<","<<(int)sU[0]<<","<<(int)sV[0];
            if(s+1>=e)
            {
                os<<std::endl;
            }
            else
            {
                os<<",";
            }
        }
    }
    os.close();
}

#define ONCER(expr) {\
    static bool entered=false;\
    if(!entered)\
    {\
        entered=true;\
        expr;\
    }\
}
#else
#define ONCER(expr) 
#endif

//
// alpha blend functions
// 
#include "xy_intrinsics.h"
#include "../dsutil/vd.h"

#ifndef _WIN64
static void AlphaBlt_YUY2_MMX(int w, int h, BYTE* d, int dstpitch, PCUINT8 s, int srcpitch)
{
    for(int j = 0; j < h; j++, s += srcpitch, d += dstpitch)
    {
        unsigned int ia, c;
        PCUINT8 s2 = s;
        PCUINT8 s2end = s2 + w*4;
        DWORD* d2 = (DWORD*)d;
        ASSERT(w>0);
        int last_a = w>0?s2[3]:0;
        for(; s2 < s2end; s2 += 8, d2++)
        {
            ia = (last_a + 2*s2[3] + s2[7])>>2;
            last_a = s2[7];
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
    _mm_empty();
}
#endif

void AlphaBlt_YUY2_C(int w, int h, BYTE* d, int dstpitch, PCUINT8 s, int srcpitch)
{
    for(int j = 0; j < h; j++, s += srcpitch, d += dstpitch)
    {
        DWORD ia;
        PCUINT8 s2 = s;
        PCUINT8 s2end = s2 + w*4;
        DWORD* d2 = (DWORD*)d;
        ASSERT(w>0);
        int last_a = w>0?s2[3]:0;
        for(; s2 < s2end; s2 += 8, d2++)
        {
            ia = (last_a + 2*s2[3] + s2[7])>>2;
            last_a = s2[7];
            if(ia < 0xff)
            {
                DWORD y1 = (BYTE)(((((*d2&0xff))*s2[3])>>8) + s2[1]); // + y1;
                DWORD u = (BYTE)((((((*d2>>8)&0xff))*ia)>>8) + s2[0]); // + u;
                DWORD y2 = (BYTE)((((((*d2>>16)&0xff))*s2[7])>>8) + s2[5]); // + y2;                    
                DWORD v = (BYTE)((((((*d2>>24)&0xff))*ia)>>8) + s2[4]); // + v;
                *d2 = (v<<24)|(y2<<16)|(u<<8)|y1;
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
        if(m_spd.type!=MSP_AYUV_PLANAR)
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
    ONCER( SaveArgb2File(m_spd, CRect(CPoint(0,0), m_size), SPD_DUMP_FILE".argb_c") );
    return S_OK;
}

STDMETHODIMP CMemSubPic::Lock(SubPicDesc& spd)
{
    return GetDesc(spd);
}

STDMETHODIMP CMemSubPic::UnlockEx( CAtlList<CRect>* dirtyRectList )
{
    int src_type = m_spd.type;
    int dst_type = m_alpha_blt_dst_type;
    if( (src_type==MSP_RGBA && (dst_type == MSP_RGB32 ||
                                dst_type == MSP_RGB24 ||
                                dst_type == MSP_RGB16 ||
                                dst_type == MSP_RGB15)) 
        ||
        (src_type==MSP_XY_AUYV &&  dst_type == MSP_YUY2)//ToDo: fix me MSP_AYUV
        ||
        (src_type==MSP_AYUV &&  dst_type == MSP_AYUV) 
        ||
        (src_type==MSP_AYUV_PLANAR && (dst_type == MSP_IYUV ||
                                dst_type == MSP_YV12 ||
                                dst_type == MSP_P010 ||
                                dst_type == MSP_P016 ||
                                dst_type == MSP_NV12 ||
                                dst_type == MSP_NV21)))
    {
        return UnlockOther(dirtyRectList);        
    }
    else if(src_type==MSP_RGBA && (dst_type == MSP_YUY2 ||  
                                   dst_type == MSP_AYUV || //ToDo: fix me MSP_AYUV
                                   dst_type == MSP_IYUV ||
                                   dst_type == MSP_YV12 ||
                                   dst_type == MSP_NV12 ||
                                   dst_type == MSP_NV21 ||
                                   dst_type == MSP_P010 ||
                                   dst_type == MSP_P016))
    {
        return UnlockRGBA_YUV(dirtyRectList);
    }
    return E_NOTIMPL;
}

HRESULT CMemSubPic::UnlockOther(CAtlList<CRect>* dirtyRectList)
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
        if (w<=0 || h<=0)
        {
            continue;
        }

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
            for(BYTE* tempTop=top; tempTop < bottom ; tempTop += m_spd.pitch)
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
        else if(m_alpha_blt_dst_type == MSP_YV12 || m_alpha_blt_dst_type == MSP_IYUV 
            || m_alpha_blt_dst_type == MSP_AYUV)
        {
            //nothing to do
        }
        else if ( m_alpha_blt_dst_type == MSP_P010 || m_alpha_blt_dst_type == MSP_P016 
            || m_alpha_blt_dst_type == MSP_NV12 )
        {
            SubsampleAndInterlace(cRect, true);
        }
        else if( m_alpha_blt_dst_type == MSP_NV21 )
        {
            SubsampleAndInterlace(cRect, false);
        }
    }
    return S_OK;
}

HRESULT CMemSubPic::UnlockRGBA_YUV(CAtlList<CRect>* dirtyRectList)
{
    //debug
    ONCER( SaveRect2File(dirtyRectList->GetHead(), SPD_DUMP_FILE".rect") );
    ONCER( SaveArgb2File(m_spd, CRect(CPoint(0,0), m_size), SPD_DUMP_FILE".argb") );

    SetDirtyRectEx(dirtyRectList);

    ONCER( SaveRect2File(dirtyRectList->GetHead(), SPD_DUMP_FILE".rect2") );
    if(m_rectListDirty.IsEmpty()) {
        return S_OK;
    }

    POSITION pos = m_rectListDirty.GetHeadPosition();
    while(pos!=NULL)
    {
        const CRect& cRect = m_rectListDirty.GetNext(pos);
        int w = cRect.Width(), h = cRect.Height();
        if(w<=0 || h<=0)
        {
            continue;
        }

        BYTE* top = (BYTE*)m_spd.bits + m_spd.pitch*cRect.top + cRect.left*4;
        BYTE* bottom = top + m_spd.pitch*h;

        if( m_alpha_blt_dst_type == MSP_YUY2 || 
            m_alpha_blt_dst_type == MSP_YV12 || 
            m_alpha_blt_dst_type == MSP_IYUV ||
            m_alpha_blt_dst_type == MSP_P010 ||
            m_alpha_blt_dst_type == MSP_P016 ||
            m_alpha_blt_dst_type == MSP_NV12 ||
            m_alpha_blt_dst_type == MSP_NV21) {
            for(; top < bottom ; top += m_spd.pitch) {
                BYTE* s = top;
                BYTE* e = s + w*4;
                DWORD last_yuv = ColorConvTable::PreMulArgb2Ayuv(s[3], s[2], s[1], s[0]);
                for(; s < e; s+=8) { // ARGB ARGB -> AxYU AxYV
                    if((s[3]+s[7]+(last_yuv>>24)) < 0xff*3) {
                        DWORD tmp1 = ColorConvTable::PreMulArgb2Ayuv(s[3], s[2], s[1], s[0]);
                        DWORD tmp2 = ColorConvTable::PreMulArgb2Ayuv(s[7], s[6], s[5], s[4]);

                        s[1] = (tmp1>>16)&0xff;
                        s[5] = (tmp2>>16)&0xff;

                        s[0] = (((last_yuv>>8)&0xff) + 2*((tmp1>>8)&0xff) + ((tmp2>>8)&0xff) + 2)/4;
                        s[4] = ((last_yuv&0xff) + 2*(tmp1&0xff) + (tmp2&0xff) + 2)/4;
                        last_yuv = tmp2;
                    } else {
                        last_yuv = ColorConvTable::PreMulArgb2Ayuv(s[7], s[6], s[5], s[4]);

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
                        *((DWORD*)s) = ColorConvTable::PreMulArgb2Ayuv(s[3], s[2], s[1], s[0]);
                    } else {
                        s[0] = s[1] = 0;
                        s[2] = 0;
                    }
                }
            }
        }
    }
    
    ONCER( SaveAxxx2File(m_spd, CRect(CPoint(0,0), m_size), SPD_DUMP_FILE".axuv") );
    return S_OK;
}

void CMemSubPic::SubsampleAndInterlace( const CRect& cRect, bool u_first )
{
    //fix me: check alignment and log error
    int w = cRect.Width(), h = cRect.Height();
    BYTE* u_plan = reinterpret_cast<BYTE*>(m_spd.bits) + m_spd.pitch*m_spd.h*2;
    BYTE* u_start = u_plan + m_spd.pitch*(cRect.top)+ cRect.left;
    BYTE* v_start = u_start + m_spd.pitch*m_spd.h;
    BYTE* dst = u_start; 
    if(!u_first)
    {
        BYTE* tmp = v_start;
        v_start = u_start;
        u_start = tmp;
    }

    //Todo: fix me. 
    //Walkarround for alignment
    if (((m_spd.pitch|w) &15) == 0)
    {
        ASSERT(w%16==0);
        SubsampleAndInterlace(dst, u_start, v_start, h, w, m_spd.pitch);
    }
    else
    {
        SubsampleAndInterlaceC(dst, u_start, v_start, h, w, m_spd.pitch);
    }
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
        (src_type==MSP_XY_AUYV &&  dst_type == MSP_YUY2)//ToDo: fix me MSP_AYUV
        ||
        (src_type==MSP_AYUV &&  dst_type == MSP_AYUV) 
        ||
        (src_type==MSP_AYUV_PLANAR && (dst_type == MSP_IYUV ||
                                dst_type == MSP_YV12)) )
    {
        return AlphaBltOther(pSrc, pDst, pTarget);        
    }
    else if ( src_type==MSP_AYUV_PLANAR && (dst_type == MSP_NV12 ||
                                            dst_type == MSP_NV21 ) )
    {
        return AlphaBltAnv12_Nv12(pSrc, pDst, pTarget);
    }
    
    else if( src_type==MSP_AYUV_PLANAR && (dst_type == MSP_P010 ||
                                           dst_type == MSP_P016 ) )
    {
        return AlphaBltAnv12_P010(pSrc, pDst, pTarget);
    }
    else if( src_type==MSP_RGBA && (dst_type == MSP_IYUV ||
                                    dst_type == MSP_YV12)) 
    {
        return AlphaBltAxyuAxyv_Yv12(pSrc, pDst, pTarget);
    }
    else if( src_type==MSP_RGBA && (dst_type == MSP_NV12||
                                    dst_type == MSP_NV21)) 
    {
        return AlphaBltAxyuAxyv_Nv12(pSrc, pDst, pTarget);
    }
    else if( src_type==MSP_RGBA && (dst_type == MSP_P010 ||
                                    dst_type == MSP_P016))
    {
        return AlphaBltAxyuAxyv_P010(pSrc, pDst, pTarget);
    }
    return E_NOTIMPL;
}

HRESULT CMemSubPic::AlphaBltOther(const RECT* pSrc, const RECT* pDst, SubPicDesc* pTarget)
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
                        | (((((*d2&0x0000ff00)*s2[3])>>8) + (*((DWORD*)s2)&0x0000ff00))&0x0000ff00)
                        | (*d2&0xff000000);
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
        AlphaBlt_YUY2(w, h, d, dst.pitch, s, src.pitch);
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

HRESULT CMemSubPic::AlphaBltAxyuAxyv_P010(const RECT* pSrc, const RECT* pDst, SubPicDesc* pTarget)
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

    //Y
    BYTE* s = static_cast<BYTE*>(src.bits) + src.pitch*rs.top + ((rs.left*src.bpp)>>3);
    BYTE* d = static_cast<BYTE*>(dst.bits) + dst.pitch*rd.top + rd.left*2;

    if(rd.top > rd.bottom) {
        d = (BYTE*)dst.bits + dst.pitch*(rd.top-1) + rd.left;

        dst.pitch = -dst.pitch;
    }
    
    for(ptrdiff_t i=0; i<h; i++, s += src.pitch, d += dst.pitch)
    {
        BYTE* s2 = s;
        BYTE* s2end = s2 + w*4;
        WORD* d2 = reinterpret_cast<WORD*>(d);
        for(; s2 < s2end; s2 += 4, d2++)
        {
            if(s2[3] < 0xff) {
                d2[0] = ((d2[0]*s2[3])>>8) + (s2[1]<<8);
            }
        }
    }

    //UV
    int h2 = h/2;
    if(!dst.pitchUV)
    {
        dst.pitchUV = abs(dst.pitch);
    }
    if(!dst.bitsU || !dst.bitsV)
    {
        dst.bitsU = static_cast<BYTE*>(dst.bits) + abs(dst.pitch)*dst.h;
        dst.bitsV = dst.bitsU + 2;
    }
    BYTE* ddUV = dst.bitsU + dst.pitchUV*rd.top/2 + rd.left*2;
    if(rd.top > rd.bottom)
    {
        ddUV = dst.bitsU + dst.pitchUV*(rd.top/2-1) + rd.left*2;
        dst.pitchUV = -dst.pitchUV;
    }
        
    s = static_cast<BYTE*>(src.bits) + src.pitch*rs.top + ((rs.left*src.bpp)>>3);

    d = ddUV;
    int pitch = src.pitch;
    for(int j = 0; j < h2; j++, s += 2*src.pitch, d += dst.pitchUV )        
    {        
        BYTE* s2 = s;
        WORD* d2=reinterpret_cast<WORD*>(d);
        WORD* d2_end = reinterpret_cast<WORD*>(d+2*w);
        DWORD last_alpha = s2[3]+s2[3+src.pitch];
        for( ; d2<d2_end; s2+=8, d2+=2)
        {
            unsigned int ia = ( 
                last_alpha +
                (s2[3]  + s2[3+src.pitch])*2 +
                s2[3+4]+ s2[3+4+src.pitch]);
            last_alpha = s2[3+4]+ s2[3+4+src.pitch];
            if( ia!=0xFF*8 )
            {
                d2[0] = (((d2[0])*ia)>>11) + ((s2[0] + s2[0+src.pitch])<<7);
                d2[1] = (((d2[1])*ia)>>11) + ((s2[4] + s2[4+src.pitch])<<7);
            }
        }
    }

    return S_OK;
}

HRESULT CMemSubPic::AlphaBltAxyuAxyv_Yv12(const RECT* pSrc, const RECT* pDst, SubPicDesc* pTarget)
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
        BYTE* a = ss[0]+3;
        for(ptrdiff_t j = 0; j < h2; j++, s += src.pitch*2, d += dst.pitchUV, a += src.pitch*2) {
            BYTE* s2 = s;
            BYTE* s2end = s2 + w*4;
            BYTE* d2 = d;
            BYTE* a2 = a;
            
            DWORD last_alpha = a2[0]+a2[0+src.pitch];
            for(; s2 < s2end; s2 += 8, d2++, a2 += 8) {
                unsigned int ia = (last_alpha + 2*(a2[0]+a2[0+src.pitch]) + a2[4] + a2[4+src.pitch] + 4 )>>3;
                last_alpha = a2[4] + a2[4+src.pitch];
                if(ia < 0xff) {
                    *d2 = ((*d2*ia)>>8) + ((s2[0]+s2[src.pitch])>>1);
                }
            }
        }
    }

    return S_OK;
}

HRESULT CMemSubPic::AlphaBltAxyuAxyv_Nv12(const RECT* pSrc, const RECT* pDst, SubPicDesc* pTarget)
{
    ONCER( SaveArgb2File(*pTarget, CRect(CPoint(0,0), m_size), SPD_DUMP_FILE".nv12") );
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
        dst.pitchUV = dst.pitch;
    }

    BYTE* ss[2];
    ss[0] = (BYTE*)src.bits + src.pitch*rs.top + rs.left*4;
    ss[1] = ss[0] + 4;

    if(!dst.bitsU || !dst.bitsV) {
        dst.bitsU = (BYTE*)dst.bits + dst.pitch*dst.h;
        dst.bitsV = dst.bitsU + 1;

        if(dst.type == MSP_NV21) {
            BYTE* p = dst.bitsU;
            dst.bitsU = dst.bitsV;
            dst.bitsV = p;
        }
    }

    BYTE* dd[2];
    dd[0] = dst.bitsU + dst.pitchUV*rd.top/2 + rd.left;
    dd[1] = dd[0]+1;

    if(rd.top > rd.bottom) {
        dd[0] = dst.bitsU + dst.pitchUV*(rd.top/2-1) + rd.left;
        dd[1] = dd[0]+1;
        dst.pitchUV = -dst.pitchUV;
    }

    for(ptrdiff_t i = 0; i < 2; i++) {
        s = ss[i];
        d = dd[i];
        BYTE* a = ss[0]+3;
        for(ptrdiff_t j = 0; j < h2; j++, s += src.pitch*2, d += dst.pitchUV, a += src.pitch*2) {
            BYTE* s2 = s;
            BYTE* s2end = s2 + w*4;
            BYTE* d2 = d;
            BYTE* a2 = a;
            DWORD last_alpha = a2[0]+a2[0+src.pitch];
            for(; s2 < s2end; s2 += 8, d2+=2, a2 += 8) {
                unsigned int ia = (last_alpha+2*(a2[0]+a2[0+src.pitch])+a2[4]+a2[4+src.pitch]+4)>>3;
                last_alpha = a2[4]+a2[4+src.pitch];
                if(ia < 0xff) {
                    *d2 = ((*d2*ia)>>8) + ((s2[0]+s2[src.pitch])>>1);
                }
            }
        }
    }
    
    ONCER( SaveArgb2File(*pTarget, CRect(CPoint(0,0), m_size), SPD_DUMP_FILE".nv12_2") );
    return S_OK;
}

HRESULT CMemSubPic::AlphaBltAnv12_P010( const RECT* pSrc, const RECT* pDst, SubPicDesc* pTarget )
{
    //fix me: check colorspace and log error
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

    const BYTE* sa = reinterpret_cast<const BYTE*>(src.bits) + src.pitch*rs.top + rs.left;            
    const BYTE* sy = sa + src.pitch*src.h;
    const BYTE* s_uv = sy + src.pitch*src.h;//UV
    return AlphaBltAnv12_P010(sa, sy, s_uv, src.pitch, d, dUV, dst.pitch, w, h);
}

HRESULT CMemSubPic::AlphaBltAnv12_Nv12( const RECT* pSrc, const RECT* pDst, SubPicDesc* pTarget )
{
    //fix me: check colorspace and log error
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

    const BYTE* sa = reinterpret_cast<const BYTE*>(src.bits) + src.pitch*rs.top + rs.left;            
    const BYTE* sy = sa + src.pitch*src.h;
    const BYTE* s_uv = sy + src.pitch*src.h;//UV

    return AlphaBltAnv12_Nv12(sa, sy, s_uv, src.pitch, d, dUV, dst.pitch, w, h);
}

STDMETHODIMP CMemSubPic::SetDirtyRectEx(CAtlList<CRect>* dirtyRectList )
{
    //if(m_spd.type == MSP_YUY2 || m_spd.type == MSP_YV12 || m_spd.type == MSP_IYUV || m_spd.type == MSP_AYUV)
    if(dirtyRectList!=NULL)
    {
        POSITION pos = dirtyRectList->GetHeadPosition();
        if(m_spd.type == MSP_AYUV_PLANAR || m_alpha_blt_dst_type==MSP_IYUV || m_alpha_blt_dst_type==MSP_YV12 
            || m_alpha_blt_dst_type==MSP_P010 || m_alpha_blt_dst_type==MSP_P016 
            || m_alpha_blt_dst_type==MSP_NV12 || m_alpha_blt_dst_type==MSP_NV21 )
        {
            while(pos!=NULL)
            {
                CRect& cRectSrc = dirtyRectList->GetNext(pos);
                cRectSrc.left &= ~15;
                cRectSrc.right = (cRectSrc.right+15)&~15;
                if(cRectSrc.right>m_spd.w)
                {
                    cRectSrc.right = m_spd.w;
                }
                cRectSrc.top &= ~1;
                cRectSrc.bottom = (cRectSrc.bottom+1)&~1;
                ASSERT(cRectSrc.bottom<=m_spd.h);
            }
        }
        else if(m_spd.type == MSP_XY_AUYV || m_alpha_blt_dst_type==MSP_YUY2)
        {
            while(pos!=NULL)
            {
                CRect& cRectSrc = dirtyRectList->GetNext(pos);
                cRectSrc.left &= ~3;
                cRectSrc.right = (cRectSrc.right+3)&~3;
                cRectSrc.right = cRectSrc.right < m_spd.w ? cRectSrc.right : m_spd.w;
                ASSERT((cRectSrc.right & 3)==0);
            }
        }
    }
    return __super::SetDirtyRectEx(dirtyRectList);
}

//
// static 
// 

void CMemSubPic::AlphaBltYv12Luma(byte* dst, int dst_pitch,
    int w, int h,
    const byte* sub, const byte* alpha, int sub_pitch)
{
    if( (
         ((reinterpret_cast<intptr_t>(alpha) ^ reinterpret_cast<intptr_t>(sub))
         |(reinterpret_cast<intptr_t>(alpha) ^ reinterpret_cast<intptr_t>(dst))
         | static_cast<intptr_t>(sub_pitch)
         | static_cast<intptr_t>(dst_pitch) ) & 15 )==0
        && w > 32)
    {
        int head = (16 - (reinterpret_cast<intptr_t>(alpha)&15))&15;
        int tail = (w-head) & 15;
        int w1 = w - head - tail;
        for(int i=0; i<h; i++, dst += dst_pitch, alpha += sub_pitch, sub += sub_pitch)
        {
            const BYTE* sa = alpha;
            const BYTE* s2 = sub;
            const BYTE* s2end_mod16 = s2 + w1;
            const BYTE* s2end = s2 + w;
            BYTE* d2 = dst;

            for( ; (reinterpret_cast<intptr_t>(s2)&15) != 0; s2++, sa++, d2++)
            {
                if(sa[0] < 0xff)
                {                    
                    d2[0] = ((d2[0]*sa[0])>>8) + s2[0];
                }
            }
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
        CMemSubPic::AlphaBltYv12LumaC(dst, dst_pitch, w, h, sub, alpha, sub_pitch);
    }
}

void CMemSubPic::AlphaBltYv12LumaC( byte* dst, int dst_pitch, int w, int h, const byte* sub, const byte* alpha, int sub_pitch )
{
    for(int i=0; i<h; i++, dst += dst_pitch, alpha += sub_pitch, sub += sub_pitch)
    {
        const BYTE* sa = alpha;
        const BYTE* s2 = sub;
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

void CMemSubPic::AlphaBltYv12Chroma(byte* dst_uv, int dst_pitch,
    int w, int chroma_h,
    const byte* src_uv, const byte* src_a, int src_pitch)
{
    if( (
        ((reinterpret_cast<intptr_t>(src_a) ^ reinterpret_cast<intptr_t>(src_uv)) 
        |(reinterpret_cast<intptr_t>(src_a) ^ (2*reinterpret_cast<intptr_t>(dst_uv)))
        | static_cast<intptr_t>(src_pitch) 
        | (2*static_cast<intptr_t>(dst_pitch)) ) & 15) ==0 &&
        w > 16)
    {
        int head = (16 - (reinterpret_cast<intptr_t>(src_a)&15))&15;
        int tail = (w-head) & 15;
        int w00 = w - head - tail;

        int pitch = src_pitch;
        for(int j = 0; j < chroma_h; j++, src_uv += src_pitch*2, src_a += src_pitch*2, dst_uv += dst_pitch)
        {
            hleft_vmid_mix_uv_yv12_c2(dst_uv, head, src_uv, src_a, src_pitch);
            hleft_vmid_mix_uv_yv12_sse2(dst_uv+(head>>1), w00, src_uv+head, src_a+head, src_pitch, head>0 ? -1 : 0);
            hleft_vmid_mix_uv_yv12_c2(dst_uv+((head+w00)>>1), tail, src_uv+head+w00, src_a+head+w00, src_pitch, (w00+head)>0 ? -1 : 0);
        }
    }
    else//fix me: only a workaround for non-mod-16 size video
    {
        AlphaBltYv12ChromaC(dst_uv, dst_pitch, w, chroma_h, src_uv, src_a, src_pitch);
    }
}

void CMemSubPic::AlphaBltYv12ChromaC( byte* dst, int dst_pitch, int w, int chroma_h, const byte* sub_chroma, const byte* alpha, int sub_pitch )
{
    for(int j = 0; j < chroma_h; j++, sub_chroma += sub_pitch*2, alpha += sub_pitch*2, dst += dst_pitch)
    {
        hleft_vmid_mix_uv_yv12_c(dst, w, sub_chroma, alpha, sub_pitch);
    }
}

HRESULT CMemSubPic::AlphaBltAnv12_P010( const BYTE* src_a, const BYTE* src_y, const BYTE* src_uv, int src_pitch, 
    BYTE* dst_y, BYTE* dst_uv, int dst_pitch, int w, int h )
{
    const BYTE* sa = src_a;
    if( (
        ((reinterpret_cast<intptr_t>(src_a) ^ reinterpret_cast<intptr_t>(src_y))         
        |(reinterpret_cast<intptr_t>(src_a) ^ reinterpret_cast<intptr_t>(dst_y))
        | static_cast<intptr_t>(src_pitch)
        | static_cast<intptr_t>(dst_pitch) ) & 15 )==0 && 
        w > 32 )
    {
        int head = (16 - reinterpret_cast<intptr_t>(src_a)&15)&15;
        int tail = (w - head) & 15;

        for(int i=0; i<h; i++, sa += src_pitch, src_y += src_pitch, dst_y += dst_pitch)
        {
            const BYTE* sa2 = sa;
            const BYTE* s2 = src_y;
            const BYTE* s2end_mod16 = s2 + (w&~15);
            BYTE* d2 = dst_y;
            WORD* d_w=reinterpret_cast<WORD*>(dst_y);

            switch( head )//important: it is safe since w > 16 
            {
            case 15:
#define _XY_MIX_ONE if(sa2[0] < 0xff) { d_w[0] = ((d_w[0]*sa2[0])>>8) + (s2[0]<<8); } sa2++;d_w++;s2++;
                _XY_MIX_ONE
            case 14:
                _XY_MIX_ONE
            case 13:
                _XY_MIX_ONE
            case 12:
                _XY_MIX_ONE
            case 11:
                _XY_MIX_ONE
            case 10:
                _XY_MIX_ONE
            case 9:
                _XY_MIX_ONE
            case 8:
                _XY_MIX_ONE
            case 7:
                _XY_MIX_ONE
            case 6:
                _XY_MIX_ONE
            case 5:
                _XY_MIX_ONE
            case 4:
                _XY_MIX_ONE
            case 3:
                _XY_MIX_ONE
            case 2:
                _XY_MIX_ONE
            case 1://fall through on purpose
                _XY_MIX_ONE
            }
            for(; s2 < s2end_mod16; s2+=16, sa2+=16, d_w+=16)
            {
                mix_16_y_p010_sse2( reinterpret_cast<BYTE*>(d_w), s2, sa2);
            }
            switch( tail )//important: it is safe since w > 16 
            {
            case 15:
                _XY_MIX_ONE
            case 14:
                _XY_MIX_ONE
            case 13:
                _XY_MIX_ONE
            case 12:
                _XY_MIX_ONE
            case 11:
                _XY_MIX_ONE
            case 10:
                _XY_MIX_ONE
            case 9:
                _XY_MIX_ONE
            case 8:
                _XY_MIX_ONE
            case 7:
                _XY_MIX_ONE
            case 6:
                _XY_MIX_ONE
            case 5:
                _XY_MIX_ONE
            case 4:
                _XY_MIX_ONE
            case 3:
                _XY_MIX_ONE
            case 2:
                _XY_MIX_ONE
            case 1://fall through on purpose
                _XY_MIX_ONE
            }
        }
    }
    else //fix me: only a workaround for non-mod-16 size video
    {
        for(int i=0; i<h; i++, sa += src_pitch, src_y += src_pitch, dst_y += dst_pitch)
        {
            const BYTE* sa2 = sa;
            const BYTE* s2 = src_y;
            const BYTE* s2end = s2 + w;
            WORD* d_w = reinterpret_cast<WORD*>(dst_y);
            for(; s2 < s2end; s2+=1, sa2+=1, d_w+=1)
            {
                if(sa2[0] < 0xff)
                {                            
                    d_w[0] = ((d_w[0]*sa2[0])>>8) + (s2[0]<<8);
                }
            }
        }
    }
    //UV
    int h2 = h/2;
    BYTE* d = dst_uv;
    if( (
        ((reinterpret_cast<intptr_t>(src_a) ^ reinterpret_cast<intptr_t>(src_uv)) 
        |(reinterpret_cast<intptr_t>(src_a) ^ reinterpret_cast<intptr_t>(dst_uv))
        | static_cast<intptr_t>(src_pitch) 
        | static_cast<intptr_t>(dst_pitch) ) & 15) ==0 &&
        w > 16 )
    {
        int head = (16-(reinterpret_cast<intptr_t>(src_a)&15))&15;
        int tail = (w-head) & 15;
        int w00 = w - head - tail;

        ASSERT(w>0);//the calls to mix may failed if w==0
        for(int j = 0; j < h2; j++, src_uv += src_pitch, src_a += src_pitch*2, d += dst_pitch)
        {
            hleft_vmid_mix_uv_p010_c2(d, head, src_uv, src_a, src_pitch);
            hleft_vmid_mix_uv_p010_sse2(d+2*head, w00, src_uv+head, src_a+head, src_pitch, head>0 ? -1 : 0);
            hleft_vmid_mix_uv_p010_c2(d+2*(head+w00), tail, src_uv+head+w00, src_a+head+w00, src_pitch, (w00+head)>0 ? -1 : 0);
        }
    }
    else
    {        
        for(int j = 0; j < h2; j++, src_uv += src_pitch, src_a += src_pitch*2, d += dst_pitch)
        {
            hleft_vmid_mix_uv_p010_c(d, w, src_uv, src_a, src_pitch);
        }
    }
#ifndef _WIN64
    // TODOX64 : fixme!
    _mm_empty();
#endif
    return S_OK;
}

HRESULT CMemSubPic::AlphaBltAnv12_P010_C( const BYTE* src_a, const BYTE* src_y, const BYTE* src_uv, int src_pitch, BYTE* dst_y, BYTE* dst_uv, int dst_pitch, int w, int h )
{
    const BYTE* sa = src_a;
    for(int i=0; i<h; i++, sa += src_pitch, src_y += src_pitch, dst_y += dst_pitch)
    {
        const BYTE* sa2 = sa;
        const BYTE* s2 = src_y;
        const BYTE* s2end = s2 + w;
        WORD* d2 = reinterpret_cast<WORD*>(dst_y);
        for(; s2 < s2end; s2+=1, sa2+=1, d2+=1)
        {
            if(sa2[0] < 0xff)
            {                            
                d2[0] = ((d2[0]*sa2[0])>>8) + (s2[0]<<8);
            }
        }
    }
    //UV
    int h2 = h/2;
    BYTE* d = dst_uv;
    for(int j = 0; j < h2; j++, src_uv += src_pitch, src_a += src_pitch*2, d += dst_pitch)
    {
        hleft_vmid_mix_uv_p010_c(d, w, src_uv, src_a, src_pitch);
    }
    return S_OK;
}

HRESULT CMemSubPic::AlphaBltAnv12_Nv12( const BYTE* src_a, const BYTE* src_y, const BYTE* src_uv, int src_pitch, 
    BYTE* dst_y, BYTE* dst_uv, int dst_pitch, int w, int h )
{
    AlphaBltYv12Luma( dst_y, dst_pitch, w, h, src_y, src_a, src_pitch );

    int h2 = h/2;
    if( (
         ((reinterpret_cast<intptr_t>(src_a) ^ reinterpret_cast<intptr_t>(src_uv)) 
         |(reinterpret_cast<intptr_t>(src_a) ^ reinterpret_cast<intptr_t>(dst_uv))
         | static_cast<intptr_t>(src_pitch) 
         | static_cast<intptr_t>(dst_pitch) ) & 15) ==0 &&
        w > 16 )
    {
        BYTE* d = dst_uv;

        int head = (16-(reinterpret_cast<intptr_t>(src_a)&15))&15;
        int tail = (w-head) & 15;
        int w00 = w - head - tail;

        ASSERT(w>0);//the calls to mix may failed if w==0
        for(int j = 0; j < h2; j++, src_uv += src_pitch, src_a += src_pitch*2, d += dst_pitch)
        {
            hleft_vmid_mix_uv_nv12_c2(d, head, src_uv, src_a, src_pitch);
            hleft_vmid_mix_uv_nv12_sse2(d+head, w00, src_uv+head, src_a+head, src_pitch, head>0 ? -1 : 0);
            hleft_vmid_mix_uv_nv12_c2(d+head+w00, tail, src_uv+head+w00, src_a+head+w00, src_pitch, (w00+head)>0 ? -1 : 0);
        }
#ifndef _WIN64
        // TODOX64 : fixme!
        _mm_empty();
#endif
    }
    else
    {
        BYTE* d = dst_uv;
        for(int j = 0; j < h2; j++, src_uv += src_pitch, src_a += src_pitch*2, d += dst_pitch)
        {
            hleft_vmid_mix_uv_nv12_c(d, w, src_uv, src_a, src_pitch);
        }
    }
    return S_OK;
}

HRESULT CMemSubPic::AlphaBltAnv12_Nv12_C( const BYTE* src_a, const BYTE* src_y, const BYTE* src_uv, int src_pitch, BYTE* dst_y, BYTE* dst_uv, int dst_pitch, int w, int h )
{
    AlphaBltYv12LumaC( dst_y, dst_pitch, w, h, src_y, src_a, src_pitch );
    int h2 = h/2;
    BYTE* d = dst_uv;
    for(int j = 0; j < h2; j++, src_uv += src_pitch, src_a += src_pitch*2, d += dst_pitch)
    {
        hleft_vmid_mix_uv_nv12_c(d, w, src_uv, src_a, src_pitch);
    }
    return S_OK;
}

void CMemSubPic::SubsampleAndInterlace( BYTE* dst, const BYTE* u, const BYTE* v, int h, int w, int pitch )
{
    for (int i=0;i<h;i+=2)
    {
        hleft_vmid_subsample_and_interlace_2_line_sse2(dst, u, v, w, pitch);
        u += 2*pitch;
        v += 2*pitch;
        dst += pitch;
    }
}

void CMemSubPic::SubsampleAndInterlaceC( BYTE* dst, const BYTE* u, const BYTE* v, int h, int w, int pitch )
{
    for (int i=0;i<h;i+=2)
    {
        hleft_vmid_subsample_and_interlace_2_line_c(dst, u, v, w, pitch);
        u += 2*pitch;
        v += 2*pitch;
        dst += pitch;
    }
}

void CMemSubPic::AlphaBlt_YUY2(int w, int h, BYTE* d, int dstpitch, PCUINT8 s, int srcpitch)
{
#ifdef _WIN64
    AlphaBlt_YUY2_C(w, h, d, dstpitch, s, srcpitch);
#else
    AlphaBlt_YUY2_MMX(w, h, d, dstpitch, s, srcpitch);
#endif
}

HRESULT CMemSubPic::FlipAlphaValue( const CRect& dirtyRect )
{
    ONCER( SaveArgb2File(m_spd, CRect(CPoint(0,0), m_size), SPD_DUMP_FILE".argb_f") );
    XY_LOG_TRACE(dirtyRect);
    const CRect& cRect = dirtyRect;
    int w = cRect.Width(), h = cRect.Height();
    if (w<=0 || h<=0)
    {
        return S_OK;
    }
    ASSERT(m_spd.type == MSP_RGBA);
    if(m_spd.type != MSP_RGBA)
    {
        XY_LOG_ERROR("Unexpected color type "<<m_spd.type);
    }
    BYTE* top = (BYTE*)m_spd.bits + m_spd.pitch*(cRect.top) + cRect.left*4;
    XyBitmap::FlipAlphaValue(top, w, h, m_spd.pitch);
    return S_OK;
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
            m_type = MSP_XY_AUYV;
            break;
        case MSP_AYUV:
            m_type = MSP_AYUV;
            break;
        case MSP_IYUV:
        case MSP_YV12:
        case MSP_P010:
        case MSP_P016:
        case MSP_NV12:
        case MSP_NV21:
            m_type = MSP_AYUV_PLANAR;
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
    spd.bits = DEBUG_NEW BYTE[spd.pitch*spd.h];
    if(!spd.bits) {
        return false;
    }    
    *ppSubPic = DEBUG_NEW CMemSubPic(spd, m_alpha_blt_dst_type);
    if(!(*ppSubPic)) {
        return false;
    }
    (*ppSubPic)->AddRef();
    return true;
}
