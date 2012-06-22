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

    ULONG m_hash_value;
public:
    bool operator==(const TextInfoCacheKey& key)const;

    ULONG UpdateHashValue();
    ULONG GetHashValue()const
    {
        return m_hash_value;
    }
};

class CWordCacheKey
{
public:
    CWordCacheKey(const CWord& word);
    CWordCacheKey(const CWordCacheKey& key);
    CWordCacheKey(const FwSTSStyle& style, const CStringW& str, int ktype, int kstart, int kend);
    bool operator==(const CWordCacheKey& key)const;
    bool operator==(const CWord& key)const;

    ULONG UpdateHashValue();
    ULONG GetHashValue()const
    {
        return m_hash_value;
    }
public:
    CStringW m_str;
    FwSTSStyle m_style;
    int m_ktype, m_kstart, m_kend;

    ULONG m_hash_value;
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

class ScanLineData2CacheKey: public PathDataCacheKey
{
public:
    ScanLineData2CacheKey(const CWord& word, const POINT& org):PathDataCacheKey(word),m_org(org) { }
    ScanLineData2CacheKey(const ScanLineData2CacheKey& key):PathDataCacheKey(key),m_org(key.m_org) { }
    ScanLineData2CacheKey(const FwSTSStyle& style, const CStringW& str, const POINT& org)
        :PathDataCacheKey(style, str),m_org(org) { }
    bool operator==(const ScanLineData2CacheKey& key)const;

    POINT m_org;

    friend class ScanLineData2CacheKeyTraits;
};

class OverlayNoBlurKey: public ScanLineData2CacheKey
{
public:
    OverlayNoBlurKey(const CWord& word, const POINT& p, const POINT& org):ScanLineData2CacheKey(word,org),m_p(p) { }
    OverlayNoBlurKey(const OverlayNoBlurKey& key):ScanLineData2CacheKey(key),m_p(key.m_p) { }
    OverlayNoBlurKey(const FwSTSStyle& style, const CStringW& str, const POINT& p, const POINT& org)
        :ScanLineData2CacheKey(style, str, org),m_p(p) { }
    bool operator==(const OverlayNoBlurKey& key)const;

    POINT m_p;    
};

class OverlayKey: public OverlayNoBlurKey
{
public:
    OverlayKey(const CWord& word, const POINT& p, const POINT& org):OverlayNoBlurKey(word, p, org) {  }
    OverlayKey(const OverlayKey& key):OverlayNoBlurKey(key), m_hash_value(key.m_hash_value) {  }

    bool operator==(const OverlayKey& key)const;

    ULONG UpdateHashValue();
    ULONG GetHashValue()const
    {
        return m_hash_value;
    }
public:
    ULONG m_hash_value;
};

class ScanLineDataCacheKey
{
public:
    ScanLineDataCacheKey(const SharedPtrConstPathData& path_data):m_path_data(path_data){}
    bool operator==(const ScanLineDataCacheKey& key)const;

    SharedPtrConstPathData m_path_data;
};

class OverlayNoOffsetKey:public ScanLineDataCacheKey
{
public:
    OverlayNoOffsetKey(const SharedPtrConstPathData& path_data, int xsub, int ysub, int border_x, int border_y)
        : ScanLineDataCacheKey(path_data)
        , m_border( border_x+(border_y<<16) )
        , m_rasterize_sub( xsub+(ysub<<16) ){}
    bool operator==(const OverlayNoOffsetKey& key)const;

