/************************************************************************/
/* author: xy                                                           */
/* date: 20110927                                                       */
/************************************************************************/
#include "stdafx.h"
#include "subpixel_position_controler.h"

SubpixelPositionControler SubpixelPositionControler::s_subpixel_position_controler;

SubpixelPositionControler::SUBPIXEL_LEVEL SubpixelPositionControler::SetSubpixelLevel( SUBPIXEL_LEVEL subpixel_level )
{
    if(subpixel_level!=_subpixel_level)
    {
        switch(subpixel_level)
        {
        case NONE:
            _subpixel_mask = NONE_MASK;
            break;
        case TWO_X_TWO:
            _subpixel_mask = TWO_X_TWO_MASK;
            break;
        case FOUR_X_FOUR:
            _subpixel_mask = FOUR_X_FOUR_MASK;
            break;
        case EIGHT_X_EIGHT:
            _subpixel_mask = EIGHT_X_EIGHT_MASK;
            break;
        case EIGHT_X_EIGHT_INTERPOLATE:
            _subpixel_mask = EIGHT_X_EIGHT_INTERPOLATE_MASK;
            break;
        }
        _subpixel_level = subpixel_level;
    }
    return subpixel_level;
}
