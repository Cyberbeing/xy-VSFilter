#pragma once

#include "ISubPic.h"
#include <atlbase.h>

//
// ISimpleSubPic
//

interface __declspec(uuid("8ca58386-cc13-439b-a226-eaaaffbcedcb"))
ISimpleSubPic :
public IUnknown {
	STDMETHOD (AlphaBlt) (SubPicDesc* target) PURE;
};


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