    int m_border;
    int m_rasterize_sub;
};

class ClipperAlphaMaskCacheKey
{
public:
    ClipperAlphaMaskCacheKey(const SharedPtrCClipper& clipper):m_clipper(clipper){}
    bool operator==(const ClipperAlphaMaskCacheKey& key)const;
public:
    SharedPtrCClipper m_clipper;
};

template<class CahcheKey>
class XyCacheKeyTraits:public CElementTraits<CahcheKey>
{    
public:
    static ULONG Hash(const CahcheKey& key)
    {
        return key.GetHashValue();
    }
};

class PathDataCacheKeyTraits:public CElementTraits<PathDataCacheKey>
{
public:
    static ULONG Hash(const PathDataCacheKey& key);
};

class ScanLineData2CacheKeyTraits:public CElementTraits<ScanLineData2CacheKey>
{
public:
    static ULONG Hash(const ScanLineData2CacheKey& key);
};

class OverlayNoBlurKeyTraits:public CElementTraits<OverlayNoBlurKey>
{
public:
    static ULONG Hash(const OverlayNoBlurKey& key);
};

class ScanLineDataCacheKeyTraits:public CElementTraits<ScanLineDataCacheKey>
{
public:
    static ULONG Hash(const ScanLineDataCacheKey& key);
};

class OverlayNoOffsetKeyTraits:public CElementTraits<OverlayNoOffsetKey>
{
public:
    static ULONG Hash(const OverlayNoOffsetKey& key);
};

class PathDataTraits:public CElementTraits<PathData>
{
public:
    static ULONG Hash(const PathData& key);
};

class ClipperAlphaMaskCacheKeyTraits:public CElementTraits<ClipperAlphaMaskCacheKey>
{
public:
    static ULONG Hash(const ClipperAlphaMaskCacheKey& key);
    static ULONG Hash(const CClipper& key);
};

typedef EnhancedXyMru<
    TextInfoCacheKey, 
    CText::SharedPtrTextInfo, 
    XyCacheKeyTraits<TextInfoCacheKey>
> TextInfoMruCache;

typedef EnhancedXyMru<
    CStringW, 
    CRenderedTextSubtitle::SharedPtrConstAssTagList, 
    CStringElementTraits<CStringW>
> AssTagListMruCache;

typedef EnhancedXyMru<CWordCacheKey, SharedPtrCWord, XyCacheKeyTraits<CWordCacheKey>> CWordMruCache;

typedef EnhancedXyMru<PathDataCacheKey, SharedPtrConstPathData, PathDataCacheKeyTraits> PathDataMruCache;

typedef EnhancedXyMru<ScanLineData2CacheKey, SharedPtrConstScanLineData2, ScanLineData2CacheKeyTraits> ScanLineData2MruCache;

typedef EnhancedXyMru<OverlayNoBlurKey, SharedPtrOverlay, OverlayNoBlurKeyTraits> OverlayNoBlurMruCache;

typedef EnhancedXyMru<OverlayKey, SharedPtrOverlay, XyCacheKeyTraits<OverlayKey>> OverlayMruCache;

typedef EnhancedXyMru<ScanLineDataCacheKey, SharedPtrConstScanLineData, ScanLineDataCacheKeyTraits> ScanLineDataMruCache;

typedef EnhancedXyMru<OverlayNoOffsetKey, OverlayNoBlurKey, OverlayNoOffsetKeyTraits> OverlayNoOffsetMruCache;

typedef EnhancedXyMru<ClipperAlphaMaskCacheKey, SharedPtrGrayImage2, ClipperAlphaMaskCacheKeyTraits> ClipperAlphaMaskMruCache;

class CacheManager
{
public:
    static const int CLIPPER_ALPHA_MASK_MRU_CACHE = 48;

    static const int TEXT_INFO_CACHE_ITEM_NUM = 2048;
    static const int ASS_TAG_LIST_CACHE_ITEM_NUM = 256;
    static const int SUBPIXEL_VARIANCE_CACHE_ITEM_NUM = 256;
    static const int OVERLAY_CACHE_ITEM_NUM = 256;

    static const int OVERLAY_NO_BLUR_CACHE_ITEM_NUM = 256;
    static const int SCAN_LINE_DATA_CACHE_ITEM_NUM = 512;
    static const int PATH_CACHE_ITEM_NUM = 512;
    static const int WORD_CACHE_ITEM_NUM = 512;

    static ClipperAlphaMaskMruCache* GetClipperAlphaMaskMruCache();
    static TextInfoMruCache* GetTextInfoCache();
    static AssTagListMruCache* GetAssTagListMruCache();

    static ScanLineDataMruCache* GetScanLineDataMruCache();
    static OverlayNoOffsetMruCache* GetOverlayNoOffsetMruCache();

    static OverlayMruCache* GetSubpixelVarianceCache();
    static OverlayMruCache* GetOverlayMruCache();
    static OverlayNoBlurMruCache* GetOverlayNoBlurMruCache();
    static ScanLineData2MruCache* GetScanLineData2MruCache();
    static PathDataMruCache* GetPathDataMruCache();
    static CWordMruCache* GetCWordMruCache();
};

#endif // end of __CACHE_MANAGER_H_310C134F_844C_4590_A4D2_AD30165AF10A__

