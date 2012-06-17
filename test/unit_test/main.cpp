#include <afx.h>
#include <gtest/gtest.h>
#define XY_UNIT_TEST
//#include "test_interlaced_uv_alphablend.h"
#include "test_subsample_and_interlace.h"
#include "test_alphablend.h"
#include "test_instrinsics_macro.h"


int main(int argc, char ** argv)
{
    testing::InitGoogleTest(&argc, argv);

    CRect r(1,1,2,2);
    CRect r2(0,0,1,1);
    r &= r2;

    std::cout<<"l "<<r.left<<" t "<<r.top<<" r "<<r.right<<" b "<<r.bottom<<" E "<<r.IsRectEmpty()<<std::endl;
    r.SetRect(2,1,2,2);
    std::cout<<"l "<<r.left<<" t "<<r.top<<" r "<<r.right<<" b "<<r.bottom<<" E "<<r.IsRectEmpty()
        <<" w "<<r.Width()
        <<std::endl;

    return RUN_ALL_TESTS();
}