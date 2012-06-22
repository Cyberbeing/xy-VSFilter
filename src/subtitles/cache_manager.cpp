/************************************************************************/
/* author: xy                                                           */
/* date: 20110926                                                       */
/************************************************************************/
#include "StdAfx.h"
#include "cache_manager.h"

ULONG PathDataCacheKeyTraits::Hash( const PathDataCacheKey& key )
{
    return( CStringElementTraits<CString>::Hash(key.m_str) ); 
}

ULONG ScanLineData2CacheKeyTraits::Hash( const ScanLineData2CacheKey& key )
{
    //return hash_value(static_cast<PathDataCacheKey>(key)) ^ key.m_org.x ^ key.m_org.y;
    size_t hash = PathDataCacheKeyTraits::Hash(static_cast<const PathDataCacheKey&>(key));
    hash += (hash<<5);
    hash += key.m_org.x;
    hash += (hash<<5);
    hash += key.m_org.y;
    return  hash;
}

ULONG PathDataTraits::Hash( const PathData& key )
{
    ULONG hash = 515;
    hash += (hash<<5);
    hash += key.mPathPoints;
    for (int i=0;i<key.mPathPoints;i++)
    {
        hash += (hash<<5);
        hash += key.mpPathTypes[i];
    }
    for (int i=0;i<key.mPathPoints;i++)
    {
        hash += (hash<<5);
        hash += key.mpPathPoints[i].x;
        hash += (hash<<5);
        hash += key.mpPathPoints[i].y;
    }
    return hash;
}

ULONG ScanLineDataCacheKeyTraits::Hash( const ScanLineDataCacheKey& key )
{
    return PathDataTraits::Hash(*key.m_path_data);
}

ULONG OverlayNoOffsetKeyTraits::Hash( const OverlayNoOffsetKey& key )
{
    ULONG hash = ScanLineDataCacheKeyTraits::Hash(key);
    hash += (hash<<5);
    hash += key.m_border;
    hash += (hash<<5);
    hash += key.m_rasterize_sub;
    return hash;
}

ULONG ClipperAlphaMaskCacheKeyTraits::Hash( const ClipperAlphaMaskCacheKey& key )
{
    return Hash(*key.m_clipper);
}

ULONG ClipperAlphaMaskCacheKeyTraits::Hash( const CClipper& key )
{
    ULONG hash = CStringElementTraits<CString>::Hash(key.m_polygon->m_str);;
    hash += (hash<<5);
    hash += key.m_inverse;
    hash += (hash<<5);
    hash += key.m_effectType;
    hash += (hash<<5);
    hash += key.m_size.cx;
    hash += (hash<<5);
    hash += key.m_size.cy;
    hash += (hash<<5);
    hash += hash_value(key.m_polygon->m_scalex);
    hash += (hash<<5);
    hash += hash_value(key.m_polygon->m_scaley);

    for (int i=0;i<sizeof(key.m_effect.param)/sizeof(key.m_effect.param[0]);i++)
    {
        hash += (hash<<5);
        hash += key.m_effect.param[i];
    }
    for (int i=0;i<sizeof(key.m_effect.t)/sizeof(key.m_effect.t[0]);i++)
    {
        hash += (hash<<5);
        hash += key.m_effect.t[i];
    }
    return hash;
}

//////////////////////////////////////////////////////////////////////////////////////////////

// TextInfoCacheKey

bool TextInfoCacheKey::operator==( const TextInfoCacheKey& key ) const
{
    return m_str == key.m_str 
        && static_cast<const STSStyleBase&>(m_style).operator==(key.m_style)
        && m_style.get().fontScaleX == key.m_style.get().fontScaleX
        && m_style.get().fontScaleY == key.m_style.get().fontScaleY
        && m_style.get().fontSpacing == key.m_style.get().fontSpacing;
}

ULONG TextInfoCacheKey::UpdateHashValue() 
{
    m_hash_value = CStringElementTraits<CString>::Hash(m_str);
    m_hash_value += (m_hash_value<<5);
    m_hash_value += hash_value( static_cast<const STSStyleBase&>(m_style.get()) );
    m_hash_value += (m_hash_value<<5);
    m_hash_value += hash_value( m_style.get().fontScaleX );
    m_hash_value += (m_hash_value<<5);
    m_hash_value += hash_value( m_style.get().fontScaleY );
    m_hash_value += (m_hash_value<<5);
    m_hash_value += hash_value( m_style.get().fontSpacing );
    return m_hash_value;
}

//////////////////////////////////////////////////////////////////////////////////////////////

// CWordCacheKey

