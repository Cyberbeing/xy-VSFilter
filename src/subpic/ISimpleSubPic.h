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
    STDMETHOD_(bool, LookupSubPic) (REFERENCE_TIME now /*[in]*/, ISimpleSubPic** output_subpic /*[out]*/) PURE;
};
