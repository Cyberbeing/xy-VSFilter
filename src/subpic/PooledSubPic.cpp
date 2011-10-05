#include "PooledSubPic.h"
#include "../subtitles/xy_malloc.h"


STDMETHODIMP_(void) CPooledSubPicAllocator::ReleaseItem( void* Item )
{
	//DbgLog((LOG_TRACE, 3, "ReleaseItem::free:%d", _free.GetCount()));
	CAutoLock lock(&_poolLock);
	POSITION pos = _using.Find((CPooledSubPic*)Item);
	if(pos!=NULL)
	{
		_using.RemoveAt(pos);
		_free.AddTail((CPooledSubPic*)Item);
	}
}

STDMETHODIMP_(void) CPooledSubPicAllocator::OnItemDestruct( void* Item )
{
	//DbgLog((LOG_TRACE, 3, "OnItemDestruct::free:%d", _free.GetCount()));
	CAutoLock lock(&_poolLock);
	//CPooledSubPic* typedItem = (CPooledSubPic*)Item;
	POSITION pos = _using.Find((CPooledSubPic*)Item);
	if(pos!=NULL)
	{
		_using.RemoveAt(pos);
		_capacity--;
	}
	ASSERT(_free.Find((CPooledSubPic*)Item)==NULL);
}

bool STDMETHODCALLTYPE CPooledSubPicAllocator::InitPool( int capacity )
{
	DbgLog((LOG_TRACE, 3, "%s(%d), %s", __FILE__, __LINE__, __FUNCTION__));
	CAutoLock lock(&_poolLock);
	CPooledSubPic* temp;

	if(capacity<0)
		capacity = 0;

	while(capacity<_capacity)
	{
		if(!_free.IsEmpty())
		{
			temp = _free.RemoveTail();
			_capacity--;
			temp->_pool = NULL;
			temp->Release();
		}
		else if(!_using.IsEmpty())
		{
			temp = _using.RemoveTail();
			_capacity--;
			temp->_pool = NULL;
			temp->Release();
		}
	}

	SubPicDesc spd;
	while(_capacity<capacity)
	{
		spd.type = _type;
		spd.w = _maxsize.cx;
		spd.h = _maxsize.cy;
//		spd.bpp = 32;
    	spd.bpp = (spd.type == MSP_YV12 || spd.type == MSP_IYUV) ? 8 : 32;
		spd.pitch = (spd.w*spd.bpp)>>3;

//		if(!(spd.bits = new BYTE[spd.pitch*spd.h]))
        if(spd.type == MSP_YV12 || spd.type == MSP_IYUV)
        {
            spd.bits = xy_malloc(spd.pitch*spd.h*4);
        }
        else
        {
            spd.bits = xy_malloc(spd.pitch*spd.h);
        }
		if(!spd.bits)
		{
			ASSERT(0);
			return(false);
		}

		if(!(temp = new CPooledSubPic(spd, this)))
		{
			delete[] spd.bits;
			ASSERT(0);
			return(false);
		}

		_free.AddTail(temp);
		_capacity++;
		temp->AddRef();
	}
	//DbgLog((LOG_TRACE, 3, "InitPool::free:%d", _free.GetCount()));
	return true;
}

CPooledSubPicAllocator::CPooledSubPicAllocator( int type, SIZE maxsize, int capacity )
	:CSubPicAllocatorImpl(maxsize, false, false)
	, _type(type)
	, _maxsize(maxsize)
{
	_capacity = 0;
	InitPool(capacity);
}


bool CPooledSubPicAllocator::Alloc( bool fStatic, ISubPic** ppSubPic )
{
	if(!ppSubPic)
		return(false);
	{
		CAutoLock lock(&_poolLock);
		if(!_free.IsEmpty())
		{
			CPooledSubPic *item = _free.RemoveHead();
			_using.AddTail(item);
			*ppSubPic = item;
			item->AddRef();
		}
	}
	if(*ppSubPic!=NULL)
	{
		//SubPicDesc* pSpd = (*ppSubPic)->GetObject();
		//pSpd->w = _maxsize.cx;
		//pSpd->h = _maxsize.cy;
		//pSpd->bpp = 32;
		//pSpd->pitch = ((spd->w) * (pSpd->bpp))>>3;
		//pSpd->type = _type;

		//DbgLog((LOG_TRACE, 3, "Alloc succeed: free:%d", _free.GetCount()));
		return true;
	}
	else
	{
		//DbgLog((LOG_TRACE, 3, "Alloc failed: free:%d", _free.GetCount()));
		return false;
	}
}

CPooledSubPicAllocator::~CPooledSubPicAllocator()
{
	CPooledSubPic* item = NULL;
	CAutoLock lock(&_poolLock);
	for(POSITION pos = _free.GetHeadPosition(); pos!=NULL; )
	{
		item = _free.GetNext(pos);
		item->_pool = NULL;
		item->Release();
	}
	for(POSITION pos = _using.GetHeadPosition(); pos!=NULL; )
	{
		item = _using.GetNext(pos);
		item->_pool = NULL;
		item->Release();
	}
}

STDMETHODIMP_(ULONG) CPooledSubPic::Release( void )
{
	ULONG count = 0;
	{
		CAutoLock lock(&_csLock);
		count = __super::Release();
		//DbgLog((LOG_TRACE, 3, "SubPicRelease:%x:%d", this, count));
	}
	if(count==1 && _pool!=NULL)
		_pool->ReleaseItem(this);
	return count;
}

CPooledSubPic::~CPooledSubPic()
{
	if(_pool!=NULL)
		_pool->OnItemDestruct(this);
    xy_free(m_spd.bits);
    m_spd.bits = NULL;
}