CWordCacheKey::CWordCacheKey( const CWord& word )
{
    m_str = word.m_str;
    m_style = word.m_style;
    m_ktype = word.m_ktype;
    m_kstart = word.m_kstart;
    m_kend = word.m_kend;
}

CWordCacheKey::CWordCacheKey( const CWordCacheKey& key )
{
    m_str = key.m_str;
    m_style = key.m_style;
    m_ktype = key.m_ktype;
    m_kstart = key.m_kstart;
    m_kend = key.m_kend;
    m_hash_value = key.m_hash_value;
}

CWordCacheKey::CWordCacheKey( const FwSTSStyle& style, const CStringW& str, int ktype, int kstart, int kend )
    :m_style(style),m_str(str),m_ktype(ktype),m_kstart(kstart),m_kend(m_kend)
{

}

bool CWordCacheKey::operator==( const CWordCacheKey& key ) const
{
    return (m_str == key.m_str &&
        m_style == key.m_style &&
        m_ktype == key.m_ktype &&
        m_kstart == key.m_kstart &&
        m_kend == key.m_kend);
}

bool CWordCacheKey::operator==(const CWord& key)const
{
    return (m_str == key.m_str &&
        m_style == key.m_style &&
        m_ktype == key.m_ktype &&
        m_kstart == key.m_kstart &&
        m_kend == key.m_kend);
}

ULONG CWordCacheKey::UpdateHashValue() 
{
    m_hash_value = CStringElementTraits<CString>::Hash(m_str);//fix me
    return m_hash_value;
}
//////////////////////////////////////////////////////////////////////////////////////////////

// PathDataCacheKey

bool PathDataCacheKey::CompareSTSStyle( const STSStyle& lhs, const STSStyle& rhs )
{
    return lhs.charSet==rhs.charSet &&
        lhs.fontName==rhs.fontName &&
        lhs.fontSize==rhs.fontSize &&
        lhs.fontSpacing==rhs.fontSpacing &&
        lhs.fontWeight==rhs.fontWeight &&
        lhs.fItalic==rhs.fItalic &&
        lhs.fUnderline==rhs.fUnderline &&
        lhs.fStrikeOut==rhs.fStrikeOut;
}

//////////////////////////////////////////////////////////////////////////////////////////////

// ScanLineData2CacheKey

bool ScanLineData2CacheKey::operator==( const ScanLineData2CacheKey& key ) const
{ 
    return //(static_cast<PathDataCacheKey>(*this)==static_cast<PathDataCacheKey>(key)) 
        PathDataCacheKey::operator==(key) //static_cast will call copy constructer to construct a tmp obj
        && this->m_style.get().borderStyle == key.m_style.get().borderStyle
        && fabs(this->m_style.get().outlineWidthX - key.m_style.get().outlineWidthX) < 0.000001
        && fabs(this->m_style.get().outlineWidthY - key.m_style.get().outlineWidthY) < 0.000001
        && fabs(this->m_style.get().fontScaleX - key.m_style.get().fontScaleX) < 0.000001
        && fabs(this->m_style.get().fontScaleY - key.m_style.get().fontScaleY) < 0.000001
        && fabs(this->m_style.get().fontAngleX - key.m_style.get().fontAngleX) < 0.000001
        && fabs(this->m_style.get().fontAngleY - key.m_style.get().fontAngleY) < 0.000001
        && fabs(this->m_style.get().fontAngleZ - key.m_style.get().fontAngleZ) < 0.000001
        && fabs(this->m_style.get().fontShiftX - key.m_style.get().fontShiftX) < 0.000001
        && fabs(this->m_style.get().fontShiftY - key.m_style.get().fontShiftY) < 0.000001
        && (m_org.x==key.m_org.x) && (m_org.y==key.m_org.y); 
}

//////////////////////////////////////////////////////////////////////////////////////////////

// OverlayNoBlurKey

bool OverlayNoBlurKey::operator==( const OverlayNoBlurKey& key ) const
{ 
    //static_cast will call copy constructer to construct a tmp obj
    //return (static_cast<ScanLineDataCacheKey>(*this)==static_cast<ScanLineDataCacheKey>(key)) 
    //    && (m_p.x==key.m_p.x) && (m_p.y==key.m_p.y); 
    return ScanLineData2CacheKey::operator==(key) && (m_p.x==key.m_p.x) && (m_p.y==key.m_p.y);
}

ULONG OverlayNoBlurKey::UpdateHashValue()
{
    m_hash_value = ScanLineData2CacheKeyTraits::Hash(*this);
    m_hash_value += (m_hash_value<<5);
    m_hash_value += m_p.x;
    m_hash_value += (m_hash_value<<5);
    m_hash_value += m_p.y;
    return  m_hash_value;
}


