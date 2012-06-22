#ifndef __XY_OVERLAY_PAINT_MACHINE_47C522B7_4317_441C_A536_5F667DCB0141_H__
#define __XY_OVERLAY_PAINT_MACHINE_47C522B7_4317_441C_A536_5F667DCB0141_H__

#include <boost/shared_ptr.hpp>
class CWord;
typedef ::boost::shared_ptr<CWord> SharedPtrCWord;

struct Overlay;
typedef ::boost::shared_ptr<Overlay> SharedPtrOverlay;

class OverlayPaintMachine
{
public:
    OverlayPaintMachine(const SharedPtrCWord& word, const CPoint& psub, 
        const CPoint& trans_org)
        : m_word(word)
        , m_psub(psub)
        , m_trans_org(trans_org){}
 
    void Paint(SharedPtrOverlay* overlay);
private:
    SharedPtrCWord m_word;
    CPoint m_psub;
    CPoint m_trans_org;
};

#endif // __XY_OVERLAY_PAINT_MACHINE_47C522B7_4317_441C_A536_5F667DCB0141_H__