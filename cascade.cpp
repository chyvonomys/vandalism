struct test_point
{
    float x;
    float y;
    float w;
    test_point() : w(1.0f) {}
    test_point(float a, float b) : x(a), y(b), w(1.0f) {}
    test_point(const float2& p, float v) : x(p.x), y(p.y), w(v) {}
};

struct test_stroke
{
    size_t pi0;
    size_t pi1;
    size_t brush_id;
    box2 cached_bbox;
};

struct test_image
{
    size_t nameidx;
    basis2r basis;

    box2 get_bbox() const
    {
        box2 bbox;

        bbox.add(basis.o);
        bbox.add(basis.o + basis.x);
        bbox.add(basis.o + basis.y);
        bbox.add(basis.o + basis.y + basis.x);

        return bbox;
    }
};

const size_t NPOS = static_cast<size_t>(-1);

struct test_view
{
    basis2s tr;
    size_t si0;
    size_t si1;
    size_t ii;
    size_t li;
    size_t pi;
    box2 cached_bbox;
    box2 cached_imgbbox;
    test_view(const basis2s& t, size_t s0, size_t s1, size_t l)
        : tr(t), si0(s0), si1(s1), ii(NPOS), li(l), pi(NPOS)
    {}

    bool has_image() const { return ii != NPOS; }
    bool is_pinned() const { return pi != NPOS; }
    bool has_strokes() const { return si1 > si0; }
};

struct test_data
{
    const test_point *points;
    const test_stroke *strokes;
    const test_image *images;
    const test_view *views;
    size_t nviews;
};

