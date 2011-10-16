#pragma once

#include "StdAfx.h"
#include "MemSubPic.h"
//#include <atlcoll.h>

using namespace std;

class CPooledSubPic;
interface IPooledAllocator
{
	STDMETHOD_(void, ReleaseItem)(void* Item) PURE;
	STDMETHOD_(void, OnItemDestruct)(void* Item) PURE;
};

class CPooledSubPicAllocator : public CSubPicExAllocatorImpl, public IPooledAllocator
{
public:
	STDMETHODIMP_(void) ReleaseItem(void* Item);
	STDMETHODIMP_(void) OnItemDestruct(void* Item);
	
	bool STDMETHODCALLTYPE InitPool(int capacity);

	CPooledSubPicAllocator(int type, SIZE maxsize, int capacity);
	virtual ~CPooledSubPicAllocator();
private:
	CCritSec _poolLock;
	
	int _type;
	CSize _maxsize;	
	int _capacity;
	CAtlList<CPooledSubPic*> _free;
	CAtlList<CPooledSubPic*> _using;
		
	virtual bool AllocEx(bool fStatic, ISubPicEx** ppSubPic);
};

class CPooledSubPic : public CMemSubPic
{
	friend class CPooledSubPicAllocator;
public:
	CPooledSubPic(SubPicDesc& spd, CPooledSubPicAllocator* pool):CMemSubPic(spd),_pool(pool){}
	virtual ~CPooledSubPic();

	STDMETHODIMP_(ULONG) Release(void);

private:
	CCritSec _csLock;
	IPooledAllocator* _pool;
};
