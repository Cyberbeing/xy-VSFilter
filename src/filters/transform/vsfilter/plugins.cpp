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
#include <afxdlgs.h>
#include <atlpath.h>
#include "resource.h"
#include "../../../Subtitles/VobSubFile.h"
#include "../../../Subtitles/RTS.h"
#include "../../../Subtitles/SSF.h"
#include "../../../SubPic/PooledSubPic.h"
#include "../../../SubPic/SimpleSubPicProviderImpl.h"
#include "../../../subpic/color_conv_table.h"
#include "../../../subpic/SimpleSubPicWrapper.h"
#include "DirectVobSub.h"
#include "vfr.h"

#ifndef _WIN64
#include "vd2/extras/FilterSDK/VirtualDub.h"
#else
#include "vd2/plugin/vdplugin.h"
#include "vd2/plugin/vdvideofilt.h"
#endif

using namespace DirectVobSubXyOptions;

//
// Generic interface
//

namespace Plugin
{

class CFilter : public CUnknown, public CDirectVobSub, public CAMThread, public CCritSec
{
private:
	CString m_fn;

protected:
	float m_fps;
	CCritSec m_csSubLock;
	CComPtr<ISimpleSubPicProvider> m_simple_provider;
	CComPtr<ISubPicProvider> m_pSubPicProvider;
	DWORD_PTR m_SubPicProviderId;

    CSimpleTextSubtitle::YCbCrMatrix m_script_selected_yuv;
    CSimpleTextSubtitle::YCbCrRange m_script_selected_range;

    bool m_fLazyInit;
public:
    CFilter() : CUnknown(NAME("CFilter"), NULL), m_fps(-1), m_SubPicProviderId(0), m_fLazyInit(false)
    {
        //fix me: should not do init here
        CacheManager::GetPathDataMruCache()->SetMaxItemNum(m_xy_int_opt[INT_PATH_DATA_CACHE_MAX_ITEM_NUM]);
        CacheManager::GetScanLineData2MruCache()->SetMaxItemNum(m_xy_int_opt[INT_SCAN_LINE_DATA_CACHE_MAX_ITEM_NUM]);
        CacheManager::GetOverlayNoBlurMruCache()->SetMaxItemNum(m_xy_int_opt[INT_OVERLAY_NO_BLUR_CACHE_MAX_ITEM_NUM]);
        CacheManager::GetOverlayMruCache()->SetMaxItemNum(m_xy_int_opt[INT_OVERLAY_CACHE_MAX_ITEM_NUM]);

        XyFwGroupedDrawItemsHashKey::GetCacher()->SetMaxItemNum(m_xy_int_opt[INT_BITMAP_MRU_CACHE_ITEM_NUM]);
        CacheManager::GetBitmapMruCache()->SetMaxItemNum(m_xy_int_opt[INT_BITMAP_MRU_CACHE_ITEM_NUM]);

        CacheManager::GetClipperAlphaMaskMruCache()->SetMaxItemNum(m_xy_int_opt[INT_CLIPPER_MRU_CACHE_ITEM_NUM]);
        CacheManager::GetTextInfoCache()->SetMaxItemNum(m_xy_int_opt[INT_TEXT_INFO_CACHE_ITEM_NUM]);
        CacheManager::GetAssTagListMruCache()->SetMaxItemNum(m_xy_int_opt[INT_ASS_TAG_LIST_CACHE_ITEM_NUM]);

        SubpixelPositionControler::GetGlobalControler().SetSubpixelLevel( static_cast<SubpixelPositionControler::SUBPIXEL_LEVEL>(m_xy_int_opt[INT_SUBPIXEL_POS_LEVEL]) );
        
        m_script_selected_yuv = CSimpleTextSubtitle::YCbCrMatrix_AUTO;
        m_script_selected_range = CSimpleTextSubtitle::YCbCrRange_AUTO;

        CAMThread::Create();
    }
	virtual ~CFilter() {CAMThread::CallWorker(0);}

    DECLARE_IUNKNOWN;
    STDMETHODIMP NonDelegatingQueryInterface(REFIID riid, void** ppv)
    {
        CheckPointer(ppv, E_POINTER);
        
        return QI(IDirectVobSub)
            QI(IDirectVobSub2)
            QI(IDirectVobSubXy)
            QI(IFilterVersion)
            __super::NonDelegatingQueryInterface(riid, ppv);
    }
    
	CString GetFileName() {CAutoLock cAutoLock(this); return m_fn;}
	void SetFileName(CString fn) {CAutoLock cAutoLock(this); m_fn = fn;}

    void SetYuvMatrix(SubPicDesc& dst)
    {
        ColorConvTable::YuvMatrixType yuv_matrix = ColorConvTable::BT601;
        ColorConvTable::YuvRangeType yuv_range = ColorConvTable::RANGE_TV;

        if ( m_xy_int_opt[INT_COLOR_SPACE]==CDirectVobSub::YuvMatrix_AUTO )
        {
            switch(m_script_selected_yuv)
            {
            case CSimpleTextSubtitle::YCbCrMatrix_BT601:
                yuv_matrix = ColorConvTable::BT601;
                break;
            case CSimpleTextSubtitle::YCbCrMatrix_BT709:
                yuv_matrix = ColorConvTable::BT709;
                break;
            case CSimpleTextSubtitle::YCbCrMatrix_AUTO:
            default:
                yuv_matrix = ColorConvTable::BT601;                
                break;
            }
        }
        else
        {
            switch(m_xy_int_opt[INT_COLOR_SPACE])
            {
            case CDirectVobSub::BT_601:
                yuv_matrix = ColorConvTable::BT601;
                break;
            case CDirectVobSub::BT_709:
                yuv_matrix = ColorConvTable::BT709;
                break;
            case CDirectVobSub::GUESS:
                yuv_matrix = (dst.w > m_bt601Width || dst.h > m_bt601Height) ? ColorConvTable::BT709 : ColorConvTable::BT601;
                break;
            }
        }

        if( m_xy_int_opt[INT_YUV_RANGE]==CDirectVobSub::YuvRange_Auto )
        {
            switch(m_script_selected_range)
            {
            case CSimpleTextSubtitle::YCbCrRange_PC:
                yuv_range = ColorConvTable::RANGE_PC;
                break;
            case CSimpleTextSubtitle::YCbCrRange_TV:
                yuv_range = ColorConvTable::RANGE_TV;
                break;
            case CSimpleTextSubtitle::YCbCrRange_AUTO:
            default:        
                yuv_range = ColorConvTable::RANGE_TV;
                break;
            }
        }
        else
        {
            switch(m_xy_int_opt[INT_YUV_RANGE])
            {
            case CDirectVobSub::YuvRange_TV:
                yuv_range = ColorConvTable::RANGE_TV;
                break;
            case CDirectVobSub::YuvRange_PC:
                yuv_range = ColorConvTable::RANGE_PC;
                break;
            case CDirectVobSub::YuvRange_Auto:
                yuv_range = ColorConvTable::RANGE_TV;
                break;
            }
        }

        ColorConvTable::SetDefaultConvType(yuv_matrix, yuv_range);
    }

