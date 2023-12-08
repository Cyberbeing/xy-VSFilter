/*
 *  Copyright (C) 2003-2006 Gabest
 *  http://www.gabest.org
 *
 *  This Program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2, or (at your option)
 *  any later version.
 *
 *  This Program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with GNU Make; see the file COPYING.  If not, write to
 *  the Free Software Foundation, 675 Mass Ave, Cambridge, MA 02139, USA.
 *  http://www.gnu.org/copyleft/gpl.html
 *
 */

#include "stdafx.h"
#include "STS.h"
#include <atlbase.h>

#include "RealTextParser.h"
#include <fstream>

#include "CAutoTiming.h"

#include <algorithm>
#include <vector>
#include "xy_logger.h"

// gathered from http://www.netwave.or.jp/~shikai/shikai/shcolor.htm

static struct htmlcolor {TCHAR* name; DWORD color;} hmtlcolors[] =
{
    {_T("white"), 0xffffff},
    {_T("whitesmoke"), 0xf5f5f5},
    {_T("ghostwhite"), 0xf8f8ff},
    {_T("snow"), 0xfffafa},
    {_T("gainsboro"), 0xdcdcdc},
    {_T("lightgrey"), 0xd3d3d3},
    {_T("silver"), 0xc0c0c0},
    {_T("darkgray"), 0xa9a9a9},
    {_T("gray"), 0x808080},
    {_T("dimgray"), 0x696969},
    {_T("lightslategray"), 0x778899},
    {_T("slategray"), 0x708090},
    {_T("darkslategray"), 0x2f4f4f},
    {_T("black"), 0x000000},

    {_T("azure"), 0xf0ffff},
    {_T("aliceblue"), 0xf0f8ff},
    {_T("mintcream"), 0xf5fffa},
    {_T("honeydew"), 0xf0fff0},
    {_T("lightcyan"), 0xe0ffff},
    {_T("paleturqoise"), 0xafeeee},
    {_T("powderblue"), 0xb0e0e6},
    {_T("lightblue"), 0xadd8ed},
    {_T("lightsteelblue"), 0xb0c4de},
    {_T("skyblue"), 0x87ceeb},
    {_T("lightskyblue"), 0x87cefa},
    {_T("cyan"), 0x00ffff},
    {_T("aqua"), 0x00ff80},
    {_T("deepskyblue"), 0x00bfff},
    {_T("aquamarine"), 0x7fffd4},
    {_T("turquoise"), 0x40e0d0},
    {_T("darkturquoise"), 0x00ced1},
    {_T("lightseagreen"), 0x20b2aa},
    {_T("mediumturquoise"), 0x40e0dd},
    {_T("mediumaquamarine"), 0x66cdaa},
    {_T("cadetblue"), 0x5f9ea0},
    {_T("teal"), 0x008080},
    {_T("darkcyan"), 0x008b8b},
    {_T("comflowerblue"), 0x6495ed},
    {_T("dodgerblue"), 0x1e90ff},
    {_T("steelblue"), 0x4682b4},
    {_T("royalblue"), 0x4169e1},
    {_T("blue"), 0x0000ff},
    {_T("mediumblue"), 0x0000cd},
    {_T("mediumslateblue"), 0x7b68ee},
    {_T("slateblue"), 0x6a5acd},
    {_T("darkslateblue"), 0x483d8b},
    {_T("darkblue"), 0x00008b},
    {_T("midnightblue"), 0x191970},
    {_T("navy"), 0x000080},

    {_T("palegreen"), 0x98fb98},
    {_T("lightgreen"), 0x90ee90},
    {_T("mediumspringgreen"), 0x00fa9a},
    {_T("springgreen"), 0x00ff7f},
    {_T("chartreuse"), 0x7fff00},
    {_T("lawngreen"), 0x7cfc00},
    {_T("lime"), 0x00ff00},
    {_T("limegreen"), 0x32cd32},
    {_T("greenyellow"), 0xadff2f},
    {_T("yellowgreen"), 0x9acd32},
    {_T("darkseagreen"), 0x8fbc8f},
    {_T("mediumseagreen"), 0x3cb371},
    {_T("seagreen"), 0x2e8b57},
    {_T("olivedrab"), 0x6b8e23},
    {_T("forestgreen"), 0x228b22},
    {_T("green"), 0x008000},
    {_T("darkkhaki"), 0xbdb76b},
    {_T("olive"), 0x808000},
    {_T("darkolivegreen"), 0x556b2f},
    {_T("darkgreen"), 0x006400},

    {_T("floralwhite"), 0xfffaf0},
    {_T("seashell"), 0xfff5ee},
    {_T("ivory"), 0xfffff0},
    {_T("beige"), 0xf5f5dc},
    {_T("cornsilk"), 0xfff8dc},
    {_T("lemonchiffon"), 0xfffacd},
    {_T("lightyellow"), 0xffffe0},
    {_T("lightgoldenrodyellow"), 0xfafad2},
    {_T("papayawhip"), 0xffefd5},
    {_T("blanchedalmond"), 0xffedcd},
    {_T("palegoldenrod"), 0xeee8aa},
    {_T("khaki"), 0xf0eb8c},
    {_T("bisque"), 0xffe4c4},
    {_T("moccasin"), 0xffe4b5},
    {_T("navajowhite"), 0xffdead},
    {_T("peachpuff"), 0xffdab9},
    {_T("yellow"), 0xffff00},
    {_T("gold"), 0xffd700},
    {_T("wheat"), 0xf5deb3},
    {_T("orange"), 0xffa500},
    {_T("darkorange"), 0xff8c00},

    {_T("oldlace"), 0xfdf5e6},
    {_T("linen"), 0xfaf0e6},
    {_T("antiquewhite"), 0xfaebd7},
    {_T("lightsalmon"), 0xffa07a},
    {_T("darksalmon"), 0xe9967a},
    {_T("salmon"), 0xfa8072},
    {_T("lightcoral"), 0xf08080},
    {_T("indianred"), 0xcd5c5c},
    {_T("coral"), 0xff7f50},
    {_T("tomato"), 0xff6347},
    {_T("orangered"), 0xff4500},
    {_T("red"), 0xff0000},
    {_T("crimson"), 0xdc143c},
    {_T("firebrick"), 0xb22222},
    {_T("maroon"), 0x800000},
    {_T("darkred"), 0x8b0000},

    {_T("lavender"), 0xe6e6fe},
    {_T("lavenderblush"), 0xfff0f5},
    {_T("mistyrose"), 0xffe4e1},
    {_T("thistle"), 0xd8bfd8},
    {_T("pink"), 0xffc0cb},
    {_T("lightpink"), 0xffb6c1},
    {_T("palevioletred"), 0xdb7093},
    {_T("hotpink"), 0xff69b4},
    {_T("fuchsia"), 0xff00ee},
    {_T("magenta"), 0xff00ff},
    {_T("mediumvioletred"), 0xc71585},
    {_T("deeppink"), 0xff1493},
    {_T("plum"), 0xdda0dd},
    {_T("violet"), 0xee82ee},
    {_T("orchid"), 0xda70d6},
    {_T("mediumorchid"), 0xba55d3},
    {_T("mediumpurple"), 0x9370db},
    {_T("purple"), 0x9370db},
    {_T("blueviolet"), 0x8a2be2},
    {_T("darkviolet"), 0x9400d3},
    {_T("darkorchid"), 0x9932cc},

    {_T("tan"), 0xd2b48c},
    {_T("burlywood"), 0xdeb887},
    {_T("sandybrown"), 0xf4a460},
    {_T("peru"), 0xcd853f},
    {_T("goldenrod"), 0xdaa520},
    {_T("darkgoldenrod"), 0xb8860b},
    {_T("chocolate"), 0xd2691e},
    {_T("rosybrown"), 0xbc8f8f},
    {_T("sienna"), 0xa0522d},
    {_T("saddlebrown"), 0x8b4513},
    {_T("brown"), 0xa52a2a},
};

CHtmlColorMap::CHtmlColorMap()
{
    for(int i = 0; i < countof(hmtlcolors); i++)
        SetAt(hmtlcolors[i].name, hmtlcolors[i].color);
}

CHtmlColorMap g_colors;

CString g_default_style(_T("Default"));

//

BYTE CharSetList[] =
{
    ANSI_CHARSET,
    DEFAULT_CHARSET,
    SYMBOL_CHARSET,
    SHIFTJIS_CHARSET,
    HANGEUL_CHARSET,
    HANGUL_CHARSET,
    GB2312_CHARSET,
    CHINESEBIG5_CHARSET,
    OEM_CHARSET,
    JOHAB_CHARSET,
    HEBREW_CHARSET,
    ARABIC_CHARSET,
    GREEK_CHARSET,
    TURKISH_CHARSET,
    VIETNAMESE_CHARSET,
    THAI_CHARSET,
    EASTEUROPE_CHARSET,
    RUSSIAN_CHARSET,
    MAC_CHARSET,
    BALTIC_CHARSET
};

TCHAR* CharSetNames[] =
{
    _T("ANSI"),
    _T("DEFAULT"),
    _T("SYMBOL"),
    _T("SHIFTJIS"),
    _T("HANGEUL"),
    _T("HANGUL"),
    _T("GB2312"),
    _T("CHINESEBIG5"),
    _T("OEM"),
    _T("JOHAB"),
    _T("HEBREW"),
    _T("ARABIC"),
    _T("GREEK"),
    _T("TURKISH"),
    _T("VIETNAMESE"),
    _T("THAI"),
    _T("EASTEUROPE"),
    _T("RUSSIAN"),
    _T("MAC"),
    _T("BALTIC"),
};

int CharSetLen = countof(CharSetList);

static void LogSegments(const CAtlArray<STSSegment>& segments)
{
#ifdef __DO_LOG
    for (int i=0;i<segments.GetCount();i++)
    {
        const STSSegment& s = segments[i];
        XY_LOG_INFO(_T("\tsegments ")<<i<<_T(":")<<s.start<<_T(" ")
            <<s.end<<_T(" ")<<s.subs.GetCount());
        XY_LOG_INFO(_T("\tsubs: "));
        for (int j=0;j<s.subs.GetCount();j++)
        {
            XY_LOG_INFO(_T("\t\t ")<<s.subs[j]);
        }
    }
#endif
}

//

static DWORD CharSetToCodePage(DWORD dwCharSet)
{
    CHARSETINFO cs={0};
    ::TranslateCharsetInfo((DWORD *)dwCharSet, &cs, TCI_SRCCHARSET);
    return cs.ciACP;
}

static int FindChar(CStringW str, WCHAR c, int pos, bool fUnicode, int CharSet)
{
    if(fUnicode) return(str.Find(c, pos));

    int fStyleMod = 0;

    DWORD cp = CharSetToCodePage(CharSet);
    int OrgCharSet = CharSet;

    for(int i = 0, j = str.GetLength(), k; i < j; i++)
    {
        WCHAR c2 = str[i];

        if(IsDBCSLeadByteEx(cp, (BYTE)c2)) i++;
        else if(i >= pos)
        {
            if(c2 == c) return(i);
        }

        if(c2 == '{') fStyleMod++;
        else if(fStyleMod > 0)
        {
            if(c2 == '}') fStyleMod--;
            else if(c2 == 'e' && i >= 3 && i < j-1 && str.Mid(i-2, 3) == L"\\fe")
            {
                CharSet = 0;
                for(k = i+1; _istdigit(str[k]); k++) CharSet = CharSet*10 + (str[k] - '0');
                if(k == i+1) CharSet = OrgCharSet;

                cp = CharSetToCodePage(CharSet);
            }
        }
    }

    return(-1);
}
/*
int FindChar(CStringA str, char c, int pos, bool fUnicode, int CharSet)
{
    ASSERT(!fUnicode);

    return(FindChar(AToW(str), c, pos, false, CharSet));
}
*/
static CStringW ToMBCS(CStringW str, DWORD CharSet)
{
    CStringW ret;

    DWORD cp = CharSetToCodePage(CharSet);

    for(int i = 0, j = str.GetLength(); i < j; i++)
    {
        WCHAR wc = str.GetAt(i);
        char c[8];

        int len;
        if((len = WideCharToMultiByte(cp, 0, &wc, 1, c, 8, NULL, NULL)) > 0)
        {
            for(int k = 0; k < len; k++)
                ret += (WCHAR)(BYTE)c[k];
        }
        else
        {
            ret += '?';
        }
    }

    return(ret);
}

static CStringW UnicodeSSAToMBCS(CStringW str, DWORD CharSet)
{
    CStringW ret;

    int OrgCharSet = CharSet;

    for(int j = 0; j < str.GetLength(); )
    {
        j = str.Find('{', j);
        if(j >= 0)
        {
            ret += ToMBCS(str.Left(j), CharSet);
            str = str.Mid(j);

            j = str.Find('}');
            if(j < 0)
            {
                ret += ToMBCS(str, CharSet);
                break;
            }
            else
            {
                int k = str.Find(L"\\fe");
                if(k >= 0 && k < j)
                {
                    CharSet = 0;
                    int l = k+3;
                    for(; _istdigit(str[l]); l++) CharSet = CharSet*10 + (str[l] - '0');
                    if(l == k+3) CharSet = OrgCharSet;
                }

                j++;

                ret += ToMBCS(str.Left(j), OrgCharSet);
                str = str.Mid(j);
                j = 0;
            }
        }
        else
        {
            ret += ToMBCS(str, CharSet);
            break;
        }
    }

    return(ret);
}

static CStringW ToUnicode(CStringW str, DWORD CharSet)
{
    CStringW ret;

    DWORD cp = CharSetToCodePage(CharSet);

    for(int i = 0, j = str.GetLength(); i < j; i++)
    {
        WCHAR wc = str.GetAt(i);
        char c = wc&0xff;

        if(IsDBCSLeadByteEx(cp, (BYTE)wc))
        {
            i++;

            if(i < j)
            {
                char cc[2];
                cc[0] = c;
                cc[1] = (char)str.GetAt(i);

                MultiByteToWideChar(cp, 0, cc, 2, &wc, 1);
            }
        }
        else
        {
            MultiByteToWideChar(cp, 0, &c, 1, &wc, 1);
        }

        ret += wc;
    }

    return(ret);
}

