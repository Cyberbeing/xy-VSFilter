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
#include "subpixel_position_controler.h"
#include "xy_overlay_paint_machine.h"
#include "xy_clipper_paint_machine.h"

#if ENABLE_XY_LOG_TEXT_PARSER
#  define TRACE_PARSER(msg) XY_LOG_TRACE(msg)
#else
#  define TRACE_PARSER(msg)
#endif

#if ENABLE_XY_LOG_RENDERER_REQUEST2
#  define TRACE_RENDERER_REQUEST(msg) XY_LOG_TRACE(msg)
#  define TRACE_RENDERER_REQUEST_TIMING(msg) XY_AUTO_TIMING(msg)
#else
#  define TRACE_RENDERER_REQUEST(msg)
#  define TRACE_RENDERER_REQUEST_TIMING(msg)
#endif

const int MAX_SUB_PIXEL = 8;
const double MAX_SUB_PIXEL_F = 8.0;

// WARNING: this isn't very thread safe, use only one RTS a time.
static HDC g_hDC;
static int g_hDC_refcnt = 0;

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
    ZeroMemory(&lf, sizeof(lf));
    lf <<= style;
    lf.lfHeight         = (LONG)(style.fontSize+0.5);
    lf.lfOutPrecision   = OUT_TT_PRECIS;
    lf.lfClipPrecision  = CLIP_DEFAULT_PRECIS;
    lf.lfQuality        = ANTIALIASED_QUALITY;
    lf.lfPitchAndFamily = DEFAULT_PITCH|FF_DONTCARE;
    if(!CreateFontIndirect(&lf))
    {
        _tcscpy(lf.lfFaceName, _T("Arial"));
        VERIFY(CreateFontIndirect(&lf));
    }
    HFONT hOldFont = SelectFont(g_hDC, *this);
    TEXTMETRIC tm;
    GetTextMetrics(g_hDC, &tm);
    m_ascent  = ((tm.tmAscent  + 4) >> 3);
    m_descent = ((tm.tmDescent + 4) >> 3);
    SelectFont(g_hDC, hOldFont);
}

// CWord

CWord::CWord( const FwSTSStyle& style, const CStringW& str, int ktype, int kstart, int kend
    , double target_scale_x/*=1.0*/, double target_scale_y/*=1.0*/
    , bool round_to_whole_pixel_after_scale_to_target/*=false*/)
    : m_style(style), m_str(DEBUG_NEW CStringW(str))
    , m_width(0), m_ascent(0), m_descent(0)
    , m_ktype(ktype), m_kstart(kstart), m_kend(kend)
    , m_fLineBreak(false), m_fWhiteSpaceChar(false)
    , m_target_scale_x(target_scale_x), m_target_scale_y(target_scale_y)
    , m_round_to_whole_pixel_after_scale_to_target(round_to_whole_pixel_after_scale_to_target)
    //, m_pOpaqueBox(NULL)
{
    if(m_str.Get().IsEmpty())
    {
        m_fWhiteSpaceChar = m_fLineBreak = true;
    }
    m_width = 0;
}

CWord::CWord( const CWord& src):m_str(src.m_str)
{
    m_fWhiteSpaceChar                            = src.m_fWhiteSpaceChar;
    m_fLineBreak                                 = src.m_fLineBreak;
    m_style                                      = src.m_style;
    m_pOpaqueBox                                 = src.m_pOpaqueBox;//allow since it is shared_ptr
    m_ktype                                      = src.m_ktype;
    m_kstart                                     = src.m_kstart;
    m_kend                                       = src.m_kend;
    m_width                                      = src.m_width;
    m_ascent                                     = src.m_ascent;
    m_descent                                    = src.m_descent;
    m_target_scale_x                             = src.m_target_scale_x;
    m_target_scale_y                             = src.m_target_scale_y;
    m_round_to_whole_pixel_after_scale_to_target = src.m_round_to_whole_pixel_after_scale_to_target;
}

CWord::~CWord()
{
    //if(m_pOpaqueBox) delete m_pOpaqueBox;
}

bool CWord::Append(const SharedPtrCWord& w)
{
    if (m_style != w->m_style ||
          m_fLineBreak || w->m_fLineBreak || 
          w->m_kstart != w->m_kend || m_ktype != w->m_ktype)
          return(false);
    m_fWhiteSpaceChar = m_fWhiteSpaceChar && w->m_fWhiteSpaceChar;
    CStringW *str = DEBUG_NEW CStringW();//Fix me: anyway to avoid this flyweight update?
    ASSERT(str);
    *str  =    m_str.Get();
    *str += w->m_str.Get();
    m_str = XyFwStringW(str);
    m_width += w->m_width;
    return(true);
}

void CWord::PaintFromOverlay(const CPointCoor2& p, const CPointCoor2& trans_org2, OverlayKey &subpixel_variance_key, SharedPtrOverlay& overlay)
{
    if( SubpixelPositionControler::GetGlobalControler().UseBilinearShift() )
    {
        CPoint psub = SubpixelPositionControler::GetGlobalControler().GetSubpixel(p);
        if( (psub.x!=(p.x&SubpixelPositionControler::EIGHT_X_EIGHT_MASK) || 
             psub.y!=(p.y&SubpixelPositionControler::EIGHT_X_EIGHT_MASK)) )
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
    if( Rasterizer::IsItReallyBlur(m_style.get().fBlur, m_style.get().fGaussianBlur) )
    {
        overlay->reset(DEBUG_NEW Overlay());
        if (!Rasterizer::Blur(
            *raterize_result, 
            m_style.get().fBlur, 
            m_style.get().fGaussianBlur, 
            m_target_scale_x, 
            m_target_scale_y, 
            *overlay))
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

bool CWord::PaintFromScanLineData2(const CPointCoor2& psub, const ScanLineData2& scan_line_data2, const OverlayKey& key, SharedPtrOverlay* overlay)
{
    SharedPtrOverlay raterize_result(DEBUG_NEW Overlay());
    if(!Rasterizer::Rasterize(scan_line_data2, psub.x, psub.y, raterize_result)) 
    {     
        return false;
    }
    OverlayNoBlurMruCache* overlay_no_blur_cache = CacheManager::GetOverlayNoBlurMruCache();
    overlay_no_blur_cache->UpdateCache(key, raterize_result);
    PaintFromNoneBluredOverlay(raterize_result, key, overlay);
    return true;
}

bool CWord::PaintFromPathData(const CPointCoor2& psub, const CPointCoor2& trans_org, const PathData& path_data, const OverlayKey& key, SharedPtrOverlay* overlay )
{
    bool result = false;

    PathData *path_data2 = DEBUG_NEW PathData(path_data);//fix me: this copy operation can be saved if no transform is needed
    SharedPtrConstPathData shared_ptr_path_data2(path_data2);
    bool need_transform = NeedTransform();
    if(need_transform)
        Transform(path_data2, CPoint(trans_org.x*MAX_SUB_PIXEL, trans_org.y*MAX_SUB_PIXEL));

    CPoint left_top;
    CSize  size;
    path_data2->AlignLeftTop(&left_top, &size);

    int border_x = static_cast<int>(m_style.get().outlineWidthX*m_target_scale_x+0.5);//fix me: rounding err
    int border_y = static_cast<int>(m_style.get().outlineWidthY*m_target_scale_y+0.5);//fix me: rounding err
    int wide_border = border_x>border_y ? border_x:border_y;
    if (m_style.get().borderStyle==1)
    {
        border_x = border_y = 0;
    }

    OverlayNoOffsetMruCache* overlay_key_cache = CacheManager::GetOverlayNoOffsetMruCache();
    OverlayNoOffsetKey overlay_no_offset_key(shared_ptr_path_data2, psub.x, psub.y, border_x, border_y);
    overlay_no_offset_key.UpdateHashValue();
    POSITION pos_key = overlay_key_cache->Lookup(overlay_no_offset_key);
    POSITION pos = NULL;
        
    OverlayNoBlurMruCache* overlay_cache = CacheManager::GetOverlayNoBlurMruCache();
    if (pos_key!=NULL)
    {
        OverlayNoBlurKey overlay_key = overlay_key_cache->GetAt(pos_key);
        pos = overlay_cache->Lookup(overlay_key);
    }
    if (pos)
    {
        SharedPtrOverlay raterize_result( DEBUG_NEW Overlay() );
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
            ScanLineData *tmp = DEBUG_NEW ScanLineData();
            scan_line_data.reset(tmp);
            if(!tmp->ScanConvert(*path_data2, size))
            {
                return false;
            }
            scan_line_data_cache->UpdateCache(overlay_no_offset_key, scan_line_data);
        }
        ScanLineData2 *tmp = DEBUG_NEW ScanLineData2(left_top, scan_line_data);
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
        if (pos_key!=NULL)
        {
            overlay_key_cache->UpdateCache(pos_key, key);
        }
        else
        {
            overlay_key_cache->UpdateCache(overlay_no_offset_key, key);
        }
    }
    return result;
}

bool CWord::PaintFromRawData( const CPointCoor2& psub, const CPointCoor2& trans_org, const OverlayKey& key, SharedPtrOverlay* overlay )
{
    PathDataMruCache* path_data_cache = CacheManager::GetPathDataMruCache();

    PathData *tmp=DEBUG_NEW PathData();
    SharedPtrPathData path_data(tmp);
    if(!CreatePath(tmp))
    {
        return false;
    }
    path_data_cache->UpdateCache(key, path_data);
    return PaintFromPathData(psub, trans_org, *tmp, key, overlay);
}

bool CWord::DoPaint(const CPointCoor2& psub, const CPointCoor2& trans_org, SharedPtrOverlay* overlay, const OverlayKey& key)
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
           (fabs(m_style.get().fontShiftY) > 0.000001) ||
           (fabs(m_target_scale_x-1.0) > 0.000001) ||
           (fabs(m_target_scale_y-1.0) > 0.000001);
}

//void CWord::Transform(PathData* path_data, const CPointCoor2& org)
//{
//    //// CPUID from VDub
//    //bool fSSE2 = !!(g_cpuid.m_flags & CCpuID::sse2);
//
//    //if(fSSE2) {	// SSE code
//    //	Transform_SSE2(path_data, org);
//    //} else		// C-code
//          Transform_C(path_data, org);
//}

void CWord::Transform(PathData* path_data, const CPointCoor2 &org )
{
    ASSERT(path_data);
    const STSStyle& style = m_style.get();

    const double scalex = style.fontScaleX/100.0;
    const double scaley = style.fontScaleY/100.0;

    const double caz = cos((M_PI/180.0)*style.fontAngleZ);
    const double saz = sin((M_PI/180.0)*style.fontAngleZ);
    const double cax = cos((M_PI/180.0)*style.fontAngleX);
    const double sax = sin((M_PI/180.0)*style.fontAngleX);
    const double cay = cos((M_PI/180.0)*style.fontAngleY);
    const double say = sin((M_PI/180.0)*style.fontAngleY);

    double xxx[3][3];
    /******************
          targetScaleX            0    0
     S0 =            0 targetScaleY    0
                     0            0    1
    /******************
          20000     0    0
     A0 =     0 20000    0
              0     0    1
    /******************
          cay    0  say
     A1 =   0    1    0
          say    0 -cay
    /******************
            1    0    0
     A2 =   0  cax  sax
            0  sax -cax
    /******************
          caz  saz    0
     A3 =-saz  caz    0
            0    0    1
    /******************
          scalex            scalex*fontShiftX -org.x/targetScaleX
     A4 = scaley*fontShiftY scaley            -org.y/targetScaleY
          0                 0                  0
    /******************
              0     0      0
     B0 =     0     0      0
              0     0  20000
    /******************
     Formula:
       (x,y,z)' = (S0*A0*A1*A2*A3*A4 + B0) * (x y 1)'
       z = max(1000,z)
       x = x/z + org.x
       y = y/z + org.y
    *******************/

    //A3*A4
    ASSERT(m_target_scale_x!=0 && m_target_scale_y!=0);
    double tmp1 = -org.x/m_target_scale_x;
    double tmp2 = -org.y/m_target_scale_y;

    xxx[0][0] = caz*scalex + saz*scaley*style.fontShiftY;
    xxx[0][1] = caz*scalex*style.fontShiftX + saz*scaley;
    xxx[0][2] = caz*tmp1 + saz*tmp2;

    xxx[1][0] = -saz*scalex + caz*scaley*style.fontShiftY;
    xxx[1][1] = -saz*scalex*style.fontShiftX + caz*scaley;
    xxx[1][2] = -saz*tmp1 + caz*tmp2;

    xxx[2][0] = 
    xxx[2][0] = 
    xxx[2][0] = 0;
    
    //A2*A3*A4

    xxx[2][0] = sax*xxx[1][0];
    xxx[2][1] = sax*xxx[1][1];
    xxx[2][2] = sax*xxx[1][2];

    xxx[1][0] = cax*xxx[1][0];
    xxx[1][1] = cax*xxx[1][1];
    xxx[1][2] = cax*xxx[1][2];

    //A1*A2*A3*A4

    tmp1 = xxx[0][0];
    tmp2 = xxx[0][1];
    double tmp3 = xxx[0][2];
    xxx[0][0] = cay*tmp1 + say*xxx[2][0];
    xxx[0][1] = cay*tmp2 + say*xxx[2][1];
    xxx[0][2] = cay*tmp3 + say*xxx[2][2];

    xxx[2][0] = say*tmp1 - cay*xxx[2][0];
    xxx[2][1] = say*tmp2 - cay*xxx[2][1];
    xxx[2][2] = say*tmp3 - cay*xxx[2][2];

    //S0*A0*A1*A2*A3*A4

    tmp1 = 20000.0*m_target_scale_x;
    xxx[0][0] *= tmp1;
    xxx[0][1] *= tmp1;
    xxx[0][2] *= tmp1;

    tmp1 = 20000.0*m_target_scale_y;
    xxx[1][0] *= tmp1;
    xxx[1][1] *= tmp1;
    xxx[1][2] *= tmp1;

    //A0*A1*A2*A3*A4+B0

    xxx[2][2] += 20000.0;

    double scaled_org_x = org.x+0.5;
    double scaled_org_y = org.y+0.5;

    for (ptrdiff_t i = 0; i < path_data->mPathPoints; i++) {
        double x, y, z, xx;

        xx = path_data->mpPathPoints[i].x;
        y = path_data->mpPathPoints[i].y;

        z = xxx[2][0] * xx + xxx[2][1] * y + xxx[2][2];
        x = xxx[0][0] * xx + xxx[0][1] * y + xxx[0][2];
        y = xxx[1][0] * xx + xxx[1][1] * y + xxx[1][2];

        z = z > 1000.0 ? z : 1000.0;

        x = x / z;
        y = y / z;

        path_data->mpPathPoints[i].x = (long)(x + scaled_org_x);
        path_data->mpPathPoints[i].y = (long)(y + scaled_org_y);
        if (m_round_to_whole_pixel_after_scale_to_target && (m_target_scale_x!=1.0 || m_target_scale_y!=1.0))
        {
            path_data->mpPathPoints[i].x = (path_data->mpPathPoints[i].x + 32)&~63;
            path_data->mpPathPoints[i].y = (path_data->mpPathPoints[i].y + 32)&~63;//fix me: readability
        }
    }
}

bool CWord::CreateOpaqueBox()
{
    if(m_pOpaqueBox) return(true);
    STSStyle style      = m_style.get();
    style.borderStyle   = 0;
    style.outlineWidthX = style.outlineWidthY = 0;
    style.colors[0]     = m_style.get().colors[2];
    style.alpha[0]      = m_style.get().alpha[2];
    int w               = (int)(m_style.get().outlineWidthX + 0.5);
    int h               = (int)(m_style.get().outlineWidthY + 0.5);
    CStringW str;
    str.Format(L"m %d %d l %d %d %d %d %d %d",
               -w,                   -h,
        m_width+w,                   -h,
        m_width+w, m_ascent+m_descent+h,
               -w, m_ascent+m_descent+h);
    m_pOpaqueBox.reset( DEBUG_NEW CPolygon(FwSTSStyle(style), str, 0, 0, 0, 1.0/MAX_SUB_PIXEL, 1.0/MAX_SUB_PIXEL, 0, m_target_scale_x, m_target_scale_y) );
    return(!!m_pOpaqueBox);
}

bool CWord::operator==( const CWord& rhs ) const
{
    return (         this ==&rhs) || (
        m_str.GetId()     == rhs.m_str.GetId()     &&
        m_fWhiteSpaceChar == rhs.m_fWhiteSpaceChar &&
        m_fLineBreak      == rhs.m_fLineBreak      &&
        m_style           == rhs.m_style           && //fix me:?
        m_ktype           == rhs.m_ktype           &&
        m_kstart          == rhs.m_kstart          &&
        m_kend            == rhs.m_kend            &&
        m_width           == rhs.m_width           &&
        m_ascent          == rhs.m_ascent          &&
        m_descent         == rhs.m_descent         &&
        m_target_scale_x  == rhs.m_target_scale_x  &&
        m_target_scale_y  == rhs.m_target_scale_y);
    //m_pOpaqueBox
}


// CText

CText::CText( const FwSTSStyle& style, const CStringW& str, int ktype, int kstart, int kend
    , double target_scale_x/*=1.0*/, double target_scale_y/*=1.0*/ )
    : CWord(style, str, ktype, kstart, kend, target_scale_x, target_scale_y)
{
    if(m_str.Get() == L" ")
    {
        m_fWhiteSpaceChar = true;
    }
    SharedPtrTextInfo text_info;
    TextInfoCacheKey text_info_key;
    text_info_key.m_str_id = m_str.GetId();
    text_info_key.m_style  = m_style;
    text_info_key.UpdateHashValue();
    TextInfoMruCache* text_info_cache = CacheManager::GetTextInfoCache();
    POSITION pos = text_info_cache->Lookup(text_info_key);
    if(pos==NULL)
    {
        TextInfo* tmp=DEBUG_NEW TextInfo();
        GetTextInfo(tmp, m_style, m_str.Get());
        text_info.reset(tmp);
        text_info_cache->UpdateCache(text_info_key, text_info);
    }
    else
    {
        text_info = text_info_cache->GetAt(pos);
        text_info_cache->UpdateCache( pos );
    }
    this->m_ascent  = text_info->m_ascent;
    this->m_descent = text_info->m_descent;
    this->m_width   = text_info->m_width;
}

CText::CText( const CText& src ):CWord(src)
{
    m_width = src.m_width;
}

SharedPtrCWord CText::Copy()
{
    SharedPtrCWord result(DEBUG_NEW CText(*this));
	return result;
}

bool CText::Append(const SharedPtrCWord& w)
{
    boost::shared_ptr<CText> p = boost::dynamic_pointer_cast<CText>(w);
    return (p && CWord::Append(w));
}

bool CText::CreatePath(PathData* path_data)
{
    bool succeeded = false;
    FwCMyFont font(m_style);
    HFONT hOldFont = SelectFont(g_hDC, font.get());
    ASSERT(hOldFont);

    int width = 0;
    const CStringW& str = m_str.Get();
    if(m_style.get().fontSpacing || (long)GetVersion() < 0)
    {
        bool bFirstPath = true;
        for(LPCWSTR s = str; *s; s++)
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
        succeeded = !!GetTextExtentPoint32W(g_hDC, str, str.GetLength(), &extent);
        if(!succeeded)
        {
            SelectFont(g_hDC, hOldFont); ASSERT(0); return(false);
        }
        succeeded = path_data->BeginPath(g_hDC);
        if(!succeeded)
        {
            SelectFont(g_hDC, hOldFont); ASSERT(0); return(false);
        }
        succeeded = !!TextOutW(g_hDC, 0, 0, str, str.GetLength());
        if(!succeeded)
        {
            SelectFont(g_hDC, hOldFont); ASSERT(0); return(false);
        }
        succeeded = path_data->EndPath(g_hDC);
        if(!succeeded)
        {
            SelectFont(g_hDC, hOldFont); ASSERT(0); return(false);
        }
    }
    ASSERT(SelectFont(g_hDC, hOldFont));
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
        if(!GetTextExtentPoint32W(g_hDC, str, str.GetLength(), &extent)) {SelectFont(g_hDC, hOldFont); ASSERT(0); return;}
        output->m_width += extent.cx;
    }
    output->m_width = (int)(style.get().fontScaleX/100*output->m_width + 4) >> 3;
    SelectFont(g_hDC, hOldFont);
}

