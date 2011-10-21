/************************************************************************/
/* author: xy                                                           */
/* date: 20110918                                                       */
/************************************************************************/
#ifndef __MRU_CACHE_H_256FCF72_8663_41DC_B98A_B822F6007912__
#define __MRU_CACHE_H_256FCF72_8663_41DC_B98A_B822F6007912__

/***
 * gather from boost::multi_index MRU example
 * http://www.boost.org/doc/libs/1_46_1/libs/multi_index/example/serialization.cpp
 */

#if !defined(NDEBUG)
#define BOOST_MULTI_INDEX_ENABLE_INVARIANT_CHECKING
#define BOOST_MULTI_INDEX_ENABLE_SAFE_MODE
#endif

#include <boost/config.hpp> /* keep it first to prevent nasty warns in MSVC */
#include <boost/multi_index_container.hpp>
#include <boost/multi_index/hashed_index.hpp>
#include <boost/multi_index/member.hpp>
#include <boost/multi_index/sequenced_index.hpp>
#include <boost/multi_index/mem_fun.hpp> 

/* An MRU (most recently used) list keeps record of the last n
 * inserted items, listing first the newer ones. Care has to be
 * taken when a duplicate item is inserted: instead of letting it
 * appear twice, the MRU list relocates it to the first position.
 */

template <typename Item, typename KeyExtractor>
class mru_list
{
public:
  typedef boost::multi_index::multi_index_container<
    Item,
    boost::multi_index::indexed_by<
      boost::multi_index::sequenced<>,
      boost::multi_index::hashed_unique<
        KeyExtractor
      >
    >
  > item_list;

  typedef typename Item item_type;
  typedef typename item_list::iterator iterator;
  typedef typename item_list::nth_index<1>::type hashed_cache;

  mru_list(std::size_t max_num_items):_max_num_items(max_num_items){}

  void update_cache(const Item& item)
  {
      std::pair<iterator,bool> p=_il.push_front(item);

      if(!p.second){                     /* duplicate item */
          _il.relocate(_il.begin(),p.first); /* put in front */            
      }
      else if(_il.size()>_max_num_items){  /* keep the length <= max_num_items */        
          _il.pop_back();
      }
  }

  std::size_t set_max_num_items( std::size_t max_num_items )
  {
      _max_num_items = max_num_items;
      while(_il.size()>_max_num_items)
      {
          _il.pop_back();
      }
      return _max_num_items;
  }
  inline std::size_t get_max_num_items() const { return _max_num_items; }
  inline std::size_t get_cur_items_num() const { return _il.size(); }

  iterator begin(){return _il.begin();}
  iterator end(){return _il.end();}
  void clear() { _il.clear();}
  const hashed_cache& get_hashed_cache() {return _il.get<1>();} 

protected:
  item_list   _il;
  std::size_t _max_num_items;
};

template <typename Item, typename KeyExtractor>
class enhanced_mru_list:public mru_list<Item, KeyExtractor>
{
public:
    typedef typename hashed_cache::const_iterator hashed_cache_const_iterator;

    enhanced_mru_list(std::size_t max_num_items):mru_list(max_num_items),_cache_hit(0),_query_count(0){}

    std::size_t set_max_num_items( std::size_t max_num_items, bool clear_statistic_info=false )
    {
        if(clear_statistic_info)
        {
            _cache_hit = 0;
            _query_count = 0;
        }
        return __super::set_max_num_items(max_num_items);
    }
    void clear(bool clear_statistic_info=false) 
    { 
        if(clear_statistic_info) 
        { 
            _cache_hit=0; 
            _query_count=0; 
        } 
        __super::clear();         
    }

    template< typename CompatibleKey >
    hashed_cache_const_iterator hash_find(const CompatibleKey & k)
    {
        hashed_cache_const_iterator& iter = _il.get<1>().find(k);
        _query_count++;
        _cache_hit += (iter!=_il.get<1>().end());
        return iter;
    }

    hashed_cache_const_iterator hash_end() const
    {
        return _il.get<1>().end();
    }

    inline std::size_t get_cache_hit() const { return _cache_hit; }
    inline std::size_t get_query_count() const { return _query_count; }
protected:
      std::size_t _cache_hit;
      std::size_t _query_count;
};

#endif // end of __MRU_CACHE_H_256FCF72_8663_41DC_B98A_B822F6007912__
