struct bezier4
{
    float2 p0;
    float2 p1;
    float2 p2;
    float2 p3;

    float2 eval(float t) const
    {
        float2 p01 = lerp(p0, p1, t);
        float2 p12 = lerp(p1, p2, t);
        float2 p23 = lerp(p2, p3, t);
        return lerp(lerp(p01, p12, t), lerp(p12, p23, t), t);
    }
};

struct bezier3
{
    float2 p0;
    float2 p1;
    float2 p2;

    float2 eval(float t) const
    {
        return lerp(lerp(p0, p1, t), lerp(p1, p2, t), t);
    }
};

struct bezier2
{
    float2 p0;
    float2 p1;

    float2 eval(float t) const
    {
        return lerp(p0, p1, t);
    }
};

float B0(float u)
{
    float t = 1.0f - u;
    return t * t * t;
}

float B1(float u)
{
    float t = 1.0f - u;
    return 3.0f * u * t * t;
}

float B2(float u)
{
    float t = 1.0f - u;
    return 3.0f * u * u * t;
}

float B3(float u)
{
    return u * u * u;
}

float newton_raphson_root(const bezier4& Q, const float2& P, float u)
{
    bezier3 Q1;
    Q1.p0 = (Q.p1 - Q.p0) * 3.0f;
    Q1.p1 = (Q.p2 - Q.p1) * 3.0f;
    Q1.p2 = (Q.p3 - Q.p2) * 3.0f;

    bezier2 Q2;
    Q2.p0 = (Q1.p1 - Q1.p0) * 2.0f;
    Q2.p1 = (Q1.p2 - Q1.p1) * 2.0f;

    float2 Q_u = Q.eval(u);
    float2 Q1_u = Q1.eval(u);
    float2 Q2_u = Q2.eval(u);

    float numer = dot(Q_u - P, Q1_u);
    float denom = dot(Q1_u, Q1_u) + dot(Q_u - P, Q2_u);

    if (denom == 0.0f)
        return u;

    return u - numer/denom;
}

std::vector<float> chord_length_parametrize(const test_point* pts,
                                            size_t from, size_t to)
{
    std::vector<float> r;
    r.resize(to - from);

    float total = 0.0f;
    r[0] = total;
    for (size_t i = from + 1; i < to; ++i)
    {
        float2 d = {pts[i].x - pts[i-1].x,
                    pts[i].y - pts[i-1].y};
        total += len(d);
        r[i-from] = total;
    }

    for (size_t i = from + 1; i < to; ++i)
    {
        r[i-from] /= total;
    }

    return r;
}

std::vector<float> reparametrize(const test_point* pts,
                                 size_t from, size_t to,
                                 const std::vector<float>& u,
                                 const bezier4& curve)
{
    std::vector<float> r;
    r.resize(to - from);

    for (size_t i = from; i < to; ++i)
    {
        r[i-from] = newton_raphson_root(curve, {pts[i].x, pts[i].y}, u[i-from]);
    }

    return r;
}

