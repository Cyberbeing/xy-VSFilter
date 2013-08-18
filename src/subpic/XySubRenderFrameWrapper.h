#pragma once

#include "../subtitles/flyweight_base_types.h"

class CMemSubPic;
interface IXySubRenderFrame;

class XySubRenderFrameWrapper : public CUnknown, public IXySubRenderFrame
{
public:
    XySubRenderFrameWrapper(CMemSubPic * inner_obj, const CRect& output_rect, const CRect& clip_rect, 
        ULONGLONG id,
        HRESULT *phr=NULL);
    virtual ~XySubRenderFrameWrapper(){}

    DECLARE_IUNKNOWN;
    STDMETHODIMP NonDelegatingQueryInterface(REFIID riid, void** ppv);

    STDMETHODIMP GetOutputRect(RECT *outputRect);
    STDMETHODIMP GetClipRect(RECT *clipRect);
    STDMETHODIMP GetXyColorSpace(int *xyColorSpace);
    STDMETHODIMP GetBitmapCount(int *count);
    STDMETHODIMP GetBitmap(int index, ULONGLONG *id, POINT *position, SIZE *size, LPCVOID *pixels, int *pitch);
    STDMETHODIMP GetBitmapExtra(int index, LPVOID extra_info);
private:
    CComPtr<CMemSubPic> m_inner_obj;
    CRect m_output_rect, m_clip_rect;
    ULONGLONG m_id;
};

class XySubRenderFrameWrapper2 : public CUnknown, public IXySubRenderFrame
{
public:
    XySubRenderFrameWrapper2(IXySubRenderFrame *inner_obj, ULONGLONG context_id);
    virtual ~XySubRenderFrameWrapper2(){}

    DECLARE_IUNKNOWN;
    STDMETHODIMP NonDelegatingQueryInterface(REFIID riid, void** ppv);

    STDMETHODIMP GetOutputRect(RECT *outputRect);
    STDMETHODIMP GetClipRect(RECT *clipRect);
    STDMETHODIMP GetXyColorSpace(int *xyColorSpace);
    STDMETHODIMP GetBitmapCount(int *count);
    STDMETHODIMP GetBitmap(int index, ULONGLONG *id, POINT *position, SIZE *size, LPCVOID *pixels, int *pitch);
    STDMETHODIMP GetBitmapExtra(int index, LPVOID extra_info);

private:
    CComPtr<IXySubRenderFrame> m_inner_obj;
    CAtlArray<ULONGLONG> m_ids;
};

#ifdef SUBTITLE_FRAME_DUMP_FILE
HRESULT DumpSubRenderFrame( IXySubRenderFrame *sub, const char * filename );
HRESULT DumpToBitmap( IXySubRenderFrame *sub, LPCTSTR filename, bool dump_alpha_channel, bool dump_rgb_bmp
    , DWORD background_color );
#endif
