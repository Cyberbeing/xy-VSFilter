#ifndef __XY_CIRCULAR_ARRAY_QUEUE_HPP_47831C93_EDAA_44B9_94F2_6FF36839D1A6__ 
#define __XY_CIRCULAR_ARRAY_QUEUE_HPP_47831C93_EDAA_44B9_94F2_6FF36839D1A6__

template <typename ElementType>
class XYCircularArrayQueue
{
public:
    typedef ElementType                 value_type;
    typedef ElementType&                reference;
    typedef const ElementType&          const_reference;
public:
    /***
     *  @brief  Default constructor creates no elements.
     **/
    explicit XYCircularArrayQueue()
        :_elements(NULL),
        _head(0),
        _tail(0),
        _CAPACITY(0)
    {
    }

    ~XYCircularArrayQueue()
    {
        delete[]  _elements;
    }

    inline int init(size_t  capacity)
    {
        _CAPACITY = capacity;
        _elements = new value_type[_CAPACITY];
        return _elements ? 0 : -1;
    }

    inline size_t capacity()
    {
        return _CAPACITY;
    }

    inline bool full() const
    {
        return size() >= (_CAPACITY -1);
    }

    inline bool empty() const
    {
        return _head == _tail;
    }

    /***
     * @return: the number of elements in the @queue.
     **/
    inline size_t size() const
    {
        return (_CAPACITY -_head+_tail) %_CAPACITY;
    }

    /***
     * @return: the number of free elements in the @queue. 
     **/
    inline size_t free() const
    {
        return _CAPACITY - (_CAPACITY -_head+_tail) %_CAPACITY;
    }

    /***
     *  @return: a read/write reference to the i-th element.
     **/
    inline reference get_at(int i)
    {
        i += _head;
        i %= _CAPACITY;
        return _elements[i];
    }

    /***
     *  @return: a read-only reference to the i-th element.
     **/
    inline const_reference get_at(int i) const
    {
        i += _head;
        i %= _CAPACITY;
        return _elements[i];
    }

    /***
     *  @return: a read/write reference to the last element
     **/
    inline reference back()
    {
        return _elements[(_CAPACITY +_tail -1) % _CAPACITY];
    }

    /***
     *  @return: a read-only reference to the last element
     **/
    inline const_reference back() const
    {
        return _elements[(_CAPACITY +_tail -1) % _CAPACITY];
    }

    /***
     *  @return: 0 if succeeded, -1 if failed
     **/
    inline int push_back(const_reference value)
    {
        if(size() == (_CAPACITY -1))
        {
            return -1;
        }
        else
        {
            _elements[_tail] = value;
            _tail = (_tail+1) % _CAPACITY;
            return 0;
        }
    }

    inline reference inc_1_at_tail()
    {
        _tail++;
        _tail += _CAPACITY;
        _tail %= _CAPACITY;
        return _elements[(_CAPACITY +_tail -1) % _CAPACITY];
    }

    inline void pop_front()
    {
        if(empty())
        {
            return;
        }
        else
        {
            _head = (_head + 1) % _CAPACITY;
        }
    }

    inline void pop_back()
    {
        if(empty())
        {
            return;
        }
        else
        {
            _tail = (_tail - 1 + _CAPACITY) % _CAPACITY;
        }
    }

    inline void pop_last_n(int n)
    {
        if (size()<n)
        {
            _head = _tail = 0;
        }
        else
        {
            _tail -= n;
            _tail += _CAPACITY;
            _tail %= _CAPACITY;
        }
    }
private:
    value_type* _elements;
    int32_t  _head;
    int32_t  _tail;
    size_t   _CAPACITY;

};

#endif /* #ifndef __XY_CIRCULAR_ARRAY_QUEUE_HPP_47831C93_EDAA_44B9_94F2_6FF36839D1A6__ */

