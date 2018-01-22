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

#pragma once

#include "STS.h"
#include "Rasterizer.h"
#include "../SubPic/SubPicProviderImpl.h"
#include "../SubPic/ISimpleSubPic.h"
#include <atlcoll.h>
#include <boost/flyweight/key_value.hpp>
#include <boost/smart_ptr.hpp>
#include "mru_cache.h"
#include "xy_int_map.h"

struct AssTag;
typedef ::boost::shared_ptr<CAtlList<AssTag>> AssTagList;

typedef CTagCache<CStringW, AssTagList, CStringElementTraits<CStringW>> CAssTagsCache;

struct TagCache {
    CAssTagsCache AssTagsCache;

    TagCache()
        : AssTagsCache(2048) {}
};

//how hard positioned is the ass cmd 
enum AssCmdPosLevel
{
    POS_LVL_NONE,
    POS_LVL_SOFT,
    POS_LVL_HARD
};

class CMyFont : public CFont
{
public:
    int m_ascent, m_descent;

    CMyFont(const STSStyleBase& style);
};

typedef ::boost::flyweights::flyweight<::boost::flyweights::key_value<STSStyleBase, CMyFont>, ::boost::flyweights::no_locking> FwCMyFont;

class CPolygon;

class CClipper;
typedef ::boost::shared_ptr<CClipper> SharedPtrCClipper;

struct CompositeDrawItem;
typedef CAtlList<CompositeDrawItem> CompositeDrawItemList;
typedef CAtlList<CompositeDrawItemList> CompositeDrawItemListList;

class CWord;
typedef CWord* PCWord;
typedef ::boost::shared_ptr<CWord> SharedPtrCWord;
typedef ::boost::shared_ptr<CPolygon> SharedPtrCPolygon;

class CClipperPaintMachine;
typedef ::boost::shared_ptr<CClipperPaintMachine> SharedPtrCClipperPaintMachine;

class OverlayKey;
interface IXySubRenderFrame;

class CWord
{
    bool NeedTransform  ();
    void Transform      (PathData* path_data, const CPointCoor2& org);
//  void Transform_C    (PathData* path_data, const CPointCoor2 &org );
//  void Transform_SSE2 (PathData* path_data, const CPointCoor2 &org );
    bool CreateOpaqueBox();

protected:
    virtual bool CreatePath(PathData* path_data) = 0;

    bool DoPaint(const CPointCoor2& p, const CPointCoor2& trans_org, SharedPtrOverlay* overlay, const OverlayKey& key);
public:
    // str[0] = 0 -> m_fLineBreak = true (in this case we only need and use the height of m_font from the whole class)
    CWord(const FwSTSStyle& style, const CStringW& str, int ktype, int kstart, int kend
        , double target_scale_x=1.0, double target_scale_y=1.0
        , bool round_to_whole_pixel_after_scale_to_target = false);
    CWord(const CWord&);
    virtual ~CWord();

    virtual SharedPtrCWord Copy() = 0;
    virtual bool Append(const SharedPtrCWord& w);
    
    bool operator==(const CWord& rhs)const;

    static void PaintFromOverlay   (const CPointCoor2& p, const CPointCoor2& trans_org2, OverlayKey &subpixel_variance_key, SharedPtrOverlay& overlay);
    void PaintFromNoneBluredOverlay(SharedPtrOverlay raterize_result, const OverlayKey& overlay_key, SharedPtrOverlay* overlay);
    bool PaintFromScanLineData2    (const CPointCoor2& psub, const ScanLineData2& scan_line_data2, const OverlayKey& key    , SharedPtrOverlay* overlay);
    bool PaintFromPathData         (const CPointCoor2& psub, const CPointCoor2& trans_org, const PathData& path_data, const OverlayKey& key, SharedPtrOverlay* overlay );
    bool PaintFromRawData          (const CPointCoor2& psub, const CPointCoor2& trans_org, const OverlayKey& key    , SharedPtrOverlay* overlay );

protected:
    XyFwStringW       m_str;
public:
    bool              m_fWhiteSpaceChar, m_fLineBreak;
    FwSTSStyle        m_style;
    SharedPtrCPolygon m_pOpaqueBox;
    int               m_ktype, m_kstart, m_kend;
    int               m_width, m_ascent, m_descent;
    double            m_target_scale_x, m_target_scale_y;
    bool              m_round_to_whole_pixel_after_scale_to_target;//it is necessary to avoid some artifacts

    //friend class CWordCache;
    friend class PathDataCacheKey;
    friend class ClipperTraits;
    friend class ClipperAlphaMaskCacheKey;
    friend class CWordPaintMachine;
    friend class CWordPaintResultKey;
    friend class CClipper;
};