// CPolygon

CPolygon::CPolygon( const FwSTSStyle& style, const CStringW& str, int ktype, int kstart, int kend 
    , double scalex, double scaley, int baseline 
    , double target_scale_x/*=1.0*/, double target_scale_y/*=1.0*/
    , bool round_to_whole_pixel_after_scale_to_target/*=false*/)
    : CWord(style, str, ktype, kstart, kend, target_scale_x, target_scale_y, round_to_whole_pixel_after_scale_to_target)
    , m_scalex(scalex), m_scaley(scaley), m_baseline(baseline)
{
    ParseStr();
}

CPolygon::CPolygon(CPolygon& src) : CWord(src)
{
    m_scalex           = src.m_scalex;
    m_scaley           = src.m_scaley;
    m_baseline         = src.m_baseline;
    m_width            = src.m_width;
    m_ascent           = src.m_ascent;
    m_descent          = src.m_descent;
    m_pathTypesOrg.Copy (src.m_pathTypesOrg );
    m_pathPointsOrg.Copy(src.m_pathPointsOrg);
}

CPolygon::~CPolygon()
{
}

SharedPtrCWord CPolygon::Copy()
{
    SharedPtrCWord result(DEBUG_NEW CPolygon(*this));
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
    TRACE_PARSER(ret);//fix me: use a specific logger for it
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
    CStringW str = m_str.Get();
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
    m_width      = max(maxx - minx, 0);
    m_ascent     = max(maxy - miny, 0);
    int baseline = (int)(64 * m_scaley * m_baseline);
    m_descent    = baseline;
    m_ascent    -= baseline;
    m_width      = ((int)(m_style.get().fontScaleX/100 * m_width  ) + 4) >> 3;
    m_ascent     = ((int)(m_style.get().fontScaleY/100 * m_ascent ) + 4) >> 3;
    m_descent    = ((int)(m_style.get().fontScaleY/100 * m_descent) + 4) >> 3;
    return(true);
}

bool CPolygon::CreatePath(PathData* path_data)
{
    int len = m_pathTypesOrg.GetCount();
    if(len == 0) return(false);
    if(path_data->mPathPoints != len)
    {
        BYTE* pNewPathTypes = (BYTE*)realloc(path_data->mpPathTypes, len * sizeof(BYTE));
        if (!pNewPathTypes)
        {
            TRACE_PARSER("Overflow!");
            return false;
        }
        path_data->mpPathTypes = pNewPathTypes;

        POINT* pNewPathPoints = (POINT*)realloc(path_data->mpPathPoints, len*sizeof(POINT));
        if (!pNewPathPoints)
        {
            TRACE_PARSER("Overflow!");
            return false;
        }
        path_data->mpPathPoints = pNewPathPoints;
        path_data->mPathPoints  = len;
    }
    memcpy(path_data->mpPathTypes, m_pathTypesOrg.GetData(), len*sizeof(BYTE));
    memcpy(path_data->mpPathPoints, m_pathPointsOrg.GetData(), len*sizeof(POINT));
    return(true);
}

// CClipper

CClipper::CClipper(CStringW str, CSizeCoor2 size, double scalex, double scaley, bool inverse
    , double target_scale_x/*=1.0*/, double target_scale_y/*=1.0*/)
    : m_polygon( DEBUG_NEW CPolygon(FwSTSStyle(), str, 0, 0, 0, scalex, scaley, 0, target_scale_x, target_scale_y, true) )
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
    CWordPaintMachine::PaintBody( m_polygon, CPoint(0, 0), CPoint(0, 0), &overlay );
    int w =  overlay->mOverlayWidth, 
        h =  overlay->mOverlayHeight;
    int x = (overlay->mOffsetX+4)>>3,
        y = (overlay->mOffsetY+4)>>3;
    result = DEBUG_NEW GrayImage2();
    if( !result )
        return result;
    result->data  = overlay->mBody;
    result->pitch = overlay->mOverlayPitch;
    result->size.SetSize(w, h);
    result->left_top.SetPoint(x, y);
    return result;
}

GrayImage2* CClipper::PaintBaseClipper()
{
    GrayImage2* result = NULL;
    //m_pAlphaMask = NULL;
    if (m_size.cx <= 0 || m_size.cy <= 0) {
        return result;
    }

    const size_t alphaMaskSize = size_t(m_size.cx) * m_size.cy;

    SharedPtrOverlay overlay;

    CWordPaintMachine::PaintBody( m_polygon, CPoint(0, 0), CPoint(0, 0), &overlay );
    int w =  overlay->mOverlayWidth,
        h =  overlay->mOverlayHeight;
    int x = (overlay->mOffsetX+4)>>3,
        y = (overlay->mOffsetY+4)>>3;
    int xo = 0, yo = 0;

    if(x < 0) {xo = -x; w -= -x; x = 0;}
    if(y < 0) {yo = -y; h -= -y; y = 0;}
    if(x+w > m_size.cx) w = m_size.cx-x;
    if(y+h > m_size.cy) h = m_size.cy-y;

    if(w <= 0 || h <= 0) return result;

    result = DEBUG_NEW GrayImage2();

    if( !result ) {
        return result;
    }

    result->data.reset( reinterpret_cast<BYTE*>(xy_malloc(alphaMaskSize)), xy_free );
    result->pitch = m_size.cx;
    result->size  = m_size;
    result->left_top.SetPoint(0, 0);

    BYTE * result_data = result->data.get();

    if(!result_data)
    {
        delete result;
        return NULL;
    }

    memset(result_data, (m_inverse ? 0x40 : 0), alphaMaskSize);

    const BYTE* src = overlay->mBody.get() + (overlay->mOverlayPitch * yo + xo);
    BYTE* dst = result_data + m_size.cx * y + x;

    if (m_inverse) {
          for (ptrdiff_t i = 0; i < h; ++i) {
            for (ptrdiff_t wt = 0; wt < w; ++wt) {
                dst[wt] = 0x40 - src[wt];
            }

            src += overlay->mOverlayPitch;
            dst += m_size.cx;
          }
    } else {
       for (ptrdiff_t i = 0; i < h; ++i) {
            memcpy(dst, src, w * sizeof(BYTE));
            src += overlay->mOverlayPitch;
            dst += m_size.cx;
       }
    }
    return result;
}

