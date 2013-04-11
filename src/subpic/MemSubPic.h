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

#pragma once

#include "SubPicImpl.h"

// CMemSubPic

typedef const UINT8 CUINT8, *PCUINT8;

class CMemSubPic : public CSubPicExImpl
{
public:
    static void AlphaBltYv12Luma(byte* dst, int dst_pitch, int w, int h, const byte* sub, const byte* alpha, int sub_pitch);
    static void AlphaBltYv12LumaC(byte* dst, int dst_pitch, int w, int h, const byte* sub, const byte* alpha, int sub_pitch);

    static void AlphaBltYv12Chroma(byte* dst, int dst_pitch, int w, int chroma_h, const byte* sub_chroma, 
        const byte* alpha, int sub_pitch);
    static void AlphaBltYv12ChromaC(byte* dst, int dst_pitch, int w, int chroma_h, const byte* sub_chroma, 
        const byte* alpha, int sub_pitch);

    static HRESULT AlphaBltAnv12_P010(const BYTE* src_a, const BYTE* src_y, const BYTE* src_uv, int src_pitch,
        BYTE* dst_y, BYTE* dst_uv, int dst_pitch,
        int w, int h);
    static HRESULT AlphaBltAnv12_P010_C(const BYTE* src_a, const BYTE* src_y, const BYTE* src_uv, int src_pitch,
        BYTE* dst_y, BYTE* dst_uv, int dst_pitch,
        int w, int h);
    static HRESULT AlphaBltAnv12_Nv12(const BYTE* src_a, const BYTE* src_y, const BYTE* src_uv, int src_pitch,
        BYTE* dst_y, BYTE* dst_uv, int dst_pitch,
        int w, int h);
    static HRESULT AlphaBltAnv12_Nv12_C(const BYTE* src_a, const BYTE* src_y, const BYTE* src_uv, int src_pitch,
        BYTE* dst_y, BYTE* dst_uv, int dst_pitch,
        int w, int h);

    static void AlphaBlt_YUY2(int w, int h, BYTE* d, int dstpitch, PCUINT8 s, int srcpitch);

    static void SubsampleAndInterlace(BYTE* dst, const BYTE* u, const BYTE* v, int h, int w, int pitch);
    static void SubsampleAndInterlaceC(BYTE* dst, const BYTE* u, const BYTE* v, int h, int w, int pitch);
public:
    CMemSubPic(SubPicDesc& spd, int alpha_blt_dst_type);
    virtual ~CMemSubPic();

    // ISubPic
    STDMETHODIMP GetDesc(SubPicDesc& spd) const;
    STDMETHODIMP ClearDirtyRect(DWORD color);
    STDMETHODIMP Lock(SubPicDesc& spd);
    STDMETHODIMP AlphaBlt(const RECT* pSrc, const RECT* pDst, SubPicDesc* pTarget);

    // ISubPicEx
    STDMETHODIMP CopyTo(ISubPicEx* pSubPic);
    STDMETHODIMP UnlockEx(CAtlList<CRect>* dirtyRectList);
    STDMETHODIMP SetDirtyRectEx(CAtlList<CRect>* dirtyRectList);

    //
    HRESULT FlipAlphaValue(const CRect& dirtyRect);

protected:
    bool converted;

    int m_alpha_blt_dst_type;

    SubPicDesc m_spd;

protected:
	STDMETHODIMP_(void*) GetObject() const; // returns SubPicDesc*
    
    HRESULT AlphaBltAxyuAxyv_P010(const RECT* pSrc, const RECT* pDst, SubPicDesc* pTarget);
    HRESULT AlphaBltAxyuAxyv_Yv12(const RECT* pSrc, const RECT* pDst, SubPicDesc* pTarget);
    HRESULT AlphaBltAxyuAxyv_Nv12(const RECT* pSrc, const RECT* pDst, SubPicDesc* pTarget);
    HRESULT AlphaBltAnv12_P010(const RECT* pSrc, const RECT* pDst, SubPicDesc* pTarget);   
    HRESULT AlphaBltAnv12_Nv12(const RECT* pSrc, const RECT* pDst, SubPicDesc* pTarget);
    HRESULT AlphaBltOther(const RECT* pSrc, const RECT* pDst, SubPicDesc* pTarget);

    HRESULT UnlockRGBA_YUV(CAtlList<CRect>* dirtyRectList);
    HRESULT UnlockOther(CAtlList<CRect>* dirtyRectList);

    void SubsampleAndInterlace( const CRect& cRect, bool u_first );

    friend class XySubRenderFrameWrapper;
};

// CMemSubPicAllocator

class CMemSubPicAllocator : public CSubPicExAllocatorImpl
{
	int m_type;
    int m_alpha_blt_dst_type;
	CSize m_maxsize;

	bool AllocEx(bool fStatic, ISubPicEx** ppSubPic);

public:
	CMemSubPicAllocator(int alpha_blt_dst_type, SIZE maxsize, int type=-1);
};


