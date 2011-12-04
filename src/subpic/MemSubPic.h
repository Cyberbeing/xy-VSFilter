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

class CMemSubPic : public CSubPicExImpl
{
protected:
    int m_type;
    bool converted;

    int m_alpha_blt_dst_type;

	SubPicDesc m_spd;

protected:
	STDMETHODIMP_(void*) GetObject() const; // returns SubPicDesc*
    
    STDMETHODIMP AlphaBltAyuv_P010(const RECT* pSrc, const RECT* pDst, SubPicDesc* pTarget);
    STDMETHODIMP AlphaBltAyuv_Yv12(const RECT* pSrc, const RECT* pDst, SubPicDesc* pTarget);
    STDMETHODIMP AlphaBltOther(const RECT* pSrc, const RECT* pDst, SubPicDesc* pTarget);
    
    STDMETHODIMP UnlockRGBA_YUV(CAtlList<CRect>* dirtyRectList);
    STDMETHODIMP UnlockOther(CAtlList<CRect>* dirtyRectList);

    void SubsampleAndInterlace( const CRect& cRect );
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
	STDMETHODIMP Unlock(CAtlList<CRect>* dirtyRectList);
	STDMETHODIMP SetDirtyRectEx(CAtlList<CRect>* dirtyRectList);    
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


