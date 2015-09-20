#include <vector>

struct Vandalism
{
    struct Point
    {
        float x;
        float y;
        float t;
    };

    std::vector<Point> points;

    struct Stroke
    {
        uint32 startIdx;
        uint32 endIdx;
        float size;
    };

    // a subrange of some stroke
    struct Visible
    {
        uint32 startIdx;
        uint32 endIdx;
        uint32 strokeIdx;
    };

    std::vector<Stroke> strokes;
    std::vector<Visible> visibles;

    // TODO: change this
    bool visiblesChanged;

    struct Pan
    {
        float dx;
        float dy;
    };

    enum Mode
    {
        IDLE     = 0,
        DRAWING  = 1,
        PANNING  = 2,
        ZOOMING  = 3,
        ROTATING = 4,
        MODECNT  = 5
    };

    struct Input
    {
        float mousex;
        float mousey;
        bool mousedown;
        bool shiftdown;
        bool ctrldown;
        bool altdown;
    };

    typedef void (Vandalism::*MC_FN)(const Input *);


    void idle(const Input *) {}
    void start_pan(const Input *) {}
    void start_zoom(const Input *) {}
    void start_rotate(const Input *) {}
    void done_pan(const Input *) {}
    void done_zoom(const Input *) {}
    void done_rotate(const Input *) {}
    void do_pan(const Input *) {}
    void do_zoom(const Input *) {}
    void do_rotate(const Input *) {}

    void illegal(const Input *) {}

    void start_draw(const Input *input)
    {
        Stroke stroke;
        stroke.startIdx = points.size();
        {
            Point point;
            // TODO: transform from mouse to local space
            point.x = input->mousex;
            point.y = input->mousey;
            // TODO: fill t properly
            point.t = 0.0f;
            points.push_back(point);
        }
        stroke.endIdx = points.size();
        // TODO: stroke size and possibly other params
        stroke.size = 5.0f;
        strokes.push_back(stroke);
    }

    void do_draw(const Input *input)
    {
        Point point;
        // TODO: transform from mouse to local space
        point.x = input->mousex;
        point.y = input->mousey;
        // TODO: fill t properly
        point.t = 0.0f;
        points.push_back(point);

        strokes.back().endIdx = points.size();
    }

    void done_draw(const Input *)
    {
        Visible vis;
        vis.startIdx = strokes.back().startIdx;
        vis.endIdx = strokes.back().endIdx;
        vis.strokeIdx = strokes.size();

        visibles.push_back(vis);
        visiblesChanged = true;
    }

    MC_FN mode_change_handlers[MODECNT][MODECNT] =
    {
        // to ->      IDLE                     DRAW                    PAN                    ZOOM                    ROTATE
        /* IDLE */   {&Vandalism::idle,        &Vandalism::start_draw, &Vandalism::start_pan, &Vandalism::start_zoom, &Vandalism::start_rotate},
        /* DRAW */   {&Vandalism::done_draw,   &Vandalism::do_draw,    &Vandalism::illegal,   &Vandalism::illegal,    &Vandalism::illegal},
        /* PAN */    {&Vandalism::done_pan,    &Vandalism::illegal,    &Vandalism::do_pan,    &Vandalism::illegal,    &Vandalism::illegal},
        /* ZOOM */   {&Vandalism::done_zoom,   &Vandalism::illegal,    &Vandalism::illegal,   &Vandalism::do_zoom,    &Vandalism::illegal},
        /* ROTATE */ {&Vandalism::done_rotate, &Vandalism::illegal,    &Vandalism::illegal,   &Vandalism::illegal,    &Vandalism::do_rotate}
    };

    void handle_mode_change(Mode prevMode, Mode currMode, Input *input)
    {
        MC_FN fn = mode_change_handlers[prevMode][currMode];
        if (fn == &Vandalism::illegal)
        {
            MC_FN toIdle = mode_change_handlers[prevMode][IDLE];
            MC_FN fromIdle = mode_change_handlers[IDLE][currMode];

            (this->*toIdle)(input);
            (this->*fromIdle)(input);
        }
        else
        {
            (this->*fn)(input);
        }
    }

    Mode current_mode(Input *input)
    {
        bool l = input->mousedown;
        bool s = input->shiftdown;
        bool c = input->ctrldown;
        bool a = input->altdown;

        if (l && !s && !c && !a)
        {
            return DRAWING;
        }
        else if (l && s && !c && !a)
        {
            return PANNING;
        }
        else if (l && !s && c && !a)
        {
            return ZOOMING;
        }
        else if (l && !s && !c && a)
        {
            return ROTATING;
        }
        return IDLE;
    }

    Mode currentMode;

