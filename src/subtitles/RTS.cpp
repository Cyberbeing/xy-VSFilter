/*
 *  Copyright (C) 2003-2006 Gabest
 *  http://www.gabest.org
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
#include <math.h>
#include <time.h>
#include "RTS.h"
#include "draw_item.h"
#include "cache_manager.h"
#include "../subpic/color_conv_table.h"
#include "subpixel_position_controler.h"

// WARNING: this isn't very thread safe, use only one RTS a time.
static HDC g_hDC;
static int g_hDC_refcnt = 0;

enum XY_MSP_SUBTYPE {XY_AYUV, XY_AUYV};
static inline DWORD rgb2yuv(DWORD argb, XY_MSP_SUBTYPE type)
{
    if(type==XY_AYUV)
        return ColorConvTable::Argb2Ayuv(argb);
    else
        return ColorConvTable::Argb2Auyv(argb);
}

static long revcolor(long c)
{
    return ((c&0xff0000)>>16) + (c&0xff00) + ((c&0xff)<<16);
}

// Skip all leading whitespace
inline CStringW::PCXSTR SkipWhiteSpaceLeft(const CStringW& str)
{
    CStringW::PCXSTR psz = str.GetString();

    while( iswspace( *psz ) )
    {
        psz++;
    }
    return psz;
}

// Skip all trailing whitespace
inline CStringW::PCXSTR SkipWhiteSpaceRight(const CStringW& str)
{
    CStringW::PCXSTR psz = str.GetString();
    CStringW::PCXSTR pszLast = psz + str.GetLength() - 1;
    bool first_white = false;
    while( iswspace( *pszLast ) )
    {
        pszLast--;
        if(pszLast<psz)
            break;
    }
    return pszLast;
}

// Skip all leading whitespace
inline CStringW::PCXSTR SkipWhiteSpaceLeft(CStringW::PCXSTR start, CStringW::PCXSTR end)
{
    while( start!=end && iswspace( *start ) )
    {
        start++;
    }
    return start;
}

// Skip all trailing whitespace, first char must NOT be white space
inline CStringW::PCXSTR FastSkipWhiteSpaceRight(CStringW::PCXSTR start, CStringW::PCXSTR end)
{
    while( iswspace( *--end ) );
    return end+1;
}

inline CStringW::PCXSTR FindChar(CStringW::PCXSTR start, CStringW::PCXSTR end, WCHAR c)
{
    while( start!=end && *start!=c )
    {
        start++;
    }
    return start;
}

//////////////////////////////////////////////////////////////////////////////////////////////

// CMyFont

CMyFont::CMyFont(const STSStyleBase& style)
{
    LOGFONT lf;
    memset(&lf, 0, sizeof(lf));
    lf <<= style;
    lf.lfHeight = (LONG)(style.fontSize+0.5);
    lf.lfOutPrecision = OUT_TT_PRECIS;
    lf.lfClipPrecision = CLIP_DEFAULT_PRECIS;
    lf.lfQuality = ANTIALIASED_QUALITY;
    lf.lfPitchAndFamily = DEFAULT_PITCH|FF_DONTCARE;
    if(!CreateFontIndirect(&lf))
    {
        _tcscpy(lf.lfFaceName, _T("Arial"));
        CreateFontIndirect(&lf);
    }
    HFONT hOldFont = SelectFont(g_hDC, *this);
    TEXTMETRIC tm;
    GetTextMetrics(g_hDC, &tm);
    m_ascent = ((tm.tmAscent + 4) >> 3);
    m_descent = ((tm.tmDescent + 4) >> 3);
    SelectFont(g_hDC, hOldFont);
}

// CWord

CWord::CWord(const FwSTSStyle& style, const CStringW& str, int ktype, int kstart, int kend)
    : m_style(style), m_str(str)
    , m_width(0), m_ascent(0), m_descent(0)
    , m_ktype(ktype), m_kstart(kstart), m_kend(kend)
    , m_fLineBreak(false), m_fWhiteSpaceChar(false)
    //, m_pOpaqueBox(NULL)
{
    if(m_str.IsEmpty())
    {
        m_fWhiteSpaceChar = m_fLineBreak = true;
    }
    m_width = 0;
}

CWord::CWord( const CWord& src)
{
    m_str = src.m_str;
    m_fWhiteSpaceChar = src.m_fWhiteSpaceChar;
    m_fLineBreak = src.m_fLineBreak;
    m_style = src.m_style;
    m_pOpaqueBox = src.m_pOpaqueBox;//allow since it is shared_ptr
    m_ktype = src.m_ktype;
    m_kstart = src.m_kstart;
    m_kend = src.m_kend;
    m_width = src.m_width;
    m_ascent = src.m_ascent;
    m_descent = src.m_descent;
}

CWord::~CWord()
{
    //if(m_pOpaqueBox) delete m_pOpaqueBox;
}

bool CWord::Append(const SharedPtrCWord& w)
{
    if(!(m_style == w->m_style)
            || m_fLineBreak || w->m_fLineBreak
            || w->m_kstart != w->m_kend || m_ktype != w->m_ktype) return(false);
    m_fWhiteSpaceChar = m_fWhiteSpaceChar && w->m_fWhiteSpaceChar;    
    m_str += w->m_str;    
    m_width += w->m_width;
    return(true);
}

void CWord::PaintBody( const SharedPtrCWord& word, const CPoint& p, const CPoint& trans_org, SharedPtrOverlay* overlay )
{
    if(!word->m_str || overlay==NULL) return;
    bool error = false;
    do 
    {
        CPoint trans_org2 = trans_org;    
        bool need_transform = word->NeedTransform();
        if(!need_transform)
        {
            trans_org2.x=0;
            trans_org2.y=0;
        }

        CPoint psub_true( (p.x&SubpixelPositionControler::EIGHT_X_EIGHT_MASK), (p.y&SubpixelPositionControler::EIGHT_X_EIGHT_MASK) );
        OverlayKey sub_key(*word, psub_true, trans_org2);
        sub_key.UpdateHashValue();
        if( SubpixelPositionControler::GetGlobalControler().UseBilinearShift() )
        {
            OverlayMruCache* overlay_cache = CacheManager::GetSubpixelVarianceCache();

            POSITION pos = overlay_cache->Lookup(sub_key);
            if(pos!=NULL) 
            {
                *overlay = overlay_cache->GetAt(pos);
                overlay_cache->UpdateCache( pos );
            }
        }
        if( !overlay->get() )
        {
            CPoint psub = SubpixelPositionControler::GetGlobalControler().GetSubpixel(p);
            OverlayKey overlay_key(*word, psub, trans_org2);
            overlay_key.UpdateHashValue();
            OverlayMruCache* overlay_cache = CacheManager::GetOverlayMruCache();
            POSITION pos = overlay_cache->Lookup(overlay_key);
            if(pos==NULL)
            {
                if( !word->DoPaint(psub, trans_org2, overlay, overlay_key) )
                {
                    error = true;
                    break;
                }                
            }
            else
            {
                *overlay = overlay_cache->GetAt(pos);
                overlay_cache->UpdateCache( pos );
            }
            PaintFromOverlay(p, trans_org2, sub_key, *overlay);
        }
    } while(false);
    if(error)
    {
        overlay->reset( new Overlay() );
    }
}

void CWord::PaintOutline( const SharedPtrCWord& word, const CPoint& psub, const CPoint& trans_org, SharedPtrOverlay* overlay )
{
    if (word->m_style.get().borderStyle==0)
    {
        PaintBody(word, psub, trans_org, overlay);
    }
    else if (word->m_style.get().borderStyle==1)
    {
        if(word->CreateOpaqueBox())
        {
            PaintBody(word->m_pOpaqueBox, psub, trans_org, overlay);
        }
    }
}

void CWord::PaintShadow( const SharedPtrCWord& word, const CPoint& psub, const CPoint& trans_org, SharedPtrOverlay* overlay )
{
    PaintOutline(word, psub, trans_org, overlay);
}

void CWord::PaintFromOverlay(const CPoint& p, const CPoint& trans_org2, OverlayKey &subpixel_variance_key, SharedPtrOverlay& overlay)
{
    if( SubpixelPositionControler::GetGlobalControler().UseBilinearShift() )
    {
        CPoint psub = SubpixelPositionControler::GetGlobalControler().GetSubpixel(p);
        if( (psub.x!=(p.x&SubpixelPositionControler::EIGHT_X_EIGHT_MASK) 
            || psub.y!=(p.y&SubpixelPositionControler::EIGHT_X_EIGHT_MASK)) )
        {
            overlay.reset(overlay->GetSubpixelVariance((p.x&SubpixelPositionControler::EIGHT_X_EIGHT_MASK) - psub.x, 
                (p.y&SubpixelPositionControler::EIGHT_X_EIGHT_MASK) - psub.y));        
            OverlayMruCache* overlay_cache = CacheManager::GetSubpixelVarianceCache();                
            overlay_cache->UpdateCache(subpixel_variance_key, overlay);
        }
    }
}

void CWord::PaintFromNoneBluredOverlay(SharedPtrOverlay raterize_result, const OverlayKey& overlay_key, SharedPtrOverlay* overlay)
{
    if( m_style.get().fBlur>0 || m_style.get().fGaussianBlur>0.000001 )
    {
        overlay->reset(new Overlay());
        if(!Rasterizer::Blur(*raterize_result, m_style.get().fBlur, m_style.get().fGaussianBlur, *overlay))
        {
            *overlay = raterize_result;
        }
    }
    else
    {
        *overlay = raterize_result;
    }
    OverlayMruCache* overlay_cache = CacheManager::GetOverlayMruCache();
    overlay_cache->UpdateCache(overlay_key, *overlay);
}

bool CWord::PaintFromScanLineData2(const CPoint& psub, const ScanLineData2& scan_line_data2, const OverlayKey& key, SharedPtrOverlay* overlay)
{
    SharedPtrOverlay raterize_result(new Overlay());
    if(!Rasterizer::Rasterize(scan_line_data2, psub.x, psub.y, raterize_result)) 
    {     
        return false;
    }
    OverlayNoBlurMruCache* overlay_no_blur_cache = CacheManager::GetOverlayNoBlurMruCache();
    overlay_no_blur_cache->UpdateCache(key, raterize_result);
    PaintFromNoneBluredOverlay(raterize_result, key, overlay);
    return true;
}

bool CWord::PaintFromPathData(const CPoint& psub, const CPoint& trans_org, const PathData& path_data, const OverlayKey& key, SharedPtrOverlay* overlay )
{
    bool result = false;

    PathData *path_data2 = new PathData(path_data);//fix me: this copy operation can be saved if no transform is needed
    SharedPtrConstPathData shared_ptr_path_data2(path_data2);
    bool need_transform = NeedTransform();
    if(need_transform)
        Transform(path_data2, CPoint(trans_org.x*8, trans_org.y*8));

    CPoint left_top;
    CSize size;
    path_data2->AlignLeftTop(&left_top, &size);

    int border_x = static_cast<int>(m_style.get().outlineWidthX+0.5);
    int border_y = static_cast<int>(m_style.get().outlineWidthY+0.5);
    int wide_border = border_x>border_y ? border_x:border_y;
    if (m_style.get().borderStyle==1)
    {
        border_x = border_y = 0;
    }

    OverlayNoOffsetMruCache* overlay_key_cache = CacheManager::GetOverlayNoOffsetMruCache();
    OverlayNoOffsetKey overlay_no_offset_key(shared_ptr_path_data2, psub.x, psub.y, border_x, border_y);
    overlay_no_offset_key.UpdateHashValue();
    POSITION pos = overlay_key_cache->Lookup(overlay_no_offset_key);
        
    OverlayNoBlurMruCache* overlay_cache = CacheManager::GetOverlayNoBlurMruCache();
    if (pos!=NULL)
    {
        OverlayNoBlurKey overlay_key = overlay_key_cache->GetAt(pos);
        pos = overlay_cache->Lookup(overlay_key);        
    }
    if (pos)
    {
        SharedPtrOverlay raterize_result( new Overlay() );
        *raterize_result = *overlay_cache->GetAt(pos);
        raterize_result->mOffsetX = left_top.x - psub.x - ((wide_border+7)&~7);
        raterize_result->mOffsetY = left_top.y - psub.y - ((wide_border+7)&~7);
        PaintFromNoneBluredOverlay(raterize_result, key, overlay);
        result = true;
        overlay_cache->UpdateCache(key, raterize_result);
    }
    else
    {
        ScanLineDataMruCache* scan_line_data_cache = CacheManager::GetScanLineDataMruCache();
        pos = scan_line_data_cache->Lookup(overlay_no_offset_key);
        SharedPtrConstScanLineData scan_line_data;
        if( pos != NULL )
        {
            scan_line_data = scan_line_data_cache->GetAt(pos);
            scan_line_data_cache->UpdateCache(pos);
        }
        else
        {
            ScanLineData *tmp = new ScanLineData();
            scan_line_data.reset(tmp);
            if(!tmp->ScanConvert(*path_data2, size))
            {
                return false;
            }
            scan_line_data_cache->UpdateCache(overlay_no_offset_key, scan_line_data);
        }
        ScanLineData2 *tmp = new ScanLineData2(left_top, scan_line_data);
        SharedPtrScanLineData2 scan_line_data2( tmp );
        if(m_style.get().borderStyle == 0 && (m_style.get().outlineWidthX+m_style.get().outlineWidthY > 0))
        {
            if(!tmp->CreateWidenedRegion(border_x, border_y)) 
            {
                return false;
            }
        }
        ScanLineData2MruCache* scan_line_data2_cache = CacheManager::GetScanLineData2MruCache();
        scan_line_data2_cache->UpdateCache(key, scan_line_data2); 
        result = PaintFromScanLineData2(psub, *tmp, key, overlay);        
    }
    if (result)
    {
        overlay_key_cache->UpdateCache(overlay_no_offset_key, key);
    }
    return result;
}

bool CWord::PaintFromRawData( const CPoint& psub, const CPoint& trans_org, const OverlayKey& key, SharedPtrOverlay* overlay )
{
    PathDataMruCache* path_data_cache = CacheManager::GetPathDataMruCache();

    PathData *tmp=new PathData();
    SharedPtrPathData path_data(tmp);
    if(!CreatePath(tmp))
    {
        return false;
    }
    path_data_cache->UpdateCache(key, path_data);
    return PaintFromPathData(psub, trans_org, *tmp, key, overlay);
}

bool CWord::DoPaint(const CPoint& psub, const CPoint& trans_org, SharedPtrOverlay* overlay, const OverlayKey& key)
{
    bool result = true;
    OverlayNoBlurMruCache* overlay_no_blur_cache = CacheManager::GetOverlayNoBlurMruCache();
    POSITION pos = overlay_no_blur_cache->Lookup(key);

    if(pos!=NULL)
    {
        SharedPtrOverlay raterize_result = overlay_no_blur_cache->GetAt(pos);
        overlay_no_blur_cache->UpdateCache( pos );
        PaintFromNoneBluredOverlay(raterize_result, key, overlay);
    }  
    else
    {
        ScanLineData2MruCache* scan_line_data_cache = CacheManager::GetScanLineData2MruCache();
        pos = scan_line_data_cache->Lookup(key);
        if(pos!=NULL)
        {
            SharedPtrConstScanLineData2 scan_line_data = scan_line_data_cache->GetAt(pos);
            scan_line_data_cache->UpdateCache( pos );
            result = PaintFromScanLineData2(psub, *scan_line_data, key, overlay);
        }
        else
        {     
            PathDataMruCache* path_data_cache = CacheManager::GetPathDataMruCache();
            POSITION pos_path = path_data_cache->Lookup(key);
            if(pos_path!=NULL)    
            {
                SharedPtrConstPathData path_data = path_data_cache->GetAt(pos_path); //important! copy not ref                
                path_data_cache->UpdateCache( pos_path );
                result = PaintFromPathData(psub, trans_org, *path_data, key, overlay);
            }
            else
            {
                result = PaintFromRawData(psub, trans_org, key, overlay);
            }
        }
    }
    return result;
}

bool CWord::NeedTransform()
{
    return (fabs(m_style.get().fontScaleX - 100) > 0.000001) ||
           (fabs(m_style.get().fontScaleY - 100) > 0.000001) ||
           (fabs(m_style.get().fontAngleX) > 0.000001) ||
           (fabs(m_style.get().fontAngleY) > 0.000001) ||
           (fabs(m_style.get().fontAngleZ) > 0.000001) ||
           (fabs(m_style.get().fontShiftX) > 0.000001) ||
           (fabs(m_style.get().fontShiftY) > 0.000001);
}

void CWord::Transform(PathData* path_data, const CPoint& org)
{
	//// CPUID from VDub
	//bool fSSE2 = !!(g_cpuid.m_flags & CCpuID::sse2);

	//if(fSSE2) {	// SSE code
	//	Transform_SSE2(path_data, org);
	//} else		// C-code
		Transform_C(path_data, org);
}

void CWord::Transform_C(PathData* path_data, const CPoint &org )
{
	double scalex = m_style.get().fontScaleX/100;
	double scaley = m_style.get().fontScaleY/100;

	double caz = cos((3.1415/180)*m_style.get().fontAngleZ);
	double saz = sin((3.1415/180)*m_style.get().fontAngleZ);
	double cax = cos((3.1415/180)*m_style.get().fontAngleX);
	double sax = sin((3.1415/180)*m_style.get().fontAngleX);
	double cay = cos((3.1415/180)*m_style.get().fontAngleY);
	double say = sin((3.1415/180)*m_style.get().fontAngleY);

#ifdef _VSMOD
	// patch m003. random text points
	double xrnd = m_style.get().mod_rand.X*100;
	double yrnd = m_style.get().mod_rand.Y*100;
	double zrnd = m_style.get().mod_rand.Z*100;

	srand(m_style.get().mod_rand.Seed);

	// patch m008. distort
	int xsz,ysz;
	double dst1x,dst1y,dst2x,dst2y,dst3x,dst3y;
	int minx = INT_MAX, miny = INT_MAX, maxx = -INT_MAX, maxy = -INT_MAX;

	bool is_dist = m_style.get().mod_distort.enabled;
	if (is_dist) {
		for(int i = 0; i < path_data->mPathPoints; i++) {
			if(minx > path_data->mpPathPoints[i].x) {
				minx = path_data->mpPathPoints[i].x;
			}
			if(miny > path_data->mpPathPoints[i].y) {
				miny = path_data->mpPathPoints[i].y;
			}
			if(maxx < path_data->mpPathPoints[i].x) {
				maxx = path_data->mpPathPoints[i].x;
			}
			if(maxy < path_data->mpPathPoints[i].y) {
				maxy = path_data->mpPathPoints[i].y;
			}
		}

		xsz = max(maxx - minx, 0);
		ysz = max(maxy - miny, 0);

		dst1x = m_style.get().mod_distort.pointsx[0];
		dst1y = m_style.get().mod_distort.pointsy[0];
		dst2x = m_style.get().mod_distort.pointsx[1];
		dst2y = m_style.get().mod_distort.pointsy[1];
		dst3x = m_style.get().mod_distort.pointsx[2];
		dst3y = m_style.get().mod_distort.pointsy[2];
	}
#endif

	for (int i = 0; i < path_data->mPathPoints; i++) {
		double x, y, z, xx, yy, zz;

		x = path_data->mpPathPoints[i].x;
		y = path_data->mpPathPoints[i].y;
#ifdef _VSMOD
		// patch m002. Z-coord
		z = m_style.get().mod_z;

		double u, v;
		if (is_dist) {
			u = (x-minx) / xsz;
			v = (y-miny) / ysz;

			x = minx+(0 + (dst1x - 0)*u + (dst3x-0)*v+(0+dst2x-dst1x-dst3x)*u*v)*xsz;
			y = miny+(0 + (dst1y - 0)*u + (dst3y-0)*v+(0+dst2y-dst1y-dst3y)*u*v)*ysz;
			//P = P0 + (P1 - P0)u + (P3 - P0)v + (P0 + P2 - P1 - P3)uv
		}

		// patch m003. random text points
		x = xrnd > 0 ? (xrnd - rand() % (int)(xrnd * 2 + 1)) / 100.0 + x : x;
		y = yrnd > 0 ? (yrnd - rand() % (int)(yrnd * 2 + 1)) / 100.0 + y : y;
		z = zrnd > 0 ? (zrnd - rand() % (int)(zrnd * 2 + 1)) / 100.0 + z : z;
#else
		z = 0;
#endif
		double _x = x;
		x = scalex * (x + m_style.get().fontShiftX * y) - org.x;
		y = scaley * (y + m_style.get().fontShiftY * _x) - org.y;

		xx = x*caz + y*saz;
		yy = -(x*saz - y*caz);
		zz = z;

		x = xx;
		y = yy*cax + zz*sax;
		z = yy*sax - zz*cax;

		xx = x*cay + z*say;
		yy = y;
		zz = x*say - z*cay;

		zz = max(zz, -19000);

		x = (xx * 20000) / (zz + 20000);
		y = (yy * 20000) / (zz + 20000);

		path_data->mpPathPoints[i].x = (LONG)(x + org.x + 0.5);
		path_data->mpPathPoints[i].y = (LONG)(y + org.y + 0.5);
	}
}

void CWord::Transform_SSE2(PathData* path_data, const CPoint &org )
{
	// __m128 union data type currently not supported with Intel C++ Compiler, so just call C version
#ifdef __ICL
	Transform_C(org);
#else
	// SSE code
	// speed up ~1.5-1.7x
	double scalex = m_style.get().fontScaleX/100;
	double scaley = m_style.get().fontScaleY/100;

	double caz = cos((3.1415/180)*m_style.get().fontAngleZ);
	double saz = sin((3.1415/180)*m_style.get().fontAngleZ);
	double cax = cos((3.1415/180)*m_style.get().fontAngleX);
	double sax = sin((3.1415/180)*m_style.get().fontAngleX);
	double cay = cos((3.1415/180)*m_style.get().fontAngleY);
	double say = sin((3.1415/180)*m_style.get().fontAngleY);

	__m128 __xshift = _mm_set_ps1(m_style.get().fontShiftX);
	__m128 __yshift = _mm_set_ps1(m_style.get().fontShiftY);

	__m128 __xorg = _mm_set_ps1(org.x);
	__m128 __yorg = _mm_set_ps1(org.y);

	__m128 __xscale = _mm_set_ps1(scalex);
	__m128 __yscale = _mm_set_ps1(scaley);

#ifdef _VSMOD
	// patch m003. random text points
	double xrnd = m_style.get().mod_rand.X*100;
	double yrnd = m_style.get().mod_rand.Y*100;
	double zrnd = m_style.get().mod_rand.Z*100;

	srand(m_style.get().mod_rand.Seed);

	__m128 __xsz = _mm_setzero_ps();
	__m128 __ysz = _mm_setzero_ps();

	__m128 __dst1x, __dst1y, __dst213x, __dst213y, __dst3x, __dst3y;

	__m128 __miny;
	__m128 __minx = _mm_set_ps(INT_MAX, INT_MAX, 0, 0);
	__m128 __max = _mm_set_ps(-INT_MAX, -INT_MAX, 1, 1);

	bool is_dist = m_style.get().mod_distort.enabled;
	if(is_dist) {
		for(int i = 0; i < path_data->mPathPoints; i++) {
			__m128 __point = _mm_set_ps(path_data->mpPathPoints[i].x, path_data->mpPathPoints[i].y, 0, 0);
			__minx = _mm_min_ps(__minx, __point);
			__max = _mm_max_ps(__max, __point);
		}

		__m128 __zero = _mm_setzero_ps();
		__max = _mm_sub_ps(__max, __minx); // xsz, ysz, 1, 1
		__max = _mm_max_ps(__max, __zero);

		__xsz = _mm_shuffle_ps(__max, __max, _MM_SHUFFLE(3,3,3,3));
		__ysz = _mm_shuffle_ps(__max, __max, _MM_SHUFFLE(2,2,2,2));

		__miny = _mm_shuffle_ps(__minx, __minx, _MM_SHUFFLE(2,2,2,2));
		__minx = _mm_shuffle_ps(__minx, __minx, _MM_SHUFFLE(3,3,3,3));

		__dst1x = _mm_set_ps1(m_style.get().mod_distort.pointsx[0]);
		__dst1y = _mm_set_ps1(m_style.get().mod_distort.pointsy[0]);
		__dst3x = _mm_set_ps1(m_style.get().mod_distort.pointsx[2]);
		__dst3y = _mm_set_ps1(m_style.get().mod_distort.pointsy[2]);
		__dst213x = _mm_set_ps1(m_style.get().mod_distort.pointsx[1]); // 2 - 1 - 3
		__dst213x = _mm_sub_ps(__dst213x, __dst1x);
		__dst213x = _mm_sub_ps(__dst213x, __dst3x);

		__dst213y = _mm_set_ps1(m_style.get().mod_distort.pointsy[1]);
		__dst213x = _mm_sub_ps(__dst213y, __dst1y);
		__dst213x = _mm_sub_ps(__dst213y, __dst3y);
	}
#endif

	__m128 __caz = _mm_set_ps1(caz);
	__m128 __saz = _mm_set_ps1(saz);
	__m128 __cax = _mm_set_ps1(cax);
	__m128 __sax = _mm_set_ps1(sax);
	__m128 __cay = _mm_set_ps1(cay);
	__m128 __say = _mm_set_ps1(say);

	// this can be paralleled for openmp
	int mPathPointsD4 = path_data->mPathPoints / 4;
	int mPathPointsM4 = path_data->mPathPoints % 4;
        
	for(ptrdiff_t i = 0; i < mPathPointsD4 + 1; i++) {
        POINT* const temp_points = path_data->mpPathPoints + 4 * i;

		__m128 __pointx, __pointy;
		// we can't use load .-.
		if(i != mPathPointsD4) {
			__pointx = _mm_set_ps(temp_points[0].x, temp_points[1].x, temp_points[2].x, temp_points[3].x);
			__pointy = _mm_set_ps(temp_points[0].y, temp_points[1].y, temp_points[2].y, temp_points[3].y);
		} else { // last cycle
			switch(mPathPointsM4) {
				default:
				case 0:
					continue;
				case 1:
					__pointx = _mm_set_ps(temp_points[0].x, 0, 0, 0);
					__pointy = _mm_set_ps(temp_points[0].y, 0, 0, 0);
					break;
				case 2:
					__pointx = _mm_set_ps(temp_points[0].x, temp_points[1].x, 0, 0);
					__pointy = _mm_set_ps(temp_points[0].y, temp_points[1].y, 0, 0);
					break;
				case 3:
					__pointx = _mm_set_ps(temp_points[0].x, temp_points[1].x, temp_points[2].x, 0);
					__pointy = _mm_set_ps(temp_points[0].y, temp_points[1].y, temp_points[2].y, 0);
					break;
			}
		}

#ifdef _VSMOD
		__m128 __pointz = _mm_set_ps1(m_style.get().mod_z);

		// distort
		if(is_dist) {
			//P = P0 + (P1 - P0)u + (P3 - P0)v + (P0 + P2 - P1 - P3)uv
			__m128 __u = _mm_sub_ps(__pointx, __minx);
			__m128 __v = _mm_sub_ps(__pointy, __miny);
			__m128 __1_xsz = _mm_rcp_ps(__xsz);
			__m128 __1_ysz = _mm_rcp_ps(__ysz);
			__u = _mm_mul_ps(__u, __1_xsz);
			__v = _mm_mul_ps(__v, __1_ysz);

			// x
			__pointx = _mm_mul_ps(__dst213x, __u);
			__pointx = _mm_mul_ps(__pointx, __v);

			__m128 __tmpx = _mm_mul_ps(__dst3x, __v);
			__pointx = _mm_add_ps(__pointx, __tmpx);
			__tmpx = _mm_mul_ps(__dst1x, __u);
			__pointx = _mm_add_ps(__pointx, __tmpx);

			__pointx = _mm_mul_ps(__pointx, __xsz);
			__pointx = _mm_add_ps(__pointx, __minx);

			// y
			__pointy = _mm_mul_ps(__dst213y, __u);
			__pointy = _mm_mul_ps(__pointy, __v);

			__m128 __tmpy = _mm_mul_ps(__dst3y, __v);
			__pointy = _mm_add_ps(__pointy, __tmpy);
			__tmpy = _mm_mul_ps(__dst1y, __u);
			__pointy = _mm_add_ps(__pointy, __tmpy);

			__pointy = _mm_mul_ps(__pointy, __ysz);
			__pointy = _mm_add_ps(__pointy, __miny);
		}

		// randomize
		if(xrnd!=0 || yrnd!=0 || zrnd!=0) {
			__declspec(align(16)) float rx[4], ry[4], rz[4];
			for(int k=0; k<4; k++) {
				rx[k] = xrnd > 0 ? (xrnd - rand() % (int)(xrnd * 2 + 1)) : 0;
				ry[k] = yrnd > 0 ? (yrnd - rand() % (int)(yrnd * 2 + 1)) : 0;
				rz[k] = zrnd > 0 ? (zrnd - rand() % (int)(zrnd * 2 + 1)) : 0;
			}
			__m128 __001 = _mm_set_ps1(0.01f);

			if(xrnd!=0) {
				__m128 __rx = _mm_load_ps(rx);
				__rx = _mm_mul_ps(__rx, __001);
				__pointx = _mm_add_ps(__pointx, __rx);
			}

			if(yrnd!=0) {
				__m128 __ry = _mm_load_ps(ry);
				__ry = _mm_mul_ps(__ry, __001);
				__pointy = _mm_add_ps(__pointy, __ry);
			}

			if(zrnd!=0) {
				__m128 __rz = _mm_load_ps(rz);
				__rz = _mm_mul_ps(__rz, __001);
				__pointz = _mm_add_ps(__pointz, __rz);
			}
		}
#else
		__m128 __pointz = _mm_set_ps1(0);
#endif

		// scale and shift
		__m128 __tmpx;
		if(m_style.get().fontShiftX!=0) {
			__tmpx = _mm_mul_ps(__xshift, __pointy);
			__tmpx = _mm_add_ps(__tmpx, __pointx);
		} else {
			__tmpx = __pointx;
		}
		__tmpx = _mm_mul_ps(__tmpx, __xscale);
		__tmpx = _mm_sub_ps(__tmpx, __xorg);

		__m128 __tmpy;
		if(m_style.get().fontShiftY!=0) {
			__tmpy = _mm_mul_ps(__yshift, __pointx);
			__tmpy = _mm_add_ps(__tmpy, __pointy);
		} else {
			__tmpy = __pointy;
		}
		__tmpy = _mm_mul_ps(__tmpy, __yscale);
		__tmpy = _mm_sub_ps(__tmpy, __yorg);

		// rotate
		__m128 __xx = _mm_mul_ps(__tmpx, __caz);
		__m128 __yy = _mm_mul_ps(__tmpy, __saz);
		__pointx = _mm_add_ps(__xx, __yy);
		__xx = _mm_mul_ps(__tmpx, __saz);
		__yy = _mm_mul_ps(__tmpy, __caz);
		__pointy = _mm_sub_ps(__yy, __xx);

		__m128 __zz = _mm_mul_ps(__pointz, __sax);
		__yy = _mm_mul_ps(__pointy, __cax);
		__pointy = _mm_add_ps(__yy, __zz);
		__zz = _mm_mul_ps(__pointz, __cax);
		__yy = _mm_mul_ps(__pointy, __sax);
		__pointz = _mm_sub_ps(__zz, __yy);

		__xx = _mm_mul_ps(__pointx, __cay);
		__zz = _mm_mul_ps(__pointz, __say);
		__pointx = _mm_add_ps(__xx, __zz);
		__xx = _mm_mul_ps(__pointx, __say);
		__zz = _mm_mul_ps(__pointz, __cay);
		__pointz = _mm_sub_ps(__xx, __zz);

		__zz = _mm_set_ps1(-19000);
		__pointz = _mm_max_ps(__pointz, __zz);

		__m128 __20000 = _mm_set_ps1(20000);
		__zz = _mm_add_ps(__pointz, __20000);
		__zz = _mm_rcp_ps(__zz);

		__pointx = _mm_mul_ps(__pointx, __20000);
		__pointx = _mm_mul_ps(__pointx, __zz);

		__pointy = _mm_mul_ps(__pointy, __20000);
		__pointy = _mm_mul_ps(__pointy, __zz);

		__pointx = _mm_add_ps(__pointx, __xorg);
		__pointy = _mm_add_ps(__pointy, __yorg);

		__m128 __05 = _mm_set_ps1(0.5);

		__pointx = _mm_add_ps(__pointx, __05);
		__pointy = _mm_add_ps(__pointy, __05);

		if(i == mPathPointsD4) { // last cycle
			for(int k=0; k<mPathPointsM4; k++) {
				temp_points[k].x = static_cast<LONG>(__pointx.m128_f32[3-k]);
				temp_points[k].y = static_cast<LONG>(__pointy.m128_f32[3-k]);
			}
		} else {
			for(int k=0; k<4; k++) {
				temp_points[k].x = static_cast<LONG>(__pointx.m128_f32[3-k]);
				temp_points[k].y = static_cast<LONG>(__pointy.m128_f32[3-k]);
			}
		}
	}
#endif // __ICL
}

bool CWord::CreateOpaqueBox()
{
    if(m_pOpaqueBox) return(true);
    STSStyle style = m_style.get();
    style.borderStyle = 0;
    style.outlineWidthX = style.outlineWidthY = 0;
    style.colors[0] = m_style.get().colors[2];
    style.alpha[0] = m_style.get().alpha[2];
    int w = (int)(m_style.get().outlineWidthX + 0.5);
    int h = (int)(m_style.get().outlineWidthY + 0.5);
    CStringW str;
    str.Format(L"m %d %d l %d %d %d %d %d %d",
               -w, -h,
               m_width+w, -h,
               m_width+w, m_ascent+m_descent+h,
               -w, m_ascent+m_descent+h);
    m_pOpaqueBox.reset( new CPolygon(FwSTSStyle(style), str, 0, 0, 0, 1.0/8, 1.0/8, 0) );
    return(!!m_pOpaqueBox);
}

void CWord::PaintAll( const SharedPtrCWord& word, 
    const CPoint& shadowPos, const CPoint& outlinePos, const CPoint& bodyPos, const CPoint& org,  
    SharedPtrOverlay* shadow, SharedPtrOverlay* outline, SharedPtrOverlay* body )
{
    CPoint transOrg;
    if(shadow!=NULL) 
    {
        //has shadow
        transOrg = org - shadowPos;
    }
    else if(outline!=NULL)
    {
        //has outline
        transOrg = org - outlinePos;
    }
    else if(body!=NULL)
    {
        transOrg = org - bodyPos;
    }    
    if(shadow!=NULL)
    {
        PaintShadow(word, shadowPos, transOrg, shadow);
    }
    if(outline!=NULL)
    {
        PaintOutline(word, outlinePos, transOrg, outline);
    }
    if(body!=NULL)
    {
        PaintBody(word, bodyPos, transOrg, body);
    }    
}


// CText

CText::CText(const FwSTSStyle& style, const CStringW& str, int ktype, int kstart, int kend)
    : CWord(style, str, ktype, kstart, kend)
{
    if(m_str == L" ")
    {
        m_fWhiteSpaceChar = true;
    }
    SharedPtrTextInfo text_info;
    TextInfoCacheKey text_info_key;
    text_info_key.m_str = m_str;
    text_info_key.m_style = m_style;
    text_info_key.UpdateHashValue();
    TextInfoMruCache* text_info_cache = CacheManager::GetTextInfoCache();
    POSITION pos = text_info_cache->Lookup(text_info_key);
    if(pos==NULL)
    {
        TextInfo* tmp=new TextInfo();
        GetTextInfo(tmp, m_style, m_str);
        text_info.reset(tmp);
        text_info_cache->UpdateCache(text_info_key, text_info);
    }
    else
    {
        text_info = text_info_cache->GetAt(pos);
        text_info_cache->UpdateCache( pos );
    }
    this->m_ascent = text_info->m_ascent;
    this->m_descent = text_info->m_descent;
    this->m_width = text_info->m_width;
}

CText::CText( const CText& src ):CWord(src)
{
    m_width = src.m_width;
}

SharedPtrCWord CText::Copy()
{
    SharedPtrCWord result(new CText(*this));
	return result;
}

bool CText::Append(const SharedPtrCWord& w)
{
    boost::shared_ptr<CText> p = boost::dynamic_pointer_cast<CText>(w);
    return (p && CWord::Append(w));
}

bool CText::CreatePath(PathData* path_data)
{
    FwCMyFont font(m_style);
    HFONT hOldFont = SelectFont(g_hDC, font.get());
    int width = 0;
    if(m_style.get().fontSpacing || (long)GetVersion() < 0)
    {
        bool bFirstPath = true;
        for(LPCWSTR s = m_str; *s; s++)
        {
            CSize extent;
            if(!GetTextExtentPoint32W(g_hDC, s, 1, &extent)) {SelectFont(g_hDC, hOldFont); ASSERT(0); return(false);}
            path_data->PartialBeginPath(g_hDC, bFirstPath);
            bFirstPath = false;
            TextOutW(g_hDC, 0, 0, s, 1);
            path_data->PartialEndPath(g_hDC, width, 0);
            width += extent.cx + (int)m_style.get().fontSpacing;
        }
    }
    else
    {
        CSize extent;
        if(!GetTextExtentPoint32W(g_hDC, m_str, m_str.GetLength(), &extent)) {SelectFont(g_hDC, hOldFont); ASSERT(0); return(false);}
        path_data->BeginPath(g_hDC);
        TextOutW(g_hDC, 0, 0, m_str, m_str.GetLength());
        path_data->EndPath(g_hDC);
    }
    SelectFont(g_hDC, hOldFont);
    return(true);
}

void CText::GetTextInfo(TextInfo *output, const FwSTSStyle& style, const CStringW& str )
{
    FwCMyFont font(style);
    output->m_ascent = (int)(style.get().fontScaleY/100*font.get().m_ascent);
    output->m_descent = (int)(style.get().fontScaleY/100*font.get().m_descent);

    HFONT hOldFont = SelectFont(g_hDC, font.get());
    if(style.get().fontSpacing || (long)GetVersion() < 0)
    {
        bool bFirstPath = true;
        for(LPCWSTR s = str; *s; s++)
        {
            CSize extent;
            if(!GetTextExtentPoint32W(g_hDC, s, 1, &extent)) {SelectFont(g_hDC, hOldFont); ASSERT(0); return;}
            output->m_width += extent.cx + (int)style.get().fontSpacing;
        }
        //          m_width -= (int)m_style.get().fontSpacing; // TODO: subtract only at the end of the line
    }
    else
    {
        CSize extent;
        if(!GetTextExtentPoint32W(g_hDC, str, wcslen(str), &extent)) {SelectFont(g_hDC, hOldFont); ASSERT(0); return;}
        output->m_width += extent.cx;
    }
    output->m_width = (int)(style.get().fontScaleX/100*output->m_width + 4) >> 3;
    SelectFont(g_hDC, hOldFont);
}

// CPolygon

CPolygon::CPolygon(const FwSTSStyle& style, const CStringW& str, int ktype, int kstart, int kend, double scalex, double scaley, int baseline)
    : CWord(style, str, ktype, kstart, kend)
    , m_scalex(scalex), m_scaley(scaley), m_baseline(baseline)
{
    ParseStr();
}

CPolygon::CPolygon(CPolygon& src) : CWord(src)
{
	m_scalex = src.m_scalex;
	m_scaley = src.m_scaley;
	m_baseline = src.m_baseline;
	m_width = src.m_width;
 	m_ascent = src.m_ascent;
 	m_descent = src.m_descent;
	m_pathTypesOrg.Copy(src.m_pathTypesOrg);
	m_pathPointsOrg.Copy(src.m_pathPointsOrg);
}
CPolygon::~CPolygon()
{
}

SharedPtrCWord CPolygon::Copy()
{
    SharedPtrCWord result(DNew CPolygon(*this));
	return result;
}

bool CPolygon::Append(const SharedPtrCWord& w)
{
    // TODO
    return(false);
}

bool CPolygon::Get6BitFixedPoint(CStringW& str, LONG& ret)
{
    LPWSTR s = (LPWSTR)(LPCWSTR)str, e = s;
    ret = wcstod(str, &e) * 64;
    str.Delete(0,e-s); 
    XY_LOG_INFO(ret);
    return(e > s);
}

bool CPolygon::GetPOINT(CStringW& str, POINT& ret)
{
    return(Get6BitFixedPoint(str, ret.x) && Get6BitFixedPoint(str, ret.y));
}

bool CPolygon::ParseStr()
{
    if(m_pathTypesOrg.GetCount() > 0) return(true);
    CPoint p;
    int j, lastsplinestart = -1, firstmoveto = -1, lastmoveto = -1;
    CStringW str = m_str;
    str.SpanIncluding(L"mnlbspc 0123456789");
    str.Replace(L"m", L"*m");
    str.Replace(L"n", L"*n");
    str.Replace(L"l", L"*l");
    str.Replace(L"b", L"*b");
    str.Replace(L"s", L"*s");
    str.Replace(L"p", L"*p");
    str.Replace(L"c", L"*c");
    int k = 0;
    for(CStringW s = str.Tokenize(L"*", k); !s.IsEmpty(); s = str.Tokenize(L"*", k))
    {
        WCHAR c = s[0];
        s.TrimLeft(L"mnlbspc ");
        switch(c)
        {
        case 'm':
            lastmoveto = m_pathTypesOrg.GetCount();
            if(firstmoveto == -1) 
                firstmoveto = lastmoveto;
            while(GetPOINT(s, p)) {
                m_pathTypesOrg.Add(PT_MOVETO); 
                m_pathPointsOrg.Add(p);
            }
            break;
        case 'n':
            while(GetPOINT(s, p)) {
                m_pathTypesOrg.Add(PT_MOVETONC);
                m_pathPointsOrg.Add(p);
            }
            break;
        case 'l':
            if (m_pathPointsOrg.GetCount() < 1) {
                break;
            }
            while(GetPOINT(s, p)) {
                m_pathTypesOrg.Add(PT_LINETO);
                m_pathPointsOrg.Add(p);
            }
            break;
        case 'b':
            j = m_pathTypesOrg.GetCount();
            if (j < 1) {
                break;
            }
            while(GetPOINT(s, p)) {
                m_pathTypesOrg.Add(PT_BEZIERTO);
                m_pathPointsOrg.Add(p);
                j++;
            }
            j = m_pathTypesOrg.GetCount() - ((m_pathTypesOrg.GetCount()-j)%3);
            m_pathTypesOrg.SetCount(j);
            m_pathPointsOrg.SetCount(j);
            break;
        case 's':
            if (m_pathPointsOrg.GetCount() < 1) {
                break;
            }
            {
                j = lastsplinestart = m_pathTypesOrg.GetCount();
                int i = 3;
                while(i-- && GetPOINT(s, p)) {
                    m_pathTypesOrg.Add(PT_BSPLINETO);
                    m_pathPointsOrg.Add(p);
                    j++;
                }
                if(m_pathTypesOrg.GetCount()-lastsplinestart < 3) {
                    m_pathTypesOrg.SetCount(lastsplinestart);
                    m_pathPointsOrg.SetCount(lastsplinestart);
                    lastsplinestart = -1;
                }
            }            
            // no break here
        case 'p':
            if (m_pathPointsOrg.GetCount() < 3) {
                break;
            }
            while(GetPOINT(s, p)) {
                m_pathTypesOrg.Add(PT_BSPLINEPATCHTO);
                m_pathPointsOrg.Add(p);
            }
            break;
        case 'c':
            if(lastsplinestart > 0)
            {
                m_pathTypesOrg.Add(PT_BSPLINEPATCHTO);
                m_pathTypesOrg.Add(PT_BSPLINEPATCHTO);
                m_pathTypesOrg.Add(PT_BSPLINEPATCHTO);
                p = m_pathPointsOrg[lastsplinestart-1]; // we need p for temp storage, because operator [] will return a reference to CPoint and Add() may reallocate its internal buffer (this is true for MFC 7.0 but not for 6.0, hehe)
                m_pathPointsOrg.Add(p);
                p = m_pathPointsOrg[lastsplinestart];
                m_pathPointsOrg.Add(p);
                p = m_pathPointsOrg[lastsplinestart+1];
                m_pathPointsOrg.Add(p);
                lastsplinestart = -1;
            }
            break;
        default:
            break;
        }
    }
    if(lastmoveto == -1 || firstmoveto > 0)
    {
        m_pathTypesOrg.RemoveAll();
        m_pathPointsOrg.RemoveAll();
        return(false);
    }
    int minx = INT_MAX, miny = INT_MAX, maxx = -INT_MAX, maxy = -INT_MAX;
    for(size_t i = 0; i < m_pathTypesOrg.GetCount(); i++)
    {
        m_pathPointsOrg[i].x = (int)(m_scalex * m_pathPointsOrg[i].x);
        m_pathPointsOrg[i].y = (int)(m_scaley * m_pathPointsOrg[i].y);
        if(minx > m_pathPointsOrg[i].x) minx = m_pathPointsOrg[i].x;
        if(miny > m_pathPointsOrg[i].y) miny = m_pathPointsOrg[i].y;
        if(maxx < m_pathPointsOrg[i].x) maxx = m_pathPointsOrg[i].x;
        if(maxy < m_pathPointsOrg[i].y) maxy = m_pathPointsOrg[i].y;
    }
    m_width = max(maxx - minx, 0);
    m_ascent = max(maxy - miny, 0);
    int baseline = (int)(64 * m_scaley * m_baseline);
    m_descent = baseline;
    m_ascent -= baseline;
    m_width = ((int)(m_style.get().fontScaleX/100 * m_width) + 4) >> 3;
    m_ascent = ((int)(m_style.get().fontScaleY/100 * m_ascent) + 4) >> 3;
    m_descent = ((int)(m_style.get().fontScaleY/100 * m_descent) + 4) >> 3;
    return(true);
}

bool CPolygon::CreatePath(PathData* path_data)
{
    int len = m_pathTypesOrg.GetCount();
    if(len == 0) return(false);
    if(path_data->mPathPoints != len)
    {
        path_data->mpPathTypes = (BYTE*)realloc(path_data->mpPathTypes, len*sizeof(BYTE));
        path_data->mpPathPoints = (POINT*)realloc(path_data->mpPathPoints, len*sizeof(POINT));
        if(!path_data->mpPathTypes || !path_data->mpPathPoints) return(false);
        path_data->mPathPoints = len;
    }
    memcpy(path_data->mpPathTypes, m_pathTypesOrg.GetData(), len*sizeof(BYTE));
    memcpy(path_data->mpPathPoints, m_pathPointsOrg.GetData(), len*sizeof(POINT));
    return(true);
}

// CClipper

CClipper::CClipper(CStringW str, CSize size, double scalex, double scaley, bool inverse)
    : m_polygon( new CPolygon(FwSTSStyle(), str, 0, 0, 0, scalex, scaley, 0) )
    , m_size(size), m_inverse(inverse)
    , m_effectType(-1), m_painted(false)
{    
    
}

CClipper::~CClipper()
{    
}

GrayImage2* CClipper::PaintSimpleClipper()
{
    GrayImage2* result = NULL;
    if(m_size.cx < 0 || m_size.cy < 0)
        return result;

    SharedPtrOverlay overlay;
    CWord::PaintBody( m_polygon, CPoint(0, 0), CPoint(0, 0), &overlay );
    int w = overlay->mOverlayWidth, h = overlay->mOverlayHeight;
    int x = (overlay->mOffsetX+4)>>3, y = (overlay->mOffsetY+4)>>3;
    result = new GrayImage2();
    if( !result )
        return result;
    result->data = overlay->mBody;
    result->pitch = overlay->mOverlayPitch;
    result->size.SetSize(w, h);
    result->left_top.SetPoint(x, y);
    return result;
}

GrayImage2* CClipper::PaintBaseClipper()
{
    GrayImage2* result = NULL;
    //m_pAlphaMask = NULL;
    if(m_size.cx < 0 || m_size.cy < 0)
        return result;

    SharedPtrOverlay overlay;
    CWord::PaintBody( m_polygon, CPoint(0, 0), CPoint(0, 0), &overlay );
    int w = overlay->mOverlayWidth, h = overlay->mOverlayHeight;
    int x = (overlay->mOffsetX+4)>>3, y = (overlay->mOffsetY+4)>>3;
    int xo = 0, yo = 0;
    if(x < 0) {xo = -x; w -= -x; x = 0;}
    if(y < 0) {yo = -y; h -= -y; y = 0;}
    if(x+w > m_size.cx) w = m_size.cx-x;
    if(y+h > m_size.cy) h = m_size.cy-y;
    if(w <= 0 || h <= 0) return result;

    result = new GrayImage2();
    if( !result )
        return result;
    result->data.reset( reinterpret_cast<BYTE*>(xy_malloc(m_size.cx*m_size.cy)), xy_free );
    result->pitch = m_size.cx;
    result->size = m_size;
    result->left_top.SetPoint(0, 0);

    BYTE * result_data = result->data.get();
    if(!result_data)
    {
        delete result;
        return NULL;
    }

    memset( result_data, 0, m_size.cx*m_size.cy );

    const BYTE* src = overlay->mBody.get() + (overlay->mOverlayPitch * yo + xo);
    BYTE* dst = result_data + m_size.cx * y + x;
    while(h--)
    {
        //for(int wt=0; wt<w; ++wt)
        //  dst[wt] = src[wt];
        memcpy(dst, src, w);
        src += overlay->mOverlayPitch;
        dst += m_size.cx;
    }
    if(m_inverse)
    {
        BYTE* dst = result_data;
        for(int i = m_size.cx*m_size.cy; i>0; --i, ++dst)
            *dst = 0x40 - *dst; // mask is 6 bit
    }
    return result;
}

GrayImage2* CClipper::PaintBannerClipper()
{
    int width = m_effect.param[2];
    int w = m_size.cx, h = m_size.cy;

    GrayImage2* result = PaintBaseClipper();
    if(!result)
        return result;

    int da = (64<<8)/width;
    BYTE* am = result->data.get();
    for(int j = 0; j < h; j++, am += w)
    {
        int a = 0;
        int k = min(width, w);
        for(int i = 0; i < k; i++, a += da)
            am[i] = (am[i]*a)>>14;
        a = 0x40<<8;
        k = w-width;
        if(k < 0) {a -= -k*da; k = 0;}
        for(int i = k; i < w; i++, a -= da)
            am[i] = (am[i]*a)>>14;
    }
    return result;
}

GrayImage2* CClipper::PaintScrollClipper()
{
    int height = m_effect.param[4];
    int w = m_size.cx, h = m_size.cy;
    
    GrayImage2* result = PaintBaseClipper();
    if(!result)
        return result;

    BYTE* data = result->data.get();

    int da = (64<<8)/height;
    int a = 0;
    int k = m_effect.param[0]>>3;
    int l = k+height;
    if(k < 0) {a += -k*da; k = 0;}
    if(l > h) {l = h;}
    if(k < h)
    {
        BYTE* am = &data[k*w];
        memset(data, 0, am - data);
        for(int j = k; j < l; j++, a += da)
        {
            for(int i = 0; i < w; i++, am++)
                *am = ((*am)*a)>>14;
        }
    }
    da = -(64<<8)/height;
    a = 0x40<<8;
    l = m_effect.param[1]>>3;
    k = l-height;
    if(k < 0) {a += -k*da; k = 0;}
    if(l > h) {l = h;}
    if(k < h)
    {
        BYTE* am = &data[k*w];
        int j = k;
        for(; j < l; j++, a += da)
        {
            for(int i = 0; i < w; i++, am++)
                *am = ((*am)*a)>>14;
        }
        memset(am, 0, (h-j)*w);
    }
    return result;
}

GrayImage2* CClipper::Paint()
{
    GrayImage2* result = NULL;
    switch(m_effectType)
    {
    case -1:
        if (!m_inverse)
        {
            result = PaintSimpleClipper();
        }
        else
        {
            result = PaintBaseClipper();
        }
        break;
    case EF_BANNER:
        result = PaintBannerClipper();
        break;
    case EF_SCROLL:
        result = PaintScrollClipper();
        break;
    }
    return result;
}

void CClipper::SetEffect( const Effect& effect, int effectType )
{
    m_effectType = effectType;
    m_effect = effect;
}

SharedPtrGrayImage2 CClipper::GetAlphaMask( const SharedPtrCClipper& clipper )
{
    if (clipper!=NULL)
    {
        ClipperAlphaMaskCacheKey key(clipper);
        key.UpdateHashValue();
        ClipperAlphaMaskMruCache * cache = CacheManager::GetClipperAlphaMaskMruCache();
        POSITION pos = cache->Lookup(key);
        if( pos!=NULL )
        {
            const SharedPtrGrayImage2& result = cache->GetAt(pos);
            cache->UpdateCache(pos);
            return result;
        }
        else
        {
            SharedPtrGrayImage2 result( clipper->Paint() );
            cache->UpdateCache(key, result);
            return result;
        }
    }
    else
    {
        SharedPtrGrayImage2 result;
        return result;
    }
}

// CLine

CLine::~CLine()
{
    //POSITION pos = GetHeadPosition();
    //while(pos) delete GetNext(pos);
}

void CLine::Compact()
{
    POSITION pos = GetHeadPosition();
    while(pos)
    {
        SharedPtrCWord w = GetNext(pos);
        if(!w->m_fWhiteSpaceChar) break;
        m_width -= w->m_width;
//        delete w;
        RemoveHead();
    }
    pos = GetTailPosition();
    while(pos)
    {
        SharedPtrCWord w = GetPrev(pos);
        if(!w->m_fWhiteSpaceChar) break;
        m_width -= w->m_width;
//        delete w;
        RemoveTail();
    }
    if(IsEmpty()) return;
    CLine l;
    l.AddTailList(this);
    RemoveAll();
    SharedPtrCWord last;
    pos = l.GetHeadPosition();
    while(pos)
    {
        SharedPtrCWord w = l.GetNext(pos);
        if(!last || !last->Append(w))
            AddTail(last = w->Copy());
    }
    m_ascent = m_descent = m_borderX = m_borderY = 0;
    pos = GetHeadPosition();
    while(pos)
    {
        SharedPtrCWord w = GetNext(pos);
        if(m_ascent < w->m_ascent) m_ascent = w->m_ascent;
        if(m_descent < w->m_descent) m_descent = w->m_descent;
        if(m_borderX < w->m_style.get().outlineWidthX) m_borderX = (int)(w->m_style.get().outlineWidthX+0.5);
        if(m_borderY < w->m_style.get().outlineWidthY) m_borderY = (int)(w->m_style.get().outlineWidthY+0.5);
    }
}

CRect CLine::PaintAll( CompositeDrawItemList* output, SubPicDesc& spd, const CRect& clipRect, 
    const SharedPtrCClipper &clipper, CPoint p, const CPoint& org, const int time, const int alpha )
{
    CRect bbox(0, 0, 0, 0);
    POSITION pos = GetHeadPosition();
    POSITION outputPos = output->GetHeadPosition();
    while(pos)
    {
        SharedPtrCWord w = GetNext(pos);
        CompositeDrawItem& outputItem = output->GetNext(outputPos);
        if(w->m_fLineBreak) return(bbox); // should not happen since this class is just a line of text without any breaks
        CPoint shadowPos, outlinePos, bodyPos, transOrg;
        shadowPos.x = p.x + static_cast<int>(w->m_style.get().shadowDepthX+0.5);
        shadowPos.y = p.y + m_ascent - w->m_ascent + static_cast<int>(w->m_style.get().shadowDepthY+0.5);
        outlinePos = CPoint(p.x, p.y + m_ascent - w->m_ascent);
        bodyPos = CPoint(p.x, p.y + m_ascent - w->m_ascent);
        bool hasShadow = w->m_style.get().shadowDepthX != 0 || w->m_style.get().shadowDepthY != 0;
        bool hasOutline = w->m_style.get().outlineWidthX+w->m_style.get().outlineWidthY > 0 && !(w->m_ktype == 2 && time < w->m_kstart);
        bool hasBody = true;

        SharedPtrOverlay shadowOverlay, outlineOverlay, bodyOverlay;
        CWord::PaintAll(w, shadowPos, outlinePos, bodyPos, org,
            hasShadow ? &shadowOverlay : NULL, 
            hasOutline ? &outlineOverlay : NULL, 
            hasBody ? &bodyOverlay : NULL);
        //shadow
        if(hasShadow)
        {    
            DWORD a = 0xff - w->m_style.get().alpha[3];
            if(alpha > 0) a = MulDiv(a, 0xff - alpha, 0xff);
            COLORREF shadow = revcolor(w->m_style.get().colors[3]) | (a<<24);
            DWORD sw[6] = {shadow, -1};
            //xy
            if(spd.type == MSP_XY_AUYV)
            {
                sw[0] =rgb2yuv(sw[0], XY_AUYV);
            }
            else if(spd.type == MSP_AYUV || spd.type == MSP_AYUV_PLANAR)
            {
                sw[0] =rgb2yuv(sw[0], XY_AYUV);
            }            
            if(w->m_style.get().borderStyle == 0)
            {
                outputItem.shadow.reset( 
                    DrawItem::CreateDrawItem(shadowOverlay, clipRect, clipper, shadowPos.x, shadowPos.y, sw,
                    w->m_ktype > 0 || w->m_style.get().alpha[0] < 0xff,
                    (w->m_style.get().outlineWidthX+w->m_style.get().outlineWidthY > 0) && !(w->m_ktype == 2 && time < w->m_kstart))
                    );
            }
            else if(w->m_style.get().borderStyle == 1 && w->m_pOpaqueBox)
            {
                outputItem.shadow.reset( 
                    DrawItem::CreateDrawItem( shadowOverlay, clipRect, clipper, shadowPos.x, shadowPos.y, sw, true, false)
                    );
            }
        }
        //outline
        if(hasOutline)
        {              
            DWORD aoutline = w->m_style.get().alpha[2];
            if(alpha > 0) aoutline += MulDiv(alpha, 0xff - w->m_style.get().alpha[2], 0xff);
            COLORREF outline = revcolor(w->m_style.get().colors[2]) | ((0xff-aoutline)<<24);
            DWORD sw[6] = {outline, -1};
            //xy
            if(spd.type == MSP_XY_AUYV)
            {
                sw[0] =rgb2yuv(sw[0], XY_AUYV);
            }
            else if(spd.type == MSP_AYUV || spd.type == MSP_AYUV_PLANAR)
            {
                sw[0] =rgb2yuv(sw[0], XY_AYUV);
            }
            if(w->m_style.get().borderStyle == 0)
            {
                outputItem.outline.reset( 
                    DrawItem::CreateDrawItem(outlineOverlay, clipRect, clipper, outlinePos.x, outlinePos.y, sw, !w->m_style.get().alpha[0] && !w->m_style.get().alpha[1] && !alpha, true)
                    );
            }
            else if(w->m_style.get().borderStyle == 1 && w->m_pOpaqueBox)
            {
                outputItem.outline.reset( 
                    DrawItem::CreateDrawItem(outlineOverlay, clipRect, clipper, outlinePos.x, outlinePos.y, sw, true, false)
                    );
            }
        }
        //body
        if(hasBody)
        {   
            // colors
            DWORD aprimary = w->m_style.get().alpha[0];
            if(alpha > 0) aprimary += MulDiv(alpha, 0xff - w->m_style.get().alpha[0], 0xff);
            COLORREF primary = revcolor(w->m_style.get().colors[0]) | ((0xff-aprimary)<<24);
            DWORD asecondary = w->m_style.get().alpha[1];
            if(alpha > 0) asecondary += MulDiv(alpha, 0xff - w->m_style.get().alpha[1], 0xff);
            COLORREF secondary = revcolor(w->m_style.get().colors[1]) | ((0xff-asecondary)<<24);
            DWORD sw[6] = {primary, 0, secondary};
            // karaoke
            double t;
            if(w->m_ktype == 0 || w->m_ktype == 2)
            {
                t = time < w->m_kstart ? 0 : 1;
            }
            else if(w->m_ktype == 1)
            {
                if(time < w->m_kstart) t = 0;
                else if(time < w->m_kend)
                {
                    t = 1.0 * (time - w->m_kstart) / (w->m_kend - w->m_kstart);
                    double angle = fmod(w->m_style.get().fontAngleZ, 360.0);
                    if(angle > 90 && angle < 270)
                    {
                        t = 1-t;
                        COLORREF tmp = sw[0];
                        sw[0] = sw[2];
                        sw[2] = tmp;
                    }
                }
                else t = 1.0;
            }
            if(t >= 1)
            {
                sw[1] = 0xffffffff;
            }
            sw[3] = (int)(w->m_style.get().outlineWidthX + t*w->m_width) >> 3;
            sw[4] = sw[2];
            sw[5] = 0x00ffffff;
            //xy
            if(spd.type == MSP_XY_AUYV)
            {
                sw[0] =rgb2yuv(sw[0], XY_AUYV);
                sw[2] =rgb2yuv(sw[2], XY_AUYV);
                sw[4] =rgb2yuv(sw[4], XY_AUYV);
            }
            else if(spd.type == MSP_AYUV || spd.type == MSP_AYUV_PLANAR)
            {
                sw[0] =rgb2yuv(sw[0], XY_AYUV);
                sw[2] =rgb2yuv(sw[2], XY_AYUV);
                sw[4] =rgb2yuv(sw[4], XY_AYUV);
            }
            outputItem.body.reset( 
                DrawItem::CreateDrawItem(bodyOverlay, clipRect, clipper, bodyPos.x, bodyPos.y, sw, true, false)
                );
        }
        bbox |= CompositeDrawItem::GetDirtyRect(outputItem);
        p.x += w->m_width;
    }
    return(bbox);
}

void CLine::AddWord2Tail( SharedPtrCWord words )
{
    __super::AddTail(words);
}

bool CLine::IsEmpty()
{
    return __super::IsEmpty();
}

int CLine::GetWordCount()
{
    return GetCount();
}

// CSubtitle

CSubtitle::CSubtitle()
{
    memset(m_effects, 0, sizeof(Effect*)*EF_NUMBEROFEFFECTS);
    m_clipInverse = false;
    m_scalex = m_scaley = 1;
    m_fAnimated2 = false;
}

CSubtitle::~CSubtitle()
{
    Empty();
}

void CSubtitle::Empty()
{
    POSITION pos = GetHeadPosition();
    while(pos) delete GetNext(pos);
//    pos = m_words.GetHeadPosition();
//    while(pos) delete m_words.GetNext(pos);
    for(int i = 0; i < EF_NUMBEROFEFFECTS; i++) {if(m_effects[i]) delete m_effects[i];}
    memset(m_effects, 0, sizeof(Effect*)*EF_NUMBEROFEFFECTS);
}

int CSubtitle::GetFullWidth()
{
    int width = 0;
    POSITION pos = m_words.GetHeadPosition();
    while(pos) width += m_words.GetNext(pos)->m_width;
    return(width);
}

int CSubtitle::GetFullLineWidth(POSITION pos)
{
    int width = 0;
    while(pos)
    {
        SharedPtrCWord w = m_words.GetNext(pos);
        if(w->m_fLineBreak) break;
        width += w->m_width;
    }
    return(width);
}

int CSubtitle::GetWrapWidth(POSITION pos, int maxwidth)
{
    if(m_wrapStyle == 0 || m_wrapStyle == 3)
    {
        if(maxwidth > 0)
        {
//          int fullwidth = GetFullWidth();
            int fullwidth = GetFullLineWidth(pos);
            int minwidth = fullwidth / ((abs(fullwidth) / maxwidth) + 1);
            int width = 0, wordwidth = 0;
            while(pos && width < minwidth)
            {
                SharedPtrCWord w = m_words.GetNext(pos);
                wordwidth = w->m_width;
                if(abs(width + wordwidth) < abs(maxwidth)) width += wordwidth;
            }
            maxwidth = width;
            if(m_wrapStyle == 3 && pos) maxwidth -= wordwidth;
        }
    }
    else if(m_wrapStyle == 1)
    {
//      maxwidth = maxwidth;
    }
    else if(m_wrapStyle == 2)
    {
        maxwidth = INT_MAX;
    }
    return(maxwidth);
}

CLine* CSubtitle::GetNextLine(POSITION& pos, int maxwidth)
{
    if(pos == NULL) return(NULL);
    CLine* ret = new CLine();
    if(!ret) return(NULL);
    ret->m_width = ret->m_ascent = ret->m_descent = ret->m_borderX = ret->m_borderY = 0;
    maxwidth = GetWrapWidth(pos, maxwidth);
    bool fEmptyLine = true;
    while(pos)
    {
        SharedPtrCWord w = m_words.GetNext(pos);
        if(ret->m_ascent < w->m_ascent) ret->m_ascent = w->m_ascent;
        if(ret->m_descent < w->m_descent) ret->m_descent = w->m_descent;
        if(ret->m_borderX < w->m_style.get().outlineWidthX) ret->m_borderX = (int)(w->m_style.get().outlineWidthX+0.5);
        if(ret->m_borderY < w->m_style.get().outlineWidthY) ret->m_borderY = (int)(w->m_style.get().outlineWidthY+0.5);
        if(w->m_fLineBreak)
        {
            if(fEmptyLine) {ret->m_ascent /= 2; ret->m_descent /= 2; ret->m_borderX = ret->m_borderY = 0;}
            ret->Compact();
            return(ret);
        }
        fEmptyLine = false;
        bool fWSC = w->m_fWhiteSpaceChar;
        int width = w->m_width;
        POSITION pos2 = pos;
        while(pos2)
        {
            if(m_words.GetAt(pos2)->m_fWhiteSpaceChar != fWSC
                    || m_words.GetAt(pos2)->m_fLineBreak) break;
            SharedPtrCWord w2 = m_words.GetNext(pos2);
            width += w2->m_width;
        }
        if((ret->m_width += width) <= maxwidth || ret->IsEmpty())
        {
            ret->AddWord2Tail(w);
            while(pos != pos2)
            {
                ret->AddWord2Tail(m_words.GetNext(pos));
            }
            pos = pos2;
        }
        else
        {
            if(pos) m_words.GetPrev(pos);
            else pos = m_words.GetTailPosition();
            ret->m_width -= width;
            break;
        }
    }
    ret->Compact();
    return(ret);
}

void CSubtitle::CreateClippers(CSize size)
{
    size.cx >>= 3;
    size.cy >>= 3;
    if(m_effects[EF_BANNER] && m_effects[EF_BANNER]->param[2])
    {
        int w = size.cx, h = size.cy;
        if(!m_pClipper)
        {
            CStringW str;
            str.Format(L"m %d %d l %d %d %d %d %d %d", 0, 0, w, 0, w, h, 0, h);
            m_pClipper.reset( new CClipper(str, size, 1, 1, false) );
            if(!m_pClipper) return;
        }
        m_pClipper->SetEffect( *m_effects[EF_BANNER], EF_BANNER );
    }
    else if(m_effects[EF_SCROLL] && m_effects[EF_SCROLL]->param[4])
    {
        int height = m_effects[EF_SCROLL]->param[4];
        int w = size.cx, h = size.cy;
        if(!m_pClipper)
        {
            CStringW str;
            str.Format(L"m %d %d l %d %d %d %d %d %d", 0, 0, w, 0, w, h, 0, h);
            m_pClipper.reset( new CClipper(str, size, 1, 1, false) ); 
            if(!m_pClipper) return;
        }
        m_pClipper->SetEffect(*m_effects[EF_SCROLL], EF_SCROLL);
    }
}

void CSubtitle::MakeLines(CSize size, CRect marginRect)
{
    CSize spaceNeeded(0, 0);
    bool fFirstLine = true;
    m_topborder = m_bottomborder = 0;
    CLine* l = NULL;
    POSITION pos = m_words.GetHeadPosition();
    while(pos)
    {
        l = GetNextLine(pos, size.cx - marginRect.left - marginRect.right);
        if(!l) break;
        if(fFirstLine) {m_topborder = l->m_borderY; fFirstLine = false;}
        spaceNeeded.cx = max(l->m_width+l->m_borderX, spaceNeeded.cx);
        spaceNeeded.cy += l->m_ascent + l->m_descent;
        AddTail(l);
    }
    if(l) m_bottomborder = l->m_borderY;
    m_rect = CRect(
                 CPoint((m_scrAlignment%3) == 1 ? marginRect.left
                        : (m_scrAlignment%3) == 2 ? (marginRect.left + (size.cx - marginRect.right) - spaceNeeded.cx + 1) / 2
                        : (size.cx - marginRect.right - spaceNeeded.cx),
                        m_scrAlignment <= 3 ? (size.cy - marginRect.bottom - spaceNeeded.cy)
                        : m_scrAlignment <= 6 ? (marginRect.top + (size.cy - marginRect.bottom) - spaceNeeded.cy + 1) / 2
                        : marginRect.top),
                 spaceNeeded);
}

POSITION CSubtitle::GetHeadLinePosition()
{
    return __super::GetHeadPosition();
}

CLine* CSubtitle::GetNextLine( POSITION& pos )
{
    return __super::GetNext(pos);
}

// CScreenLayoutAllocator

void CScreenLayoutAllocator::Empty()
{
    m_subrects.RemoveAll();
}

void CScreenLayoutAllocator::AdvanceToSegment(int segment, const CAtlArray<int>& sa)
{
    POSITION pos = m_subrects.GetHeadPosition();
    while(pos)
    {
        POSITION prev = pos;
        SubRect& sr = m_subrects.GetNext(pos);
        bool fFound = false;
        if(abs(sr.segment - segment) <= 1) // using abs() makes it possible to play the subs backwards, too :)
        {
            for(size_t i = 0; i < sa.GetCount() && !fFound; i++)
            {
                if(sa[i] == sr.entry)
                {
                    sr.segment = segment;
                    fFound = true;
                }
            }
        }
        if(!fFound) m_subrects.RemoveAt(prev);
    }
}

CRect CScreenLayoutAllocator::AllocRect(CSubtitle* s, int segment, int entry, int layer, int collisions)
{
    // TODO: handle collisions == 1 (reversed collisions)
    POSITION pos = m_subrects.GetHeadPosition();
    while(pos)
    {
        SubRect& sr = m_subrects.GetNext(pos);
        if(sr.segment == segment && sr.entry == entry)
        {
            return(sr.r + CRect(0, -s->m_topborder, 0, -s->m_bottomborder));
        }
    }
    CRect r = s->m_rect + CRect(0, s->m_topborder, 0, s->m_bottomborder);
    bool fSearchDown = s->m_scrAlignment > 3;
    bool fOK;
    do
    {
        fOK = true;
        pos = m_subrects.GetHeadPosition();
        while(pos)
        {
            SubRect& sr = m_subrects.GetNext(pos);
            if(layer == sr.layer && !(r & sr.r).IsRectEmpty())
            {
                if(fSearchDown)
                {
                    r.bottom = sr.r.bottom + r.Height();
                    r.top = sr.r.bottom;
                }
                else
                {
                    r.top = sr.r.top - r.Height();
                    r.bottom = sr.r.top;
                }
                fOK = false;
            }
        }
    }
    while(!fOK);
    SubRect sr;
    sr.r = r;
    sr.segment = segment;
    sr.entry = entry;
    sr.layer = layer;
    m_subrects.AddTail(sr);
    return(sr.r + CRect(0, -s->m_topborder, 0, -s->m_bottomborder));
}

// CRenderedTextSubtitle

CAtlMap<CStringW, CRenderedTextSubtitle::AssCmdType, CStringElementTraits<CStringW>> CRenderedTextSubtitle::m_cmdMap;

CRenderedTextSubtitle::CRenderedTextSubtitle(CCritSec* pLock)
    : CSubPicProviderImpl(pLock)
{
    if( m_cmdMap.IsEmpty() )
    {
        InitCmdMap();
    }
    m_size = CSize(0, 0);
    if(g_hDC_refcnt == 0)
    {
        g_hDC = CreateCompatibleDC(NULL);
        SetBkMode(g_hDC, TRANSPARENT);
        SetTextColor(g_hDC, 0xffffff);
        SetMapMode(g_hDC, MM_TEXT);
    }
    g_hDC_refcnt++;
}

CRenderedTextSubtitle::~CRenderedTextSubtitle()
{
    Deinit();
    g_hDC_refcnt--;
    if(g_hDC_refcnt == 0) DeleteDC(g_hDC);
}

void CRenderedTextSubtitle::InitCmdMap()
{
    if( m_cmdMap.IsEmpty() )
    {
        m_cmdMap.SetAt(L"1c",        CMD_1c);
        m_cmdMap.SetAt(L"2c",        CMD_2c);
        m_cmdMap.SetAt(L"3c",        CMD_3c);
        m_cmdMap.SetAt(L"4c",        CMD_4c);
        m_cmdMap.SetAt(L"1a",        CMD_1a);
        m_cmdMap.SetAt(L"2a",        CMD_2a);
        m_cmdMap.SetAt(L"3a",        CMD_3a);
        m_cmdMap.SetAt(L"4a",        CMD_4a);
        m_cmdMap.SetAt(L"alpha",     CMD_alpha);
        m_cmdMap.SetAt(L"an",        CMD_an);
        m_cmdMap.SetAt(L"a",         CMD_a);
        m_cmdMap.SetAt(L"blur",      CMD_blur);
        m_cmdMap.SetAt(L"bord",      CMD_bord);
        m_cmdMap.SetAt(L"be",        CMD_be);
        m_cmdMap.SetAt(L"b",         CMD_b);
        m_cmdMap.SetAt(L"clip",      CMD_clip);
        m_cmdMap.SetAt(L"iclip",     CMD_iclip);
        m_cmdMap.SetAt(L"c",         CMD_c);
        m_cmdMap.SetAt(L"fade",      CMD_fade);
        m_cmdMap.SetAt(L"fad",       CMD_fad);
        m_cmdMap.SetAt(L"fax",       CMD_fax);
        m_cmdMap.SetAt(L"fay",       CMD_fay);
        m_cmdMap.SetAt(L"fe",        CMD_fe);
        m_cmdMap.SetAt(L"fn",        CMD_fn);
        m_cmdMap.SetAt(L"frx",       CMD_frx);
        m_cmdMap.SetAt(L"fry",       CMD_fry);
        m_cmdMap.SetAt(L"frz",       CMD_frz);
        m_cmdMap.SetAt(L"fr",        CMD_fr);
        m_cmdMap.SetAt(L"fscx",      CMD_fscx);
        m_cmdMap.SetAt(L"fscy",      CMD_fscy);
        m_cmdMap.SetAt(L"fsc",       CMD_fsc);
        m_cmdMap.SetAt(L"fsp",       CMD_fsp);
        m_cmdMap.SetAt(L"fs",        CMD_fs);
        m_cmdMap.SetAt(L"i",         CMD_i);
        m_cmdMap.SetAt(L"kt",        CMD_kt);
        m_cmdMap.SetAt(L"kf",        CMD_kf);
        m_cmdMap.SetAt(L"K",         CMD_K);
        m_cmdMap.SetAt(L"ko",        CMD_ko);
        m_cmdMap.SetAt(L"k",         CMD_k);
        m_cmdMap.SetAt(L"move",      CMD_move);
        m_cmdMap.SetAt(L"org",       CMD_org);
        m_cmdMap.SetAt(L"pbo",       CMD_pbo);
        m_cmdMap.SetAt(L"pos",       CMD_pos);
        m_cmdMap.SetAt(L"p",         CMD_p);
        m_cmdMap.SetAt(L"q",         CMD_q);
        m_cmdMap.SetAt(L"r",         CMD_r);
        m_cmdMap.SetAt(L"shad",      CMD_shad);
        m_cmdMap.SetAt(L"s",         CMD_s);
        m_cmdMap.SetAt(L"t",         CMD_t);
        m_cmdMap.SetAt(L"u",         CMD_u);
        m_cmdMap.SetAt(L"xbord",     CMD_xbord);
        m_cmdMap.SetAt(L"xshad",     CMD_xshad);
        m_cmdMap.SetAt(L"ybord",     CMD_ybord);
        m_cmdMap.SetAt(L"yshad",     CMD_yshad);
    }
}

void CRenderedTextSubtitle::Copy(CSimpleTextSubtitle& sts)
{
    __super::Copy(sts);
    m_size = CSize(0, 0);
    if(CRenderedTextSubtitle* pRTS = dynamic_cast<CRenderedTextSubtitle*>(&sts))
    {
        m_size = pRTS->m_size;
    }
}

void CRenderedTextSubtitle::Empty()
{
    Deinit();
    __super::Empty();
}

void CRenderedTextSubtitle::OnChanged()
{
    __super::OnChanged();
    POSITION pos = m_subtitleCache.GetStartPosition();
    while(pos)
    {
        int i;
        CSubtitle* s;
        m_subtitleCache.GetNextAssoc(pos, i, s);
        delete s;
    }
    m_subtitleCache.RemoveAll();
    m_sla.Empty();
}

bool CRenderedTextSubtitle::Init(CSize size, CRect vidrect)
{
    Deinit();
    m_size = CSize(size.cx*8, size.cy*8);
    m_vidrect = CRect(vidrect.left*8, vidrect.top*8, vidrect.right*8, vidrect.bottom*8);
    m_sla.Empty();
    return(true);
}

void CRenderedTextSubtitle::Deinit()
{
    POSITION pos = m_subtitleCache.GetStartPosition();
    while(pos)
    {
        int i;
        CSubtitle* s;
        m_subtitleCache.GetNextAssoc(pos, i, s);
        delete s;
    }
    m_subtitleCache.RemoveAll();
    m_sla.Empty();
    m_size = CSize(0, 0);
    m_vidrect.SetRectEmpty();

    CacheManager::GetCWordMruCache()->RemoveAll();
    CacheManager::GetPathDataMruCache()->RemoveAll();
    CacheManager::GetScanLineData2MruCache()->RemoveAll();
    CacheManager::GetOverlayNoBlurMruCache()->RemoveAll();
    CacheManager::GetOverlayMruCache()->RemoveAll();
    CacheManager::GetAssTagListMruCache()->RemoveAll();
    CacheManager::GetSubpixelVarianceCache()->RemoveAll();
    CacheManager::GetTextInfoCache()->RemoveAll();
}

void CRenderedTextSubtitle::ParseEffect(CSubtitle* sub, const CStringW& str)
{
    CStringW::PCXSTR str_start = str.GetString();
    CStringW::PCXSTR str_end = str_start + str.GetLength();
    str_start = SkipWhiteSpaceLeft(str_start, str_end);

    if(!sub || *str_start==0)
        return;

    str_end = FastSkipWhiteSpaceRight(str_start, str_end);

    const WCHAR* s = FindChar(str_start, str_end, L';');
    if(*s==L';') {
        s++;
    }
    
    const CStringW effect(str_start, s-str_start);
    if(!effect.CompareNoCase( L"Banner;" ) )
    {
        int delay, lefttoright = 0, fadeawaywidth = 0;
        if(swscanf(s, L"%d;%d;%d", &delay, &lefttoright, &fadeawaywidth) < 1) return;
        Effect* e = new Effect;
        if(!e) return;
        sub->m_effects[e->type = EF_BANNER] = e;
        e->param[0] = (int)(max(1.0*delay/sub->m_scalex, 1));
        e->param[1] = lefttoright;
        e->param[2] = (int)(sub->m_scalex*fadeawaywidth);
        sub->m_wrapStyle = 2;
    }
    else if(!effect.CompareNoCase(L"Scroll up;") || !effect.CompareNoCase(L"Scroll down;"))
    {
        int top, bottom, delay, fadeawayheight = 0;
        if(swscanf(s, L"%d;%d;%d;%d", &top, &bottom, &delay, &fadeawayheight) < 3) return;
        if(top > bottom) {int tmp = top; top = bottom; bottom = tmp;}
        Effect* e = new Effect;
        if(!e) return;
        sub->m_effects[e->type = EF_SCROLL] = e;
        e->param[0] = (int)(sub->m_scaley*top*8);
        e->param[1] = (int)(sub->m_scaley*bottom*8);
        e->param[2] = (int)(max(1.0*delay/sub->m_scaley, 1));
        e->param[3] = (effect.GetLength() == 12);
        e->param[4] = (int)(sub->m_scaley*fadeawayheight);
    }
}

void CRenderedTextSubtitle::ParseString(CSubtitle* sub, CStringW str, const FwSTSStyle& style)
{
    if(!sub) return;
    str.Replace(L"\\N", L"\n");
    str.Replace(L"\\n", (sub->m_wrapStyle < 2 || sub->m_wrapStyle == 3) ? L" " : L"\n");
    str.Replace(L"\\h", L"\x00A0");    
    for(int ite = 0, j = 0, len = str.GetLength(); j <= len; j++)
    {
        WCHAR c = str[j];
        if(c != L'\n' && c != L' ' && c != 0)
            continue;
        if(ite < j)
        {
            if(PCWord tmp_ptr = new CText(style, str.Mid(ite, j-ite), m_ktype, m_kstart, m_kend))
            {
                SharedPtrCWord w(tmp_ptr);
                sub->m_words.AddTail(w);
            }
            else
            {
                ///TODO: overflow handling
            }
            m_kstart = m_kend;
        }
        if(c == L'\n')
        {
            if(PCWord tmp_ptr = new CText(style, CStringW(), m_ktype, m_kstart, m_kend))
            {
                SharedPtrCWord w(tmp_ptr);
                sub->m_words.AddTail(w);
            }
            else
            {
                ///TODO: overflow handling
            }
            m_kstart = m_kend;
        }
        else if(c == L' ')
        {
            if(PCWord tmp_ptr = new CText(style, CStringW(c), m_ktype, m_kstart, m_kend))
            {
                SharedPtrCWord w(tmp_ptr);
                sub->m_words.AddTail(w);
            }
            else
            {
                ///TODO: overflow handling
            }
            m_kstart = m_kend;
        }
        ite = j+1;
    }
    return;
}

void CRenderedTextSubtitle::ParsePolygon(CSubtitle* sub, const CStringW& str, const FwSTSStyle& style)
{
    if(!sub || !str.GetLength() || !m_nPolygon) return;

    if(PCWord tmp_ptr = new CPolygon(style, str, m_ktype, m_kstart, m_kend, sub->m_scalex/(1<<(m_nPolygon-1)), sub->m_scaley/(1<<(m_nPolygon-1)), m_polygonBaselineOffset))
    {
        SharedPtrCWord w(tmp_ptr);
        ///Todo: fix me
        //if( PCWord w_cache = m_wordCache.lookup(*w) )
        //{
        //    sub->m_words.AddTail(w_cache);
        //    delete w;
        //}
        //else
        {
            sub->m_words.AddTail(w);            
        }
        m_kstart = m_kend;
    }
}

bool CRenderedTextSubtitle::ParseSSATag( AssTagList *assTags, const CStringW& str )
{
    if(!assTags) return(false);
    int nTags = 0, nUnrecognizedTags = 0;
    for(int i = 0, j; (j = str.Find(L'\\', i)) >= 0; i = j)
    {
        POSITION pos = assTags->AddTail();
        AssTag& assTag = assTags->GetAt(pos);
        assTag.cmdType = CMD_COUNT;

        j++;
        CStringW::PCXSTR str_start = str.GetString() + j;
        CStringW::PCXSTR pc = str_start;
        while( iswspace(*pc) ) 
        {
            pc++;
        }
        j += pc-str_start;
        str_start = pc;
        while( *pc && *pc != L'(' && *pc != L'\\' )
        {
            pc++;
        }
        j += pc-str_start;        
        if( pc-str_start>0 )
        {
            while( iswspace(*--pc) );
            pc++;
        }
                
        const CStringW cmd(str_start, pc-str_start);
        if(cmd.IsEmpty()) continue;

        CAtlArray<CStringW>& params = assTag.strParams;
        if(str[j] == L'(')
        {
            j++;
            CStringW::PCXSTR str_start = str.GetString() + j;
            CStringW::PCXSTR pc = str_start;
            while( iswspace(*pc) ) 
            {
                pc++;
            }
            j += pc-str_start;
            str_start = pc;
            while( *pc && *pc != L')' )
            {
                pc++;
            }
            j += pc-str_start;        
            if( pc-str_start>0 )
            {
                while( iswspace(*--pc) );
                pc++;
            }

            CStringW::PCXSTR param_start = str_start;
            CStringW::PCXSTR param_end = pc;
            while( param_start<param_end )
            {
                param_start = SkipWhiteSpaceLeft(param_start, param_end);

                CStringW::PCXSTR newstart = FindChar(param_start, param_end, L',');
                CStringW::PCXSTR newend = FindChar(param_start, param_end, L'\\');
                if(newstart > param_start && newstart < newend)
                {
                    newstart = FastSkipWhiteSpaceRight(param_start, newstart);
                    CStringW s(param_start, newstart - param_start);

                    if(!s.IsEmpty()) params.Add(s);
                    param_start = newstart + 1;
                }
                else if(param_start<param_end)
                {
                    CStringW s(param_start, param_end - param_start);

                    params.Add(s);
                    param_start = param_end;
                }
            }
        }

        AssCmdType cmd_type = CMD_COUNT;
        int cmd_length = min(MAX_CMD_LENGTH, cmd.GetLength());
        for( ;cmd_length>=MIN_CMD_LENGTH;cmd_length-- )
        {
            if( m_cmdMap.Lookup(cmd.Left(cmd_length), cmd_type) )
                break;
        }
        if(cmd_length<MIN_CMD_LENGTH)
            cmd_type = CMD_COUNT;
        switch( cmd_type )
        {
        case CMD_fax:
        case CMD_fay:
        case CMD_fe:
        case CMD_fn:
        case CMD_frx:
        case CMD_fry:
        case CMD_frz:
        case CMD_fr:
        case CMD_fscx:
        case CMD_fscy:
        case CMD_fsc:
        case CMD_fsp:
        case CMD_fs:
        case CMD_i:
        case CMD_kt:
        case CMD_kf:
        case CMD_K:
        case CMD_ko:
        case CMD_k:
        case CMD_pbo:
        case CMD_p:
        case CMD_q:
        case CMD_r:
        case CMD_shad:
        case CMD_s:
        case CMD_an:
        case CMD_a:
        case CMD_blur:
        case CMD_bord:
        case CMD_be:
        case CMD_b:
        case CMD_u:
        case CMD_xbord:
        case CMD_xshad:
        case CMD_ybord:
        case CMD_yshad:
            //        default:
            params.Add(cmd.Mid(cmd_length));
            break;
        case CMD_c:
        case CMD_1c :
        case CMD_2c :
        case CMD_3c :
        case CMD_4c :
        case CMD_1a :
        case CMD_2a :
        case CMD_3a :
        case CMD_4a :
        case CMD_alpha:
            params.Add(cmd.Mid(cmd_length).Trim(L"&H"));
            break;
        case CMD_clip:
        case CMD_iclip:
        case CMD_fade:
        case CMD_fad:
        case CMD_move:
        case CMD_org:
        case CMD_pos:
            break;
        case CMD_t:
            if (!params.IsEmpty())
                ParseSSATag(&assTag.embeded, params[params.GetCount()-1]);
            break;
        case CMD_COUNT:
            nUnrecognizedTags++;
            break;
        }

        assTag.cmdType = cmd_type;
        
        nTags++;
    }
    return(true);
}

bool CRenderedTextSubtitle::ParseSSATag( CSubtitle* sub, const AssTagList& assTags, STSStyle& style, const STSStyle& org, bool fAnimate /*= false*/ )
{
    if(!sub) return(false);
    
    POSITION pos = assTags.GetHeadPosition();
    while(pos)
    {
        const AssTag& assTag = assTags.GetNext(pos);
        AssCmdType cmd_type = assTag.cmdType;
        const CAtlArray<CStringW>& params = assTag.strParams;

        // TODO: call ParseStyleModifier(cmd, params, ..) and move the rest there
        const CStringW& p = params.GetCount() > 0 ? params[0] : CStringW("");
        switch ( cmd_type )
        {
        case CMD_1c :
            {
                const int i = 0;
                DWORD c = wcstol(p, NULL, 16);
                style.colors[i] = !p.IsEmpty()
                    ? (((int)CalcAnimation(c&0xff, style.colors[i]&0xff, fAnimate))&0xff
                    |((int)CalcAnimation(c&0xff00, style.colors[i]&0xff00, fAnimate))&0xff00
                    |((int)CalcAnimation(c&0xff0000, style.colors[i]&0xff0000, fAnimate))&0xff0000)
                    : org.colors[i];
                break;
            }
        case CMD_2c :
            {
                const int i = 1;
                DWORD c = wcstol(p, NULL, 16);
                style.colors[i] = !p.IsEmpty()
                    ? (((int)CalcAnimation(c&0xff, style.colors[i]&0xff, fAnimate))&0xff
                    |((int)CalcAnimation(c&0xff00, style.colors[i]&0xff00, fAnimate))&0xff00
                    |((int)CalcAnimation(c&0xff0000, style.colors[i]&0xff0000, fAnimate))&0xff0000)
                    : org.colors[i];
                break;
            }
        case CMD_3c :
            {
                const int i = 2;
                DWORD c = wcstol(p, NULL, 16);
                style.colors[i] = !p.IsEmpty()
                    ? (((int)CalcAnimation(c&0xff, style.colors[i]&0xff, fAnimate))&0xff
                    |((int)CalcAnimation(c&0xff00, style.colors[i]&0xff00, fAnimate))&0xff00
                    |((int)CalcAnimation(c&0xff0000, style.colors[i]&0xff0000, fAnimate))&0xff0000)
                    : org.colors[i];
                break;
            }
        case CMD_4c :
            {
                const int i = 3;
                DWORD c = wcstol(p, NULL, 16);
                style.colors[i] = !p.IsEmpty()
                                  ? (((int)CalcAnimation(c&0xff, style.colors[i]&0xff, fAnimate))&0xff
                                     |((int)CalcAnimation(c&0xff00, style.colors[i]&0xff00, fAnimate))&0xff00
                                     |((int)CalcAnimation(c&0xff0000, style.colors[i]&0xff0000, fAnimate))&0xff0000)
                                  : org.colors[i];
                break;
            }
        case CMD_1a :
            {
                int i = 0;
                style.alpha[i] = !p.IsEmpty()
                    ? (BYTE)CalcAnimation(wcstol(p, NULL, 16), style.alpha[i], fAnimate)
                    : org.alpha[i];
                break;
            }
        case CMD_2a :
            {
                int i = 1;
                style.alpha[i] = !p.IsEmpty()
                    ? (BYTE)CalcAnimation(wcstol(p, NULL, 16), style.alpha[i], fAnimate)
                    : org.alpha[i];
                break;
            }
        case CMD_3a :
            {
                int i = 2;
                style.alpha[i] = !p.IsEmpty()
                    ? (BYTE)CalcAnimation(wcstol(p, NULL, 16), style.alpha[i], fAnimate)
                    : org.alpha[i];
                break;
            }
        case CMD_4a :
            {
                int i = 3;
                style.alpha[i] = !p.IsEmpty()
                                 ? (BYTE)CalcAnimation(wcstol(p, NULL, 16), style.alpha[i], fAnimate)
                                 : org.alpha[i];
                break;
            }
        case CMD_alpha:
            {
                for(int i = 0; i < 4; i++)
                {
                    style.alpha[i] = !p.IsEmpty()
                                     ? (BYTE)CalcAnimation(wcstol(p, NULL, 16), style.alpha[i], fAnimate)
                                     : org.alpha[i];
                }
                break;
            }
        case CMD_an:
            {
                int n = wcstol(p, NULL, 10);
                if(sub->m_scrAlignment < 0)
                    sub->m_scrAlignment = (n > 0 && n < 10) ? n : org.scrAlignment;
                break;
            }
        case CMD_a:
            {
                int n = wcstol(p, NULL, 10);
                if(sub->m_scrAlignment < 0)
                    sub->m_scrAlignment = (n > 0 && n < 12) ? ((((n-1)&3)+1)+((n&4)?6:0)+((n&8)?3:0)) : org.scrAlignment;
                break;
            }
        case CMD_blur:
            {
                double n = CalcAnimation(wcstod(p, NULL), style.fGaussianBlur, fAnimate);
                style.fGaussianBlur = !p.IsEmpty()
                                      ? (n < 0 ? 0 : n)
                                          : org.fGaussianBlur;
                break;
            }
        case CMD_bord:
            {
                double dst = wcstod(p, NULL);
                double nx = CalcAnimation(dst, style.outlineWidthX, fAnimate);
                style.outlineWidthX = !p.IsEmpty()
                                      ? (nx < 0 ? 0 : nx)
                                          : org.outlineWidthX;
                double ny = CalcAnimation(dst, style.outlineWidthY, fAnimate);
                style.outlineWidthY = !p.IsEmpty()
                                      ? (ny < 0 ? 0 : ny)
                                          : org.outlineWidthY;
                break;
            }
        case CMD_be:
            {
                int n = (int)(CalcAnimation(wcstod(p, NULL), style.fBlur, fAnimate)+0.5);
                style.fBlur = !p.IsEmpty()
                              ? n
                              : org.fBlur;
                break;
            }
        case CMD_b:
            {
                int n = wcstol(p, NULL, 10);
                style.fontWeight = !p.IsEmpty()
                                   ? (n == 0 ? FW_NORMAL : n == 1 ? FW_BOLD : n >= 100 ? n : org.fontWeight)
                                       : org.fontWeight;
                break;
            }
        case CMD_clip:
        case CMD_iclip:
            {
                bool invert = (cmd_type == CMD_iclip);
                if(params.GetCount() == 1 && !sub->m_pClipper)
                {
                    sub->m_pClipper.reset( new CClipper(params[0], CSize(m_size.cx>>3, m_size.cy>>3), sub->m_scalex, sub->m_scaley, invert) );
                }
                else if(params.GetCount() == 2 && !sub->m_pClipper)
                {
                    int scale = max(wcstol(p, NULL, 10), 1);
                    sub->m_pClipper.reset( new CClipper(params[1], CSize(m_size.cx>>3, m_size.cy>>3), sub->m_scalex/(1<<(scale-1)), sub->m_scaley/(1<<(scale-1)), invert) );
                }
                else if(params.GetCount() == 4)
                {
                    CRect r;
                    sub->m_clipInverse = invert;
                    r.SetRect(
                        wcstod(params[0], NULL)+0.5,
                        wcstod(params[1], NULL)+0.5,
                        wcstod(params[2], NULL)+0.5,
                        wcstod(params[3], NULL)+0.5);
                    CPoint o(0, 0);
                    if(sub->m_relativeTo == 1) // TODO: this should also apply to the other two clippings above
                    {
                        o.x = m_vidrect.left>>3;
                        o.y = m_vidrect.top>>3;
                    }
                    sub->m_clip.SetRect(
                        (int)CalcAnimation(sub->m_scalex*r.left + o.x, sub->m_clip.left, fAnimate),
                        (int)CalcAnimation(sub->m_scaley*r.top + o.y, sub->m_clip.top, fAnimate),
                        (int)CalcAnimation(sub->m_scalex*r.right + o.x, sub->m_clip.right, fAnimate),
                        (int)CalcAnimation(sub->m_scaley*r.bottom + o.y, sub->m_clip.bottom, fAnimate));
                }
                break;
            }
        case CMD_c:
            {
                DWORD c = wcstol(p, NULL, 16);
                style.colors[0] = !p.IsEmpty()
                                  ? (((int)CalcAnimation(c&0xff, style.colors[0]&0xff, fAnimate))&0xff
                                     |((int)CalcAnimation(c&0xff00, style.colors[0]&0xff00, fAnimate))&0xff00
                                     |((int)CalcAnimation(c&0xff0000, style.colors[0]&0xff0000, fAnimate))&0xff0000)
                                  : org.colors[0];
                break;
            }
        case CMD_fade:
        case CMD_fad:
            {
                sub->m_fAnimated2 = true;
                if(params.GetCount() == 7 && !sub->m_effects[EF_FADE])// {\fade(a1=param[0], a2=param[1], a3=param[2], t1=t[0], t2=t[1], t3=t[2], t4=t[3])
                {
                    if(Effect* e = new Effect)
                    {
                        for(int i = 0; i < 3; i++)
                            e->param[i] = wcstol(params[i], NULL, 10);
                        for(int i = 0; i < 4; i++)
                            e->t[i] = wcstol(params[3+i], NULL, 10);
                        sub->m_effects[EF_FADE] = e;
                    }
                }
                else if(params.GetCount() == 2 && !sub->m_effects[EF_FADE]) // {\fad(t1=t[1], t2=t[2])
                {
                    if(Effect* e = new Effect)
                    {
                        e->param[0] = e->param[2] = 0xff;
                        e->param[1] = 0x00;
                        for(int i = 1; i < 3; i++)
                            e->t[i] = wcstol(params[i-1], NULL, 10);
                        e->t[0] = e->t[3] = -1; // will be substituted with "start" and "end"
                        sub->m_effects[EF_FADE] = e;
                    }
                }
                break;
            }
        case CMD_fax:
            {
                style.fontShiftX = !p.IsEmpty()
                                   ? CalcAnimation(wcstod(p, NULL), style.fontShiftX, fAnimate)
                                   : org.fontShiftX;
                break;
            }
        case CMD_fay:
            {
                style.fontShiftY = !p.IsEmpty()
                                   ? CalcAnimation(wcstod(p, NULL), style.fontShiftY, fAnimate)
                                   : org.fontShiftY;
                break;
            }
        case CMD_fe:
            {
                int n = wcstol(p, NULL, 10);
                style.charSet = !p.IsEmpty()
                                ? n
                                : org.charSet;
                break;
            }
        case CMD_fn:
            {
                if(!p.IsEmpty() && p != L'0')
                    style.fontName = CString(p).Trim();
                else
                    style.fontName = org.fontName;
                break;
            }
        case CMD_frx:
            {
                style.fontAngleX = !p.IsEmpty()
                                   ? CalcAnimation(wcstod(p, NULL), style.fontAngleX, fAnimate)
                                   : org.fontAngleX;
                break;
            }
        case CMD_fry:
            {
                style.fontAngleY = !p.IsEmpty()
                                   ? CalcAnimation(wcstod(p, NULL), style.fontAngleY, fAnimate)
                                   : org.fontAngleY;
                break;
            }
        case CMD_frz:
        case CMD_fr:
            {
                style.fontAngleZ = !p.IsEmpty()
                                   ? CalcAnimation(wcstod(p, NULL), style.fontAngleZ, fAnimate)
                                   : org.fontAngleZ;
                break;
            }
        case CMD_fscx:
            {
                double n = CalcAnimation(wcstod(p, NULL), style.fontScaleX, fAnimate);
                style.fontScaleX = !p.IsEmpty()
                                   ? ((n < 0) ? 0 : n)
                                       : org.fontScaleX;
                break;
            }
        case CMD_fscy:
            {
                double n = CalcAnimation(wcstod(p, NULL), style.fontScaleY, fAnimate);
                style.fontScaleY = !p.IsEmpty()
                                   ? ((n < 0) ? 0 : n)
                                       : org.fontScaleY;
                break;
            }
        case CMD_fsc:
            {
                style.fontScaleX = org.fontScaleX;
                style.fontScaleY = org.fontScaleY;
                break;
            }
        case CMD_fsp:
            {
                style.fontSpacing = !p.IsEmpty()
                                    ? CalcAnimation(wcstod(p, NULL), style.fontSpacing, fAnimate)
                                    : org.fontSpacing;
                break;
            }
        case CMD_fs:
            {
                if(!p.IsEmpty())
                {
                    if(p[0] == L'-' || p[0] == L'+')
                    {
                        double n = CalcAnimation(style.fontSize + style.fontSize*wcstod(p, NULL)/10, style.fontSize, fAnimate);
                        style.fontSize = (n > 0) ? n : org.fontSize;
                    }
                    else
                    {
                        double n = CalcAnimation(wcstod(p, NULL), style.fontSize, fAnimate);
                        style.fontSize = (n > 0) ? n : org.fontSize;
                    }
                }
                else
                {
                    style.fontSize = org.fontSize;
                }
                break;
            }
        case CMD_i:
            {
                int n = wcstol(p, NULL, 10);
                style.fItalic = !p.IsEmpty()
                                ? (n == 0 ? false : n == 1 ? true : org.fItalic)
                                    : org.fItalic;
                break;
            }
        case CMD_kt:
            {
                m_kstart = !p.IsEmpty()
                           ? wcstod(p, NULL)*10
                           : 0;
                m_kend = m_kstart;
                break;
                sub->m_fAnimated2 = true;//fix me: define m_fAnimated m_fAnimated2 strictly
            }
        case CMD_kf:
        case CMD_K:
            {
                m_ktype = 1;
                m_kstart = m_kend;
                m_kend += !p.IsEmpty()
                          ? wcstod(p, NULL)*10
                          : 1000;
                sub->m_fAnimated2 = true;//fix me: define m_fAnimated m_fAnimated2 strictly
                break;
            }
        case CMD_ko:
            {
                m_ktype = 2;
                m_kstart = m_kend;
                m_kend += !p.IsEmpty()
                          ? wcstod(p, NULL)*10
                          : 1000;
                sub->m_fAnimated2 = true;//fix me: define m_fAnimated m_fAnimated2 strictly
                break;
            }
        case CMD_k:
            {
                m_ktype = 0;
                m_kstart = m_kend;
                m_kend += !p.IsEmpty()
                          ? wcstod(p, NULL)*10
                          : 1000;
                sub->m_fAnimated2 = true;//fix me: define m_fAnimated m_fAnimated2 strictly
                break;
            }
        case CMD_move: // {\move(x1=param[0], y1=param[1], x2=param[2], y2=param[3][, t1=t[0], t2=t[1]])}
            {
                if((params.GetCount() == 4 || params.GetCount() == 6) && !sub->m_effects[EF_MOVE])
                {
                    if(Effect* e = new Effect)
                    {
                        e->param[0] = (int)(sub->m_scalex*wcstod(params[0], NULL)*8);
                        e->param[1] = (int)(sub->m_scaley*wcstod(params[1], NULL)*8);
                        e->param[2] = (int)(sub->m_scalex*wcstod(params[2], NULL)*8);
                        e->param[3] = (int)(sub->m_scaley*wcstod(params[3], NULL)*8);
                        e->t[0] = e->t[1] = -1;
                        if(params.GetCount() == 6)
                        {
                            for(int i = 0; i < 2; i++)
                                e->t[i] = wcstol(params[4+i], NULL, 10);
                        }
                        sub->m_effects[EF_MOVE] = e;
                        sub->m_fAnimated2 = true;
                    }
                }
                break;
            }
        case CMD_org: // {\org(x=param[0], y=param[1])}
            {
                if(params.GetCount() == 2 && !sub->m_effects[EF_ORG])
                {
                    if(Effect* e = new Effect)
                    {
                        e->param[0] = (int)(sub->m_scalex*wcstod(params[0], NULL)*8);
                        e->param[1] = (int)(sub->m_scaley*wcstod(params[1], NULL)*8);
                        sub->m_effects[EF_ORG] = e;
                    }
                }
                break;
            }
        case CMD_pbo:
            {
                m_polygonBaselineOffset = wcstol(p, NULL, 10);
                break;
            }
        case CMD_pos:
            {
                if(params.GetCount() == 2 && !sub->m_effects[EF_MOVE])
                {
                    if(Effect* e = new Effect)
                    {
                        e->param[0] = e->param[2] = (int)(sub->m_scalex*wcstod(params[0], NULL)*8);
                        e->param[1] = e->param[3] = (int)(sub->m_scaley*wcstod(params[1], NULL)*8);
                        e->t[0] = e->t[1] = 0;
                        sub->m_effects[EF_MOVE] = e;
                    }
                }
                break;
            }
        case CMD_p:
            {
                int n = wcstol(p, NULL, 10);
                m_nPolygon = (n <= 0 ? 0 : n);
                break;
            }
        case CMD_q:
            {
                int n = wcstol(p, NULL, 10);
                sub->m_wrapStyle = !p.IsEmpty() && (0 <= n && n <= 3)
                                   ? n
                                   : m_defaultWrapStyle;
                break;
            }
        case CMD_r:
            {
                STSStyle* val;
                style = (!p.IsEmpty() && m_styles.Lookup(WToT(p), val) && val) ? *val : org;
                break;
            }
        case CMD_shad:
            {
                double dst = wcstod(p, NULL);
                double nx = CalcAnimation(dst, style.shadowDepthX, fAnimate);
                style.shadowDepthX = !p.IsEmpty()
                                     ? (nx < 0 ? 0 : nx)
                                         : org.shadowDepthX;
                double ny = CalcAnimation(dst, style.shadowDepthY, fAnimate);
                style.shadowDepthY = !p.IsEmpty()
                                     ? (ny < 0 ? 0 : ny)
                                         : org.shadowDepthY;
                break;
            }
        case CMD_s:
            {
                int n = wcstol(p, NULL, 10);
                style.fStrikeOut = !p.IsEmpty()
                                   ? (n == 0 ? false : n == 1 ? true : org.fStrikeOut)
                                       : org.fStrikeOut;
                break;
            }
        case CMD_t: // \t([<t1>,<t2>,][<accel>,]<style modifiers>)
            {
                CStringW param;
                m_animStart = m_animEnd = 0;
                m_animAccel = 1;
                if(params.GetCount() == 1)
                {
                    param = params[0];
                }
                else if(params.GetCount() == 2)
                {
                    m_animAccel = wcstod(params[0], NULL);
                    param = params[1];
                }
                else if(params.GetCount() == 3)
                {
                    m_animStart = (int)wcstod(params[0], NULL);
                    m_animEnd = (int)wcstod(params[1], NULL);
                    param = params[2];
                }
                else if(params.GetCount() == 4)
                {
                    m_animStart = wcstol(params[0], NULL, 10);
                    m_animEnd = wcstol(params[1], NULL, 10);
                    m_animAccel = wcstod(params[2], NULL);
                    param = params[3];
                }
                ParseSSATag(sub, assTag.embeded, style, org, true);
                sub->m_fAnimated = true;
                sub->m_fAnimated2 = true;
                break;
            }
        case CMD_u:
            {
                int n = wcstol(p, NULL, 10);
                style.fUnderline = !p.IsEmpty()
                                   ? (n == 0 ? false : n == 1 ? true : org.fUnderline)
                                       : org.fUnderline;
                break;
            }
        case CMD_xbord:
            {
                double dst = wcstod(p, NULL);
                double nx = CalcAnimation(dst, style.outlineWidthX, fAnimate);
                style.outlineWidthX = !p.IsEmpty()
                                      ? (nx < 0 ? 0 : nx)
                                          : org.outlineWidthX;
                break;
            }
        case CMD_xshad:
            {
                double dst = wcstod(p, NULL);
                double nx = CalcAnimation(dst, style.shadowDepthX, fAnimate);
                style.shadowDepthX = !p.IsEmpty()
                                     ? nx
                                     : org.shadowDepthX;
                break;
            }
        case CMD_ybord:
            {
                double dst = wcstod(p, NULL);
                double ny = CalcAnimation(dst, style.outlineWidthY, fAnimate);
                style.outlineWidthY = !p.IsEmpty()
                                      ? (ny < 0 ? 0 : ny)
                                          : org.outlineWidthY;
                break;
            }
        case CMD_yshad:
            {
                double dst = wcstod(p, NULL);
                double ny = CalcAnimation(dst, style.shadowDepthY, fAnimate);
                style.shadowDepthY = !p.IsEmpty()
                                     ? ny
                                     : org.shadowDepthY;
                break;
            }
        default:
            break;
        }
    }
    return(true);
}