static CStringW MBCSSSAToUnicode(CStringW str, int CharSet)
{
    CStringW ret;

    int OrgCharSet = CharSet;

    for(int j = 0; j < str.GetLength(); )
    {
        j = FindChar(str, '{', 0, false, CharSet);

        if(j >= 0)
        {
            ret += ToUnicode(str.Left(j), CharSet);
            str = str.Mid(j);

            j = FindChar(str, '}', 0, false, CharSet);

            if(j < 0)
            {
                ret += ToUnicode(str, CharSet);
                break;
            }
            else
            {
                int k = str.Find(L"\\fe");
                if(k >= 0 && k < j)
                {
                    CharSet = 0;
                    int l = k+3;
                    for(; _istdigit(str[l]); l++) CharSet = CharSet*10 + (str[l] - '0');
                    if(l == k+3) CharSet = OrgCharSet;
                }

                j++;

                ret += ToUnicode(str.Left(j), OrgCharSet);
                str = str.Mid(j);
                j = 0;
            }
        }
        else
        {
            ret += ToUnicode(str, CharSet);
            break;
        }
    }

    return(ret);
}

static CStringW RemoveSSATags(CStringW str, bool fUnicode, int CharSet)
{
    str.Replace (L"{\\i1}", L"<i>");
    str.Replace (L"{\\i}", L"</i>");

    for(int i = 0, j; i < str.GetLength(); )
    {
        if((i = FindChar(str, '{', i, fUnicode, CharSet)) < 0) break;
        if((j = FindChar(str, '}', i, fUnicode, CharSet)) < 0) break;
        str.Delete(i, j-i+1);
    }

    str.Replace(L"\\N", L"\n");
    str.Replace(L"\\n", L"\n");
    str.Replace(L"\\h", L" ");

    return(str);
}

//

static CStringW SubRipper2SSA(CStringW str, int CharSet)
{
    str.Replace(L"<i>", L"{\\i1}");
    str.Replace(L"</i>", L"{\\i}");
    str.Replace(L"<b>", L"{\\b1}");
    str.Replace(L"</b>", L"{\\b}");
    str.Replace(L"<u>", L"{\\u1}");
    str.Replace(L"</u>", L"{\\u}");

    return(str);
}

static bool OpenSubRipper(CTextFile* file, CSimpleTextSubtitle& ret, int CharSet)
{
    int num = 0;

    CStringW buff;
    while(file->ReadString(buff))
    {
        buff.Trim();
        if(buff.IsEmpty()) continue;

        WCHAR sep;
        int hh1, mm1, ss1, ms1, hh2, mm2, ss2, ms2;
        int c = swscanf(buff, L"%d%c%d%c%d%c%d --> %d%c%d%c%d%c%d\n",
            &hh1, &sep, &mm1, &sep, &ss1, &sep, &ms1,
            &hh2, &sep, &mm2, &sep, &ss2, &sep, &ms2);

        if(c == 1) // numbering
        {
            num = hh1;
        }
        else if(c == 14) // time info
        {
            CStringW str, tmp;

            bool fFoundEmpty = false;

            while(true)
            {
                bool fEOF = !file->ReadString(tmp);
                if (fEOF && tmp.IsEmpty()) break;

                tmp.Trim();
                if(tmp.IsEmpty()) fFoundEmpty = true;

                int num2;
                WCHAR c;
                if(swscanf(tmp, L"%d%c", &num2, &c) == 1 && fFoundEmpty)
                {
                    num = num2;
                    break;
                }

                str += tmp + '\n';

                if(fEOF) break;
            }

            ret.Add(
                SubRipper2SSA(str, CharSet),
                file->IsUnicode(),
                (((hh1*60 + mm1)*60) + ss1)*1000 + ms1,
                (((hh2*60 + mm2)*60) + ss2)*1000 + ms2);
        }
        else if(c != EOF) // might be another format
        {
            return(false);
        }
    }

    return(!ret.IsEmpty());
}

static bool OpenOldSubRipper(CTextFile* file, CSimpleTextSubtitle& ret, int CharSet)
{
    CStringW buff;
    while(file->ReadString(buff))
    {
        buff.Trim();
        if(buff.IsEmpty()) continue;

        for(int i = 0; i < buff.GetLength(); i++)
        {
            if((i = FindChar(buff, '|', i, file->IsUnicode(), CharSet)) < 0) break;
            buff.SetAt(i, '\n');
        }

        int hh1, mm1, ss1, hh2, mm2, ss2;
        int c = swscanf(buff, L"{%d:%d:%d}{%d:%d:%d}", &hh1, &mm1, &ss1, &hh2, &mm2, &ss2);

        if(c == 6)
        {
            ret.Add(
                buff.Mid(buff.Find('}', buff.Find('}')+1)+1),
                file->IsUnicode(),
                (((hh1*60 + mm1)*60) + ss1)*1000,
                (((hh2*60 + mm2)*60) + ss2)*1000);
        }
        else if(c != EOF) // might be another format
        {
            return(false);
        }
    }

    return(!ret.IsEmpty());
}

static bool OpenSubViewer(CTextFile* file, CSimpleTextSubtitle& ret, int CharSet)
{
    STSStyle def;
    CStringW font, color, size;
    bool fBold, fItalic, fStriked, fUnderline;

    CStringW buff;
    while(file->ReadString(buff))
    {
        buff.Trim();
        if(buff.IsEmpty()) continue;

        if(buff[0] == '[')
        {
            for(int i = 0; i < buff.GetLength() && buff[i]== '['; )
            {
                int j = buff.Find(']', ++i);
                if(j < i) break;

                CStringW tag = buff.Mid(i,j-i);
                tag.Trim();
                tag.MakeLower();

                i += j-i;

                j = buff.Find('[', ++i);
                if(j < 0) j = buff.GetLength();

                CStringW param = buff.Mid(i,j-i);
                param.Trim(L" \\t,");

                i = j;

                if(tag == L"font")
                    font = def.fontName.CompareNoCase(WToT(param)) ? param : L"";
                else if(tag == L"colf")
                    color = def.colors[0] != wcstol(((LPCWSTR)param)+2, 0, 16) ? param : L"";
                else if(tag == L"size")
                    size = def.fontSize != wcstol(param, 0, 10) ? param : L"";
                else if(tag == L"style")
                {
                    if(param.Find(L"no") >= 0)
                    {
                        fBold = fItalic = fStriked = fUnderline = false;
                    }
                    else
                    {
                        fBold = def.fontWeight < FW_BOLD && param.Find(L"bd") >= 0;
                        fItalic = def.fItalic && param.Find(L"it") >= 0;
                        fStriked = def.fStrikeOut && param.Find(L"st") >= 0;
                        fUnderline = def.fUnderline && param.Find(L"ud") >= 0;
                    }
                }
            }

            continue;
        }

        WCHAR sep;
        int hh1, mm1, ss1, hs1, hh2, mm2, ss2, hs2;
        int c = swscanf(buff, L"%d:%d:%d%c%d,%d:%d:%d%c%d\n",
            &hh1, &mm1, &ss1, &sep, &hs1, &hh2, &mm2, &ss2, &sep, &hs2);

        if(c == 10)
        {
            CStringW str;
            file->ReadString(str);

            str.Replace(L"[br]", L"\\N");

            CStringW prefix;
            if(!font.IsEmpty()) prefix += L"\\fn" + font;
            if(!color.IsEmpty()) prefix += L"\\c" + color;
            if(!size.IsEmpty()) prefix += L"\\fs" + size;
            if(fBold) prefix += L"\\b1";
            if(fItalic) prefix += L"\\i1";
            if(fStriked) prefix += L"\\s1";
            if(fUnderline) prefix += L"\\u1";
            if(!prefix.IsEmpty()) str = L"{" + prefix + L"}" + str;

            ret.Add(str,
                file->IsUnicode(),
                (((hh1*60 + mm1)*60) + ss1)*1000 + hs1*10,
                (((hh2*60 + mm2)*60) + ss2)*1000 + hs2*10);
        }
        else if(c != EOF) // might be another format
        {
            return(false);
        }
    }

    return(!ret.IsEmpty());
}

static STSStyle* GetMicroDVDStyle(CString str, int CharSet)
{
    STSStyle* ret = new STSStyle();
    if(!ret) return(NULL);

    for(int i = 0, len = str.GetLength(); i < len; i++)
    {
        int j = str.Find('{', i);
        if(j < 0) j = len;

        if(j >= len) break;

        int k = str.Find('}', j);
        if(k < 0) k = len;

        CString code = str.Mid(j, k-j);
        if(code.GetLength() > 2) code.SetAt(1, (TCHAR)towlower(code[1]));

        if(!_tcsnicmp(code, _T("{c:$"), 4))
        {
            _stscanf(code, _T("{c:$%x"), &ret->colors[0]);
        }
        else if(!_tcsnicmp(code, _T("{f:"), 3))
        {
            ret->fontName = code.Mid(3);
        }
        else if(!_tcsnicmp(code, _T("{s:"), 3))
        {
            float f;
            if(1 == _stscanf(code, _T("{s:%f"), &f))
                ret->fontSize = f;
        }
        else if(!_tcsnicmp(code, _T("{h:"), 3))
        {
            _stscanf(code, _T("{h:%d"), &ret->charSet);
        }
        else if(!_tcsnicmp(code, _T("{y:"), 3))
        {
            code.MakeLower();
            if(code.Find('b') >= 0) ret->fontWeight = FW_BOLD;
            if(code.Find('i') >= 0) ret->fItalic = true;
            if(code.Find('u') >= 0) ret->fUnderline = true;
            if(code.Find('s') >= 0) ret->fStrikeOut = true;
        }
        else if(!_tcsnicmp(code, _T("{p:"), 3))
        {
            int p;
            _stscanf(code, _T("{p:%d"), &p);
            ret->scrAlignment = (p == 0) ? 8 : 2;
        }

        i = k;
    }

    return(ret);
}

static CStringW MicroDVD2SSA(CStringW str, bool fUnicode, int CharSet)
{
    CStringW ret;

    enum {COLOR=0, FONTNAME, FONTSIZE, FONTCHARSET, BOLD, ITALIC, UNDERLINE, STRIKEOUT};
    bool fRestore[8];
    int fRestoreLen = 8;
    memset(fRestore, 0, sizeof(bool)*fRestoreLen);

    for(int pos = 0, eol; pos < str.GetLength(); pos++)
    {
        if((eol = FindChar(str, '|', pos, fUnicode, CharSet)) < 0) eol = str.GetLength();

        CStringW line = str.Mid(pos, eol-pos);

        pos = eol;

        for(int i = 0, j, k, len = line.GetLength(); i < len; i++)
        {
            if((j = FindChar(line, '{', i, fUnicode, CharSet)) < 0) j = str.GetLength();

            ret += line.Mid(i, j-i);

            if(j >= len) break;

            if((k = FindChar(line, '}', j, fUnicode, CharSet)) < 0) k = len;

            {
                CStringW code = line.Mid(j, k-j);

                if(!wcsnicmp(code, L"{c:$", 4))
                {
                    fRestore[COLOR] = (iswupper(code[1]) == 0);
                    code.MakeLower();

                    int color;
                    swscanf(code, L"{c:$%x", &color);
                    code.Format(L"{\\c&H%x&}", color);
                    ret += code;
                }
                else if(!wcsnicmp(code, L"{f:", 3))
                {
                    fRestore[FONTNAME] = (iswupper(code[1]) == 0);

                    code.Format(L"{\\fn%s}", code.Mid(3));
                    ret += code;
                }
                else if(!wcsnicmp(code, L"{s:", 3))
                {
                    fRestore[FONTSIZE] = (iswupper(code[1]) == 0);
                    code.MakeLower();

                    float size;
                    swscanf(code, L"{s:%f", &size);
                    code.Format(L"{\\fs%f}", size);
                    ret += code;
                }
                else if(!wcsnicmp(code, L"{h:", 3))
                {
                    fRestore[COLOR] = (_istupper(code[1]) == 0);
                    code.MakeLower();

                    int CharSet;
                    swscanf(code, L"{h:%d", &CharSet);
                    code.Format(L"{\\fe%d}", CharSet);
                    ret += code;
                }
                else if(!wcsnicmp(code, L"{y:", 3))
                {
                    bool f = (_istupper(code[1]) == 0);

                    code.MakeLower();

                    ret += '{';
                    if(code.Find('b') >= 0) {ret += L"\\b1"; fRestore[BOLD] = f;}
                    if(code.Find('i') >= 0) {ret += L"\\i1"; fRestore[ITALIC] = f;}
                    if(code.Find('u') >= 0) {ret += L"\\u1"; fRestore[UNDERLINE] = f;}
                    if(code.Find('s') >= 0) {ret += L"\\s1"; fRestore[STRIKEOUT] = f;}
                    ret += '}';
                }
                else if(!wcsnicmp(code, L"{o:", 3))
                {
                    code.MakeLower();

                    int x, y;
                    TCHAR c;
                    swscanf(code, L"{o:%d%c%d", &x, &c, &y);
                    code.Format(L"{\\move(%d,%d,0,0,0,0)}", x, y);
                    ret += code;
                }
                else ret += code;
            }

            i = k;
        }

        if(pos >= str.GetLength()) break;

        for(int i = 0; i < fRestoreLen; i++)
        {
            if(fRestore[i])
            {
                switch(i)
                {
                case COLOR: ret += L"{\\c}"; break;
                case FONTNAME: ret += L"{\\fn}"; break;
                case FONTSIZE: ret += L"{\\fs}"; break;
                case FONTCHARSET: ret += L"{\\fe}"; break;
                case BOLD: ret += L"{\\b}"; break;
                case ITALIC: ret += L"{\\i}"; break;
                case UNDERLINE: ret += L"{\\u}"; break;
                case STRIKEOUT: ret += L"{\\s}"; break;
                default: break;
                }
            }
        }

        memset(fRestore, 0, sizeof(bool)*fRestoreLen);

        ret += L"\\N";
    }

    return(ret);
}

