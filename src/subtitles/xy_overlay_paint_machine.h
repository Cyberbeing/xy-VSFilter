#ifndef __XY_OVERLAY_PAINT_MACHINE_47C522B7_4317_441C_A536_5F667DCB0141_H__
#define __XY_OVERLAY_PAINT_MACHINE_47C522B7_4317_441C_A536_5F667DCB0141_H__

#include <boost/shared_ptr.hpp>
class CWord;
typedef ::boost::shared_ptr<CWord> SharedPtrCWord;

struct Overlay;
typedef ::boost::shared_ptr<Overlay> SharedPtrOverlay;

class OverlayPaintMachine;
typedef ::boost::shared_ptr<OverlayPaintMachine> SharedPtrOverlayPaintMachine;

class OverlayKey;
typedef ::boost::shared_ptr<OverlayKey> SharedPtrOverlayKey;

class CWordPaintMachine
{
public:
    enum LAYER 
    {
        SHADOW = 0
        , OUTLINE
        , BODY
    };
public:
    static void CreatePaintMachines(const SharedPtrCWord& word
        , const CPoint& shadow_pos, const CPoint& outline_pos, const CPoint& body_pos
        , const CPoint& org
        , SharedPtrOverlayPaintMachine *shadow, SharedPtrOverlayPaintMachine *outline, SharedPtrOverlayPaintMachine *body);

    static void PaintBody(const SharedPtrCWord& word, const CPoint& p, const CPoint& org, SharedPtrOverlay* overlay);

    void Paint(LAYER layer, SharedPtrOverlay* overlay);
    const SharedPtrOverlayKey& GetHashKey(LAYER layer);
private:
    CWordPaintMachine(){}

    void PaintBody(const SharedPtrCWord& word, const CPoint& p, SharedPtrOverlay* overlay);
    void PaintOutline(const SharedPtrCWord& word, const CPoint& p, SharedPtrOverlay* overlay);
    void PaintShadow(const SharedPtrCWord& word, const CPoint& p, SharedPtrOverlay* overlay);

    OverlayKey* CreateBodyOverlayHashKey(const SharedPtrCWord& word, const CPoint& p);
    OverlayKey* CreateOutlineOverlayHashKey(const SharedPtrCWord& word, const CPoint& p);
    OverlayKey* CreateShadowOverlayHashKey(const SharedPtrCWord& word, const CPoint& p);

    SharedPtrCWord m_word;
    CPoint m_shadow_pos, m_outline_pos, m_body_pos;
    CPoint m_trans_org;

    SharedPtrOverlayKey m_shadow_key, m_border_key, m_body_key;
};

typedef ::boost::shared_ptr<CWordPaintMachine> SharedCWordPaintMachine;

class OverlayPaintMachine
{
public:
    OverlayPaintMachine(const SharedCWordPaintMachine& inner_paint_machine, CWordPaintMachine::LAYER layer)
        : m_inner_paint_machine(inner_paint_machine)
        , m_layer(layer) {}
 
    void Paint(SharedPtrOverlay* overlay);
    CRect CalcDirtyRect();//return a 8x8 supersampled rect
    const SharedPtrOverlayKey& GetHashKey();
private:
    SharedCWordPaintMachine m_inner_paint_machine;
    CWordPaintMachine::LAYER m_layer;

    SharedPtrOverlay m_overlay;
};


#endif // __XY_OVERLAY_PAINT_MACHINE_47C522B7_4317_441C_A536_5F667DCB0141_H__
