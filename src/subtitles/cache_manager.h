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
    PathDataCacheKey(const PathDataCacheKey& key)
        :m_str(key.m_str)
        ,m_style(key.m_style)
        ,m_hash_value(key.m_hash_value){}
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
    
    ULONG UpdateHashValue();
    ULONG GetHashValue()const
    {
        return m_hash_value;
    }
public:
    ULONG m_hash_value;
protected:
    CStringW m_str;
    FwSTSStyle m_style;
};

class ScanLineData2CacheKey: public PathDataCacheKey
{
public:
    ScanLineData2CacheKey(const CWord& word, const POINT& org):PathDataCacheKey(word),m_org(org) { }
    ScanLineData2CacheKey(const ScanLineData2CacheKey& key)
        :PathDataCacheKey(key)
        ,m_org(key.m_org)
        ,m_hash_value(key.m_hash_value) { }
    ScanLineData2CacheKey(const FwSTSStyle& style, const CStringW& str, const POINT& org)
        :PathDataCacheKey(style, str),m_org(org) { }
    bool operator==(const ScanLineData2CacheKey& key)const;

    ULONG UpdateHashValue();
    ULONG GetHashValue()const
    {
        return m_hash_value;
    }
public:
    ULONG m_hash_value;
    POINT m_org;
};

class OverlayNoBlurKey: public ScanLineData2CacheKey
{
public:
    OverlayNoBlurKey(const CWord& word, const POINT& p, const POINT& org):ScanLineData2CacheKey(word,org),m_p(p) {}
    OverlayNoBlurKey(const OverlayNoBlurKey& key):ScanLineData2CacheKey(key),m_p(key.m_p),m_hash_value(key.m_hash_value) {}
    OverlayNoBlurKey(const FwSTSStyle& style, const CStringW& str, const POINT& p, const POINT& org)
        :ScanLineData2CacheKey(style, str, org),m_p(p) {}
    bool operator==(const OverlayNoBlurKey& key)const;

    ULONG UpdateHashValue();
    ULONG GetHashValue()const
    {
        return m_hash_value;
    }
public:
    ULONG m_hash_value;
    POINT m_p;    
};

class OverlayKey: public OverlayNoBlurKey
{
public:
    OverlayKey(const CWord& word, const POINT& p, const POINT& org):OverlayNoBlurKey(word, p, org) {}
    OverlayKey(const OverlayKey& key):OverlayNoBlurKey(key), m_hash_value(key.m_hash_value) {}

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

    ULONG UpdateHashValue();
    ULONG GetHashValue()const
    {
        return m_hash_value;
    }
public:
    ULONG m_hash_value;
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

    ULONG UpdateHashValue();
    ULONG GetHashValue()const
    {
        return m_hash_value;
    }
public:
    ULONG m_hash_value;

    int m_border;
    int m_rasterize_sub;
};

class ClipperAlphaMaskCacheKey
{
public:
    ClipperAlphaMaskCacheKey(const SharedPtrCClipper& clipper):m_clipper(clipper){}
    bool operator==(const ClipperAlphaMaskCacheKey& key)const;

    ULONG UpdateHashValue();
    ULONG GetHashValue()const
    {
        return m_hash_value;
    }
public:
    ULONG m_hash_value;

    SharedPtrCClipper m_clipper;
};

struct DrawItem;
class DrawItemHashKey
{
public:
    DrawItemHashKey(const DrawItem& draw_item);
    bool operator==(const DrawItemHashKey& hash_key) const;

    ULONG UpdateHashValue();
    ULONG GetHashValue()const
    {
        return m_hash_value;
    }
public:
    ULONG m_hash_value;
private:
    typedef ::boost::shared_ptr<OverlayKey> SharedOverlayKey;
    SharedOverlayKey m_overlay_key;
    ClipperAlphaMaskCacheKey m_clipper_key;
    CRect m_clip_rect;
    int m_xsub;
    int m_ysub;
    DWORD m_switchpts[6];
    bool m_fBody;
    bool m_fBorder;
};

class GroupedDrawItemsHashKey
{
public:
    bool operator==(const GroupedDrawItemsHashKey& key) const;

    ULONG UpdateHashValue();
    ULONG GetHashValue()const
    {
        return m_hash_value;
    }
public:
    ULONG m_hash_value;
private:
    typedef ::boost::shared_ptr<DrawItemHashKey> PKey;
    typedef CAtlArray<PKey> Keys;
    typedef ::boost::shared_ptr<Keys> PKeys;

    PKeys m_key;
    CRect m_clip_rect;

    friend struct GroupedDrawItems;
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

class PathDataTraits:public CElementTraits<PathData>
{
public:
    static ULONG Hash(const PathData& key);
};

class ClipperTraits:public CElementTraits<CClipper>
{
public:
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

typedef EnhancedXyMru<PathDataCacheKey, SharedPtrConstPathData, XyCacheKeyTraits<PathDataCacheKey>> PathDataMruCache;

typedef EnhancedXyMru<ScanLineData2CacheKey, SharedPtrConstScanLineData2, XyCacheKeyTraits<ScanLineData2CacheKey>> ScanLineData2MruCache;

typedef EnhancedXyMru<OverlayNoBlurKey, SharedPtrOverlay, XyCacheKeyTraits<OverlayNoBlurKey>> OverlayNoBlurMruCache;

typedef EnhancedXyMru<OverlayKey, SharedPtrOverlay, XyCacheKeyTraits<OverlayKey>> OverlayMruCache;

typedef EnhancedXyMru<ScanLineDataCacheKey, SharedPtrConstScanLineData, XyCacheKeyTraits<ScanLineDataCacheKey>> ScanLineDataMruCache;

typedef EnhancedXyMru<OverlayNoOffsetKey, OverlayNoBlurKey, XyCacheKeyTraits<OverlayNoOffsetKey>> OverlayNoOffsetMruCache;

typedef EnhancedXyMru<ClipperAlphaMaskCacheKey, SharedPtrGrayImage2, XyCacheKeyTraits<ClipperAlphaMaskCacheKey>> ClipperAlphaMaskMruCache;

class XyBitmap;
typedef ::boost::shared_ptr<XyBitmap> SharedPtrXyBitmap;
typedef EnhancedXyMru<GroupedDrawItemsHashKey, SharedPtrXyBitmap, XyCacheKeyTraits<GroupedDrawItemsHashKey>> BitmapMruCache;


class CacheManager
{
public:
    static const int BITMAP_MRU_CACHE_ITEM_NUM = 64;//for test only 
    static const int CLIPPER_MRU_CACHE_ITEM_NUM = 48;

    static const int TEXT_INFO_CACHE_ITEM_NUM = 2048;
    static const int ASS_TAG_LIST_CACHE_ITEM_NUM = 256;
    static const int SUBPIXEL_VARIANCE_CACHE_ITEM_NUM = 256;
    static const int OVERLAY_CACHE_ITEM_NUM = 256;

    static const int OVERLAY_NO_BLUR_CACHE_ITEM_NUM = 256;
    static const int SCAN_LINE_DATA_CACHE_ITEM_NUM = 512;
    static const int PATH_CACHE_ITEM_NUM = 512;
    static const int WORD_CACHE_ITEM_NUM = 512;


    static BitmapMruCache* GetBitmapMruCache();

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