struct test_visible
{
    enum vis_type {IMAGE, STROKE};
    vis_type ty;
    size_t obj_id;
    size_t tform_id;
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

bool intersects(const box2 &viewport, const float2 &p0, const float2 &p1)
{
    return liang_barsky(viewport.min.x, viewport.max.x, viewport.min.y, viewport.max.y,
                        p0.x, p0.y, p1.x, p1.y);
}

bool overlaps(const box2 &b1, const box2 &b2)
{
    if (b1.empty() || b2.empty()) return false;
    if (b1.min.y > b2.max.y) return false;
    if (b2.min.y > b1.max.y) return false;
    if (b1.min.x > b2.max.x) return false;
    if (b2.min.x > b1.max.x) return false;
    return true;
}

void crop(const test_data &data, size_t vi, size_t ti,
          const box2 &viewport,
          std::vector<test_visible> &visibles,
          float negligibledist)
{
    test_view view = data.views[vi];

    if (view.has_image())
    {
        // TODO: why do we check with negligibledist here?
        if (overlaps(viewport, view.cached_imgbbox) &&
            view.cached_imgbbox.width() > negligibledist &&
            view.cached_imgbbox.height() > negligibledist)
        {
            test_visible vis;
            vis.ty = test_visible::IMAGE;
            vis.obj_id = view.ii;
            vis.tform_id = ti;
            visibles.push_back(vis);
        }
    }

    // TODO: why do we check with negligibledist here?
    if (overlaps(viewport, view.cached_bbox) &&
        view.cached_bbox.width() > negligibledist &&
        view.cached_bbox.height() > negligibledist)
    {

        for (size_t si = view.si0; si < view.si1; ++si)
        {
            test_stroke stroke = data.strokes[si];
            // TODO: why do we check with negligibledist here?
            if (overlaps(viewport, stroke.cached_bbox) &&
                stroke.cached_bbox.width() > negligibledist &&
                stroke.cached_bbox.height() > negligibledist)
            {
                test_visible vis;
                vis.ty = test_visible::STROKE;
                vis.obj_id = si;
                vis.tform_id = ti;
                visibles.push_back(vis);
            }
        }
    }
}

basis2s default_basis()
{
    return{ {0.0f, 0.0f}, {1.0f, 0.0f} };
}

basis2s make_zoom(float a, float b)
{
    return{ {0.0f, 0.0f}, {b / a, 0.0f} };
}

basis2s make_pan(const float2 &p)
{
    return{ p, {1.0f, 0.0f} };
}

basis2s make_rotate(float a)
{
    return{ {0.0f, 0.0f}, {si_cosf(a), si_sinf(a)} };
}

basis2s inverse_basis(const basis2s &basis)
{
    float det = basis.x.x * basis.x.x + basis.x.y * basis.x.y;

    float nx = basis.x.x / det;
    float ny = -basis.x.y / det;

    return basis2s{ -basis.o, {nx, ny} };
}

float2 point_in_basis(const basis2s &b, const float2 &p)
{
    float Xx = b.x.x;
    float Xy = b.x.y;
    float Yx = b.get_y().x;
    float Yy = b.get_y().y;

    return
    {
        p.x * Xx + p.y * Yx + b.o.x,
        p.x * Xy + p.y * Yy + b.o.y
    };
}

basis2s basis_in_basis(const basis2s &b0, const basis2s &b1)
{
    float2 b1o_ = point_in_basis(b0, b1.o);
    float2 b1x_ = point_in_basis(b0, b1.o + b1.x);
    
    return{ b1o_, b1x_ - b1o_ };
}

box2 box_in_basis(const basis2s &t, const box2 &b)
{
    if (b.empty())
        return b;

    box2 result;
    result.add(point_in_basis(t, b.min)); // BL
    result.add(point_in_basis(t, b.get_tl())); // TL
    result.add(point_in_basis(t, b.max)); // TR
    result.add(point_in_basis(t, b.get_br())); // BR
    return result;
}

float dist_in_basis(const basis2s &b, float d)
{
    return d * len(b.x);
}

// get transform which, when applied to points in src_idx view space
// will produce their coordinates in dst_idx view space
basis2s get_relative_transform(const test_data &data,
                              size_t src_idx, size_t dst_idx)
{
    basis2s accum = default_basis();
    if (dst_idx > src_idx)
    {
        for (size_t vi = dst_idx; vi != src_idx; --vi)
        {
            accum = basis_in_basis(accum,
                                   inverse_basis(data.views[vi].tr));
        }
    }
    else
    {
        for (size_t vi = dst_idx + 1; vi <= src_idx; ++vi)
        {
            accum = basis_in_basis(accum,
                                   data.views[vi].tr);
        }
    }
    return accum;
}

// given a pin (a view index) collect list of strokes
// along with needed transforms, that are inside viewport
// which is in pin's view local space
void query(size_t layer_id,
           const test_data &data, size_t pin, const box2 &viewport,
           std::vector<test_visible> &visibles,
           std::vector<basis2s> &transforms,
           float negligibledist)
{
    // ps_*** -- pin space
    // ls_*** -- current view's space

    for (size_t vi = 0; vi < data.nviews; ++vi)
    {
        if (data.views[vi].li == layer_id)
        {
            basis2s ls2ps = get_relative_transform(data, vi, pin);
            basis2s ps2ls = get_relative_transform(data, pin, vi);

            box2 ls_box = box_in_basis(ps2ls, viewport);
            float ls_negligible = dist_in_basis(ps2ls, negligibledist);
            crop(data, vi, transforms.size(), ls_box, visibles, ls_negligible);
            transforms.push_back(ls2ps);
        }
    }
}

box2 query_bbox(const test_data &data, size_t pin)
{
    box2 ps_accum_box;

    for (size_t vi = 0; vi < data.nviews; ++vi)
    {
        basis2s ls2ps = get_relative_transform(data, vi, pin);

        box2 ps_view_box = box_in_basis(ls2ps, data.views[vi].cached_bbox);
        ps_accum_box.add_box(ps_view_box);
    }

    return ps_accum_box;
}

float dist_to_seg(const float2 &p,
                  const float2 &p1, const float2 &p2)
{
    float2 d = p2 - p1;
    return ::fabsf(d.y * p.x - d.x * p.y + p2.x * p1.y - p2.y * p1.x) / len(d);
}

void ramer_douglas_peucker(const test_point* from, const test_point* to,
                           std::vector<test_point>& result, float epsilon)
{
    if (from == to)
    {
        result.push_back(*from);
        return;
    }

    if (to - from == 1)
    {
        result.push_back(*from);
        result.push_back(*to);
        return;
    }

    float max_dist = 0.0f;
    const test_point* max_pt = 0;
    float2 frompt{ from->x, from->y };
    float2 topt{ to->x, to->y };
    for (const test_point* p = from + 1; p != to; ++p)
    {
        // TODO: this may be actually not exactly what we need here
        float2 pt{ p->x, p->y };
        float d = dist_to_seg(pt, frompt, topt);

        if (d > max_dist)
        {
            max_pt = p;
            max_dist = d;
        }
    }

    if (max_dist > epsilon)
    {
        ramer_douglas_peucker(from, max_pt, result, epsilon);
        result.pop_back();
        ramer_douglas_peucker(max_pt, to, result, epsilon);
    }
    else
    {
        result.push_back(*from);
        result.push_back(*to);
    }
}

#include "fitbezier.cpp"

void sample_bezier(const std::vector<bezier4>& pieces,
                   std::vector<test_point>& pts)
{
    for (size_t i = 0; i < pieces.size(); ++i)
    {
        for (float t = 0.0f; t < 0.99f; t += 0.1f)
        {
            pts.push_back(test_point(pieces[i].eval(t), 1.0f));
        }
    }
    if (!pieces.empty())
    {
        pts.push_back(test_point(pieces.back().eval(1.0f), 1.0f));
    }
}

// N > 2
void smooth_stroke_2(const test_point* input, size_t N,
                     std::vector<test_point>& output,
                     float error)
{
    std::vector<bezier4> pieces;
    float2 t0 = {input[1].x - input[0].x,
                 input[1].y - input[0].y};
    t0 = t0 / len(t0);
    float2 t1 = {input[N-2].x - input[N-1].x,
                 input[N-2].y - input[N-1].y};
    t1 = t1 / len(t1);
    fit_bezier(input, 0, N, t0, t1, pieces, error);
    sample_bezier(pieces, output);
}

float2 hermite(const float2& p0, const float2& m0,
               const float2& p1, const float2& m1,
               float t)
{
    float t2 = t * t;
    float t3 = t2 * t;
    float h00 = 2.0f * t3 - 3.0f * t2 + 1.0f;
    float h10 = t3 - 2.0f * t2 + t;
    float h01 = -2.0f * t3 + 3.0f * t2;
    float h11 = t3 - t2;
    return h00 * p0 + h10 * m0 + h01 * p1 + h11 * m1;
}

float2 tangent2(const float2& p0, float t0,
                const float2& p1, float t1,
                const float2& p2, float t2)
{
    return 0.5f * ((p2 - p1) / (t2 - t1) + (p1 - p0) / (t1 - t0));
}

float2 tangent(const float2& p0, float t0,
               const float2& p1, float t1)
{
    return (p1 - p0) / (t1 - t0);
}

// N > 2
void smooth_stroke_1(const test_point* input, size_t N,
                     std::vector<test_point>& output)
{
    {
        float2 p0 = {input[0].x, input[0].y};
        float2 p1 = {input[1].x, input[1].y};
        float2 p2 = {input[2].x, input[2].y};
        float t0 = 0.0f;//input[0].t;
        float t1 = 1.0f;//input[1].t;
        float t2 = 2.0f;//input[2].t;
        float2 m0 = tangent(p0, t0, p1, t1);
        float2 m1 = tangent2(p0, t0, p1, t1, p2, t2);

        float w0 = input[0].w;
        float w1 = input[1].w;

        output.push_back(input[0]);
        output.push_back(test_point(hermite(p0, m0, p1, m1, 0.25f),
                                    lerp(w0, w1, 0.25f)));
        output.push_back(test_point(hermite(p0, m0, p1, m1, 0.50f),
                                    lerp(w0, w1, 0.50f)));
        output.push_back(test_point(hermite(p0, m0, p1, m1, 0.75f),
                                    lerp(w0, w1, 0.75f)));
    }

	// TODO: better tangents
    for (size_t i = 1; i <= N-3; ++i)
    {
        float2 p0 = {input[i-1].x, input[i-1].y};
        float2 p1 = {input[i+0].x, input[i+0].y};
        float2 p2 = {input[i+1].x, input[i+1].y};
        float2 p3 = {input[i+2].x, input[i+2].y};
        float t0 = i-1.0f;//input[i-1].w;
        float t1 = i+0.0f;//input[i+0].w;
        float t2 = i+1.0f;//input[i+1].w;
        float t3 = i+2.0f;//input[i+2].w;
        float2 m1 = tangent2(p0, t0, p1, t1, p2, t2);
        float2 m2 = tangent2(p1, t1, p2, t2, p3, t3);

        float w1 = input[i+0].w;
        float w2 = input[i+1].w;

        output.push_back(input[i]);
        output.push_back(test_point(hermite(p1, m1, p2, m2, 0.25f),
                                    lerp(w1, w2, 0.25f)));
        output.push_back(test_point(hermite(p1, m1, p2, m2, 0.50f),
                                    lerp(w1, w2, 0.50f)));
        output.push_back(test_point(hermite(p1, m1, p2, m2, 0.75f),
                                    lerp(w1, w2, 0.75f)));
    }

    {
        float2 p0 = {input[N-3].x, input[N-3].y};
        float2 p1 = {input[N-2].x, input[N-2].y};
        float2 p2 = {input[N-1].x, input[N-1].y};
        float t0 = N-3.0f;//input[N-3].t;
        float t1 = N-2.0f;//input[N-2].t;
        float t2 = N-1.0f;//input[N-1].t;
        float2 m1 = tangent2(p0, t0, p1, t1, p2, t2);
        float2 m2 = tangent(p1, t1, p2, t2);

        float w1 = input[N-2].w;
        float w2 = input[N-1].w;

        output.push_back(input[N-2]);
        output.push_back(test_point(hermite(p1, m1, p2, m2, 0.25f),
                                    lerp(w1, w2, 0.25f)));
        output.push_back(test_point(hermite(p1, m1, p2, m2, 0.50f),
                                    lerp(w1, w2, 0.50f)));
        output.push_back(test_point(hermite(p1, m1, p2, m2, 0.75f),
                                    lerp(w1, w2, 0.75f)));
        output.push_back(input[N-1]);
    }
}

void smooth_stroke(const test_point* input, size_t N,
                   std::vector<test_point>& output,
                   float error, bool ng)
{
    if (N < 3)
    {
        for (size_t i = 0; i < N; ++i)
        {
            output.push_back(input[i]);
        }
        return;
    }

    if (ng)
    {
        smooth_stroke_2(input, N, output, error);
    }
    else
    {
        smooth_stroke_1(input, N, output);
    }
}

box2 get_strokes_bbox(const test_data &data, size_t vi)
{
    box2 result;
    const test_view &v = data.views[vi];
    for (size_t si = v.si0; si < v.si1; ++si)
    {
        result.add_box(data.strokes[si].cached_bbox);
    }
    return result;
}

#include "tests.cpp"
