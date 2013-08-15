#ifndef __XY_INT_MAP_B96846C2_2500_4AD9_8779_C3469DB75F33_H__
#define __XY_INT_MAP_B96846C2_2500_4AD9_8779_C3469DB75F33_H__

#include <stdint.h>
#include <string.h>

template<typename T>
class XyIntMap
{
public:
    static const int IND0_SIZE = 2048;
    static const int IND1_SIZE = 1024;
    static const int IND2_SIZE = 2048;
public:
    XyIntMap() { m_indx=NULL; m_cur_level=0; }
    ~XyIntMap(){ ReleaseMemory(); }

    void ReleaseMemory(uint32_t preserve_key_number=0)
    {
        if (m_indx)
        {
            int preserve_key_number2 = preserve_key_number;
            void *data = m_indx;
            switch(m_cur_level)
            {
            case 2:
                ReleaseLevel2Data(reinterpret_cast<T***>(m_indx), preserve_key_number);
                if (preserve_key_number==0)
                {
                    m_indx = NULL;
                    m_cur_level = 0;
                }
                break;
            case 1:
                ReleaseLevel1Data(reinterpret_cast<T** >(m_indx), preserve_key_number);
                if (preserve_key_number==0)
                {
                    m_indx = NULL;
                    m_cur_level = 0;
                }
                break;
            case 0:
                ReleaseLevel0Data(reinterpret_cast<T*  >(m_indx), preserve_key_number);
                if (preserve_key_number==0)
                {
                    m_indx = NULL;
                    m_cur_level = 0;
                }
                break;
            }
            switch(m_cur_level)
            {
            case 2:
                if (preserve_key_number<=IND0_SIZE*IND1_SIZE)
                {
                    void * tmp = reinterpret_cast<void**>(m_indx)[0];
                    delete[] reinterpret_cast<T***>(m_indx);
                    m_indx = tmp;
                    m_cur_level--;
                }
            case 1:
                if (preserve_key_number<=IND0_SIZE)
                {
                    void * tmp = reinterpret_cast<void**>(m_indx)[0];
                    delete[] reinterpret_cast<T**>(m_indx);
                    m_indx = tmp;
                    m_cur_level--;
                }
                ASSERT(preserve_key_number>0);
            }
        }
    }

    T GetAt(uint32_t i) const
    {
        void * data = m_indx;
        switch(m_cur_level)
        {
        case 2:
            if (data)
            {
                data = reinterpret_cast<T***>(data)[i/(IND0_SIZE*IND1_SIZE)];
                i %= IND0_SIZE*IND1_SIZE;
            }
        case 1:
            if (data && i<IND0_SIZE*IND1_SIZE)
            {
                data = reinterpret_cast<T**>(data)[i/IND0_SIZE];
                i %= IND0_SIZE;
            }
        case 0:
            if (data && i<IND0_SIZE)
            {
                return reinterpret_cast<T*>(data)[i];
            }
            break;
        }
        return 0;
    }

    T SetAt(uint32_t i, const T& t)
    {
        switch(m_cur_level)
        {
        case 0:
            if (i>=IND0_SIZE)
            {
                T** ind1 = new T*[IND1_SIZE];
                if (!ind1)
                {
                    return 0;
                }
                memset(ind1, 0, sizeof(T*)*IND1_SIZE);
                ind1[0] = reinterpret_cast<T*>(m_indx);
                m_indx = ind1;
                m_cur_level++;
            }
        case 1:
            if (i>=IND0_SIZE*IND1_SIZE)
            {
                T*** ind2 = new T**[IND2_SIZE];
                if (!ind2)
                {
                    return 0;
                }
                memset(ind2, 0, sizeof(T**)*IND2_SIZE);
                ind2[0] = reinterpret_cast<T**>(m_indx);
                m_indx = ind2;
                m_cur_level++;
            }
        }

        T**** ind2 = reinterpret_cast<T****>(&m_indx);
        T***  ind1 = reinterpret_cast<T*** >(&m_indx);
        T**   ind0 = reinterpret_cast<T**  >(&m_indx);
        switch(m_cur_level)
        {
        case 2:
            ind1 = &((*ind2)[i/(IND0_SIZE*IND1_SIZE)]);
            if (!*ind1)
            {
                *ind1 = new T*[IND1_SIZE];
                if (!*ind1)
                {
                    return 0;
                }
                memset(*ind1, 0, sizeof(T*)*IND1_SIZE);
            }
            i %= IND0_SIZE*IND1_SIZE;
        case 1:
            ind0 = &((*ind1)[i/IND0_SIZE]);
            if (!*ind0)
            {
                *ind0 = new T[IND0_SIZE];
                if (!*ind0)
                {
                    return 0;
                }
                memset(*ind0, 0, sizeof(T)*IND0_SIZE);
            }
            i %= IND0_SIZE;
        case 0:
            if (!*ind0)
            {
                *ind0 = new T[IND0_SIZE];
                if (!*ind0)
                {
                    return 0;
                }
                memset(*ind0, 0, sizeof(T)*IND0_SIZE);
            }
            return (*ind0)[i] = t;
            break;
        }
        return 0;
    }
private:
    void ReleaseLevel2Data(T*** p, uint32_t preserve)
    {
        int i = preserve/(IND0_SIZE*IND1_SIZE) + 1;
        for (;i<IND2_SIZE;i++)
        {
            ReleaseLevel1Data(p[i], 0);
            p[i] = NULL;
        }
        ReleaseLevel1Data(p[preserve/(IND0_SIZE*IND1_SIZE)], preserve%(IND0_SIZE*IND1_SIZE));
        if (!(preserve/(IND0_SIZE*IND1_SIZE)))
        {
            p[preserve/(IND0_SIZE*IND1_SIZE)] = NULL;
        }
        if (preserve==0)
        {
            delete[] p;
            return;
        }
    }
    void ReleaseLevel1Data(T** p, uint32_t preserve)
    {
        if (preserve>=IND0_SIZE*IND1_SIZE)
        {
            return;
        }
        int i = preserve/IND0_SIZE + 1;
        for (;i<IND1_SIZE;i++)
        {
            delete[] p[i];
            p[i] = NULL;
        }
        ReleaseLevel0Data(p[preserve/IND0_SIZE], preserve%IND0_SIZE);
        if (!(preserve/IND0_SIZE))
        {
            p[preserve/IND0_SIZE] = NULL;
        }
        if (preserve==0)
        {
            delete[] p;
        }
    }
    void ReleaseLevel0Data(T * p, uint32_t preserve)
    {
        if (preserve>=IND0_SIZE)
        {
            return;
        }
        if (!preserve)
        {
            delete[] p;
        }
        else
        {
            for (int i=preserve;i<IND0_SIZE;i++)
            {
                p[i] = 0;
            }
        }
    }
private:
    int   m_cur_level;
    void* m_indx;
};


#endif // __XY_INT_MAP_B96846C2_2500_4AD9_8779_C3469DB75F33_H__
