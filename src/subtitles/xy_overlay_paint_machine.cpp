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
    if (!m_overlay)
    {
        m_inner_paint_machine->Paint(m_layer, &m_overlay);
    }
    if(overlay)
        *overlay = m_overlay;
}

CRectCoor2 OverlayPaintMachine::CalcDirtyRect()
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

const SharedPtrOverlayKey& OverlayPaintMachine::GetHashKey()
{
    return m_inner_paint_machine->GetHashKey(m_layer);
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

void CWordPaintMachine::PaintBody( const SharedPtrCWord& word, const CPointCoor2& p, SharedPtrOverlay* overlay )
{
    if(!word->m_str.Get() || overlay==NULL) return;
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
        overlay->reset( DEBUG_NEW Overlay() );
    }
}

void CWordPaintMachine::PaintOutline( const SharedPtrCWord& word, const CPointCoor2& p, SharedPtrOverlay* overlay )
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

void CWordPaintMachine::PaintShadow( const SharedPtrCWord& word, const CPointCoor2& p, SharedPtrOverlay* overlay )
{
    PaintOutline(word, p, overlay);
}

void CWordPaintMachine::CreatePaintMachines( const SharedPtrCWord& word
    , const CPointCoor2& shadow_pos, const CPointCoor2& outline_pos, const CPointCoor2& body_pos
    , const CPointCoor2& org
    , SharedPtrOverlayPaintMachine *shadow, SharedPtrOverlayPaintMachine *outline, SharedPtrOverlayPaintMachine *body)
{
    SharedCWordPaintMachine machine(DEBUG_NEW CWordPaintMachine());
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

    machine->m_body_key.reset( machine->CreateBodyOverlayHashKey(word, machine->m_body_pos) );
    machine->m_border_key.reset( machine->CreateOutlineOverlayHashKey(word, machine->m_outline_pos) );
    machine->m_shadow_key.reset( machine->CreateShadowOverlayHashKey(word, machine->m_shadow_pos) );

    if (shadow)
    {
        shadow->reset( DEBUG_NEW OverlayPaintMachine(machine, SHADOW) );
    }
    if (outline)
    {
        outline->reset( DEBUG_NEW OverlayPaintMachine(machine, OUTLINE) );
    }
    if (body)
    {
        body->reset( DEBUG_NEW OverlayPaintMachine(machine, BODY) );
    }
}

void CWordPaintMachine::PaintBody( const SharedPtrCWord& word, const CPointCoor2& p, const CPointCoor2& org, SharedPtrOverlay* overlay )
{
    CWordPaintMachine machine;
    machine.m_word = word;
    machine.m_trans_org = org - p;//IMPORTANT! NOTE: not totally initiated
    machine.m_body_pos = p;
    machine.m_body_key.reset( machine.CreateBodyOverlayHashKey(word, machine.m_body_pos) );
    machine.PaintBody(word, p, overlay);
}

const SharedPtrOverlayKey& CWordPaintMachine::GetHashKey(LAYER layer)
{
    //fix me: sharing keys with paint functions
    switch (layer)
    {
    case SHADOW:
        return m_shadow_key;
        break;
    case OUTLINE:
        return m_border_key;
        break;
    case BODY:
        return m_body_key;
        break;
    }
    ASSERT(0);
    return m_body_key;
}

OverlayKey* CWordPaintMachine::CreateBodyOverlayHashKey( const SharedPtrCWord& word, const CPointCoor2& p )
{
    CPoint trans_org2 = m_trans_org;    
    bool need_transform = word->NeedTransform();
    if(!need_transform)
    {
        trans_org2.x=0;
        trans_org2.y=0;
    }
    CPoint psub_true( (p.x&SubpixelPositionControler::EIGHT_X_EIGHT_MASK), (p.y&SubpixelPositionControler::EIGHT_X_EIGHT_MASK) );
    OverlayKey *body_overlay_key=DEBUG_NEW OverlayKey(*word, psub_true, trans_org2);
    body_overlay_key->UpdateHashValue();
    return body_overlay_key;
}

OverlayKey* CWordPaintMachine::CreateOutlineOverlayHashKey( const SharedPtrCWord& word, const CPointCoor2& p )
{
    if (word->m_style.get().borderStyle==0)
    {
        return CreateBodyOverlayHashKey(word, p);
    }
    else if (word->m_style.get().borderStyle==1)
    {
        if(word->CreateOpaqueBox())
        {
            return CreateBodyOverlayHashKey(word->m_pOpaqueBox, p);
        }
    }
    ASSERT(0);
    return NULL;
}

OverlayKey* CWordPaintMachine::CreateShadowOverlayHashKey( const SharedPtrCWord& word, const CPointCoor2& p )
{
    return CreateOutlineOverlayHashKey(word, p);
}
