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
