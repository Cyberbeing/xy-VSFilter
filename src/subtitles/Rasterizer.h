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

#include <boost/smart_ptr.hpp>
#include <vector>
#include "../SubPic/ISubPic.h"
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

struct PathData
{
public:
    PathData();
    ~PathData();

    PathData(const PathData&);//important! disable default copy constructor
    const PathData& operator=(const PathData&);
    void _TrashPath();
    bool BeginPath(HDC hdc);
    bool EndPath(HDC hdc);
    bool PartialBeginPath(HDC hdc, bool bClearPath);
    bool PartialEndPath(HDC hdc, long dx, long dy);

    BYTE* mpPathTypes;
    POINT* mpPathPoints;
    int mPathPoints;
};

typedef ::boost::shared_ptr<const PathData> SharedPtrConstPathData;
typedef ::boost::shared_ptr<PathData> SharedPtrPathData;

class ScanLineData
{
    bool fFirstSet;
    CPoint firstp, lastp;

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
    int mPathOffsetX, mPathOffsetY;	

private:
    void _ReallocEdgeBuffer(int edges);
    void _EvaluateBezier(const PathData& path_data, int ptbase, bool fBSpline);
    void _EvaluateLine(const PathData& path_data, int pt1idx, int pt2idx);
    void _EvaluateLine(int x0, int y0, int x1, int y1);	
    static void _OverlapRegion(tSpanBuffer& dst, tSpanBuffer& src, int dx, int dy);

public:
    ScanLineData();
    virtual ~ScanLineData();

    bool ScanConvert(SharedPtrPathData path_data);
    bool CreateWidenedRegion(int borderX, int borderY);
    void DeleteOutlines();

    friend class Rasterizer;
};

typedef ::boost::shared_ptr<const ScanLineData> SharedPtrConstScanLineData;
typedef ::boost::shared_ptr<ScanLineData> SharedPtrScanLineData;

typedef ::boost::shared_ptr<BYTE> SharedPtrByte;

struct Overlay
{
public:
    Overlay()
    {
        memset(this, 0, sizeof(*this));//todo: fix me. not safe.
    }
    ~Overlay()
    {
        CleanUp();       
    }

    void CleanUp()
    {
        xy_free(mpOverlayBuffer.base);
        memset(&mpOverlayBuffer, 0, sizeof(mpOverlayBuffer));        
        mOverlayWidth=mOverlayHeight=mOverlayPitch=0;
        mfWideOutlineEmpty = false;
    }

    void FillAlphaMash(byte* outputAlphaMask, bool fBody, bool fBorder, 
        int x, int y, int w, int h, 
        const byte* pAlphaMask, int pitch, DWORD color_alpha);

    Overlay* GetSubpixelVariance(unsigned int xshift, unsigned int yshift);

    struct {
        byte *base;
        byte *body;
        byte *border;
    } mpOverlayBuffer;
    int mOffsetX, mOffsetY;
    int mWidth, mHeight;
        
    int mOverlayWidth, mOverlayHeight, mOverlayPitch;

    bool mfWideOutlineEmpty;//specially for blur
private:
    void _DoFillAlphaMash(byte* outputAlphaMask, const byte* pBody, const byte* pBorder,
        int x, int y, int w, int h,
        const byte* pAlphaMask, int pitch, DWORD color_alpha);
};

typedef ::boost::shared_ptr<Overlay> SharedPtrOverlay;

typedef ::boost::shared_array<BYTE> SharedArrayByte;

struct DrawItem
{
    SharedPtrOverlay overlay;
    CRect clip_rect;
    SharedArrayByte alpha_mask;
    int xsub;
    int ysub;
    DWORD switchpts[6];
    bool fBody;
    bool fBorder;
};

typedef ::boost::shared_ptr<DrawItem> SharedPtrDrawItem;

class Rasterizer
{
private:
    typedef unsigned char byte;
    
    struct DM
    {
        enum
        {
            SSE2 = 1,
            ALPHA_MASK = 1<<1,
            SINGLE_COLOR = 1<<2,
            BODY = 1<<3,
            AYUV_PLANAR = 1<<4
        };
    };
public:    
    static bool Rasterize(const ScanLineData& scan_line_data, int xsub, int ysub, SharedPtrOverlay overlay);
    static bool Blur(const Overlay& input_overlay, int fBlur, double fGaussianBlur, SharedPtrOverlay output_overlay);

    static DrawItem* CreateDrawItem(SubPicDesc& spd, 
        SharedPtrOverlay overlay, 
        const CRect& clipRect, 
        SharedArrayByte pAlphaMask, 
        int xsub, int ysub, 
        const DWORD* switchpts, bool fBody, bool fBorder);

    static SharedPtrByte CompositeAlphaMask(SubPicDesc& spd, SharedPtrOverlay overlay, const CRect& clipRect, byte* pAlphaMask, 
        int xsub, int ysub, const DWORD* switchpts, bool fBody, bool fBorder, 
        CRect *outputDirtyRect);
    static CRect Draw(SubPicDesc& spd, DrawItem& draw_item);

    static CRect DryDraw(SubPicDesc& spd, DrawItem& draw_item);

    static CRect DryDraw(SubPicDesc& spd, 
        SharedPtrOverlay overlay, 
        const CRect& clipRect, 
        byte* pAlphaMask, 
        int xsub, int ysub, 
        const DWORD* switchpts, bool fBody, bool fBorder);

    static CRect Draw(SubPicDesc& spd, 
        SharedPtrOverlay overlay, 
        const CRect& clipRect, 
        byte* pAlphaMask, 
        int xsub, int ysub, 
        const DWORD* switchpts, bool fBody, bool fBorder);
		
	static void FillSolidRect(SubPicDesc& spd, int x, int y, int nWidth, int nHeight, DWORD lColor);
	
};