bool CRenderedTextSubtitle::ParseSSATag(CSubtitle* sub, const CStringW& str, STSStyle& style, const STSStyle& org, bool fAnimate)
{
    if(!sub) return(false);   

    SharedPtrConstAssTagList assTags;
    AssTagListMruCache *ass_tag_cache = CacheManager::GetAssTagListMruCache();
    POSITION pos = ass_tag_cache->Lookup(str);
    if (pos==NULL)
    {
        AssTagList *tmp = new AssTagList();
        ParseSSATag(tmp, str);
        assTags.reset(tmp);
        ass_tag_cache->UpdateCache(str, assTags);
    }
    else
    {
        assTags = ass_tag_cache->GetAt(pos);
        ass_tag_cache->UpdateCache( pos );
    }
    return ParseSSATag(sub, *assTags, style, org, fAnimate);
}

bool CRenderedTextSubtitle::ParseHtmlTag(CSubtitle* sub, CStringW str, STSStyle& style, STSStyle& org)
{
    if(str.Find(L"!--") == 0)
        return(true);
    bool fClosing = str[0] == L'/';
    str.Trim(L" /");
    int i = str.Find(L' ');
    if(i < 0) i = str.GetLength();
    CStringW tag = str.Left(i).MakeLower();
    str = str.Mid(i).Trim();
    CAtlArray<CStringW> attribs, params;
    while((i = str.Find(L'=')) > 0)
    {
        attribs.Add(str.Left(i).Trim().MakeLower());
        str = str.Mid(i+1);
        for(i = 0; _istspace(str[i]); i++);
        str = str.Mid(i);
        if(str[0] == L'\"') {str = str.Mid(1); i = str.Find(L'\"');}
        else i = str.Find(L' ');
        if(i < 0) i = str.GetLength();
        params.Add(str.Left(i).Trim().MakeLower());
        str = str.Mid(i+1);
    }
    if(tag == L"text")
        ;
    else if(tag == L"b" || tag == L"strong")
        style.fontWeight = !fClosing ? FW_BOLD : org.fontWeight;
    else if(tag == L"i" || tag == L"em")
        style.fItalic = !fClosing ? true : org.fItalic;
    else if(tag == L"u")
        style.fUnderline = !fClosing ? true : org.fUnderline;
    else if(tag == L"s" || tag == L"strike" || tag == L"del")
        style.fStrikeOut = !fClosing ? true : org.fStrikeOut;
    else if(tag == L"font")
    {
        if(!fClosing)
        {
            for(size_t i = 0; i < attribs.GetCount(); i++)
            {
                if(params[i].IsEmpty()) continue;
                int nColor = -1;
                if(attribs[i] == L"face")
                {
                    style.fontName = params[i];
                }
                else if(attribs[i] == L"size")
                {
                    if(params[i][0] == L'+')
                        style.fontSize += wcstol(params[i], NULL, 10);
                    else if(params[i][0] == L'-')
                        style.fontSize -= wcstol(params[i], NULL, 10);
                    else
                        style.fontSize = wcstol(params[i], NULL, 10);
                }
                else if(attribs[i] == L"color")
                {
                    nColor = 0;
                }
                else if(attribs[i] == L"outline-color")
                {
                    nColor = 2;
                }
                else if(attribs[i] == L"outline-level")
                {
                    style.outlineWidthX = style.outlineWidthY = wcstol(params[i], NULL, 10);
                }
                else if(attribs[i] == L"shadow-color")
                {
                    nColor = 3;
                }
                else if(attribs[i] == L"shadow-level")
                {
                    style.shadowDepthX = style.shadowDepthY = wcstol(params[i], NULL, 10);
                }
                if(nColor >= 0 && nColor < 4)
                {
                    CString key = WToT(params[i]).TrimLeft(L'#');
                    DWORD val;
                    if(g_colors.Lookup(key, val))
                        style.colors[nColor] = val;
                    else if((style.colors[nColor] = _tcstol(key, NULL, 16)) == 0)
                        style.colors[nColor] = 0x00ffffff;  // default is white
                    style.colors[nColor] = ((style.colors[nColor]>>16)&0xff)|((style.colors[nColor]&0xff)<<16)|(style.colors[nColor]&0x00ff00);
                }
            }
        }
        else
        {
            style.fontName = org.fontName;
            style.fontSize = org.fontSize;
            memcpy(style.colors, org.colors, sizeof(style.colors));
        }
    }
    else if(tag == L"k" && attribs.GetCount() == 1 && attribs[0] == L"t")
    {
        m_ktype = 1;
        m_kstart = m_kend;
        m_kend += wcstol(params[0], NULL, 10);
    }
    else
        return(false);
    return(true);
}

