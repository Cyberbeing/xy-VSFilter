#include "stdafx.h"
#include "xy_sub_filter.h"
#include "DirectVobSubFilter.h"
#include "VSFilter.h"

////////////////////////////////////////////////////////////////////////////
//
// Constructor
//

XySubFilter::XySubFilter( CDirectVobSubFilter *p_dvs, LPUNKNOWN punk )
    : CUnknown( NAME("XySubFilter"), punk )
    , m_dvs(p_dvs)
{
    CAMThread::Create();
    m_frd.EndThreadEvent.Create(0, FALSE, FALSE, 0);
    m_frd.RefreshEvent.Create(0, FALSE, FALSE, 0);
}

XySubFilter::~XySubFilter()
{
    m_frd.EndThreadEvent.Set();
    CAMThread::Close();
}

STDMETHODIMP XySubFilter::NonDelegatingQueryInterface(REFIID riid, void** ppv)
{
    CheckPointer(ppv, E_POINTER);

    return
		QI(IAMStreamSelect)
		__super::NonDelegatingQueryInterface(riid, ppv);
}

//
// IAMStreamSelect
//
STDMETHODIMP XySubFilter::Count(DWORD* pcStreams)
{
	if(!pcStreams) return E_POINTER;

	*pcStreams = 0;

	int nLangs = 0;
	if(SUCCEEDED(m_dvs->get_LanguageCount(&nLangs)))
		(*pcStreams) += nLangs;

	(*pcStreams) += 2; // enable ... disable

	(*pcStreams) += 2; // normal flipped

	return S_OK;
}

STDMETHODIMP XySubFilter::Enable(long lIndex, DWORD dwFlags)
{
	if(!(dwFlags & AMSTREAMSELECTENABLE_ENABLE))
		return E_NOTIMPL;

	int nLangs = 0;
	m_dvs->get_LanguageCount(&nLangs);

	if(!(lIndex >= 0 && lIndex < nLangs+2+2))
		return E_INVALIDARG;

	int i = lIndex-1;

	if(i == -1 && !m_dvs->m_fLoading) // we need this because when loading something stupid media player pushes the first stream it founds, which is "enable" in our case
	{
		m_dvs->put_HideSubtitles(false);
	}
	else if(i >= 0 && i < nLangs)
	{
		m_dvs->put_HideSubtitles(false);
		m_dvs->put_SelectedLanguage(i);

		WCHAR* pName = NULL;
		if(SUCCEEDED(m_dvs->get_LanguageName(i, &pName)))
		{
			m_dvs->UpdatePreferedLanguages(CString(pName));
			if(pName) CoTaskMemFree(pName);
		}
	}
	else if(i == nLangs && !m_dvs->m_fLoading)
	{
		m_dvs->put_HideSubtitles(true);
	}
	else if((i == nLangs+1 || i == nLangs+2) && !m_dvs->m_fLoading)
	{
		m_dvs->put_Flip(i == nLangs+2, m_dvs->m_fFlipSubtitles);
	}

	return S_OK;
}

STDMETHODIMP XySubFilter::Info(long lIndex, AM_MEDIA_TYPE** ppmt, DWORD* pdwFlags, LCID* plcid, DWORD* pdwGroup, WCHAR** ppszName, IUnknown** ppObject, IUnknown** ppUnk)
{
	AFX_MANAGE_STATE(AfxGetStaticModuleState());
    const int FLAG_CMD = 1;
    const int FLAG_EXTERNAL_SUB = 2;
    const int FLAG_PICTURE_CMD = 4;
    const int FLAG_VISIBILITY_CMD = 8;
    
    const int GROUP_NUM_BASE = 0x648E40;//random number

	int nLangs = 0;
	m_dvs->get_LanguageCount(&nLangs);

	if(!(lIndex >= 0 && lIndex < nLangs+2+2))
		return E_INVALIDARG;

	int i = lIndex-1;

	if(ppmt) *ppmt = CreateMediaType(&m_dvs->m_pInput->CurrentMediaType());

	if(pdwFlags)
	{
		*pdwFlags = 0;

		if(i == -1 && !m_dvs->m_fHideSubtitles
		|| i >= 0 && i < nLangs && i == m_dvs->m_iSelectedLanguage
		|| i == nLangs && m_dvs->m_fHideSubtitles
		|| i == nLangs+1 && !m_dvs->m_fFlipPicture
		|| i == nLangs+2 && m_dvs->m_fFlipPicture)
		{
			*pdwFlags |= AMSTREAMSELECTINFO_ENABLED;
		}
	}

	if(plcid) *plcid = 0;

	if(pdwGroup)
    {
        *pdwGroup = GROUP_NUM_BASE;
        if(i == -1)
        {
            *pdwGroup = GROUP_NUM_BASE | FLAG_CMD | FLAG_VISIBILITY_CMD;
        }
        else if(i >= 0 && i < nLangs)
        {
            bool isEmbedded = false;
            if( SUCCEEDED(m_dvs->GetIsEmbeddedSubStream(i, &isEmbedded)) )
            {
                if(isEmbedded)
                {
                    *pdwGroup = GROUP_NUM_BASE & ~(FLAG_CMD | FLAG_EXTERNAL_SUB);
                }
                else
                {
                    *pdwGroup = (GROUP_NUM_BASE & ~FLAG_CMD) | FLAG_EXTERNAL_SUB;
                }
            }            
        }
        else if(i == nLangs)
        {
            *pdwGroup = GROUP_NUM_BASE | FLAG_CMD | FLAG_VISIBILITY_CMD;
        }
        else if(i == nLangs+1)
        {
            *pdwGroup = GROUP_NUM_BASE | FLAG_CMD | FLAG_PICTURE_CMD;
        }
        else if(i == nLangs+2)
        {
            *pdwGroup = GROUP_NUM_BASE | FLAG_CMD | FLAG_PICTURE_CMD;
        }
    }

	if(ppszName)
	{
		*ppszName = NULL;

		CStringW str;
		if(i == -1) str = ResStr(IDS_M_SHOWSUBTITLES);
		else if(i >= 0 && i < nLangs)
        {
            m_dvs->get_LanguageName(i, ppszName);
        }
		else if(i == nLangs)
        {
            str = ResStr(IDS_M_HIDESUBTITLES);
        }
		else if(i == nLangs+1)
        {
            str = ResStr(IDS_M_ORIGINALPICTURE);
        }
		else if(i == nLangs+2)
        {
            str = ResStr(IDS_M_FLIPPEDPICTURE);
        }

		if(!str.IsEmpty())
		{
			*ppszName = (WCHAR*)CoTaskMemAlloc((str.GetLength()+1)*sizeof(WCHAR));
			if(*ppszName == NULL) return S_FALSE;
			wcscpy(*ppszName, str);
		}
	}

	if(ppObject) *ppObject = NULL;

	if(ppUnk) *ppUnk = NULL;

	return S_OK;
}

