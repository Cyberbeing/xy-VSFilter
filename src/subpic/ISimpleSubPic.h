#pragma once

#include "ISubPic.h"
#include "XySubRenderIntf.h"

//
// ISimpleSubPic
//

interface __declspec(uuid("8ca58386-cc13-439b-a226-eaaaffbcedcb"))
ISimpleSubPic :
public IUnknown {
	STDMETHOD (AlphaBlt) (SubPicDesc* target) PURE;
};

//
// ISimpleSubPicProvider 
// 

interface __declspec(uuid("b3a13c82-efcf-4433-95a4-0c750cc638f6"))
ISimpleSubPicProvider :
public IUnknown
{
public:
    STDMETHOD (SetSubPicProvider) (IUnknown* subpic_provider /*[in]*/) PURE;
    STDMETHOD (GetSubPicProvider) (IUnknown** subpic_provider /*[out]*/) PURE;

    STDMETHOD (SetFPS) (double fps /*[in]*/) PURE;
    STDMETHOD (SetTime) (REFERENCE_TIME now /*[in]*/) PURE;

    STDMETHOD (Invalidate) (REFERENCE_TIME invalidate_rt = -1) PURE;
    STDMETHOD_(bool, LookupSubPic) (REFERENCE_TIME now /*[in]*/, ISimpleSubPic** output_subpic /*[out]*/) PURE;

    //fix me: & => *
    STDMETHOD (GetStats) (int& nSubPics, REFERENCE_TIME& rtNow, REFERENCE_TIME& rtStart, REFERENCE_TIME& rtStop /*[out]*/) PURE;
    STDMETHOD (GetStats) (int nSubPic /*[in]*/, REFERENCE_TIME& rtStart, REFERENCE_TIME& rtStop /*[out]*/) PURE;
};

interface __declspec(uuid("1f277b1b-c28d-4022-b00e-373d0b1b54cd"))
ISubPicProviderEx2 ://A tmp thing for test
public IUnknown {
    STDMETHOD (Lock) () PURE;
    STDMETHOD (Unlock) () PURE;

    STDMETHOD_(POSITION, GetStartPosition) (REFERENCE_TIME rt, double fps) PURE;
    STDMETHOD_(POSITION, GetNext) (POSITION pos) PURE;

    STDMETHOD_(REFERENCE_TIME, GetStart) (POSITION pos, double fps) PURE;
    STDMETHOD_(REFERENCE_TIME, GetStop) (POSITION pos, double fps) PURE;

    STDMETHOD_(bool, IsAnimated) (POSITION pos) PURE;

    STDMETHOD_(VOID, GetStartStop) (POSITION pos, double fps, /*out*/REFERENCE_TIME& start, /*out*/REFERENCE_TIME& stop) PURE;
    STDMETHOD (RenderEx) (IXySubRenderFrame**subRenderFrame, int spd_type, 
        const RECT& video_rect, const RECT& subtitle_target_rect,
        const SIZE& original_video_size, 
        REFERENCE_TIME rt, double fps) PURE;

    STDMETHOD_(bool, IsColorTypeSupported) (int type) PURE;

    STDMETHOD_(bool, IsMovable) () PURE;
};