double CRenderedTextSubtitle::CalcAnimation(double dst, double src, bool fAnimate)
{
    int s = m_animStart ? m_animStart : 0;
    int e = m_animEnd ? m_animEnd : m_delay;
    if(fabs(dst-src) >= 0.0001 && fAnimate)
    {
        if(m_time < s) dst = src;
        else if(s <= m_time && m_time < e)
        {
            double t = pow(1.0 * (m_time - s) / (e - s), m_animAccel);
            dst = (1 - t) * src + t * dst;
        }
//      else dst = dst;
    }
    return(dst);
}

CSubtitle* CRenderedTextSubtitle::GetSubtitle(int entry)
{
    CSubtitle* sub;
    if(m_subtitleCache.Lookup(entry, sub))
    {
        if(sub->m_fAnimated) {delete sub; sub = NULL;}
        else return(sub);
    }
    sub = new CSubtitle();
    if(!sub) return(NULL);
    CStringW str = GetStrW(entry, true);
    STSStyle stss, orgstss;
    GetStyle(entry, &stss);
    if (stss.fontScaleX == stss.fontScaleY && m_dPARCompensation != 1.0)
    {
        switch(m_ePARCompensationType)
        {
        case EPCTUpscale:
            if (m_dPARCompensation < 1.0)
                stss.fontScaleY /= m_dPARCompensation;
            else
                stss.fontScaleX *= m_dPARCompensation;
            break;
        case EPCTDownscale:
            if (m_dPARCompensation < 1.0)
                stss.fontScaleX *= m_dPARCompensation;
            else
                stss.fontScaleY /= m_dPARCompensation;
            break;
        case EPCTAccurateSize:
            stss.fontScaleX *= m_dPARCompensation;
            break;
        }
    }
    orgstss = stss;
    sub->m_clip.SetRect(0, 0, m_size.cx>>3, m_size.cy>>3);
    sub->m_scrAlignment = -stss.scrAlignment;
    sub->m_wrapStyle = m_defaultWrapStyle;
    sub->m_fAnimated = false;
    sub->m_relativeTo = stss.relativeTo;
    sub->m_scalex = m_dstScreenSize.cx > 0 ? 1.0 * (stss.relativeTo == 1 ? m_vidrect.Width() : m_size.cx) / (m_dstScreenSize.cx*8) : 1.0;
    sub->m_scaley = m_dstScreenSize.cy > 0 ? 1.0 * (stss.relativeTo == 1 ? m_vidrect.Height() : m_size.cy) / (m_dstScreenSize.cy*8) : 1.0;
    m_animStart = m_animEnd = 0;
    m_animAccel = 1;
    m_ktype = m_kstart = m_kend = 0;
    m_nPolygon = 0;
    m_polygonBaselineOffset = 0;
    ParseEffect(sub, m_entries.GetAt(entry).effect);
    while(!str.IsEmpty())
    {
        bool fParsed = false;
        int i;
        if(str[0] == L'{' && (i = str.Find(L'}')) > 0)
        {
            if(fParsed = ParseSSATag(sub, str.Mid(1, i-1), stss, orgstss))
                str = str.Mid(i+1);
        }
        else if(str[0] == L'<' && (i = str.Find(L'>')) > 0)
        {
            if(fParsed = ParseHtmlTag(sub, str.Mid(1, i-1), stss, orgstss))
                str = str.Mid(i+1);
        }
        if(fParsed)
        {
            i = str.FindOneOf(L"{<");
            if(i < 0) i = str.GetLength();
            if(i == 0) continue;
        }
        else
        {
            i = str.Mid(1).FindOneOf(L"{<");
            if(i < 0) i = str.GetLength()-1;
            i++;
        }
        STSStyle tmp = stss;
        tmp.fontSize = sub->m_scaley*tmp.fontSize*64;
        tmp.fontSpacing = sub->m_scalex*tmp.fontSpacing*64;
        tmp.outlineWidthX *= (m_fScaledBAS ? sub->m_scalex : 1) * 8;
        tmp.outlineWidthY *= (m_fScaledBAS ? sub->m_scaley : 1) * 8;
        tmp.shadowDepthX *= (m_fScaledBAS ? sub->m_scalex : 1) * 8;
        tmp.shadowDepthY *= (m_fScaledBAS ? sub->m_scaley : 1) * 8;
        FwSTSStyle fw_tmp(tmp);
        if(m_nPolygon)
        {
            ParsePolygon(sub, str.Left(i), fw_tmp);
        }
        else
        {
            ParseString(sub, str.Left(i), fw_tmp);
        }
        str = str.Mid(i);
    }    
    if( sub->m_effects[EF_BANNER] || sub->m_effects[EF_SCROLL] )
        sub->m_fAnimated2 = true;
    // just a "work-around" solution... in most cases nobody will want to use \org together with moving but without rotating the subs
    if(sub->m_effects[EF_ORG] && (sub->m_effects[EF_MOVE] || sub->m_effects[EF_BANNER] || sub->m_effects[EF_SCROLL]))
        sub->m_fAnimated = true;
    sub->m_scrAlignment = abs(sub->m_scrAlignment);
    STSEntry stse = m_entries.GetAt(entry);
    CRect marginRect = stse.marginRect;
    if(marginRect.left == 0) marginRect.left = orgstss.marginRect.get().left;
    if(marginRect.top == 0) marginRect.top = orgstss.marginRect.get().top;
    if(marginRect.right == 0) marginRect.right = orgstss.marginRect.get().right;
    if(marginRect.bottom == 0) marginRect.bottom = orgstss.marginRect.get().bottom;
    marginRect.left = (int)(sub->m_scalex*marginRect.left*8);
    marginRect.top = (int)(sub->m_scaley*marginRect.top*8);
    marginRect.right = (int)(sub->m_scalex*marginRect.right*8);
    marginRect.bottom = (int)(sub->m_scaley*marginRect.bottom*8);
    if(stss.relativeTo == 1)
    {
        marginRect.left += m_vidrect.left;
        marginRect.top += m_vidrect.top;
        marginRect.right += m_size.cx - m_vidrect.right;
        marginRect.bottom += m_size.cy - m_vidrect.bottom;
    }
    sub->CreateClippers(m_size);
    sub->MakeLines(m_size, marginRect);
    m_subtitleCache[entry] = sub;
    return(sub);
}

