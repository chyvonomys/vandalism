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

struct test_segment
{
    test_point a;
    test_point b;
};

struct test_box
{
    float x0, x1;
    float y0, y1;

    test_box() :
        x0(INFINITY), x1(-INFINITY),
        y0(INFINITY), y1(-INFINITY)
    {}

    test_box(float l, float r, float b, float t) :
        x0(l), x1(r),
        y0(b), y1(t)
    {}

    void grow(float s)
    {
        x0 -= s;
        x1 += s;
        y0 -= s;
        y1 += s;
    }

    void add(const test_point &p)
    {
        if (p.x > x1) x1 = p.x;
        if (p.x < x0) x0 = p.x;
        if (p.y > y1) y1 = p.y;
        if (p.y < y0) y0 = p.y;
    }

    void add_box(const test_box &b)
    {
        add({b.x0, b.y0});
        add({b.x1, b.y1});
    }

    bool empty() const
    {
        return x0 > x1 || y0 > y1;
    }
};

struct test_stroke
{
    size_t pi0;
    size_t pi1;
    size_t brush_id;
    test_box bbox;
};

enum transition_type { TZOOM, TPAN };

struct test_transition
{
    transition_type type;
    float a;
    float b;
};

struct test_view
{
    test_transition tr;
    size_t si0;
    size_t si1;
    test_box bbox;
};

struct test_data
{
    const test_point *points;
    const test_stroke *strokes;
    const test_view *views;
    const size_t nviews;
};

test_point t_points1[] =
{
    {-1.0f, -1.0f}, {1.0f, 1.0f},
    {-1.0f, -1.0f}, {1.0f, 1.0f}
};

test_stroke t_strokes1[] =
{
    {0, 2},
    {2, 4}
};

test_view t_views1[] =
{
    {{TPAN, 0.0f, 0.0f}, 0, 1},
    {{TPAN, 1.0f, 0.0f}, 1, 2}
};
const size_t NVIEWS1 = 2;
const size_t PIN1 = 1;

test_data test1 = {t_points1, t_strokes1, t_views1, NVIEWS1};

test_point t_points2[] =
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

test_stroke t_strokes2[] =
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

test_view t_views2[] =
{
    {{TPAN,  0.0f, 0.0f}, 0, 2},
    {{TPAN,  5.0f, 0.0f}, 2, 4},
    {{TPAN,  5.0f, 0.0f}, 4, 6},
    {{TPAN, -5.0f, 0.0f}, 6, 6},
    {{TZOOM, 1.0f, 5.0f}, 6, 8},
    {{TPAN, -1.0f, 0.0f}, 8, 8},
    {{TZOOM, 5.0f, 3.0f}, 8, 8}
};
const size_t NVIEWS2 = 7;
const size_t PIN2 = 6;

test_data test2 = {t_points2, t_strokes2, t_views2, NVIEWS2};

test_point t_points3[] =
{
    {0.0f, -0.5f}, {0.0f, 0.5f},
    {1.5f, -0.5f}, {1.5f, 0.5f}
};

test_stroke t_strokes3[] =
{
    {0, 2}
};

test_view t_views3[] =
{
    {{TPAN, 0.0f, 0.0f}, 0, 1},
    {{TZOOM, 2.0f, 1.0f}, 1, 1}
};
const size_t NVIEWS3 = 2;
const size_t PIN3 = 1;

test_data test3 = {t_points3, t_strokes3, t_views3, NVIEWS3};


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


bool intersects(const test_box &viewport,
                const test_point &p0,
                const test_point &p1)
{
    return liang_barsky(viewport.x0, viewport.x1, viewport.y0, viewport.y1,
                        p0.x, p0.y, p1.x, p1.y);
}

bool overlaps(const test_box &b1, const test_box &b2)
{
    if (b1.empty() || b2.empty()) return false;
    if (b1.y0 > b2.y1) return false;
    if (b2.y0 > b1.y1) return false;
    if (b1.x0 > b2.x1) return false;
    if (b2.x0 > b1.x1) return false;
    return true;
}

void crop(const test_data &data, size_t vi, size_t ti,
          const test_box &viewport,
          std::vector<test_visible> &visibles)
{
    test_view view = data.views[vi];
    if (overlaps(viewport, view.bbox))
    {
        for (size_t si = view.si0; si < view.si1; ++si)
        {
            test_stroke stroke = data.strokes[si];
            if (overlaps(viewport, stroke.bbox))
            {
                test_visible vis;
                vis.si = si;
                vis.ti = ti;
                visibles.push_back(vis);
            }
        }
    }
}

struct test_transform
{
    float s;
    float tx;
    float ty;
};

test_transform id_transform()
{
    return {1.0f, 0.0f, 0.0f};
}

test_transform transform_from_transition(const test_transition &transition)
{
    test_transform result = id_transform();

    if (transition.type == TZOOM)
    {
        result.s = transition.b / transition.a;
    }
    else if (transition.type == TPAN)
    {
        result.tx = transition.a;
        result.ty = transition.b;
    }

    return result;
}

