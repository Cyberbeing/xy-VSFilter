#pragma once

#include "MemSubPic.h"

using namespace std;

class CPooledSubPic;
interface IPooledAllocator
{
    STDMETHOD_(void, ReleaseItem)(void* Item) PURE;
    STDMETHOD_(void, OnItemDestruct)(void* Item) PURE;
};

class CPooledSubPicAllocator : public CSubPicExAllocatorImpl//, public IPooledAllocator
{
public:
    STDMETHODIMP_(void) ReleaseItem(void* Item);
    STDMETHODIMP_(void) OnItemDestruct(void* Item);

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