//

STDMETHODIMP CRenderedTextSubtitle::NonDelegatingQueryInterface(REFIID riid, void** ppv)
{
    CheckPointer(ppv, E_POINTER);
    *ppv = NULL;
    return
        QI(IPersist)
        QI(ISubStream)
        QI(ISubPicProvider)
        QI(ISubPicProviderEx)
        __super::NonDelegatingQueryInterface(riid, ppv);
}

// ISubPicProvider

STDMETHODIMP_(POSITION) CRenderedTextSubtitle::GetStartPosition(REFERENCE_TIME rt, double fps)
{    
    m_fps = fps;
    if (m_fps>0)
    {
        m_period = 1000/m_fps;
        if(m_period<=0)
        {
            m_period = 1;
        }
    }
    else
    {
        //Todo: fix me. max has been defined as a macro. Use #define NOMINMAX to fix it.
        //std::numeric_limits<int>::max(); 
        m_period = INT_MAX;
    }

    int iSegment;
    int subIndex = 1;//If a segment has animate effect then it corresponds to several subpics.
    //subIndex, 1 based, indicates which subpic the result corresponds to.
    rt /= 10000i64;
    const STSSegment *stss = SearchSubs((int)rt, fps, &iSegment, NULL);
    if(stss==NULL)
        return NULL;
    else if(stss->animated)
    {
        int start = TranslateSegmentStart(iSegment, fps);
        if(rt > start)
            subIndex = (rt-start)/m_period + 1;
    }
    //DbgLog((LOG_TRACE, 3, "animated:%d seg:%d idx:%d DUR:%d rt:%lu", stss->animated, iSegment, subIndex, RTS_ANIMATE_SUBPIC_DUR, (ULONG)rt/10000));
    return (POSITION)(subIndex | (iSegment<<RTS_POS_SEGMENT_INDEX_BITS));
    //if(iSegment < 0) iSegment = 0;
    //return(GetNext((POSITION)iSegment));
}

