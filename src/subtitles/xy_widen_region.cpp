#include "stdafx.h"
#include "xy_widen_region.h"
#include "Rasterizer.h"
#include <vector>
#include "xy_circular_array_queue.h"
#include <math.h>

//terminology
//  @left arc : left part of an ellipse
//  @right arc: right part of an ellipse
//  @link arc : curve composed by all left most points of a series of left arc , or all right most points of a series of right arc 
//  @cross_line of arc a and b (a.y < b.y):
//    if both a and b are left arc, @cross_line is the max line that satisfied:
//      a(line)< b(line) on line<@cross_line, 
//    and
//      a(line)>=b(line) on line>=@cross_line
//    if both a and b are right arc, @cross_line is the max line that satisfied:
//      a(line)> b(line) on line<@cross_line, 
//    and
//      a(line)<=b(line) on line>=@cross_line

static const int MIN_CROSS_LINE = (-2147483647 - 1);
static const int MAX_CROSS_LINE = 0x7fffffff;
static const int MAX_X = 0x7fffffff;

typedef unsigned __int64 XY_POINT;
#define  XY_POINT_X(point)               ((point)&0xffffffff)
#define  XY_POINT_Y(point)               ((point)>>32)
#define  XY_POINT_SET(point,x,y)         ((point)=(((long long)(y)<<32)|(x)))

//ToDo: set a reg option for these
static const int LOOKUP_TABLE_SIZE_RX = 64;
static const int LOOKUP_TABLE_SIZE_RY = 64;

struct LinkArcItem
{
    LinkArcItem():dead_line(0),arc_center(0){}

    XY_POINT arc_center;
    int dead_line;
};

typedef XYCircularArrayQueue<LinkArcItem> LinkArc;

struct LinkSpan
{
    LinkArc left;
    LinkArc right;
};

typedef CAtlList<LinkSpan> LinkSpanList;

typedef tSpan Span;
typedef tSpanBuffer SpanBuffer;
#define SPAN_LEFT(span)       ((span).first)
#define SPAN_RIGHT(span)      ((span).second)
#define SPAN_PTR_LEFT(span)   ((span)->first)
#define SPAN_PTR_RIGHT(span)  ((span)->second)

struct XyEllipse
{
public:
    XyEllipse();
    ~XyEllipse();

    /*** 
     * ry must >0 and rx must >=0
     **/
    int init(int rx, int ry);

    inline int get_left_arc_x(int y) const;
    inline int get_left_arc_x(XY_POINT center, int y) const;

    inline int get_right_arc_x(int y) const;
    inline int get_right_arc_x(XY_POINT center, int y) const;

    int cross_left(XY_POINT c1, XY_POINT base) const;
    int cross_right(XY_POINT c1, XY_POINT base) const;

    int init_cross_point();

    int m_rx, m_ry;
    float m_f_rx, m_f_ry;
    double m_f_half_inv_rx, m_f_half_inv_ry;

    int *m_left_arc;
    int *m_left_arc_base;

    int *m_cross_matrix;
    int *m_cross_matrix_base;
private:
    int cross_left(int dx, int dy) const;

    XyEllipse(const XyEllipse&);
    void operator=(const XyEllipse&);
};

class WidenRegionCreaterImpl
{
public:
    WidenRegionCreaterImpl();
    ~WidenRegionCreaterImpl();

    void xy_overlap_region(SpanBuffer* dst, const SpanBuffer& src, int rx, int ry);
private:
    //return <ry if arc(o)<inner_pl, >ry if arc(0)>inner_pl, else cross_line exists
    int cross_left(const LinkArc& arc, XY_POINT center);

    void add_left(LinkArc* arc, XY_POINT center);
    void add_right(LinkArc* arc, XY_POINT center);
    void add_span(LinkSpan* spans, const Span& span);
    void add_line(SpanBuffer* dst, LinkSpanList& spans, int cur_line, int dead_line);//add all line<dead_line to dst

private:
    XyEllipse *m_ellipse;
};

//
// WidenRegionCreater
// 