class CText : public CWord
{
public:
    struct TextInfo
    {
        int m_width, m_ascent, m_descent;
    };
    typedef ::boost::shared_ptr<TextInfo> SharedPtrTextInfo;
protected:
    virtual bool CreatePath(PathData* path_data);

    static void GetTextInfo(TextInfo *output, const FwSTSStyle& style, const CStringW& str);
public:
    CText(const FwSTSStyle& style, const CStringW& str, int ktype, int kstart, int kend
        , double target_scale_x=1.0, double target_scale_y=1.0);
    CText(const CText& src);

    virtual SharedPtrCWord Copy();
    virtual bool Append(const SharedPtrCWord& w);
};

class CPolygon : public CWord
{
    bool Get6BitFixedPoint(CStringW& str, LONG& ret);
    bool GetPOINT(CStringW& str, POINT& ret);
    bool ParseStr();

protected:
    double            m_scalex, m_scaley;
    int               m_baseline;
    CAtlArray<BYTE>   m_pathTypesOrg;
    CAtlArray<CPoint> m_pathPointsOrg;

    virtual bool CreatePath(PathData* path_data);

public:
    CPolygon(const FwSTSStyle& style, const CStringW& str, int ktype, int kstart, int kend
        , double scalex, double scaley, int baseline
        , double target_scale_x=1.0, double target_scale_y=1.0
        , bool round_to_whole_pixel_after_scale_to_target = false);
	// can't use a const reference because we need to use CAtlArray::Copy which expects a non-const reference
    CPolygon(CPolygon&); 
    virtual ~CPolygon();

    virtual SharedPtrCWord Copy();
    virtual bool Append(const SharedPtrCWord& w);

    friend class ClipperTraits;
    friend class ClipperAlphaMaskCacheKey;
    friend class PathDataCacheKey;
};

enum AssCmdType {
    // /!\ Keep those four grouped together in that order
    CMD_1c,
    CMD_2c,
    CMD_3c,
    CMD_4c,
    // /!\ Keep those four grouped together in that order
    CMD_1a,
    CMD_2a,
    CMD_3a,
    CMD_4a,
    CMD_alpha,
    CMD_an,
    CMD_a,
    CMD_blur,
    CMD_bord,
    CMD_be,
    CMD_b,
    CMD_clip,
    CMD_c,
    CMD_fade,
    CMD_fe,
    CMD_fn,
    CMD_frx,
    CMD_fry,
    CMD_frz,
    CMD_fax,
    CMD_fay,
    CMD_fr,
    CMD_fscx,
    CMD_fscy,
    CMD_fsc,
    CMD_fsp,
    CMD_fs,
    CMD_iclip,
    CMD_i,
    CMD_kt,
    CMD_kf,
    CMD_ko,
    CMD_k,
    CMD_K,
    CMD_move,
    CMD_org,
    CMD_pbo,
    CMD_pos,
    CMD_p,
    CMD_q,
    CMD_r,
    CMD_shad,
    CMD_s,
    CMD_t,
    CMD_u,
    CMD_xbord,
    CMD_xshad,
    CMD_ybord,
    CMD_yshad,
    CMD_COUNT
};

#define CMD_MIN_LENGTH 1
#define CMD_MAX_LENGTH 5

struct AssTag {
    AssCmdType cmdType;
    CAtlArray<CStringW, CStringElementTraits<CStringW>> params;
    CAtlArray<int> paramsInt;
    CAtlArray<double> paramsReal;
    AssTagList embeded;

    AssTag() : cmdType(CMD_COUNT) {};

    AssTag(const AssTag& tag)
        : cmdType(tag.cmdType)
        , params()
        , paramsInt()
        , paramsReal()
        , embeded(tag.embeded) {
        params.Copy(tag.params);
        paramsInt.Copy(tag.paramsInt);
        paramsReal.Copy(tag.paramsReal);
    }
};

enum eftype {
    EF_MOVE = 0,    // {\move(x1=param[0], y1=param[1], x2=param[2], y2=param[3], t1=t[0], t2=t[1])} or {\pos(x=param[0], y=param[1])}
    EF_ORG,         // {\org(x=param[0], y=param[1])}
    EF_FADE,        // {\fade(a1=param[0], a2=param[1], a3=param[2], t1=t[0], t2=t[1], t3=t[2], t4=t[3])} or {\fad(t1=t[1], t2=t[2])
    EF_BANNER,      // Banner;delay=param[0][;lefttoright=param[1];fadeawaywidth=param[2]]
    EF_SCROLL       // Scroll up/down=param[3];top=param[0];bottom=param[1];delay=param[2][;fadeawayheight=param[4]]
};

#define EF_NUMBEROFEFFECTS 5

