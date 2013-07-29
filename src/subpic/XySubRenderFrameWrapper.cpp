#include "stdafx.h"
#include "XySubRenderFrameWrapper.h"

#if ENABLE_XY_LOG_SUB_RENDER_FRAME
#  define TRACE_SUB_RENDER_FRAME(msg) XY_LOG_TRACE(msg)
#else
#  define TRACE_SUB_RENDER_FRAME(msg)
#endif

XySubRenderFrameWrapper::XySubRenderFrameWrapper( CMemSubPic * inner_obj, const CRect& output_rect
    , const CRect& clip_rect, ULONGLONG id, HRESULT *phr/*=NULL*/ )
    : CUnknown(NAME("XySubRenderFrameWrapper"), NULL, phr)
    , m_inner_obj(inner_obj)
    , m_output_rect(output_rect)
    , m_clip_rect(clip_rect)
    , m_id(id)
{
    ASSERT(m_inner_obj);
    if (!m_inner_obj && phr)
    {
        *phr = E_FAIL;
    }
}

STDMETHODIMP XySubRenderFrameWrapper::NonDelegatingQueryInterface( REFIID riid, void** ppv )
{
    return
        QI(IXySubRenderFrame)
        __super::NonDelegatingQueryInterface(riid, ppv);
}

STDMETHODIMP XySubRenderFrameWrapper::GetOutputRect( RECT *outputRect )
{
    TRACE_SUB_RENDER_FRAME(outputRect<<" "<<m_output_rect);
    if (!outputRect)
    {
        return E_INVALIDARG;
    }
    *outputRect = m_output_rect;
    return S_OK;
}

STDMETHODIMP XySubRenderFrameWrapper::GetClipRect( RECT *clipRect )
{
    TRACE_SUB_RENDER_FRAME(clipRect<<" "<<m_clip_rect);
    if (!clipRect)
    {
        return E_INVALIDARG;
    }
    *clipRect = m_clip_rect;
    return S_OK;
}

STDMETHODIMP XySubRenderFrameWrapper::GetXyColorSpace( int *xyColorSpace )
{
    TRACE_SUB_RENDER_FRAME(xyColorSpace);
    if (!xyColorSpace)
    {
        return S_FALSE;
    }
    *xyColorSpace = XY_CS_ARGB_F;
    return S_OK;
}

STDMETHODIMP XySubRenderFrameWrapper::GetBitmapCount( int *count )
{
    TRACE_SUB_RENDER_FRAME(count<<" "<<1);
    if (!count)
    {
        return S_FALSE;
    }
    *count = 1;
    return S_OK;
}

STDMETHODIMP XySubRenderFrameWrapper::GetBitmap( int index, ULONGLONG *id, POINT *position, SIZE *size
    , LPCVOID *pixels, int *pitch )
{
    if (index!=0)
    {
        return E_INVALIDARG;
    }
    if (!id && !position && !size && !pixels && !pitch)
    {
        return S_FALSE;
    }
    if (id)
    {
        *id = m_id;
    }
    CRect dirty_rect;
    HRESULT hr = m_inner_obj->GetDirtyRect(&dirty_rect);
    if (FAILED(hr))
    {
        return hr;
    }
    if (position)
    {
        *position = dirty_rect.TopLeft();
    }
    if (size)
    {
        *size = dirty_rect.Size();
    }
    if (pitch)
    {
        *pitch = m_inner_obj->m_spd.pitch;
    }
    if (pixels)
    {
        ASSERT(m_inner_obj->m_spd.bpp==32);
        *pixels = (BYTE*)(m_inner_obj->m_spd.bits) + dirty_rect.top * m_inner_obj->m_spd.pitch 
            + dirty_rect.left * (32/8);
    }
    TRACE_SUB_RENDER_FRAME(index<<" id: "<<m_id<<" "<<dirty_rect<<" pitch:"<<m_inner_obj->m_spd.pitch<<" pixels:"<<(pixels?*pixels:NULL)
        <<XY_LOG_VAR_2_STR(m_inner_obj->m_spd.bits) );
    return S_OK;
}

