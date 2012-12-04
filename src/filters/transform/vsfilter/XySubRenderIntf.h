#pragma once

#include "SubRenderIntf.h"
#include "XyOptions.h"

enum XyColorSpace
{
    XY_CS_ARGB,   //pre-multiplied alpha. Use A*g + RGB to mix
    XY_CS_ARGB_F, //pre-multiplied alpha. Use (0xff-A)*g + RGB to mix
    XY_CS_AUYV,
    XY_CS_AYUV,
    XY_CS_AYUV_PLANAR
};

struct XyPlannerFormatExtra
{
    LPCVOID plans[4];
};

[uuid("4237bf3b-14fd-44a4-9704-86ec87f89897")]
interface IXySubRenderFrame : public ISubRenderFrame
{
    STDMETHOD(GetXyColorSpace)(int *xyColorSpace) = 0;

    //if xyColorSpace == XY_AYUV_PLANAR, extra_info should point to a XyPlannerFormatExtra struct
    STDMETHOD(GetBitmapExtra)(int index, LPVOID extra_info) = 0;
};

[uuid("68f052bf-dcf2-476e-baab-494b6288c58e")]
interface IXySubRenderProvider: public IUnknown
{
    STDMETHOD(Connect)(IXyOptions *consumer) = 0;
    STDMETHOD(RequestFrame)(IXySubRenderFrame**subRenderFrame, REFERENCE_TIME now) = 0;
    STDMETHOD (Invalidate) (REFERENCE_TIME rtInvalidate = -1) = 0;
};