WidenRegionCreater* WidenRegionCreater::GetDefaultWidenRegionCreater()
{
    static WidenRegionCreater result;
    static WidenRegionCreaterImpl impl;
    result.m_impl = &impl;
    return &result;
}

WidenRegionCreater::WidenRegionCreater():m_impl(NULL)
{
    
}

WidenRegionCreater::~WidenRegionCreater()
{

}

void WidenRegionCreater::xy_overlap_region( SpanBuffer* dst, const SpanBuffer& src, int rx, int ry )
{
    m_impl->xy_overlap_region(dst, src, rx, ry);
}

//
// WidenRegionCreaterImpl
// 

WidenRegionCreaterImpl::WidenRegionCreaterImpl(): m_ellipse(NULL)
{
}

WidenRegionCreaterImpl::~WidenRegionCreaterImpl()
{
    delete m_ellipse;
}

void WidenRegionCreaterImpl::xy_overlap_region(SpanBuffer* dst, const SpanBuffer& src, int rx, int ry)
{
    if (m_ellipse!=NULL && (m_ellipse->m_rx!=rx || m_ellipse->m_ry!=ry))
    {
        delete m_ellipse;
        m_ellipse = NULL;
    }
    if (m_ellipse==NULL)
    {
        m_ellipse = DEBUG_NEW XyEllipse();
        if (m_ellipse==NULL)
        {
            ASSERT(0);
            return;
        }
        int rv = m_ellipse->init(rx, ry);
        if (rv<0)
        {
            ASSERT(0);
            return;
        }
    }

    LinkSpanList link_span_list;
    POSITION pos=NULL;
    int dst_line = 0;

    SpanBuffer::const_iterator it_src = src.begin();
    SpanBuffer::const_iterator it_src_end = src.end();
    
    LinkSpan &sentinel = link_span_list.GetAt(link_span_list.AddTail());
    sentinel.left.init(2*ry+2);
    sentinel.right.init(2*ry+2);
    LinkArcItem& item = sentinel.left.inc_1_at_tail();
    XY_POINT_SET(item.arc_center, MAX_X, 0);
    item.dead_line = MAX_CROSS_LINE;

    for ( ;it_src!=it_src_end; it_src++)
    {
        const XY_POINT &left = SPAN_PTR_LEFT(it_src);
        const XY_POINT &right = SPAN_PTR_RIGHT(it_src);
        int line = XY_POINT_Y(left); //fix me
        if (line-ry>dst_line)
        {
            add_line(dst, link_span_list, dst_line, line - ry);
            dst_line = line - ry;
            ASSERT(!link_span_list.IsEmpty() && !link_span_list.GetTail().left.empty());
            ASSERT(XY_POINT_X(link_span_list.GetTail().left.get_at(0).arc_center)==MAX_X);
            XY_POINT_SET(link_span_list.GetTail().left.get_at(0).arc_center, MAX_X, line-1);//update sentinel
            pos = link_span_list.GetHeadPosition();
        }
        while(true)
        {
            LinkSpan &spans = link_span_list.GetAt(pos);
            LinkArc &arc = spans.left;
            int y = cross_left(arc, left);
            if (y>ry)
            {
                link_span_list.GetNext(pos);
                continue;
            }
            else if ( y>=-ry )
            {
                add_span(&spans, *it_src);
                link_span_list.GetNext(pos);
                break;
            }
            else
            {
                LinkSpan &new_span = link_span_list.GetAt(link_span_list.InsertBefore(pos, LinkSpan()));
                new_span.left.init(2*ry+2);
                LinkArcItem& item = new_span.left.inc_1_at_tail();

                item.arc_center = SPAN_PTR_LEFT(it_src);
                item.dead_line = line+ry+1;

                new_span.right.init(2*ry+2);
                LinkArcItem& item2 = new_span.right.inc_1_at_tail();
                item2.arc_center = SPAN_PTR_RIGHT(it_src);
                item2.dead_line = line+ry+1;
                break;
            }
        }
    }
    add_line(dst, link_span_list, dst_line, MAX_CROSS_LINE);
}