//////////////////////////////////////////////////////////////////////////////////////////////

// OverlayKey

bool OverlayKey::operator==( const OverlayKey& key ) const
{    
    return OverlayNoBlurKey::operator==(key)
        && fabs(this->m_style.get().fGaussianBlur - key.m_style.get().fGaussianBlur) < 0.000001
        && this->m_style.get().fBlur == key.m_style.get().fBlur;
    //static_cast will call copy constructer to construct a tmp obj
    //return ((CWordCacheKey)(*this)==(CWordCacheKey)key) && (m_p.x==key.m_p.x) && (m_p.y==key.m_p.y) 
    //        && (m_org.x==key.m_org.x) && (m_org.y==key.m_org.y);
}

ULONG OverlayKey::UpdateHashValue()
{
    m_hash_value = __super::UpdateHashValue();
    m_hash_value += (m_hash_value<<5);
    m_hash_value += m_style.get().fBlur;
    m_hash_value += (m_hash_value<<5);
    m_hash_value += hash_value(m_style.get().fGaussianBlur);
    return  m_hash_value;
}


//////////////////////////////////////////////////////////////////////////////////////////////

// ScanLineDataCacheKey

bool ScanLineDataCacheKey::operator==( const ScanLineDataCacheKey& key ) const
{
    return (m_path_data && key.m_path_data) ? *m_path_data==*key.m_path_data : m_path_data==key.m_path_data;
}

//////////////////////////////////////////////////////////////////////////////////////////////

// OverlayNoOffsetKey

bool OverlayNoOffsetKey::operator==( const OverlayNoOffsetKey& key ) const
{
    return (this==&key) || ( this->m_border == key.m_border && this->m_rasterize_sub == key.m_rasterize_sub &&
        ScanLineDataCacheKey::operator==(key) );
}

//////////////////////////////////////////////////////////////////////////////////////////////

// ClipperAlphaMaskCacheKey

bool ClipperAlphaMaskCacheKey::operator==( const ClipperAlphaMaskCacheKey& key ) const
{
    bool result = false;
    if (m_clipper==key.m_clipper)
    {
        result = true;
    }
    else if ( m_clipper!=NULL && key.m_clipper!=NULL )
    {
        const CClipper& lhs = *m_clipper;
        const CClipper& rhs = *key.m_clipper;
        result = (lhs.m_polygon->m_str == rhs.m_polygon->m_str 
            && fabs(lhs.m_polygon->m_scalex - rhs.m_polygon->m_scalex) < 0.000001
            && fabs(lhs.m_polygon->m_scaley - rhs.m_polygon->m_scaley) < 0.000001
            && lhs.m_size == rhs.m_size        
            && lhs.m_inverse == rhs.m_inverse
            && lhs.m_effectType == rhs.m_effectType
            && lhs.m_effect == rhs.m_effect);//fix me: unsafe code
    }
    return result;
}

//////////////////////////////////////////////////////////////////////////////////////////////

// CacheManager

struct Caches
{
public:
    Caches()
    {
        s_clipper_alpha_mask_cache = NULL;

        s_text_info_cache = NULL;
        s_word_mru_cache = NULL;
        s_path_data_mru_cache = NULL;
        s_scan_line_data_2_mru_cache = NULL;
        s_overlay_no_blur_mru_cache = NULL;
        s_overlay_mru_cache = NULL;

        s_scan_line_data_mru_cache = NULL;
        s_overlay_no_offset_mru_cache = NULL;
		
        s_subpixel_variance_cache = NULL;
        s_ass_tag_list_cache = NULL;
    }
    ~Caches()
    {
        delete s_clipper_alpha_mask_cache;

        delete s_text_info_cache;
        delete s_word_mru_cache;
        delete s_path_data_mru_cache;
        delete s_scan_line_data_2_mru_cache;
        delete s_overlay_no_blur_mru_cache;
        delete s_overlay_mru_cache;

        delete s_scan_line_data_mru_cache;
        delete s_overlay_no_offset_mru_cache;

        delete s_subpixel_variance_cache;
        delete s_ass_tag_list_cache;
    }
public:
    ClipperAlphaMaskMruCache* s_clipper_alpha_mask_cache;

    TextInfoMruCache* s_text_info_cache;
    AssTagListMruCache* s_ass_tag_list_cache;

    ScanLineDataMruCache* s_scan_line_data_mru_cache;
    OverlayNoOffsetMruCache* s_overlay_no_offset_mru_cache;

