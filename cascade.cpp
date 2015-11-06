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

    float width() const
    {
        return x1 - x0;
    }

    float height() const
    {
        return y1 - y0;
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
          std::vector<test_visible> &visibles,
          float negligibledist)
{
    test_view view = data.views[vi];
    if (overlaps(viewport, view.bbox) &&
        view.bbox.width() > negligibledist &&
        view.bbox.height() > negligibledist)
    {
        for (size_t si = view.si0; si < view.si1; ++si)
        {
            test_stroke stroke = data.strokes[si];
            if (overlaps(viewport, stroke.bbox) &&
                stroke.bbox.width() > negligibledist &&
                stroke.bbox.height() > negligibledist)
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

float apply_transform_dist(const test_transform &t, float d)
{
    return d * t.s;
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
           std::vector<test_transform> &transforms,
           float negligibledist)
{
    // ps_*** -- pin space
    // ls_*** -- current view's space

    for (size_t vi = 0; vi < data.nviews; ++vi)
    {
        test_transform ls2ps = get_relative_transform(data, vi, pin);
        test_transform ps2ls = get_relative_transform(data, pin, vi);

        test_box ls_box = apply_transform_box(ps2ls, viewport);
        float ls_negligible = apply_transform_dist(ps2ls, negligibledist);
        crop(data, vi, transforms.size(), ls_box, visibles, ls_negligible);
        transforms.push_back(ls2ps);
    }
}

#include "tests.cpp"
