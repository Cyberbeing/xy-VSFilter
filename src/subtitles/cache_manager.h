/************************************************************************/
/* author: xy                                                           */
/* date: 20110926                                                       */
/************************************************************************/
#ifndef __CACHE_MANAGER_H_310C134F_844C_4590_A4D2_AD30165AF10A__
#define __CACHE_MANAGER_H_310C134F_844C_4590_A4D2_AD30165AF10A__

#include "RTS.h"
#include "mru_cache.h"

class TextInfoCacheKey
{
public:
    CStringW m_str;
    FwSTSStyle m_style;
    bool operator==(const TextInfoCacheKey& key)const;

    friend class TextInfoCacheKeyTraits;
};

class CWordCacheKey
{
public:
    CWordCacheKey(const CWord& word);
    CWordCacheKey(const CWordCacheKey& key);
    CWordCacheKey(const FwSTSStyle& style, const CStringW& str, int ktype, int kstart, int kend);
    bool operator==(const CWordCacheKey& key)const;
    bool operator==(const CWord& key)const;

    CStringW m_str;
    FwSTSStyle m_style;
    int m_ktype, m_kstart, m_kend;

    friend class CWordCacheKeyTraits;
};

class PathDataCacheKey
{
public:
    PathDataCacheKey(const CWord& word):m_str(word.m_str),m_style(word.m_style){}
    PathDataCacheKey(const PathDataCacheKey& key):m_str(key.m_str),m_style(key.m_style){}
    PathDataCacheKey(const FwSTSStyle& style, const CStringW& str):m_str(str),m_style(style){}
    bool operator==(const PathDataCacheKey& key)const
    {
        return m_str==key.m_str && ( m_style==key.m_style || CompareSTSStyle(m_style, key.m_style) );
    }
    bool operator==(const CWord& key)const
    {
        return m_str==key.m_str && ( m_style==key.m_style || CompareSTSStyle(m_style.get(), key.m_style.get()) );
    }

    static bool CompareSTSStyle(const STSStyle& lhs, const STSStyle& rhs);
protected:
    CStringW m_str;
    FwSTSStyle m_style;

    friend class PathDataCacheKeyTraits;
};

class ScanLineDataCacheKey: public PathDataCacheKey
{
public:
    ScanLineDataCacheKey(const CWord& word, const POINT& org):PathDataCacheKey(word),m_org(org) { }
    ScanLineDataCacheKey(const ScanLineDataCacheKey& key):PathDataCacheKey(key),m_org(key.m_org) { }
    ScanLineDataCacheKey(const FwSTSStyle& style, const CStringW& str, const POINT& org)
        :PathDataCacheKey(style, str),m_org(org) { }
    bool operator==(const ScanLineDataCacheKey& key)const;

    POINT m_org;

    friend class ScanLineDataCacheKeyTraits;
};

class OverlayNoBlurKey: public ScanLineDataCacheKey
{
public:
    OverlayNoBlurKey(const CWord& word, const POINT& p, const POINT& org):ScanLineDataCacheKey(word,org),m_p(p) { }
    OverlayNoBlurKey(const OverlayNoBlurKey& key):ScanLineDataCacheKey(key),m_p(key.m_p) { }
    OverlayNoBlurKey(const FwSTSStyle& style, const CStringW& str, const POINT& p, const POINT& org)
        :ScanLineDataCacheKey(style, str, org),m_p(p) { }
    bool operator==(const OverlayNoBlurKey& key)const;

    POINT m_p;    
};

class OverlayKey: public OverlayNoBlurKey
{
public:
    OverlayKey(const CWord& word, const POINT& p, const POINT& org):OverlayNoBlurKey(word, p, org) { }
    OverlayKey(const OverlayKey& key):OverlayNoBlurKey(key){ }

    bool operator==(const OverlayKey& key)const;

    friend class OverlayKeyTraits;
};

std::size_t hash_value(const CWord& key);

