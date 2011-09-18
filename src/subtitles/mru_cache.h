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
#include <boost/bimap/bimap.hpp>

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

  mru_list(std::size_t max_num_items_):max_num_items(max_num_items_){}

  iterator begin(){return il.begin();}
  iterator end(){return il.end();}
  void clear() { il.clear();}
  const hashed_cache& get_hashed_cache() {return il.get<1>();} 

protected:
  item_list   il;
  std::size_t max_num_items;
};

#endif // end of __MRU_CACHE_H_256FCF72_8663_41DC_B98A_B822F6007912__
