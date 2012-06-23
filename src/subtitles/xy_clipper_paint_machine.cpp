#include "stdafx.h"
#include "xy_clipper_paint_machine.h"
#include "RTS.h"
#include "Rasterizer.h"
#include "cache_manager.h"
#include "subpixel_position_controler.h"

//////////////////////////////////////////////////////////////////////////
//
// CClipperPaintMachine
//

void CClipperPaintMachine::Paint(SharedPtrGrayImage2 *output)
{
    ASSERT(output);
    if (m_clipper!=NULL)
    {
        ClipperAlphaMaskCacheKey key(m_clipper);
        key.UpdateHashValue();
        ClipperAlphaMaskMruCache * cache = CacheManager::GetClipperAlphaMaskMruCache();
        POSITION pos = cache->Lookup(key);
        if( pos!=NULL )
        {
            *output = cache->GetAt(pos);
            cache->UpdateCache(pos);
        }
        else
        {
            (*output).reset(m_clipper->Paint());
            cache->UpdateCache(key, *output);
        }
    }
}

CRect CClipperPaintMachine::CalcDirtyRect()
{
    return CRect(0,0,INT_MAX, INT_MAX);//fix me: not a decent state machine yet
}

const ClipperAlphaMaskCacheKey& CClipperPaintMachine::GetHashKey()
{
    return m_hash_key;
}