static bool OpenMicroDVD(CTextFile* file, CSimpleTextSubtitle& ret, int CharSet)
{
    bool fCheck = false, fCheck2 = false;

    CString style(_T("Default"));

    CStringW buff;;
    while(file->ReadString(buff))
    {
        buff.Trim();
        if(buff.IsEmpty()) continue;

        int start, end;
        int c = swscanf(buff, L"{%d}{%d}", &start, &end);

        if(c != 2) {c = swscanf(buff, L"{%d}{}", &start)+1; end = start + 60; fCheck = true;}

        if(c != 2)
        {
            int i;
            if(buff.Find('{') == 0 && (i = buff.Find('}')) > 1 && i < buff.GetLength())
            {
                if(STSStyle* s = GetMicroDVDStyle(WToT(buff.Mid(i+1)), CharSet))
                {
                    style = buff.Mid(1, i-1);
                    style.MakeUpper();
                    if(style.GetLength()) {CString str = style.Mid(1); str.MakeLower(); style = style.Left(1) + str;}
                    ret.AddStyle(style, s);
                    CharSet = s->charSet;
                    continue;
                }
            }
        }

        if(c == 2)
        {
            if(fCheck2 && !ret.IsEmpty())
            {
                STSEntry& stse = ret.m_entries[ret.m_entries.GetCount()-1];
                stse.end = min(stse.end, start);
                fCheck2 = false;
            }

            ret.Add(
                MicroDVD2SSA(buff.Mid(buff.Find('}', buff.Find('}')+1)+1), file->IsUnicode(), CharSet),
                file->IsUnicode(),
                start, end,
                style);

            if(fCheck)
            {
                fCheck = false;
                fCheck2 = true;
            }
        }
        else if(c != EOF) // might be another format
        {
            return(false);
        }
    }

    return(!ret.IsEmpty());
}

static void ReplaceNoCase(CStringW& str, CStringW from, CStringW to)
{
    CStringW lstr = str;
    lstr.MakeLower();

    int i, j, k;

    for(i = 0, j = str.GetLength(); i < j; )
    {
        if((k = lstr.Find(from, i)) >= 0)
        {
            str.Delete(k, from.GetLength()); lstr.Delete(k, from.GetLength());
            str.Insert(k, to); lstr.Insert(k, to);
            i = k + to.GetLength();
            j = str.GetLength();
        }
        else break;
    }
}

static CStringW SMI2SSA(CStringW str, int CharSet)
{
    ReplaceNoCase(str, L"&nbsp;", L" ");
    ReplaceNoCase(str, L"&quot;", L"\"");
    ReplaceNoCase(str, L"<br>", L"\\N");
    ReplaceNoCase(str, L"<i>", L"{\\i1}");
    ReplaceNoCase(str, L"</i>", L"{\\i}");
    ReplaceNoCase(str, L"<b>", L"{\\b1}");
    ReplaceNoCase(str, L"</b>", L"{\\b}");

    CStringW lstr = str;
    lstr.MakeLower();

    // maven@maven.de
    // now parse line
    for(int i = 0, j = str.GetLength(); i < j; )
    {
        int k;
        if((k = lstr.Find('<', i)) < 0) break;

        int chars_inserted = 0;

        int l = 1;
        for(; k+l < j && lstr[k+l] != '>'; l++);
        l++;

// Modified by Cookie Monster
        if (lstr.Find(L"<font ", k) == k)
        {
            CStringW args = lstr.Mid(k+6, l-6); // delete "<font "
            CStringW arg ;

            args.Remove('\"'); args.Remove('#');    // may include 2 * " + #
            arg.TrimLeft(); arg.TrimRight(L" >");

            for (;;)
            {
                args.TrimLeft();
                arg = args.SpanExcluding(L" \t>");
                args = args.Mid(arg.GetLength());

                if(arg.IsEmpty())
                    break;
                if (arg.Find(L"color=") == 0 )
                {
                    DWORD color;

                    arg = arg.Mid(6);   // delete "color="
                    if ( arg.IsEmpty())
                        continue;

                    DWORD val;
                    if(g_colors.Lookup(CString(arg), val))
                        color = (DWORD)val;
                    else if((color = wcstol(arg, NULL, 16) ) == 0)
                        color = 0x00ffffff;  // default is white

                    arg.Format(L"%02x%02x%02x", color&0xff, (color>>8)&0xff, (color>>16)&0xff);
                    lstr.Insert(k + l + chars_inserted, CStringW(L"{\\c&H") + arg + L"&}");
                    str.Insert(k + l + chars_inserted, CStringW(L"{\\c&H") + arg + L"&}");
                    chars_inserted += 5 + arg.GetLength() + 2;
                }
/*
                else if (arg.Find(_T("size=" )) == 0 )
                {
                    uint fsize;

                    arg = arg.Mid(5);   // delete "size="
                    if ( arg.GetLength() == 0)
                        continue;

                    if ( fsize = _tcstol(arg, &tmp, 10) == 0 )
                        continue;

                    lstr.Insert(k + l + chars_inserted, CString(_T("{\\fs")) + arg + _T("&}"));
                    str.Insert(k + l + chars_inserted, CString(_T("{\\fs")) + arg + _T("&}"));
                    chars_inserted += 4 + arg.GetLength() + 2;
                }
*/
            }
        }

// Original Code
/*
        if (lstr.Find(L"<font color=", k) == k)
        {
            CStringW arg = lstr.Mid(k+12, l-12); // may include 2 * " + #

            arg.Remove('\"');
            arg.Remove('#');
            arg.TrimLeft(); arg.TrimRight(L" >");

            if(arg.GetLength() > 0)
            {
                DWORD color;

                CString key = WToT(arg);
                void* val;
                if(g_colors.Lookup(key, val)) color = (DWORD)val;
                else color = wcstol(arg, NULL, 16);

                arg.Format(L"%02x%02x%02x", color&0xff, (color>>8)&0xff, (color>>16)&0xff);
            }

            lstr.Insert(k + l + chars_inserted, L"{\\c&H" + arg + L"&}");
            str.Insert(k + l + chars_inserted, L"{\\c&H" + arg + L"&}");
            chars_inserted += 5 + arg.GetLength() + 2;
        }
*/
        else if (lstr.Find(L"</font>", k) == k)
        {
            lstr.Insert(k + l + chars_inserted, L"{\\c}");
            str.Insert(k + l + chars_inserted, L"{\\c}");
            chars_inserted += 4;
        }

        str.Delete(k, l); lstr.Delete(k, l);
        i = k + chars_inserted;
        j = str.GetLength();
    }

    return(str);
}

static bool OpenSami(CTextFile* file, CSimpleTextSubtitle& ret, int CharSet)
{
    CStringW buff, caption;

    ULONGLONG pos = file->GetPosition();

    bool fSAMI = false;

    while(file->ReadString(buff) && !fSAMI)
    {
        if(buff.MakeUpper().Find(L"<SAMI>") >= 0) fSAMI = true;
    }

    if(!fSAMI) return(false);

    file->Seek(pos, 0);

    bool fComment = false;

    int start_time = 0;

    while(file->ReadString(buff))
    {
        buff.Trim();
        if(buff.IsEmpty()) continue;

        CStringW ubuff = buff;
        ubuff.MakeUpper();

        if(ubuff.Find(L"<!--") >= 0 || ubuff.Find(L"<TITLE>") >= 0)
            fComment = true;

        if(!fComment)
        {
            int i;

            if((i = ubuff.Find(L"<SYNC START=")) >= 0)
            {
                int time = 0;

                for(i = 12; i < ubuff.GetLength(); i++)
                {
                    if(ubuff[i] != '>' && ubuff[i] != 'M')
                    {
                        if(iswdigit(ubuff[i]))
                        {
                            time *= 10;
                            time += ubuff[i] - 0x30;
                        }
                    }
                    else break;
                }

                ret.Add(
                    SMI2SSA(caption, CharSet),
                    file->IsUnicode(),
                    start_time, time);

                start_time = time;
                caption.Empty();
            }

            caption += buff;
        }

        if(ubuff.Find(L"-->") >= 0 || ubuff.Find(L"</TITLE>") >= 0)
            fComment = false;
    }

    ret.Add(
        SMI2SSA(caption, CharSet),
        file->IsUnicode(),
        start_time, MAXLONG);

    return(true);
}

static bool OpenVPlayer(CTextFile* file, CSimpleTextSubtitle& ret, int CharSet)
{
    CStringW buff;
    while(file->ReadString(buff))
    {
        buff.Trim();
        if(buff.IsEmpty()) continue;

        for(int i = 0; i < buff.GetLength(); i++)
        {
            if((i = FindChar(buff, '|', i, file->IsUnicode(), CharSet)) < 0) break;
            buff.SetAt(i, '\n');
        }

        int hh, mm, ss;
        int c = swscanf(buff, L"%d:%d:%d:", &hh, &mm, &ss);

        if(c == 3)
        {
            CStringW str = buff.Mid(buff.Find(':', buff.Find(':', buff.Find(':')+1)+1)+1);
            ret.Add(str,
                file->IsUnicode(),
                (((hh*60 + mm)*60) + ss)*1000,
                (((hh*60 + mm)*60) + ss)*1000 + 1000 + 50*str.GetLength());
        }
        else if(c != EOF) // might be another format
        {
            return(false);
        }
    }

    return(!ret.IsEmpty());
}

static inline CStringW GetStr(CStringW& buff, char sep = ',') //throw(...)
{
    buff.TrimLeft();

    int pos = buff.Find(sep);
    if(pos < 0)
    {
        pos = buff.GetLength();
        if(pos < 1) throw 1;
    }

    CStringW ret = buff.Left(pos);
    if(pos < buff.GetLength()) buff = buff.Mid(pos+1);

    return(ret);
}

static inline int GetInt(CStringW& buff, char sep = ',') //throw(...)
{
    CStringW str;

    str = GetStr(buff, sep);
    str.MakeLower();

    CStringW fmtstr = str.GetLength() > 2 && (str.Left(2) == L"&h" || str.Left(2) == L"0x")
        ? str = str.Mid(2), L"%x"
        : L"%d";

    int ret;
    if(swscanf(str, fmtstr, &ret) != 1) throw 1;

    return(ret);
}

static inline double GetFloat(CStringW& buff, char sep = ',') //throw(...)
{
    CStringW str;

    str = GetStr(buff, sep);
    str.MakeLower();

    float ret;
    if(swscanf(str, L"%f", &ret) != 1) throw 1;

    return((double)ret);
}

static inline CStringW::PCXSTR TryNextStr(CStringW::PXSTR * buff, WCHAR sep = WCHAR(','))
{
    CStringW::PXSTR start = NULL;
    CStringW::PXSTR ret = NULL;
    for(start=*buff; *start!=0 && *start==WCHAR(' '); start++) ;
    
    *buff=start;
    ret = start;

    for( ;*start!=0 && *start!=sep; start++) ;

    if(*start!=0)
        *buff = start+1;

    *start = 0;

    return(ret);
}

static inline int NextInt(CStringW::PXSTR * buff, WCHAR sep = WCHAR(',')) //throw(...)
{
    CStringW::PCXSTR str = TryNextStr(buff, sep);

    const wchar_t *fmtstr = L"%d";
    if (str[0] == '&' && (str[1] == 'h' || str[1] == 'H') || str[0] == '0' && (str[1] == 'x' || str[1] == 'X')) {
        fmtstr = L"%x";
        str += 2;
    }

    int ret;
    if(swscanf(str, fmtstr, &ret) != 1) throw 1;

    return(ret);
}

static inline double NextFloat(CStringW::PXSTR * buff, WCHAR sep = WCHAR(',')) //throw(...)
{
    CStringW str;

    str = TryNextStr(buff, sep);
    str.MakeLower();

    float ret;
    if(swscanf(str, L"%f", &ret) != 1) throw 1;

    return((double)ret);
}

static bool LoadFont(CString& font)
{
    int len = font.GetLength();

    CAutoVectorPtr<BYTE> pData;
    if(len == 0 || (len&3) == 1 || !pData.Allocate(len))
        return(false);

    const TCHAR* s = font;
    const TCHAR* e = s + len;
    for(BYTE* p = pData; s < e; s++, p++) *p = *s - 33;

    for(int i = 0, j = 0, k = len&~3; i < k; i+=4, j+=3)
    {
        pData[j+0] = ((pData[i+0]&63)<<2)|((pData[i+1]>>4)& 3);
        pData[j+1] = ((pData[i+1]&15)<<4)|((pData[i+2]>>2)&15);
        pData[j+2] = ((pData[i+2]& 3)<<6)|((pData[i+3]>>0)&63);
    }

    int datalen = (len&~3)*3/4;

    if((len&3) == 2)
    {
        pData[datalen++] = ((pData[(len&~3)+0]&63)<<2)|((pData[(len&~3)+1]>>4)&3);
    }
    else if((len&3) == 3)
    {
        pData[datalen++] = ((pData[(len&~3)+0]&63)<<2)|((pData[(len&~3)+1]>>4)& 3);
        pData[datalen++] = ((pData[(len&~3)+1]&15)<<4)|((pData[(len&~3)+2]>>2)&15);
    }

    HANDLE hFont = INVALID_HANDLE_VALUE;

    if(HMODULE hModule = LoadLibrary(_T("GDI32.DLL")))
    {
        typedef HANDLE (WINAPI *PAddFontMemResourceEx)( IN PVOID, IN DWORD, IN PVOID , IN DWORD*);
        if(PAddFontMemResourceEx f = (PAddFontMemResourceEx)GetProcAddress(hModule, "AddFontMemResourceEx"))
        {
            DWORD cFonts;
            hFont = f(pData, datalen, NULL, &cFonts);
        }

        FreeLibrary(hModule);
    }

    if(hFont == INVALID_HANDLE_VALUE)
    {
        TCHAR path[MAX_PATH];
        GetTempPath(MAX_PATH, path);

        DWORD chksum = 0;
        for(int i = 0, j = datalen>>2; i < j; i++)
            chksum += ((DWORD*)(BYTE*)pData)[i];

        CString fn;
        fn.Format(_T("%sfont%08x.ttf"), path, chksum);

        CFileStatus fs;
        if(!CFileGetStatus(fn, fs))
        {
            CFile f;
            if(f.Open(fn, CFile::modeCreate|CFile::modeWrite|CFile::typeBinary|CFile::shareDenyWrite))
            {
                f.Write(pData, datalen);
                f.Close();
            }
        }

        AddFontResource(fn);
    }

    return(true);
}

