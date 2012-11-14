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

	//while(capacity<_capacity)
    while(_capacity>0)
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

	while(_capacity<capacity)
	{
		if(!(temp = DoAlloc()))
		{
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

CPooledSubPicAllocator::CPooledSubPicAllocator( int alpha_blt_dst_type, SIZE maxsize, int capacity, int type/*=-1*/ )
	:CSubPicExAllocatorImpl(maxsize, false, false)
    , _alpha_blt_dst_type(alpha_blt_dst_type)
	, _maxsize(maxsize)
    , _type(type)
{
    if(_type==-1)
    {
        switch(alpha_blt_dst_type)
        {
        case MSP_YUY2:
            _type = MSP_XY_AUYV;
            break;
        case MSP_AYUV:
            _type = MSP_AYUV;
            break;
        case MSP_IYUV:
        case MSP_YV12:
        case MSP_P010:
        case MSP_P016:
        case MSP_NV12:
        case MSP_NV21:
            _type = MSP_AYUV_PLANAR;
            break;
        default:
            _type = MSP_RGBA;
            break;
        }
    }
	_capacity = 0;
	InitPool(capacity);
}


bool CPooledSubPicAllocator::AllocEx( bool fStatic, ISubPicEx** ppSubPic )
{
	if(!ppSubPic)
    {
		return(false);
    }
	{
		CAutoLock lock(&_poolLock);
        CollectUnUsedItem();
		if(!_free.IsEmpty())
		{
			CPooledSubPic *item = _free.RemoveHead();
            if(item->m_type!=_type)
            {
                item->_pool = NULL;
                item->Release();
                item = DoAlloc();
                item->AddRef();
            }
			_using.AddTail(item);
			*ppSubPic = item;
			item->AddRef();
		}
	}
	if(*ppSubPic!=NULL)
	{
		return true;
	}
	else
	{
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
        if(item!=NULL)
        {
		    item->_pool = NULL;
		    item->Release();
        }
	}
	for(POSITION pos = _using.GetHeadPosition(); pos!=NULL; )
	{
		item = _using.GetNext(pos);
        if(item!=NULL)
        {
		    item->_pool = NULL;
		    item->Release();
        }
	}
}

STDMETHODIMP_(int) CPooledSubPicAllocator::SetSpdColorType( int color_type )
{
    if(_type!=color_type)
    {
        m_pStatic = NULL;
        _type = color_type;
    }
    return _type;
}

STDMETHODIMP_(bool) CPooledSubPicAllocator::IsSpdColorTypeSupported( int type )
{
    if( (type==MSP_RGBA) 
        ||
        (type==MSP_XY_AUYV &&  _alpha_blt_dst_type == MSP_YUY2)
        ||
        (type==MSP_AYUV &&  _alpha_blt_dst_type == MSP_AYUV)
        ||
        (type==MSP_AYUV_PLANAR && (_alpha_blt_dst_type == MSP_IYUV ||
                            _alpha_blt_dst_type == MSP_YV12 ||
                            _alpha_blt_dst_type == MSP_P010 ||
                            _alpha_blt_dst_type == MSP_P016 ||
                            _alpha_blt_dst_type == MSP_NV12 ||
                            _alpha_blt_dst_type == MSP_NV21)) )
    {
        return true;
    }
    return false;
}

CPooledSubPic* CPooledSubPicAllocator::DoAlloc()
{
    SubPicDesc spd;
    spd.type = _type;
    spd.w = _maxsize.cx;
    spd.h = _maxsize.cy;
    //		spd.bpp = 32;
    spd.bpp = (_type == MSP_AYUV_PLANAR) ? 8 : 32;
    spd.pitch = (spd.w*spd.bpp)>>3;

    //		if(!(spd.bits = new BYTE[spd.pitch*spd.h]))
    if(_type == MSP_AYUV_PLANAR)
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
        return(NULL);
    }
    CPooledSubPic* temp = NULL;
    if(!(temp = DEBUG_NEW CPooledSubPic(spd, _alpha_blt_dst_type, this)))
    {
        xy_free(spd.bits);
        ASSERT(0);
        return(NULL);
    }
    return temp;
}

void CPooledSubPicAllocator::CollectUnUsedItem()
{
    POSITION pos = _using.GetHeadPosition();
    if(pos)
    {
        CPooledSubPic* item = _using.RemoveHead();
        if (item->m_cRef==1)
        {
            _free.AddTail(item);
        }
        else
        {
            _using.AddTail(item);
        }
    }
}

CPooledSubPic::~CPooledSubPic()
{
	if(_pool!=NULL)
		_pool->OnItemDestruct(this);
    xy_free(m_spd.bits);
    m_spd.bits = NULL;
}