test_transition inverse_transition(const test_transition &transition)
{
    test_transition result = transition;

    if (transition.type == TZOOM)
    {
        result.a = transition.b;
        result.b = transition.a;
    }
    else if (transition.type == TPAN)
    {
        result.a = -transition.a;
        result.b = -transition.b;
    }

    return result;
}

/*
test_box apply_transition_to_viewport(const test_transition &transition,
                                      const test_box &viewport)
{
    test_box result = viewport;

    if (transition.type == TZOOM)
    {
        float s = transition.b / transition.a;
        float w = s * (result.x1 - result.x0);
        float h = s * (result.y1 - result.y0);
        float cx = 0.5f * (result.x1 + result.x0);
        float cy = 0.5f * (result.y1 + result.y0);
        result.x0 = cx - 0.5f * w;
        result.x1 = cx + 0.5f * w;
        result.y0 = cy - 0.5f * h;
        result.y1 = cy + 0.5f * h;
    }
    else if (transition.type == TPAN)
    {
        result.x0 += transition.a;
        result.x1 += transition.a;
        result.y0 += transition.b;
        result.y1 += transition.b;
    }

    return result;
}
*/

test_point apply_transform_pt(const test_transform &t,
                           const test_point &p)
{
    test_point result;

    result.x = p.x * t.s + t.tx;
    result.y = p.y * t.s + t.ty;

    return result;
}

test_box apply_transform_box(const test_transform &t,
                         const test_box &b)
{
    test_point BL = apply_transform_pt(t, {b.x0, b.y0});
    test_point TR = apply_transform_pt(t, {b.x1, b.y1});
    return {BL.x, TR.x, BL.y, TR.y};
}

test_segment apply_transform_seg(const test_transform &t,
                             const test_segment &s)
{
    return {apply_transform_pt(t, s.a), apply_transform_pt(t, s.b)};
}

test_transform combine_transforms(const test_transform &t0,
                                  const test_transform &t1)
{
    test_transform result;

    result.s = t0.s * t1.s;
    result.tx = t0.s * t1.tx + t0.tx;
    result.ty = t0.s * t1.ty + t0.ty;

    return result;
}

bool points_eq(const test_point &a, const test_point &b)
{
    return iszero({a.x - b.x, a.y - b.y});
}

bool transforms_test()
{
    test_point p = {-6, 2};

    test_transform t1 = {0.5f, 2.0f, 2.0f};
    test_transform t2 = {4.0f, -1.0f, -5.0f};

    test_transform id = id_transform();

    test_transform t_1_2 = combine_transforms(t1, t2);

    test_transform t_1_id = combine_transforms(t1, id);
    test_transform t_id_1 = combine_transforms(id, t1);

    bool a1 = points_eq({-1, 3}, apply_transform_pt(t1, p));

    bool a2 = points_eq(p, apply_transform_pt(id, p));

    bool a3 = points_eq(apply_transform_pt(t_1_id, p),
                        apply_transform_pt(t_id_1, p));

    bool a4 = points_eq(apply_transform_pt(t_1_2, p),
                        apply_transform_pt(t1, apply_transform_pt(t2, p)));

    printf("transform test 1: %s\n", a1 ? "passed" : "failed");
    printf("transform test 2: %s\n", a2 ? "passed" : "failed");
    printf("transform test 3: %s\n", a3 ? "passed" : "failed");
    printf("transform test 4: %s\n", a4 ? "passed" : "failed");

    return true;
}

bool t_test_result = transforms_test();

// get transform which, when applied to points in src_idx view space
// will produce their coordinates in dst_idx view space
test_transform get_relative_transform(const test_data &data,
                                      size_t src_idx, size_t dst_idx)
{
    test_transform accum = id_transform();
    if (dst_idx > src_idx)
    {
        for (size_t vi = dst_idx; vi != src_idx; --vi)
        {
            const auto &view = data.views[vi];

            test_transition inv_tr = inverse_transition(view.tr);
            test_transform transform = transform_from_transition(inv_tr);
            accum = combine_transforms(accum, transform);
        }
    }
    else
    {
        for (size_t vi = dst_idx + 1; vi <= src_idx; ++vi)
        {
            const auto &view = data.views[vi];

            test_transform transform = transform_from_transition(view.tr);
            accum = combine_transforms(accum, transform);
        }
    }
    return accum;
}

// given a pin (a view index) collect list of strokes
// along with needed transforms, that are inside viewport
// which is in pin's view local space
void query(const test_data &data, size_t pin, const test_box &viewport,
           std::vector<test_visible> &visibles,
           std::vector<test_transform> &transforms)
{
    // ps_*** -- pin space
    // ls_*** -- current view's space

    for (size_t vi = 0; vi < data.nviews; ++vi)
    {
        test_transform ls2ps = get_relative_transform(data, vi, pin);
        test_transform ps2ls = get_relative_transform(data, pin, vi);

        test_box ls_box = apply_transform_box(ps2ls, viewport);
        crop(data, vi, transforms.size(), ls_box, visibles);
        transforms.push_back(ls2ps);
    }
}