GrayImage2* CClipper::PaintBannerClipper()
{
    ASSERT(m_polygon);

    int width = static_cast<int>(m_effect.param[2] * m_polygon->m_target_scale_x);//fix me: rounding err
    int w = m_size.cx, 
        h = m_size.cy;

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
    ASSERT(m_polygon);

    int height = static_cast<int>(m_effect.param[4] * m_polygon->m_target_scale_y);//fix me: rounding err
    int w = m_size.cx,
        h = m_size.cy;

    GrayImage2* result = PaintBaseClipper();
    if(!result)
        return result;

    BYTE* data = result->data.get();

    int da = (64<<8)/height;
    int a = 0;
    int k = (static_cast<int>(m_effect.param[0] * m_polygon->m_target_scale_y)>>3);//fix me: rounding err
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
    l = (static_cast<int>(m_effect.param[1] * m_polygon->m_target_scale_y)>>3);//fix me: rounding err
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
    m_effect     = effect;
}

SharedPtrGrayImage2 CClipper::GetAlphaMask( const SharedPtrCClipper& clipper )
{
    SharedPtrGrayImage2 result;
    CClipperPaintMachine paint_machine(clipper);
    paint_machine.Paint(&result);
    return result;
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
        if(m_ascent  < w->m_ascent)  m_ascent  = w->m_ascent;
        if(m_descent < w->m_descent) m_descent = w->m_descent;
        if(m_borderX < w->m_style.get().outlineWidthX) m_borderX = (int)(w->m_style.get().outlineWidthX+0.5);
        if(m_borderY < w->m_style.get().outlineWidthY) m_borderY = (int)(w->m_style.get().outlineWidthY+0.5);
    }
}

