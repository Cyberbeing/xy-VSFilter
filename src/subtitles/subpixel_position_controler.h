/************************************************************************/
/* author: xy                                                           */
/* date: 20110926                                                       */
/************************************************************************/
#ifndef __SUBPIXEL_POSITION_CONTROLER_H_61444994_2880_4407_9308_4B17D0259B2C__
#define __SUBPIXEL_POSITION_CONTROLER_H_61444994_2880_4407_9308_4B17D0259B2C__

#include <atltypes.h>

class SubpixelPositionControler
{
public:
    enum SUBPIXEL_LEVEL
    {
        NONE = 0,
        TWO_X_TWO = 1,
        FOUR_X_FOUR = 2,
        EIGHT_X_EIGHT = 3,
        EIGHT_X_EIGHT_INTERPOLATE = 4,
        MAX_COUNT
    };
    enum SUBPIXEL_MASK
    {
        NONE_MASK = 0,
        TWO_X_TWO_MASK = 4,
        FOUR_X_FOUR_MASK = 6,
        EIGHT_X_EIGHT_MASK = 7,
        EIGHT_X_EIGHT_INTERPOLATE_MASK = 0,
    };
    SUBPIXEL_LEVEL SetSubpixelLevel(SUBPIXEL_LEVEL subpixel_level);
    inline SUBPIXEL_LEVEL GetSubpixelLevel()
    {
        return _subpixel_level;
    }
    inline CPoint GetSubpixel(const CPoint& p)
    {
        CPoint result(p.x & _subpixel_mask, p.y & _subpixel_mask);
        return result;
    }
    inline bool UseBilinearShift() { return _subpixel_level==EIGHT_X_EIGHT_INTERPOLATE; }

    static SubpixelPositionControler& GetGlobalControler()
    {
        return s_subpixel_position_controler;
    }

private:
    SubpixelPositionControler()
    {
        _subpixel_level = EIGHT_X_EIGHT;
        _subpixel_mask = EIGHT_X_EIGHT_MASK;    
    }
    SUBPIXEL_LEVEL _subpixel_level;
    SUBPIXEL_MASK _subpixel_mask;
    static SubpixelPositionControler s_subpixel_position_controler;
};

#endif // end of __SUBPIXEL_POSITION_CONTROLER_H_61444994_2880_4407_9308_4B17D0259B2C__
