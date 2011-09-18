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
#include "..\SubPic\ISubPic.h"
#include <atlcoll.h>
#include <boost/flyweight/key_value.hpp>
#include "mru_cache.h"

#define RTS_POS_SEGMENT_INDEX_BITS  16
#define RTS_POS_SUB_INDEX_MASK      ((1<<RTS_POS_SEGMENT_INDEX_BITS)-1)
#define RTS_ANIMATE_SUBPIC_DUR      ((int)(1000/23.976))

class CMyFont : public CFont
{
public:
    int m_ascent, m_descent;
    
    CMyFont(const FwSTSStyle& style);
};

typedef ::boost::flyweights::flyweight<::boost::flyweights::key_value<FwSTSStyle, CMyFont>, ::boost::flyweights::no_locking> FwCMyFont;

class CPolygon;

struct OverlayList
{
    Overlay* overlay;
    OverlayList* next;

    OverlayList()
    {
        overlay = NULL;
        next = NULL;
    }
    ~OverlayList()
    {
        delete next;
    }
};

class CWord : public Rasterizer
{
    bool m_fDrawn;
    CPoint m_p;

    bool NeedTransform();
    void Transform(CPoint org);

	void Transform_C( CPoint &org );
	void Transform_SSE2( CPoint &org );
    bool CreateOpaqueBox();

protected:
    FwStringW m_str;

    virtual bool CreatePath() = 0;

public:
    bool m_fWhiteSpaceChar, m_fLineBreak;

    FwSTSStyle m_style;

    CPolygon* m_pOpaqueBox;

    int m_ktype, m_kstart, m_kend;

    int m_width, m_ascent, m_descent;

    CWord(const STSStyle& style, const CStringW& str, int ktype, int kstart, int kend); // str[0] = 0 -> m_fLineBreak = true (in this case we only need and use the height of m_font from the whole class)
    virtual ~CWord();

    virtual CWord* Copy() = 0;
    virtual bool Append(CWord* w);

    void Paint(CPoint p, CPoint org, OverlayList* overlay_list);
    
    
    //friend class CWordCache;
    friend class CWordCacheKey;
    friend std::size_t hash_value(const CWord& key); 
};

class CWordCacheKey
{
public:
    CWordCacheKey(const CWord& word);
    CWordCacheKey(const CWordCacheKey& key);
    CWordCacheKey(const STSStyle& style, const CStringW& str, int ktype, int kstart, int kend);
    bool operator==(const CWordCacheKey& key)const;
    bool operator==(const CWord& key)const;

    FwStringW m_str;
    FwSTSStyle m_style;
    int m_ktype, m_kstart, m_kend;
};

class OverlayKey: public CWordCacheKey
{
public:
    OverlayKey(const CWord& word, const POINT& p):CWordCacheKey(word),m_p(p) { }
    OverlayKey(const OverlayKey& key):CWordCacheKey(key) { m_p.x=key.m_p.x; m_p.y=key.m_p.y; }
    OverlayKey(const STSStyle& style, const CStringW& str, int ktype, int kstart, int kend,POINT p):
            CWordCacheKey(style, str, ktype, kstart, kend),m_p(p) { }
    bool operator==(const OverlayKey& key)const { return (m_p.x==key.m_p.x) && (m_p.y==key.m_p.y) && ((CWordCacheKey)(*this)==(CWordCacheKey)key); }    
    bool CompareTo(const CWord& word, const POINT& p)const { return (m_p.x==p.x) && (m_p.y==p.y) && ((CWordCacheKey)(*this)==word); }

    POINT m_p;
};

struct OverlayCompatibleKey
{
    struct CompKey
    {
        CompKey(const CWord* word_, const CPoint& p_):word(word_),p(p_){}

        const CWord* word;
        CPoint p;
    };
    bool operator()(const CompKey& comp_key, const OverlayKey& key )const
    {
        return key.CompareTo(*comp_key.word,comp_key.p);
    }
    bool operator()(const OverlayKey& key, const CompKey& comp_key )const
    {
        return key.CompareTo(*comp_key.word,comp_key.p);
    }
    std::size_t operator()(const CompKey& comp_key)const
    {
        return hash_value(*comp_key.word) ^ comp_key.p.x ^ comp_key.p.y;
    }
};

class CText : public CWord
{
protected:
    virtual bool CreatePath();

public:
    CText(const STSStyle& style, const CStringW& str, int ktype, int kstart, int kend);

    virtual CWord* Copy();
    virtual bool Append(CWord* w);
};

class CPolygon : public CWord
{
    bool GetLONG(CStringW& str, LONG& ret);
    bool GetPOINT(CStringW& str, POINT& ret);
    bool ParseStr();

protected:
    double m_scalex, m_scaley;
    int m_baseline;

