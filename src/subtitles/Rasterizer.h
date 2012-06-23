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
    bool operator==(const PathData& rhs)const;

    void _TrashPath();
    bool BeginPath(HDC hdc);
    bool EndPath(HDC hdc);
    bool PartialBeginPath(HDC hdc, bool bClearPath);
    bool PartialEndPath(HDC hdc, long dx, long dy);
    
    void AlignLeftTop(CPoint *left_top, CSize *size);

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

    struct Edge {
        int next;
        int posandflag;
    } *mpEdgeBuffer;
    unsigned mEdgeHeapSize;
    unsigned mEdgeNext;

    unsigned int* mpScanBuffer;

    typedef unsigned char byte;

private:
    void _ReallocEdgeBuffer(int edges);
    void _EvaluateBezier(const PathData& path_data, int ptbase, bool fBSpline);
    void _EvaluateLine(const PathData& path_data, int pt1idx, int pt2idx);
    void _EvaluateLine(int x0, int y0, int x1, int y1);	

public:
    ScanLineData();
    virtual ~ScanLineData();

    bool ScanConvert(const PathData& path_data, const CSize& size);
    void DeleteOutlines();

    friend class Rasterizer;
    friend class ScanLineData2;
};

typedef ::boost::shared_ptr<const ScanLineData> SharedPtrConstScanLineData;
typedef ::boost::shared_ptr<ScanLineData> SharedPtrScanLineData;

class ScanLineData2
{
public:
    ScanLineData2():mPathOffsetX(0),mPathOffsetY(0),mWideBorder(0){}
    ScanLineData2(const CPoint& left_top, const SharedPtrConstScanLineData& scan_line_data)
        :m_scan_line_data(scan_line_data)
        ,mPathOffsetX(left_top.x)
        ,mPathOffsetY(left_top.y)
    {
    }
    void SetOffset(const CPoint& offset)
    {
        mPathOffsetX = offset.x;
        mPathOffsetY = offset.y;
    }
    bool CreateWidenedRegion(int borderX, int borderY);
private:
    typedef ScanLineData::tSpanBuffer tSpanBuffer;
    typedef ScanLineData::tSpan tSpan;

    SharedPtrConstScanLineData m_scan_line_data;
    int mPathOffsetX, mPathOffsetY;	
    tSpanBuffer mWideOutline;
    int mWideBorder;

private:
    static void _OverlapRegion(tSpanBuffer& dst, const tSpanBuffer& src, int dx, int dy);

    friend class Rasterizer;
};

typedef ::boost::shared_ptr<const ScanLineData2> SharedPtrConstScanLineData2;
typedef ::boost::shared_ptr<ScanLineData2> SharedPtrScanLineData2;

typedef ::boost::shared_ptr<BYTE> SharedPtrByte;

struct Overlay
{
public:
    Overlay()
    {
        mOffsetX=mOffsetY=mWidth=mHeight=0;
        mOverlayWidth=mOverlayHeight=mOverlayPitch=0;
        mfWideOutlineEmpty = false;
    }
    ~Overlay()
    {
        CleanUp();       
    }

    void CleanUp()
    {
        mBody.reset((BYTE*)NULL);
        mBorder.reset((BYTE*)NULL);
        mOffsetX=mOffsetY=mWidth=mHeight=0;
        mOverlayWidth=mOverlayHeight=mOverlayPitch=0;
        mfWideOutlineEmpty = false;
    }

    void FillAlphaMash(byte* outputAlphaMask, bool fBody, bool fBorder, 
        int x, int y, int w, int h, 
        const byte* pAlphaMask, int pitch, DWORD color_alpha);

    Overlay* GetSubpixelVariance(unsigned int xshift, unsigned int yshift);
public:
    SharedPtrByte mBody;
    SharedPtrByte mBorder;
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

struct GrayImage2
{
public:
    CPoint left_top;
    CSize size;
    int pitch;
    SharedPtrByte data;
};

typedef ::boost::shared_ptr<GrayImage2> SharedPtrGrayImage2;

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
    static bool Rasterize(const ScanLineData2& scan_line_data2, int xsub, int ysub, SharedPtrOverlay overlay);
    static bool Blur(const Overlay& input_overlay, int fBlur, double fGaussianBlur, SharedPtrOverlay output_overlay);

    static SharedPtrByte CompositeAlphaMask(SubPicDesc& spd, const SharedPtrOverlay& overlay, const CRect& clipRect, 
        const GrayImage2* alpha_mask, 
        int xsub, int ysub, const DWORD* switchpts, bool fBody, bool fBorder, 
        CRect *outputDirtyRect);

    static CRect DryDraw(SubPicDesc& spd, 
        SharedPtrOverlay overlay, 
        const CRect& clipRect, 
        byte* pAlphaMask, 
        int xsub, int ysub, 
        const DWORD* switchpts, bool fBody, bool fBorder);

    static void Draw(SubPicDesc& spd, 
        SharedPtrOverlay overlay, 
        const CRect& clipRect, 
        byte* s_base, 
        int xsub, int ysub, 
        const DWORD* switchpts, bool fBody, bool fBorder);
		
	static void FillSolidRect(SubPicDesc& spd, int x, int y, int nWidth, int nHeight, DWORD lColor);
	
};

