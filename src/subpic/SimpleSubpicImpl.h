#pragma once

#include "ISimpleSubPic.h"

struct IXySubRenderFrame;

class SimpleSubpic : public CUnknown, public ISimpleSubPic
{
public:
    SimpleSubpic(IXySubRenderFrame*sub_render_frame, int alpha_blt_dst_type);
    ~SimpleSubpic();
public:
    DECLARE_IUNKNOWN;
    STDMETHODIMP NonDelegatingQueryInterface(REFIID riid, void** ppv);

    STDMETHODIMP AlphaBlt(SubPicDesc* target);
private:
    struct Bitmap
    {
        ULONGLONG id;
        LPCVOID pixels;
        POINT pos;
        SIZE size;
        int pitch;
        XyPlannerFormatExtra extra;
    };
private:
    SimpleSubpic(const SimpleSubpic&);
    void operator=(const SimpleSubpic&)const;

    HRESULT AlphaBltAnv12_P010( SubPicDesc* target, const Bitmap& src );
    HRESULT AlphaBltAnv12_Nv12(SubPicDesc* target, const Bitmap& src);
    HRESULT AlphaBlt(SubPicDesc* target, const Bitmap& src);
    HRESULT ConvertColorSpace();
    void SubsampleAndInterlace(int index, Bitmap*bitmap, bool u_first );
private:
    CComPtr<IXySubRenderFrame> m_sub_render_frame;

    CAtlArray<Bitmap> m_bitmap;
    CAtlArray<BYTE*> m_buffers;

    int m_bitmap_count;
    int m_alpha_blt_dst_type;
};