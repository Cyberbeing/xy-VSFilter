/************************************************************************/
/* author: xy                                                           */
/* date: 20110918                                                       */
/************************************************************************/
#ifndef __MRU_CACHE_H_256FCF72_8663_41DC_B98A_B822F6007912__
#define __MRU_CACHE_H_256FCF72_8663_41DC_B98A_B822F6007912__

#include <atlcoll.h>
#include <utility>

template<
    typename K,
    typename V,
    class KTraits = CElementTraits< K >
>
class XyMru
{
public:
    XyMru(std::size_t max_item_num):_max_item_num(max_item_num){}

    inline POSITION UpdateCache(POSITION pos)
    {
        _list.MoveToHead(pos);
        return pos;
    }
    inline POSITION UpdateCache(const K& key, const V& value)
    {
        POSITION pos;
        if( !_hash.Lookup(key,pos) )
        {
            pos = _list.AddHead( ListItem(key, value) );
            _hash[key] = pos;
        }
        else
        {
            _list.MoveToHead(pos);
        }
        if(_list.GetCount()>_max_item_num)
        {
            _hash.RemoveKey(_list.GetTail().first);
            _list.RemoveTail();
        }
        return pos;
    }
    inline void RemoveAll() 
    { 
        _hash.RemoveAll();
        _list.RemoveAll();
    }
    
    inline POSITION Lookup(const K& key) const
    {
        POSITION pos;
        if( _hash.Lookup(key,pos) )
        {
            return pos;
        }
        else
        {
            return NULL;
        }
    }
    inline const V& GetAt(POSITION pos) const
    {
        return _list.GetAt(pos).second;
    }
    inline std::size_t SetMaxItemNum( std::size_t max_item_num )
    {
        _max_item_num = max_item_num;
        while(_list.GetCount()>_max_item_num)
        {
            _hash.RemoveKey(_list.GetTail().first);
            _list.RemoveTail();
        }
        return _max_item_num;
    }
    inline std::size_t GetMaxItemNum() const { return _max_item_num; }
    inline std::size_t GetCurItemNum() const { return _list.GetCount(); }
protected:
    typedef std::pair<K,V> ListItem;
    CAtlList<ListItem> _list;
    CAtlMap<K,POSITION,KTraits> _hash;

    std::size_t _max_item_num;
};

template<
    typename K,
    typename V,
class KTraits = CElementTraits< K >
>
class EnhancedXyMru:public XyMru<K,V,KTraits>
{
public:
    EnhancedXyMru(std::size_t max_item_num):XyMru(max_item_num),_cache_hit(0),_query_count(0){}

    std::size_t SetMaxItemNum( std::size_t max_item_num, bool clear_statistic_info=false )
    {
        if(clear_statistic_info)
        {
            _cache_hit = 0;
            _query_count = 0;
        }
        return __super::SetMaxItemNum(max_item_num);
    }
    void RemoveAll(bool clear_statistic_info=false) 
    { 
        if(clear_statistic_info) 
        { 
            _cache_hit=0; 
            _query_count=0; 
        } 
        __super::RemoveAll();         
    }

    inline POSITION Lookup(const K& key)
    {
        _query_count++;
        POSITION pos = __super::Lookup(key);
        _cache_hit += (pos!=NULL);
        return pos;
    }

    inline std::size_t GetCacheHitCount() const { return _cache_hit; }
    inline std::size_t GetQueryCount() const { return _query_count; }
protected:
    std::size_t _cache_hit;
    std::size_t _query_count;
};

#endif // end of __MRU_CACHE_H_256FCF72_8663_41DC_B98A_B822F6007912__
