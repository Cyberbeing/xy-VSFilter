#include "stdafx.h"
#include "xy_overlay_paint_machine.h"
#include "RTS.h"
#include "Rasterizer.h"
#include "cache_manager.h"
#include "subpixel_position_controler.h"

//////////////////////////////////////////////////////////////////////////
//
// OverlayPaintMachine
//

void OverlayPaintMachine::Paint(SharedPtrOverlay* overlay)
{
    if(!m_word->m_str || overlay==NULL) return;
    bool error = false;
    do 
    {
        CPoint trans_org2 = m_trans_org;    
        bool need_transform = m_word->NeedTransform();
        if(!need_transform)
        {
            trans_org2.x=0;
            trans_org2.y=0;
        }

        CPoint psub_true( (m_psub.x&SubpixelPositionControler::EIGHT_X_EIGHT_MASK), (m_psub.y&SubpixelPositionControler::EIGHT_X_EIGHT_MASK) );
        OverlayKey sub_key(*m_word, psub_true, trans_org2);
        sub_key.UpdateHashValue();
        if( SubpixelPositionControler::GetGlobalControler().UseBilinearShift() )
        {
            OverlayMruCache* overlay_cache = CacheManager::GetSubpixelVarianceCache();

            POSITION pos = overlay_cache->Lookup(sub_key);
            if(pos!=NULL) 
            {
                *overlay = overlay_cache->GetAt(pos);
                overlay_cache->UpdateCache( pos );
            }
        }
        if( !overlay->get() )
        {
            CPoint psub = SubpixelPositionControler::GetGlobalControler().GetSubpixel(m_psub);
            OverlayKey overlay_key(*m_word, psub, trans_org2);
            overlay_key.UpdateHashValue();
            OverlayMruCache* overlay_cache = CacheManager::GetOverlayMruCache();
            POSITION pos = overlay_cache->Lookup(overlay_key);
            if(pos==NULL)
            {
                if( !m_word->DoPaint(psub, trans_org2, overlay, overlay_key) )
                {
                    error = true;
                    break;
                }                
            }
            else
            {
                *overlay = overlay_cache->GetAt(pos);
                overlay_cache->UpdateCache( pos );
            }
            CWord::PaintFromOverlay(m_psub, trans_org2, sub_key, *overlay);
        }
    } while(false);
    if(error)
    {
        overlay->reset( new Overlay() );
    }
}