STDMETHODIMP_(POSITION) CRenderedTextSubtitle::GetNext(POSITION pos)
{
    int iSegment = ((int)pos>>RTS_POS_SEGMENT_INDEX_BITS);
    int subIndex = ((int)pos & RTS_POS_SUB_INDEX_MASK);
    const STSSegment *stss = GetSegment(iSegment);
    ASSERT(stss!=NULL && stss->subs.GetCount()>0);
    //DbgLog((LOG_TRACE, 3, "stss:%x count:%d", stss, stss->subs.GetCount()));
    if(!stss->animated)
    {
        iSegment++;
        subIndex = 1;
    }
    else
    {
        int start, end;
        TranslateSegmentStartEnd(iSegment, m_fps, start, end);
        if(start+m_period*subIndex < end)
            subIndex++;
        else
        {
            iSegment++;
            subIndex = 1;
        }
    }
    if(GetSegment(iSegment) != NULL)
    {
        ASSERT(GetSegment(iSegment)->subs.GetCount()>0);
        return (POSITION)(subIndex | (iSegment<<RTS_POS_SEGMENT_INDEX_BITS));
    }
    else
        return NULL;
}

//@return: <0 if segment not found
STDMETHODIMP_(REFERENCE_TIME) CRenderedTextSubtitle::GetStart(POSITION pos, double fps)
{
    //return(10000i64 * TranslateSegmentStart((int)pos-1, fps));
    int iSegment = ((int)pos>>RTS_POS_SEGMENT_INDEX_BITS);
    int subIndex = ((int)pos & RTS_POS_SUB_INDEX_MASK);
    int start = TranslateSegmentStart(iSegment, fps);
    const STSSegment *stss = GetSegment(iSegment);
    if(stss!=NULL)
    {
        return (start + (subIndex-1)*m_period)*10000i64;
    }
    else
    {
        return -1;
    }
}