static bool LoadUUEFont(CTextFile* file)
{
    CString s, font;
    while(file->ReadString(s))
    {
        s.Trim();
        if(s.IsEmpty()) break;
        if(s[0] == '[') // check for standard section headers
        {
        	if(s.Find(_T("[Script Info]")) == 0) break;
        	if(s.Find(_T("[V4+ Styles]")) == 0) break;
        	if(s.Find(_T("[V4 Styles]")) == 0) break;
        	if(s.Find(_T("[Events]")) == 0) break;
        	if(s.Find(_T("[Fonts]")) == 0) break;
        	if(s.Find(_T("[Graphics]")) == 0) break;
        }
        if(s.Find(_T("fontname:")) == 0) {LoadFont(font); font.Empty(); continue;}

        font += s;
    }

    if(!font.IsEmpty())
        LoadFont(font);

    return(true);
}

static bool OpenSubStationAlpha(CTextFile* file, CSimpleTextSubtitle& ret, int CharSet)
{
    bool fRet = false;

    int version = 3, sver = 3;

    ret.m_eYCbCrMatrix = CSimpleTextSubtitle::YCbCrMatrix_BT601;
    ret.m_eYCbCrRange  = CSimpleTextSubtitle::YCbCrRange_TV;

    CStringW buff;
    while(true)
    {
    	bool fEOF = !file->ReadString(buff);
    	if(fEOF && buff.IsEmpty()) break;

        buff.Trim();
        if(buff.IsEmpty() || buff.GetAt(0) == ';') continue;

        CStringW entry;

//      try {
            entry = GetStr(buff, ':');
//  }
//      catch(...) {continue;}

        entry.MakeLower();

        if(entry == L"dialogue")
        {
            try
            {
                CStringW::PXSTR __buff = buff.GetBuffer();
                int hh1, mm1, ss1, ms1_div10, hh2, mm2, ss2, ms2_div10, layer = 0;
                CString Style, Actor, Effect;
                CRect marginRect;

                if(version <= 4){TryNextStr(&__buff, '='); NextInt(&__buff);} /* Marked = */
                if(version >= 5)layer = NextInt(&__buff);
                hh1 = NextInt(&__buff, ':');
                mm1 = NextInt(&__buff, ':');
                ss1 = NextInt(&__buff, '.');
                ms1_div10 = NextInt(&__buff);
                hh2 = NextInt(&__buff, ':');
                mm2 = NextInt(&__buff, ':');
                ss2 = NextInt(&__buff, '.');
                ms2_div10 = NextInt(&__buff);
                Style = WToT(TryNextStr(&__buff));
                Actor = WToT(TryNextStr(&__buff));
                marginRect.left = NextInt(&__buff);
                marginRect.right = NextInt(&__buff);
                marginRect.top = marginRect.bottom = NextInt(&__buff);
                if(version >= 6)marginRect.bottom = NextInt(&__buff);
                Effect = WToT(TryNextStr(&__buff));

                CStringW buff2 = __buff;
                int len = min(Effect.GetLength(), buff2.GetLength());
                if(Effect.Left(len) == WToT(buff2.Left(len))) Effect.Empty();

                Style.TrimLeft('*');
                if(!Style.CompareNoCase(_T("Default"))) Style = _T("Default");

                ret.AddSTSEntryOnly(buff2,
                    file->IsUnicode(),
                    (((hh1*60 + mm1)*60) + ss1)*1000 + ms1_div10*10,
                    (((hh2*60 + mm2)*60) + ss2)*1000 + ms2_div10*10,
                    Style, Actor, Effect,
                    marginRect,
                    layer);
            }
            catch(...)
            {
                //                ASSERT(0);
                //                throw;
                return(false);
            }
        }
        else if(entry == L"[script info]")
        {
            fRet = true;
        }
        else if(entry == L"playresx")
        {
            try {ret.m_dstScreenSize.cx = GetInt(buff);}
            catch(...) {ret.m_dstScreenSize = CSize(0, 0); return(false);}

            if(ret.m_dstScreenSize.cy <= 0)
            {
                ret.m_dstScreenSize.cy = (ret.m_dstScreenSize.cx == 1280)
                    ? 1024
                    : ret.m_dstScreenSize.cx * 3 / 4;
            }
        }
        else if(entry == L"playresy")
        {
            try {ret.m_dstScreenSize.cy = GetInt(buff);}
            catch(...) {ret.m_dstScreenSize = CSize(0, 0); return(false);}

            if(ret.m_dstScreenSize.cx <= 0)
            {
                ret.m_dstScreenSize.cx = (ret.m_dstScreenSize.cy == 1024)
                    ? 1280
                    : ret.m_dstScreenSize.cy * 4 / 3;
            }
        }
        else if(entry == L"layoutresx")
        {
            try {ret.m_layout_size.cx = GetInt(buff);}
            catch(...) {ret.m_layout_size = CSize(0, 0); return(false);}
        }
        else if(entry == L"layoutresy")
        {
            try {ret.m_layout_size.cy = GetInt(buff);}
            catch(...) {ret.m_layout_size = CSize(0, 0); return(false);}
        }
        else if(entry == L"wrapstyle")
        {
            try {ret.m_defaultWrapStyle = GetInt(buff);}
            catch(...) {ret.m_defaultWrapStyle = 1; return(false);}
        }
        else if(entry == L"scripttype")
        {
            if(buff.GetLength() >= 4 && !buff.Right(4).CompareNoCase(L"4.00")) version = sver = 4;
            else if(buff.GetLength() >= 5 && !buff.Right(5).CompareNoCase(L"4.00+")) version = sver = 5;
            else if(buff.GetLength() >= 6 && !buff.Right(6).CompareNoCase(L"4.00++")) version = sver = 6;
        }
        else if(entry == L"collisions")
        {
            buff = GetStr(buff);
            buff.MakeLower();
            ret.m_collisions = buff.Find(L"reverse") >= 0 ? 1 : 0;
        }
        else if(entry == L"scaledborderandshadow")
        {
            buff = GetStr(buff);
            buff.MakeLower();
            ret.m_fScaledBAS = buff.Find(L"yes") >= 0;
        }
        else if(entry == L"[v4 styles]")
        {
            fRet = true;
            sver = 4;
        }
        else if(entry == L"[v4+ styles]")
        {
            fRet = true;
            sver = 5;
        }
        else if(entry == L"[v4++ styles]")
        {
            fRet = true;
            sver = 6;
        }
        else if(entry == L"style")
        {
            STSStyle* style = new STSStyle;
            if(!style) return(false);

            try
            {
                CString StyleName;
                int alpha;
                int encoding;
                CRect tmp_rect;

                StyleName = WToT(GetStr(buff));
                style->fontName = WToT(GetStr(buff));
                style->fontSize = GetFloat(buff);
                for(int i = 0; i < 4; i++) style->colors[i] = (COLORREF)GetInt(buff);
                style->fontWeight = !!GetInt(buff) ? FW_BOLD : FW_NORMAL;
                style->fItalic = !!GetInt(buff);
if(sver >= 5)   style->fUnderline = !!GetInt(buff);
if(sver >= 5)   style->fStrikeOut = !!GetInt(buff);
if(sver >= 5)   style->fontScaleX = GetFloat(buff);
if(sver >= 5)   style->fontScaleY = GetFloat(buff);
if(sver >= 5)   style->fontSpacing = GetFloat(buff);
if(sver >= 5)   style->fontAngleZ = GetFloat(buff);
if(sver >= 4)   style->borderStyle = GetInt(buff);
                style->outlineWidthX = style->outlineWidthY = GetFloat(buff);
                style->shadowDepthX = style->shadowDepthY = GetFloat(buff);
                style->scrAlignment = GetInt(buff);
                tmp_rect.left =  GetInt(buff);
                tmp_rect.right = GetInt(buff);
                tmp_rect.top = tmp_rect.bottom = GetInt(buff);
if(sver >= 6)   tmp_rect.bottom = GetInt(buff);
                style->marginRect = tmp_rect;
if(sver <= 4)   alpha = GetInt(buff);
                encoding = GetInt(buff);
if(sver >= 6)   style->relativeTo = GetInt(buff);

                // Map unsupported extension to the most permissive charSet
                style->charSet = encoding < 0 ? DEFAULT_CHARSET : encoding;
if(sver <= 4)   style->colors[2] = style->colors[3]; // style->colors[2] is used for drawing the outline
if(sver <= 4)   alpha = max(min(alpha, 0xff), 0);
if(sver <= 4)   {for(int i = 0; i < 3; i++) style->alpha[i] = alpha; style->alpha[3] = 0x80;}
if(sver >= 5)   for(int i = 0; i < 4; i++) {style->alpha[i] = (BYTE)(style->colors[i]>>24); style->colors[i] &= 0xffffff;}
if(sver >= 5)   style->fontScaleX = max(style->fontScaleX, 0);
if(sver >= 5)   style->fontScaleY = max(style->fontScaleY, 0);
if(sver >= 5)   style->fontSpacing = max(style->fontSpacing, 0);
                style->fontAngleX = style->fontAngleY = 0;
                style->borderStyle = style->borderStyle == 1 ? 0 : style->borderStyle == 3 ? 1 : 0;
                style->outlineWidthX = max(style->outlineWidthX, 0);
                style->outlineWidthY = max(style->outlineWidthY, 0);
                style->shadowDepthX = max(style->shadowDepthX, 0);
                style->shadowDepthY = max(style->shadowDepthY, 0);
if(sver <= 4)   style->scrAlignment = (style->scrAlignment&4) ? ((style->scrAlignment&3)+6) // top
                                        : (style->scrAlignment&8) ? ((style->scrAlignment&3)+3) // mid
                                        : (style->scrAlignment&3); // bottom

                StyleName.TrimLeft('*');

                ret.AddStyle(StyleName, style);
            }
            catch(...)
            {
                delete style;
                return(false);
            }
        }
        else if(entry == L"[events]")
        {
            fRet = true;
        }
        else if(entry == L"fontname")
        {
            LoadUUEFont(file);
        }
        else if(entry == L"ycbcr matrix")
        {
            buff = GetStr(buff);
            // When used with no parameters, TrimRight removes trailing newline,
            // space, and tab characters from the string.
            buff.TrimRight();
            buff.MakeLower();
            if (buff=="none")
            {
                ret.m_eYCbCrMatrix = CSimpleTextSubtitle::YCbCrMatrix_AUTO;
                ret.m_eYCbCrRange = CSimpleTextSubtitle::YCbCrRange_AUTO;
            }
            else if (buff=="tv.601")
            {
                ret.m_eYCbCrMatrix = CSimpleTextSubtitle::YCbCrMatrix_BT601;
                ret.m_eYCbCrRange = CSimpleTextSubtitle::YCbCrRange_TV;
            }
            else if (buff=="tv.709")
            {
                ret.m_eYCbCrMatrix = CSimpleTextSubtitle::YCbCrMatrix_BT709;
                ret.m_eYCbCrRange = CSimpleTextSubtitle::YCbCrRange_TV;
            }
            else if (buff=="pc.601")
            {
                ret.m_eYCbCrMatrix = CSimpleTextSubtitle::YCbCrMatrix_BT601;
                ret.m_eYCbCrRange = CSimpleTextSubtitle::YCbCrRange_PC;
            }
            else if (buff=="pc.709")
            {
                ret.m_eYCbCrMatrix = CSimpleTextSubtitle::YCbCrMatrix_BT709;
                ret.m_eYCbCrRange = CSimpleTextSubtitle::YCbCrRange_PC;
            }
        }
        if(fEOF) break;
    }
//    ret.Sort();
    return(fRet);
}

