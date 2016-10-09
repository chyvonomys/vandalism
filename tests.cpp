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

bool points_eq(const test_point &a, const test_point &b)
{
    return iszero({a.x - b.x, a.y - b.y});
}

bool transforms_test()
{
    /*
    test_point p = {-6, 2};

    test_transform t1 = {0.5f, 2.0f, 2.0f, 0.0f};
    test_transform t2 = {4.0f, -1.0f, -5.0f, 0.0f};

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
    */
    return true;
}


test_point t_points1[] =
{
    {-1.0f, -1.0f}, {1.0f, 1.0f},
    {-1.0f, -1.0f}, {1.0f, 1.0f}
};

test_stroke t_strokes1[] =
{
    {0, 2,   0,   box2()},
    {2, 4,   0,   box2()}
};

test_view t_views1[] =
{
    test_view(make_pan({0.0f, 0.0f}), 0, 1, 0),
    test_view(make_pan({1.0f, 0.0f}), 1, 2, 0)
};
const size_t NVIEWS1 = 2;
const size_t PIN1 = 1;

test_data test1 = {t_points1, t_strokes1, nullptr, t_views1, NVIEWS1};

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
    { 0,  2,   0,   box2()},
    { 2,  4,   0,   box2()},
    { 4,  6,   0,   box2()},
    { 6,  8,   0,   box2()},
    { 8, 10,   0,   box2()},
    {10, 12,   0,   box2()},
    {12, 14,   0,   box2()},
    {14, 16,   0,   box2()}
};

test_view t_views2[] =
{
    test_view(make_pan({0.0f, 0.0f}), 0, 2, 0),
    test_view(make_pan({5.0f, 0.0f}), 2, 4, 0),
    test_view(make_pan({5.0f, 0.0f}), 4, 6, 0),
    test_view(make_pan({-5.0f, 0.0f}), 6, 6, 0),
    test_view(make_zoom(1.0f, 5.0f), 6, 8, 0),
    test_view(make_pan({-1.0f, 0.0f}), 8, 8, 0),
    test_view(make_zoom(5.0f, 3.0f), 8, 8, 0)
};
const size_t NVIEWS2 = 7;
const size_t PIN2 = 6;

test_data test2 = {t_points2, t_strokes2, nullptr, t_views2, NVIEWS2};

test_point t_points3[] =
{
    {0.0f, -0.5f}, {0.0f, 0.5f},
    {1.5f, -0.5f}, {1.5f, 0.5f}
};

test_stroke t_strokes3[] =
{
    {0, 2,   0,   box2()}
};

test_view t_views3[] =
{
    test_view(make_pan({0.0f, 0.0f}), 0, 1, 0),
    test_view(make_zoom(2.0f, 1.0f), 1, 1, 0)
};
const size_t NVIEWS3 = 2;
const size_t PIN3 = 1;

test_data test3 = {t_points3, t_strokes3, nullptr, t_views3, NVIEWS3};

bool cfg_run_tests = false;

bool run_tests()
{
    if (cfg_run_tests)
    {
        bool lb_test_result = liang_barsky_test();
        bool t_test_result = transforms_test();
        return lb_test_result && t_test_result;
    }
    return true;
}

bool test_result = run_tests();