    CAtlArray<BYTE> m_pathTypesOrg;
    CAtlArray<CPoint> m_pathPointsOrg;

    virtual bool CreatePath();

public:
    CPolygon(const FwSTSStyle& style, CStringW str, int ktype, int kstart, int kend, double scalex, double scaley, int baseline);
	CPolygon(CPolygon&); // can't use a const reference because we need to use CAtlArray::Copy which expects a non-const reference
    virtual ~CPolygon();

    virtual CWord* Copy();
    virtual bool Append(CWord* w);
};

class CClipper:CPolygon
{
private:
    CWord* Copy();
    virtual bool Append(CWord* w);
public:
    CClipper(CStringW str, CSize size, double scalex, double scaley, bool inverse);
    virtual ~CClipper();

    CSize m_size;
    bool m_inverse;
    BYTE* m_pAlphaMask;
};

class CLine : public CAtlList<CWord*>
{
public:
    int m_width, m_ascent, m_descent, m_borderX, m_borderY;

    virtual ~CLine();

    void Compact();

    CRect PaintShadow(SubPicDesc& spd, CRect& clipRect, BYTE* pAlphaMask, CPoint p, CPoint org, int time, int alpha);
    CRect PaintOutline(SubPicDesc& spd, CRect& clipRect, BYTE* pAlphaMask, CPoint p, CPoint org, int time, int alpha);
    CRect PaintBody(SubPicDesc& spd, CRect& clipRect, BYTE* pAlphaMask, CPoint p, CPoint org, int time, int alpha);
};

enum eftype
{
    EF_MOVE = 0,    // {\move(x1=param[0], y1=param[1], x2=param[2], y2=param[3], t1=t[0], t2=t[1])} or {\pos(x=param[0], y=param[1])}
    EF_ORG,         // {\org(x=param[0], y=param[1])}
    EF_FADE,        // {\fade(a1=param[0], a2=param[1], a3=param[2], t1=t[0], t2=t[1], t3=t[2], t4=t[3])} or {\fad(t1=t[1], t2=t[2])
    EF_BANNER,      // Banner;delay=param[0][;lefttoright=param[1];fadeawaywidth=param[2]]
    EF_SCROLL,      // Scroll up/down=param[3];top=param[0];bottom=param[1];delay=param[2][;fadeawayheight=param[4]]
};

#define EF_NUMBEROFEFFECTS 5

class Effect
{
public:
    enum eftype type;
    int param[8];
    int t[4];
};

class CSubtitle : public CAtlList<CLine*>
{
    int GetFullWidth();
    int GetFullLineWidth(POSITION pos);
    int GetWrapWidth(POSITION pos, int maxwidth);
    CLine* GetNextLine(POSITION& pos, int maxwidth);

public:
    int m_scrAlignment;
    int m_wrapStyle;
    bool m_fAnimated;
    bool m_fAnimated2; //If this Subtitle has animate effect
    int m_relativeTo;

    Effect* m_effects[EF_NUMBEROFEFFECTS];

    CAtlList<CWord*> m_words;

    CClipper* m_pClipper;

    CRect m_rect, m_clip;
    int m_topborder, m_bottomborder;
    bool m_clipInverse;

    double m_scalex, m_scaley;

public:
    CSubtitle();
    virtual ~CSubtitle();
    virtual void Empty();

    void CreateClippers(CSize size);

    void MakeLines(CSize size, CRect marginRect);
};

std::size_t hash_value(const CWord& key);
std::size_t hash_value(const OverlayKey& key);
std::size_t hash_value(const CWordCacheKey& key);

//shouldn't use std::pair, or else VC complaining error C2440
//typedef std::pair<OverlayKey, Overlay*> OverlayMruItem; 
struct OverlayMruItem
{
    OverlayMruItem(const OverlayCompatibleKey::CompKey& comp_key, Overlay* overlay_):
        overlay_key(*comp_key.word, comp_key.p),overlay(overlay_){}
    OverlayMruItem(const OverlayKey& overlay_key_, Overlay* overlay_):overlay_key(overlay_key_),overlay(overlay_){}

    OverlayKey overlay_key;
    Overlay* overlay;
};

struct CWordMruItem
{
    CWordMruItem(const CWordCacheKey& word_key_, CWord* word_):word_key(word_key_),word(word_){}

    CWordCacheKey word_key;
    CWord* word;
};