class TextInfoCacheKeyTraits:public CElementTraits<TextInfoCacheKey>
{
public:
    static ULONG Hash(const TextInfoCacheKey& key);
};

class CWordCacheKeyTraits:public CElementTraits<CWordCacheKey>
{
public:
    static ULONG Hash(const CWordCacheKey& key);
};

class PathDataCacheKeyTraits:public CElementTraits<PathDataCacheKey>
{
public:
    static ULONG Hash(const PathDataCacheKey& key);
};

class ScanLineDataCacheKeyTraits:public CElementTraits<ScanLineDataCacheKey>
{
public:
    static ULONG Hash(const ScanLineDataCacheKey& key);
};

class OverlayNoBlurKeyTraits:public CElementTraits<OverlayNoBlurKey>
{
public:
    static ULONG Hash(const OverlayNoBlurKey& key);
};

class OverlayKeyTraits:public CElementTraits<OverlayKey>
{
public:
    static ULONG Hash(const OverlayKey& key);
};

typedef EnhancedXyMru<
    TextInfoCacheKey, 
    CText::SharedPtrTextInfo, 
    TextInfoCacheKeyTraits
> TextInfoMruCache;

typedef EnhancedXyMru<
    CStringW, 
    CRenderedTextSubtitle::SharedPtrConstAssTagList, 
    CStringElementTraits<CStringW>
> AssTagListMruCache;

typedef EnhancedXyMru<CWordCacheKey, SharedPtrCWord, CWordCacheKeyTraits> CWordMruCache;

typedef EnhancedXyMru<PathDataCacheKey, SharedPtrConstPathData, PathDataCacheKeyTraits> PathDataMruCache;

typedef EnhancedXyMru<ScanLineDataCacheKey, SharedPtrConstScanLineData, ScanLineDataCacheKeyTraits> ScanLineDataMruCache;

typedef EnhancedXyMru<OverlayNoBlurKey, SharedPtrOverlay, OverlayNoBlurKeyTraits> OverlayNoBlurMruCache;

typedef EnhancedXyMru<OverlayKey, SharedPtrOverlay, OverlayKeyTraits> OverlayMruCache;

class CacheManager
{
public:
    static const int TEXT_INFO_CACHE_ITEM_NUM = 2048;
    static const int ASS_TAG_LIST_CACHE_ITEM_NUM = 256;
    static const int SUBPIXEL_VARIANCE_CACHE_ITEM_NUM = 256;
    static const int OVERLAY_CACHE_ITEM_NUM = 256;

    static const int OVERLAY_NO_BLUR_CACHE_ITEM_NUM = 256;
    static const int SCAN_LINE_DATA_CACHE_ITEM_NUM = 256;
    static const int PATH_CACHE_ITEM_NUM = 256;
    static const int WORD_CACHE_ITEM_NUM = 512;

    static TextInfoMruCache* GetTextInfoCache();
    static AssTagListMruCache* GetAssTagListMruCache();
    static OverlayMruCache* GetSubpixelVarianceCache();
    static OverlayMruCache* GetOverlayMruCache();
    static OverlayNoBlurMruCache* GetOverlayNoBlurMruCache();
    static ScanLineDataMruCache* GetScanLineDataMruCache();
    static PathDataMruCache* GetPathDataMruCache();
    static CWordMruCache* GetCWordMruCache();
private:
    static TextInfoMruCache* s_text_info_cache;
    static AssTagListMruCache* s_ass_tag_list_cache;
    static OverlayMruCache* s_subpixel_variance_cache;
    static OverlayMruCache* s_overlay_mru_cache;
    static OverlayNoBlurMruCache* s_overlay_no_blur_mru_cache;
    static PathDataMruCache* s_path_data_mru_cache;
    static ScanLineDataMruCache* s_scan_line_data_mru_cache;
    static CWordMruCache* s_word_mru_cache;
};

#endif // end of __CACHE_MANAGER_H_310C134F_844C_4590_A4D2_AD30165AF10A__