bezier4 generate_bezier(const test_point* pts, size_t from, size_t to,
                        const std::vector<float>& u,
                        const float2& t1, const float2& t2)
{
    static std::vector<float2> A[2];
    size_t npts = to - from;
    A[0].resize(npts);
    A[1].resize(npts);

    for (size_t i = 0; i < npts; ++i)
    {
        A[0][i] = t1 * B1(u[i]);
        A[1][i] = t2 * B2(u[i]);
    }

    float C[2][2] = {{0.0f, 0.0f}, {0.0f, 0.0f}};
    float X[2] = {0.0f, 0.0f};


    float2 P0{pts[from].x, pts[from].y};
    float2 P1{pts[to-1].x, pts[to-1].y};
    
    for (size_t i = 0; i < npts; ++i)
    {
        C[0][0] += dot(A[0][i], A[0][i]);
        C[0][1] += dot(A[0][i], A[1][i]);
        C[1][0] += dot(A[1][i], A[0][i]);
        C[1][1] += dot(A[1][i], A[1][i]);

		float2 tmp = float2{pts[from+i].x, pts[from+i].y};
        tmp = tmp - (P0 * B0(u[i]) + P0 * B1(u[i]) +
                     P1 * B2(u[i]) + P1 * B3(u[i]));

        X[0] += dot(A[0][i], tmp);
        X[1] += dot(A[1][i], tmp);
    }

    float det_C0_C1 = C[0][0] * C[1][1] - C[1][0] * C[0][1];
    float det_C0_X  = C[0][0] * X[1]    - C[1][0] * X[0];
    float det_X_C1  = X[0]    * C[1][1] - X[1]    * C[0][1];

    float alpha_l = (det_C0_C1 == 0.0f) ? 0.0f : det_X_C1 / det_C0_C1;
    float alpha_r = (det_C0_C1 == 0.0f) ? 0.0f : det_C0_X / det_C0_C1;

    float segLength = len(P1 - P0);
    float epsilon = 1e-6f * segLength;

    bezier4 result;

    if (alpha_l < epsilon || alpha_r < epsilon)
    {
		float dist = segLength / 3.0f;
		result.p0 = P0;
		result.p1 = P0 + t1 * dist;
        result.p2 = P1 + t2 * dist;
		result.p3 = P1;
		return result;
    }

    result.p0 = P0;
    result.p1 = P0 + t1 * alpha_l;
    result.p2 = P1 + t2 * alpha_r;
    result.p3 = P1;
    return result;
}

struct error_pos
{
    float error;
    size_t idx;
};

error_pos compute_max_error(const test_point* pts, size_t from, size_t to,
                            const bezier4& curve,
                            const std::vector<float>& u)
{
    error_pos ep;
    ep.error = 0.0f;
    ep.idx = (to + from) / 2;

    for (size_t i = from; i < to; ++i)
    {
        float2 p = curve.eval(u[i-from]);
        float2 d = p - float2{pts[i].x, pts[i].y};
        float error = dot(d, d);
        if (error >= ep.error)
        {
            ep.error = error;
            ep.idx = i;
        }
    }
    
    return ep;
}

const size_t MAX_ITERATIONS = 4;

// t0, t1 -- unit length
// range is [from, to)
void fit_bezier(const test_point* pts, size_t from, size_t to,
                const float2& t0, const float2& t1,
                std::vector<bezier4>& pieces,
                float error)
{
    if (to - from == 2)
    {
        float2 P0{pts[from].x, pts[from].y};
        float2 P1{pts[to-1].x, pts[to-1].y};
        float dist = len(P1 - P0);

        bezier4 curve;
        curve.p0 = P0;
        curve.p1 = P0 + t0 * dist / 3.0f;
        curve.p2 = P1 + t1 * dist / 3.0f;
        curve.p3 = P1;
        pieces.push_back(curve);
        return;
    }

    std::vector<float> u = chord_length_parametrize(pts, from, to);
    bezier4 curve = generate_bezier(pts, from, to, u, t0, t1);

    error_pos ep = compute_max_error(pts, from, to, curve, u);
    if (ep.error < error)
    {
        pieces.push_back(curve);
        return;
    }

    if (ep.error < error + error)
    {
        for (size_t i = 0; i < MAX_ITERATIONS; ++i)
        {
            u = reparametrize(pts, from, to, u, curve);
            curve = generate_bezier(pts, from, to, u, t0, t1);
            ep = compute_max_error(pts, from, to, curve, u);
            if (ep.error < error)
            {
                pieces.push_back(curve);
                return;
            }
        }
    }

    // only split
    float2 t_split {pts[ep.idx+1].x - pts[ep.idx-1].x,
                    pts[ep.idx+1].y - pts[ep.idx-1].y};
    t_split = t_split / len(t_split);

    fit_bezier(pts, from, ep.idx+1, t0, -t_split, pieces, error);
    fit_bezier(pts, ep.idx, to, t_split, t1, pieces, error);
}
