/************************************************************************/
/* author: xy                                                           */
/* date: 20110511                                                       */
/************************************************************************/
#ifndef __TEXT_READER_H_B2D9DE8F_8730_436C_9F0A_219EE6D78352__
#define __TEXT_READER_H_B2D9DE8F_8730_436C_9F0A_219EE6D78352__

#include <afx.h>

namespace xy_utils
{

template<typename ReaderType>
class TextReader
{
public:
    typedef enum {ASCII, UTF8, LE16, BE16, ANSI} Encoding;
    TextReader(ReaderType &reader, Encoding encoding): _inner_reader(reader),_encoding(encoding)
    {}

    ~TextReader() {}

    bool ReadLine( CStringW * str )
    {
        if(str==NULL) return false;

        bool fEOF = true;

        str->Empty();

        if(_encoding == ASCII || _encoding == ANSI)
        {
            CStringA stra;
            char c;
            while( _inner_reader.Read(&c, sizeof(c)) == sizeof(c) )
            {
                fEOF = false;
                if(c == '\r') continue;
                if(c == '\n') break;
                stra += c;
            }
            *str = CStringW(CString(stra)); // TODO: codepage
        }
        else if(_encoding == UTF8)
        {
            BYTE b;
            while(_inner_reader.Read(&b, sizeof(b)) == sizeof(b))
            {
                fEOF = false;
                WCHAR c = '?';
                if(!(b&0x80)) // 0xxxxxxx
                {
                    c = b&0x7f;
                }
                else if((b&0xe0) == 0xc0) // 110xxxxx 10xxxxxx
                {
                    c = (b&0x1f)<<6;
                    if(_inner_reader.Read(&b, sizeof(b)) != sizeof(b)) break;
                    c |= (b&0x3f);
                }
                else if((b&0xf0) == 0xe0) // 1110xxxx 10xxxxxx 10xxxxxx
                {
                    c = (b&0x0f)<<12;
                    if(_inner_reader.Read(&b, sizeof(b)) != sizeof(b)) break;
                    c |= (b&0x3f)<<6;
                    if(_inner_reader.Read(&b, sizeof(b)) != sizeof(b)) break;
                    c |= (b&0x3f);
                }
                if(c == '\r') continue;
                if(c == '\n') break;
                *str += c;
            }
        }
        else if(_encoding == LE16)
        {
            WCHAR wc;
            while(_inner_reader.Read(&wc, sizeof(wc)) == sizeof(wc))
            {
                fEOF = false;
                if(wc == '\r') continue;
                if(wc == '\n') break;
                *str += wc;
            }
        }
        else if(_encoding == BE16)
        {
            WCHAR wc;
            while(_inner_reader.Read(&wc, sizeof(wc)) == sizeof(wc))
            {
                fEOF = false;
                wc = ((wc>>8)&0x00ff)|((wc<<8)&0xff00);
                if(wc == '\r') continue;
                if(wc == '\n') break;
                *str += wc;
            }
        }

        return(!fEOF);
    }
    int GetEncoding() const { return _encoding; }

private:
    ReaderType& _inner_reader;
    Encoding _encoding;
};

} //namespace xy_utils

#endif // end of __TEXT_READER_H_B2D9DE8F_8730_436C_9F0A_219EE6D78352__