//@return: <0 if segment not found
STDMETHODIMP_(REFERENCE_TIME) CRenderedTextSubtitle::GetStop(POSITION pos, double fps)
{
//  return(10000i64 * TranslateSegmentEnd((int)pos-1, fps));
    int iSegment = ((int)pos>>RTS_POS_SEGMENT_INDEX_BITS);
    int subIndex = ((int)pos & RTS_POS_SUB_INDEX_MASK);
    int start, end, ret;
    TranslateSegmentStartEnd(iSegment, fps, start, end);
    const STSSegment *stss = GetSegment(iSegment);
    if(stss!=NULL)
    {
        if(!stss->animated)
            ret = end;
        else
        {
            ret = start+subIndex*m_period;
            if(ret > end)
                ret = end;
        }
        return ret*10000i64;
    }
    else
        return -1;
}

//@start, @stop: -1 if segment not found; @stop may < @start if subIndex exceed uppper bound
STDMETHODIMP_(VOID) CRenderedTextSubtitle::GetStartStop(POSITION pos, double fps, /*out*/REFERENCE_TIME &start, /*out*/REFERENCE_TIME &stop)
{
    int iSegment = ((int)pos>>RTS_POS_SEGMENT_INDEX_BITS);
    int subIndex = ((int)pos & RTS_POS_SUB_INDEX_MASK);
    int tempStart, tempEnd;
    TranslateSegmentStartEnd(iSegment, fps, tempStart, tempEnd);
    start = tempStart;
    stop = tempEnd;
    const STSSegment *stss = GetSegment(iSegment);
    if(stss!=NULL)
    {
        if(stss->animated)
        {
            start += (subIndex-1)*m_period;
            if(start+m_period < stop)
                stop = start+m_period;
        }
        //DbgLog((LOG_TRACE, 3, "animated:%d seg:%d idx:%d start:%d stop:%lu", stss->animated, iSegment, subIndex, (ULONG)start, (ULONG)stop));
        start *= 10000i64;
        stop *= 10000i64;
    }
    else
    {
        start = -1;
        stop = -1;
    }
}

