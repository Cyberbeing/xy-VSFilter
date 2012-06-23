#ifndef __XY_BITMAP_674EB983_2A49_432D_A750_62C3B3E9DA67_H__
#define __XY_BITMAP_674EB983_2A49_432D_A750_62C3B3E9DA67_H__

#include <atltypes.h>

class XyBitmap
{
public:
    enum MemLayout { PACK, PLANNA, RESERVED };

    int type;
    
    int x, y, w, h;
    int pitch, bpp;
    void* bits;
    BYTE* plans[4];
public:
    XyBitmap()
    {
        x = y = w = h = 0;
        type = 0;
        pitch = 0;
        bits = NULL;
        plans[0] = plans[1] = plans[2] = plans[3] = NULL;
    }
    ~XyBitmap();

    static XyBitmap *CreateBitmap(const CRect& target_rect, MemLayout layout);    
    static void BitBlt(SubPicDesc& spd, const XyBitmap& bitmap);
private:
    static void ClearBitmap(XyBitmap *bitmap);
};

#endif // __XY_BITMAP_674EB983_2A49_432D_A750_62C3B3E9DA67_H__
