#pragma once

enum XyColorSpace
{
    XY_CS_ARGB,
    XY_CS_AUYV,
    XY_CS_AYUV,
    XY_CS_AYUV_PLANAR
};

struct XyPlannerFormatExtra
{
    LPCVOID plans[4];
};

[uuid("4237bf3b-14fd-44a4-9704-86ec87f89897")]
interface IXySubRenderFrame : public IUnknown
{
    STDMETHOD(GetOutputRect)(RECT *outputRect) = 0;

    STDMETHOD(GetClipRect)(RECT *clipRect) = 0;

    STDMETHOD(GetXyColorSpace)(int *xyColorSpace) = 0;

    STDMETHOD(GetBitmapCount)(int *count) = 0;

    STDMETHOD(GetBitmap)(int index, ULONGLONG *id, POINT *position, SIZE *size, LPCVOID *pixels, int *pitch) = 0;

    //if xyColorSpace == XY_AYUV_PLANAR, extra_info should point to a XyPlannerFormatExtra struct
    STDMETHOD(GetBitmapExtra)(int index, LPVOID extra_info) = 0;
};