    bool Render(SubPicDesc& dst, REFERENCE_TIME rt, float fps)
    {
        if(!m_pSubPicProvider)
            return(false);

        if(!m_fLazyInit)
        {
            m_fLazyInit = true;

            SetYuvMatrix(dst);
        }
        CSize size(dst.w, dst.h);

        if(!m_simple_provider)
        {
            HRESULT hr;
            if(!(m_simple_provider = new SimpleSubPicProvider2(dst.type, size, size, CRect(CPoint(0,0), size), this, &hr)) || FAILED(hr))
            {
                m_simple_provider = NULL;
                return(false);
            }
            XySetSize(SIZE_ORIGINAL_VIDEO, size);
        }

        if(m_SubPicProviderId != (DWORD_PTR)(ISubPicProvider*)m_pSubPicProvider)
        {
            CSize playres(0,0);
            CLSID clsid;
            CComQIPtr<IPersist> tmp = m_pSubPicProvider;
            tmp->GetClassID(&clsid);
            if(clsid == __uuidof(CRenderedTextSubtitle))
            {
                CRenderedTextSubtitle* pRTS = dynamic_cast<CRenderedTextSubtitle*>((ISubPicProvider*)m_pSubPicProvider);
                playres = pRTS->m_dstScreenSize;
            }
            XySetSize(SIZE_ASS_PLAY_RESOLUTION, playres);

            m_simple_provider->SetSubPicProvider(m_pSubPicProvider);
            m_SubPicProviderId = (DWORD_PTR)(ISubPicProvider*)m_pSubPicProvider;
        }

		CComPtr<ISimpleSubPic> pSubPic;
		if(!m_simple_provider->LookupSubPic(rt, &pSubPic))
			return(false);

        if(dst.type == MSP_RGB32 || dst.type == MSP_RGB24 || dst.type == MSP_RGB16 || dst.type == MSP_RGB15)
            dst.h = -dst.h;
        pSubPic->AlphaBlt(&dst);

        return(true);
    }

	DWORD ThreadProc()
	{
		SetThreadPriority(m_hThread, THREAD_PRIORITY_LOWEST);

		CAtlArray<HANDLE> handles;
		handles.Add(GetRequestHandle());

		CString fn = GetFileName();
		CFileStatus fs;
		fs.m_mtime = 0;
		CFileGetStatus(fn, fs);

		while(1)
		{
			DWORD i = WaitForMultipleObjects(handles.GetCount(), handles.GetData(), FALSE, 1000);

			if(WAIT_OBJECT_0 == i)
			{
				Reply(S_OK);
				break;
			}
			else if(WAIT_OBJECT_0 + 1 >= i && i <= WAIT_OBJECT_0 + handles.GetCount())
			{
				if(FindNextChangeNotification(handles[i - WAIT_OBJECT_0]))
				{
					CFileStatus fs2;
					fs2.m_mtime = 0;
					CFileGetStatus(fn, fs2);

					if(fs.m_mtime < fs2.m_mtime)
					{
						fs.m_mtime = fs2.m_mtime;

						if(CComQIPtr<ISubStream> pSubStream = m_pSubPicProvider)
						{
							CAutoLock cAutoLock(&m_csSubLock);
							pSubStream->Reload();
						}
					}
				}
			}
			else if(WAIT_TIMEOUT == i)
			{
				CString fn2 = GetFileName();

				if(fn != fn2)
				{
					CPath p(fn2);
					p.RemoveFileSpec();
					HANDLE h = FindFirstChangeNotification(p, FALSE, FILE_NOTIFY_CHANGE_LAST_WRITE); 
					if(h != INVALID_HANDLE_VALUE)
					{
						fn = fn2;
						handles.SetCount(1);
						handles.Add(h);
					}
				}
			}
			else // if(WAIT_ABANDONED_0 == i || WAIT_FAILED == i)
			{
				break;
			}
		}

		m_hThread = 0;

		for(size_t i = 1; i < handles.GetCount(); i++)
			FindCloseChangeNotification(handles[i]);

		return 0;
	}
};

class CVobSubFilter : virtual public CFilter
{
public:
	CVobSubFilter(CString fn = _T(""))
	{
		if(!fn.IsEmpty()) Open(fn);
	}

	bool Open(CString fn)
	{
		SetFileName(_T(""));
		m_pSubPicProvider = NULL;

		if(CVobSubFile* vsf = new CVobSubFile(&m_csSubLock))
		{
			m_pSubPicProvider = (ISubPicProvider*)vsf;
			if(vsf->Open(CString(fn))) SetFileName(fn);
			else m_pSubPicProvider = NULL;
		}

		return !!m_pSubPicProvider;
	}
};

class CTextSubFilter : virtual public CFilter
{
	int m_CharSet;

public:
	CTextSubFilter(CString fn = _T(""), int CharSet = DEFAULT_CHARSET, float fps = -1)
		: m_CharSet(CharSet)
	{
		m_fps = fps;
		if(!fn.IsEmpty()) Open(fn, CharSet);
	}

	int GetCharSet() {return(m_CharSet);}

	bool Open(CString fn, int CharSet = DEFAULT_CHARSET)
	{
		SetFileName(_T(""));
		m_pSubPicProvider = NULL;

		if(!m_pSubPicProvider)
		{
			if(ssf::CRenderer* ssf = new ssf::CRenderer(&m_csSubLock))
			{
				m_pSubPicProvider = (ISubPicProvider*)ssf;
				if(ssf->Open(CString(fn))) SetFileName(fn);
				else m_pSubPicProvider = NULL;
			}
		}

		if(!m_pSubPicProvider)
		{
			if(CRenderedTextSubtitle* rts = new CRenderedTextSubtitle(&m_csSubLock))
			{
				m_pSubPicProvider = (ISubPicProvider*)rts;
				if(rts->Open(CString(fn), CharSet)) SetFileName(fn);
				else m_pSubPicProvider = NULL;

                m_script_selected_yuv = rts->m_eYCbCrMatrix;
                m_script_selected_range = rts->m_eYCbCrRange;
			}
		}

		return !!m_pSubPicProvider;
	}
};

#ifndef _WIN64
    //
    // old VirtualDub interface
    //

    namespace VirtualDub
    {
        class CVirtualDubFilter : virtual public CFilter
        {
        public:
            CVirtualDubFilter() {}
            virtual ~CVirtualDubFilter() {}

            virtual int RunProc(const FilterActivation* fa, const FilterFunctions* ff) {
                SubPicDesc dst;
                dst.type = MSP_RGB32;
                dst.w = fa->src.w;
                dst.h = fa->src.h;
                dst.bpp = 32;
                dst.pitch = fa->src.pitch;
                dst.bits = (LPVOID)fa->src.data;

                Render(dst, 10000i64 * fa->pfsi->lSourceFrameMS, (float)1000 / fa->pfsi->lMicrosecsPerFrame);

                return 0;
            }

            virtual long ParamProc(FilterActivation* fa, const FilterFunctions* ff) {
                fa->dst.offset  = fa->src.offset;
                fa->dst.modulo  = fa->src.modulo;
                fa->dst.pitch   = fa->src.pitch;

                return 0;
            }

            virtual int ConfigProc(FilterActivation* fa, const FilterFunctions* ff, HWND hwnd) = 0;
            virtual void StringProc(const FilterActivation* fa, const FilterFunctions* ff, char* str) = 0;
            virtual bool FssProc(FilterActivation* fa, const FilterFunctions* ff, char* buf, int buflen) = 0;
        };

