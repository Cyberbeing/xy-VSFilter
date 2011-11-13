/************************************************************************/
/* author: xy                                                           */
/* date: 20110926                                                       */
/************************************************************************/
#include "StdAfx.h"
#include "cache_manager.h"

std::size_t hash_value( const TextInfoCacheKey& key )
{
    size_t hash = CStringElementTraits<CString>::Hash(key.m_str);
    hash += (hash<<5);
    hash += hash_value( static_cast<const STSStyleBase&>(key.m_style.get()) );
    hash += (hash<<5);
    hash += hash_value( key.m_style.get().fontScaleX );
    hash += (hash<<5);
    hash += hash_value( key.m_style.get().fontScaleY );
    hash += (hash<<5);
    hash += hash_value( key.m_style.get().fontSpacing );
    return hash;
}

std::size_t hash_value(const CWord& key)
{
    return( CStringElementTraits<CString>::Hash(key.m_str) );
}

std::size_t hash_value(const CWordCacheKey& key)
{
    return( CStringElementTraits<CString>::Hash(key.m_str) );
}

std::size_t hash_value( const PathDataCacheKey& key )
{
    return( CStringElementTraits<CString>::Hash(key.m_str) ); 
}

ULONG ScanLineDataCacheKeyTraits::Hash( const ScanLineDataCacheKey& key )
{
    //return hash_value(static_cast<PathDataCacheKey>(key)) ^ key.m_org.x ^ key.m_org.y;
    size_t hash = hash_value(static_cast<const PathDataCacheKey&>(key));
    hash += (hash<<5);
    hash += key.m_org.x;
    hash += (hash<<5);
    hash += key.m_org.y;
    return  hash;
}

ULONG OverlayNoBlurKeyTraits::Hash( const OverlayNoBlurKey& key )
{
    ULONG hash = ScanLineDataCacheKeyTraits::Hash(static_cast<const ScanLineDataCacheKey&>(key));
    hash += (hash<<5);
    hash += key.m_p.x;
    hash += (hash<<5);
    hash += key.m_p.y;
    return  hash;
}

std::size_t hash_value(const OverlayKey& key)
{
    size_t hash = OverlayNoBlurKeyTraits::Hash(static_cast<const OverlayNoBlurKey&>(key));
    hash += (hash<<5);
    hash += key.m_style.get().fBlur;
    hash += (hash<<5);
    hash += hash_value(key.m_style.get().fGaussianBlur);
    return  hash;
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

// ScanLineDataCacheKey

bool ScanLineDataCacheKey::operator==( const ScanLineDataCacheKey& key ) const
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
    return ScanLineDataCacheKey::operator==(key) && (m_p.x==key.m_p.x) && (m_p.y==key.m_p.y);
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

//////////////////////////////////////////////////////////////////////////////////////////////

// CacheManager
TextInfoMruCache* CacheManager::s_text_info_cache = NULL;
CWordMruCache* CacheManager::s_word_mru_cache = NULL;
PathDataMruCache* CacheManager::s_path_data_mru_cache = NULL;
ScanLineDataMruCache* CacheManager::s_scan_line_data_mru_cache = NULL;
OverlayNoBlurMruCache* CacheManager::s_overlay_no_blur_mru_cache = NULL;
OverlayMruCache* CacheManager::s_overlay_mru_cache = NULL;
OverlayMruCache* CacheManager::s_subpixel_variance_cache = NULL;
AssTagListMruCache* CacheManager::s_ass_tag_list_cache = NULL;

OverlayMruCache* CacheManager::GetOverlayMruCache()
{
    if(s_overlay_mru_cache==NULL)
    {
        s_overlay_mru_cache = new OverlayMruCache(OVERLAY_CACHE_ITEM_NUM);
    }
    return s_overlay_mru_cache;
}

PathDataMruCache* CacheManager::GetPathDataMruCache()
{
    if (s_path_data_mru_cache==NULL)
    {
        s_path_data_mru_cache = new PathDataMruCache(PATH_CACHE_ITEM_NUM);
    }
    return s_path_data_mru_cache;
}

CWordMruCache* CacheManager::GetCWordMruCache()
{
    if(s_word_mru_cache==NULL)
    {
        s_word_mru_cache = new CWordMruCache(WORD_CACHE_ITEM_NUM);
    }
    return s_word_mru_cache;
}

OverlayNoBlurMruCache* CacheManager::GetOverlayNoBlurMruCache()
{
    if(s_overlay_no_blur_mru_cache==NULL)
    {
        s_overlay_no_blur_mru_cache = new OverlayNoBlurMruCache(OVERLAY_NO_BLUR_CACHE_ITEM_NUM);
    }
    return s_overlay_no_blur_mru_cache;
}

ScanLineDataMruCache* CacheManager::GetScanLineDataMruCache()
{
    if(s_scan_line_data_mru_cache==NULL)
    {
        s_scan_line_data_mru_cache = new ScanLineDataMruCache(SCAN_LINE_DATA_CACHE_ITEM_NUM);
    }
    return s_scan_line_data_mru_cache;
}

OverlayMruCache* CacheManager::GetSubpixelVarianceCache()
{
    if(s_subpixel_variance_cache==NULL)
    {
        s_subpixel_variance_cache = new OverlayMruCache(SUBPIXEL_VARIANCE_CACHE_ITEM_NUM);
    }
    return s_subpixel_variance_cache;    
}

AssTagListMruCache* CacheManager::GetAssTagListMruCache()
{
    if(s_ass_tag_list_cache==NULL)
    {
        s_ass_tag_list_cache = new AssTagListMruCache(ASS_TAG_LIST_CACHE_ITEM_NUM);
    }
    return s_ass_tag_list_cache;  
}

TextInfoMruCache* CacheManager::GetTextInfoCache()
{
    if(s_text_info_cache==NULL)
    {
        s_text_info_cache = new TextInfoMruCache(TEXT_INFO_CACHE_ITEM_NUM);
    }
    return s_text_info_cache;
}