STDMETHODIMP XySubRenderFrameWrapper::GetBitmapExtra( int index, LPVOID extra_info )
{
    if (index!=0)
    {
        return E_INVALIDARG;
    }
    return E_NOTIMPL;
}

//
// ContextedBitmapId
//

class ContextedBitmapId
{
public:
    ContextedBitmapId(ULONGLONG context_id, ULONGLONG bitmap_id):m_context_id(context_id),m_bitmap_id(bitmap_id){}
    bool operator==(const ContextedBitmapId& key) const
    {
        return key.m_context_id == m_context_id && key.m_bitmap_id == m_bitmap_id;
    }

public:
    ULONGLONG m_context_id, m_bitmap_id;
};

class ContextedBitmapIdTraits:public CElementTraits<ContextedBitmapId>
{
public:
    static inline ULONG Hash(const ContextedBitmapId& key)
    {
        return (key.m_context_id<<32) | key.m_bitmap_id;
    }
};

typedef XyFlyWeight<ContextedBitmapId, 1024, ContextedBitmapIdTraits> XyFwContextedBitmapId;

//
// XySubRenderFrameWrapper2
//

XySubRenderFrameWrapper2::XySubRenderFrameWrapper2( IXySubRenderFrame *inner_obj, ULONGLONG context_id)
    : CUnknown(NAME("XySubRenderFrameWrapper"), NULL)
    , m_inner_obj(inner_obj)
{
    ASSERT(inner_obj);
    int count = 0;
    HRESULT hr = inner_obj->GetBitmapCount(&count);
    ASSERT(SUCCEEDED(hr) && count>=0);
    if (count>0)
    {
        m_ids.SetCount(count);
        for (int i=0;i<count;i++)
        {
            ULONGLONG id;
            hr = inner_obj->GetBitmap(i, &id, NULL, NULL, NULL, NULL);
            ASSERT(SUCCEEDED(hr));
            m_ids[i] = XyFwContextedBitmapId(DEBUG_NEW ContextedBitmapId(context_id, id)).GetId();
        }
    }
}

STDMETHODIMP XySubRenderFrameWrapper2::NonDelegatingQueryInterface( REFIID riid, void** ppv )
{
    return
        QI(IXySubRenderFrame)
        __super::NonDelegatingQueryInterface(riid, ppv);
}

STDMETHODIMP XySubRenderFrameWrapper2::GetOutputRect( RECT *outputRect )
{
    return m_inner_obj->GetOutputRect(outputRect);
}

STDMETHODIMP XySubRenderFrameWrapper2::GetClipRect( RECT *clipRect )
{
    return m_inner_obj->GetClipRect(clipRect);
}

STDMETHODIMP XySubRenderFrameWrapper2::GetXyColorSpace( int *xyColorSpace )
{
    return m_inner_obj->GetXyColorSpace(xyColorSpace);
}

STDMETHODIMP XySubRenderFrameWrapper2::GetBitmapCount( int *count )
{
    return m_inner_obj->GetBitmapCount(count);
}

STDMETHODIMP XySubRenderFrameWrapper2::GetBitmap( int index, ULONGLONG *id, POINT *position, SIZE *size
    , LPCVOID *pixels, int *pitch )
{
    HRESULT hr = m_inner_obj->GetBitmap(index, id, position, size, pixels, pitch);
    if (FAILED(hr))
    {
        return hr;
    }
    if (id)
    {
        ASSERT(index>=0 && index<m_ids.GetCount());
        *id = m_ids.GetAt(index);
        TRACE_SUB_RENDER_FRAME(index<<" contexted id:"<<*id);
    }
    return hr;
}

STDMETHODIMP XySubRenderFrameWrapper2::GetBitmapExtra( int index, LPVOID extra_info )
{
    return m_inner_obj->GetBitmapExtra(index, extra_info);
}

#ifdef SUBTITLE_FRAME_DUMP_FILE
#include <fstream>
#include <iomanip>