        class CVobSubVirtualDubFilter : public CVobSubFilter, public CVirtualDubFilter
        {
        public:
            CVobSubVirtualDubFilter(CString fn = _T(""))
                : CVobSubFilter(fn) {}

            int ConfigProc(FilterActivation* fa, const FilterFunctions* ff, HWND hwnd) {
                AFX_MANAGE_STATE(AfxGetStaticModuleState());

                CFileDialog fd(TRUE, NULL, GetFileName(), OFN_EXPLORER | OFN_ENABLESIZING | OFN_HIDEREADONLY,
                               _T("VobSub files (*.idx;*.sub)|*.idx;*.sub||"), CWnd::FromHandle(hwnd), 0);

                if (fd.DoModal() != IDOK) {
                    return 1;
                }

                return Open(fd.GetPathName()) ? 0 : 1;
            }

            void StringProc(const FilterActivation* fa, const FilterFunctions* ff, char* str) {
                sprintf(str, " (%s)", !GetFileName().IsEmpty() ? CStringA(GetFileName()) : " (empty)");
            }

            bool FssProc(FilterActivation* fa, const FilterFunctions* ff, char* buf, int buflen) {
                CStringA fn(GetFileName());
                fn.Replace("\\", "\\\\");
                _snprintf_s(buf, buflen, buflen, "Config(\"%s\")", fn);
                return true;
            }
        };

        class CTextSubVirtualDubFilter : public CTextSubFilter, public CVirtualDubFilter
        {
        public:
            CTextSubVirtualDubFilter(CString fn = _T(""), int CharSet = DEFAULT_CHARSET)
                : CTextSubFilter(fn, CharSet) {}

            int ConfigProc(FilterActivation* fa, const FilterFunctions* ff, HWND hwnd) {
                AFX_MANAGE_STATE(AfxGetStaticModuleState());

                const TCHAR formats[] = _T("TextSub files (*.sub;*.srt;*.smi;*.ssa;*.ass;*.xss;*.psb;*.txt)|*.sub;*.srt;*.smi;*.ssa;*.ass;*.xss;*.psb;*.txt||");
                CFileDialog fd(TRUE, NULL, GetFileName(), OFN_EXPLORER | OFN_ENABLESIZING | OFN_HIDEREADONLY | OFN_ENABLETEMPLATE | OFN_ENABLEHOOK,
                               formats, CWnd::FromHandle(hwnd), sizeof(OPENFILENAME));
                UINT_PTR CALLBACK OpenHookProc(HWND hDlg, UINT uiMsg, WPARAM wParam, LPARAM lParam);

                fd.m_pOFN->hInstance = AfxGetResourceHandle();
                fd.m_pOFN->lpTemplateName = MAKEINTRESOURCE(IDD_TEXTSUBOPENTEMPLATE);
                fd.m_pOFN->lpfnHook = (LPOFNHOOKPROC)OpenHookProc;
                fd.m_pOFN->lCustData = (LPARAM)DEFAULT_CHARSET;

                if (fd.DoModal() != IDOK) {
                    return 1;
                }

                return Open(fd.GetPathName(), fd.m_pOFN->lCustData) ? 0 : 1;
            }

            void StringProc(const FilterActivation* fa, const FilterFunctions* ff, char* str) {
                if (!GetFileName().IsEmpty()) {
                    sprintf(str, " (%s, %d)", CStringA(GetFileName()), GetCharSet());
                } else {
                    sprintf(str, " (empty)");
                }
            }

            bool FssProc(FilterActivation* fa, const FilterFunctions* ff, char* buf, int buflen) {
                CStringA fn(GetFileName());
                fn.Replace("\\", "\\\\");
                _snprintf_s(buf, buflen, buflen, "Config(\"%s\", %d)", fn, GetCharSet());
                return true;
            }
        };

        int vobsubInitProc(FilterActivation* fa, const FilterFunctions* ff)
        {
            *(CVirtualDubFilter**)fa->filter_data = DEBUG_NEW CVobSubVirtualDubFilter();
            return !(*(CVirtualDubFilter**)fa->filter_data);
        }

        int textsubInitProc(FilterActivation* fa, const FilterFunctions* ff)
        {
            *(CVirtualDubFilter**)fa->filter_data = DEBUG_NEW CTextSubVirtualDubFilter();
            return !(*(CVirtualDubFilter**)fa->filter_data);
        }

        void baseDeinitProc(FilterActivation* fa, const FilterFunctions* ff)
        {
            CVirtualDubFilter* f = *(CVirtualDubFilter**)fa->filter_data;
            SAFE_DELETE(f);
        }

        int baseRunProc(const FilterActivation* fa, const FilterFunctions* ff)
        {
            CVirtualDubFilter* f = *(CVirtualDubFilter**)fa->filter_data;
            return f ? f->RunProc(fa, ff) : 1;
        }

        long baseParamProc(FilterActivation* fa, const FilterFunctions* ff)
        {
            CVirtualDubFilter* f = *(CVirtualDubFilter**)fa->filter_data;
            return f ? f->ParamProc(fa, ff) : 1;
        }

        int baseConfigProc(FilterActivation* fa, const FilterFunctions* ff, HWND hwnd)
        {
            CVirtualDubFilter* f = *(CVirtualDubFilter**)fa->filter_data;
            return f ? f->ConfigProc(fa, ff, hwnd) : 1;
        }

        void baseStringProc(const FilterActivation* fa, const FilterFunctions* ff, char* str)
        {
            CVirtualDubFilter* f = *(CVirtualDubFilter**)fa->filter_data;
            if (f) {
                f->StringProc(fa, ff, str);
            }
        }

        bool baseFssProc(FilterActivation* fa, const FilterFunctions* ff, char* buf, int buflen)
        {
            CVirtualDubFilter* f = *(CVirtualDubFilter**)fa->filter_data;
            return f ? f->FssProc(fa, ff, buf, buflen) : false;
        }

        void vobsubScriptConfig(IScriptInterpreter* isi, void* lpVoid, CScriptValue* argv, int argc)
        {
            FilterActivation* fa = (FilterActivation*)lpVoid;
            CVirtualDubFilter* f = *(CVirtualDubFilter**)fa->filter_data;
            if (f) {
                delete f;
            }
            f = DEBUG_NEW CVobSubVirtualDubFilter(CString(*argv[0].asString()));
            *(CVirtualDubFilter**)fa->filter_data = f;
        }

        void textsubScriptConfig(IScriptInterpreter* isi, void* lpVoid, CScriptValue* argv, int argc)
        {
            FilterActivation* fa = (FilterActivation*)lpVoid;
            CVirtualDubFilter* f = *(CVirtualDubFilter**)fa->filter_data;
            if (f) {
                delete f;
            }
            f = DEBUG_NEW CTextSubVirtualDubFilter(CString(*argv[0].asString()), argv[1].asInt());
            *(CVirtualDubFilter**)fa->filter_data = f;
        }

        ScriptFunctionDef vobsub_func_defs[] = {
            { (ScriptFunctionPtr)vobsubScriptConfig, "Config", "0s" },
            { NULL },
        };

        CScriptObject vobsub_obj = {
            NULL, vobsub_func_defs
        };

