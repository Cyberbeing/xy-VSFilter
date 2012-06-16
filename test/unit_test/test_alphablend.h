#ifndef __TEST_P010_ALPHABLEND_CBC6E4D7_E58B_4843_885D_80CEDB8C1709_H__
#define __TEST_P010_ALPHABLEND_CBC6E4D7_E58B_4843_885D_80CEDB8C1709_H__

#include "subpic_alphablend_test_data.h"

void mix_16_uv_p010_sse2(BYTE* dst, const BYTE* src, const BYTE* src_alpha, int pitch);
void mix_16_uv_p010_c(BYTE* dst, const BYTE* src, const BYTE* src_alpha, int pitch);
void mix_16_y_p010_sse2(BYTE* dst, const BYTE* src, const BYTE* src_alpha);
void mix_16_y_p010_c(BYTE* dst, const BYTE* src, const BYTE* src_alpha);

void mix_16_uv_nvxx_sse2(BYTE* dst, const BYTE* src, const BYTE* src_alpha, int pitch);
void mix_16_uv_nvxx_c(BYTE* dst, const BYTE* src, const BYTE* src_alpha, int pitch);

TEST_F(SubSampleAndInterlaceTest, CheckP010LumaSSE2)
{
    AlphaSrcDstTestData data0,data1,data2;

    for (int i=0;i<10000;i++)
    {        
        int w = 16;
        //for (int w=16;w<33*16;w+=16)
        {            
            //zero
            data0 = GetZeroData(w);
            data1 = data0;
            data2 = data1;

            mix_16_y_p010_c( data1.dst, data1.src, data1.alpha);
            mix_16_y_p010_sse2( data2.dst, data2.src, data2.alpha);

            ASSERT_EQ(true, data1==data0)
                <<"data0"<<data0
                <<"data1"<<data1
                <<"data2"<<data2;
            ASSERT_EQ(true, data2==data0)
                <<"data0"<<data0
                <<"data1"<<data1
                <<"data2"<<data2;

            //random
            data0 = GetRandomData2(w);
            data1 = data0;
            data2 = data1;

            mix_16_y_p010_c( data1.dst, data1.src, data1.alpha);
            mix_16_y_p010_sse2( data2.dst, data2.src, data2.alpha);

            ASSERT_EQ(true, data1==data2)
                <<"data0"<<data0
                <<"data1"<<data1
                <<"data2"<<data2;
        }

        ASSERT_EQ(true, data1==data2)
            <<"data0"<<data0
            <<"data1"<<data1
            <<"data2"<<data2;
    }    
}

TEST_F(SubSampleAndInterlaceTest, CheckP010ChromaSSE2)
{
    AlphaSrcDstTestData data0,data1,data2;

    for (int i=0;i<10000;i++)
    {        
        int w = 16;
        //for (int w=16;w<33*16;w+=16)
        {            
            //zero
            data0 = GetZeroData(w);
            data1 = data0;
            data2 = data1;

            mix_16_uv_p010_c( data1.dst, data1.src, data1.alpha, w);
            mix_16_uv_p010_sse2( data2.dst, data2.src, data2.alpha, w);

            ASSERT_EQ(true, data1==data0)
                <<"data0"<<data0
                <<"data1"<<data1
                <<"data2"<<data2;
            ASSERT_EQ(true, data2==data0)
                <<"data0"<<data0
                <<"data1"<<data1
                <<"data2"<<data2;

            //random
            data0 = GetRandomData2(w);
            data1 = data0;
            data2 = data1;

            mix_16_uv_p010_c( data1.dst, data1.src, data1.alpha, w);
            mix_16_uv_p010_sse2( data2.dst, data2.src, data2.alpha, w);

            ASSERT_EQ(true, data1==data2)
                <<"data0"<<data0
                <<"data1"<<data1
                <<"data2"<<data2;
        }

        ASSERT_EQ(true, data1==data2)
            <<"data0"<<data0
            <<"data1"<<data1
            <<"data2"<<data2;
    }    
}

TEST_F(SubSampleAndInterlaceTest, CheckNvxxChromaSSE2)
{
    AlphaSrcDstTestData data0,data1,data2;

    for (int i=0;i<10000;i++)
    {        
        int w = 16;
        //for (int w=16;w<33*16;w+=16)
        {            
            //zero
            data0 = GetZeroData(w);
            data1 = data0;
            data2 = data1;

            mix_16_uv_nvxx_c( data1.dst, data1.src, data1.alpha, w);
            mix_16_uv_nvxx_sse2( data2.dst, data2.src, data2.alpha, w);

            ASSERT_EQ(true, data1==data0)
                <<"data0"<<data0
                <<"data1"<<data1
                <<"data2"<<data2;
            ASSERT_EQ(true, data2==data0)
                <<"data0"<<data0
                <<"data1"<<data1
                <<"data2"<<data2;

            //random
            data0 = GetRandomData3(w);
            data1 = data0;
            data2 = data1;

            mix_16_uv_nvxx_c( data1.dst, data1.src, data1.alpha, w);
            mix_16_uv_nvxx_sse2( data2.dst, data2.src, data2.alpha, w);

            ASSERT_EQ(true, data1==data2)
                <<"data0"<<data0
                <<"data1"<<data1
                <<"data2"<<data2;
        }

        ASSERT_EQ(true, data1==data2)
            <<"data0"<<data0
            <<"data1"<<data1
            <<"data2"<<data2;
    }    
}
#endif // __TEST_P010_ALPHABLEND_CBC6E4D7_E58B_4843_885D_80CEDB8C1709_H__
