/* 
 *	Copyright (C) 2003-2006 Gabest
 *	http://www.gabest.org
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
#include <io.h>
#include "TextFile.h"
#include "GFN.h"

TCHAR* G_EXTTYPESTR[] = 
{
    _T("srt"), _T("sub"), _T("smi"), _T("psb"), 
    _T("ssa"), _T("ass"), _T("idx"), _T("usf"), 
    _T("xss"), _T("txt"), _T("ssf"), _T("rt"), _T("sup")
};

#define WEBSUBEXT _T(".wse")

static int SubFileCompare(const void* elem1, const void* elem2)
{
    return(((SubFile*)elem1)->fn.CompareNoCase(((SubFile*)elem2)->fn));
}

static bool is_supported_ext(const CString& ext)
{
    static bool s_inited = false;
    static CAtlMap<CString, bool, CStringElementTraits<CString>> s_ext_map;
    if (!s_inited)
    {
        int extsubnum = countof(G_EXTTYPESTR);
        for (int i=0;i<extsubnum;i++)
        {
            s_ext_map[CString(G_EXTTYPESTR[i])] = true;
        }
        s_inited = true;
    }
    bool tmp = false;
    return s_ext_map.Lookup(ext, tmp);
}

void GetSubFileNames(CString fn, CAtlArray<CString>& paths, CAtlArray<SubFile>& ret)
{
    ret.RemoveAll();

    fn.Replace('\\', '/');

    bool fWeb = false;
    {
        //int i = fn.Find(_T("://"));
        int i = fn.Find(_T("http://"));
        if(i > 0) {fn = _T("http") + fn.Mid(i); fWeb = true;}
    }

    int	l = fn.GetLength(), l2 = l;
    l2 = fn.ReverseFind('.');
    l = fn.ReverseFind('/') + 1;
    if(l2 < l) l2 = l;

    CString orgpath = fn.Left(l);
    CString title = fn.Mid(l, l2-l);
    CString filename = title + _T(".nooneexpectsthespanishinquisition");

    if(!fWeb)
    {
        WIN32_FIND_DATA wfd;
        for(size_t k = 0; k < paths.GetCount(); k++)
        {
            CString path = paths[k];
            path.Replace('\\', '/');

            l = path.GetLength();
            if(l > 0 && path[l-1] != '/') path += '/';

            if(path.Find(':') == -1 && path.Find(_T("\\\\")) != 0) path = orgpath + path;

            path.Replace(_T("/./"), _T("/"));
            path.Replace('/', '\\');

            CAtlList<CString> sl;

            bool fEmpty = true;

            HANDLE hFile = FindFirstFile(path + title + _T(".*"), &wfd);
            if(hFile != INVALID_HANDLE_VALUE)
            {
                do
                {
                    if(filename.CompareNoCase(wfd.cFileName) != 0) 
                    {
                        fEmpty = false;
                        sl.AddTail(path + wfd.cFileName);
                    }
                }
                while(FindNextFile(hFile, &wfd));

                FindClose(hFile);
            }

            if(fEmpty) continue;

            POSITION pos = sl.GetHeadPosition();
            while(pos)
            {
                const CString& fn = sl.GetNext(pos);
                CString ext = fn.Mid(fn.ReverseFind('.'));
                if (is_supported_ext(ext))
                {
                    SubFile f;
                    f.fn = fn;
                    ret.Add(f);
                }
            }
        }
    }
    else if(l > 7)
    {
        CWebTextFile wtf; // :)
        if(wtf.Open(orgpath + title + WEBSUBEXT))
        {
            CString fn;
            while(wtf.ReadString(fn) && fn.Find(_T("://")) >= 0)
            {
                SubFile f;
                f.fn = fn;
                ret.Add(f);
            }
        }
    }

    // sort files, this way the user can define the order (movie.00.English.srt, movie.01.Hungarian.srt, etc)

    qsort(ret.GetData(), ret.GetCount(), sizeof(SubFile), SubFileCompare);
}
