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

struct XySubPicPlan {
    int w, h, pitch;
    BYTE *bits;
};

class BaseAlphaBlender
{
public:
    virtual void AlphaBlendNoClip(const XySubPicPlan src_plans[], const CRect& src_rect, 
        const XySubPicPlan dst_plans[], CPoint dst_pos) = 0;
    virtual ~BaseAlphaBlender() {}

    void AlphaBlend(const XySubPicPlan src_plans[], CRect src_rect, 
        const XySubPicPlan dst_plans[], CPoint dst_pos);
    void AlphaBlend(const XySubPicPlan src_plans[], const XySubPicPlan dst_plans[], POINT dst_pos);
    void AlphaBlend(const XySubPicPlan src_plans[], const XySubPicPlan dst_plans[], POINT dst_pos, SIZE size);
public:
    static void DefaultAlphaBlend(const XySubPicPlan src_plans[], const CRect& src_rect, 
        const XySubPicPlan dst_plans[], const CPoint& dst_pos)
    {
        return s_default_alpha_blender->AlphaBlend(src_plans, src_rect, dst_plans, dst_pos);
    }
    static void DefaultAlphaBlend(const XySubPicPlan src_plans[], const XySubPicPlan dst_plans[], const CPoint& dst_pos)
    {
        return s_default_alpha_blender->AlphaBlend(src_plans, dst_plans, dst_pos);
    }
    static void DefaultAlphaBlend(const XySubPicPlan src_plans[], const XySubPicPlan dst_plans[], const POINT& dst_pos, const SIZE& size)
    {
        return s_default_alpha_blender->AlphaBlend(src_plans, dst_plans, dst_pos, size);
    }
    static BaseAlphaBlender* SetupDefaultAlphaBlender(int src_type, int dst_type, int cpu_flags);
    static BaseAlphaBlender* CreateAlphaBlender(int src_type, int dst_type, int cpu_flags);

    static inline void ClipTopLeftToZero(CPoint *src_point, CPoint *dst_point);
    static inline void LimitPoint( int limit_x, int limit_y, CPoint &p );
    static void ClipRect(int src_w, int src_h, int dst_w, int dst_h, CRect *src_point, CPoint *dst_point);
private:
    static BaseAlphaBlender* s_default_alpha_blender;
};

class AlphaBlenderArgbC : public BaseAlphaBlender
{
public:
    AlphaBlenderArgbC(){}
    ~AlphaBlenderArgbC(){}
    void AlphaBlendNoClip(const XySubPicPlan src_plans[], const CRect& src_rect, 
        const XySubPicPlan dst_plans[], CPoint dst_pos);
    virtual void AlphaBlend( const BYTE *src, int w, int h, int pitch, BYTE *dst, int dst_pitch );
};