        struct FilterDefinition filterDef_vobsub = {
            NULL, NULL, NULL,           // next, prev, module
            "VobSub",                   // name
            "Adds subtitles from a vob sequence.", // desc
            "Gabest",                   // maker
            NULL,                       // private_data
            sizeof(CVirtualDubFilter**), // inst_data_size
            vobsubInitProc,             // initProc
            baseDeinitProc,             // deinitProc
            baseRunProc,                // runProc
            baseParamProc,              // paramProc
            baseConfigProc,             // configProc
            baseStringProc,             // stringProc
            NULL,                       // startProc
            NULL,                       // endProc
            &vobsub_obj,                // script_obj
            baseFssProc,                // fssProc
        };

        ScriptFunctionDef textsub_func_defs[] = {
            { (ScriptFunctionPtr)textsubScriptConfig, "Config", "0si" },
            { NULL },
        };

        CScriptObject textsub_obj = {
            NULL, textsub_func_defs
        };

        struct FilterDefinition filterDef_textsub = {
            NULL, NULL, NULL,           // next, prev, module
            "TextSub",                  // name
            "Adds subtitles from srt, sub, psb, smi, ssa, ass file formats.", // desc
            "Gabest",                   // maker
            NULL,                       // private_data
            sizeof(CVirtualDubFilter**), // inst_data_size
            textsubInitProc,            // initProc
            baseDeinitProc,             // deinitProc
            baseRunProc,                // runProc
            baseParamProc,              // paramProc
            baseConfigProc,             // configProc
            baseStringProc,             // stringProc
            NULL,                       // startProc
            NULL,                       // endProc
            &textsub_obj,               // script_obj
            baseFssProc,                // fssProc
        };

        static FilterDefinition* fd_vobsub;
        static FilterDefinition* fd_textsub;

        extern "C" __declspec(dllexport) int __cdecl VirtualdubFilterModuleInit2(FilterModule* fm, const FilterFunctions* ff, int& vdfd_ver, int& vdfd_compat)
        {
            fd_vobsub = ff->addFilter(fm, &filterDef_vobsub, sizeof(FilterDefinition));
            if (!fd_vobsub) {
                return 1;
            }
            fd_textsub = ff->addFilter(fm, &filterDef_textsub, sizeof(FilterDefinition));
            if (!fd_textsub) {
                return 1;
            }

            vdfd_ver = VIRTUALDUB_FILTERDEF_VERSION;
            vdfd_compat = VIRTUALDUB_FILTERDEF_COMPATIBLE;

            return 0;
        }

        extern "C" __declspec(dllexport) void __cdecl VirtualdubFilterModuleDeinit(FilterModule* fm, const FilterFunctions* ff)
        {
            ff->removeFilter(fd_textsub);
            ff->removeFilter(fd_vobsub);
        }
    }/**/

#else
    //
    // VirtualDub new plugin interface sdk 1.1
    //
    namespace VirtualDubNew
    {
        class CVirtualDubFilter : virtual public CFilter
        {
        public:
            CVirtualDubFilter() {}
            virtual ~CVirtualDubFilter() {}

            virtual int RunProc(const VDXFilterActivation* fa, const VDXFilterFunctions* ff) {
                SubPicDesc dst;
                dst.type = MSP_RGB32;
                dst.w = fa->src.w;
                dst.h = fa->src.h;
                dst.bpp = 32;
                dst.pitch = fa->src.pitch;
                dst.bits = (BYTE*)fa->src.data;

                Render(dst, 10000i64 * fa->pfsi->lSourceFrameMS, (float)1000 / fa->pfsi->lMicrosecsPerFrame);

                return 0;
            }

            virtual long ParamProc(VDXFilterActivation* fa, const VDXFilterFunctions* ff) {
                fa->dst.offset  = fa->src.offset;
                fa->dst.modulo  = fa->src.modulo;
                fa->dst.pitch   = fa->src.pitch;

                return 0;
            }

            virtual int ConfigProc(VDXFilterActivation* fa, const VDXFilterFunctions* ff, VDXHWND hwnd) = 0;
            virtual void StringProc(const VDXFilterActivation* fa, const VDXFilterFunctions* ff, char* str) = 0;
            virtual bool FssProc(VDXFilterActivation* fa, const VDXFilterFunctions* ff, char* buf, int buflen) = 0;
        };

        class CVobSubVirtualDubFilter : public CVobSubFilter, public CVirtualDubFilter
        {
        public:
            CVobSubVirtualDubFilter(CString fn = _T(""))
                : CVobSubFilter(fn) {}

            int ConfigProc(VDXFilterActivation* fa, const VDXFilterFunctions* ff, VDXHWND hwnd) {
                AFX_MANAGE_STATE(AfxGetStaticModuleState());

                CFileDialog fd(TRUE, NULL, GetFileName(), OFN_EXPLORER | OFN_ENABLESIZING | OFN_HIDEREADONLY,
                               _T("VobSub files (*.idx;*.sub)|*.idx;*.sub||"), CWnd::FromHandle((HWND)hwnd), 0);

                if (fd.DoModal() != IDOK) {
                    return 1;
                }

                return Open(fd.GetPathName()) ? 0 : 1;
            }

            void StringProc(const VDXFilterActivation* fa, const VDXFilterFunctions* ff, char* str) {
                sprintf(str, " (%s)", !GetFileName().IsEmpty() ? CStringA(GetFileName()) : " (empty)");
            }

            bool FssProc(VDXFilterActivation* fa, const VDXFilterFunctions* ff, char* buf, int buflen) {
                CStringA fn(GetFileName());
                fn.Replace("\\", "\\\\");
                _snprintf_s(buf, buflen, buflen, "Config(\"%s\")", fn);
                return true;
            }
        };

        class CTextSubVirtualDubFilter : public CTextSubFilter, public CVirtualDubFilter
        {
        public:
            CTextSubVirtualDubFilter(CString fn = _T(""), int CharSet = DEFAULT_CHARSET)
                : CTextSubFilter(fn, CharSet) {}

            int ConfigProc(VDXFilterActivation* fa, const VDXFilterFunctions* ff, VDXHWND hwnd) {
                AFX_MANAGE_STATE(AfxGetStaticModuleState());

                /* off encoding changing */
#ifndef _DEBUG
                const TCHAR formats[] = _T("TextSub files (*.sub;*.srt;*.smi;*.ssa;*.ass;*.xss;*.psb;*.txt)|*.sub;*.srt;*.smi;*.ssa;*.ass;*.xss;*.psb;*.txt||");
                CFileDialog fd(TRUE, NULL, GetFileName(), OFN_EXPLORER | OFN_ENABLESIZING | OFN_HIDEREADONLY | OFN_ENABLETEMPLATE | OFN_ENABLEHOOK,
                               formats, CWnd::FromHandle((HWND)hwnd), sizeof(OPENFILENAME));
                UINT_PTR CALLBACK OpenHookProc(HWND hDlg, UINT uiMsg, WPARAM wParam, LPARAM lParam);

                fd.m_pOFN->hInstance = AfxGetResourceHandle();
                fd.m_pOFN->lpTemplateName = MAKEINTRESOURCE(IDD_TEXTSUBOPENTEMPLATE);
                fd.m_pOFN->lpfnHook = (LPOFNHOOKPROC)OpenHookProc;
                fd.m_pOFN->lCustData = (LPARAM)DEFAULT_CHARSET;
#else
                const TCHAR formats[] = _T("TextSub files (*.sub;*.srt;*.smi;*.ssa;*.ass;*.xss;*.psb;*.txt)|*.sub;*.srt;*.smi;*.ssa;*.ass;*.xss;*.psb;*.txt||");
                CFileDialog fd(TRUE, NULL, GetFileName(), OFN_ENABLESIZING | OFN_HIDEREADONLY,
                               formats, CWnd::FromHandle((HWND)hwnd), sizeof(OPENFILENAME));
#endif
                if (fd.DoModal() != IDOK) {
                    return 1;
                }

                return Open(fd.GetPathName(), fd.m_pOFN->lCustData) ? 0 : 1;
            }

