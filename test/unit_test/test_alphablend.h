#ifndef __TEST_P010_ALPHABLEND_CBC6E4D7_E58B_4843_885D_80CEDB8C1709_H__
#define __TEST_P010_ALPHABLEND_CBC6E4D7_E58B_4843_885D_80CEDB8C1709_H__

#include "subpic_alphablend_test_data.h"
#include "xy_intrinsics.h"

TEST_F(AlphaBlendTest, CheckP010LumaSSE2)
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
            data0 = GetRandomAuv12Data(w);
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

TEST_F(AlphaBlendTest, CheckP010ChromaSSE2)
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
            data0 = GetRandomAuv12Data(w);
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

TEST_F(AlphaBlendTest, CheckNvxxChromaSSE2)
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
            data0 = GetRandomAnvxxData(w);
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

TEST_F(AlphaBlendTest, Check_hleft_vmid_mix_uv_yv12)
{
    AlphaSrcDstTestData data0,data1,data2;

    for (int i=0;i<1000;i++)
    {
        for (int pitch=16;pitch<160;pitch+=16)
        {
            for (int w=pitch-15;w<=pitch;w++)
            {
                //zero
                data0 = GetZeroData(pitch);
                data1 = data0;
                data2 = data1;

                hleft_vmid_mix_uv_yv12_c( data1.dst, w, data1.src, data1.alpha, pitch);
                hleft_vmid_mix_uv_yv12_sse2( data2.dst, w, data2.src, data2.alpha, pitch);

                ASSERT_EQ(true, data1==data0)
                    <<"pitch "<<pitch<<" w "<<w
                    <<"data0"<<data0
                    <<"data1"<<data1
                    <<"data2"<<data2;
                ASSERT_EQ(true, data2==data0)
                    <<"pitch "<<pitch<<" w "<<w
                    <<"data0"<<data0
                    <<"data1"<<data1
                    <<"data2"<<data2;

                //random
                data0 = GetRandomAuv12Data(pitch);
                data1 = data0;
                data2 = data1;

                hleft_vmid_mix_uv_yv12_c( data1.dst, w, data1.src, data1.alpha, pitch);
                hleft_vmid_mix_uv_yv12_sse2( data2.dst, w, data2.src, data2.alpha, pitch);

                ASSERT_EQ(true, data1==data2)
                    <<"pitch "<<pitch<<" w "<<w
                    <<"data0"<<data0
                    <<"data1"<<data1
                    <<"data2"<<data2;
            }
        }
    }    
}

TEST_F(AlphaBlendTest, Check_hleft_vmid_mix_uv_nvxx)
{
    AlphaSrcDstTestData data0,data1,data2;

    for (int i=0;i<1000;i++)
    {
        for (int pitch=16;pitch<160;pitch+=16)
        {
            for (int w=pitch-14;w<=pitch;w+=2)
            {
                //zero
                data0 = GetZeroData(pitch);
                data1 = data0;
                data2 = data1;

                hleft_vmid_mix_uv_nvxx_c( data1.dst, w, data1.src, data1.alpha, pitch);
                hleft_vmid_mix_uv_nvxx_sse2( data2.dst, w, data2.src, data2.alpha, pitch);

                ASSERT_EQ(true, data1==data0)
                    <<"pitch "<<pitch<<" w "<<w
                    <<"data0"<<data0
                    <<"data1"<<data1
                    <<"data2"<<data2;
                ASSERT_EQ(true, data2==data0)
                    <<"pitch "<<pitch<<" w "<<w
                    <<"data0"<<data0
                    <<"data1"<<data1
                    <<"data2"<<data2;

                //random
                data0 = GetRandomAnvxxData2(pitch);
                data1 = data0;
                data2 = data1;

                hleft_vmid_mix_uv_nvxx_c( data1.dst, w, data1.src, data1.alpha, pitch);
                hleft_vmid_mix_uv_nvxx_sse2( data2.dst, w, data2.src, data2.alpha, pitch);

                ASSERT_EQ(true, data1==data2)
                    <<"pitch "<<pitch<<" w "<<w
                    <<"data0"<<data0
                    <<"data1"<<data1
                    <<"data2"<<data2;
            }
        }
    }    
}

TEST_F(AlphaBlendTest, Check_hleft_vmid_mix_uv_p010)
{
    AlphaSrcDstTestData data0,data1,data2;

    for (int i=0;i<1000;i++)
    {
        for (int pitch=16;pitch<160;pitch+=16)
        {
            for (int w=pitch-14;w<=pitch;w+=2)
            {
                //zero
                data0 = GetZeroData(pitch);
                data1 = data0;
                data2 = data1;

                hleft_vmid_mix_uv_p010_c( data1.dst, w, data1.src, data1.alpha, pitch);
                hleft_vmid_mix_uv_p010_sse2( data2.dst, w, data2.src, data2.alpha, pitch);

                ASSERT_EQ(true, data1==data0)
                    <<"pitch "<<pitch<<" w "<<w
                    <<"data0"<<data0
                    <<"data1"<<data1
                    <<"data2"<<data2;
                ASSERT_EQ(true, data2==data0)
                    <<"pitch "<<pitch<<" w "<<w
                    <<"data0"<<data0
                    <<"data1"<<data1
                    <<"data2"<<data2;

                //random
                data0 = GetRandomAuv12Data(pitch);
                data1 = data0;
                data2 = data1;

                hleft_vmid_mix_uv_p010_c( data1.dst, w, data1.src, data1.alpha, pitch);
                hleft_vmid_mix_uv_p010_sse2( data2.dst, w, data2.src, data2.alpha, pitch);

                ASSERT_EQ(true, data1==data2)
                    <<"pitch "<<pitch<<" w "<<w
                    <<"data0"<<data0
                    <<"data1"<<data1
                    <<"data2"<<data2;
            }
        }
    }    
}

#endif // __TEST_P010_ALPHABLEND_CBC6E4D7_E58B_4843_885D_80CEDB8C1709_H__