STDMETHODIMP_(bool) CRenderedTextSubtitle::IsAnimated(POSITION pos)
{
    int iSegment = ((int)pos>>RTS_POS_SEGMENT_INDEX_BITS);
    if(iSegment>=0 && iSegment<m_segments.GetCount())
        return m_segments[iSegment].animated;
    else
        return false;
    //return(true);
}

struct LSub {int idx, layer, readorder;};

static int lscomp(const void* ls1, const void* ls2)
{
    int ret = ((LSub*)ls1)->layer - ((LSub*)ls2)->layer;
    if(!ret) ret = ((LSub*)ls1)->readorder - ((LSub*)ls2)->readorder;
    return(ret);
}

STDMETHODIMP CRenderedTextSubtitle::ParseScript(SubPicDesc& spd, REFERENCE_TIME rt, double fps, CSubtitle2List *outputSub2List )
{
    //fix me: check input and log error
    int t = (int)(rt / 10000);
    int segment;
    //const
    STSSegment* stss = SearchSubs2(t, fps, &segment);
    if(!stss) return S_FALSE;
    // clear any cached subs not in the range of +/-30secs measured from the segment's bounds
    {
        POSITION pos = m_subtitleCache.GetStartPosition();
        while(pos)
        {
            int key;
            CSubtitle* value;
            m_subtitleCache.GetNextAssoc(pos, key, value);
            STSEntry& stse = m_entries.GetAt(key);
            if(stse.end <= (t-30000) || stse.start > (t+30000))
            {
                delete value;
                m_subtitleCache.RemoveKey(key);
                pos = m_subtitleCache.GetStartPosition();
            }
        }
    }
    m_sla.AdvanceToSegment(segment, stss->subs);
    CAtlArray<LSub> subs;
    for(int i = 0, j = stss->subs.GetCount(); i < j; i++)
    {
        LSub ls;
        ls.idx = stss->subs[i];
        ls.layer = m_entries.GetAt(stss->subs[i]).layer;
        ls.readorder = m_entries.GetAt(stss->subs[i]).readorder;
        subs.Add(ls);
    }
    qsort(subs.GetData(), subs.GetCount(), sizeof(LSub), lscomp);
        
    for(int i = 0, j = subs.GetCount(); i < j; i++)
    {
        int entry = subs[i].idx;
        STSEntry stse = m_entries.GetAt(entry);
        {
            int start = TranslateStart(entry, fps);
            m_time = t - start;
            m_delay = TranslateEnd(entry, fps) - start;
        }        
        CSubtitle* s = GetSubtitle(entry);
        if(!s) continue;
        stss->animated |= s->m_fAnimated2;
        CRect clipRect = s->m_clip & CRect(0,0, spd.w, spd.h);
        CRect r = s->m_rect;
        CSize spaceNeeded = r.Size();
        // apply the effects
        bool fPosOverride = false, fOrgOverride = false;
        int alpha = 0x00;
        CPoint org2;
        for(int k = 0; k < EF_NUMBEROFEFFECTS; k++)
        {
            if(!s->m_effects[k]) continue;
            switch(k)
            {
            case EF_MOVE: // {\move(x1=param[0], y1=param[1], x2=param[2], y2=param[3], t1=t[0], t2=t[1])}
                {
                    CPoint p;
                    CPoint p1(s->m_effects[k]->param[0], s->m_effects[k]->param[1]);
                    CPoint p2(s->m_effects[k]->param[2], s->m_effects[k]->param[3]);
                    int t1 = s->m_effects[k]->t[0];
                    int t2 = s->m_effects[k]->t[1];
                    if(t2 < t1) {int t = t1; t1 = t2; t2 = t;}
                    if(t1 <= 0 && t2 <= 0) {t1 = 0; t2 = m_delay;}
                    if(m_time <= t1) p = p1;
                    else if (p1 == p2) p = p1;
                    else if(t1 < m_time && m_time < t2)
                    {
                        double t = 1.0*(m_time-t1)/(t2-t1);
                        p.x = (int)((1-t)*p1.x + t*p2.x);
                        p.y = (int)((1-t)*p1.y + t*p2.y);
                    }
                    else p = p2;
                    r = CRect(
                        CPoint((s->m_scrAlignment%3) == 1 ? p.x : (s->m_scrAlignment%3) == 0 ? p.x - spaceNeeded.cx : p.x - (spaceNeeded.cx+1)/2,
                        s->m_scrAlignment <= 3 ? p.y - spaceNeeded.cy : s->m_scrAlignment <= 6 ? p.y - (spaceNeeded.cy+1)/2 : p.y),
                        spaceNeeded);
                    if(s->m_relativeTo == 1)
                        r.OffsetRect(m_vidrect.TopLeft());
                    fPosOverride = true;
                }
                break;
            case EF_ORG: // {\org(x=param[0], y=param[1])}
                {
                    org2 = CPoint(s->m_effects[k]->param[0], s->m_effects[k]->param[1]);
                    fOrgOverride = true;
                }
                break;
            case EF_FADE: // {\fade(a1=param[0], a2=param[1], a3=param[2], t1=t[0], t2=t[1], t3=t[2], t4=t[3]) or {\fad(t1=t[1], t2=t[2])
                {
                    int t1 = s->m_effects[k]->t[0];
                    int t2 = s->m_effects[k]->t[1];
                    int t3 = s->m_effects[k]->t[2];
                    int t4 = s->m_effects[k]->t[3];
                    if(t1 == -1 && t4 == -1) {t1 = 0; t3 = m_delay-t3; t4 = m_delay;}
                    if(m_time < t1) alpha = s->m_effects[k]->param[0];
                    else if(m_time >= t1 && m_time < t2)
                    {
                        double t = 1.0 * (m_time - t1) / (t2 - t1);
                        alpha = (int)(s->m_effects[k]->param[0]*(1-t) + s->m_effects[k]->param[1]*t);
                    }
                    else if(m_time >= t2 && m_time < t3) alpha = s->m_effects[k]->param[1];
                    else if(m_time >= t3 && m_time < t4)
                    {
                        double t = 1.0 * (m_time - t3) / (t4 - t3);
                        alpha = (int)(s->m_effects[k]->param[1]*(1-t) + s->m_effects[k]->param[2]*t);
                    }
                    else if(m_time >= t4) alpha = s->m_effects[k]->param[2];
                }
                break;
            case EF_BANNER: // Banner;delay=param[0][;leftoright=param[1];fadeawaywidth=param[2]]
                {
                    int left = s->m_relativeTo == 1 ? m_vidrect.left : 0,
                        right = s->m_relativeTo == 1 ? m_vidrect.right : m_size.cx;
                    r.left = !!s->m_effects[k]->param[1]
                    ? (left/*marginRect.left*/ - spaceNeeded.cx) + (int)(m_time*8.0/s->m_effects[k]->param[0])
                        : (right /*- marginRect.right*/) - (int)(m_time*8.0/s->m_effects[k]->param[0]);
                    r.right = r.left + spaceNeeded.cx;
                    clipRect &= CRect(left>>3, clipRect.top, right>>3, clipRect.bottom);
                    fPosOverride = true;
                }
                break;
            case EF_SCROLL: // Scroll up/down(toptobottom=param[3]);top=param[0];bottom=param[1];delay=param[2][;fadeawayheight=param[4]]
                {
                    r.top = !!s->m_effects[k]->param[3]
                    ? s->m_effects[k]->param[0] + (int)(m_time*8.0/s->m_effects[k]->param[2]) - spaceNeeded.cy
                        : s->m_effects[k]->param[1] - (int)(m_time*8.0/s->m_effects[k]->param[2]);
                    r.bottom = r.top + spaceNeeded.cy;
                    CRect cr(0, (s->m_effects[k]->param[0] + 4) >> 3, spd.w, (s->m_effects[k]->param[1] + 4) >> 3);
                    if(s->m_relativeTo == 1)
                        r.top += m_vidrect.top,
                        r.bottom += m_vidrect.top,
                        cr.top += m_vidrect.top>>3,
                        cr.bottom += m_vidrect.top>>3;
                    clipRect &= cr;
                    fPosOverride = true;
                }
                break;
            default:
                break;
            }
        }
        if(!fPosOverride && !fOrgOverride && !s->m_fAnimated)
            r = m_sla.AllocRect(s, segment, entry, stse.layer, m_collisions);
        CPoint org;
        org.x = (s->m_scrAlignment%3) == 1 ? r.left : (s->m_scrAlignment%3) == 2 ? r.CenterPoint().x : r.right;
        org.y = s->m_scrAlignment <= 3 ? r.bottom : s->m_scrAlignment <= 6 ? r.CenterPoint().y : r.top;
        if(!fOrgOverride) org2 = org;
        CPoint p2(0, r.top);
        // Rectangles for inverse clip

        CSubtitle2& sub2 = outputSub2List->GetAt(outputSub2List->AddTail( CSubtitle2(s, clipRect, org, org2, p2, alpha, m_time) ));
    }

    return (subs.GetCount()) ? S_OK : S_FALSE;
}