HRESULT DumpSubRenderFrame( IXySubRenderFrame *sub, const char * filename )
{
    using namespace std;

    ASSERT(sub);
    ofstream output(filename, ios::out);
    if (!output)
    {
        XY_LOG_ERROR("Failed to open file '"<<filename<<"'");
        return E_FAIL;
    }
    HRESULT hr = E_FAIL;
    RECT rect;
    hr = sub->GetOutputRect(&rect);
    if (FAILED(hr))
    {
        XY_LOG_ERROR("Failed to get bitmap count. "<<XY_LOG_VAR_2_STR(hr));
        return hr;
    }
    output<<"output rect:"<<rect<<endl;

    hr = sub->GetClipRect(&rect);
    if (FAILED(hr))
    {
        XY_LOG_ERROR("Failed to get bitmap count. "<<XY_LOG_VAR_2_STR(hr));
        return hr;
    }
    output<<"clip rect:"<<rect<<endl;

    int xyColorSpace = 0;
    hr = sub->GetXyColorSpace(&xyColorSpace);
    if (FAILED(hr))
    {
        XY_LOG_ERROR("Failed to get bitmap count. "<<XY_LOG_VAR_2_STR(hr));
        return hr;
    }
    output<<"xyColorSpace:"<<xyColorSpace<<endl;
    int count = 0;
    hr = sub->GetBitmapCount(&count);
    if (FAILED(hr)) {
        XY_LOG_ERROR("Failed to get bitmap count. "<<XY_LOG_VAR_2_STR(hr));
        return hr;
    }
    output<<"count:"<<count<<endl;

    if (xyColorSpace!=XY_CS_ARGB_F) {
        output<<"Colorspace NOT Supported! "<<endl;
        return S_FALSE;
    }
    for (int i=0;i<count;i++)
    {
        ULONGLONG id;
        POINT position;
        SIZE size;
        LPCVOID pixels;
        int pitch;
        hr = sub->GetBitmap(i, &id, &position, &size, &pixels, &pitch );
        if (FAILED(hr))
        {
            XY_LOG_ERROR("Failed to get bitmap count. "<<XY_LOG_VAR_2_STR(hr));
            return hr;
        }
        const DWORD* pixels2 = (const DWORD*)pixels;
        output<<"\tbitmap "<<i<<" id: "<<id<<" position: "<<position<<" size: "<<size<<" pitch: "<<pitch
            <<" pixels: "<<pixels2<<endl;
        output<<hex;
        for (int m=0;m<size.cy;m++)
        {
            output<<"\timg:";
            for (int n=0;n<size.cx;n++)
            {
                output<<" "<<setfill('0')<<setw(8)<<(int)pixels2[n];
            }
            output<<endl;
            pixels2 = (const DWORD*)((const BYTE*)pixels2+pitch);
        }
        output<<dec;
    }
    return S_OK;
}

#include <atlstr.h>
#include <atlimage.h>
DEFINE_GUID(ImageFormatBMP, 0xb96b3cab,0x0728,0x11d3,0x9d,0x7b,0x00,0x00,0xf8,0x1e,0xf3,0x2e);

DEFINE_GUID(ImageFormatPNG, 0xb96b3caf,0x0728,0x11d3,0x9d,0x7b,0x00,0x00,0xf8,0x1e,0xf3,0x2e);

#define ARGB_TO_COLORREEF(a,r,g,b) ( (a<<24) | (b<<16) | \
    (g<<8) | (r) )