int WidenRegionCreaterImpl::cross_left(const LinkArc& arc, XY_POINT center)
{
    ASSERT(!arc.empty());
    int y = m_ellipse->cross_left(arc.back().arc_center, center);
    if (y>=MAX_CROSS_LINE)
    {
        return MAX_CROSS_LINE;
    }
    else 
    {
        y = m_ellipse->cross_left(arc.get_at(0).arc_center, center);
        if (y<=MIN_CROSS_LINE)
        {
            return MIN_CROSS_LINE;
        }
    }
    return 0;
}

void WidenRegionCreaterImpl::add_span(LinkSpan* spans, const Span& span )
{
    ASSERT(spans);
    add_left(&spans->left, SPAN_LEFT(span));
    add_right(&spans->right, SPAN_RIGHT(span));
}

void WidenRegionCreaterImpl::add_left( LinkArc* arc, XY_POINT center )
{
    int rx = m_ellipse->m_rx;
    int ry = m_ellipse->m_ry;
    ASSERT(arc);
    ASSERT(!arc->empty()&& m_ellipse->cross_left(arc->get_at(0).arc_center, center)>=-ry );
    int y = 0;
    int i = 0;
    int line = XY_POINT_Y(center);
    for (i=arc->size()-1;i>=1;i--)
    {
        y = m_ellipse->cross_left( arc->get_at(i).arc_center, center );
        if( y>ry )//not cross
        {
            break;
        }
        else if  ( y+line>arc->get_at(i-1).dead_line )
        {
            arc->get_at(i).dead_line = y+line;
            break;
        }
        else
        {
            arc->pop_back();
        }
    }    
    if (i==0)
    {
        y = m_ellipse->cross_left( arc->get_at(0).arc_center, center );
        ASSERT(y>=-ry && y+line<=arc->get_at(0).dead_line);
        if (y<=-ry)
        {
            arc->pop_back();
        }
        else
        {
            arc->get_at(0).dead_line = y + line;
        }
    }
    LinkArcItem& item = arc->inc_1_at_tail();
    item.arc_center = center;
    item.dead_line = line + ry + 1;
}

void WidenRegionCreaterImpl::add_right( LinkArc* arc, XY_POINT center )
{
    int rx = m_ellipse->m_rx;
    int ry = m_ellipse->m_ry;
    ASSERT(arc);
    ASSERT(!arc->empty());
    int y = 0;
    int i = 0;
    int line = XY_POINT_Y(center);
    for (i=arc->size()-1;i>=1;i--)
    {
        y = m_ellipse->cross_right( arc->get_at(i).arc_center, center );
        if( y>ry )//not cross
        {
            break;
        }
        else if ( y+line>arc->get_at(i-1).dead_line )
        {
            arc->get_at(i).dead_line = y+line;
            break;
        }
        else
        {
            arc->pop_back();
        }
    }    
    if (i==0)
    {
        y = m_ellipse->cross_right( arc->get_at(0).arc_center, center );
        if (y<=-ry)
        {
            arc->pop_back();
        }
        else if (y<MAX_CROSS_LINE)
        {
            ASSERT(y+line<=arc->get_at(0).dead_line);
            XY_POINT& arc_center = arc->get_at(0).arc_center;
            arc->get_at(0).dead_line = y + line;
        }
    }
    LinkArcItem& item = arc->inc_1_at_tail();
    item.arc_center = center;
    item.dead_line = line + ry + 1;
}