static bool OpenXombieSub(CTextFile* file, CSimpleTextSubtitle& ret, int CharSet)
{
    float version = 0;

//  CMapStringToPtr stylemap;

    CStringW buff;
    while(file->ReadString(buff))
    {
        buff.Trim();
        if(buff.IsEmpty() || buff.GetAt(0) == ';') continue;

        CStringW entry;

//      try {
            entry = GetStr(buff, '=');
//  }
//      catch(...) {continue;}

        entry.MakeLower();

        if(entry == L"version")
        {
            version = (float)GetFloat(buff);
        }
        else if(entry == L"screenhorizontal")
        {
            try {ret.m_dstScreenSize.cx = GetInt(buff);}
            catch(...) {ret.m_dstScreenSize = CSize(0, 0); return(false);}

            if(ret.m_dstScreenSize.cy <= 0)
            {
                ret.m_dstScreenSize.cy = (ret.m_dstScreenSize.cx == 1280)
                    ? 1024
                    : ret.m_dstScreenSize.cx * 3 / 4;
            }
        }
        else if(entry == L"screenvertical")
        {
            try {ret.m_dstScreenSize.cy = GetInt(buff);}
            catch(...) {ret.m_dstScreenSize = CSize(0, 0); return(false);}

            if(ret.m_dstScreenSize.cx <= 0)
            {
                ret.m_dstScreenSize.cx = (ret.m_dstScreenSize.cy == 1024)
                    ? 1280
                    : ret.m_dstScreenSize.cy * 4 / 3;
            }
        }
        else if(entry == L"style")
        {
            STSStyle* style = new STSStyle;
            if(!style) return(false);

            try
            {
                CString StyleName;
                CRect tmp_rect;

                StyleName = WToT(GetStr(buff)) + _T("_") + WToT(GetStr(buff));
                style->fontName = WToT(GetStr(buff));
                style->fontSize = GetFloat(buff);
                for(int i = 0; i < 4; i++) style->colors[i] = (COLORREF)GetInt(buff);
                for(int i = 0; i < 4; i++) style->alpha[i] = GetInt(buff);
                style->fontWeight = !!GetInt(buff) ? FW_BOLD : FW_NORMAL;
                style->fItalic = !!GetInt(buff);
                style->fUnderline = !!GetInt(buff);
                style->fStrikeOut = !!GetInt(buff);
                style->fBlur = !!GetInt(buff);
                style->fontScaleX = GetFloat(buff);
                style->fontScaleY = GetFloat(buff);
                style->fontSpacing = GetFloat(buff);
                style->fontAngleX = GetFloat(buff);
                style->fontAngleY = GetFloat(buff);
                style->fontAngleZ = GetFloat(buff);
                style->borderStyle = GetInt(buff);
                style->outlineWidthX = style->outlineWidthY = GetFloat(buff);
                style->shadowDepthX = style->shadowDepthY = GetFloat(buff);
                style->scrAlignment = GetInt(buff);

                tmp_rect.left = GetInt(buff);
                tmp_rect.right = GetInt(buff);
                tmp_rect.top = tmp_rect.bottom = GetInt(buff);
                style->marginRect = tmp_rect;

                style->charSet = GetInt(buff);

                style->fontScaleX = max(style->fontScaleX, 0);
                style->fontScaleY = max(style->fontScaleY, 0);
                style->fontSpacing = max(style->fontSpacing, 0);
                style->borderStyle = style->borderStyle == 1 ? 0 : style->borderStyle == 3 ? 1 : 0;
                style->outlineWidthX = max(style->outlineWidthX, 0);
                style->outlineWidthY = max(style->outlineWidthY, 0);
                style->shadowDepthX = max(style->shadowDepthX, 0);
                style->shadowDepthY = max(style->shadowDepthY, 0);

                ret.AddStyle(StyleName, style);
            }
            catch(...)
            {
                delete style;
                return(false);
            }
        }
        else if(entry == L"line")
        {
            try
            {
                CString id;
                int hh1, mm1, ss1, ms1, hh2, mm2, ss2, ms2, layer = 0;
                CString Style, Actor;
                CRect marginRect;

                if(GetStr(buff) != L"D") continue;
                id = GetStr(buff);
                layer = GetInt(buff);
                hh1 = GetInt(buff, ':');
                mm1 = GetInt(buff, ':');
                ss1 = GetInt(buff, '.');
                ms1 = GetInt(buff);
                hh2 = GetInt(buff, ':');
                mm2 = GetInt(buff, ':');
                ss2 = GetInt(buff, '.');
                ms2 = GetInt(buff);
                Style = WToT(GetStr(buff)) + _T("_") + WToT(GetStr(buff));
                Actor = WToT(GetStr(buff));
                marginRect.left = GetInt(buff);
                marginRect.right = GetInt(buff);
                marginRect.top = marginRect.bottom = GetInt(buff);

                Style.TrimLeft('*');
                if(!Style.CompareNoCase(_T("Default"))) Style = _T("Default");

                ret.Add(buff,
                    file->IsUnicode(),
                    (((hh1*60 + mm1)*60) + ss1)*1000 + ms1,
                    (((hh2*60 + mm2)*60) + ss2)*1000 + ms2,
                    Style, Actor, _T(""),
                    marginRect,
                    layer);
            }
            catch(...)
            {
                return(false);
            }
        }
        else if(entry == L"fontname")
        {
            LoadUUEFont(file);
        }
    }

    return(!ret.IsEmpty());
}

#include "USFSubtitles.h"

static bool OpenUSF(CTextFile* file, CSimpleTextSubtitle& ret, int CharSet)
{
    CString str;
    while(file->ReadString(str))
    {
        if(str.Find(_T("USFSubtitles")) >= 0)
        {
            CUSFSubtitles usf;
            if(usf.Read(file->GetFilePath()) && usf.ConvertToSTS(ret))
                return(true);

            break;
        }
    }

    return(false);
}

static CStringW MPL22SSA(CStringW str)
{
    CAtlList<CStringW> sl;
    Explode(str, sl, '|');
    POSITION pos = sl.GetHeadPosition();
    while(pos)
    {
        CStringW& s = sl.GetNext(pos);
        if(s[0] == '/') {s = L"{\\i1}" + s.Mid(1) + L"{\\i0}";}
    }
    str = Implode(sl, '\n');
    str.Replace(L"\n", L"\\N");
    return str;
}

static bool OpenMPL2(CTextFile* file, CSimpleTextSubtitle& ret, int CharSet)
{
    CStringW buff;;
    while(file->ReadString(buff))
    {
        buff.Trim();
        if(buff.IsEmpty()) continue;

        int start, end;
        int c = swscanf(buff, L"[%d][%d]", &start, &end);

        if(c == 2)
        {
            ret.Add(
                MPL22SSA(buff.Mid(buff.Find(']', buff.Find(']')+1)+1)),
                file->IsUnicode(),
                start*100, end*100);
        }
        else if(c != EOF) // might be another format
        {
            return(false);
        }
    }

    return(!ret.IsEmpty());
}

typedef bool (*STSOpenFunct)(CTextFile* file, CSimpleTextSubtitle& ret, int CharSet);

static bool OpenRealText(CTextFile* file, CSimpleTextSubtitle& ret, int CharSet);

typedef struct {STSOpenFunct open; tmode mode;} OpenFunctStruct;

static OpenFunctStruct OpenFuncts[] =
{
    OpenSubStationAlpha, TIME,
    OpenSubRipper, TIME,
    OpenOldSubRipper, TIME,
    OpenSubViewer, TIME,
    OpenMicroDVD, FRAME,
    OpenSami, TIME,
    OpenVPlayer, TIME,
    OpenXombieSub, TIME,
    OpenUSF, TIME,
    OpenMPL2, TIME,
    OpenRealText, TIME,
};

static int nOpenFuncts = countof(OpenFuncts);

//

CSimpleTextSubtitle::CSimpleTextSubtitle()
{
    m_mode = TIME;
    m_dstScreenSize = CSize(0, 0);
    m_defaultWrapStyle = 0;
    m_collisions = 0;
    m_fScaledBAS = false;
    m_encoding = CTextFile::ASCII;
    m_ePARCompensationType = EPCTDisabled;
    m_dPARCompensation = 1.0;
    m_eYCbCrMatrix = YCbCrMatrix_AUTO;
    m_eYCbCrRange = YCbCrRange_AUTO;
}

CSimpleTextSubtitle::~CSimpleTextSubtitle()
{
    Empty();
}
/*
CSimpleTextSubtitle::CSimpleTextSubtitle(CSimpleTextSubtitle& sts)
{
    *this = sts;
}

CSimpleTextSubtitle& CSimpleTextSubtitle::operator = (CSimpleTextSubtitle& sts)
{
    Empty();

    m_name = sts.m_name;
    m_mode = sts.m_mode;
    m_dstScreenSize = sts.m_dstScreenSize;
    m_defaultWrapStyle = sts.m_defaultWrapStyle;
    m_collisions = sts.m_collisions;
    m_fScaledBAS = sts.m_fScaledBAS;
    m_fSSA = sts.m_fSSA;
    m_fUsingAutoGeneratedDefaultStyle = sts.m_fUsingAutoGeneratedDefaultStyle;
    CopyStyles(sts.m_styles);
    m_segments.Copy(sts.m_segments);
    Copy(sts);

    return(*this);
}
*/

void CSimpleTextSubtitle::Copy(CSimpleTextSubtitle& sts)
{
    Empty();

    m_name = sts.m_name;
    m_mode = sts.m_mode;
    m_dstScreenSize = sts.m_dstScreenSize;
    m_defaultWrapStyle = sts.m_defaultWrapStyle;
    m_collisions = sts.m_collisions;
    m_fScaledBAS = sts.m_fScaledBAS;
    m_encoding = sts.m_encoding;
    m_fUsingAutoGeneratedDefaultStyle = sts.m_fUsingAutoGeneratedDefaultStyle;
    CopyStyles(sts.m_styles);
    m_segments.Copy(sts.m_segments);
    m_entries.Copy(sts.m_entries);
}

void CSimpleTextSubtitle::Append(CSimpleTextSubtitle& sts, int timeoff)
{
    if(timeoff < 0)
    {
        timeoff = m_entries.GetCount() > 0 ? m_entries.GetAt(m_entries.GetCount()-1).end : 0;
    }

    for(int i = 0, j = m_entries.GetCount(); i < j; i++)
    {
        if(m_entries.GetAt(i).start > timeoff)
        {
            m_entries.RemoveAt(i, j - i);
            break;
        }
    }

    CopyStyles(sts.m_styles, true);

    for(int i = 0, j = sts.m_entries.GetCount(); i < j; i++)
    {
        STSEntry stse = sts.m_entries.GetAt(i);
        stse.start += timeoff;
        stse.end += timeoff;
        stse.readorder += m_entries.GetCount();
        m_entries.Add(stse);
    }

    CreateSegments();
}

void CSTSStyleMap::Free()
{
    POSITION pos = GetStartPosition();
    while(pos)
    {
        CString key;
        STSStyle* val;
        GetNextAssoc(pos, key, val);
        delete val;
    }

    RemoveAll();
}

bool CSimpleTextSubtitle::CopyStyles(const CSTSStyleMap& styles, bool fAppend)
{
    if(!fAppend) m_styles.Free();

    POSITION pos = styles.GetStartPosition();
    while(pos)
    {
        CString key;
        STSStyle* val;
        styles.GetNextAssoc(pos, key, val);

        STSStyle* s = new STSStyle;
        if(!s) return(false);

        *s = *val;

        AddStyle(key, s);
    }

    return(true);
}

void CSimpleTextSubtitle::Empty()
{
    m_dstScreenSize = CSize(0, 0);
    m_styles.Free();
    m_segments.RemoveAll();
    m_entries.RemoveAll();
}

void CSimpleTextSubtitle::Add(CStringW str, bool fUnicode, int start, int end, 
    CString style, const CString& actor, const CString& effect, const CRect& marginRect, int layer, int readorder)
{
    XY_LOG_INFO(start<<_T(" ")<<end<<_T(" ")<<str.GetString()<<_T(" style:")<<style.GetString()
        <<_T(" Unicode:")<<fUnicode
        <<_T(" actor:")<<actor.GetString()<<_T(" effect:")<<effect.GetString()
        <<_T(" (l:")<<marginRect.left<<_T(",t:")<<marginRect.top<<_T(",r:")<<marginRect.right<<_T(",b:")<<marginRect.bottom
        <<_T(" layer:")<<layer<<_T(" readorder:")<<readorder
        <<_T(" entries:")<<m_entries.GetCount()<<_T(" seg:")<<m_segments.GetCount());

    if(start > end || str.Trim().IsEmpty() ) return;

    str.Remove('\r');
    str.Replace(L"\n", L"\\N");
    if(style.IsEmpty()) style = g_default_style;
    else if(style!=g_default_style)
    {
        style.TrimLeft('*');
    }

    STSEntry sub;
    sub.str = str;
    sub.fUnicode = fUnicode;
    sub.style = style;
    sub.actor = actor;
    sub.effect = effect;
    sub.marginRect = marginRect;
    sub.layer = layer;
    sub.start = start;
    sub.end = end;
    sub.readorder = readorder < 0 ? m_entries.GetCount() : readorder;
    int n = m_entries.Add(sub);

    int len = m_segments.GetCount();

    if(len == 0)
    {
        STSSegment stss(start, end);
        stss.subs.Add(n);
        m_segments.Add(stss);
    }
    else if(end <= m_segments[0].start)
    {
        STSSegment stss(start, end);
        stss.subs.Add(n);
        m_segments.InsertAt(0, stss);
    }
    else if(start >= m_segments[len-1].end)
    {
        STSSegment stss(start, end);
        stss.subs.Add(n);
        m_segments.Add(stss);
    }
    else
    {
        if(start < m_segments[0].start)
        {
            STSSegment stss(start, m_segments[0].start);
            stss.subs.Add(n);
            start = m_segments[0].start;
            m_segments.InsertAt(0, stss);
        }

        for(size_t i = 0; i < m_segments.GetCount(); i++)
        {
            STSSegment& s = m_segments[i];

            if(start >= s.end)
            {
                continue;
            }
            else if(end <= s.start)
            {
                break;
            }
            else if(s.start < start && start < s.end)
            {
                STSSegment stss(s.start, start);
                stss.subs.Copy(s.subs);
                s.start = start;
                m_segments.InsertAt(i, stss);
                continue;
            }
            if(start <= s.start && s.end <= end)
            {
                for(int j = 0, k = s.subs.GetCount(); j <= k; j++)
                {
                    if(j == k || sub.readorder < m_entries.GetAt(s.subs[j]).readorder)
                    {
                        s.subs.InsertAt(j, n);
                        break;
                    }
                }
            }
            else if(s.start < end && end < s.end)
            {
                STSSegment stss(s.start, end);
                stss.subs.Copy(s.subs);
                for(int j = 0, k = s.subs.GetCount(); j <= k; j++)
                {
                    if(j == k || sub.readorder < m_entries.GetAt(stss.subs[j]).readorder)
                    {
                        stss.subs.InsertAt(j, n);
                        break;
                    }
                }
                s.start = end;
                m_segments.InsertAt(i, stss);
            }            
        }

        if(end > m_segments[m_segments.GetCount()-1].end)
        {
            STSSegment stss(m_segments[m_segments.GetCount()-1].end, end);
            stss.subs.Add(n);
            m_segments.Add(stss);
        }
    }
/*
    str.Remove('\r');
    str.Replace(L"\n", L"\\N");
    if(style.IsEmpty()) style = _T("Default");

    int j = m_segments.GetCount();
    for(int i = j-1; i >= 0; i--)
    {
        if(m_segments[i].end <= start)
        {
            break;
        }
        else if(m_segments[i].start >= start)
        {
            m_segments.SetCount(m_segments.GetCount()-1);
            j--;
        }
        else if(m_segments[i].end > start)
        {
            if(i < j-1) m_segments.RemoveAt(i+1, j-i-1);
            m_segments[i].end = start;
            break;
        }
    }

    if(m_segments.GetCount() == 0 && j > 0)
        CSTSArray::RemoveAll();

    STSSegment stss(start, end);
    int len = m_entries.GetCount();
    stss.subs.Add(len);
    m_segments.Add(stss);

    STSEntry sub;
    sub.str = str;
    sub.fUnicode = fUnicode;
    sub.style = style;
    sub.actor = actor;
    sub.effect = effect;
    sub.marginRect = marginRect;
    sub.layer = layer;
    sub.start = start;
    sub.end = end;
    sub.readorder = m_entries.GetCount();
    CSTSArray::Add(sub);
*/
}