    void update(Input *input)
    {
        Mode mode = current_mode(input);
        handle_mode_change(currentMode, mode, input);
        currentMode = mode;
    }

    void setup()
    {
        currentMode = IDLE;
        visiblesChanged = true;
    }

    void cleanup()
    {
    }

    void get_current_stroke(size_t &from, size_t &to)
    {
        if (currentMode == DRAWING)
        {
            from = strokes.back().startIdx;
            to = strokes.back().endIdx;
        }
        else
        {
            from = 0;
            to = 0;
        }
    }
};

/*************************************************************

point coordinates are in 'inches'
viewport and bbox extents are in 'inches'
zoom > 1 is 'zoomin' zoom < 1 is 'zoomout'

@0 default
   0. stroke $0 [  #0  (-1, 0) ... #1 (1, 1)  ]
   1. stroke $1 [  #2 (-1, -1) ... #3 (1, 0)  ]
   bbox = (-1 ... 1 x  -1 ... 1)

@1 pan (+5, 0)
   0. stroke $2 [  #4  (-1, 0) ... #5 (1, 1)  ]
   1. stroke $3 [  #6 (-1, -1) ... #7 (1, 0)  ]
   bbox = (-1 ... 1 x  -1 ... 1)

@2 pan (+5, 0)
   0. stroke $4 [  #8  (-1, 0) ... #9 (1, 1)  ]
   1. stroke $5 [  #a (-1, -1) ... #b (1, 0)  ]
   bbox = (-1 ... 1 x  -1 ... 1)

@3 pan (-5, 0)

@4 zoom (1/5)
   0. stroke $6 [  #c    (-1, 2) ... #d (1, -2)      ]
   1. stroke $7 [  #e (3/2, 1/2) ... #f (3/2, -1/2)  ]
   bbox = (-1 ... 3/2 x -2 ... 2)

@5 pan (-1, 0)

@6 zoom (5/3)

query_visibles( @6, viewport(-5/2..5/2, -5/2..5/2) )
->
add_strokes_reversed(result, @6)  // result = []
attached = crop_strokes( @6, viewport(-5/2..5/2, -5/2..5/2) ) // []
reverse(attached) :: query_visibles( @5, viewport(-3/2..3/2, -3/2..3/2) )
                     ->
                     add_strokes_reversed(result, @5)  // result = []
                     attached = crop_strokes( @5, viewport(-3/2..3/2, -3/2..3/2) ) // []
                     reverse(attached) :: query_visibles( @5, viewport(3, 3) )



 *************************************************************/

struct test_point
{
    float x;
    float y;
};
struct test_stroke
{
    size_t pi0;
    size_t pi1;
};

enum transform_type { TZOOM, TPAN };

struct test_transform
{
    transform_type type;
    float a;
    float b;
};

struct test_view
{
    test_transform tr;
    size_t si0;
    size_t si1;
};

test_point t_points[] =
{
    {-1.0f,  0.0f},    {1.0f,  1.0f},
    {-1.0f, -1.0f},    {1.0f,  0.0f},
    {-1.0f,  0.0f},    {1.0f,  1.0f},
    {-1.0f, -1.0f},    {1.0f,  0.0f},
    {-1.0f,  0.0f},    {1.0f,  1.0f},
    {-1.0f, -1.0f},    {1.0f,  0.0f},
    {-1.0f,  2.0f},    {1.0f, -2.0f},
    { 1.5f,  0.5f},    {1.5f, -0.5f}
};

test_stroke t_strokes[] =
{
    { 0,  2},
    { 2,  4},
    { 4,  6},
    { 6,  8},
    { 8, 10},
    {10, 12},
    {12, 14},
    {14, 16}
};

test_view t_views[] =
{
    {{TPAN,  0.0f, 0.0f}, 0, 2},
    {{TPAN,  5.0f, 0.0f}, 2, 4},
    {{TPAN,  5.0f, 0.0f}, 4, 6},
    {{TPAN, -5.0f, 0.0f}, 6, 6},
    {{TZOOM, 1.0f, 5.0f}, 6, 8},
    {{TPAN, -1.0f, 0.0f}, 8, 8},
    {{TZOOM, 5.0f, 3.0f}, 8, 8}
};

struct test_box
{
    float x0, y0;
    float x1, y1;
};

struct test_visible
{
    size_t si;
    size_t ti;
};