void WidenRegionCreaterImpl::add_line( SpanBuffer* dst, LinkSpanList& spans, int cur_dst_line, int dead_line )
{
    ASSERT(dst);

    Span dst_span(0,0);

    for( ;cur_dst_line<dead_line;cur_dst_line++)
    {
        POSITION pos_cur = spans.GetHeadPosition();
        POSITION pos_next = pos_cur;
        ASSERT(pos_next!=NULL);
        LinkSpan *span = &spans.GetNext(pos_next);
        if (pos_next==NULL)
        {
            break;
        }

        while(pos_next!=NULL)
        {
            ASSERT(!span->left.empty());
            const LinkArcItem &left_top = span->left.get_at(0);

            ASSERT(!span->right.empty());
            const LinkArcItem &right_top = span->right.get_at(0);

            ASSERT(cur_dst_line < left_top.dead_line);
            ASSERT(cur_dst_line < right_top.dead_line);

            XY_POINT left, right;
            int x = m_ellipse->get_left_arc_x(left_top.arc_center, cur_dst_line);
            XY_POINT_SET(left, x, cur_dst_line);
            x = m_ellipse->get_right_arc_x(right_top.arc_center, cur_dst_line);
            XY_POINT_SET(right, x, cur_dst_line);
            //ASSERT(SPAN_LEFT(dst_span)<=left);
            //  This assertion may fail because we're using a curve generated using Bresenham type algorithm
            //  and we're calculate the cross line using the exactly ellipse formula.
            //  It is hard to use the Bresenham ellipse curve to calculate the cross line because Bresenham
            //  ellipse curve c does NOT satisfy this necessary condition: 
            //    if c(y0) == c(y0+x)+dx and y1>y0 then 
            //       c(y1) >= c(y1+x)+dx
            if ( SPAN_RIGHT(dst_span) >= left )
            {
                if (SPAN_RIGHT(dst_span) < right )
                    SPAN_RIGHT(dst_span) = right;
                //if (SPAN_LEFT (dst_span) < left)
                //    SPAN_LEFT (dst_span) = left;
                // It may happen but I don't know if it is necessary to do this. Would it look any better?
            }
            else if ( SPAN_RIGHT(dst_span)>SPAN_LEFT(dst_span) )
            {
                dst->push_back(dst_span);
                SPAN_LEFT(dst_span) = left;
                SPAN_RIGHT(dst_span) = right;
            }
            else
            {
                SPAN_LEFT(dst_span) = left;
                SPAN_RIGHT(dst_span) = right;
            }

            if (cur_dst_line+1==left_top.dead_line)
            {
                span->left.pop_front();
            }
            if (cur_dst_line+1==right_top.dead_line)
            {
                span->right.pop_front();
            }
            if (span->left.empty())
            {
                ASSERT(span->right.empty());
                spans.RemoveAt(pos_cur);
                pos_cur = pos_next;
                span = &spans.GetNext(pos_next);
            }
            else
            {
                pos_cur = pos_next;
                span = &spans.GetNext(pos_next);
            }
        }
    }
    if ( SPAN_RIGHT(dst_span)>SPAN_LEFT(dst_span) )
    {
        dst->push_back(dst_span);
    }
}

//
// ref: A Fast Bresenham Type Algorithm For Drawing Ellipses by John Kennedy
// http://homepage.smc.edu/kennedy_john/belipse.pdf
// 
int gen_left_arc(int left_arc[], int rx, int ry)
{
    if (left_arc==NULL)
        return 0;
    
    int *left_arc2 = left_arc + ry;
    if (rx==0)
    {
        for (int y=-ry;y<=ry;y++)
        {
            left_arc2[y] = 0;
        }
        return 0;
    }
    else if (ry==0)
    {
        left_arc2[0] = rx;
        return 0;
    }

    rx = abs(rx);
    ry = abs(ry);
    __int64 a = rx;
    __int64 b = ry;
    __int64 a2 = 2* a * a;
    __int64 b2 = 2* b * b;
    __int64 x = a;
    __int64 y = 0;
    __int64 dx = b*b*(1-2*a);
    __int64 dy = a*a;
    __int64 err = 0;
    __int64 stopx = b2 * a;
    __int64 stopy = 0;
    while (stopx >= stopy)
    {
        left_arc2[y] = -x;
        left_arc2[-y] = -x;

        y++;
        stopy += a2;
        err += dy;
        dy += a2;
        if ( 2*err + dx > 0 )
        {
            x--;
            stopx -= b2;
            err += dx;
            dx += b2;
        }
    }
    __int64 last_y_stop = y;
    ASSERT(y>=-ry && y<=ry);
    x = 0;
    y = b;
    dx = b*b;
    dy = a*a*(1-2*b);
    err = 0;
    stopx = 0;
    stopy = a2*b;
    while (stopx <= stopy)
    {
        left_arc2[y] = -x;
        left_arc2[-y] = -x;

        x++;
        stopx += b2;
        err += dx;
        dx += b2;
        if (2*err+dy > 0) 
        {
            y--;            
            stopy -= a2;
            err += dy;
            dy += a2;
            
            left_arc2[y] = -x;
            left_arc2[-y] = -x;
        }
    }
    ASSERT(y>=-ry && y<=ry);
    while(y>last_y_stop)
    {
        y--;
        left_arc2[y] = left_arc2[y+1];
        left_arc2[-y] = left_arc2[y+1];
        if (y<=last_y_stop)
        {
            break;
        }
        left_arc2[last_y_stop] = left_arc[last_y_stop-1];
        left_arc2[-last_y_stop] = left_arc[last_y_stop-1];
        last_y_stop++;
    }
    //ASSERT(y<last_y_stop);
    return 0;
}

