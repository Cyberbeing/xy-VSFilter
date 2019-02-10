#pragma once
#include "ass.h"

struct ASS_LibraryDeleter
{
    void operator()(ASS_Library* p) { if (p) ass_library_done(p); }
};

struct ASS_RendererDeleter
{
    void operator()(ASS_Renderer* p) { if (p) ass_renderer_done(p); }
};

struct ASS_TrackDeleter
{
    void operator()(ASS_Track* p) { if (p) ass_free_track(p); }
};