#ifndef __TEST_SUBPIC_ALPHABLEND_BBE0029F_21BF_4317_8D71_7583FE1B6D5F_H__
#define __TEST_SUBPIC_ALPHABLEND_BBE0029F_21BF_4317_8D71_7583FE1B6D5F_H__


#include <gtest/gtest.h>
#include <wtypes.h>
#include <cmath>
#include <sstream>
#include <ostream>
#include <iomanip>
#include "xy_malloc.h"


struct AlphaSrcDstTestData
{
public:
    static const int BUF_SIZE = 2048;
    BYTE *alpha;
    BYTE *src;
    BYTE *dst;
    int w;

    AlphaSrcDstTestData()
    {
        memset(this, 0, sizeof(*this));
        alpha = reinterpret_cast<BYTE*>(xy_malloc(BUF_SIZE*6));            
        src = alpha + 2*BUF_SIZE;
        dst = alpha + 4*BUF_SIZE;
    }
    ~AlphaSrcDstTestData()
    {
        xy_free(alpha);
    }

    void FillZero()
    {
        memset(alpha, 0, 6*BUF_SIZE);
    }
    void FillRandomByte()
    {
        for (int i=0;i<6*BUF_SIZE;i++)
        {
            alpha[i] = rand() & 0xff;
        }
    }
    bool operator==(const AlphaSrcDstTestData& rhs)
    {
        return ( w == rhs.w ) &&
            !memcmp(alpha, rhs.alpha, BUF_SIZE*6);
    }
    const AlphaSrcDstTestData& operator=(const AlphaSrcDstTestData& rhs)
    {
        w = rhs.w;
        memcpy(alpha, rhs.alpha, BUF_SIZE*6);        
        return *this;
    }

    friend std::ostream& operator<<(std::ostream& os, const AlphaSrcDstTestData& test_data);   
    std::string GetAlphaString() const
    {
        std::stringstream os;
        os<<std::endl<<"alpha:";
        GetBufString(os, alpha);
        return os.str();
    }
    std::string GetSrcString() const 
    {
        std::stringstream os;
        os<<std::endl<<"Src:";
        GetBufString(os, src);
        return os.str();
    }
    std::string GetDstString() const
    {
        std::stringstream os;
        os<<std::endl<<"Dst:";
        GetBufString(os, dst);
        return os.str();
    }
    template<typename T>
    std::ostream& GetBufString(std::ostream& os, T* buf) const
    {        
        os<<std::hex;
        for (int i=0;i<w;i++)
        {
            os<<std::setw(4)<<static_cast<int>(buf[i])<<" ";
        }
        os<<std::endl;
        for (int i=0;i<w;i++)
        {
            os<<std::setw(4)<<static_cast<int>(buf[w+i])<<" ";
        }
        os<<std::endl;
        os<<std::dec;
        return os;
    }
};

std::ostream& operator<<(std::ostream& os, const AlphaSrcDstTestData& test_data)
{
    os<<std::endl
        <<"\tw:"<<test_data.w<<std::endl;
    test_data.GetBufString(os, test_data.alpha);
    test_data.GetBufString(os, test_data.src);
    test_data.GetBufString(os, test_data.dst);    
    return os;
}

class SubSampleAndInterlaceTest : public ::testing::Test 
{
protected:
    virtual void SetUp() 
    {  
    }

    const AlphaSrcDstTestData& GetZeroData(int w)
    {
        data1.w = w;
        data1.FillZero();
        return data1;
    }
    const AlphaSrcDstTestData& GetRandomData(int w)
    {
        data1.w = w;
        data1.FillRandomByte();
        return data1;        
    }

    //src <= 255-alpha
    const AlphaSrcDstTestData& GetRandomData2(int w)
    {
        data1.w = w;
        data1.FillRandomByte();

        for (int i=0;i<2*AlphaSrcDstTestData::BUF_SIZE;i++)
        {
            data1.src[i]=rand()%(256-data1.alpha[i]);
        }
        return data1;        
    }

    //src <= 255-avg(alpha[0]+alpha[1]+alpha[0+w]+alpha[1+w])
    const AlphaSrcDstTestData& GetRandomData3(int w)
    {
        data1.w = w;
        data1.FillRandomByte();

        for (int i=0;i<AlphaSrcDstTestData::BUF_SIZE;i+=2)
        {
            unsigned int ia = ( 
                (data1.alpha[i]+data1.alpha[i+w]+1)/2+
                (data1.alpha[i+1]+data1.alpha[i+1+w]+1)/2+1)/2;
            data1.src[i]=rand()%(256-ia);
            data1.src[i+1]=rand()%(256-ia);
        }
        return data1;        
    }
    // virtual void TearDown() {}
    AlphaSrcDstTestData data1;
};




#endif // __TEST_SUBPIC_ALPHABLEND_BBE0029F_21BF_4317_8D71_7583FE1B6D5F_H__