            void StringProc(const VDXFilterActivation* fa, const VDXFilterFunctions* ff, char* str) {
                if (!GetFileName().IsEmpty()) {
                    sprintf(str, " (%s, %d)", CStringA(GetFileName()), GetCharSet());
                } else {
                    sprintf(str, " (empty)");
                }
            }

            bool FssProc(VDXFilterActivation* fa, const VDXFilterFunctions* ff, char* buf, int buflen) {
                CStringA fn(GetFileName());
                fn.Replace("\\", "\\\\");
                _snprintf_s(buf, buflen, buflen, "Config(\"%s\", %d)", fn, GetCharSet());
                return true;
            }
        };

        int vobsubInitProc(VDXFilterActivation* fa, const VDXFilterFunctions* ff)
        {
            return ((*(CVirtualDubFilter**)fa->filter_data = DEBUG_NEW CVobSubVirtualDubFilter()) == NULL);
        }

        int textsubInitProc(VDXFilterActivation* fa, const VDXFilterFunctions* ff)
        {
            return ((*(CVirtualDubFilter**)fa->filter_data = DEBUG_NEW CTextSubVirtualDubFilter()) == NULL);
        }

        void baseDeinitProc(VDXFilterActivation* fa, const VDXFilterFunctions* ff)
        {
            CVirtualDubFilter* f = *(CVirtualDubFilter**)fa->filter_data;
            SAFE_DELETE(f);
        }

        int baseRunProc(const VDXFilterActivation* fa, const VDXFilterFunctions* ff)
        {
            CVirtualDubFilter* f = *(CVirtualDubFilter**)fa->filter_data;
            return f ? f->RunProc(fa, ff) : 1;
        }

        long baseParamProc(VDXFilterActivation* fa, const VDXFilterFunctions* ff)
        {
            CVirtualDubFilter* f = *(CVirtualDubFilter**)fa->filter_data;
            return f ? f->ParamProc(fa, ff) : 1;
        }

        int baseConfigProc(VDXFilterActivation* fa, const VDXFilterFunctions* ff, VDXHWND hwnd)
        {
            CVirtualDubFilter* f = *(CVirtualDubFilter**)fa->filter_data;
            return f ? f->ConfigProc(fa, ff, hwnd) : 1;
        }

        void baseStringProc(const VDXFilterActivation* fa, const VDXFilterFunctions* ff, char* str)
        {
            CVirtualDubFilter* f = *(CVirtualDubFilter**)fa->filter_data;
            if (f) {
                f->StringProc(fa, ff, str);
            }
        }

        bool baseFssProc(VDXFilterActivation* fa, const VDXFilterFunctions* ff, char* buf, int buflen)
        {
            CVirtualDubFilter* f = *(CVirtualDubFilter**)fa->filter_data;
            return f ? f->FssProc(fa, ff, buf, buflen) : false;
        }

        void vobsubScriptConfig(IVDXScriptInterpreter* isi, void* lpVoid, VDXScriptValue* argv, int argc)
        {
            VDXFilterActivation* fa = (VDXFilterActivation*)lpVoid;
            CVirtualDubFilter* f = *(CVirtualDubFilter**)fa->filter_data;
            if (f) {
                delete f;
            }
            f = DEBUG_NEW CVobSubVirtualDubFilter(CString(*argv[0].asString()));
            *(CVirtualDubFilter**)fa->filter_data = f;
        }

        void textsubScriptConfig(IVDXScriptInterpreter* isi, void* lpVoid, VDXScriptValue* argv, int argc)
        {
            VDXFilterActivation* fa = (VDXFilterActivation*)lpVoid;
            CVirtualDubFilter* f = *(CVirtualDubFilter**)fa->filter_data;
            if (f) {
                delete f;
            }
            f = DEBUG_NEW CTextSubVirtualDubFilter(CString(*argv[0].asString()), argv[1].asInt());
            *(CVirtualDubFilter**)fa->filter_data = f;
        }

        VDXScriptFunctionDef vobsub_func_defs[] = {
            { (VDXScriptFunctionPtr)vobsubScriptConfig, "Config", "0s" },
            { NULL },
        };

        VDXScriptObject vobsub_obj = {
            NULL, vobsub_func_defs
        };

        struct VDXFilterDefinition filterDef_vobsub = {
            NULL, NULL, NULL,           // next, prev, module
            "VobSub",                   // name
            "Adds subtitles from a vob sequence.", // desc
            "Gabest",                   // maker
            NULL,                       // private_data
            sizeof(CVirtualDubFilter**), // inst_data_size
            vobsubInitProc,             // initProc
            baseDeinitProc,             // deinitProc
            baseRunProc,                // runProc
            baseParamProc,              // paramProc
            baseConfigProc,             // configProc
            baseStringProc,             // stringProc
            NULL,                       // startProc
            NULL,                       // endProc
            &vobsub_obj,                // script_obj
            baseFssProc,                // fssProc
        };

        VDXScriptFunctionDef textsub_func_defs[] = {
            { (VDXScriptFunctionPtr)textsubScriptConfig, "Config", "0si" },
            { NULL },
        };

        VDXScriptObject textsub_obj = {
            NULL, textsub_func_defs
        };

        struct VDXFilterDefinition filterDef_textsub = {
            NULL, NULL, NULL,           // next, prev, module
            "TextSub",                  // name
            "Adds subtitles from srt, sub, psb, smi, ssa, ass file formats.", // desc
            "Gabest",                   // maker
            NULL,                       // private_data
            sizeof(CVirtualDubFilter**), // inst_data_size
            textsubInitProc,            // initProc
            baseDeinitProc,             // deinitProc
            baseRunProc,                // runProc
            baseParamProc,              // paramProc
            baseConfigProc,             // configProc
            baseStringProc,             // stringProc
            NULL,                       // startProc
            NULL,                       // endProc
            &textsub_obj,               // script_obj
            baseFssProc,                // fssProc
        };

        static VDXFilterDefinition* fd_vobsub;
        static VDXFilterDefinition* fd_textsub;

        extern "C" __declspec(dllexport) int __cdecl VirtualdubFilterModuleInit2(VDXFilterModule* fm, const VDXFilterFunctions* ff, int& vdfd_ver, int& vdfd_compat)
        {
            if (((fd_vobsub = ff->addFilter(fm, &filterDef_vobsub, sizeof(VDXFilterDefinition))) == NULL)
                    || ((fd_textsub = ff->addFilter(fm, &filterDef_textsub, sizeof(VDXFilterDefinition))) == NULL)) {
                return 1;
            }

            vdfd_ver = VIRTUALDUB_FILTERDEF_VERSION;
            vdfd_compat = VIRTUALDUB_FILTERDEF_COMPATIBLE;

            return 0;
        }