CRectCoor2 CLine::PaintAll( CompositeDrawItemList* output, const CRectCoor2& clipRect, 
    const CPointCoor2& margin,
    const SharedPtrCClipperPaintMachine &clipper, CPoint p, const CPoint& org, const int time, const int alpha )
{
    CRectCoor2 bbox(0, 0, 0, 0);
    POSITION pos = GetHeadPosition();
    POSITION outputPos = output->GetHeadPosition();
    while(pos)
    {
        SharedPtrCWord w = GetNext(pos);
        CompositeDrawItem& outputItem = output->GetNext(outputPos);
        if(w->m_fLineBreak) return(bbox); // should not happen since this class is just a line of text without any breaks
        CPointCoor2 shadowPos, outlinePos, bodyPos, org_coor2;

        double shadowPos_x = p.x + w->m_style.get().shadowDepthX;
        double shadowPos_y = p.y + w->m_style.get().shadowDepthY + m_ascent - w->m_ascent;
        outlinePos = CPoint(p.x, p.y + m_ascent - w->m_ascent);
        bodyPos    = CPoint(p.x, p.y + m_ascent - w->m_ascent);

        shadowPos.x   = static_cast<int>(w->m_target_scale_x * shadowPos_x + 0.5) + margin.x;
        shadowPos.y   = static_cast<int>(w->m_target_scale_y * shadowPos_y + 0.5) + margin.y;
        outlinePos.x  =                  w->m_target_scale_x * outlinePos.x       + margin.x;
        outlinePos.y  =                  w->m_target_scale_y * outlinePos.y       + margin.y;
        bodyPos.x     =                  w->m_target_scale_x * bodyPos.x          + margin.x;
        bodyPos.y     =                  w->m_target_scale_y * bodyPos.y          + margin.y;
        org_coor2.x   =                  w->m_target_scale_x * org.x              + margin.x;//fix me: move it out of this loop
        org_coor2.y   =                  w->m_target_scale_y * org.y              + margin.y;

        bool hasShadow  =   w->m_style.get().shadowDepthX != 0 || w->m_style.get().shadowDepthY != 0;
        bool hasOutline = ((w->m_style.get().outlineWidthX*w->m_target_scale_x+0.5>=1.0) ||
                           (w->m_style.get().outlineWidthY*w->m_target_scale_y+0.5>=1.0)) && 
                          !(w->m_ktype == 2 && time < w->m_kstart);
        bool hasBody = true;

        SharedPtrOverlayPaintMachine shadow_pm, outline_pm, body_pm;
        CWordPaintMachine::CreatePaintMachines(w, shadowPos, outlinePos, bodyPos, org_coor2,
            hasShadow  ? &shadow_pm  : NULL, 
            hasOutline ? &outline_pm : NULL, 
            hasBody    ? &body_pm    : NULL);

        //shadow
        if(hasShadow)
        {
            //NOTE: Calculation of shadow's alpha value is different from outline's and body's in the way they're rounded.
            // Shadow is rounded to invisible while outline and body is rounded to visible.
            // Should we change it to be consist with one rounding polocy?
            DWORD a = 0xff - w->m_style.get().alpha[3];
            if(alpha > 0) a = MulDiv(a, 0xff - alpha, 0xff);
            COLORREF shadow = revcolor(w->m_style.get().colors[3]) | (a<<24);

            DWORD sw[6] = {shadow, -1};
            sw[0] = XySubRenderFrameCreater::GetDefaultCreater()->TransColor(sw[0]);
            if(w->m_style.get().borderStyle == 0)
            {
                outputItem.shadow.reset( 
                    DrawItem::CreateDrawItem(shadow_pm, clipRect, clipper, shadowPos.x, shadowPos.y, sw,
                    w->m_ktype > 0 || w->m_style.get().alpha[0] < 0xff,
                    hasOutline)
                    );
            }
            else if(w->m_style.get().borderStyle == 1)
            {
                outputItem.shadow.reset( 
                    DrawItem::CreateDrawItem( shadow_pm, clipRect, clipper, shadowPos.x, shadowPos.y, sw, true, false)
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
            sw[0] = XySubRenderFrameCreater::GetDefaultCreater()->TransColor(sw[0]);
            if(w->m_style.get().borderStyle == 0)
            {
                outputItem.outline.reset( 
                    DrawItem::CreateDrawItem(outline_pm, clipRect, clipper, outlinePos.x, outlinePos.y, sw, 
                    !w->m_style.get().alpha[0] && !w->m_style.get().alpha[1] && !alpha, true)
                    );
            }
            else if(w->m_style.get().borderStyle == 1)
            {
                outputItem.outline.reset( 
                    DrawItem::CreateDrawItem(outline_pm, clipRect, clipper, outlinePos.x, outlinePos.y, sw, true, false)
                    );
            }
        }
        //body
        if(hasBody)
        {
            // colors
            DWORD aprimary   = w->m_style.get().alpha[0];
            DWORD asecondary = w->m_style.get().alpha[1];
            if(alpha > 0) aprimary   += MulDiv(alpha, 0xff - w->m_style.get().alpha[0], 0xff),
                          asecondary += MulDiv(alpha, 0xff - w->m_style.get().alpha[1], 0xff);
            COLORREF primary   = revcolor(w->m_style.get().colors[0]) | ((0xff-aprimary  )<<24);
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
            sw[3] = (int)( (w->m_style.get().outlineWidthX + t*w->m_width)*w->m_target_scale_x ) >> 3;
            sw[4] = sw[2];
            sw[5] = 0x00ffffff;
            sw[0] = XySubRenderFrameCreater::GetDefaultCreater()->TransColor(sw[0]);
            sw[2] = XySubRenderFrameCreater::GetDefaultCreater()->TransColor(sw[2]);
            sw[4] = XySubRenderFrameCreater::GetDefaultCreater()->TransColor(sw[4]);
            outputItem.body.reset( 
                DrawItem::CreateDrawItem(body_pm, clipRect, clipper, bodyPos.x, bodyPos.y, sw, true, false)
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

    m_target_scale_x = m_target_scale_y = 1.0;
    m_hard_position_level = -1;
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
            int fullwidth = GetFullLineWidth(pos);
            int minwidth = fullwidth / ((abs(fullwidth) / maxwidth) + 1);
            int width = 0, wordwidth = 0;
            while(pos && width < minwidth)
            {
                SharedPtrCWord w = m_words.GetNext(pos);
                wordwidth = w->m_width;
                if(abs(width + wordwidth) < abs(maxwidth)) width += wordwidth;
            }
            if (m_wrapStyle == 3 && width < fullwidth && fullwidth - width + wordwidth < maxwidth) {
                width -= wordwidth;
            }
            maxwidth = width;
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
    CLine* ret = DEBUG_NEW CLine();
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

void CSubtitle::CreateClippers( CSize size1, const CSizeCoor2& size_scale_to )
{
    size1.cx >>= 3;
    size1.cy >>= 3;
    if(m_effects[EF_BANNER] && m_effects[EF_BANNER]->param[2])
    {
        int w = size1.cx, h = size1.cy;
        if(!m_pClipper)
        {
            CStringW str;
            str.Format(L"m %d %d l %d %d %d %d %d %d", 0, 0, w, 0, w, h, 0, h);
            m_pClipper.reset( DEBUG_NEW CClipper(str, size_scale_to, 1, 1, false, m_target_scale_x, m_target_scale_y) );
            if(!m_pClipper) return;
        }
        m_pClipper->SetEffect( *m_effects[EF_BANNER], EF_BANNER );
    }
    else if(m_effects[EF_SCROLL] && m_effects[EF_SCROLL]->param[4])
    {
        int height = m_effects[EF_SCROLL]->param[4];
        int w = size1.cx, h = size1.cy;
        if(!m_pClipper)
        {
            CStringW str;
            str.Format(L"m %d %d l %d %d %d %d %d %d", 0, 0, w, 0, w, h, 0, h);
            m_pClipper.reset( DEBUG_NEW CClipper(str, size_scale_to, 1, 1, false, m_target_scale_x, m_target_scale_y) ); 
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
    TRACE_RENDERER_REQUEST("Begin AdvanceToSegment. m_subrects.size:"
        <<m_subrects.GetCount()<<" sa.size:"<<sa.GetCount());
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
                    r.top    = sr.r.bottom;
                }
                else
                {
                    r.top    = sr.r.top - r.Height();
                    r.bottom = sr.r.top;
                }
                fOK = false;
            }
        }
    }
    while(!fOK);
    SubRect sr;
    sr.r       = r;
    sr.segment = segment;
    sr.entry   = entry;
    sr.layer   = layer;
    m_subrects.AddTail(sr);
    return(sr.r + CRect(0, -s->m_topborder, 0, -s->m_bottomborder));
}

// CRenderedTextSubtitle

CAtlMap<CStringW, AssCmdType, CStringElementTraits<CStringW>> CRenderedTextSubtitle::m_cmdMap;

std::size_t CRenderedTextSubtitle::s_max_cache_size = SIZE_MAX;

std::size_t CRenderedTextSubtitle::SetMaxCacheSize( std::size_t max_cache_size )
{
    s_max_cache_size = max_cache_size;
    XY_LOG_INFO("MAX_CACHE_SIZE: "<<s_max_cache_size);
    return s_max_cache_size;
}

CAtlArray<AssCmdPosLevel> CRenderedTextSubtitle::m_cmd_pos_level;

CRenderedTextSubtitle::CRenderedTextSubtitle(CCritSec* pLock)
    : CSubPicProviderImpl(pLock)
    , m_target_scale_x(1.0), m_target_scale_y(1.0)
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
    m_movable = true;
}

CRenderedTextSubtitle::~CRenderedTextSubtitle()
{
    Deinit();
    g_hDC_refcnt--;
    if(g_hDC_refcnt == 0) DeleteDC(g_hDC);
}

void CRenderedTextSubtitle::InitCmdMap()
{
    if (m_cmdMap.IsEmpty()) {
        m_cmdMap[L"1c"]    = CMD_1c;
        m_cmdMap[L"2c"]    = CMD_2c;
        m_cmdMap[L"3c"]    = CMD_3c;
        m_cmdMap[L"4c"]    = CMD_4c;
        m_cmdMap[L"1a"]    = CMD_1a;
        m_cmdMap[L"2a"]    = CMD_2a;
        m_cmdMap[L"3a"]    = CMD_3a;
        m_cmdMap[L"4a"]    = CMD_4a;
        m_cmdMap[L"alpha"] = CMD_alpha;
        m_cmdMap[L"an"]    = CMD_an;
        m_cmdMap[L"a"]     = CMD_a;
        m_cmdMap[L"blur"]  = CMD_blur;
        m_cmdMap[L"bord"]  = CMD_bord;
        m_cmdMap[L"be"]    = CMD_be;
        m_cmdMap[L"b"]     = CMD_b;
        m_cmdMap[L"clip"]  = CMD_clip;
        m_cmdMap[L"iclip"] = CMD_iclip;
        m_cmdMap[L"c"]     = CMD_c;
        m_cmdMap[L"fade"]  = CMD_fade;
        m_cmdMap[L"fad"]   = CMD_fade;
        m_cmdMap[L"fax"]   = CMD_fax;
        m_cmdMap[L"fay"]   = CMD_fay;
        m_cmdMap[L"fe"]    = CMD_fe;
        m_cmdMap[L"fn"]    = CMD_fn;
        m_cmdMap[L"frx"]   = CMD_frx;
        m_cmdMap[L"fry"]   = CMD_fry;
        m_cmdMap[L"frz"]   = CMD_frz;
        m_cmdMap[L"fr"]    = CMD_fr;
        m_cmdMap[L"fscx"]  = CMD_fscx;
        m_cmdMap[L"fscy"]  = CMD_fscy;
        m_cmdMap[L"fsc"]   = CMD_fsc;
        m_cmdMap[L"fsp"]   = CMD_fsp;
        m_cmdMap[L"fs"]    = CMD_fs;
        m_cmdMap[L"i"]     = CMD_i;
        m_cmdMap[L"kt"]    = CMD_kt;
        m_cmdMap[L"kf"]    = CMD_kf;
        m_cmdMap[L"K"]     = CMD_K;
        m_cmdMap[L"ko"]    = CMD_ko;
        m_cmdMap[L"k"]     = CMD_k;
        m_cmdMap[L"move"]  = CMD_move;
        m_cmdMap[L"org"]   = CMD_org;
        m_cmdMap[L"pbo"]   = CMD_pbo;
        m_cmdMap[L"pos"]   = CMD_pos;
        m_cmdMap[L"p"]     = CMD_p;
        m_cmdMap[L"q"]     = CMD_q;
        m_cmdMap[L"r"]     = CMD_r;
        m_cmdMap[L"shad"]  = CMD_shad;
        m_cmdMap[L"s"]     = CMD_s;
        m_cmdMap[L"t"]     = CMD_t;
        m_cmdMap[L"u"]     = CMD_u;
        m_cmdMap[L"xbord"] = CMD_xbord;
        m_cmdMap[L"xshad"] = CMD_xshad;
        m_cmdMap[L"ybord"] = CMD_ybord;
        m_cmdMap[L"yshad"] = CMD_yshad;
    }
    m_cmd_pos_level.SetCount(CMD_COUNT+1);
    m_cmd_pos_level[CMD_1c   ] = POS_LVL_NONE;
    m_cmd_pos_level[CMD_2c   ] = POS_LVL_NONE;
    m_cmd_pos_level[CMD_3c   ] = POS_LVL_NONE;
    m_cmd_pos_level[CMD_4c   ] = POS_LVL_NONE;
    m_cmd_pos_level[CMD_1a   ] = POS_LVL_NONE;
    m_cmd_pos_level[CMD_2a   ] = POS_LVL_NONE;
    m_cmd_pos_level[CMD_3a   ] = POS_LVL_NONE;
    m_cmd_pos_level[CMD_4a   ] = POS_LVL_NONE;
    m_cmd_pos_level[CMD_alpha] = POS_LVL_NONE;
    m_cmd_pos_level[CMD_an   ] = POS_LVL_NONE;
    m_cmd_pos_level[CMD_a    ] = POS_LVL_NONE;
    m_cmd_pos_level[CMD_blur ] = POS_LVL_NONE;
    m_cmd_pos_level[CMD_bord ] = POS_LVL_NONE;
    m_cmd_pos_level[CMD_be   ] = POS_LVL_NONE;
    m_cmd_pos_level[CMD_b    ] = POS_LVL_NONE;

    m_cmd_pos_level[CMD_clip ] = POS_LVL_HARD;
    m_cmd_pos_level[CMD_iclip] = POS_LVL_HARD;

    m_cmd_pos_level[CMD_c    ] = POS_LVL_NONE;
    m_cmd_pos_level[CMD_fade ] = POS_LVL_NONE;

    m_cmd_pos_level[CMD_fax  ] = POS_LVL_SOFT;
    m_cmd_pos_level[CMD_fay  ] = POS_LVL_SOFT;

    m_cmd_pos_level[CMD_fe   ] = POS_LVL_NONE;
    m_cmd_pos_level[CMD_fn   ] = POS_LVL_NONE;

    m_cmd_pos_level[CMD_frx  ] = POS_LVL_SOFT;
    m_cmd_pos_level[CMD_fry  ] = POS_LVL_SOFT;
    m_cmd_pos_level[CMD_frz  ] = POS_LVL_SOFT;
    m_cmd_pos_level[CMD_fr   ] = POS_LVL_SOFT;
    m_cmd_pos_level[CMD_fscx ] = POS_LVL_SOFT;
    m_cmd_pos_level[CMD_fscy ] = POS_LVL_SOFT;
    m_cmd_pos_level[CMD_fsc  ] = POS_LVL_SOFT;
    m_cmd_pos_level[CMD_fsp  ] = POS_LVL_SOFT;
    m_cmd_pos_level[CMD_fs   ] = POS_LVL_SOFT;

    m_cmd_pos_level[CMD_i    ] = POS_LVL_NONE;

    m_cmd_pos_level[CMD_kt   ] = POS_LVL_SOFT;
    m_cmd_pos_level[CMD_kf   ] = POS_LVL_SOFT;
    m_cmd_pos_level[CMD_K    ] = POS_LVL_SOFT;
    m_cmd_pos_level[CMD_ko   ] = POS_LVL_SOFT;
    m_cmd_pos_level[CMD_k    ] = POS_LVL_SOFT;

    m_cmd_pos_level[CMD_move ] = POS_LVL_HARD;
    m_cmd_pos_level[CMD_org  ] = POS_LVL_HARD;
    m_cmd_pos_level[CMD_pbo  ] = POS_LVL_HARD;
    m_cmd_pos_level[CMD_pos  ] = POS_LVL_HARD;
    m_cmd_pos_level[CMD_p    ] = POS_LVL_HARD;

    m_cmd_pos_level[CMD_q    ] = POS_LVL_NONE;
    m_cmd_pos_level[CMD_r    ] = POS_LVL_NONE;
    m_cmd_pos_level[CMD_shad ] = POS_LVL_NONE;
    m_cmd_pos_level[CMD_s    ] = POS_LVL_NONE;
    m_cmd_pos_level[CMD_t    ] = POS_LVL_NONE;
    m_cmd_pos_level[CMD_u    ] = POS_LVL_NONE;
    m_cmd_pos_level[CMD_xbord] = POS_LVL_NONE;
    m_cmd_pos_level[CMD_xshad] = POS_LVL_NONE;
    m_cmd_pos_level[CMD_ybord] = POS_LVL_NONE;
    m_cmd_pos_level[CMD_yshad] = POS_LVL_NONE;

    m_cmd_pos_level[CMD_COUNT] = POS_LVL_NONE;
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
    POSITION pos = m_subtitleCacheEntry.GetHeadPosition();
    while(pos)
    {
        int i = m_subtitleCacheEntry.GetNext(pos);
        delete m_subtitleCache.GetAt(i);
        m_subtitleCache.SetAt(i, NULL);
    }
    m_subtitleCacheEntry.RemoveAll();
    m_subtitleCache.ReleaseMemory(m_entries.GetCount());

    m_sla.Empty();
}


bool CRenderedTextSubtitle::Init( const CRectCoor2& video_rect, const CRectCoor2& subtitle_target_rect,
    const SIZE& original_video_size )
{
    XY_LOG_INFO(_T(""));
    Deinit();
    m_video_rect = CRect(video_rect.left*MAX_SUB_PIXEL, 
                         video_rect.top*MAX_SUB_PIXEL, 
                         video_rect.right*MAX_SUB_PIXEL, 
                         video_rect.bottom*MAX_SUB_PIXEL);
    m_subtitle_target_rect = CRect(subtitle_target_rect.left*MAX_SUB_PIXEL, subtitle_target_rect.top*MAX_SUB_PIXEL, 
        subtitle_target_rect.right*MAX_SUB_PIXEL, subtitle_target_rect.bottom*MAX_SUB_PIXEL);
    m_size = CSize(original_video_size.cx*MAX_SUB_PIXEL, original_video_size.cy*MAX_SUB_PIXEL);

    ASSERT(original_video_size.cx!=0 && original_video_size.cy!=0);

    m_target_scale_x = video_rect.Width()  * 1.0 / original_video_size.cx;
    m_target_scale_y = video_rect.Height() * 1.0 / original_video_size.cy;

    return(true);
}

void CRenderedTextSubtitle::Deinit()
{
    POSITION pos = m_subtitleCacheEntry.GetHeadPosition();
    while(pos)
    {
        int i = m_subtitleCacheEntry.GetNext(pos);
        delete m_subtitleCache.GetAt(i);
    }
    m_subtitleCache.ReleaseMemory();
    m_subtitleCacheEntry.RemoveAll();
    m_sla.Empty();

    m_video_rect.SetRectEmpty();
    m_subtitle_target_rect.SetRectEmpty();
    m_size = CSize(0, 0);

    m_target_scale_x = m_target_scale_y = 1.0;

    CacheManager::GetBitmapMruCache()->RemoveAll();

    CacheManager::GetClipperAlphaMaskMruCache()->RemoveAll();
    CacheManager::GetTextInfoCache           ()->RemoveAll();
    //CacheManager::GetAssTagListMruCache      ()->RemoveAll();

    CacheManager::GetScanLineDataMruCache   ()->RemoveAll();
    CacheManager::GetOverlayNoOffsetMruCache()->RemoveAll();

    CacheManager::GetSubpixelVarianceCache()->RemoveAll();
    CacheManager::GetOverlayMruCache      ()->RemoveAll();
    CacheManager::GetOverlayNoBlurMruCache()->RemoveAll();
    CacheManager::GetScanLineData2MruCache()->RemoveAll();
    CacheManager::GetPathDataMruCache     ()->RemoveAll();

    //The ids generated by XyFlyWeight is ever increasing.
    //That is good for ids generated after re-init won't conflict with ids generated before re-init.
    XyFwGroupedDrawItemsHashKey::GetCacher()->RemoveAll();
}

void CRenderedTextSubtitle::ParseEffect(CSubtitle* sub, const CStringW& str)
{
    CStringW::PCXSTR str_start = str.GetString();
    CStringW::PCXSTR str_end   = str.GetLength() + str_start;
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
        Effect* e = DEBUG_NEW Effect;
        if(!e) return;
        sub->m_effects[e->type = EF_BANNER] = e;
        e->param[0] = (int)(max(1.0*delay/sub->m_scalex, 1));
        e->param[1] = lefttoright;
        e->param[2] = (int)(sub->m_scalex*fadeawaywidth);
        sub->m_wrapStyle = 2;

        sub->m_hard_position_level = sub->m_hard_position_level > POS_LVL_NONE ? 
                                     sub->m_hard_position_level : POS_LVL_NONE;
    }
    else if(!effect.CompareNoCase(L"Scroll up;") || !effect.CompareNoCase(L"Scroll down;"))
    {
        int top, bottom, delay, fadeawayheight = 0;
        if(swscanf(s, L"%d;%d;%d;%d", &top, &bottom, &delay, &fadeawayheight) < 3) return;
        if(top > bottom) {int tmp = top; top = bottom; bottom = tmp;}
        Effect* e = DEBUG_NEW Effect;
        if(!e) return;
        sub->m_effects[e->type = EF_SCROLL] = e;
        e->param[0] = (int)(sub->m_scaley*top*MAX_SUB_PIXEL);
        e->param[1] = (int)(sub->m_scaley*bottom*MAX_SUB_PIXEL);
        e->param[2] = (int)(max(1.0*delay/sub->m_scaley, 1));
        e->param[3] = (effect.GetLength() == 12);
        e->param[4] = (int)(sub->m_scaley*fadeawayheight);

        sub->m_hard_position_level = POS_LVL_HARD;
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
            if(PCWord tmp_ptr = DEBUG_NEW CText(style, str.Mid(ite, j-ite), m_ktype, m_kstart, m_kend
                , m_target_scale_x, m_target_scale_y))
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
            if(PCWord tmp_ptr = DEBUG_NEW CText(style, CStringW(), m_ktype, m_kstart, m_kend
                , m_target_scale_x, m_target_scale_y))
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
            if(PCWord tmp_ptr = DEBUG_NEW CText(style, CStringW(c), m_ktype, m_kstart, m_kend
                , m_target_scale_x, m_target_scale_y))
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
    if (!sub || !str.GetLength() || !m_nPolygon) return;

    if (PCWord tmp_ptr = DEBUG_NEW CPolygon(style, str, m_ktype, m_kstart, m_kend, sub->m_scalex/(1<<(m_nPolygon-1))
        , sub->m_scaley/(1<<(m_nPolygon-1)), m_polygonBaselineOffset
        , m_target_scale_x, m_target_scale_y))
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

bool CRenderedTextSubtitle::ParseSSATag(AssTagList& assTags, const CStringW& str)
{
    if (m_tagCache.AssTagsCache.Lookup(str, assTags)) {
        return true;
    }

    int nTags = 0, nUnrecognizedTags = 0;
    assTags.reset(DEBUG_NEW CAtlList<AssTag>());

    for (int i = 0, j; (j = str.Find(L'\\', i)) >= 0; i = j) {
        int jOld;
        // find the end of the current tag or the start of its parameters
        for (jOld = ++j; str[j] && str[j] != L'(' && str[j] != L'\\'; ++j) {
            ;
        }
        CStringW cmdType = str.Mid(jOld, j - jOld);
        cmdType.Trim();
        if (cmdType.IsEmpty()) {
            continue;
        }

        nTags++;

        AssTag tag;
        tag.cmdType = CMD_COUNT;
        for (int cmdLength = min(CMD_MAX_LENGTH, cmdType.GetLength()), cmdLengthMin = CMD_MIN_LENGTH; cmdLength >= cmdLengthMin; cmdLength--) {
            if (m_cmdMap.Lookup(cmdType.Left(cmdLength), tag.cmdType)) {
                break;
            }
        }
        if (tag.cmdType == CMD_COUNT) {
            nUnrecognizedTags++;
            continue;
        }

        if (str[j] == L'(') {
            // complex tags search
            int br = 1; // 1 bracket
            // find the end of the parameters
            for (jOld = ++j; str[j] && br > 0; ++j) {
                if (str[j] == L'(') {
                    br++;
                } else if (str[j] == L')') {
                    br--;
                }
                if (br == 0) {
                    break;
                }
            }
            CStringW param = str.Mid(jOld, j - jOld);
            param.Trim();

            while (!param.IsEmpty()) {
                int k = param.Find(L','), l = param.Find(L'\\');

                if (k >= 0 && (l < 0 || k < l)) {
                    CStringW s = param.Left(k).Trim();
                    if (!s.IsEmpty()) {
                        tag.params.Add(s);
                    }
                    param = k + 1 < param.GetLength() ? param.Mid(k + 1) : L"";
                } else {
                    param.Trim();
                    if (!param.IsEmpty()) {
                        tag.params.Add(param);
                    }
                    param.Empty();
                }
            }
        }

        switch (tag.cmdType) {
            case CMD_1c:
            case CMD_2c:
            case CMD_3c:
            case CMD_4c:
            case CMD_1a:
            case CMD_2a:
            case CMD_3a:
            case CMD_4a:
                if (cmdType.GetLength() > 2) {
                    tag.paramsInt.Add(wcstol(cmdType.Mid(2).Trim(L"&H"), nullptr, 16));
                }
                break;
            case CMD_alpha:
                if (cmdType.GetLength() > 5) {
                    tag.paramsInt.Add(wcstol(cmdType.Mid(5).Trim(L"&H"), nullptr, 16));
                }
                break;
            case CMD_an:
            case CMD_fe:
            case CMD_kt:
            case CMD_kf:
            case CMD_ko:
                if (cmdType.GetLength() > 2) {
                    tag.paramsInt.Add(wcstol(cmdType.Mid(2), nullptr, 10));
                }
                break;
            case CMD_fn:
                tag.params.Add(cmdType.Mid(2));
                break;
            case CMD_be:
            case CMD_fr:
                if (cmdType.GetLength() > 2) {
                    tag.paramsReal.Add(wcstod(cmdType.Mid(2), nullptr));
                }
                break;
            case CMD_fs:
                if (cmdType.GetLength() > 2) {
                    int s = 2;
                    if (cmdType[s] == L'+' || cmdType[s] == L'-') {
                        tag.params.Add(cmdType.Mid(s, 1));
                    }
                    tag.paramsReal.Add(wcstod(cmdType.Mid(s), nullptr));
                }
                break;
            case CMD_a:
            case CMD_b:
            case CMD_i:
            case CMD_k:
            case CMD_K:
            case CMD_p:
            case CMD_q:
            case CMD_s:
            case CMD_u:
                if (cmdType.GetLength() > 1) {
                    tag.paramsInt.Add(wcstol(cmdType.Mid(1), nullptr, 10));
                }
                break;
            case CMD_r:
                tag.params.Add(cmdType.Mid(1));
                break;
            case CMD_blur:
            case CMD_bord:
            case CMD_fscx:
            case CMD_fscy:
            case CMD_shad:
                if (cmdType.GetLength() > 4) {
                    tag.paramsReal.Add(wcstod(cmdType.Mid(4), nullptr));
                }
                break;
            case CMD_clip:
            case CMD_iclip: {
                size_t nParams = tag.params.GetCount();
                if (nParams == 2) {
                    tag.paramsInt.Add(wcstol(tag.params[0], nullptr, 10));
                    tag.params.RemoveAt(0);
                } else if (nParams == 4) {
                    for (size_t n = 0; n < nParams; n++) {
                        tag.paramsReal.Add(wcstod(tag.params[n], nullptr));
                    }
                    tag.params.RemoveAll();
                }
            }
            break;
            case CMD_fade: {
                size_t nParams = tag.params.GetCount();
                if (nParams == 7 || nParams == 2) {
                    for (size_t n = 0; n < nParams; n++) {
                        tag.paramsInt.Add(wcstol(tag.params[n], nullptr, 10));
                    }
                    tag.params.RemoveAll();
                }
            }
            break;
            case CMD_move: {
                size_t nParams = tag.params.GetCount();
                if (nParams == 4 || nParams == 6) {
                    for (size_t n = 0; n < 4; n++) {
                        tag.paramsReal.Add(wcstod(tag.params[n], nullptr));
                    }
                    for (size_t n = 4; n < nParams; n++) {
                        tag.paramsInt.Add(wcstol(tag.params[n], nullptr, 10));
                    }
                    tag.params.RemoveAll();
                }
            }
            break;
            case CMD_org:
            case CMD_pos: {
                size_t nParams = tag.params.GetCount();
                if (nParams == 2) {
                    for (size_t n = 0; n < nParams; n++) {
                        tag.paramsReal.Add(wcstod(tag.params[n], nullptr));
                    }
                    tag.params.RemoveAll();
                }
            }
            break;
            case CMD_c:
                if (cmdType.GetLength() > 1) {
                    tag.paramsInt.Add(wcstol(cmdType.Mid(1).Trim(L"&H"), nullptr, 16));
                }
                break;
            case CMD_frx:
            case CMD_fry:
            case CMD_frz:
            case CMD_fax:
            case CMD_fay:
            case CMD_fsc:
            case CMD_fsp:
                if (cmdType.GetLength() > 3) {
                    tag.paramsReal.Add(wcstod(cmdType.Mid(3), nullptr));
                }
                break;
            case CMD_pbo:
                if (cmdType.GetLength() > 3) {
                    tag.paramsInt.Add(wcstol(cmdType.Mid(3), nullptr, 10));
                }
                break;
            case CMD_t: {
                size_t nParams = tag.params.GetCount();
                if (nParams >= 1 && nParams <= 4) {
                    if (nParams == 2) {
                        tag.paramsReal.Add(wcstod(tag.params[0], nullptr));
                    } else if (nParams == 3) {
                        tag.paramsReal.Add(wcstod(tag.params[0], nullptr));
                        tag.paramsReal.Add(wcstod(tag.params[1], nullptr));
                    } else if (nParams == 4) {
                        tag.paramsInt.Add(wcstol(tag.params[0], nullptr, 10));
                        tag.paramsInt.Add(wcstol(tag.params[1], nullptr, 10));
                        tag.paramsReal.Add(wcstod(tag.params[2], nullptr));
                    }

                    ParseSSATag(tag.embeded, tag.params[nParams - 1]);
                }
                tag.params.RemoveAll();
            }
            break;
            case CMD_xbord:
            case CMD_xshad:
            case CMD_ybord:
            case CMD_yshad:
                if (cmdType.GetLength() > 5) {
                    tag.paramsReal.Add(wcstod(cmdType.Mid(5), nullptr));
                }
                break;
        }

        assTags->AddTail(tag);
    }

    m_tagCache.AssTagsCache.SetAt(str, assTags);

    //return (nUnrecognizedTags < nTags);
    return true; // there are people keeping comments inside {}, lets make them happy now
}

bool CRenderedTextSubtitle::CreateSubFromSSATag(CSubtitle* sub, const AssTagList& assTags,
                                                STSStyle& style, STSStyle& org, bool fAnimate /*= false*/)
{
    if (!sub || !assTags) {
        return false;
    }

    POSITION pos = assTags->GetHeadPosition();
    while (pos) {
        const AssTag& tag = assTags->GetNext(pos);

        sub->m_hard_position_level = sub->m_hard_position_level > m_cmd_pos_level[tag.cmdType] ?
                                     sub->m_hard_position_level : m_cmd_pos_level[tag.cmdType];
        // TODO: call ParseStyleModifier(cmdType, params, ..) and move the rest there

        switch (tag.cmdType) {
            case CMD_1c:
            case CMD_2c:
            case CMD_3c:
            case CMD_4c: {
                int k = tag.cmdType - CMD_1c;

                if (!tag.paramsInt.IsEmpty()) {
                    DWORD c = tag.paramsInt[0];
                    style.colors[k] = (((int)CalcAnimation(c & 0xff, style.colors[k] & 0xff, fAnimate)) & 0xff
                                       | ((int)CalcAnimation(c & 0xff00, style.colors[k] & 0xff00, fAnimate)) & 0xff00
                                       | ((int)CalcAnimation(c & 0xff0000, style.colors[k] & 0xff0000, fAnimate)) & 0xff0000);
                } else {
                    style.colors[k] = org.colors[k];
                }
            }
            break;
            case CMD_1a:
            case CMD_2a:
            case CMD_3a:
            case CMD_4a: {
                int k = tag.cmdType - CMD_1a;

                style.alpha[k] = !tag.paramsInt.IsEmpty()
                                 ? (BYTE)CalcAnimation(tag.paramsInt[0] & 0xff, style.alpha[k], fAnimate)
                                 : org.alpha[k];
            }
            break;
            case CMD_alpha:
                for (ptrdiff_t k = 0; k < 4; k++) {
                    style.alpha[k] = !tag.paramsInt.IsEmpty()
                                     ? (BYTE)CalcAnimation(tag.paramsInt[0] & 0xff, style.alpha[k], fAnimate)
                                     : org.alpha[k];
                }
                break;
            case CMD_an: {
                int n = !tag.paramsInt.IsEmpty() ? tag.paramsInt[0] : 0;
                if (sub->m_scrAlignment < 0) {
                    sub->m_scrAlignment = (n > 0 && n < 10) ? n : org.scrAlignment;
                }
            }
            break;
            case CMD_a: {
                int n = !tag.paramsInt.IsEmpty() ? tag.paramsInt[0] : 0;
                if (sub->m_scrAlignment < 0) {
                    sub->m_scrAlignment = (n > 0 && n < 12) ? ((((n - 1) & 3) + 1) + ((n & 4) ? 6 : 0) + ((n & 8) ? 3 : 0)) : org.scrAlignment;
                }
            }
            break;
            case CMD_blur:
                if (!tag.paramsReal.IsEmpty()) {
                    double n = CalcAnimation(tag.paramsReal[0], style.fGaussianBlur, fAnimate);
                    style.fGaussianBlur = (n < 0 ? 0 : n);
                } else {
                    style.fGaussianBlur = org.fGaussianBlur;
                }
                break;
            case CMD_bord:
                if (!tag.paramsReal.IsEmpty()) {
                    double nx = CalcAnimation(tag.paramsReal[0], style.outlineWidthX, fAnimate);
                    style.outlineWidthX = (nx < 0 ? 0 : nx);
                    double ny = CalcAnimation(tag.paramsReal[0], style.outlineWidthY, fAnimate);
                    style.outlineWidthY = (ny < 0 ? 0 : ny);
                } else {
                    style.outlineWidthX = org.outlineWidthX;
                    style.outlineWidthY = org.outlineWidthY;
                }
                break;
            case CMD_be:
                style.fBlur = !tag.paramsReal.IsEmpty()
                              ? (double)(CalcAnimation(tag.paramsReal[0], style.fBlur, fAnimate))
                              : org.fBlur;
                break;
            case CMD_b: {
                int n = !tag.paramsInt.IsEmpty() ? tag.paramsInt[0] : -1;
                style.fontWeight = (n == 0 ? FW_NORMAL : n == 1 ? FW_BOLD : n >= 100 ? n : org.fontWeight);
            }
            break;
            case CMD_clip:
            case CMD_iclip: {
                bool invert = (tag.cmdType == CMD_iclip);
                size_t nParams = tag.params.GetCount();
                size_t nParamsInt = tag.paramsInt.GetCount();
                size_t nParamsReal = tag.paramsReal.GetCount();

                if (nParams == 1 && nParamsInt == 0 && !sub->m_pClipper) {
                    sub->m_pClipper.reset ( DEBUG_NEW CClipper(tag.params[0], CSize(m_video_rect.Width()>>3, m_video_rect.Height()>>3), sub->m_scalex, sub->m_scaley, invert, m_target_scale_x, m_target_scale_y) );
                } else if (nParams == 1 && nParamsInt == 1 && !sub->m_pClipper) {
                    long scale = tag.paramsInt[0];
                    if (scale < 1) {
                        scale = 1;
                    }
                    sub->m_pClipper.reset ( DEBUG_NEW CClipper(tag.params[0], CSize(m_video_rect.Width()>>3, m_video_rect.Height()>>3),
                                                         sub->m_scalex / (1 << (scale - 1)), sub->m_scaley / (1 << (scale - 1)), invert, m_target_scale_x, m_target_scale_y) );
                } else if (nParamsReal == 4) {
                    CRect r;
                    sub->m_clipInverse = invert;
                    r.SetRect(
                        tag.paramsReal[0]+0.5,
                        tag.paramsReal[1]+0.5,
                        tag.paramsReal[2]+0.5,
                        tag.paramsReal[3]+0.5);
                    sub->m_clip.SetRect(
                        static_cast<int>(CalcAnimation(sub->m_scalex*r.left  , sub->m_clip.left  , fAnimate)),
                        static_cast<int>(CalcAnimation(sub->m_scaley*r.top   , sub->m_clip.top   , fAnimate)),
                        static_cast<int>(CalcAnimation(sub->m_scalex*r.right , sub->m_clip.right , fAnimate)),
                        static_cast<int>(CalcAnimation(sub->m_scaley*r.bottom, sub->m_clip.bottom, fAnimate)));
                }
            }
            break;
            case CMD_c:
                if (!tag.paramsInt.IsEmpty()) {
                    DWORD c = tag.paramsInt[0];
                    style.colors[0] = (((int)CalcAnimation(c & 0xff, style.colors[0] & 0xff, fAnimate)) & 0xff
                                       | ((int)CalcAnimation(c & 0xff00, style.colors[0] & 0xff00, fAnimate)) & 0xff00
                                       | ((int)CalcAnimation(c & 0xff0000, style.colors[0] & 0xff0000, fAnimate)) & 0xff0000);
                } else {
                    style.colors[0] = org.colors[0];
                }
                break;
            case CMD_fade: {
                sub->m_fAnimated2 = true;

                if (tag.paramsInt.GetCount() == 7 && !sub->m_effects[EF_FADE]) { // {\fade(a1=param[0], a2=param[1], a3=param[2], t1=t[0], t2=t[1], t3=t[2], t4=t[3])
                    if (Effect* e = DEBUG_NEW Effect) {
                        for (size_t k = 0; k < 3; k++) {
                            e->param[k] = tag.paramsInt[k];
                        }
                        for (size_t k = 0; k < 4; k++) {
                            e->t[k] = tag.paramsInt[3 + k];
                        }

                        sub->m_effects[EF_FADE] = e;
                    }
                } else if (tag.paramsInt.GetCount() == 2 && !sub->m_effects[EF_FADE]) { // {\fad(t1=t[1], t2=t[2])
                    if (Effect* e = DEBUG_NEW Effect) {
                        e->param[0] = e->param[2] = 0xff;
                        e->param[1] = 0x00;
                        for (size_t k = 1; k < 3; k++) {
                            e->t[k] = tag.paramsInt[k - 1];
                        }
                        e->t[0] = e->t[3] = -1; // will be substituted with "start" and "end"

                        sub->m_effects[EF_FADE] = e;
                    }
                }
            }
            break;
            case CMD_fax:
                style.fontShiftX = !tag.paramsReal.IsEmpty()
                                   ? CalcAnimation(tag.paramsReal[0], style.fontShiftX, fAnimate)
                                   : org.fontShiftX;
                break;
            case CMD_fay:
                style.fontShiftY = !tag.paramsReal.IsEmpty()
                                   ? CalcAnimation(tag.paramsReal[0], style.fontShiftY, fAnimate)
                                   : org.fontShiftY;
                break;
            case CMD_fe:
                style.charSet = !tag.paramsInt.IsEmpty()
                                ? tag.paramsInt[0]
                                : org.charSet;
                break;
            case CMD_fn:
                style.fontName = (!tag.params.IsEmpty() && !tag.params[0].IsEmpty() && tag.params[0] != L"0")
                                 ? CString(tag.params[0]).Trim()
                                 : org.fontName;
                break;
            case CMD_frx:
                style.fontAngleX = !tag.paramsReal.IsEmpty()
                                   ? CalcAnimation(tag.paramsReal[0], style.fontAngleX, fAnimate)
                                   : org.fontAngleX;
                break;
            case CMD_fry:
                style.fontAngleY = !tag.paramsReal.IsEmpty()
                                   ? CalcAnimation(tag.paramsReal[0], style.fontAngleY, fAnimate)
                                   : org.fontAngleY;
                break;
            case CMD_frz:
            case CMD_fr:
                style.fontAngleZ = !tag.paramsReal.IsEmpty()
                                   ? CalcAnimation(tag.paramsReal[0], style.fontAngleZ, fAnimate)
                                   : org.fontAngleZ;
                break;
            case CMD_fscx:
                if (!tag.paramsReal.IsEmpty()) {
                    double n = CalcAnimation(tag.paramsReal[0], style.fontScaleX, fAnimate);
                    style.fontScaleX = (n < 0 ? 0 : n);
                } else {
                    style.fontScaleX = org.fontScaleX;
                }
                break;
            case CMD_fscy:
                if (!tag.paramsReal.IsEmpty()) {
                    double n = CalcAnimation(tag.paramsReal[0], style.fontScaleY, fAnimate);
                    style.fontScaleY = (n < 0 ? 0 : n);
                } else {
                    style.fontScaleY = org.fontScaleY;
                }
                break;
            case CMD_fsc:
                style.fontScaleX = org.fontScaleX;
                style.fontScaleY = org.fontScaleY;
                break;
            case CMD_fsp:
                style.fontSpacing = !tag.paramsReal.IsEmpty()
                                    ? CalcAnimation(tag.paramsReal[0], style.fontSpacing, fAnimate)
                                    : org.fontSpacing;
                break;
            case CMD_fs:
                if (!tag.paramsReal.IsEmpty()) {
                    if (!tag.params.IsEmpty() && (tag.params[0][0] == L'-' || tag.params[0][0] == L'+')) {
                        double n = CalcAnimation(style.fontSize + style.fontSize * tag.paramsReal[0] / 10, style.fontSize, fAnimate);
                        style.fontSize = (n > 0) ? n : org.fontSize;
                    } else {
                        double n = CalcAnimation(tag.paramsReal[0], style.fontSize, fAnimate);
                        style.fontSize = (n > 0) ? n : org.fontSize;
                    }
                } else {
                    style.fontSize = org.fontSize;
                }
                break;
            case CMD_i: {
                int n = !tag.paramsInt.IsEmpty() ? tag.paramsInt[0] : -1;
                style.fItalic = (n == 0 ? false : n == 1 ? true : org.fItalic);
            }
            break;
            case CMD_kt:
                sub->m_fAnimated2 = true;

                m_kstart = !tag.paramsInt.IsEmpty()
                           ? tag.paramsInt[0] * 10
                           : 0;
                m_kend = m_kstart;
                break;
            case CMD_kf:
            case CMD_K:
                sub->m_fAnimated2 = true;

                m_ktype = 1;
                m_kstart = m_kend;
                m_kend += !tag.paramsInt.IsEmpty()
                          ? tag.paramsInt[0] * 10
                          : 1000;
                break;
            case CMD_ko:
                sub->m_fAnimated2 = true;

                m_ktype = 2;
                m_kstart = m_kend;
                m_kend += !tag.paramsInt.IsEmpty()
                          ? tag.paramsInt[0] * 10
                          : 1000;
                break;
            case CMD_k:
                sub->m_fAnimated2 = true;

                m_ktype = 0;
                m_kstart = m_kend;
                m_kend += !tag.paramsInt.IsEmpty()
                          ? tag.paramsInt[0] * 10
                          : 1000;
                break;
            case CMD_move: // {\move(x1=param[0], y1=param[1], x2=param[2], y2=param[3][, t1=t[0], t2=t[1]])}
                sub->m_fAnimated2 = true;

                if (tag.paramsReal.GetCount() == 4 && !sub->m_effects[EF_MOVE]) {
                    if (Effect* e = DEBUG_NEW Effect) {
                        e->param[0] = (int)(sub->m_scalex * tag.paramsReal[0] * MAX_SUB_PIXEL);
                        e->param[1] = (int)(sub->m_scaley * tag.paramsReal[1] * MAX_SUB_PIXEL);
                        e->param[2] = (int)(sub->m_scalex * tag.paramsReal[2] * MAX_SUB_PIXEL);
                        e->param[3] = (int)(sub->m_scaley * tag.paramsReal[3] * MAX_SUB_PIXEL);
                        e->t[0] = e->t[1] = -1;

                        if (tag.paramsInt.GetCount() == 2) {
                            for (size_t k = 0; k < 2; k++) {
                                e->t[k] = tag.paramsInt[k];
                            }
                        }

                        sub->m_effects[EF_MOVE] = e;
                    }
                }
                break;
            case CMD_org: // {\org(x=param[0], y=param[1])}
                if (tag.paramsReal.GetCount() == 2 && !sub->m_effects[EF_ORG]) {
                    if (Effect* e = DEBUG_NEW Effect) {
                        e->param[0] = (int)(sub->m_scalex * tag.paramsReal[0] * MAX_SUB_PIXEL);
                        e->param[1] = (int)(sub->m_scaley * tag.paramsReal[1] * MAX_SUB_PIXEL);

                        sub->m_effects[EF_ORG] = e;
                    }
                }
                break;
            case CMD_pbo:
                m_polygonBaselineOffset = !tag.paramsInt.IsEmpty() ? tag.paramsInt[0] : 0;
                break;
            case CMD_pos:
                if (tag.paramsReal.GetCount() == 2 && !sub->m_effects[EF_MOVE]) {
                    if (Effect* e = DEBUG_NEW Effect) {
                        e->param[0] = e->param[2] = (int)(sub->m_scalex * tag.paramsReal[0] * MAX_SUB_PIXEL);
                        e->param[1] = e->param[3] = (int)(sub->m_scaley * tag.paramsReal[1] * MAX_SUB_PIXEL);
                        e->t[0] = e->t[1] = 0;

                        sub->m_effects[EF_MOVE] = e;
                    }
                }
                break;
            case CMD_p: {
                int n = !tag.paramsInt.IsEmpty() ? tag.paramsInt[0] : 0;
                m_nPolygon = (n <= 0 ? 0 : n);
            }
            break;
            case CMD_q: {
                int n = !tag.paramsInt.IsEmpty() ? tag.paramsInt[0] : -1;
                sub->m_wrapStyle = (0 <= n && n <= 3)
                                   ? n
                                   : m_defaultWrapStyle;
            }
            break;
            case CMD_r: {
                STSStyle* val;
                style = (!tag.params[0].IsEmpty() && m_styles.Lookup(tag.params[0], val) && val) ? *val : org;
                break;
                }
            case CMD_shad:
                if (!tag.paramsReal.IsEmpty()) {
                    double nx = CalcAnimation(tag.paramsReal[0], style.shadowDepthX, fAnimate);
                    style.shadowDepthX = (nx < 0 ? 0 : nx);
                    double ny = CalcAnimation(tag.paramsReal[0], style.shadowDepthY, fAnimate);
                    style.shadowDepthY = (ny < 0 ? 0 : ny);
                } else {
                    style.shadowDepthX = org.shadowDepthX;
                    style.shadowDepthY = org.shadowDepthY;
                }
                break;
            case CMD_s: {
                int n = !tag.paramsInt.IsEmpty() ? tag.paramsInt[0] : -1;
                style.fStrikeOut = (n == 0 ? false : n == 1 ? true : org.fStrikeOut);
            }
            break;
            case CMD_t: // \t([<t1>,<t2>,][<accel>,]<style modifiers>)
                if (tag.embeded) {
                    sub->m_fAnimated2 = true;
                    m_animStart = m_animEnd = 0;
                    m_animAccel = 1;

                    size_t nParams = tag.paramsInt.GetCount() + tag.paramsReal.GetCount();
                    if (nParams == 1) {
                        m_animAccel = tag.paramsReal[0];
                    } else if (nParams == 2) {
                        m_animStart = (int)tag.paramsReal[0];
                        m_animEnd = (int)tag.paramsReal[1];
                    } else if (nParams == 3) {
                        m_animStart = tag.paramsInt[0];
                        m_animEnd = tag.paramsInt[1];
                        m_animAccel = tag.paramsReal[0];
                    }

                    CreateSubFromSSATag(sub, tag.embeded, style, org, true);

                    sub->m_fAnimated = true;
                }
                break;
            case CMD_u: {
                int n = !tag.paramsInt.IsEmpty() ? tag.paramsInt[0] : -1;
                style.fUnderline = (n == 0 ? false : n == 1 ? true : org.fUnderline);
            }
            break;
            case CMD_xbord:
                if (!tag.paramsReal.IsEmpty()) {
                    double nx = CalcAnimation(tag.paramsReal[0], style.outlineWidthX, fAnimate);
                    style.outlineWidthX = (nx < 0 ? 0 : nx);
                } else {
                    style.outlineWidthX = org.outlineWidthX;
                }
                break;
            case CMD_xshad:
                style.shadowDepthX = !tag.paramsReal.IsEmpty()
                                     ? CalcAnimation(tag.paramsReal[0], style.shadowDepthX, fAnimate)
                                     : org.shadowDepthX;
                break;
            case CMD_ybord:
                if (!tag.paramsReal.IsEmpty()) {
                    double ny = CalcAnimation(tag.paramsReal[0], style.outlineWidthY, fAnimate);
                    style.outlineWidthY = (ny < 0 ? 0 : ny);
                } else {
                    style.outlineWidthY = org.outlineWidthY;
                }
                break;
            case CMD_yshad:
                style.shadowDepthY = !tag.paramsReal.IsEmpty()
                                     ? CalcAnimation(tag.paramsReal[0], style.shadowDepthY, fAnimate)
                                     : org.shadowDepthY;
                break;
        }
    }

    return true;
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
                    CString key = params[i].TrimLeft(L'#');
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
    CSubtitle* sub = m_subtitleCache.GetAt(entry);
    if (sub)
    {
        return sub;
    }
    sub = DEBUG_NEW CSubtitle();
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
    sub->m_wrapStyle    = m_defaultWrapStyle;
    sub->m_fAnimated    = false;
    sub->m_relativeTo   = stss.relativeTo;
    sub->m_scalex       = m_dstScreenSize.cx > 0 ? 1.0 * m_size.cx / (m_dstScreenSize.cx*MAX_SUB_PIXEL) : 1.0;
    sub->m_scaley       = m_dstScreenSize.cy > 0 ? 1.0 * m_size.cy / (m_dstScreenSize.cy*MAX_SUB_PIXEL) : 1.0;

    sub->m_target_scale_x = m_target_scale_x;
    sub->m_target_scale_y = m_target_scale_y;

    m_animStart             =
    m_animEnd               = 0;
    m_animAccel             = 1;
    m_ktype                 = 
    m_kstart                = 
    m_kend                  = 0;
    m_nPolygon              = 0;
    m_polygonBaselineOffset = 0;
    ParseEffect(sub, m_entries.GetAt(entry).effect);

    for (int iStart = 0, iEnd; iStart < str.GetLength(); iStart = iEnd) {
        bool bParsed = false;

        if (str[iStart] == L'{' && (iEnd = str.Find(L'}', iStart)) > 0) {
            AssTagList assTags;
            bParsed = ParseSSATag(assTags, str.Mid(iStart + 1, iEnd - iStart - 1));
            if (bParsed) {
                CreateSubFromSSATag(sub, assTags, stss, orgstss);
                iStart = iEnd + 1;
            }
        } else if (str[iStart] == L'<' && (iEnd = str.Find(L'>', iStart)) > 0) {
            bParsed = ParseHtmlTag(sub, str.Mid(iStart + 1, iEnd - iStart - 1), stss, orgstss);
            if (bParsed) {
                iStart = iEnd + 1;
            }
        }

        if (bParsed) {
            iEnd = FindOneOf(str, L"{<", iStart);
            if (iEnd < 0) {
                iEnd = str.GetLength();
            }
            if (iEnd == iStart) {
                continue;
            }
        } else {
            iEnd = FindOneOf(str, L"{<", iStart + 1);
            if (iEnd < 0) {
                iEnd = str.GetLength();
            }
        }

        STSStyle tmp       = stss;
        tmp.fontSpacing   *=                 sub->m_scalex * 64;
        tmp.fontSize      *=                 sub->m_scaley * 64;
        tmp.outlineWidthX *= (m_fScaledBAS ? sub->m_scalex : 1) * MAX_SUB_PIXEL;
        tmp.outlineWidthY *= (m_fScaledBAS ? sub->m_scaley : 1) * MAX_SUB_PIXEL;
        tmp.shadowDepthX  *= (m_fScaledBAS ? sub->m_scalex : 1) * MAX_SUB_PIXEL;
        tmp.shadowDepthY  *= (m_fScaledBAS ? sub->m_scaley : 1) * MAX_SUB_PIXEL;
        FwSTSStyle fw_tmp(tmp);
        if(m_nPolygon)
        {
            ParsePolygon(sub, str.Mid(iStart, iEnd - iStart), fw_tmp);
        }
        else
        {
            ParseString(sub, str.Mid(iStart, iEnd - iStart), fw_tmp);
        }
    }

    if( sub->m_effects[EF_BANNER] || sub->m_effects[EF_SCROLL] )
        sub->m_fAnimated2 = true;
    // just a "work-around" solution... in most cases nobody will want to use \org together with moving but without rotating the subs
    if(sub->m_effects[EF_ORG] && (sub->m_effects[EF_MOVE] || sub->m_effects[EF_BANNER] || sub->m_effects[EF_SCROLL]))
        sub->m_fAnimated = true;
    sub->m_scrAlignment = abs(sub->m_scrAlignment);
    STSEntry stse = m_entries.GetAt(entry);
    CRect marginRect = stse.marginRect;
    if(marginRect.left   == 0) marginRect.left   = orgstss.marginRect.get().left;
    if(marginRect.top    == 0) marginRect.top    = orgstss.marginRect.get().top;
    if(marginRect.right  == 0) marginRect.right  = orgstss.marginRect.get().right;
    if(marginRect.bottom == 0) marginRect.bottom = orgstss.marginRect.get().bottom;
    marginRect.left   = (int)(sub->m_scalex*marginRect.left  *MAX_SUB_PIXEL);
    marginRect.top    = (int)(sub->m_scaley*marginRect.top   *MAX_SUB_PIXEL);
    marginRect.right  = (int)(sub->m_scalex*marginRect.right *MAX_SUB_PIXEL);
    marginRect.bottom = (int)(sub->m_scaley*marginRect.bottom*MAX_SUB_PIXEL);

    sub->CreateClippers(m_size, m_video_rect.Size());
    sub->MakeLines(m_size, marginRect);
    if (!sub->m_fAnimated)
    {
        if (!m_subtitleCache.SetAt(entry, sub))
        {
            XY_LOG_FATAL("Out of Memory!");
            delete sub;
            return NULL;
        }
        m_subtitleCacheEntry.AddTail(entry);
    }
    return(sub);
}

void CRenderedTextSubtitle::ClearUnCachedSubtitle( CSubtitle2List& sub2List )
{
    POSITION pos = sub2List.GetHeadPosition();
    while(pos)
    {
        CSubtitle2 & sub2 = sub2List.GetNext(pos);
        if (sub2.s->m_fAnimated)
        {
            delete sub2.s;
        }
    }
}

void CRenderedTextSubtitle::ShrinkCache()
{
    OverlayNoBlurMruCache *cache1 = CacheManager::GetOverlayNoBlurMruCache();
    OverlayMruCache       *cache2 = CacheManager::GetOverlayMruCache();
    BitmapMruCache        *cache3 = CacheManager::GetBitmapMruCache();
    while (g_xy_malloc_used_size > s_max_cache_size && 
        (cache1->GetCurItemNum()>0 || cache2->GetCurItemNum()>0 || cache3->GetCurItemNum()>0))
    {
        cache1->RemoveTail();
        cache2->RemoveTail();
        cache3->RemoveTail();
    }
}

//

STDMETHODIMP CRenderedTextSubtitle::NonDelegatingQueryInterface(REFIID riid, void** ppv)
{
    CheckPointer(ppv, E_POINTER);
    *ppv = NULL;
    return
        QI(IPersist)
        QI(ISubStream)
        QI(ISubPicProviderEx2)
        QI(ISubPicProvider)
        QI(ISubPicProviderEx)
        __super::NonDelegatingQueryInterface(riid, ppv);
}

// ISubPicProvider

STDMETHODIMP_(POSITION) CRenderedTextSubtitle::GetStartPosition(REFERENCE_TIME rt, double fps)
{
    m_fps = fps;

    int iSegment;
    rt /= 10000i64;
    const STSSegment *stss = SearchSubs((int)rt, fps, &iSegment, NULL);
    if(stss==NULL) {
        TRACE_PARSER("No subtitle at "<<XY_LOG_VAR_2_STR(rt));
        return NULL;
    }
    return (POSITION)(iSegment + 1);
}

STDMETHODIMP_(POSITION) CRenderedTextSubtitle::GetNext(POSITION pos)
{
    int iSegment = (int)pos;
    const STSSegment *stss = GetSegment(iSegment);
    while(stss && stss->subs.GetCount() == 0) {
        iSegment++;
        stss = GetSegment(iSegment);
    }
    return(stss ? (POSITION)(iSegment+1) : NULL);
}

STDMETHODIMP_(REFERENCE_TIME) CRenderedTextSubtitle::GetStart(POSITION pos, double fps)
{
    return(10000i64 * TranslateSegmentStart((int)pos-1, fps));
}

STDMETHODIMP_(REFERENCE_TIME) CRenderedTextSubtitle::GetStop(POSITION pos, double fps)
{
	return(10000i64 * TranslateSegmentEnd((int)pos-1, fps));
}

//@start, @stop: -1 if segment not found; @stop may < @start if subIndex exceed uppper bound
STDMETHODIMP_(VOID) CRenderedTextSubtitle::GetStartStop(POSITION pos, double fps, /*out*/REFERENCE_TIME &start, /*out*/REFERENCE_TIME &stop)
{
    int iSegment = (int)pos-1;
    int tempStart, tempEnd;
    TranslateSegmentStartEnd(iSegment, fps, tempStart, tempEnd);
    start = tempStart;
    stop = tempEnd;
}

STDMETHODIMP_(bool) CRenderedTextSubtitle::IsAnimated(POSITION pos)
{
    unsigned int iSegment = (int)pos-1;
    if(iSegment<m_segments.GetCount())
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

HRESULT CRenderedTextSubtitle::ParseScript(REFERENCE_TIME rt, double fps, CSubtitle2List *outputSub2List )
{
    TRACE_RENDERER_REQUEST("Begin search subtitle segment");
    //fix me: check input and log error
    int t = (int)(rt / 10000);
    int segment;
    //const
    STSSegment* stss = SearchSubs2(t, fps, &segment);
    if(!stss) return S_FALSE;
    // clear any cached subs that has been passed
    {
        TRACE_RENDERER_REQUEST("Begin clear parsed subtitle cache. m_subtitleCache.size:"<<m_subtitleCacheEntry.GetCount());
        POSITION pos = m_subtitleCacheEntry.GetHeadPosition();
        while(pos)
        {
            POSITION pos_old = pos;
            int key = m_subtitleCacheEntry.GetNext(pos);
            STSEntry& stse = m_entries.GetAt(key);
            if(stse.end <= t)
            {
                delete m_subtitleCache.GetAt(key);
                m_subtitleCache.SetAt(key, NULL);
                m_subtitleCacheEntry.RemoveAt(pos_old);
            }
        }
    }
    m_sla.AdvanceToSegment(segment, stss->subs);
    TRACE_RENDERER_REQUEST("Begin copy LSub. subs.size:"<<stss->subs.GetCount());
    CAtlArray<LSub> subs;
    for(int i = 0, j = stss->subs.GetCount(); i < j; i++)
    {
        LSub ls;
        ls.idx       = stss->subs[i];
        ls.layer     = m_entries.GetAt(stss->subs[i]).layer;
        ls.readorder = m_entries.GetAt(stss->subs[i]).readorder;
        subs.Add(ls);
    }
    TRACE_RENDERER_REQUEST("Begin sort LSub.");
    qsort(subs.GetData(), subs.GetCount(), sizeof(LSub), lscomp);
    TRACE_RENDERER_REQUEST("Begin parse subs.");
    for(int i = 0, j = subs.GetCount(); i < j; i++)
    {
        int entry = subs[i].idx;
        STSEntry stse = m_entries.GetAt(entry);
        {
            int start = TranslateStart(entry, fps);
            m_time    = t - start;
            m_delay   = TranslateEnd(entry, fps) - start;
        }
        CSubtitle* s = GetSubtitle(entry);
        if(!s) continue;
        stss->animated |= s->m_fAnimated2;
        CRect clipRect  = s->m_clip & CRect(0,0, (m_size.cx>>3), (m_size.cy>>3));
        CRect r         = s->m_rect;
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
                    if(t2 < t1)            {int t = t1; t1 = t2; t2 = t;      }
                    if(t1 <= 0 && t2 <= 0) {            t1 = 0;  t2 = m_delay;}
                    if(m_time <= t1)  p = p1;
                    else if(p1 == p2) p = p1;
                    else if(t1 < m_time && m_time < t2)
                    {
                        double t = 1.0*(m_time-t1)/(t2-t1);
                        p.x = (int)((1-t)*p1.x + t*p2.x);
                        p.y = (int)((1-t)*p1.y + t*p2.y);
                    }
                    else p = p2;
                    int x = (s->m_scrAlignment%3) == 1 ? p.x : 
                            (s->m_scrAlignment%3) == 0 ? p.x - spaceNeeded.cx :
                                                         p.x - (spaceNeeded.cx+1)/2;
                    int y =  s->m_scrAlignment <= 3    ? p.y - spaceNeeded.cy : 
                             s->m_scrAlignment <= 6    ? p.y - (spaceNeeded.cy+1)/2 :
                                                         p.y;
                    r = CRect(CPoint(x,y), spaceNeeded);
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
                    int left = 0,
                        right = m_size.cx;
                    r.left = !!s->m_effects[k]->param[1] 
                        ? (left  /*marginRect.left*/ - spaceNeeded.cx) + (int)(m_time*MAX_SUB_PIXEL_F/s->m_effects[k]->param[0])
                        : (right /*marginRect.right*/)                 - (int)(m_time*MAX_SUB_PIXEL_F/s->m_effects[k]->param[0]);
                    r.right = r.left + spaceNeeded.cx;
                    clipRect &= CRect(left>>3, clipRect.top, right>>3, clipRect.bottom);
                    fPosOverride = true;
                }
                break;
            case EF_SCROLL: // Scroll up/down(toptobottom=param[3]);top=param[0];bottom=param[1];delay=param[2][;fadeawayheight=param[4]]
                {
                    r.top = !!s->m_effects[k]->param[3]
                        ? s->m_effects[k]->param[0] + (int)(m_time*MAX_SUB_PIXEL_F/s->m_effects[k]->param[2]) - spaceNeeded.cy
                        : s->m_effects[k]->param[1] - (int)(m_time*MAX_SUB_PIXEL_F/s->m_effects[k]->param[2]);
                    r.bottom = r.top + spaceNeeded.cy;
                    CRect cr(0, (s->m_effects[k]->param[0] + 4) >> 3, (m_size.cx>>3), (s->m_effects[k]->param[1] + 4) >> 3);
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
        org.x = (s->m_scrAlignment%3) == 1 ? r.left   : 
                (s->m_scrAlignment%3) == 2 ? r.CenterPoint().x 
                                           : r.right;
        org.y =  s->m_scrAlignment <= 3    ? r.bottom : 
                 s->m_scrAlignment <= 6    ? r.CenterPoint().y 
                                           : r.top;
        if(!fOrgOverride) org2 = org;
        CPoint p2(0, r.top);
        // Rectangles for inverse clip

        CRectCoor2 clipRect_coor2;
        clipRect_coor2.left   = clipRect.left   * m_target_scale_x;
        clipRect_coor2.right  = clipRect.right  * m_target_scale_x;
        clipRect_coor2.top    = clipRect.top    * m_target_scale_y;
        clipRect_coor2.bottom = clipRect.bottom * m_target_scale_y;
        CSubtitle2& sub2 = outputSub2List->GetAt(outputSub2List->AddTail( CSubtitle2(s, clipRect_coor2, org, org2, p2, alpha, m_time) ));
    }

    return (subs.GetCount()) ? S_OK : S_FALSE;
}

STDMETHODIMP CRenderedTextSubtitle::RenderEx(SubPicDesc& spd, REFERENCE_TIME rt, double fps, CAtlList<CRectCoor2>& rectList)
{
    CSize output_size = CSize(spd.w,spd.h);
    CRectCoor2 video_rect = CRect(0,0,spd.w,spd.h);
    rectList.RemoveAll();
    if (spd.vidrect.left!=0 || spd.vidrect.top!=0 || spd.vidrect.right!=spd.w || spd.vidrect.bottom!=spd.h)
    {
        XY_LOG_WARN("Video rectangle is different from window. But support for relative to video rectangle has been removed.");
    }

    CComPtr<IXySubRenderFrame> sub_render_frame;
    HRESULT hr = RenderEx(&sub_render_frame, spd.type, video_rect, video_rect, output_size, rt, fps);
    if (SUCCEEDED(hr) && sub_render_frame)
    {
        int count = 0;
        hr = sub_render_frame->GetBitmapCount(&count);
        if(FAILED(hr))
        {
            return hr;
        }
        int color_space;
        hr = sub_render_frame->GetXyColorSpace(&color_space);
        if(FAILED(hr))
        {
            return hr;
        }
        for (int i=0;i<count;i++)
        {
            POINT pos;
            SIZE size;
            LPCVOID pixels;
            int pitch;
            hr = sub_render_frame->GetBitmap(i, NULL, &pos, &size, &pixels, &pitch );
            if(FAILED(hr))
            {
                return hr;
            }
            rectList.AddTail(CRect(pos, size));
            if (color_space==XY_CS_AYUV_PLANAR)
            {
                XyPlannerFormatExtra plans;
                hr = sub_render_frame->GetBitmapExtra(i, &plans);
                if(FAILED(hr))
                {
                    return hr;
                }
                XyBitmap::AlphaBltPlannar(spd, pos, size, plans, pitch);
            }
            else
            {
                XyBitmap::AlphaBltPack(spd, pos, size, pixels, pitch);
            }
        }
    }
    return (!rectList.IsEmpty()) ? S_OK : S_FALSE;
}

STDMETHODIMP CRenderedTextSubtitle::RenderEx( IXySubRenderFrame**subRenderFrame, int spd_type,
    const RECT& video_rect, const RECT& subtitle_target_rect,
    const SIZE& original_video_size,
    REFERENCE_TIME rt, double fps )
{
    TRACE_RENDERER_REQUEST("Begin RenderEx rt"<<rt);
    if (!subRenderFrame)
    {
        return S_FALSE;
    }
    *subRenderFrame = NULL;
    CRect cvideo_rect = video_rect;

    cvideo_rect &= subtitle_target_rect;
    if (cvideo_rect!=video_rect)
    {
        XY_LOG_WARN("NOT supported yet!");
        return E_NOTIMPL;
    }

    if (cvideo_rect.left!=0 || cvideo_rect.top!=0)
    {
        XY_LOG_WARN("FIXME: supported with hack");
        cvideo_rect.MoveToXY(0,0);
    }

    XyColorSpace color_space = XY_CS_ARGB;
    switch(spd_type)
    {
    case MSP_AYUV_PLANAR:
        color_space = XY_CS_AYUV_PLANAR;
        break;
    case MSP_XY_AUYV:
        color_space = XY_CS_AUYV;
        break;
    case MSP_AYUV:
        color_space = XY_CS_AYUV;
        break;
    case MSP_RGBA_F:
        color_space = XY_CS_ARGB_F;
        break;
    default:
        color_space = XY_CS_ARGB;
        break;
    }

    XySubRenderFrameCreater *render_frame_creater = XySubRenderFrameCreater::GetDefaultCreater();
    render_frame_creater->SetColorSpace(color_space);

    if( m_video_rect != CRect(cvideo_rect.left*MAX_SUB_PIXEL,
                              cvideo_rect.top*MAX_SUB_PIXEL,
                              cvideo_rect.right*MAX_SUB_PIXEL,
                              cvideo_rect.bottom*MAX_SUB_PIXEL)
        || m_subtitle_target_rect != CRect(subtitle_target_rect.left*MAX_SUB_PIXEL,
                                           subtitle_target_rect.top*MAX_SUB_PIXEL,
                                           subtitle_target_rect.right*MAX_SUB_PIXEL,
                                           subtitle_target_rect.bottom*MAX_SUB_PIXEL)
        || m_size != CSize(original_video_size.cx*MAX_SUB_PIXEL, original_video_size.cy*MAX_SUB_PIXEL) )
    {
        if (!Init(cvideo_rect, subtitle_target_rect, original_video_size))
        {
            XY_LOG_FATAL("Failed to Init.");
            return E_FAIL;
        }

        render_frame_creater->SetOutputRect(cvideo_rect);
        render_frame_creater->SetClipRect(subtitle_target_rect);
    }

    TRACE_RENDERER_REQUEST("Begin ParseScript");
    CSubtitle2List sub2List;
    HRESULT hr = ParseScript(rt, fps, &sub2List);
    if(hr!=S_OK)
    {
        return hr;
    }

    if (!m_simple)
    {
        bool newMovable = true;
        POSITION pos=sub2List.GetHeadPosition();
        while ( pos!=NULL )
        {
            const CSubtitle2& sub2 = sub2List.GetNext(pos);
            if (sub2.s->m_hard_position_level > POS_LVL_NONE)
            {
              newMovable = false;
              break;
            }
        }
        m_movable = newMovable;
    }

    TRACE_RENDERER_REQUEST("Begin build draw item tree");
    CRectCoor2 margin_rect(
        m_video_rect.left - m_subtitle_target_rect.left,
        m_video_rect.top  - m_subtitle_target_rect.top,
        m_subtitle_target_rect.right - m_video_rect.right,
        m_subtitle_target_rect.bottom - m_video_rect.bottom);
    CompositeDrawItemListList compDrawItemListList;
    DoRender(m_video_rect.Size(), m_video_rect.TopLeft(), margin_rect, sub2List, &compDrawItemListList);
    ClearUnCachedSubtitle(sub2List);

    TRACE_RENDERER_REQUEST("Begin Draw");
    XySubRenderFrame *sub_render_frame;
    CompositeDrawItem::Draw(&sub_render_frame, compDrawItemListList);
    sub_render_frame->MoveTo(video_rect.left, video_rect.top);
    (*subRenderFrame = sub_render_frame)->AddRef();

    ShrinkCache();

    TRACE_RENDERER_REQUEST("Finished");
    return hr;
}

void CRenderedTextSubtitle::DoRender( const SIZECoor2& output_size, 
    const POINTCoor2& video_org,
    const RECTCoor2& margin_rect, 
    const CSubtitle2List& sub2List, 
    CompositeDrawItemListList *compDrawItemListList /*output*/)
{
    //check input and log error
    POSITION pos=sub2List.GetHeadPosition();
    while ( pos!=NULL )
    {
        const CSubtitle2& sub2 = sub2List.GetNext(pos);
        CompositeDrawItemList& compDrawItemList = compDrawItemListList->GetAt(compDrawItemListList->AddTail());
        RenderOneSubtitle(output_size, video_org, margin_rect, sub2, &compDrawItemList);
    }
}

void CRenderedTextSubtitle::RenderOneSubtitle( const SIZECoor2& output_size, 
    const POINTCoor2& video_org, const RECTCoor2& margin_rect, 
    const CSubtitle2& sub2, 
    CompositeDrawItemList* compDrawItemList /*output*/)
{
    CSubtitle   * s        = sub2.s;
    CRect         clipRect = sub2.clipRect;
    const CPoint& org      = sub2.org;
    const CPoint& org2     = sub2.org2;
    const CPoint& p2       = sub2.p;
    int           alpha    = sub2.alpha;
    int           time     = sub2.time;
    if(!s) return;

    CPointCoor2 margin;
    if (s->m_hard_position_level > POS_LVL_NONE)
    {
        margin = video_org;
    }
    else
    {
        //set margin to move subtitles to black bars
        switch(s->m_scrAlignment%3)
        {
        case 1: margin.x = video_org.x - margin_rect.left;  break; //move to left
        case 2: margin.x = video_org.x;                     break; //do not move so that it aligns with the middle of the video
        case 3: margin.x = video_org.x + margin_rect.right; break; //move to right
        }
        switch((s->m_scrAlignment-1)/3)
        {
        case 0: margin.y = video_org.y - margin_rect.top;    break; //move to top
        case 1: margin.y = video_org.y;                      break; //do not move so that it aligns with the middle of the video
        case 2: margin.y = video_org.y + margin_rect.bottom; break;//move to bottom
        }
        ASSERT(clipRect.Width()*MAX_SUB_PIXEL==output_size.cx && clipRect.Height()*MAX_SUB_PIXEL==output_size.cy);
        clipRect.SetRect(0,0, 
            (output_size.cx + video_org.x+margin_rect.left+margin_rect.right)>>3,
            (output_size.cy + video_org.y+margin_rect.top +margin_rect.bottom)>>3);
    }

    SharedPtrCClipperPaintMachine clipper( DEBUG_NEW CClipperPaintMachine(s->m_pClipper) );

    CRect iclipRect[4];
    iclipRect[0] = CRect(0             , 0              , output_size.cx>>3, clipRect.top     );
    iclipRect[1] = CRect(0             , clipRect.top   , clipRect.left    , clipRect.bottom  );
    iclipRect[2] = CRect(clipRect.right, clipRect.top   , output_size.cx>>3, clipRect.bottom  );
    iclipRect[3] = CRect(0             , clipRect.bottom, output_size.cx>>3, output_size.cy>>3);
    CRect bbox2(0,0,0,0);
    POSITION pos = s->GetHeadLinePosition();
    CPoint p = p2;
    while(pos)
    {
        CLine* l = s->GetNextLine(pos);
        p.x = (s->m_scrAlignment%3) == 1 ? org.x :
              (s->m_scrAlignment%3) == 0 ? org.x -  l->m_width :
                                           org.x - (l->m_width/2);

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
            bbox2 |= l->PaintAll(&tmp1, iclipRect[0], margin, clipper, p, org2, time, alpha);
            bbox2 |= l->PaintAll(&tmp2, iclipRect[1], margin, clipper, p, org2, time, alpha);
            bbox2 |= l->PaintAll(&tmp3, iclipRect[2], margin, clipper, p, org2, time, alpha);
            bbox2 |= l->PaintAll(&tmp4, iclipRect[3], margin, clipper, p, org2, time, alpha);
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
            bbox2 |= l->PaintAll(&tmpCompDrawItemList, clipRect, margin, clipper, p, org2, time, alpha);
        }
        compDrawItemList->AddTailList(&tmpCompDrawItemList);
        p.y += l->m_ascent + l->m_descent;
    }
}

STDMETHODIMP CRenderedTextSubtitle::Render(SubPicDesc& spd, REFERENCE_TIME rt, double fps, RECTCoor2& bbox)
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
        *ppName = (WCHAR*)CoTaskMemAlloc((m_name.GetLength()+1)*sizeof(WCHAR));
        if(!(*ppName))
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

STDMETHODIMP_(bool) CRenderedTextSubtitle::IsMovable()
{
    return m_movable;
}

STDMETHODIMP_(bool) CRenderedTextSubtitle::IsSimple()
{
    return m_simple;
}

STDMETHODIMP CRenderedTextSubtitle::Lock()
{
    return CSubPicProviderImpl::Lock();
}

STDMETHODIMP CRenderedTextSubtitle::Unlock()
{
    return CSubPicProviderImpl::Unlock();
}