bool liang_barsky(float L, float R, float B, float T,
                  float x0, float y0, float x1, float y1)
{
    float p[] = {x0 - x1, x1 - x0, y0 - y1, y1 - y0};
    float q[] = {x0 - L,   R - x0, y0 - B,   T - y0};

    float t0 = 0.0f;
    float t1 = 1.0f;

    for (size_t i = 0; i < 4; ++i)
    {
        if (p[i] == 0.0f)
        {
            if (q[i] < 0.0f) return false;
        }
        else
        {
            float t = q[i] / p[i];

            if (p[i] < 0.0f)
            {
                if (t > t0) t0 = t;
            }
            else
            {
                if (t < t1) t1 = t;
            }

            if (t0 > t1) return false;
        }
    }
    return true;
}

bool liang_barsky_test()
{
    float L = -5.0f;
    float R =  3.0f;
    float B = -2.0f;
    float T =  2.0f;

    float ax0 = -3, ay0 = -1, ax1 = -1, ay1 =  1;
    float bx0 = -2, by0 =  1, bx1 = -4, by1 =  3;
    float cx0 =  2, cy0 =  1, cx1 =  2, cy1 =  3;
    float dx0 =  6, dy0 = -1, dx1 =  6, dy1 =  1;
    float ex0 =  2, ey0 = -4, ex1 =  5, ey1 = -1;
    float fx0 =  1, fy0 =  3, fx1 =  2, fy1 = -3;
    float gx0 = -6, gy0 = -3, gx1 = -8, gy1 = -5;
    float hx0 = -6, hy0 =  1, hx1 = -4, hy1 =  3; // touch corner
    float ix0 = -1, iy0 = -4, ix1 = -1, iy1 = -2; // touch side
    float jx0 = -6, jy0 = -2, jx1 = -3, jy1 = -2; // on side

    bool ta = (true  == liang_barsky(L, R, B, T, ax0, ay0, ax1, ay1));
    bool tb = (true  == liang_barsky(L, R, B, T, bx0, by0, bx1, by1));
    bool tc = (true  == liang_barsky(L, R, B, T, cx0, cy0, cx1, cy1));
    bool td = (false == liang_barsky(L, R, B, T, dx0, dy0, dx1, dy1));
    bool te = (false == liang_barsky(L, R, B, T, ex0, ey0, ex1, ey1));
    bool tf = (true  == liang_barsky(L, R, B, T, fx0, fy0, fx1, fy1));
    bool tg = (false == liang_barsky(L, R, B, T, gx0, gy0, gx1, gy1));
    bool th = (true  == liang_barsky(L, R, B, T, hx0, hy0, hx1, hy1));
    bool ti = (true  == liang_barsky(L, R, B, T, ix0, iy0, ix1, iy1));
    bool tj = (true  == liang_barsky(L, R, B, T, jx0, jy0, jx1, jy1));

    printf("liang barsky test a: %s\n", ta ? "passed" : "failed");
    printf("liang barsky test b: %s\n", tb ? "passed" : "failed");
    printf("liang barsky test c: %s\n", tc ? "passed" : "failed");
    printf("liang barsky test d: %s\n", td ? "passed" : "failed");
    printf("liang barsky test e: %s\n", te ? "passed" : "failed");
    printf("liang barsky test f: %s\n", tf ? "passed" : "failed");
    printf("liang barsky test g: %s\n", tg ? "passed" : "failed");
    printf("liang barsky test h: %s\n", th ? "passed" : "failed");
    printf("liang barsky test i: %s\n", ti ? "passed" : "failed");
    printf("liang barsky test j: %s\n", tj ? "passed" : "failed");

    return true;
}

bool lb_test_result = liang_barsky_test();


bool intersects(const test_box &viewport, const test_stroke &stroke)
{
    test_point p0 = t_points[stroke.pi0];
    test_point p1 = t_points[stroke.pi1];
    return liang_barsky(viewport.x0, viewport.x1, viewport.y0, viewport.y1,
                        p0.x, p0.y, p1.x, p1.y);
}

void crop(const test_view &view, size_t ti, const test_box &viewport,
          std::vector<test_visible> &visibles)
{
    for (size_t si = view.si0; si < view.si1; ++si)
    {
        if (intersects(viewport, t_strokes[si]))
        {
            test_visible vis;
            vis.si = si;
            vis.ti = ti;
            visibles.push_back(vis);
        }
    }
}

void query(const test_view *views, size_t views_cnt, size_t view_idx,
           const test_box &viewport, std::vector<test_visible> &visibles)
{
}

bool run_test()
{
    test_box viewport;
    viewport.x0 = -2.5f;
    viewport.x1 =  2.5f;
    viewport.y0 = -2.5f;
    viewport.y1 =  2.5f;
    const size_t view_idx = 6;

    std::vector<test_visible> visibles;
    query(t_views, 7, view_idx, viewport, visibles);

    return true;
}

bool test_result = run_test();