        extern "C" __declspec(dllexport) void __cdecl VirtualdubFilterModuleDeinit(VDXFilterModule* fm, const VDXFilterFunctions* ff)
        {
            ff->removeFilter(fd_textsub);
            ff->removeFilter(fd_vobsub);
        }
    }
#endif
    //
    // Avisynth interface
    //

    namespace AviSynth1
    {
#include "avisynth/avisynth1.h"

        class CAvisynthFilter : public GenericVideoFilter, virtual public CFilter
        {
        public:
            CAvisynthFilter(PClip c, IScriptEnvironment* env) : GenericVideoFilter(c) {}

            PVideoFrame __stdcall GetFrame(int n, IScriptEnvironment* env) {
                PVideoFrame frame = child->GetFrame(n, env);

                env->MakeWritable(&frame);

                SubPicDesc dst;
                dst.w = vi.width;
                dst.h = vi.height;
                dst.pitch = frame->GetPitch();
                dst.bits = (void**)frame->GetWritePtr();
                dst.bpp = vi.BitsPerPixel();
                dst.type =
                    vi.IsRGB32() ? (env->GetVar("RGBA").AsBool() ? MSP_RGBA : MSP_RGB32) :
                        vi.IsRGB24() ? MSP_RGB24 :
                        vi.IsYUY2() ? MSP_YUY2 :
                        -1;

                float fps = m_fps > 0 ? m_fps : (float)vi.fps_numerator / vi.fps_denominator;

                Render(dst, (REFERENCE_TIME)(10000000i64 * n / fps), fps);

                return frame;
            }
        };

        class CVobSubAvisynthFilter : public CVobSubFilter, public CAvisynthFilter
        {
        public:
            CVobSubAvisynthFilter(PClip c, const char* fn, IScriptEnvironment* env)
                : CVobSubFilter(CString(fn))
                , CAvisynthFilter(c, env) {
                if (!m_pSubPicProvider) {
                    env->ThrowError("VobSub: Can't open \"%s\"", fn);
                }
            }
        };

        AVSValue __cdecl VobSubCreateS(AVSValue args, void* user_data, IScriptEnvironment* env)
        {
            return (DEBUG_NEW CVobSubAvisynthFilter(args[0].AsClip(), args[1].AsString(), env));
        }

        class CTextSubAvisynthFilter : public CTextSubFilter, public CAvisynthFilter
        {
        public:
            CTextSubAvisynthFilter(PClip c, IScriptEnvironment* env, const char* fn, int CharSet = DEFAULT_CHARSET, float fps = -1)
                : CTextSubFilter(CString(fn), CharSet, fps)
                , CAvisynthFilter(c, env) {
                if (!m_pSubPicProvider) {
                    env->ThrowError("TextSub: Can't open \"%s\"", fn);
                }
            }
        };

        AVSValue __cdecl TextSubCreateS(AVSValue args, void* user_data, IScriptEnvironment* env)
        {
            return (DEBUG_NEW CTextSubAvisynthFilter(args[0].AsClip(), env, args[1].AsString()));
        }

        AVSValue __cdecl TextSubCreateSI(AVSValue args, void* user_data, IScriptEnvironment* env)
        {
            return (DEBUG_NEW CTextSubAvisynthFilter(args[0].AsClip(), env, args[1].AsString(), args[2].AsInt()));
        }

        AVSValue __cdecl TextSubCreateSIF(AVSValue args, void* user_data, IScriptEnvironment* env)
        {
            return (DEBUG_NEW CTextSubAvisynthFilter(args[0].AsClip(), env, args[1].AsString(), args[2].AsInt(), (float)args[3].AsFloat()));
        }

        AVSValue __cdecl MaskSubCreateSIIFI(AVSValue args, void* user_data, IScriptEnvironment* env)
        {
            AVSValue rgb32("RGB32");
            AVSValue  tab[5] = {
                args[1],
                args[2],
                args[3],
                args[4],
                rgb32
            };
            AVSValue value(tab, 5);
            const char* nom[5] = {
                "width",
                "height",
                "fps",
                "length",
                "pixel_type"
            };
            AVSValue clip(env->Invoke("Blackness", value, nom));
            env->SetVar(env->SaveString("RGBA"), true);
            return (DEBUG_NEW CTextSubAvisynthFilter(clip.AsClip(), env, args[0].AsString()));
        }

        extern "C" __declspec(dllexport) const char* __stdcall AvisynthPluginInit(IScriptEnvironment* env)
        {
            env->AddFunction("VobSub", "cs", VobSubCreateS, 0);
            env->AddFunction("TextSub", "cs", TextSubCreateS, 0);
            env->AddFunction("TextSub", "csi", TextSubCreateSI, 0);
            env->AddFunction("TextSub", "csif", TextSubCreateSIF, 0);
            env->AddFunction("MaskSub", "siifi", MaskSubCreateSIIFI, 0);
            env->SetVar(env->SaveString("RGBA"), false);
            return NULL;
        }
    }

    namespace AviSynth25
    {
#include "avisynth/avisynth25.h"

        static bool s_fSwapUV = false;

        class CAvisynthFilter : public GenericVideoFilter, virtual public CFilter
        {
        public:
            VFRTranslator* vfr;

            CAvisynthFilter(PClip c, IScriptEnvironment* env, VFRTranslator* _vfr = 0) : GenericVideoFilter(c), vfr(_vfr) {}

            PVideoFrame __stdcall GetFrame(int n, IScriptEnvironment* env) {
                PVideoFrame frame = child->GetFrame(n, env);

                env->MakeWritable(&frame);

                SubPicDesc dst;
                dst.w = vi.width;
                dst.h = vi.height;
                dst.pitch = frame->GetPitch();
                dst.pitchUV = frame->GetPitch(PLANAR_U);
                dst.bits = (void**)frame->GetWritePtr();
                dst.bitsU = frame->GetWritePtr(PLANAR_U);
                dst.bitsV = frame->GetWritePtr(PLANAR_V);
                dst.bpp = dst.pitch / dst.w * 8; //vi.BitsPerPixel();
                dst.type =
                    vi.IsRGB32() ? (env->GetVar("RGBA").AsBool() ? MSP_RGBA : MSP_RGB32)  :
                        vi.IsRGB24() ? MSP_RGB24 :
                        vi.IsYUY2() ? MSP_YUY2 :
                /*vi.IsYV12()*/ vi.pixel_type == VideoInfo::CS_YV12 ? (s_fSwapUV ? MSP_IYUV : MSP_YV12) :
                /*vi.IsIYUV()*/ vi.pixel_type == VideoInfo::CS_IYUV ? (s_fSwapUV ? MSP_YV12 : MSP_IYUV) :
                        -1;

                float fps = m_fps > 0 ? m_fps : (float)vi.fps_numerator / vi.fps_denominator;

                REFERENCE_TIME timestamp;

                if (!vfr) {
                    timestamp = (REFERENCE_TIME)(10000000i64 * n / fps);
                } else {
                    timestamp = (REFERENCE_TIME)(10000000 * vfr->TimeStampFromFrameNumber(n));
                }

                Render(dst, timestamp, fps);

                return frame;
            }
        };