class Effect
{
public:
    Effect()
    {
        memset(param, 0, sizeof(param));
        memset(t, 0, sizeof(t));
    }
    bool operator==(const Effect& rhs)const
    {
        return type==rhs.type 
            && !memcmp(param, rhs.param, sizeof(param))
            && !memcmp(t, rhs.t, sizeof(t));
    }
public:
    enum eftype type;
    int param[8];
    int t[4];
};

class CClipper
{
private:
    SharedPtrCPolygon m_polygon;
    CSizeCoor2        m_size;
    bool              m_inverse;
    Effect            m_effect;
    int               m_effectType;
    bool              m_painted;

    GrayImage2* PaintSimpleClipper();
    GrayImage2* PaintBaseClipper();
    GrayImage2* PaintBannerClipper();
    GrayImage2* PaintScrollClipper();
    
    GrayImage2* Paint();
public:
    CClipper(CStringW str, CSizeCoor2 size, double scalex, double scaley, bool inverse
        , double target_scale_x=1.0, double target_scale_y=1.0);
    void SetEffect(const Effect& effect, int effectType);
    virtual ~CClipper();

    static SharedPtrGrayImage2 GetAlphaMask(const SharedPtrCClipper& clipper);

    friend class ClipperTraits;
    friend class ClipperAlphaMaskCacheKey;
    friend class CClipperPaintMachine;
};

class CLine: private CAtlList<SharedPtrCWord>
{
public:
    int m_width, m_ascent, m_descent, m_borderX, m_borderY;

    virtual ~CLine();

    void Compact();

    int GetWordCount();
    void AddWord2Tail(SharedPtrCWord words);
    bool IsEmpty();

    CRectCoor2 PaintAll(CompositeDrawItemList* output, const CRectCoor2& clipRect, 
        const CPointCoor2& margin,
        const SharedPtrCClipperPaintMachine &clipper, 
        CPoint p, const CPoint& org, const int time, const int alpha);
};

class CSubtitle: private CAtlList<CLine*>
{
    int GetFullWidth();
    int GetFullLineWidth(POSITION pos);
    int GetWrapWidth(POSITION pos, int maxwidth);
    CLine* GetNextLine(POSITION& pos, int maxwidth);

public:
    int                      m_scrAlignment;
    int                      m_wrapStyle;
    bool                     m_fAnimated;
    bool                     m_fAnimated2; //If this Subtitle has animate effect
    int                      m_relativeTo;
    Effect*                  m_effects[EF_NUMBEROFEFFECTS];
    CAtlList<SharedPtrCWord> m_words;
    SharedPtrCClipper        m_pClipper;
    CRect                    m_rect, m_clip;
    int                      m_topborder, m_bottomborder;
    bool                     m_clipInverse;
    double                   m_scalex, m_scaley;
    double                   m_target_scale_x, m_target_scale_y;
    int                      m_hard_position_level;//-1: for no style override at all, others see @AssCmdPosLevel
public:
    CSubtitle();
    virtual ~CSubtitle();
    virtual void Empty();

    void CreateClippers(CSize size1, const CSizeCoor2& size_scale_to);

    void MakeLines(CSize size, CRect marginRect);

    POSITION GetHeadLinePosition();
    CLine* GetNextLine(POSITION& pos);
};

struct CSubtitle2
{
    CSubtitle2():s(NULL){}

    CSubtitle2(CSubtitle* s_,const CRectCoor2& clipRect_, const CPoint& org_, const CPoint& org2_
        , const CPoint& p_, int alpha_, int time_)
        : s(s_), clipRect(clipRect_), org(org_), org2(org2_), p(p_), alpha(alpha_), time(time_)
    {

    }

    CSubtitle       *s;
    const CRectCoor2 clipRect;
    const CPoint     org;
    const CPoint     org2;
    const CPoint     p;
    int              alpha; 
    int              time;
};

typedef CAtlList<CSubtitle2> CSubtitle2List;

class CScreenLayoutAllocator
{
    typedef struct
    {
        CRect r;
        int segment, entry, layer;
    } SubRect;

    CAtlList<SubRect> m_subrects;

public:
    virtual void Empty();

    void AdvanceToSegment(int segment, const CAtlArray<int>& sa);
    CRect AllocRect(CSubtitle* s, int segment, int entry, int layer, int collisions);
};

[uuid("537DCACA-2812-4a4f-B2C6-1A34C17ADEB0")]
class CRenderedTextSubtitle : public CSubPicProviderImpl, public ISubStream, public ISubPicProviderEx2, public CSimpleTextSubtitle
{

    static CAtlArray<AssCmdPosLevel> m_cmd_pos_level;

    static CAtlMap<CStringW, AssCmdType, CStringElementTraits<CStringW>> m_cmdMap;

    TagCache m_tagCache;
public:
    static std::size_t SetMaxCacheSize(std::size_t max_cache_size);

private:
    XyIntMap<CSubtitle*>     m_subtitleCache;
    CAtlList<int>            m_subtitleCacheEntry;

