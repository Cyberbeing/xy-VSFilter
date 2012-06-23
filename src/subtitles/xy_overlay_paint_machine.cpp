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
    m_inner_paint_machine->Paint(m_layer, overlay);
}

CRect OverlayPaintMachine::CalcDirtyRect()
{
    ASSERT(m_inner_paint_machine);
    m_inner_paint_machine->Paint(m_layer, &m_overlay);//fix me: not a decent state machine yet
    if (m_overlay)
    {
        int x = m_overlay->mOffsetX;
        int y = m_overlay->mOffsetY;
        int w = 8*m_overlay->mOverlayWidth;
        int h = 8*m_overlay->mOverlayHeight;
        return CRect(x, y, x+w, y+h);
    }
    return CRect(0,0,0,0);
}

const SharedPtrCWordPaintResultKey& OverlayPaintMachine::GetHashKey()
{
    return m_inner_paint_machine->GetHashKey();
}

void CWordPaintMachine::Paint( LAYER layer, SharedPtrOverlay* overlay )
{
    switch (layer)
    {
    case SHADOW:
        PaintShadow(m_word, m_shadow_pos, overlay);
        break;
    case OUTLINE:
        PaintOutline(m_word, m_outline_pos, overlay);
        break;
    case BODY:
        PaintBody(m_word, m_body_pos, overlay);
        break;
    }
}

void CWordPaintMachine::PaintBody( const SharedPtrCWord& word, const CPoint& p, SharedPtrOverlay* overlay )
{
    if(!word->m_str || overlay==NULL) return;
    bool error = false;
    do 
    {
        CPoint trans_org2 = m_trans_org;    
        bool need_transform = word->NeedTransform();
        if(!need_transform)
        {
            trans_org2.x=0;
            trans_org2.y=0;
        }

        CPoint psub_true( (p.x&SubpixelPositionControler::EIGHT_X_EIGHT_MASK), (p.y&SubpixelPositionControler::EIGHT_X_EIGHT_MASK) );
        OverlayKey sub_key(*word, psub_true, trans_org2);
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
            CPoint psub = SubpixelPositionControler::GetGlobalControler().GetSubpixel(p);
            OverlayKey overlay_key(*word, psub, trans_org2);
            overlay_key.UpdateHashValue();
            OverlayMruCache* overlay_cache = CacheManager::GetOverlayMruCache();
            POSITION pos = overlay_cache->Lookup(overlay_key);
            if(pos==NULL)
            {
                if( !word->DoPaint(psub, trans_org2, overlay, overlay_key) )
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
            CWord::PaintFromOverlay(p, trans_org2, sub_key, *overlay);
        }
    } while(false);
    if(error)
    {
        overlay->reset( new Overlay() );
    }
}

void CWordPaintMachine::PaintOutline( const SharedPtrCWord& word, const CPoint& p, SharedPtrOverlay* overlay )
{
    if (word->m_style.get().borderStyle==0)
    {
        PaintBody(word, p, overlay);
    }
    else if (word->m_style.get().borderStyle==1)
    {
        if(word->CreateOpaqueBox())
        {
            PaintBody(word->m_pOpaqueBox, p, overlay);
        }
    }
}

void CWordPaintMachine::PaintShadow( const SharedPtrCWord& word, const CPoint& p, SharedPtrOverlay* overlay )
{
    PaintOutline(word, p, overlay);
}

void CWordPaintMachine::CreatePaintMachines( const SharedPtrCWord& word
    , const CPoint& shadow_pos, const CPoint& outline_pos, const CPoint& body_pos
    , const CPoint& org
    , SharedPtrOverlayPaintMachine *shadow, SharedPtrOverlayPaintMachine *outline, SharedPtrOverlayPaintMachine *body)
{
    SharedCWordPaintMachine machine(new CWordPaintMachine());
    machine->m_word = word;
    if (shadow!=NULL)
    {
        machine->m_trans_org = org - shadow_pos;
    }
    else if (outline!=NULL)
    {
        machine->m_trans_org = org - outline_pos;
    }
    else if (body!=NULL)
    {
        machine->m_trans_org = org - body_pos;
    }
    machine->m_shadow_pos = shadow_pos;
    machine->m_outline_pos = outline_pos;
    machine->m_body_pos = body_pos;
    machine->m_key.reset( new CWordPaintResultKey(word, 
        machine->m_shadow_pos, machine->m_outline_pos, machine->m_body_pos, machine->m_trans_org) );
    machine->m_key->UpdateHashValue();
    if (shadow)
    {
        shadow->reset( new OverlayPaintMachine(machine, SHADOW) );
    }
    if (outline)
    {
        outline->reset( new OverlayPaintMachine(machine, OUTLINE) );
    }
    if (body)
    {
        body->reset( new OverlayPaintMachine(machine, BODY) );
    }
}

void CWordPaintMachine::PaintBody( const SharedPtrCWord& word, const CPoint& p, const CPoint& org, SharedPtrOverlay* overlay )
{
    CWordPaintMachine machine;
    machine.m_word = word;
    machine.m_trans_org = org - p;//IMPORTANT! NOTE: not totally initiated
    machine.m_body_pos = p;
    machine.m_key.reset( new CWordPaintResultKey(word, 
        machine.m_shadow_pos, machine.m_outline_pos, machine.m_body_pos, machine.m_trans_org) );
    machine.m_key->UpdateHashValue();
    machine.PaintBody(word, p, overlay);
}

const SharedPtrCWordPaintResultKey& CWordPaintMachine::GetHashKey()
{
    ASSERT(m_key);
    return  m_key;
}