//
// FRD
// 
void XySubFilter::SetupFRD(CStringArray& paths, CAtlArray<HANDLE>& handles)
{
    CAutoLock cAutolock(&m_dvs->m_csSubLock);

    for(unsigned i = 2; i < handles.GetCount(); i++)
    {
        FindCloseChangeNotification(handles[i]);
    }

    paths.RemoveAll();
    handles.RemoveAll();

    handles.Add(m_frd.EndThreadEvent);
    handles.Add(m_frd.RefreshEvent);

    m_frd.mtime.SetCount(m_frd.files.GetCount());

    POSITION pos = m_frd.files.GetHeadPosition();
    for(unsigned i = 0; pos; i++)
    {
        CString fn = m_frd.files.GetNext(pos);

        CFileStatus status;
        if(CFileGetStatus(fn, status))
            m_frd.mtime[i] = status.m_mtime;

        fn.Replace('\\', '/');
        fn = fn.Left(fn.ReverseFind('/')+1);

        bool fFound = false;

        for(int j = 0; !fFound && j < paths.GetCount(); j++)
        {
            if(paths[j] == fn) fFound = true;
        }

        if(!fFound)
        {
            paths.Add(fn);

            HANDLE h = FindFirstChangeNotification(fn, FALSE, FILE_NOTIFY_CHANGE_LAST_WRITE);
            if(h != INVALID_HANDLE_VALUE) handles.Add(h);
        }
    }
}

DWORD XySubFilter::ThreadProc()
{
    SetThreadPriority(m_hThread, THREAD_PRIORITY_LOWEST/*THREAD_PRIORITY_BELOW_NORMAL*/);

    CStringArray paths;
    CAtlArray<HANDLE> handles;

    SetupFRD(paths, handles);

    while(1)
    {
        DWORD idx = WaitForMultipleObjects(handles.GetCount(), handles.GetData(), FALSE, INFINITE);

        if(idx == (WAIT_OBJECT_0 + 0)) // m_frd.hEndThreadEvent
        {
            break;
        }
        if(idx == (WAIT_OBJECT_0 + 1)) // m_frd.hRefreshEvent
        {
            SetupFRD(paths, handles);
        }
        else if(idx >= (WAIT_OBJECT_0 + 2) && idx < (WAIT_OBJECT_0 + handles.GetCount()))
        {
            bool fLocked = true;
            m_dvs->IsSubtitleReloaderLocked(&fLocked);
            if(fLocked) continue;

            if(FindNextChangeNotification(handles[idx - WAIT_OBJECT_0]) == FALSE)
                break;

            int j = 0;

            POSITION pos = m_frd.files.GetHeadPosition();
            for(int i = 0; pos && j == 0; i++)
            {
                CString fn = m_frd.files.GetNext(pos);

                CFileStatus status;
                if(CFileGetStatus(fn, status) && m_frd.mtime[i] != status.m_mtime)
                {
                    for(j = 0; j < 10; j++)
                    {
                        if(FILE* f = _tfopen(fn, _T("rb+")))
                        {
                            fclose(f);
                            j = 0;
                            break;
                        }
                        else
                        {
                            Sleep(100);
                            j++;
                        }
                    }
                }
            }

            if(j > 0)
            {
                SetupFRD(paths, handles);
            }
            else
            {
                Sleep(500);

                POSITION pos = m_frd.files.GetHeadPosition();
                for(int i = 0; pos; i++)
                {
                    CFileStatus status;
                    if(CFileGetStatus(m_frd.files.GetNext(pos), status)
                        && m_frd.mtime[i] != status.m_mtime)
                    {
                        m_dvs->Open();
                        SetupFRD(paths, handles);
                        break;
                    }
                }
            }
        }
        else
        {
            break;
        }
    }

    for(unsigned i = 2; i < handles.GetCount(); i++)
    {
        FindCloseChangeNotification(handles[i]);
    }

    return 0;
}