void CSimpleTextSubtitle::AddSTSEntryOnly( CStringW str, bool fUnicode, int start, int end, CString style /*= _T("Default")*/, const CString& actor /*= _T("")*/, const CString& effect /*= _T("")*/, const CRect& marginRect /*= CRect(0,0,0,0)*/, int layer /*= 0*/, int readorder /*= -1*/ )
{
	if (start > end) return;
	bool all_whitespace = true;
	for (int i = 0; i < str.GetLength(); i++) {
		if (!CStringW::StrTraits::IsSpace(str[i])) {
			all_whitespace = false;
			break;
		}
	}
	if (all_whitespace) return;

    str.Remove('\r');
    str.Replace(L"\n", L"\\N");
    if(style.IsEmpty()) style = _T("Default");
    style.TrimLeft('*');

    STSEntry sub;
    sub.str = str;
    sub.fUnicode = fUnicode;
    sub.style = style;
    sub.actor = actor;
    sub.effect = effect;
    sub.marginRect = marginRect;
    sub.layer = layer;
    sub.start = start;
    sub.end = end;
    sub.readorder = readorder < 0 ? m_entries.GetCount() : readorder;
    m_entries.Add(sub);
    return;
}

STSStyle* CSimpleTextSubtitle::CreateDefaultStyle(int CharSet)
{
    STSStyle* ret = NULL;

    if(!m_styles.Lookup(g_default_style, ret))
    {
        STSStyle* style = new STSStyle();
        style->charSet = CharSet;
        AddStyle(g_default_style, style);
        m_styles.Lookup(g_default_style, ret);

        m_fUsingAutoGeneratedDefaultStyle = true;
    }
    else
    {
        m_fUsingAutoGeneratedDefaultStyle = false;
    }

    return ret;
}

void CSimpleTextSubtitle::ChangeUnknownStylesToDefault()
{
    CAtlMap<CString, STSStyle*, CStringElementTraits<CString> > unknown;
    bool fReport = true;

    for(size_t i = 0; i < m_entries.GetCount(); i++)
    {
        STSEntry& stse = m_entries.GetAt(i);

        STSStyle* val;
        if(!m_styles.Lookup(stse.style, val))
        {
            if(!unknown.Lookup(stse.style, val))
            {
                if(fReport)
                {
                    CString msg;
                    msg.Format(_T("Unknown style found: \"%s\", changed to \"Default\"!\n\nPress Cancel to ignore further warnings."), stse.style);
                    if(MessageBox(NULL, msg, _T("Warning"), MB_OKCANCEL|MB_ICONWARNING) != IDOK) fReport = false;
                }

                unknown[stse.style] = NULL;
            }

            stse.style = g_default_style;
        }
    }
}

void CSimpleTextSubtitle::AddStyle(CString name, STSStyle* style)
{
    int i, j;

    if(name.IsEmpty()) name = g_default_style;

    STSStyle* val;
    if(m_styles.Lookup(name, val))
    {
        if(*val == *style)
        {
            delete style;
            return;
        }
        const CString& name_str = name;

        int len = name_str.GetLength();

        for(i = len; i > 0 && _istdigit(name_str[i-1]); i--);

        int idx = 1;

        CString name2 = name_str;

        if(i < len && _stscanf(name_str.Right(len-i), _T("%d"), &idx) == 1)
        {
            name2 = name_str.Left(i);
        }

        idx++;

        CString name3;
        CString name3_str;
        do
        {
            name3_str.Format(_T("%s%d"), name2, idx);
            name3 = name3_str;
            idx++;
        }
        while(m_styles.Lookup(name3));

        m_styles.RemoveKey(name);
        m_styles[name3] = val;

        for(i = 0, j = m_entries.GetCount(); i < j; i++)
        {
            STSEntry& stse = m_entries.GetAt(i);
            if(stse.style == name) stse.style = name3;
        }
    }

    m_styles[name] = style;
}

bool CSimpleTextSubtitle::SetDefaultStyle(STSStyle& s)
{
    DbgLog((LOG_TRACE, 3, "%s(%d): %s", __FILE__, __LINE__, __FUNCTION__));
    DbgLog((LOG_TRACE, 3, "\tm_styles count:%d", m_styles.GetCount()));
    STSStyle* val;
    if(!m_styles.Lookup(g_default_style, val)) return false;
    DbgLog((LOG_TRACE, 3, "\tm_styles Lookup Default succeed"));

#ifdef DEBUG
    for(POSITION pos=m_styles.GetStartPosition(); pos!=NULL;)
    {
        DbgLog((LOG_TRACE, 3, _T("\tm_styles[%s]"), (LPCTSTR)m_styles.GetNextKey(pos)));
    }
#endif

    *val = s;
    m_fUsingAutoGeneratedDefaultStyle = false;
    return true;
}

bool CSimpleTextSubtitle::GetDefaultStyle(STSStyle& s)
{
    STSStyle* val;
    if(!m_styles.Lookup(g_default_style, val)) return false;
    s = *val;
    return true;
}

void CSimpleTextSubtitle::ConvertToTimeBased(double fps)
{
    if(m_mode == TIME) return;

    for(int i = 0, j = m_entries.GetCount(); i < j; i++)
    {
        STSEntry& stse = m_entries[i];
        stse.start = int(1.0 * stse.start * 1000 / fps + 0.5);
        stse.end = int(1.0 * stse.end * 1000 / fps + 0.5);
    }

    m_mode = TIME;

    CreateSegments();
}

void CSimpleTextSubtitle::ConvertToFrameBased(double fps)
{
    if(m_mode == FRAME) return;

    for(int i = 0, j = m_entries.GetCount(); i < j; i++)
    {
        STSEntry& stse = m_entries[i];
        stse.start = int(1.0 * stse.start * fps / 1000 + 0.5);
        stse.end = int(1.0 * stse.end * fps / 1000 + 0.5);
    }

    m_mode = FRAME;

    CreateSegments();
}

int CSimpleTextSubtitle::SearchSub(int t, double fps)
{
    int i = 0, j = m_entries.GetCount() - 1, ret = -1;

    if(j >= 0 && t >= TranslateStart(j, fps))
    {
        return(j);
    }

    while(i < j)
    {
        int mid = (i + j) >> 1;

        int midt = TranslateStart(mid, fps);

        if(t == midt)
        {
            while(mid > 0 && t == TranslateStart(mid-1, fps)) mid--;
            ret = mid;
            break;
        }
        else if(t < midt)
        {
            ret = -1;
            if(j == mid) mid--;
            j = mid;
        }
        else if(t > midt)
        {
            ret = mid;
            if(i == mid) mid++;
            i = mid;
        }
    }

    return(ret);
}

const STSSegment* CSimpleTextSubtitle::SearchSubs(int t, double fps, /*[out]*/ int* iSegment, int* nSegments)
{
    int segmentsCount = m_segments.GetCount();
    int i = 0, j = segmentsCount - 1;

    if(nSegments) *nSegments = segmentsCount;
    if(segmentsCount<=0)
    {
        if(iSegment!=NULL)
            *iSegment = 0;
        return NULL;
    }

    if(t >= TranslateSegmentEnd(j, fps))
    {
        i = j;
        j++;
    }
    if(t < TranslateSegmentEnd(i, fps))
    {
        j = i;
        i--;
    }

    while(i < j-1)
    {
        int mid = (i + j) >> 1;

        int midt = TranslateSegmentEnd(mid, fps);

        if(t < midt)
            j=mid;
        else
            i=mid;
    }
    if(iSegment!=NULL)
        *iSegment = j;
    if(j<segmentsCount)
    {
        return &m_segments[j];
    }
    return(NULL);
}

STSSegment* CSimpleTextSubtitle::SearchSubs2(int t, double fps, /*[out]*/ int* iSegment, int* nSegments)
{
    int segmentsCount = m_segments.GetCount();
    int i = 0, j = segmentsCount - 1;

    if(iSegment) *iSegment = -1;
    if(nSegments) *nSegments = segmentsCount;

    if(segmentsCount<=0)
    {
        if(iSegment) *iSegment = 0;
        return NULL;
    }

    if(t >= TranslateSegmentEnd(j, fps))
    {
        i = j;
        j++;
    }
    if(t < TranslateSegmentEnd(i, fps))
    {
        j = i;
        i--;
    }

    while(i < j-1)
    {
        int mid = (i + j) >> 1;

        int midt = TranslateSegmentEnd(mid, fps);

        if(t < midt)
            j=mid;
        else
            i=mid;
    }
    if(j<segmentsCount && t>=TranslateSegmentStart(j, fps))
    {
        if(iSegment) *iSegment = j;
        return &m_segments[j];
    }
    return(NULL);
}



int CSimpleTextSubtitle::TranslateStart(int i, double fps)
{
    return(i < 0 || m_entries.GetCount() <= i ? -1 :
        m_mode == TIME ? m_entries.GetAt(i).start :
        m_mode == FRAME ? (int)(m_entries.GetAt(i).start*1000/fps) :
        0);
}

int CSimpleTextSubtitle::TranslateEnd(int i, double fps)
{
    return(i < 0 || m_entries.GetCount() <= i ? -1 :
        m_mode == TIME ? m_entries.GetAt(i).end :
        m_mode == FRAME ? (int)(m_entries.GetAt(i).end*1000/fps) :
        0);
}

int CSimpleTextSubtitle::TranslateSegmentStart(int i, double fps)
{
    return(i < 0 || m_segments.GetCount() <= i ? -1 :
        m_mode == TIME ? m_segments[i].start :
        m_mode == FRAME ? (int)(m_segments[i].start*1000/fps) :
        0);
}

int CSimpleTextSubtitle::TranslateSegmentEnd(int i, double fps)
{
    return(i < 0 || m_segments.GetCount() <= i ? -1 :
        m_mode == TIME ? m_segments[i].end :
        m_mode == FRAME ? (int)(m_segments[i].end*1000/fps) :
        0);
}

void CSimpleTextSubtitle::TranslateSegmentStartEnd(int i, double fps, /*out*/int& start, /*out*/int& end)
{
    if(i < 0 || m_segments.GetCount() <= i)
    {
        start=-1;
        end=-1;
    }
    else
    {
        if(m_mode == TIME)
        {
            start = m_segments[i].start;
            end = m_segments[i].end;
        }
        else //m_mode == FRAME
        {
            start = (int)(m_segments[i].start*1000/fps);
            end = (int)(m_segments[i].end*1000/fps);
        }
    }
}

STSStyle* CSimpleTextSubtitle::GetStyle(int i)
{    
    STSStyle* style = NULL;
    m_styles.Lookup(m_entries.GetAt(i).style, style);

    STSStyle* defstyle = NULL;
    m_styles.Lookup(g_default_style, defstyle);

    if(!style)
    {
        style = defstyle;
    }

    ASSERT(style);

    return style;
}

bool CSimpleTextSubtitle::GetStyle(int i, STSStyle* const stss)
{
    STSStyle* style = NULL;
    m_styles.Lookup(m_entries.GetAt(i).style, style);

    STSStyle* defstyle = NULL;
    m_styles.Lookup(g_default_style, defstyle);

    if(!style)
    {
        if(!defstyle)
        {
            defstyle = CreateDefaultStyle(DEFAULT_CHARSET);
        }

        style = defstyle;
    }

    if(!style) {ASSERT(0); return false;}

    *stss = *style;
    if(stss->relativeTo == 2 && defstyle)
        stss->relativeTo = defstyle->relativeTo;

    return true;
}

int CSimpleTextSubtitle::GetCharSet(int i)
{
    STSStyle* style = GetStyle(i);
    return style!=NULL ? style->charSet : -1;
}

bool CSimpleTextSubtitle::IsEntryUnicode(int i)
{
    return(m_entries.GetAt(i).fUnicode);
}

void CSimpleTextSubtitle::ConvertUnicode(int i, bool fUnicode)
{
    STSEntry& stse = m_entries.GetAt(i);

    if(stse.fUnicode ^ fUnicode)
    {
        int CharSet = GetCharSet(i);

        stse.str = fUnicode
            ? MBCSSSAToUnicode(stse.str, CharSet)
            : UnicodeSSAToMBCS(stse.str, CharSet);

        stse.fUnicode = fUnicode;
    }
}

CStringA CSimpleTextSubtitle::GetStrA(int i, bool fSSA)
{
    return(WToA(GetStrWA(i, fSSA)));
}

CStringW CSimpleTextSubtitle::GetStrW(int i, bool fSSA)
{
    bool fUnicode = IsEntryUnicode(i);
    int CharSet = GetCharSet(i);

    CStringW str = m_entries.GetAt(i).str;

    if(!fUnicode)
        str = MBCSSSAToUnicode(str, CharSet);

    if(!fSSA)
        str = RemoveSSATags(str, fUnicode, CharSet);

    return(str);
}

CStringW CSimpleTextSubtitle::GetStrWA(int i, bool fSSA)
{
    bool fUnicode = IsEntryUnicode(i);
    int CharSet = GetCharSet(i);

    CStringW str = m_entries.GetAt(i).str;

    if(fUnicode)
        str = UnicodeSSAToMBCS(str, CharSet);

    if(!fSSA)
        str = RemoveSSATags(str, fUnicode, CharSet);

    return(str);
}

void CSimpleTextSubtitle::SetStr(int i, CStringA str, bool fUnicode)
{
    SetStr(i, AToW(str), false);
}

void CSimpleTextSubtitle::SetStr(int i, CStringW str, bool fUnicode)
{
    STSEntry& stse = m_entries.GetAt(i);

    str.Replace(L"\n", L"\\N");

    if(stse.fUnicode && !fUnicode) stse.str = MBCSSSAToUnicode(str, GetCharSet(i));
    else if(!stse.fUnicode && fUnicode) stse.str = UnicodeSSAToMBCS(str, GetCharSet(i));
    else stse.str = str;
}

static int comp1(const void* a, const void* b)
{
    int ret = ((STSEntry*)a)->start - ((STSEntry*)b)->start;
    if(ret == 0) ret = ((STSEntry*)a)->layer - ((STSEntry*)b)->layer;
    if(ret == 0) ret = ((STSEntry*)a)->readorder - ((STSEntry*)b)->readorder;
    return(ret);
}