    CScreenLayoutAllocator   m_sla;

    CRectCoor2               m_video_rect;//scaled video rect
    CRectCoor2               m_subtitle_target_rect;//NOT really supported yet

    CSize                    m_size;

    // temp variables, used when parsing the script
    int                      m_time, m_delay;
    int                      m_animStart, m_animEnd;
    double                   m_animAccel;
    int                      m_ktype, m_kstart, m_kend;
    int                      m_nPolygon;
    int                      m_polygonBaselineOffset;
    double                   m_fps;
    int                      m_period;//1000/m_fps
    double                   m_target_scale_x, m_target_scale_y;
    bool                     m_movable;

    static void InitCmdMap();

    void ParseEffect (CSubtitle* sub, const CString& str);
    void ParseString (CSubtitle* sub, CStringW str, const FwSTSStyle& style);
    void ParsePolygon(CSubtitle* sub, const CStringW& str, const FwSTSStyle& style);

    bool ParseSSATag(AssTagList& assTags, const CStringW& str);
    bool CreateSubFromSSATag(CSubtitle* sub, const AssTagList& assTags, STSStyle& style, STSStyle& org, bool fAnimate = false);

    bool ParseHtmlTag(CSubtitle* sub, CStringW str, STSStyle& style, STSStyle& org);

    double CalcAnimation(double dst, double src, bool fAnimate);

    CSubtitle* GetSubtitle(int entry);

    void ClearUnCachedSubtitle(CSubtitle2List& sub2List);
    void ShrinkCache();
private:
    static std::size_t s_max_cache_size;
protected:
    virtual void OnChanged();
    
public:
    CRenderedTextSubtitle(CCritSec* pLock);
    virtual ~CRenderedTextSubtitle();

    virtual void Copy(CSimpleTextSubtitle& sts);
    virtual void Empty();

public:
    bool Init(const CRectCoor2& video_rect, const CRectCoor2& subtitle_target_rect,
        const SIZE& original_video_size); // will call Deinit()
    void Deinit();

    DECLARE_IUNKNOWN
    STDMETHODIMP NonDelegatingQueryInterface(REFIID riid, void** ppv);

    // ISubPicProviderEx2
    STDMETHODIMP Lock();
    STDMETHODIMP Unlock();
    STDMETHODIMP RenderEx(IXySubRenderFrame**subRenderFrame, int spd_type, 
        const RECT& video_rect, const RECT& subtitle_target_rect,
        const SIZE& original_video_size,
        REFERENCE_TIME rt, double fps);

    // ISubPicProviderEx && ISubPicProviderEx2
    STDMETHODIMP_(POSITION) GetStartPosition(REFERENCE_TIME rt, double fps);
    STDMETHODIMP_(POSITION) GetNext(POSITION pos);
    STDMETHODIMP_(REFERENCE_TIME) GetStart(POSITION pos, double fps);
    STDMETHODIMP_(REFERENCE_TIME) GetStop(POSITION pos, double fps);
    STDMETHODIMP_(VOID) GetStartStop(POSITION pos, double fps, /*out*/REFERENCE_TIME &start, /*out*/REFERENCE_TIME &stop);
    STDMETHODIMP_(bool) IsAnimated(POSITION pos);
    STDMETHODIMP Render(SubPicDesc& spd, REFERENCE_TIME rt, double fps, RECTCoor2& bbox);
    STDMETHODIMP RenderEx(SubPicDesc& spd, REFERENCE_TIME rt, double fps, CAtlList<CRectCoor2>& rectList);
    HRESULT ParseScript(REFERENCE_TIME rt, double fps, CSubtitle2List *outputSub2List );
    static void DoRender( const SIZECoor2& output_size, 
        const POINTCoor2& video_org, const RECTCoor2& margin_rect, 
        const CSubtitle2List& sub2List, 
        CompositeDrawItemListList *compDrawItemListList /*output*/);
    static void RenderOneSubtitle(const SIZECoor2& output_size, 
        const POINTCoor2& video_org, const RECTCoor2& margin_rect, 
        const CSubtitle2& sub2, 
        CompositeDrawItemList* compDrawItemList /*output*/);
    STDMETHODIMP_(bool) IsColorTypeSupported(int type);
    STDMETHODIMP_(bool) IsMovable();
    STDMETHODIMP_(bool) IsSimple();

    // IPersist
    STDMETHODIMP GetClassID(CLSID* pClassID);

    // ISubStream
    STDMETHODIMP_(int) GetStreamCount();
    STDMETHODIMP GetStreamInfo(int i, WCHAR** ppName, LCID* pLCID);
    STDMETHODIMP_(int) GetStream();
    STDMETHODIMP SetStream(int iStream);
    STDMETHODIMP Reload();
};
