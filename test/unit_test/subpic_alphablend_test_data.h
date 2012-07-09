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
    int pitch;

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
        return ( pitch == rhs.pitch ) &&
            !memcmp(alpha, rhs.alpha, BUF_SIZE*6);
    }
    const AlphaSrcDstTestData& operator=(const AlphaSrcDstTestData& rhs)
    {
        pitch = rhs.pitch;
        memcpy(alpha, rhs.alpha, BUF_SIZE*6);        
        return *this;
    }

    friend std::ostream& operator<<(std::ostream& os, const AlphaSrcDstTestData& test_data);   
    std::string GetAlphaString() const
    {
        std::stringstream os;
        os<<"alpha:"<<std::endl;
        GetBufString(os, alpha);
        return os.str();
    }
    std::string GetSrcString() const 
    {
        std::stringstream os;
        os<<"Src:"<<std::endl;
        GetBufString(os, src);
        return os.str();
    }
    std::string GetDstString() const
    {
        std::stringstream os;
        os<<"Dst:"<<std::endl;
        GetBufString(os, dst);
        return os.str();
    }
    template<typename T>
    std::ostream& GetBufString(std::ostream& os, T* buf) const
    {        
        os<<std::hex;
        for (int i=0;i<pitch;i++)
        {
            os<<std::setw(4)<<static_cast<int>(buf[i])<<" ";
        }
        os<<std::endl;
        for (int i=0;i<pitch;i++)
        {
            os<<std::setw(4)<<static_cast<int>(buf[pitch+i])<<" ";
        }
        os<<std::endl;
        os<<std::dec;
        return os;
    }
};

std::ostream& operator<<(std::ostream& os, const AlphaSrcDstTestData& test_data)
{
    os<<"\tpitch:"<<test_data.pitch<<std::endl
      <<test_data.GetAlphaString()
      <<test_data.GetSrcString()
      <<test_data.GetDstString();    
    return os;
}

class AlphaBlendTest : public ::testing::Test 
{
protected:
    virtual void SetUp() 
    {  
    }

    const AlphaSrcDstTestData& GetZeroData(int pitch)
    {
        data1.pitch = pitch;
        data1.FillZero();
        return data1;
    }
    const AlphaSrcDstTestData& GetRandomData(int pitch)
    {
        data1.pitch = pitch;
        data1.FillRandomByte();
        return data1;        
    }

    //src <= 255-alpha
    const AlphaSrcDstTestData& GetRandomAuv12Data(int pitch)
    {
        data1.pitch = pitch;
        data1.FillRandomByte();

        for (int i=0;i<2*AlphaSrcDstTestData::BUF_SIZE;i++)
        {
            data1.src[i]=rand()%(256-data1.alpha[i]);
        }
        return data1;
    }

    //src <= 255-avg(alpha[0]+alpha[1]+alpha[0+pitch]+alpha[1+pitch])
    const AlphaSrcDstTestData& GetRandomAnvxxData(int pitch)
    {
        data1.pitch = pitch;
        data1.FillRandomByte();

        for (int i=0;i<AlphaSrcDstTestData::BUF_SIZE;i+=2)
        {
            unsigned int ia = ( 
                (data1.alpha[i]+data1.alpha[i+pitch]+1)/2+
                (data1.alpha[i+1]+data1.alpha[i+1+pitch]+1)/2+1)/2;
            data1.src[i]=rand()%(256-ia);
            data1.src[i+1]=rand()%(256-ia);
        }
        return data1;        
    }
    
    //src <= 255-avg(alpha[-1], alpha[0], alpha[1], alpha[-1+pitch], alpha[0+pitch], alpha[1+pitch])
    const AlphaSrcDstTestData& GetRandomAnvxxData2(int pitch)
    {
        data1.pitch = pitch;
        data1.FillRandomByte();

        int last_a = (data1.alpha[0]+data1.alpha[0+pitch]+1)/2;
        for (int i=0;i<AlphaSrcDstTestData::BUF_SIZE;i+=2)
        {
            unsigned int ia = ( last_a + (data1.alpha[i+1]+data1.alpha[i+1+pitch]+1)/2 + 1 )/2;
            ia = ( ia + (data1.alpha[i]+data1.alpha[i+pitch]+1)/2 + 1 )/2;
            last_a = (data1.alpha[i+1]+data1.alpha[i+1+pitch]+1)/2;
            data1.src[i]=rand()%(256-ia);
            data1.src[i+1]=rand()%(256-ia);
        }
        return data1;
    }
    
    // virtual void TearDown() {}
    AlphaSrcDstTestData data1;
};


struct UV_TestData
{
public:
    static const int BUF_SIZE = 2048;
    BYTE *u;
    BYTE *v;
    int w;

    UV_TestData()
    {
        memset(this, 0, sizeof(*this));
        u = reinterpret_cast<BYTE*>(xy_malloc(BUF_SIZE*5));            
        v = u + BUF_SIZE;
    }
    ~UV_TestData()
    {
        xy_free(u);
    }

    void FillZero()
    {
        memset(u, 0, 5*BUF_SIZE);
    }

    bool operator==(const UV_TestData& rhs)
    {
        return ( w == rhs.w ) &&
            !memcmp(u, rhs.u, w*sizeof(*u)) &&
            !memcmp(v, rhs.v, w*sizeof(*v));
    }
    const UV_TestData& operator=(const UV_TestData& rhs)
    {
        w = rhs.w;
        memcpy(u, rhs.u, w*sizeof(*u));
        memcpy(v, rhs.v, w*sizeof(*v));
        return *this;
    }

    friend std::ostream& operator<<(std::ostream& os, const UV_TestData& test_data);   
    std::string GetUString()
    {
        std::stringstream os;
        os<<std::endl<<"u:";
        GetBufString(os, u);
        return os.str();
    }
    std::string GetVString()
    {
        std::stringstream os;
        os<<std::endl<<"v:";
        GetBufString(os, v);
        return os.str();
    }

    template<typename T>
    std::stringstream& GetBufString(std::stringstream& os, T* buf)
    {        
        os<<std::hex;
        for (int i=0;i<w;i++)
        {
            os<<std::setw(4)<<static_cast<int>(buf[i])<<" ";
        }
        os<<std::endl;
        os<<std::dec;
        return os;
    }
};

static std::ostream& operator<<(std::ostream& os, const UV_TestData& test_data)
{
    os<<std::endl
        <<"\tw:"<<test_data.w<<std::endl;
    os<<std::hex;
    os<<"\tu:";
    for (int i=0;i<test_data.w;i++)
    {
        os<<std::setw(2)<<static_cast<int>(test_data.u[i])<<" ";
    }
    os<<std::endl;
    os<<"\tv:";
    for (int i=0;i<test_data.w;i++)
    {
        os<<std::setw(2)<<static_cast<int>(test_data.v[i])<<" ";
    }
    os<<std::endl;
    os<<std::dec;
    return os;
}

#endif // __TEST_SUBPIC_ALPHABLEND_BBE0029F_21BF_4317_8D71_7583FE1B6D5F_H__