        class CVobSubAvisynthFilter : public CVobSubFilter, public CAvisynthFilter
        {
        public:
            CVobSubAvisynthFilter(PClip c, const char* fn, IScriptEnvironment* env)
                : CVobSubFilter(CString(fn))
                , CAvisynthFilter(c, env) {
                if (!m_pSubPicProvider) {
                    env->ThrowError("VobSub: Can't open \"%s\"", fn);
                }
            }
        };

        AVSValue __cdecl VobSubCreateS(AVSValue args, void* user_data, IScriptEnvironment* env)
        {
            return (DEBUG_NEW CVobSubAvisynthFilter(args[0].AsClip(), args[1].AsString(), env));
        }

        class CTextSubAvisynthFilter : public CTextSubFilter, public CAvisynthFilter
        {
        public:
            CTextSubAvisynthFilter(PClip c, IScriptEnvironment* env, const char* fn, int CharSet = DEFAULT_CHARSET, float fps = -1, VFRTranslator* vfr = 0) //vfr patch
                : CTextSubFilter(CString(fn), CharSet, fps)
                , CAvisynthFilter(c, env, vfr) {
                if (!m_pSubPicProvider) {
                    env->ThrowError("TextSub: Can't open \"%s\"", fn);
                }
            }
        };

        AVSValue __cdecl TextSubCreateGeneral(AVSValue args, void* user_data, IScriptEnvironment* env)
        {
            if (!args[1].Defined()) {
                env->ThrowError("TextSub: You must specify a subtitle file to use");
            }
            VFRTranslator* vfr = 0;
            if (args[4].Defined()) {
                vfr = GetVFRTranslator(args[4].AsString());
            }

            return (DEBUG_NEW CTextSubAvisynthFilter(
                        args[0].AsClip(),
                        env,
                        args[1].AsString(),
                        args[2].AsInt(DEFAULT_CHARSET),
                        (float)args[3].AsFloat(-1),
                        vfr));
        }

        AVSValue __cdecl TextSubSwapUV(AVSValue args, void* user_data, IScriptEnvironment* env)
        {
            s_fSwapUV = args[0].AsBool(false);
            return AVSValue();
        }

        AVSValue __cdecl MaskSubCreate(AVSValue args, void* user_data, IScriptEnvironment* env)/*SIIFI*/
        {
            if (!args[0].Defined()) {
                env->ThrowError("MaskSub: You must specify a subtitle file to use");
            }
            if (!args[3].Defined() && !args[6].Defined()) {
                env->ThrowError("MaskSub: You must specify either FPS or a VFR timecodes file");
            }
            VFRTranslator* vfr = 0;
            if (args[6].Defined()) {
                vfr = GetVFRTranslator(args[6].AsString());
            }

            AVSValue rgb32("RGB32");
            AVSValue fps(args[3].AsFloat(25));
            AVSValue  tab[6] = {
                args[1],
                args[2],
                args[3],
                args[4],
                rgb32
            };
            AVSValue value(tab, 5);
            const char* nom[5] = {
                "width",
                "height",
                "fps",
                "length",
                "pixel_type"
            };
            AVSValue clip(env->Invoke("Blackness", value, nom));
            env->SetVar(env->SaveString("RGBA"), true);
            //return (DNew CTextSubAvisynthFilter(clip.AsClip(), env, args[0].AsString()));
            return (DEBUG_NEW CTextSubAvisynthFilter(
                        clip.AsClip(),
                        env,
                        args[0].AsString(),
                        args[5].AsInt(DEFAULT_CHARSET),
                        (float)args[3].AsFloat(-1),
                        vfr));
        }

        extern "C" __declspec(dllexport) const char* __stdcall AvisynthPluginInit2(IScriptEnvironment* env)
        {
            env->AddFunction("VobSub", "cs", VobSubCreateS, 0);
            env->AddFunction("TextSub", "c[file]s[charset]i[fps]f[vfr]s", TextSubCreateGeneral, 0);
            env->AddFunction("TextSubSwapUV", "b", TextSubSwapUV, 0);
            env->AddFunction("MaskSub", "[file]s[width]i[height]i[fps]f[length]i[charset]i[vfr]s", MaskSubCreate, 0);
            env->SetVar(env->SaveString("RGBA"), false);
            return NULL;
        }
    }

	namespace VapourSynth {
#include "vapoursynth/VapourSynth.h"

		class CVapourSynthVobSubFilter : public CVobSubFilter {
		public:
			using CVobSubFilter::CVobSubFilter;

			bool OpenedOkay() {
				return m_pSubPicProvider != 0;
			}
		};

		class CVapourSynthTextSubFilter : public CTextSubFilter {
		public:
			using CTextSubFilter::CTextSubFilter;

			bool OpenedOkay() {
				return m_pSubPicProvider != 0;
			}
		};

		struct FilterData {
			VSNodeRef *node;
			const VSVideoInfo *vi;

			VFRTranslator *vfr_translator;
			CVapourSynthVobSubFilter *vobsub;
			CVapourSynthTextSubFilter *textsub;

			std::string file;
			int charset;
			float fps;
			const char *vfr;
			bool swapuv;
		};


		void VS_CC vsfInit(VSMap *in, VSMap *out, void **instanceData, VSNode *node, VSCore *core, const VSAPI *vsapi) {
			(void)in;
			(void)out;
			(void)core;

			const FilterData *d = (const FilterData *)*instanceData;

			vsapi->setVideoInfo(d->vi, 1, node);
		}


		void VS_CC vsfFree(void *instanceData, VSCore *core, const VSAPI *vsapi) {
			(void)core;

			FilterData *d = (FilterData *)instanceData;

			vsapi->freeNode(d->node);

			if (d->vobsub)
				delete d->vobsub;
			if (d->textsub)
				delete d->textsub;

			delete d;
		}