class OverlayMruCache:public mru_list<
                                 OverlayMruItem, 
                                 boost::multi_index::member<OverlayMruItem, 
                                     OverlayKey, 
                                     &OverlayMruItem::overlay_key
                                 >
                             >
{
public:
    OverlayMruCache(std::size_t max_num_items_):mru_list(max_num_items_){}
    void update_cache(const OverlayMruItem& item);
    void clear();
};

class CWordMruCache:public mru_list<
                               CWordMruItem, 
                               boost::multi_index::member<CWordMruItem, 
                                   CWordCacheKey, 
                                   &CWordMruItem::word_key
                               >
                           >
{
public:
    CWordMruCache(std::size_t max_num_items_):mru_list(max_num_items_){}
    void update_cache(const CWordMruItem& item);
    void clear();
};

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
class CRenderedTextSubtitle : public CSimpleTextSubtitle, public ISubPicProviderImpl, public ISubStream
{
    CAtlMap<int, CSubtitle*> m_subtitleCache;   

    CScreenLayoutAllocator m_sla;

    CSize m_size;
    CRect m_vidrect;

    enum AssCmdType
    {
        CMD_1c = 0,
        CMD_2c,
        CMD_3c,
        CMD_4c,
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
        CMD_iclip,
        CMD_c,
        CMD_fade,
        CMD_fad,
        CMD_fax,
        CMD_fay,
        CMD_fe,
        CMD_fn,
        CMD_frx,
        CMD_fry,
        CMD_frz,
        CMD_fr,
        CMD_fscx,
        CMD_fscy,
        CMD_fsc,
        CMD_fsp,
        CMD_fs,
        CMD_i,
        CMD_kt,
        CMD_kf,
        CMD_K,
        CMD_ko,
        CMD_k,
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
    static const int MIN_CMD_LENGTH = 1;//c etc
    static const int MAX_CMD_LENGTH = 5;//alpha, iclip, xbord, xshad, ybord, yshad
    static CAtlMap<CStringW, AssCmdType, CStringElementTraits<CStringW>> m_cmdMap;
    static void InitCmdMap();

    // temp variables, used when parsing the script
    int m_time, m_delay;
    int m_animStart, m_animEnd;
    double m_animAccel;
    int m_ktype, m_kstart, m_kend;
    int m_nPolygon;
    int m_polygonBaselineOffset;
    double m_fps;

    void ParseEffect(CSubtitle* sub, CString str);
    void ParseString(CSubtitle* sub, CStringW str, const FwSTSStyle& style);
    void ParsePolygon(CSubtitle* sub, CStringW str, const FwSTSStyle& style);
    bool ParseSSATag(CSubtitle* sub, CStringW str, STSStyle& style, const STSStyle& org, bool fAnimate = false);
    bool ParseHtmlTag(CSubtitle* sub, CStringW str, STSStyle& style, STSStyle& org);

    double CalcAnimation(double dst, double src, bool fAnimate);

    CSubtitle* GetSubtitle(int entry);

protected:
    virtual void OnChanged();

public:
    CRenderedTextSubtitle(CCritSec* pLock);
    virtual ~CRenderedTextSubtitle();

    virtual void Copy(CSimpleTextSubtitle& sts);
    virtual void Empty();

public:
    bool Init(CSize size, CRect vidrect); // will call Deinit()
    void Deinit();

    DECLARE_IUNKNOWN
    STDMETHODIMP NonDelegatingQueryInterface(REFIID riid, void** ppv);

    // ISubPicProvider
    STDMETHODIMP_(POSITION) GetStartPosition(REFERENCE_TIME rt, double fps);
    STDMETHODIMP_(POSITION) GetNext(POSITION pos);
    STDMETHODIMP_(REFERENCE_TIME) GetStart(POSITION pos, double fps);
    STDMETHODIMP_(REFERENCE_TIME) GetStop(POSITION pos, double fps);
    STDMETHODIMP_(VOID) GetStartStop(POSITION pos, double fps, /*out*/REFERENCE_TIME &start, /*out*/REFERENCE_TIME &stop);
    STDMETHODIMP_(bool) IsAnimated(POSITION pos);
    STDMETHODIMP Render(SubPicDesc& spd, REFERENCE_TIME rt, double fps, RECT& bbox);
    STDMETHODIMP Render(SubPicDesc& spd, REFERENCE_TIME rt, double fps, CAtlList<CRect>& rectList);

    // IPersist
    STDMETHODIMP GetClassID(CLSID* pClassID);

    // ISubStream
    STDMETHODIMP_(int) GetStreamCount();
    STDMETHODIMP GetStreamInfo(int i, WCHAR** ppName, LCID* pLCID);
    STDMETHODIMP_(int) GetStream();
    STDMETHODIMP SetStream(int iStream);
    STDMETHODIMP Reload();
};
