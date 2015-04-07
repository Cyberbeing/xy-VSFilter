#include "stdafx.h"
#include "xy_utils.h"
#include <vector>
#include <algorithm>

void MergeRects(const CAtlList<CRect>& input, CAtlList<CRect>* output)
{
    if(output==NULL || input.GetCount()==0)
        return;

    struct Segment
    {
        int top, bottom;
        int seg_start, seg_end;
        CAtlList<CRect>* rects;

        Segment()
        {
            rects = NULL;
        }
        ~Segment()
        {
            delete rects;
        }
    };

    struct BreakPoint
    {
        int x;
        const RECT* rect;

        inline bool operator<(const BreakPoint& breakpoint ) const
        {
            return (x < breakpoint.x);
        }
    };

    int input_count = input.GetCount();
    std::vector<int> vertical_breakpoints(2*input_count);
    std::vector<BreakPoint> horizon_breakpoints(input_count + 1);

    POSITION pos = input.GetHeadPosition();
    for(int i=0; i<input_count; i++)
    {
        const CRect& rect = input.GetNext(pos);
        vertical_breakpoints[2*i]=rect.top;
        vertical_breakpoints[2*i+1]=rect.bottom;

        horizon_breakpoints[i].x = rect.left;
        horizon_breakpoints[i].rect = &rect;
    }
    CRect sentinel_rect(INT_MAX, 0, INT_MAX, INT_MAX);
    horizon_breakpoints[input_count].x = INT_MAX;//sentinel
    horizon_breakpoints[input_count].rect = &sentinel_rect;

    std::sort(vertical_breakpoints.begin(), vertical_breakpoints.end());
    std::sort(horizon_breakpoints.begin(), horizon_breakpoints.end());

    std::vector<Segment> tempSegments(vertical_breakpoints.size()-1);
    int ptr = 1, prev = vertical_breakpoints[0], count = 0;
    for(int i = vertical_breakpoints.size()-1; i > 0; i--, ptr++)
    {
        if(vertical_breakpoints[ptr] != prev)
        {
            Segment& seg = tempSegments[count];
            seg.top = prev;
            seg.bottom = vertical_breakpoints[ptr];
            seg.seg_end = seg.seg_start = INT_MIN;//important! cannot use =0
            seg.rects = new CAtlList<CRect>();

            prev = vertical_breakpoints[ptr];
            count++;
        }
    }    

    for(int i=0; i<=input_count; i++)
    {
        const CRect& rect = *horizon_breakpoints[i].rect;

        int start = 0, mid, end = count;

        while(start<end)
        {
            mid = (start+end)>>1;
            if(tempSegments[mid].top < rect.top)
            {
                start = mid+1;
            }
            else
            {
                end = mid;
            }
        }
        for(; start < count && tempSegments[start].bottom <= rect.bottom; start++)
        {
            if(tempSegments[start].seg_end<rect.left)
            {
                CRect out_rect( tempSegments[start].seg_start, tempSegments[start].top, tempSegments[start].seg_end, tempSegments[start].bottom );
                tempSegments[start].rects->AddTail( out_rect );
                tempSegments[start].seg_start = rect.left;
                tempSegments[start].seg_end = rect.right;
            }
            else if( tempSegments[start].seg_end<rect.right )
            {
                tempSegments[start].seg_end=rect.right;
            }
        }
    }
    for (int i=count-1;i>0;i--)
    {
        CAtlList<CRect>& cur_line = *tempSegments[i].rects;
        CRect cur_rect = cur_line.RemoveTail();
        if(tempSegments[i].top == tempSegments[i-1].bottom)
        {            
            CAtlList<CRect>& upper_line = *tempSegments[i-1].rects;

            POSITION pos = upper_line.GetTailPosition();                          
            while(pos)
            {
                CRect& upper_rect = upper_line.GetPrev(pos);
                while( upper_rect.right<cur_rect.right || upper_rect.left<cur_rect.left )
                {
                    output->AddHead(cur_rect);
                    //if(!cur_line.IsEmpty())
                    cur_rect = cur_line.RemoveTail();
                }
                if(cur_line.IsEmpty())
                    break;

                if(upper_rect.right==cur_rect.right && upper_rect.left==cur_rect.left)
                {
                    upper_rect.bottom = cur_rect.bottom;
                    //if(!cur_line.IsEmpty())
                    cur_rect = cur_line.RemoveTail();
                }
                //else if ( upper_rect.right>cur_rect.right || upper_rect.left>cur_rect.left )             
            }
        }                
        while(!cur_line.IsEmpty())
        {
            output->AddHead(cur_rect);
            cur_rect = cur_line.RemoveTail();
        }
    }
    if(count>0)
    {
        CAtlList<CRect>& cur_line = *tempSegments[0].rects;
        CRect cur_rect = cur_line.RemoveTail();
        while(!cur_line.IsEmpty())
        {
            output->AddHead(cur_rect);
            cur_rect = cur_line.RemoveTail();
        }
    }
}