STDMETHODIMP CRenderedTextSubtitle::RenderEx(SubPicDesc& spd, REFERENCE_TIME rt, double fps, CAtlList<CRect>& rectList)
{
    if(m_size != CSize(spd.w*8, spd.h*8) || m_vidrect != CRect(spd.vidrect.left*8, spd.vidrect.top*8, spd.vidrect.right*8, spd.vidrect.bottom*8))
        Init(CSize(spd.w, spd.h), spd.vidrect);

    CSubtitle2List sub2List;
    HRESULT hr = ParseScript(spd, rt, fps, &sub2List);
    if(hr!=S_OK)
    {
        return hr;
    }

    CompositeDrawItemListList compDrawItemListList;   
    DoRender(spd, sub2List, &rectList, &compDrawItemListList);
    CompositeDrawItem::Draw(spd, compDrawItemListList);
    return (!rectList.IsEmpty()) ? S_OK : S_FALSE;
}

void CRenderedTextSubtitle::DoRender( SubPicDesc& spd, const CSubtitle2List& sub2List, 
    CAtlList<CRect> *rectList, CompositeDrawItemListList *compDrawItemListList /*output*/)
{
    //check input and log error
    POSITION pos=sub2List.GetHeadPosition();
    while ( pos!=NULL )
    {
        const CSubtitle2& sub2 = sub2List.GetNext(pos);
        CompositeDrawItemList& compDrawItemList = compDrawItemListList->GetAt(compDrawItemListList->AddTail());
        RenderOneSubtitle(spd, sub2, rectList, &compDrawItemList);
    }
}

void CRenderedTextSubtitle::RenderOneSubtitle( SubPicDesc& spd, const CSubtitle2& sub2, 
    CAtlList<CRect>* rectList, CompositeDrawItemList* compDrawItemList /*output*/)
{   
    CSubtitle* s = sub2.s;
    const CRect& clipRect = sub2.clipRect;
    const CPoint& org = sub2.org;
    const CPoint& org2 = sub2.org2;
    const CPoint& p2 = sub2.p;
    int alpha = sub2.alpha;
    int time = sub2.time;
    if(!s) return;

    const SharedPtrCClipper& clipper = s->m_pClipper;
    CRect iclipRect[4];
    iclipRect[0] = CRect(0, 0, spd.w, clipRect.top);
    iclipRect[1] = CRect(0, clipRect.top, clipRect.left, clipRect.bottom);
    iclipRect[2] = CRect(clipRect.right, clipRect.top, spd.w, clipRect.bottom);
    iclipRect[3] = CRect(0, clipRect.bottom, spd.w, spd.h);        
    CRect bbox2(0,0,0,0);
    POSITION pos = s->GetHeadLinePosition();       
    CPoint p = p2;
    while(pos)
    {
        CLine* l = s->GetNextLine(pos);
        p.x = (s->m_scrAlignment%3) == 1 ? org.x
            : (s->m_scrAlignment%3) == 0 ? org.x - l->m_width
            :                            org.x - (l->m_width/2);

        CompositeDrawItemList tmpCompDrawItemList;
        if (s->m_clipInverse)
        {
            CompositeDrawItemList tmp1,tmp2,tmp3,tmp4;              
            for (int i=0;i<l->GetWordCount();i++)
            {
                tmp1.AddTail();
                tmp2.AddTail();
                tmp3.AddTail();
                tmp4.AddTail();
            }                
            bbox2 |= l->PaintAll(&tmp1, spd, iclipRect[0], clipper, p, org2, time, alpha);
            bbox2 |= l->PaintAll(&tmp2, spd, iclipRect[1], clipper, p, org2, time, alpha);
            bbox2 |= l->PaintAll(&tmp3, spd, iclipRect[2], clipper, p, org2, time, alpha);
            bbox2 |= l->PaintAll(&tmp4, spd, iclipRect[3], clipper, p, org2, time, alpha);
            tmpCompDrawItemList.AddTailList(&tmp1);
            tmpCompDrawItemList.AddTailList(&tmp2);
            tmpCompDrawItemList.AddTailList(&tmp3);
            tmpCompDrawItemList.AddTailList(&tmp4);
        }
        else
        {
            for (int i=0;i<l->GetWordCount();i++)
            {
                tmpCompDrawItemList.AddTail();
            }
            bbox2 |= l->PaintAll(&tmpCompDrawItemList, spd, clipRect, clipper, p, org2, time, alpha);
        }
        compDrawItemList->AddTailList(&tmpCompDrawItemList);
        p.y += l->m_ascent + l->m_descent;
    }
    rectList->AddTail(bbox2);
}

STDMETHODIMP CRenderedTextSubtitle::Render(SubPicDesc& spd, REFERENCE_TIME rt, double fps, RECT& bbox)
{
    CAtlList<CRect> rectList;
    HRESULT result = RenderEx(spd, rt, fps, rectList);
    POSITION pos = rectList.GetHeadPosition();
    CRect bbox2(0,0,0,0);
    while(pos!=NULL)
    {
        bbox2 |= rectList.GetNext(pos);
    }
    bbox = bbox2;
    return result;
}

// IPersist

STDMETHODIMP CRenderedTextSubtitle::GetClassID(CLSID* pClassID)
{
    return pClassID ? *pClassID = __uuidof(this), S_OK : E_POINTER;
}

// ISubStream

STDMETHODIMP_(int) CRenderedTextSubtitle::GetStreamCount()
{
    return(1);
}

STDMETHODIMP CRenderedTextSubtitle::GetStreamInfo(int iStream, WCHAR** ppName, LCID* pLCID)
{
    if(iStream != 0) return E_INVALIDARG;
    if(ppName)
    {
        if(!(*ppName = (WCHAR*)CoTaskMemAlloc((m_name.GetLength()+1)*sizeof(WCHAR))))
            return E_OUTOFMEMORY;
        wcscpy(*ppName, CStringW(m_name));
    }
    if(pLCID)
    {
        *pLCID = 0; // TODO
    }
    return S_OK;
}

STDMETHODIMP_(int) CRenderedTextSubtitle::GetStream()
{
    return(0);
}

STDMETHODIMP CRenderedTextSubtitle::SetStream(int iStream)
{
    return iStream == 0 ? S_OK : E_FAIL;
}

STDMETHODIMP CRenderedTextSubtitle::Reload()
{
    CFileStatus s;
    if(!CFile::GetStatus(m_path, s)) return E_FAIL;
    return !m_path.IsEmpty() && Open(m_path, DEFAULT_CHARSET) ? S_OK : E_FAIL;
}

STDMETHODIMP_(bool) CRenderedTextSubtitle::IsColorTypeSupported( int type )
{
    return type==MSP_AYUV_PLANAR ||
           type==MSP_AYUV ||
           type==MSP_XY_AUYV ||
           type==MSP_RGBA;
}
