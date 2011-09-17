/************************************************************************/
/* author: xy                                                           */
/* date: 20110514                                                       */
/************************************************************************/
#ifndef __XY_BUFFERED_READER_H_F07E628D_A7F1_4FB8_91D4_DAA68E0C105B__
#define __XY_BUFFERED_READER_H_F07E628D_A7F1_4FB8_91D4_DAA68E0C105B__

namespace xy_utils
{
template<typename ReaderType>
class BufferedReader
{
public:
    BufferedReader(): 
      _inner_reader(NULL),_buff(NULL),_buff_size(0),_buff_start(0),_buff_end(0)
    {}
    ~BufferedReader()
    {
        delete[]_buff;
        _buff=NULL;
    }

    bool Init( ReaderType* reader, int buf_size )
    {
        if( reader==NULL )
        {
            return false;
        }
        _inner_reader = reader;
        if( buf_size>0 )
        {            
            _buff = new char[buf_size];
            if( _buff==NULL )
            {
                return false;
            }
            _buff_size = buf_size;
        }
        return true;
    }
    int Read( void* buf, int size )
    {
        char * buff = (char*)buf;

        int from_buff_size = size<_buff_end-_buff_start ? size : _buff_end-_buff_start;
        memcpy( buff, _buff+_buff_start, from_buff_size );
        size -= from_buff_size;
        _buff_start += from_buff_size;
        if( size>_buff_size )
            return from_buff_size + _inner_reader->Read(buff+from_buff_size, size);
        else if(size>0)
        {
            //_buff_start = 0;
            _buff_end = _inner_reader->Read(_buff, _buff_size);
            if( size > _buff_end )
                size = _buff_end;
            memcpy( buff+from_buff_size, _buff, size );
            _buff_start = size;
            return size+from_buff_size;
        }
        else
            return from_buff_size;
    }
    int BufferedNum() const { return _buff_end-_buff_start; }

private:
    ReaderType * _inner_reader;
    char* _buff;
    int _buff_size;
    int _buff_start;
    int _buff_end;
};

}
#endif // end of __XY_BUFFERED_READER_H_F07E628D_A7F1_4FB8_91D4_DAA68E0C105B__
