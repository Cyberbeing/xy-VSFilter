#pragma once

#include "MemSubPic.h"

class CPooledSubPic;

class CPooledSubPicAllocator : public CSubPicExAllocatorImpl
{
public:
    static const int MAX_FREE_ITEM = 2;
public:
    void OnItemDestruct(void* Item);

    bool STDMETHODCALLTYPE InitPool(int capacity);

    CPooledSubPicAllocator(int alpha_blt_dst_type, SIZE maxsize, int capacity, int type=-1 );
    virtual ~CPooledSubPicAllocator();
private:
    CCritSec _poolLock;

    int _type;
    int _alpha_blt_dst_type;
    CSize _maxsize;
    int _capacity;
    CAtlList<CPooledSubPic*> _free;
    CAtlList<CPooledSubPic*> _using;

    CPooledSubPic* DoAlloc();
    virtual bool AllocEx(bool fStatic, ISubPicEx** ppSubPic);

    void CollectUnUsedItem();
    STDMETHODIMP_(int) SetSpdColorType(int color_type);
    STDMETHODIMP_(bool) IsSpdColorTypeSupported(int type);
};

class CPooledSubPic : public CMemSubPic
{
    friend class CPooledSubPicAllocator;
public:
    CPooledSubPic(SubPicDesc& spd, int alpha_blt_dst_type, CPooledSubPicAllocator* pool):CMemSubPic(spd, alpha_blt_dst_type),_pool(pool){}
    virtual ~CPooledSubPic();
    
private:
    CCritSec _csLock;
    CPooledSubPicAllocator* _pool;
};
