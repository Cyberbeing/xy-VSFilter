/************************************************************************/
/* author: xy                                                           */
/* date: 20110512                                                       */
/************************************************************************/
#include "stdafx.h"
#include "xy_logger.h"
#include <fstream>
#include <moreuuids.h>

#ifdef UNICODE

#define XY_TEXT(x)  L##x

#else //UNICODE

#define XY_TEXT(x)  x

#endif

std::ostream& operator<<(std::ostream& os, const POINT& obj)
{
    return os<<"("<<obj.x<<","<<obj.y<<")";
}

std::wostream& operator<<(std::wostream& os, const POINT& obj)
{
    return os<<L"("<<obj.x<<L","<<obj.y<<L")";
}

std::ostream& operator<<(std::ostream& os, const SIZE& obj)
{
    return os<<"("<<obj.cx<<","<<obj.cy<<")";
}

std::wostream& operator<<( std::wostream& os, const SIZE& obj )
{
    return os<<L"("<<obj.cx<<L","<<obj.cy<<L")";
}

std::ostream& operator<<(std::ostream& os, const RECT& obj)
{
    return os<<"("<<obj.left<<","<<obj.top<<","<<obj.right<<","<<obj.bottom<<")";
}

std::wostream& operator<<( std::wostream& os, const RECT& obj )
{
    return os<<L"("<<obj.left<<L","<<obj.top<<L","<<obj.right<<L","<<obj.bottom<<L")";
}

CString XyUuidToString(const UUID& in_uuid)
{
    CString ret;
    ret.Format(_T("{%08X-%04X-%04X-%02X%02X-%02X%02X%02X%02X%02X%02X}"),
        in_uuid.Data1, in_uuid.Data2, in_uuid.Data3, in_uuid.Data4[0], in_uuid.Data4[1], 
        in_uuid.Data4[2], in_uuid.Data4[3], in_uuid.Data4[4], 
        in_uuid.Data4[5], in_uuid.Data4[6], in_uuid.Data4[7]);
    return ret;
}

namespace xy_logger
{

#if ENABLE_XY_LOG
    int g_log_once_id=0;
    
