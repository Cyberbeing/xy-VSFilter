#define CSRIAPI extern "C" __declspec(dllimport)
#include "csri.h"
#include <vector>

using namespace std;


csri_inst * g_csri_inst_yyy = NULL;

void OpenTestScript( const char *filename )
{
    csri_rend * csri_rend_xxx = csri_renderer_default();

    g_csri_inst_yyy = csri_open_file(csri_rend_xxx, filename, NULL);
}

void OverallTest( float fps /*= 25*/, int width/*=1280*/, int height/*=720*/, double start/*=0*/, double end/*=60*/ )
{
    csri_fmt fmt = { CSRI_F_BGR_, width, height };

    csri_request_fmt(g_csri_inst_yyy, &fmt);

    vector<unsigned char> buf( 4*(width+15)*(height+1)+256 );
    csri_frame frame;
    memset(&frame, 0, sizeof(frame));
    frame.pixfmt = CSRI_F_BGR_;
	frame.planes[0] = &buf[0]+16;
    frame.planes[0] = (unsigned char*)((int)(frame.planes[0]) & ~15);
    frame.strides[0] = ((width+15)&~15);
    
    for (double time=start;time<end;time+=1/fps)
    {
        csri_render(g_csri_inst_yyy, &frame, time);
    }
    csri_close(g_csri_inst_yyy);
}