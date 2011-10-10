/************************************************************************/
/* author: xy                                                           */
/* date: 20110926                                                       */
/************************************************************************/
#include "StdAfx.h"
#include "cache_manager.h"

std::size_t hash_value(const CWord& key)
{
    return( CStringElementTraits<CString>::Hash(key.m_str.get()) );
}

std::size_t hash_value(const CWordCacheKey& key)
{
    return( CStringElementTraits<CString>::Hash(key.m_str.get()) );
}

std::size_t hash_value(const OverlayKey& key)
{
    return( CStringElementTraits<CString>::Hash(key.m_str.get()) ^ key.m_p.x ^ key.m_p.y );
}

std::size_t hash_value( const PathDataCacheKey& key )
{
    return( CStringElementTraits<CString>::Hash(key.m_str.get()) ); 
}

std::size_t hash_value( const ScanLineDataCacheKey& key )
{
    return hash_value(static_cast<PathDataCacheKey>(key)) ^ key.m_org.x ^ key.m_org.y;
}

std::size_t hash_value( const OverlayNoBlurKey& key )
{
    return hash_value(static_cast<ScanLineDataCacheKey>(key)) ^ key.m_p.x ^ key.m_p.y;
}

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
        lhs.fontWeight==rhs.fontWeight &&
        lhs.fItalic==rhs.fItalic &&
        lhs.fUnderline==rhs.fUnderline &&
        lhs.fStrikeOut==rhs.fStrikeOut;
}

//////////////////////////////////////////////////////////////////////////////////////////////

// ScanLineDataCacheKey

bool ScanLineDataCacheKey::operator==( const ScanLineDataCacheKey& key ) const
{ 
    return (static_cast<PathDataCacheKey>(*this)==static_cast<PathDataCacheKey>(key)) 
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
    return (static_cast<ScanLineDataCacheKey>(*this)==static_cast<ScanLineDataCacheKey>(key)) 
        && (m_p.x==key.m_p.x) && (m_p.y==key.m_p.y); 
}

//////////////////////////////////////////////////////////////////////////////////////////////

// CacheManager

CWordMruCache* CacheManager::s_word_mru_cache = NULL;
PathDataMruCache* CacheManager::s_path_data_mru_cache = NULL;
ScanLineDataMruCache* CacheManager::s_scan_line_data_mru_cache = NULL;
OverlayNoBlurMruCache* CacheManager::s_overlay_no_blur_mru_cache = NULL;
OverlayMruCache* CacheManager::s_overlay_mru_cache = NULL;

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