    log4cplus::Logger g_logger = log4cplus::Logger::getInstance( XY_TEXT("global_logger_xy") );
#endif

bool doConfigure(const log4cplus::tstring& configFilename)
{
#if ENABLE_XY_LOG
    log4cplus::Hierarchy& h = log4cplus::Logger::getDefaultHierarchy();
    unsigned flags = 0;
    log4cplus::PropertyConfigurator::doConfigure(configFilename, h, flags);
#endif
    return true;
}

bool doConfigure( log4cplus::tistream& property_stream )
{
#if ENABLE_XY_LOG
    log4cplus::PropertyConfigurator p(property_stream);
    p.configure();
#endif
    return true;
}

void write_file(const char * filename, const void * buff, int size)
{
    FILE* out_file = NULL;
    int rv = fopen_s(&out_file, filename,"ab");
    if (rv!=0 || !out_file)
    {
        XY_LOG_ERROR("Failed to open file: "<<filename);
        return;
    }
    fwrite(buff, size, 1, out_file);
    fclose(out_file);
}

void DumpPackBitmap2File(POINT pos, SIZE size, LPCVOID pixels, int pitch, const char *filename)
{
    using namespace std;

    ofstream axxx(filename, ios_base::app);
    int h = size.cy;
    int w = size.cx;
    const BYTE* src = reinterpret_cast<const BYTE*>(pixels);

    axxx<<"pos:("<<pos.x<<","<<pos.y<<") size:("<<size.cx<<","<<size.cy<<")"<<std::endl;
    for(int i=0;i<h;i++, src += pitch)
    {
        const BYTE* s2 = src;
        const BYTE* s2end = s2 + w*4;
        for(; s2 < s2end; s2 += 4)
        {
            axxx<<(int)s2[0]<<","<<(int)s2[1]<<","<<(int)s2[2]<<","<<(int)s2[3]<<",";
        }
        axxx<<std::endl;
    }
    axxx.close();
}

void XyDisplayType(LPTSTR label, const AM_MEDIA_TYPE *pmtIn)
{
#if ENABLE_XY_LOG
    /* Dump the GUID types and a short description */

    XY_LOG_TRACE(_T(""));
    XY_LOG_TRACE(label<<_T("  M type ")<<GuidNames[pmtIn->majortype]<<_T("  S type "<<GuidNames[pmtIn->subtype]));
    XY_LOG_TRACE(_T("Subtype description ")<<GetSubtypeName(&pmtIn->subtype));

    /* Dump the generic media types */

    if (pmtIn->bTemporalCompression) {
        XY_LOG_TRACE(_T("Temporally compressed"));
    } else {
        XY_LOG_TRACE(_T("Not temporally compressed"));
    }

    if (pmtIn->bFixedSizeSamples) {
        XY_LOG_TRACE(_T("Sample size ")<<pmtIn->lSampleSize);
    } else {
        XY_LOG_TRACE(_T("Variable size samples"));
    }

    if (pmtIn->formattype == FORMAT_VideoInfo) {
        /* Dump the contents of the BITMAPINFOHEADER structure */
        BITMAPINFOHEADER *pbmi = HEADER(pmtIn->pbFormat);
        VIDEOINFOHEADER *pVideoInfo = (VIDEOINFOHEADER *)pmtIn->pbFormat;

        XY_LOG_TRACE(_T("Source rectangle :")<<pVideoInfo->rcSource);

        XY_LOG_TRACE(_T("Target rectangle :")<<pVideoInfo->rcTarget);

        XY_LOG_TRACE(_T("Size of BITMAPINFO structure ")<<pbmi->biSize);
        if (pbmi->biCompression < 256) {
            XY_LOG_TRACE(pbmi->biWidth<<_T("x")<<pbmi->biHeight<<_T("x")
                <<pbmi->biBitCount<<_T(" bit biCompression:")<<pbmi->biCompression);
        } 
        else {
            XY_LOG_TRACE(pbmi->biWidth<<_T("x")<<pbmi->biHeight<<_T("x")
                <<pbmi->biBitCount<<_T(" bit biCompression:")<<(char*)&pbmi->biCompression);
        }

        XY_LOG_TRACE(_T("Image size       ")<<pbmi->biSizeImage    );
        XY_LOG_TRACE(_T("Planes           ")<<pbmi->biPlanes       );
        XY_LOG_TRACE(_T("X Pels per metre ")<<pbmi->biXPelsPerMeter);
        XY_LOG_TRACE(_T("Y Pels per metre ")<<pbmi->biYPelsPerMeter);
        XY_LOG_TRACE(_T("Colours used     ")<<pbmi->biClrUsed      );

    } else if (pmtIn->majortype == MEDIATYPE_Audio) {
        XY_LOG_TRACE(_T("     Format type:")<<GuidNames[pmtIn->formattype]);
        XY_LOG_TRACE(_T("     Subtype    :")<<GuidNames[pmtIn->subtype   ]);

        if ((pmtIn->subtype != MEDIASUBTYPE_MPEG1Packet)
            && (pmtIn->cbFormat >= sizeof(PCMWAVEFORMAT)))
        {
            /* Dump the contents of the WAVEFORMATEX type-specific format structure */

            WAVEFORMATEX *pwfx = (WAVEFORMATEX *) pmtIn->pbFormat;
            XY_LOG_TRACE(_T("wFormatTag     :")<<pwfx->wFormatTag     );
            XY_LOG_TRACE(_T("nChannels      :")<<pwfx->nChannels      );
            XY_LOG_TRACE(_T("nSamplesPerSec :")<<pwfx->nSamplesPerSec );
            XY_LOG_TRACE(_T("nAvgBytesPerSec:")<<pwfx->nAvgBytesPerSec);
            XY_LOG_TRACE(_T("nBlockAlign    :")<<pwfx->nBlockAlign    );
            XY_LOG_TRACE(_T("wBitsPerSample :")<<pwfx->wBitsPerSample );

            /* PCM uses a WAVEFORMAT and does not have the extra size field */
            if (pmtIn->cbFormat >= sizeof(WAVEFORMATEX))
            XY_LOG_TRACE(_T("cbSize         :")<<pwfx->cbSize);
        } else {
        }
    } else {
        XY_LOG_TRACE(_T("     Format type ")<<GuidNames[pmtIn->formattype]);
        // !!!! should add code to dump wave format, others
    }
#endif
}

void XyDumpGraph(IFilterGraph *pGraph, DWORD dwLevel)
{
#if ENABLE_XY_LOG
    if( !pGraph )
    {
        return;
    }

    IEnumFilters *pFilters;

    XY_LOG_TRACE(_T("Graph: ")<<(void*)pGraph);

    if (FAILED(pGraph->EnumFilters(&pFilters))) {
        XY_LOG_TRACE(_T("    EnumFilters failed!"));
    }

    IBaseFilter *pFilter;
    ULONG n;
    while (pFilters->Next(1, &pFilter, &n) == S_OK) {
        FILTER_INFO info;

        if (FAILED(pFilter->QueryFilterInfo(&info))) {
            XY_LOG_TRACE(_T("    Filter [%")<<(void*)pFilter<<_T("]  -- failed QueryFilterInfo"));
        } 
        else {
            QueryFilterInfoReleaseGraph(info);

            // !!! should QueryVendorInfo here!

            XY_LOG_TRACE(_T("    Filter [")<<(void*)pFilter<<_T("]  '")<<info.achName<<_T("'"));

            IEnumPins *pins;

            if (FAILED(pFilter->EnumPins(&pins))) {
                XY_LOG_TRACE(_T("        EnumPins failed!"));
            }
            else {
                IPin *pPin;
                while (pins->Next(1, &pPin, &n) == S_OK) {
                    PIN_INFO info;

                    if (FAILED(pPin->QueryPinInfo(&info))) {
                        XY_LOG_TRACE(_T("          Pin [")<<(void*)pPin<<_T("]  -- failed QueryPinInfo"));
                    } else {
                        QueryPinInfoReleaseFilter(info);

                        IPin *pPinConnected = NULL;

                        HRESULT hr = pPin->ConnectedTo(&pPinConnected);

                        if (pPinConnected) {
                            XY_LOG_TRACE(_T("          Pin [")<<pPin<<_T("] ")<<info.achName<<_T(" ")
                                <<(info.dir == PINDIR_INPUT ? _T("[Input]") : _T("[Output]"))
                                <<_T("  Connected to pin [")<<(void*)pPinConnected<<_T("]"));

                            pPinConnected->Release();

                            // perhaps we should really dump the type both ways as a sanity
                            // check?
                            if (info.dir == PINDIR_OUTPUT) {
                                AM_MEDIA_TYPE mt;

                                hr = pPin->ConnectionMediaType(&mt);

                                if (SUCCEEDED(hr)) {
                                    XyDisplayType(TEXT("Connection type"), &mt);

                                    FreeMediaType(mt);
                                }
                            }
                        }
                        else {
                            XY_LOG_TRACE(_T("          Pin [")<<pPin<<_T("] ")<<info.achName<<_T(" ")
                                <<(info.dir == PINDIR_INPUT ? _T("[Input]") : _T("[Output]")));
                        }
                    }
                    pPin->Release();
                }
                pins->Release();
            }
        }
        pFilter->Release();
    }
    pFilters->Release();
#endif
}


} //namespace xy_logger

