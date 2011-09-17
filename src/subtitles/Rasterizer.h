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

#include <vector>
#include "..\SubPic\ISubPic.h"
#include "xy_malloc.h"

#ifdef _OPENMP
#include <omp.h>
#endif

#define PT_MOVETONC 0xfe
#define PT_BSPLINETO 0xfc
#define PT_BSPLINEPATCHTO 0xfa

typedef struct {
    int left, top;
    int w, h;                   // width, height
    unsigned char *buffer;      // w x h buffer
} Bitmap;

struct Overlay
{
public:
    Overlay()
    {
        memset(this, 0, sizeof(*this));
    }
    ~Overlay()
    {
        xy_free(mpOverlayBuffer.base);
        memset(&mpOverlayBuffer, 0, sizeof(mpOverlayBuffer));        
    }

    void CleanUp()
    {
        xy_free(mpOverlayBuffer.base);
        memset(&mpOverlayBuffer, 0, sizeof(mpOverlayBuffer));        
        mOverlayWidth=mOverlayHeight=mOverlayPitch=0;
    }

    void _Init_mpOverlayDraw(const byte* pBody, const byte* pBorder,
        int x, int y, int w, int h,
        const byte* pAlphaMask, int pitch,
        DWORD color_alpha);
    byte*_GetAlphaMash1( bool fBody, bool fBorder, int x, int y, int w, int h, const byte* pAlphaMask, int pitch, DWORD color_alpha)
    {
        mpOverlayBuffer.draw = (byte*)xy_malloc(mOverlayPitch * mOverlayHeight);
        byte* result = NULL;
        if(!fBorder && fBody && pAlphaMask==NULL)
        {
            //        result = mpOverlayBuffer.body + mOverlayPitch*y + x;
            _Init_mpOverlayDraw(mpOverlayBuffer.body, NULL, x, y, w, h, pAlphaMask, pitch, color_alpha);
            result = mpOverlayBuffer.draw + mOverlayPitch*y + x;
        }
        else if(/*fBorder &&*/ fBody && pAlphaMask==NULL)
        {
            //        result = mpOverlayBuffer.border + mOverlayPitch*y + x;
            _Init_mpOverlayDraw(NULL, mpOverlayBuffer.border, x, y, w, h, pAlphaMask, pitch, color_alpha);
            result = mpOverlayBuffer.draw + mOverlayPitch*y + x;
        }
        else if(!fBody && fBorder /* pAlphaMask==NULL or not*/)
        {
            int offset = mOverlayPitch*y + x;
            _Init_mpOverlayDraw(mpOverlayBuffer.body, mpOverlayBuffer.border, x, y, w, h, pAlphaMask, pitch, color_alpha);
            result = mpOverlayBuffer.draw + offset;
        }
        else if(!fBorder && fBody && pAlphaMask!=NULL)
        {
            _Init_mpOverlayDraw(mpOverlayBuffer.body, NULL, x, y, w, h, pAlphaMask, pitch, color_alpha);
            result = mpOverlayBuffer.draw + mOverlayPitch*y + x;
        }
        else if(fBorder && fBody && pAlphaMask!=NULL)
        {
            _Init_mpOverlayDraw(NULL, mpOverlayBuffer.border, x, y, w, h, pAlphaMask, pitch, color_alpha);
            result = mpOverlayBuffer.draw + mOverlayPitch*y + x;
        }
        else
        {
            //should NOT happen
            ASSERT(0);
        }
        return result;
    }

    struct {
        byte *base;
        byte *body;
        byte *border;
        byte *draw;
    } mpOverlayBuffer;
    int mOffsetX, mOffsetY;
        
    int mOverlayWidth, mOverlayHeight,mOverlayPitch;
};


class Rasterizer
{
	bool fFirstSet;
	CPoint firstp, lastp;

protected:
	BYTE* mpPathTypes;
	POINT* mpPathPoints;
	int mPathPoints;

private:
	int mWidth, mHeight;

	typedef std::pair<unsigned __int64, unsigned __int64> tSpan;
	typedef std::vector<tSpan> tSpanBuffer;

	tSpanBuffer mOutline;
	tSpanBuffer mWideOutline;
	int mWideBorder;

	struct Edge {
		int next;
		int posandflag;
	} *mpEdgeBuffer;
	unsigned mEdgeHeapSize;
	unsigned mEdgeNext;

	unsigned int* mpScanBuffer;

	typedef unsigned char byte;

protected:
	//struct {
	//	byte *base;
	//	byte *body;
	//	byte *border;
	//	byte *draw;
	//} mpOverlayBuffer;

	//bool mpOverlayDraw_initiated;
	//int mOverlayWidth, mOverlayHeight,mOverlayPitch;
	int mPathOffsetX, mPathOffsetY;	

private:
	void _TrashPath();
	void _ReallocEdgeBuffer(int edges);
	void _EvaluateBezier(int ptbase, bool fBSpline);
	void _EvaluateLine(int pt1idx, int pt2idx);
	void _EvaluateLine(int x0, int y0, int x1, int y1);	
	static void _OverlapRegion(tSpanBuffer& dst, tSpanBuffer& src, int dx, int dy);

public:
	Rasterizer();
	virtual ~Rasterizer();

	bool BeginPath(HDC hdc);
	bool EndPath(HDC hdc);
	bool PartialBeginPath(HDC hdc, bool bClearPath);
	bool PartialEndPath(HDC hdc, long dx, long dy);
	bool ScanConvert();
	bool CreateWidenedRegion(int borderX, int borderY);
	void DeleteOutlines();
	bool Rasterize(int xsub, int ysub, int fBlur, double fGaussianBlur, Overlay* overlay);
	CRect Draw(SubPicDesc& spd, Overlay* overlay, CRect& clipRect, byte* pAlphaMask, int xsub, int ysub, const DWORD* switchpts, bool fBody, bool fBorder);
};