//
// XyEllipse
// 

XyEllipse::XyEllipse()
    : m_left_arc(NULL)
    , m_left_arc_base(NULL)
    , m_cross_matrix(NULL)
    , m_cross_matrix_base(NULL)
    , m_rx(0), m_ry(0)
{

}

XyEllipse::~XyEllipse()
{
    delete[]m_left_arc_base;
    delete[]m_cross_matrix_base;
}

/***
 * ry must >0 and rx must >=0
 ***/
int XyEllipse::init( int rx, int ry )
{
    if (rx<0 || ry<=0)
    {
        return -1;
    }
    m_left_arc_base = DEBUG_NEW int[2*ry+2];
    if (!m_left_arc_base)
    {
        return -1;
    }
    gen_left_arc(m_left_arc_base, rx, ry);
    m_left_arc = m_left_arc_base + ry;
    m_rx = rx;
    m_ry = ry;
    m_f_rx = rx;
    m_f_ry = ry;
    m_f_half_inv_rx = rx > 0 ? 0.5f / m_f_rx : 0;
    m_f_half_inv_ry = /*ry > 0*/ 0.5f / m_f_ry;
    init_cross_point();
    return 0;
}

int XyEllipse::get_left_arc_x( int y ) const
{
    return m_left_arc[y];
}

int XyEllipse::get_left_arc_x( XY_POINT center, int y ) const
{
    return XY_POINT_X(center) + m_left_arc[y-XY_POINT_Y(center)];
}

int XyEllipse::get_right_arc_x( int y ) const
{
    return -m_left_arc[y];
}

int XyEllipse::get_right_arc_x( XY_POINT center, int y ) const
{
    return XY_POINT_X(center) - m_left_arc[y-XY_POINT_Y(center)];
}

