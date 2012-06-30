#ifndef __XY_BITMAP_674EB983_2A49_432D_A750_62C3B3E9DA67_H__
#define __XY_BITMAP_674EB983_2A49_432D_A750_62C3B3E9DA67_H__

#include <atltypes.h>
#include <boost/shared_ptr.hpp>
#include "XySubRenderIntf.h"

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
    static void AlphaBlt(SubPicDesc& spd, const XyBitmap& bitmap);
private:
    static void ClearBitmap(XyBitmap *bitmap);
};

class XySubRenderFrame: public CUnknown, public IXySubRenderFrame
{
public:
    DECLARE_IUNKNOWN;
    STDMETHODIMP NonDelegatingQueryInterface(REFIID riid, void** ppv);

    STDMETHODIMP GetOutputRect(RECT *outputRect);
    STDMETHODIMP GetClipRect(RECT *clipRect);
    STDMETHODIMP GetXyColorSpace(int *xyColorSpace);
    STDMETHODIMP GetBitmapCount(int *count);
    STDMETHODIMP GetBitmap(int index, ULONGLONG *id, POINT *position, SIZE *size, LPCVOID *pixels, int *pitch);
    STDMETHODIMP GetBitmapExtra(int index, LPVOID extra_info);
public:
    XySubRenderFrame();
    ~XySubRenderFrame();
    
    typedef ::boost::shared_ptr<XyBitmap> SharedBitmap;
public:
    CAtlArray<SharedBitmap> m_bitmaps;
    CAtlArray<int> m_bitmap_ids;
    CRect m_output_rect;
    CRect m_clip_rect;
    XyColorSpace m_xy_color_space;
};



#endif // __XY_BITMAP_674EB983_2A49_432D_A750_62C3B3E9DA67_H__