		const VSFrameRef *VS_CC vsfGetFrame(int n, int activationReason, void **instanceData, void **frameData, VSFrameContext *frameCtx, VSCore *core, const VSAPI *vsapi) {
			(void)frameData;

			FilterData *d = (FilterData *)*instanceData;

			if (activationReason == arInitial) {
				vsapi->requestFrameFilter(n, d->node, frameCtx);
			}
			else if (activationReason == arAllFramesReady) {
				const VSFrameRef *src_frame = vsapi->getFrameFilter(n, d->node, frameCtx);

				VSFrameRef *dst_frame = vsapi->copyFrame(src_frame, core);
				vsapi->freeFrame(src_frame);

				VSFrameRef *temp = NULL;

				SubPicDesc dst;
				dst.w = d->vi->width;
				dst.h = d->vi->height;

				if (d->vi->format->colorFamily == cmYUV) {
					dst.pitch = vsapi->getStride(dst_frame, 0);
					dst.pitchUV = vsapi->getStride(dst_frame, 1);
					dst.bits = vsapi->getWritePtr(dst_frame, 0);
					dst.bitsU = vsapi->getWritePtr(dst_frame, 1);
					dst.bitsV = vsapi->getWritePtr(dst_frame, 2);
					dst.bpp = 8;
					dst.type = d->swapuv ? MSP_IYUV : MSP_YV12;
				}
				else if (d->vi->format->colorFamily == cmRGB) {
					temp = vsapi->newVideoFrame(vsapi->getFormatPreset(pfCompatBGR32, core), d->vi->width, d->vi->height, NULL, core);

					const BYTE *r = vsapi->getReadPtr(dst_frame, 0);
					const BYTE *g = vsapi->getReadPtr(dst_frame, 1);
					const BYTE *b = vsapi->getReadPtr(dst_frame, 2);
					BYTE *t = vsapi->getWritePtr(temp, 0);

					int dst_stride = vsapi->getStride(dst_frame, 0);
					int temp_stride = vsapi->getStride(temp, 0);

					for (int y = 0; y < d->vi->height; y++) {
						for (int x = 0; x < d->vi->width; x++) {
							t[x * 4 + 0] = b[x];
							t[x * 4 + 1] = g[x];
							t[x * 4 + 2] = r[x];
							t[x * 4 + 3] = 0;
						}

						b += dst_stride;
						g += dst_stride;
						r += dst_stride;
						t += temp_stride;
					}

					dst.pitch = vsapi->getStride(temp, 0);
					dst.bits = vsapi->getWritePtr(temp, 0);
					dst.bpp = 32;
					dst.type = MSP_RGB32;
				}

				REFERENCE_TIME timestamp;
//				TODO add vfr support
				if (!d->vfr) {
					timestamp = (REFERENCE_TIME)(10000000i64 * n / d->fps);
				}
				else {
					timestamp = (REFERENCE_TIME)(10000000 * d->vfr_translator->TimeStampFromFrameNumber(n));
				}

				if (d->vobsub)
					d->vobsub->Render(dst, timestamp, d->fps);
				else if (d->textsub)
					d->textsub->Render(dst, timestamp, d->fps);

				if (d->vi->format->colorFamily == cmRGB) {
					BYTE *r = vsapi->getWritePtr(dst_frame, 0);
					BYTE *g = vsapi->getWritePtr(dst_frame, 1);
					BYTE *b = vsapi->getWritePtr(dst_frame, 2);
					const BYTE *t = vsapi->getReadPtr(temp, 0);

					int dst_stride = vsapi->getStride(dst_frame, 0);
					int temp_stride = vsapi->getStride(temp, 0);

					for (int y = 0; y < d->vi->height; y++) {
						for (int x = 0; x < d->vi->width; x++) {
							b[x] = t[x * 4 + 0];
							g[x] = t[x * 4 + 1];
							r[x] = t[x * 4 + 2];
						}

						b += dst_stride;
						g += dst_stride;
						r += dst_stride;
						t += temp_stride;
					}

					vsapi->freeFrame(temp);
				}

				return dst_frame;
			}

			return NULL;
		}


		void VS_CC vsfCreate(const VSMap *in, VSMap *out, void *userData, VSCore *core, const VSAPI *vsapi) {
			FilterData d;
			memset(&d, 0, sizeof(FilterData));

			int err;

			std::string filter_name((const char *)userData);


			d.file = vsapi->propGetData(in, "file", 0, NULL);

			d.charset = (int)vsapi->propGetInt(in, "charset", 0, &err);
			if (err)
				d.charset = DEFAULT_CHARSET;

			d.fps = (float)vsapi->propGetFloat(in, "fps", 0, &err);
			if (err)
				d.fps = -1;


			d.vfr = vsapi->propGetData(in, "vfr", 0, &err);
			if (err)
				d.vfr = NULL;
			else
				d.vfr_translator = GetVFRTranslator(d.vfr);

			d.swapuv = !!vsapi->propGetInt(in, "swapuv", 0, &err);

			d.node = vsapi->propGetNode(in, "clip", 0, 0);
			d.vi = vsapi->getVideoInfo(d.node);

			if (!d.vi->format || (d.vi->format->id != pfRGB24 && d.vi->format->id != pfYUV420P8)) {
				vsapi->setError(out, (filter_name + ": only constant format RGB24 and YUV420P8 input supported.").c_str());
				vsapi->freeNode(d.node);
				return;
			}

			if (!d.vi->width || !d.vi->height) {
				vsapi->setError(out, (filter_name + ": only clips with constant dimensions supported.").c_str());
				vsapi->freeNode(d.node);
				return;
			}


			if (d.fps <= 0) {
				if (!d.vi->fpsNum || !d.vi->fpsDen) {
					vsapi->setError(out, (filter_name + ": the input clip has unknown frame rate. You must pass the 'fps' parameter.").c_str());
					vsapi->freeNode(d.node);
					return;
				}

				d.fps = (float)d.vi->fpsNum / d.vi->fpsDen;
			}

			bool opened_okay = false;
			if (filter_name == "VobSub") {
				d.vobsub = DEBUG_NEW CVapourSynthVobSubFilter(CString(d.file.c_str()));
				opened_okay = d.vobsub->OpenedOkay();
			}
			else if (filter_name == "TextSub") {
				d.textsub = DEBUG_NEW CVapourSynthTextSubFilter(CString(d.file.c_str()), d.charset, d.fps);
				opened_okay = d.textsub->OpenedOkay();
			}

			if (!opened_okay) {
				vsapi->setError(out, (filter_name + ": failed to open '" + d.file + "'.").c_str());
				vsapi->freeNode(d.node);
				if (d.vobsub)
					delete d.vobsub;
				if (d.textsub)
					delete d.textsub;
				return;
			}


			FilterData *data = new FilterData(d);

			vsapi->createFilter(in, out, filter_name.c_str(), vsfInit, vsfGetFrame, vsfFree, fmUnordered, 0, data, core);
		}


		VS_EXTERNAL_API(void) VapourSynthPluginInit(VSConfigPlugin configFunc, VSRegisterFunction registerFunc, VSPlugin *plugin) {
			configFunc("com.xy.vsfilter", "xyvsf", "xy-VSFilter", VAPOURSYNTH_API_VERSION, 1, plugin);

			registerFunc("VobSub",
				"clip:clip;"
				"file:data;"
				"swapuv:int:opt;"
				, vsfCreate, (void *)"VobSub", plugin);

			registerFunc("TextSub",
				"clip:clip;"
				"file:data;"
				"charset:int:opt;"
				"fps:float:opt;"
				"vfr:data:opt;"
				"swapuv:int:opt;"
				, vsfCreate, (void *)"TextSub", plugin);
		}
	}
}

UINT_PTR CALLBACK OpenHookProc(HWND hDlg, UINT uiMsg, WPARAM wParam, LPARAM lParam)
{
    switch (uiMsg) {
        case WM_NOTIFY: {
            OPENFILENAME* ofn = ((OFNOTIFY*)lParam)->lpOFN;

            if (((NMHDR*)lParam)->code == CDN_FILEOK) {
                ofn->lCustData = (LPARAM)CharSetList[SendMessage(GetDlgItem(hDlg, IDC_COMBO1), CB_GETCURSEL, 0, 0)];
            }

            break;
        }

        case WM_INITDIALOG: {
            SetWindowLongPtr(hDlg, GWLP_USERDATA, lParam);

            for (ptrdiff_t i = 0; i < CharSetLen; i++) {
                CString s;
                s.Format(_T("%s (%d)"), CharSetNames[i], CharSetList[i]);
                SendMessage(GetDlgItem(hDlg, IDC_COMBO1), CB_ADDSTRING, 0, (LPARAM)(LPCTSTR)s);
                if (CharSetList[i] == (int)((OPENFILENAME*)lParam)->lCustData) {
                    SendMessage(GetDlgItem(hDlg, IDC_COMBO1), CB_SETCURSEL, i, 0);
                }
            }

            break;
        }

        default:
            break;
    }

    return FALSE;
}