#ifndef _PXRESULT_H
#define _PXRESULT_H

#define PNR_UNKNOWN_IMAGE      MAKE_PN_RESULT(1,SS_RPX,0)
#define PNR_UNKNOWN_EFFECT     MAKE_PN_RESULT(1,SS_RPX,1)
#define PNR_SENDIMAGE_ABORTED  MAKE_PN_RESULT(0,SS_RPX,2)
#define PNR_SENDEFFECT_ABORTED MAKE_PN_RESULT(0,SS_RPX,3)

#endif