static int comp2(const void* a, const void* b)
{
    return(((STSEntry*)a)->readorder - ((STSEntry*)b)->readorder);
}

void CSimpleTextSubtitle::Sort(bool fRestoreReadorder)
{
    qsort(m_entries.GetData(), m_entries.GetCount(), sizeof(STSEntry), !fRestoreReadorder ? comp1 : comp2);
    CreateSegments();
}

static int intcomp(const void* i1, const void* i2)
{
    return(*((int*)i1) - *((int*)i2));
}

void CSimpleTextSubtitle::CreateSegments()
{
    m_segments.RemoveAll();

    if(m_entries.GetCount()>0)
    {
        size_t start, mid, end;
        CAtlArray<STSSegment> tempSegments;//if add to m_segments directly, then remove empty entities can be a
                                           //complex operation when having large segmentCount and lots of empty entities
        std::vector<int> breakpoints(2*m_entries.GetCount());
        for(size_t i = 0; i < m_entries.GetCount(); i++)
        {
            STSEntry& stse = m_entries.GetAt(i);
            breakpoints[2*i]=stse.start;
            breakpoints[2*i+1]=stse.end;
        }

        std::sort(breakpoints.begin(), breakpoints.end());

        int ptr = 1, prev = breakpoints[0];
        for(size_t i = breakpoints.size()-1; i > 0; i--, ptr++)
        {
            if(breakpoints[ptr] != prev)
            {
                tempSegments.Add(STSSegment(prev, breakpoints[ptr]));
                prev = breakpoints[ptr];
            }
        }

        size_t segmentCount = tempSegments.GetCount();

        for(size_t i = 0; i < m_entries.GetCount(); i++)
        {
            STSEntry& stse = m_entries.GetAt(i);
            start = 0;
            end = segmentCount;
            while(start<end)
            {
                mid = (start+end)>>1;
                if(tempSegments[mid].start < stse.start)
                {
                    start = mid+1;
                }
                else
                {
                    end = mid;
                }
            }
            for(; start < tempSegments.GetCount() && tempSegments[start].end <= stse.end; start++)
                tempSegments[start].subs.Add(i);
        }
        for(size_t i = 0; i < segmentCount; i++)
            if(tempSegments[i].subs.GetCount()>0)
                m_segments.Add(tempSegments[i]);
    }
    OnChanged();
/*
    for(i = 0, j = m_segments.GetCount(); i < j; i++)
    {
        STSSegment& stss = m_segments[i];

        TRACE(_T("%d - %d"), stss.start, stss.end);

        for(int k = 0, l = stss.subs.GetCount(); k < l; k++)
        {
            TRACE(_T(", %d"), stss.subs[k]);
        }

        TRACE(_T("\n"));
    }
*/
}

bool CSimpleTextSubtitle::Open(CString fn, int CharSet, CString name)
{
    Empty();

    CWebTextFile f(CTextFile::UTF8);
    if(!f.Open(fn)) return(false);

    fn.Replace('\\', '/');
    if(name.IsEmpty())
    {
        name = fn.Left(fn.ReverseFind('.'));
        name = name.Mid(name.ReverseFind('/')+1);
        name = name.Mid(name.ReverseFind('.')+1);
    }

    return(Open(&f, CharSet, name));
}

static int CountLines(CTextFile* f, ULONGLONG from, ULONGLONG to)
{
    int n = 0;
    CString s;
    f->Seek(from, 0);
    while(f->ReadString(s) && f->GetPosition() < to) n++;
    return(n);
}

bool CSimpleTextSubtitle::Open(CTextFile* f, int CharSet, CString name)
{
    Empty();

    ULONGLONG pos = f->GetPosition();

    for(int i = 0; i < nOpenFuncts; i++)
    {
         const TCHAR* func_name[]={
             TEXT("OpenSubStationAlpha"),
             TEXT("OpenSubRipper"),
             TEXT("OpenOldSubRipper"),
             TEXT("OpenSubViewer"),
             TEXT("OpenMicroDVD"),
             TEXT("OpenSami"),
             TEXT("OpenVPlayer"),
             TEXT("OpenXombieSub"),
             TEXT("OpenUSF"),
             TEXT("OpenMPL2"),
             TEXT("OpenRealText")};
         CAutoTiming t(func_name[i],0);

        if(!OpenFuncts[i].open(f, *this, CharSet) /*|| !GetCount()*/)
        {
            if(m_entries.GetCount() > 0)
            {
                int n = CountLines(f, pos, f->GetPosition());
                CString s;
                s.Format(_T("Syntax error at line %d!\t"), n+1);
                AfxMessageBox(s, MB_OK|MB_ICONERROR);
                Empty();
                break;
            }

            f->Seek(pos, 0);
            Empty();
            continue;
        }

        m_name = name;
        m_mode = OpenFuncts[i].mode;
        m_encoding = f->GetEncoding();
        m_path = f->GetFilePath();

        CWebTextFile f2(CTextFile::UTF8);
        if(f2.Open(f->GetFilePath() + _T(".style")))
            OpenSubStationAlpha(&f2, *this, CharSet);

        //      Sort();
        CreateSegments();

        CreateDefaultStyle(CharSet);

        ChangeUnknownStylesToDefault();

        if(m_dstScreenSize == CSize(0, 0)) m_dstScreenSize = CSize(384, 288);

        return(true);
    }

    return(false);
}

bool CSimpleTextSubtitle::Open(BYTE* data, int len, int CharSet, CString name)
{
    TCHAR path[MAX_PATH];
    if(!GetTempPath(MAX_PATH, path)) return(false);

    TCHAR fn[MAX_PATH];
    if(!GetTempFileName(path, _T("vs"), 0, fn)) return(false);

    FILE* tmp = _tfopen(fn, _T("wb"));
    if(!tmp) return(false);

    int i = 0;
    for(; i <= (len-1024); i += 1024) fwrite(&data[i], 1024, 1, tmp);
    if(len > i) fwrite(&data[i], len - i, 1, tmp);

    fclose(tmp);

    bool fRet = Open(fn, CharSet, name);

    _tremove(fn);

    return(fRet);
}

bool CSimpleTextSubtitle::SaveAs(CString fn, exttype et, double fps, CTextFile::enc e)
{
    if(fn.Mid(fn.ReverseFind('.')+1).CompareNoCase(G_EXTTYPESTR[et]))
    {
        if(fn[fn.GetLength()-1] != '.') fn += _T(".");
        fn += G_EXTTYPESTR[et];
    }

    CTextFile f;
    if(!f.Save(fn, e))
        return(false);

    if(et == EXTSMI)
    {
        CString str;

        str += _T("<SAMI>\n<HEAD>\n");
        str += _T("<STYLE TYPE=\"text/css\">\n");
        str += _T("<!--\n");
        str += _T("P {margin-left: 16pt; margin-right: 16pt; margin-bottom: 16pt; margin-top: 16pt;\n");
        str += _T("   text-align: center; font-size: 18pt; font-family: arial; font-weight: bold; color: #f0f0f0;}\n");
        str += _T(".UNKNOWNCC {Name:Unknown; lang:en-US; SAMIType:CC;}\n");
        str += _T("-->\n");
        str += _T("</STYLE>\n");
        str += _T("</HEAD>\n");
        str += _T("\n");
        str += _T("<BODY>\n");

        f.WriteString(str);
    }
    else if(et == EXTSSA || et == EXTASS)
    {
        CString str;

        str  = _T("[Script Info]\n");
        str += (et == EXTSSA) ? _T("; This is a Sub Station Alpha v4 script.\n") : _T("; This is an Advanced Sub Station Alpha v4+ script.\n");
        str += _T("; For Sub Station Alpha info and downloads,\n");
        str += _T("; go to http://www.eswat.demon.co.uk/\n");
        str += _T("; or email kotus@eswat.demon.co.uk\n");
        str += _T("; \n");
        if(et == EXTASS)
        {
            str += _T("; Advanced Sub Station Alpha script format developed by #Anime-Fansubs@EfNET\n");
            str += _T("; http://www.anime-fansubs.org\n");
            str += _T("; \n");
            str += _T("; For additional info and downloads go to http://gabest.org/\n");
            str += _T("; or email gabest@freemail.hu\n");
            str += _T("; \n");
        }
        str += _T("; Note: This file was saved by Subresync.\n");
        str += _T("; \n");
        str += (et == EXTSSA) ? _T("ScriptType: v4.00\n") : _T("ScriptType: v4.00+\n");
        str += (m_collisions == 0) ? _T("Collisions: Normal\n") : _T("Collisions: Reverse\n");
        if(et == EXTASS && m_fScaledBAS) str += _T("ScaledBorderAndShadow: Yes\n");
        str += _T("PlayResX: %d\n");
        str += _T("PlayResY: %d\n");
        str += _T("Timer: 100.0000\n");
        str += _T("\n");
        str += (et == EXTSSA)
            ? _T("[V4 Styles]\nFormat: Name, Fontname, Fontsize, PrimaryColour, SecondaryColour, TertiaryColour, BackColour, Bold, Italic, BorderStyle, Outline, Shadow, Alignment, MarginL, MarginR, MarginV, AlphaLevel, Encoding\n")
            : _T("[V4+ Styles]\nFormat: Name, Fontname, Fontsize, PrimaryColour, SecondaryColour, OutlineColour, BackColour, Bold, Italic, Underline, StrikeOut, ScaleX, ScaleY, Spacing, Angle, BorderStyle, Outline, Shadow, Alignment, MarginL, MarginR, MarginV, Encoding\n");

        CString str2;
        str2.Format(str, m_dstScreenSize.cx, m_dstScreenSize.cy);
        f.WriteString(str2);

        str  = (et == EXTSSA)
            ? _T("Style: %s,%s,%d,&H%06x,&H%06x,&H%06x,&H%06x,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d\n")
            : _T("Style: %s,%s,%d,&H%08x,&H%08x,&H%08x,&H%08x,%d,%d,%d,%d,%d,%d,%d,%.2f,%d,%d,%d,%d,%d,%d,%d,%d\n");

        POSITION pos = m_styles.GetStartPosition();
        while(pos)
        {
            CString key;
            STSStyle* s;
            m_styles.GetNextAssoc(pos, key, s);

            if(et == EXTSSA)
            {
                CString str2;
                str2.Format(str, key,
                    s->fontName, (int)s->fontSize,
                    s->colors[0]&0xffffff,
                    s->colors[1]&0xffffff,
                    s->colors[2]&0xffffff,
                    s->colors[3]&0xffffff,
                    s->fontWeight > FW_NORMAL ? -1 : 0, s->fItalic ? -1 : 0,
                    s->borderStyle == 0 ? 1 : s->borderStyle == 1 ? 3 : 0,
                    (int)s->outlineWidthY, (int)s->shadowDepthY,
                    s->scrAlignment <= 3 ? s->scrAlignment : s->scrAlignment <= 6 ? ((s->scrAlignment-3)|8) : s->scrAlignment <= 9 ? ((s->scrAlignment-6)|4) : 2,
                    s->marginRect.get().left, s->marginRect.get().right, (s->marginRect.get().top + s->marginRect.get().bottom) / 2,
                    s->alpha[0],
                    s->charSet);
                f.WriteString(str2);
            }
            else
            {
                CString str2;
                str2.Format(str, key,
                    s->fontName, (int)s->fontSize,
                    (s->colors[0]&0xffffff) | (s->alpha[0]<<24),
                    (s->colors[1]&0xffffff) | (s->alpha[1]<<24),
                    (s->colors[2]&0xffffff) | (s->alpha[2]<<24),
                    (s->colors[3]&0xffffff) | (s->alpha[3]<<24),
                    s->fontWeight > FW_NORMAL ? -1 : 0,
                    s->fItalic ? -1 : 0, s->fUnderline ? -1 : 0, s->fStrikeOut ? -1 : 0,
                    (int)s->fontScaleX, (int)s->fontScaleY,
                    (int)s->fontSpacing, (float)s->fontAngleZ,
                    s->borderStyle == 0 ? 1 : s->borderStyle == 1 ? 3 : 0,
                    (int)s->outlineWidthY, (int)s->shadowDepthY,
                    s->scrAlignment,
                    s->marginRect.get().left, s->marginRect.get().right, (s->marginRect.get().top + s->marginRect.get().bottom) / 2,
                    s->charSet);
                f.WriteString(str2);
            }
        }

        if(m_entries.GetCount() > 0)
        {
            str  = _T("\n");
            str += _T("[Events]\n");
            str += (et == EXTSSA)
                ? _T("Format: Marked, Start, End, Style, Name, MarginL, MarginR, MarginV, Effect, Text\n")
                : _T("Format: Layer, Start, End, Style, Actor, MarginL, MarginR, MarginV, Effect, Text\n");
            f.WriteString(str);
        }
    }

    CStringW fmt =
        et == EXTSRT ? L"%d\n%02d:%02d:%02d,%03d --> %02d:%02d:%02d,%03d\n%s\n\n" :
        et == EXTSUB ? L"{%d}{%d}%s\n" :
        et == EXTSMI ? L"<SYNC Start=%d><P Class=UNKNOWNCC>\n%s\n<SYNC Start=%d><P Class=UNKNOWNCC>&nbsp;\n" :
        et == EXTPSB ? L"{%d:%02d:%02d}{%d:%02d:%02d}%s\n" :
        et == EXTSSA ? L"Dialogue: Marked=0,%d:%02d:%02d.%02d,%d:%02d:%02d.%02d,%s,%s,%04d,%04d,%04d,%s,%s\n" :
        et == EXTASS ? L"Dialogue: %d,%d:%02d:%02d.%02d,%d:%02d:%02d.%02d,%s,%s,%04d,%04d,%04d,%s,%s\n" :
                       L"";
//  Sort(true);

    for(int i = 0, j = m_entries.GetCount(), k = 0; i < j; i++)
    {
        STSEntry& stse = m_entries.GetAt(i);

        int t1 = TranslateStart(i, fps);
        if(t1 < 0) {k++; continue;}

        int t2 = TranslateEnd(i, fps);

        int hh1 = (t1/60/60/1000);
        int mm1 = (t1/60/1000)%60;
        int ss1 = (t1/1000)%60;
        int ms1 = (t1)%1000;
        int hh2 = (t2/60/60/1000);
        int mm2 = (t2/60/1000)%60;
        int ss2 = (t2/1000)%60;
        int ms2 = (t2)%1000;

        CStringW str = f.IsUnicode()
            ? GetStrW(i, et == EXTSSA || et == EXTASS)
            : GetStrWA(i, et == EXTSSA || et == EXTASS);

        CStringW str2;

        if(et == EXTSRT)
        {
            str2.Format(fmt, i-k+1, hh1, mm1, ss1, ms1, hh2, mm2, ss2, ms2, str);
        }
        else if(et == EXTSUB)
        {
            str.Replace('\n', '|');
            str2.Format(fmt, int(t1*fps/1000), int(t2*fps/1000), str);
        }
        else if(et == EXTSMI)
        {
            str.Replace(L"\n", L"<br>");
            str2.Format(fmt, t1, str, t2);
        }
        else if(et == EXTPSB)
        {
            str.Replace('\n', '|');
            str2.Format(fmt, hh1, mm1, ss1, hh2, mm2, ss2, str);
        }
        else if(et == EXTSSA)
        {
            str.Replace(L"\n", L"\\N");
            str2.Format(fmt,
                hh1, mm1, ss1, ms1/10,
                hh2, mm2, ss2, ms2/10,
                TToW(stse.style), TToW(stse.actor),
                stse.marginRect.left, stse.marginRect.right, (stse.marginRect.top + stse.marginRect.bottom) / 2,
                TToW(stse.effect), str);
        }
        else if(et == EXTASS)
        {
            str.Replace(L"\n", L"\\N");
            str2.Format(fmt,
                stse.layer,
                hh1, mm1, ss1, ms1/10,
                hh2, mm2, ss2, ms2/10,
                TToW(stse.style), TToW(stse.actor),
                stse.marginRect.left, stse.marginRect.right, (stse.marginRect.top + stse.marginRect.bottom) / 2,
                TToW(stse.effect), str);
        }

        f.WriteString(str2);
    }

//  Sort();

    if(et == EXTSMI)
    {
        f.WriteString(_T("</BODY>\n</SAMI>\n"));
    }

    STSStyle* s;
    if(!m_fUsingAutoGeneratedDefaultStyle && m_styles.Lookup(g_default_style, s) && et != EXTSSA && et != EXTASS)
    {
        CTextFile f;
        if(!f.Save(fn + _T(".style"), e))
            return(false);

        CString str, str2;

        str += _T("ScriptType: v4.00+\n");
        str += _T("PlayResX: %d\n");
        str += _T("PlayResY: %d\n");
        str += _T("\n");
        str += _T("[V4+ Styles]\nFormat: Name, Fontname, Fontsize, PrimaryColour, SecondaryColour, OutlineColour, BackColour, Bold, Italic, Underline, StrikeOut, ScaleX, ScaleY, Spacing, Angle, BorderStyle, Outline, Shadow, Alignment, MarginL, MarginR, MarginV, Encoding\n");
        str2.Format(str, m_dstScreenSize.cx, m_dstScreenSize.cy);
        f.WriteString(str2);

        str  = _T("Style: Default,%s,%d,&H%08x,&H%08x,&H%08x,&H%08x,%d,%d,%d,%d,%d,%d,%d,%.2f,%d,%d,%d,%d,%d,%d,%d,%d\n");
        str2.Format(str,
            s->fontName, (int)s->fontSize,
            (s->colors[0]&0xffffff) | (s->alpha[0]<<24),
            (s->colors[1]&0xffffff) | (s->alpha[1]<<24),
            (s->colors[2]&0xffffff) | (s->alpha[2]<<24),
            (s->colors[3]&0xffffff) | (s->alpha[3]<<24),
            s->fontWeight > FW_NORMAL ? -1 : 0,
            s->fItalic ? -1 : 0, s->fUnderline ? -1 : 0, s->fStrikeOut ? -1 : 0,
            (int)s->fontScaleX, (int)s->fontScaleY,
            (int)s->fontSpacing, (float)s->fontAngleZ,
            s->borderStyle == 0 ? 1 : s->borderStyle == 1 ? 3 : 0,
            (int)s->outlineWidthY, (int)s->shadowDepthY,
            s->scrAlignment,
            s->marginRect.get().left, s->marginRect.get().right, (s->marginRect.get().top + s->marginRect.get().bottom) / 2,
            s->charSet);
        f.WriteString(str2);
    }

    return(true);
}

