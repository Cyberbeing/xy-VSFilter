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

    static void FlipAlphaValue( LPVOID pixels, int w, int h, int pitch );
    static void AlphaBltPack(SubPicDesc& spd, POINT pos, SIZE size, LPCVOID pixels, int pitch);
    static void AlphaBltPlannar(SubPicDesc& spd, POINT pos, SIZE size, const XyPlannerFormatExtra& pixels, int pitch);

    static void BltPack( SubPicDesc& spd, POINT pos, SIZE size, LPCVOID pixels, int pitch );
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

    void MoveTo(int x, int y);
public:
    CAtlArray<SharedBitmap> m_bitmaps;
    CAtlArray<int> m_bitmap_ids;
    CRect m_output_rect;
    CRect m_clip_rect;
    XyColorSpace m_xy_color_space;

    int m_left, m_top;
};

typedef ::boost::shared_ptr<XySubRenderFrame> SharedPtrXySubRenderFrame;

class XySubRenderFrameCreater
{
public:
    static XySubRenderFrameCreater* GetDefaultCreater();

    HRESULT SetOutputRect(const RECT& output_rect);
    HRESULT SetClipRect(const RECT& clip_rect);
    HRESULT SetColorSpace(XyColorSpace color_space);
    HRESULT SetVsfilterCompactRgbCorrection(bool value);
    HRESULT SetRgbOutputTvLevel(bool value);

    HRESULT GetOutputRect(RECT *output_rect);
    HRESULT GetClipRect(RECT *clip_rect);
    HRESULT GetColorSpace(XyColorSpace *color_space);
    bool GetVsfilterCompactRgbCorrection();
    bool GetRgbOutputTvLevel();

    XySubRenderFrame* NewXySubRenderFrame(UINT bitmap_count);
    XyBitmap* CreateBitmap(const RECT& target_rect);
    DWORD TransColor(DWORD argb);

    XySubRenderFrameCreater():m_xy_color_space(XY_CS_ARGB), m_bitmap_layout(XyBitmap::PACK)
        , m_vsfilter_compact_rgb_correction(false), m_rgb_output_tv_level(false){}
private:
    CRect m_output_rect;
    CRect m_clip_rect;
    XyColorSpace m_xy_color_space;
    bool m_vsfilter_compact_rgb_correction;
    bool m_rgb_output_tv_level;
    XyBitmap::MemLayout m_bitmap_layout;
};


#endif // __XY_BITMAP_674EB983_2A49_432D_A750_62C3B3E9DA67_H__
