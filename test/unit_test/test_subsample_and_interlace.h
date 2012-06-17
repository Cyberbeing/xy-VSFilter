#ifndef __TEST_SUBSAMPLE_AND_INTERLACE_34786202_B970_4091_AC4A_4F4137124732_H__
#define __TEST_SUBSAMPLE_AND_INTERLACE_34786202_B970_4091_AC4A_4F4137124732_H__

#include <gtest/gtest.h>
#include <wtypes.h>
#include <cmath>
#include <sstream>
#include <ostream>
#include <iomanip>
#include "xy_malloc.h"
#include "xy_intrinsics.h"
#include "subpic_alphablend_test_data.h"


class SubSampleAndInterlaceTest : public ::testing::Test 
{
protected:
    virtual void SetUp() 
    {  
    }

    const UV_TestData& GetZeroData(int w)
    {
        data1.w = w;
        data1.FillZero();
        return data1;
    }
    const UV_TestData& GetRandomData(int w)
    {
        data1.w = w;
        FillRandByte( data1.u, w );
        FillRandByte( data1.v, w );
        return data1;        
    }
    void FillRandByte(BYTE* buf, int size)
    {
        for (int i=0;i<size;i++)
        {
            buf[i] = rand() & 0xff;
        }
    }

    // virtual void TearDown() {}
    UV_TestData data1;
    //A_Y_UV_TestData data2[20];
    //A_Y_UV_TestData data3[20];
};


TEST_F(SubSampleAndInterlaceTest, Check_subsample_and_interlace_2_line)
{
    UV_TestData data0,data1,data2;

    for (int i=0;i<10000;i++)
    {        
        for (int w=16;w<33*16;w+=16)
        {            
            //zero
            data0 = GetZeroData(w*2);
            data1 = data0;
            data2 = data1;

            subsample_and_interlace_2_line_c( data1.u, data1.u, data1.v, w, w );
            subsample_and_interlace_2_line_sse2( data2.u, data2.u, data2.v, w, w );

            ASSERT_EQ(true, data1==data0)
                <<"data0"<<data0
                <<"data1"<<data1
                <<"data2"<<data2;
            ASSERT_EQ(true, data2==data0)
                <<"data0"<<data0
                <<"data1"<<data1
                <<"data2"<<data2;

            //random
            data0 = GetRandomData(w*2);
            data1 = data0;
            data2 = data1;

            subsample_and_interlace_2_line_c( data1.u, data1.u, data1.v, w, w );
            subsample_and_interlace_2_line_sse2( data2.u, data2.u, data2.v, w, w );

            ASSERT_EQ(true, data1==data2)
                <<"data0"<<data0
                <<"data1"<<data1
                <<"data2"<<data2;
        }
        //large input test
        int w = 960;
        data0 = GetRandomData(w*2);
        data1 = data0;
        data2 = data1;

        subsample_and_interlace_2_line_c( data1.u, data1.u, data1.v, w, w );
        subsample_and_interlace_2_line_sse2( data2.u, data2.u, data2.v, w, w );

        ASSERT_EQ(true, data1==data2)
            <<"data0"<<data0
            <<"data1"<<data1
            <<"data2"<<data2;
    }    
}

TEST_F(SubSampleAndInterlaceTest, Check_hleft_vmid_subsample_and_interlace_2_line)
{
    UV_TestData data0,data1,data2;

    for (int i=0;i<1000;i++)
    {
        for (int pitch=16;pitch<=160;pitch+=16)
        {
            int w = pitch;
            //for (int w=pitch-15;w<=pitch;w++)
            {
                //zero
                data0 = GetZeroData(pitch*2);
                data1 = data0;
                data2 = data1;

                hleft_vmid_subsample_and_interlace_2_line_c( data1.u, data1.u, data1.v, w, pitch );
                hleft_vmid_subsample_and_interlace_2_line_sse2( data2.u, data2.u, data2.v, w, pitch );

                ASSERT_EQ(true, data1==data0)
                    <<"pitch:"<<pitch<<" w:"<<w<<std::endl
                    <<"data0"<<data0
                    <<"data1"<<data1
                    <<"data2"<<data2;
                ASSERT_EQ(true, data2==data0)
                    <<"pitch:"<<pitch<<" w:"<<w<<std::endl
                    <<"data0"<<data0
                    <<"data1"<<data1
                    <<"data2"<<data2;

                //random
                data0 = GetRandomData(pitch*2);
                data1 = data0;
                data2 = data1;

                hleft_vmid_subsample_and_interlace_2_line_c( data1.u, data1.u, data1.v, w, pitch );
                hleft_vmid_subsample_and_interlace_2_line_sse2( data2.u, data2.u, data2.v, w, pitch );

                ASSERT_EQ(true, data1==data2)
                    <<"pitch:"<<pitch<<" w:"<<w<<std::endl
                    <<"data0"<<data0
                    <<"data1"<<data1
                    <<"data2"<<data2;
            }
        }
        
        //large input test
        int w = 960;
        data0 = GetRandomData(w*2);
        data1 = data0;
        data2 = data1;

        hleft_vmid_subsample_and_interlace_2_line_c( data1.u, data1.u, data1.v, w, w );
        hleft_vmid_subsample_and_interlace_2_line_sse2( data2.u, data2.u, data2.v, w, w );

        ASSERT_EQ(true, data1==data2)
            <<"data0"<<data0;
    }
}

#endif // __TEST_SUBSAMPLE_AND_INTERLACE_34786202_B970_4091_AC4A_4F4137124732_H__