bool CSimpleTextSubtitle::IsEmpty()
{
    return m_entries.IsEmpty();
}

void CSimpleTextSubtitle::RemoveAllEntries()
{
    m_entries.RemoveAll();
}

////////////////////////////////////////////////////////////////////

bool STSStyleBase::operator==( const STSStyleBase& s ) const
{
    return charSet == s.charSet
        && fontName == s.fontName
        && fontSize == s.fontSize
        && fontWeight == s.fontWeight
        && fItalic == s.fItalic
        && fUnderline == s.fUnderline
        && fStrikeOut == s.fStrikeOut;
}

STSStyle::STSStyle()
{
    SetDefault();
}

void STSStyle::SetDefault()
{
    marginRect = CRect(20, 20, 20, 20);
    scrAlignment = 2;
    borderStyle = 0;
    outlineWidthX = outlineWidthY = 2;
    shadowDepthX = shadowDepthY = 3;
    colors[0] = 0x00ffffff;
    colors[1] = 0x0000ffff;
    colors[2] = 0x00000000;
    colors[3] = 0x00000000;
    alpha[0] = 0x00;
    alpha[1] = 0x00;
    alpha[2] = 0x00;
    alpha[3] = 0x80;
    charSet = DEFAULT_CHARSET;
    fontName = _T("Arial");
    fontSize = 18;
    fontScaleX = fontScaleY = 100;
    fontSpacing = 0;
    fontWeight = FW_BOLD;
    fItalic = false;
    fUnderline = false;
    fStrikeOut = false;
    fBlur = 0;
    fGaussianBlur = 0;
    fontShiftX = fontShiftY = fontAngleZ = fontAngleX = fontAngleY = 0;
    relativeTo = 2;
}

bool STSStyle::operator == (const STSStyle& s)const
{
    return(marginRect == s.marginRect
        && scrAlignment == s.scrAlignment
        && borderStyle == s.borderStyle
        && outlineWidthX == s.outlineWidthX
        && outlineWidthY == s.outlineWidthY
        && shadowDepthX == s.shadowDepthX
        && shadowDepthY == s.shadowDepthY
        && *((int*)&colors[0]) == *((int*)&s.colors[0])
        && *((int*)&colors[1]) == *((int*)&s.colors[1])
        && *((int*)&colors[2]) == *((int*)&s.colors[2])
        && *((int*)&colors[3]) == *((int*)&s.colors[3])
        && alpha[0] == s.alpha[0]
        && alpha[1] == s.alpha[1]
        && alpha[2] == s.alpha[2]
        && alpha[3] == s.alpha[3]
        && fBlur == s.fBlur
        && fGaussianBlur == s.fGaussianBlur
        && relativeTo == s.relativeTo
        && IsFontStyleEqual(s));
}

bool STSStyle::IsFontStyleEqual(const STSStyle& s) const
{
    return(
        charSet == s.charSet
        && fontName == s.fontName
        && fontSize == s.fontSize
        && fontScaleX == s.fontScaleX
        && fontScaleY == s.fontScaleY
        && fontSpacing == s.fontSpacing
        && fontWeight == s.fontWeight
        && fItalic == s.fItalic
        && fUnderline == s.fUnderline
        && fStrikeOut == s.fStrikeOut
        && fontAngleZ == s.fontAngleZ
        && fontAngleX == s.fontAngleX
        && fontAngleY == s.fontAngleY
        && fontShiftX == s.fontShiftX
        && fontShiftY == s.fontShiftY);
}

void STSStyle::operator = (const LOGFONT& lf)
{
    charSet = lf.lfCharSet;
    fontName = lf.lfFaceName;
    HDC hDC = GetDC(0);
    fontSize = -MulDiv(lf.lfHeight, 72, GetDeviceCaps(hDC, LOGPIXELSY));
    ReleaseDC(0, hDC);
//  fontAngleZ = (float)(1.0*lf.lfEscapement/10);
    fontWeight = lf.lfWeight;
    fItalic = !!lf.lfItalic;
    fUnderline = !!lf.lfUnderline;
    fStrikeOut = !!lf.lfStrikeOut;
}

LOGFONTA& operator <<= (LOGFONTA& lfa, const STSStyleBase& s)
{
    lfa.lfCharSet = s.charSet;
    strncpy_s(lfa.lfFaceName, LF_FACESIZE, CStringA(s.fontName), _TRUNCATE);
    HDC hDC = GetDC(0);
    lfa.lfHeight = -MulDiv((int)(s.fontSize+0.5), GetDeviceCaps(hDC, LOGPIXELSY), 72);
    ReleaseDC(0, hDC);
    lfa.lfWeight = s.fontWeight;
    lfa.lfItalic = s.fItalic?-1:0;
    lfa.lfUnderline = s.fUnderline?-1:0;
    lfa.lfStrikeOut = s.fStrikeOut?-1:0;
    return(lfa);
}

LOGFONTW& operator <<= (LOGFONTW& lfw, const STSStyleBase& s)
{
    lfw.lfCharSet = s.charSet;
    wcsncpy_s(lfw.lfFaceName, LF_FACESIZE, CStringW(s.fontName), _TRUNCATE);
    HDC hDC = GetDC(0);
    lfw.lfHeight = -MulDiv((int)(s.fontSize+0.5), GetDeviceCaps(hDC, LOGPIXELSY), 72);
    ReleaseDC(0, hDC);
    lfw.lfWeight = s.fontWeight;
    lfw.lfItalic = s.fItalic?-1:0;
    lfw.lfUnderline = s.fUnderline?-1:0;
    lfw.lfStrikeOut = s.fStrikeOut?-1:0;
    return(lfw);
}

CString& operator <<= (CString& style, const STSStyle& s)
{
    style.Format(_T("%d;%d;%d;%d;")
                 _T("%d;%d;%f;%f;%f;%f;")
                 _T("0x%06x;0x%06x;0x%06x;0x%06x;")
                 _T("0x%02x;0x%02x;0x%02x;0x%02x;")
                 _T("%d;")
                 _T("%s;%f;%f;%f;%f;%d;")
                 _T("%d;%d;%d;%f;%f;")
                 _T("%f;%f;%f;")
                 _T("%d"),
        s.marginRect.get().left, s.marginRect.get().right, s.marginRect.get().top, s.marginRect.get().bottom,
        s.scrAlignment, s.borderStyle,s.outlineWidthX, s.outlineWidthY, s.shadowDepthX, s.shadowDepthY,
        s.colors[0], s.colors[1], s.colors[2], s.colors[3],
        s.alpha[0], s.alpha[1], s.alpha[2], s.alpha[3],
        s.charSet,
        s.fontName,s.fontSize,s.fontScaleX, s.fontScaleY,s.fontSpacing,s.fontWeight,
        (int)s.fItalic, (int)s.fUnderline, (int)s.fStrikeOut, s.fBlur, s.fGaussianBlur,
        s.fontAngleZ, s.fontAngleX, s.fontAngleY,
        s.relativeTo);

    return(style);
}

STSStyle& operator <<= (STSStyle& s, const CString& style)
{
    s.SetDefault();

    try
    {
        CStringW str = TToW(style);
        if(str.Find(';')>=0)
        {
            CRect tmp_rect;
            tmp_rect.left = GetInt(str,';'); tmp_rect.right = GetInt(str,';'); tmp_rect.top = GetInt(str,';'); tmp_rect.bottom = GetInt(str,';');
            s.marginRect = tmp_rect;
            s.scrAlignment = GetInt(str,';'); s.borderStyle = GetInt(str,';');
            s.outlineWidthX = GetFloat(str,';'); s.outlineWidthY = GetFloat(str,';'); s.shadowDepthX = GetFloat(str,';'); s.shadowDepthY = GetFloat(str,';');
            for(int i = 0; i < 4; i++) s.colors[i] = (COLORREF)GetInt(str,';');
            for(int i = 0; i < 4; i++) s.alpha[i] = GetInt(str,';');
            s.charSet = GetInt(str,';');
            s.fontName = WToT(GetStr(str,';')); s.fontSize = GetFloat(str,';');
            s.fontScaleX = GetFloat(str,';'); s.fontScaleY = GetFloat(str,';');
            s.fontSpacing = GetFloat(str,';'); s.fontWeight = GetInt(str,';');
            s.fItalic = !!GetInt(str,';'); s.fUnderline = !!GetInt(str,';'); s.fStrikeOut = !!GetInt(str,';'); s.fBlur = GetFloat(str,';'); s.fGaussianBlur = GetFloat(str,';');
            s.fontAngleZ = GetFloat(str,';'); s.fontAngleX = GetFloat(str,';'); s.fontAngleY = GetFloat(str,';');
            s.relativeTo = GetInt(str,';');
        }
    }
    catch(...)
    {
        s.SetDefault();
    }

    return(s);
}

static bool OpenRealText(CTextFile* file, CSimpleTextSubtitle& ret, int CharSet)
{
    wstring szFile;

    CStringW buff;
    while(file->ReadString(buff))
    {
        buff.Trim();
        if(buff.IsEmpty()) continue;

        szFile += CStringW(_T("\n")) + buff.GetBuffer();
    }

    CRealTextParser RealTextParser;
    if (!RealTextParser.ParseRealText(szFile))
        return false;

    CRealTextParser::Subtitles crRealText = RealTextParser.GetParsedSubtitles();

    for (map<pair<int, int>, wstring>::const_iterator i = crRealText.m_mapLines.begin();
         i != crRealText.m_mapLines.end();
         ++i)
    {
        ret.Add(
            SubRipper2SSA(i->second.c_str(), CharSet),
            file->IsUnicode(),
            i->first.first,
            i->first.second);
    }

//  std::wofstream wofsOut(L"c:/zzz.srt");
//  RealTextParser.OutputSRT(wofsOut);

    return(!ret.IsEmpty());
}
