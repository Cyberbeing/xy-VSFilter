#ifndef __XY_POOL_C15B64F5_1F0D_490E_99EC_57A44C7FBAE2_H__
#define __XY_POOL_C15B64F5_1F0D_490E_99EC_57A44C7FBAE2_H__

class XyPool
{
public:
    XyPool();
    ~XyPool();

    int Init(unsigned int obj_size, unsigned int align, unsigned int grow_by);
    void DeInit();

    void* Alloc();
    void  Free(void*);
protected:
    typedef void** PPaddedObj;
    struct BlockNode
    {
        int used_number;
        char * block_base;
        char * block;
        PPaddedObj free_head;
        BlockNode * prev;
        BlockNode * next;
    };
protected:
    void Grow();
    void Shrink();
    void CalculeSize();
protected:
    BlockNode    m_blocks_not_full;
    BlockNode    m_blocks_full;
    unsigned int m_obj_size;
    unsigned int m_align;
    unsigned int m_grow_by;

    unsigned int m_padded_size;
    unsigned int m_padded_shift;
    unsigned int m_obj_offset;

    unsigned int m_free_block_number;
    unsigned int m_total_block_number;
};


template<typename T>
class XyObjPool : protected XyPool
{
public:
    XyObjPool() {}
    ~XyObjPool() { DeInit(); }

    int Init(unsigned int grow_by);
    void DeInit();

    T* Alloc();
    void Free(T*);
};

template<typename T>
int XyObjPool<T>::Init( unsigned int grow_by )
{
    struct TestAlign
    {
        char c;
        T a;
    };
    const int alignment = (int)(&((TestAlign*)0)->a);
    DeInit();
    return __super::Init(sizeof(T), alignment, grow_by);
}

template<typename T>
void XyObjPool<T>::DeInit()
{
    if (m_free_block_number!=m_total_block_number)
    {
        BlockNode * blocks[4] = {m_blocks_not_full.next, &m_blocks_not_full, m_blocks_full.next, &m_blocks_full};

        for (int i=0;i<4;i+=2)
        {
            BlockNode * iter1=blocks[i];
            while (iter1 != blocks[i+1])
            {
                char *block = iter1->block;
                block += m_padded_shift;
                for (unsigned i=0;i<m_grow_by;i++, block+=m_padded_size)
                {
                    if ( (BlockNode*)*PPaddedObj(block)==iter1 )
                    {
                        ((T*)block)->~T();
                    }
                }
                iter1=iter1->next;
            }
        }
    }
    __super::DeInit();
}

template<typename T>
T* XyObjPool<T>::Alloc()
{
    T* ret = new(__super::Alloc())T;
    return ret;
}

template<typename T>
void XyObjPool<T>::Free( T* obj )
{
    if (obj)
    {
        obj->~T();
        __super::Free(obj);
    }
}

#endif // __XY_POOL_C15B64F5_1F0D_490E_99EC_57A44C7FBAE2_H__
