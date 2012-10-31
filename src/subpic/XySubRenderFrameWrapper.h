#pragma once

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