    OverlayMruCache* s_subpixel_variance_cache;
    OverlayMruCache* s_overlay_mru_cache;
    OverlayNoBlurMruCache* s_overlay_no_blur_mru_cache;
    PathDataMruCache* s_path_data_mru_cache;
    ScanLineData2MruCache* s_scan_line_data_2_mru_cache;
    CWordMruCache* s_word_mru_cache;
};

static Caches s_caches;

OverlayMruCache* CacheManager::GetOverlayMruCache()
{
    if(s_caches.s_overlay_mru_cache==NULL)
    {
        s_caches.s_overlay_mru_cache = new OverlayMruCache(OVERLAY_CACHE_ITEM_NUM);
    }
    return s_caches.s_overlay_mru_cache;
}

PathDataMruCache* CacheManager::GetPathDataMruCache()
{
    if (s_caches.s_path_data_mru_cache==NULL)
    {
        s_caches.s_path_data_mru_cache = new PathDataMruCache(PATH_CACHE_ITEM_NUM);
    }
    return s_caches.s_path_data_mru_cache;
}

CWordMruCache* CacheManager::GetCWordMruCache()
{
    if(s_caches.s_word_mru_cache==NULL)
    {
        s_caches.s_word_mru_cache = new CWordMruCache(WORD_CACHE_ITEM_NUM);
    }
    return s_caches.s_word_mru_cache;
}

OverlayNoBlurMruCache* CacheManager::GetOverlayNoBlurMruCache()
{
    if(s_caches.s_overlay_no_blur_mru_cache==NULL)
    {
        s_caches.s_overlay_no_blur_mru_cache = new OverlayNoBlurMruCache(OVERLAY_NO_BLUR_CACHE_ITEM_NUM);
    }
    return s_caches.s_overlay_no_blur_mru_cache;
}

ScanLineData2MruCache* CacheManager::GetScanLineData2MruCache()
{
    if(s_caches.s_scan_line_data_2_mru_cache==NULL)
    {
        s_caches.s_scan_line_data_2_mru_cache = new ScanLineData2MruCache(SCAN_LINE_DATA_CACHE_ITEM_NUM);
    }
    return s_caches.s_scan_line_data_2_mru_cache;
}

OverlayMruCache* CacheManager::GetSubpixelVarianceCache()
{
    if(s_caches.s_subpixel_variance_cache==NULL)
    {
        s_caches.s_subpixel_variance_cache = new OverlayMruCache(SUBPIXEL_VARIANCE_CACHE_ITEM_NUM);
    }
    return s_caches.s_subpixel_variance_cache;    
}

ScanLineDataMruCache* CacheManager::GetScanLineDataMruCache()
{
    if(s_caches.s_scan_line_data_mru_cache==NULL)
    {
        s_caches.s_scan_line_data_mru_cache = new ScanLineDataMruCache(SCAN_LINE_DATA_CACHE_ITEM_NUM);
    }
    return s_caches.s_scan_line_data_mru_cache;
}

OverlayNoOffsetMruCache* CacheManager::GetOverlayNoOffsetMruCache()
{
    if(s_caches.s_overlay_no_offset_mru_cache==NULL)
    {
        s_caches.s_overlay_no_offset_mru_cache = new OverlayNoOffsetMruCache(OVERLAY_NO_BLUR_CACHE_ITEM_NUM);
    }
    return s_caches.s_overlay_no_offset_mru_cache;    
}

AssTagListMruCache* CacheManager::GetAssTagListMruCache()
{
    if(s_caches.s_ass_tag_list_cache==NULL)
    {
        s_caches.s_ass_tag_list_cache = new AssTagListMruCache(ASS_TAG_LIST_CACHE_ITEM_NUM);
    }
    return s_caches.s_ass_tag_list_cache;  
}

TextInfoMruCache* CacheManager::GetTextInfoCache()
{
    if(s_caches.s_text_info_cache==NULL)
    {
        s_caches.s_text_info_cache = new TextInfoMruCache(TEXT_INFO_CACHE_ITEM_NUM);
    }
    return s_caches.s_text_info_cache;
}

ClipperAlphaMaskMruCache* CacheManager::GetClipperAlphaMaskMruCache()
{
    if(s_caches.s_clipper_alpha_mask_cache==NULL)
    {
        s_caches.s_clipper_alpha_mask_cache = new ClipperAlphaMaskMruCache(CLIPPER_ALPHA_MASK_MRU_CACHE);
    }
    return s_caches.s_clipper_alpha_mask_cache;
}