int XyEllipse::init_cross_point()
{
    if (m_rx<0 || m_ry<=0 )
    {
        return 0;
    }
    if ((2*m_ry+1)*(2*m_rx+1)>(2*LOOKUP_TABLE_SIZE_RY+1)*(2*LOOKUP_TABLE_SIZE_RX+1))
    {
        return 0;
    }
    m_cross_matrix_base = DEBUG_NEW int[ (2*m_ry+1)*(2*m_rx+1) ];
    if (!m_cross_matrix_base)
    {
        return -1;
    }

    m_cross_matrix = m_cross_matrix_base + (2*m_ry)*(2*m_rx+1) + m_rx;


    ASSERT(m_f_ry>0);
    float rx_divide_ry = m_f_rx/m_f_ry;
    float ry_2 = m_ry * m_ry;

    std::vector<int> cross_x_base(2*m_ry+1+1);
    int *cross_x = &cross_x_base[0] + m_ry + 1;
    cross_x[-m_ry-1] = m_rx;
    for (int dy=-2*m_ry;dy<=-1;dy++)
    {
        static bool first = true;
        int y=-m_ry;
        for ( ;y-dy<=m_ry;y++)
        {
            //f_cross_x = rx_divide_ry * ( sqrt(ry_2-(y-dy)*(y-dy))-sqrt(ry_2-y*y) )
            // But we have to do it this way because ry_2 may < y*y due to float rounding error
            float fxo_2 = ry_2 - float(y-dy)*float(y-dy);
            if (fxo_2<0) fxo_2 = 0;
            float fx_2 = ry_2 - float(y)*float(y);
            if (fx_2<0) fx_2 = 0;
            float f_cross_x = rx_divide_ry * ( sqrt(fxo_2)-sqrt(fx_2) );
            cross_x[y] = ceil(f_cross_x);
            //This assertion may fail, e.g. when (m_rx,m_ry)=(15,19) and dy=y=-19, because of rounding of float
            // ASSERT(cross_x[y]<=cross_x[y-1] && abs(cross_x[y])<=m_rx);
            cross_x[y] = min(cross_x[y-1], max(min(cross_x[y],m_rx),-m_rx));
        }

        int *cross_matrix = m_cross_matrix + dy*(2*m_rx+1);

        y--;
        //[ -m_rx, cross_x[y] )
        for (int j=-m_rx;j<cross_x[y];j++)
        {
            cross_matrix[j] = MAX_CROSS_LINE;
        }
        for ( ;y>-m_ry;y-- )
        {
            //[ cross_x[y], cross_x[y-1] )
            for (int j=cross_x[y];j<cross_x[y-1];j++)
            {
                cross_matrix[j] = y;
            }
        }
        cross_matrix[cross_x[y]] = y;
        //[ cross_x[y], m_rx ]
        for (int j=cross_x[y];j<=m_rx;j++)
        {
            cross_matrix[j] = MIN_CROSS_LINE;
        }
    }
    return 0;
}

int XyEllipse::cross_left( XY_POINT c1, XY_POINT base ) const
{
    int dx = XY_POINT_X(c1) - XY_POINT_X(base);
    int dy = XY_POINT_Y(c1) - XY_POINT_Y(base);
    return cross_left(dx, dy);
}

int XyEllipse::cross_right( XY_POINT c1, XY_POINT base ) const
{
    int dx = XY_POINT_X(base) - XY_POINT_X(c1);
    int dy = XY_POINT_Y(c1) - XY_POINT_Y(base);
    return cross_left(dx, dy);
}

int XyEllipse::cross_left( int dx, int dy ) const
{
    ASSERT(dy<0 && dy>=-2*m_ry);
    if (dx < -m_rx)
    {
        return MAX_CROSS_LINE;
    }
    else if (dx > m_rx)
    {
        return MIN_CROSS_LINE;
    }
    else if (m_cross_matrix)// [-m_rx, m_rx], use lookup table
    {
        ASSERT( abs( *(m_cross_matrix + dy*(2*m_rx+1) + dx) )<=m_ry || 
            *(m_cross_matrix + dy*(2*m_rx+1) + dx) == MIN_CROSS_LINE || 
            *(m_cross_matrix + dy*(2*m_rx+1) + dx) == MAX_CROSS_LINE );
        return *(m_cross_matrix + dy*(2*m_rx+1) + dx);
    }
    else
    {
        // unit circle with center at (0,0) and unit circle with center at (dx,dy) cross at points:
        //   x = 0.5*dx +/- 0.5*dy*sqrt(4/(dx*dx+dy*dy)-1)
        //   y = 0.5*dy -/+ 0.5*dx*sqrt(4/(dx*dx+dy*dy)-1)
        float f_half_dy = dy*m_f_half_inv_ry;
        float f_half_dx = dx*m_f_half_inv_rx;
        float f_tmp = 1.0f/(f_half_dx*f_half_dx+f_half_dy*f_half_dy)-1.0f;
        if (f_tmp >= 0)
        {
            f_tmp = sqrt(f_tmp);
            if (fabs(f_half_dx)+f_half_dy*f_tmp<=0)
            {
                float ret = f_half_dy - f_half_dx*f_tmp;
                ret *= m_f_ry;
                return ceil(ret);
            }
            else
            {
                return dx > 0 ? MIN_CROSS_LINE : MAX_CROSS_LINE;
            }
        }
        else
        {
            return dx > 0 ? MIN_CROSS_LINE : MAX_CROSS_LINE;
        }
    }
}
