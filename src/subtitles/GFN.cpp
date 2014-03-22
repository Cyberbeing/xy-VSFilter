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
#include "xy_logger.h"

TCHAR* G_EXTTYPESTR[] = 
{
    _T("srt"), _T("sub"), _T("smi"), _T("psb"), 
    _T("ssa"), _T("ass"), _T("idx"), _T("usf"), 
    _T("xss"), _T("txt"), _T("ssf"), _T("rt"), _T("sup")
};

#define WEBSUBEXT _T(".wse")

static int SubFileCompare(const void* elem1, const void* elem2)
{
    const SubFile* file1 = ((const SubFile*)elem1);
    const SubFile* file2 = ((const SubFile*)elem2);

    int rv = file1->path_order - file2->path_order;
    rv = rv ? rv : file1->ext_order - file2->ext_order;
    rv = rv ? rv : file2->extra_name.IsEmpty() - file1->extra_name.IsEmpty();
    rv = rv ? rv : file1->extra_name.CompareNoCase(file2->extra_name);
    return rv;
}

class ExtSupport
{
public:
    bool init(const CString& load_ext_list)
    {
        int extsubnum = countof(G_EXTTYPESTR);
        for (int i=0;i<extsubnum;i++)
        {
            ext_map[CString(G_EXTTYPESTR[i])] = -1;
        }
        for (int i=0, start=0;true;i++)
        {
            CString ext = load_ext_list.Tokenize(_T(";"), start);
            if (ext!="")
            {
                int tmp = 0;
                if (ext_map.Lookup(ext, tmp))
                {
                    ext_map[ext] = i;
                }
            }
            else
            {
                break;
            }
        }
        return true;
    }

    int ext_support_order(const CString& ext)
    {
        int tmp = 0;
        return ext_map.Lookup(ext, tmp) ? tmp : -1;
    }
    CAtlMap<CString, int, CStringElementTraits<CString>> ext_map;
};

void GetSubFileNames(CString fn, CAtlArray<CString>& paths, CString load_ext_list, CAtlArray<SubFile>& ret)
{
    XY_LOG_INFO(XY_LOG_VAR_2_STR(fn.GetString())<<XY_LOG_VAR_2_STR(paths.GetCount())<<XY_LOG_VAR_2_STR(load_ext_list.GetString()));

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
    
    ExtSupport ext_support;
    if (!ext_support.init(load_ext_list))
    {
        XY_LOG_INFO(_T("unexpected error"));
        return;
    }

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
                        sl.AddTail(wfd.cFileName);
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
                int l = fn.ReverseFind('.');
                CString ext = fn.Mid(l+1).MakeLower();
                int ext_order = ext_support.ext_support_order(ext);
                if (ext_order>-1)
                {
                    SubFile f;
                    f.full_file_name = path + fn;

                    int l2 = fn.Find('.');
                    if (l2==l)
                    {
                        f.extra_name = "";
                    }
                    else
                    {
                        f.extra_name = fn.Mid(l2+1, l-l2-1);
                    }

                    f.ext_order = ext_order;
                    f.path_order = k;
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
                f.full_file_name = fn;
                f.extra_name = fn.Mid(fn.ReverseFind('/')+1);
                f.ext_order = MAXINT32;
                f.path_order = MAXINT32;
                ret.Add(f);
            }
        }
    }

    // sort files, this way the user can define the order (movie.00.English.srt, movie.01.Hungarian.srt, etc)

    qsort(ret.GetData(), ret.GetCount(), sizeof(SubFile), SubFileCompare);
}