HRESULT DumpToBitmap( IXySubRenderFrame *sub, LPCTSTR filename, bool dump_alpha_channel, bool dump_rgb_bmp
    , DWORD background_color)
{
    HRESULT hr = E_FAIL;
    CRect output_rect, clip_rect;
    hr = sub->GetOutputRect(&output_rect);
    if (FAILED(hr))
    {
        XY_LOG_ERROR("Failed to get bitmap count. "<<XY_LOG_VAR_2_STR(hr));
        return hr;
    }

    CImage imageBmp, image8bbp;
    bool succeeded = imageBmp.Create(output_rect.Width(),output_rect.Height(),32) &&
        image8bbp.Create(output_rect.Width(),output_rect.Height(),8);
    if (!succeeded) {
        XY_LOG_ERROR("Failed to create image.");
        hr = E_FAIL;
        return hr;
    }

    RGBQUAD bmiColors[256];
    for (int i = 0; i < 256; i++) {
        bmiColors[i].rgbRed   = i;
        bmiColors[i].rgbGreen = i;
        bmiColors[i].rgbBlue  = i;
    }
    image8bbp.SetColorTable(0, 256, bmiColors);

    DWORD ba = (background_color&0xff000000)>>24;
    DWORD br = (background_color&0x00ff0000)>>16;
    DWORD bg = (background_color&0x0000ff00)>>8;
    DWORD bb = (background_color&0x000000ff);

    hr = sub->GetClipRect(&clip_rect);
    if (FAILED(hr))
    {
        XY_LOG_ERROR("Failed to get bitmap count. "<<XY_LOG_VAR_2_STR(hr));
        return hr;
    }
    clip_rect &= output_rect;
    int xyColorSpace = 0;
    hr = sub->GetXyColorSpace(&xyColorSpace);
    if (FAILED(hr))
    {
        XY_LOG_ERROR("Failed to get bitmap count. "<<XY_LOG_VAR_2_STR(hr));
        return hr;
    }

    int count = 0;
    hr = sub->GetBitmapCount(&count);
    if (FAILED(hr)) {
        XY_LOG_ERROR("Failed to get bitmap count. "<<XY_LOG_VAR_2_STR(hr));
        return hr;
    }

    if (xyColorSpace!=XY_CS_ARGB_F) {
        XY_LOG_ERROR("Colorspace NOT Supported!");
        return S_FALSE;
    }
    for (int i=0;i<count;i++)
    {
        ULONGLONG id;
        POINT position;
        SIZE size;
        LPCVOID pixels;
        int pitch;
        hr = sub->GetBitmap(i, &id, &position, &size, &pixels, &pitch );
        if (FAILED(hr))
        {
            XY_LOG_ERROR("Failed to get bitmap count. "<<XY_LOG_VAR_2_STR(hr));
            return hr;
        }
        const DWORD* pixels2 = (const DWORD*)pixels;
        if (position.x < clip_rect.left)
        {
            pixels2 += clip_rect.left - position.x;
            size.cx -= clip_rect.left - position.x;
            position.x = clip_rect.left;
        }
        if (position.y < clip_rect.top)
        {
            pixels2 = (const DWORD*)((const BYTE*)pixels2+(clip_rect.top - position.y) * pitch);
            size.cy -= clip_rect.top - position.y;
            position.y = clip_rect.top;
        }
        if (position.x + size.cx > clip_rect.right)
        {
            size.cx -= position.x + size.cx - clip_rect.right;
        }
        if (position.y + size.cy > clip_rect.bottom)
        {
            size.cy -= position.y + size.cy - clip_rect.bottom;
        }

        for (int m=0;m<size.cy;m++)
        {
            BYTE * pix_8bpp = (BYTE *)image8bbp.GetPixelAddress(position.x-output_rect.left, position.y+m-output_rect.top);
            BYTE * pix_32bpp = (BYTE *)imageBmp.GetPixelAddress(position.x-output_rect.left, position.y+m-output_rect.top);
            for (int n=0;n<size.cx;n++)
            {
                DWORD argb = pixels2[n];
                DWORD a = (argb&0xff000000)>>24;
                DWORD r = (argb&0x00ff0000)>>16;
                DWORD g = (argb&0x0000ff00)>>8;
                DWORD b = (argb&0x000000ff);
                *pix_8bpp++ = a;
                *pix_32bpp++ = b + (((256-a)*bb)>>8);
                *pix_32bpp++ = g + (((256-a)*bg)>>8);
                *pix_32bpp++ = r + (((256-a)*br)>>8);
                *pix_32bpp++ = 0;
            }
            pixels2 = (const DWORD*)((const BYTE*)pixels2+pitch);
        }
    }

    CString filename_alpha(filename);
    filename_alpha += "_alpha.bmp";
    CString filename_rgb(filename);
    filename_rgb += "_rgb.bmp";

    succeeded = true;
    if (dump_alpha_channel)
    {
        succeeded = succeeded && SUCCEEDED( image8bbp.Save(filename_alpha, ImageFormatBMP) );
    }
    if (dump_rgb_bmp)
    {
        succeeded = succeeded && SUCCEEDED( imageBmp.Save(filename_rgb, ImageFormatBMP) );
    }

    return succeeded ? S_OK : E_FAIL;
}

#endif
