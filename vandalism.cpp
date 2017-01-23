#include <vector>
#include <iterator>
#include <fstream>
#include <sstream>
#include <iostream>
#include "cascade.cpp"

enum Undoee
{
    NOTHING,
    STROKE,
    IMAGE,
    VIEW
};

struct Masterpiece
{
public:
    typedef test_point Point;
    typedef test_stroke Stroke;
    typedef test_image Image;
    typedef test_view View;

    struct Brush
    {
        float diameter;
        float angle;
        float spread;
        color4f color;
        bool type;
    };

private:
    std::vector<Point> points;
    std::vector<Stroke> strokes;
    std::vector<View> views;
    std::vector<Image> images;
    std::vector<Brush> brushes;

private:
    void append_new_view(const basis2s &basis, size_t layeridx, bool layerlocal)
    {
        views.push_back(View(basis,
                             strokes.size(), strokes.size(),
                             layeridx, layerlocal));
    }

public:
    void clear()
    {
        points.clear();
        strokes.clear();
        views.clear();
        images.clear();
        brushes.clear();
    }

    void reset()
    {
        clear();
        views.push_back(View(make_pan({ 0.0f, 0.0f }), 0, 0, 0, false));
    }

    bool load(const char *filename, std::vector<std::string> &imgNames)
    {
        clear();

        std::ifstream is(filename);
        std::string line;

        bool ok = true;
        size_t lineNum = 0;

        while (ok && std::getline(is, line))
        {
            ++lineNum;
            if (!line.empty())
            {
                if (line.empty())
                {
                    ok = false;
                }
                else if (*line.c_str() == 'p')
                {
                    std::istringstream ss(line);
                    ss.seekg(2);
                    Point pt;
                    ok = !(ss >> pt.x >> pt.y >> pt.w).fail();
                    if (ok)
                    {
                        points.push_back(pt);
                    }
                }
                else if (*line.c_str() == 's')
                {
                    std::istringstream ss(line);
                    ss.seekg(2);
                    Stroke st;
                    ok = !(ss >> st.pi0 >> st.pi1 >> st.brush_id).fail();
                    if (ok)
                    {
                        strokes.push_back(st);
                    }
                }
                else if (*line.c_str() == 'v')
                {
                    std::istringstream ss(line);
                    ss.seekg(2);
                    View vi(make_pan({ 0.0f, 0.0f }), 0, 0, 0, false);
                    i32 imgidx;
                    ok = !(ss >> vi.tr.o.x >> vi.tr.o.y >> vi.tr.x.x >> vi.tr.x.y
                           >> vi.si0 >> vi.si1 >> imgidx >> vi.li).fail();
                    bool layerlocal = false;
                    i32 ll;
                    if (!(ss >> ll).fail())
                    {
                        layerlocal = (ll > 0);
                    }
                    if (ok)
                    {
                        vi.ii = (imgidx == -1 ? NPOS : static_cast<u32>(imgidx));
                        vi.ll = layerlocal;
                        views.push_back(vi);
                    }
                }
                else if (*line.c_str() == 'b')
                {
                    std::istringstream ss(line);
                    ss.seekg(2);
                    Brush br;
                    ok = !(ss >> br.type >> br.diameter
                           >> br.color.r >> br.color.g >> br.color.b >> br.color.a).fail();
                    if (ok)
                    {
                        brushes.push_back(br);
                    }
                }
                else if (*line.c_str() == '#')
                {
                    // skip
                }
                else if (*line.c_str() == 'i')
                {
                    size_t firstq = 0;
                    while (firstq < line.size())
                    {
                        if (line[firstq] == '\"') break;
                        ++firstq;
                    }
                    size_t secondq = firstq + 1;
                    while (secondq < line.size())
                    {
                        if (line[secondq] == '\"') break;
                        ++secondq;
                    }

                    if (firstq < line.size() && secondq < line.size())
                    {
                        std::string name;
                        name.assign(&line[firstq + 1], &line[secondq]);

                        std::istringstream ss(line);
                        ss.seekg(static_cast<std::streamoff>(secondq + 1));

                        Image img;
                        ok = !(ss >> img.basis.o.x >> img.basis.o.y
                               >> img.basis.x.x >> img.basis.x.y >> img.basis.y.x >> img.basis.y.y).fail();
                        if (ok)
                        {
                            img.nameidx = 0;
                            for (; img.nameidx < imgNames.size(); ++img.nameidx)
                            {
                                if (imgNames[img.nameidx] == name)
                                    break;
                            }
                            if (img.nameidx == imgNames.size())
                            {
                                // 'did not found'
                                imgNames.push_back(name);
                            }
                            images.push_back(img);
                        }
                    }
                    else
                    {
                        ok = false;
                    }
                }
                else
                {
                    ok = false;
                }
            }
        }

        // TODO: validation

        // calculate bboxes of strokes
        for (size_t si = 0; si < strokes.size(); ++si)
        {
            Stroke& ns = strokes[si];
            for (size_t pi = ns.pi0; pi < ns.pi1; ++pi)
            {
                ns.cached_bbox.add({ points[pi].x, points[pi].y });
            }
            // TODO: invalid for non circular brushes
            // diameter should be max bbox of drawn point shape
            ns.cached_bbox.grow(0.5f * brushes[ns.brush_id].diameter);
        }

        // calculate bboxes of views
        for (size_t vi = 0; vi < views.size(); ++vi)
        {
            views[vi].cached_bbox = get_strokes_bbox(get_data(), vi);
            if (views[vi].has_image())
            {
                views[vi].cached_imgbbox = images[views[vi].ii].get_bbox();
            }
        }

        return ok;
    }
    
    void save(const char *filename, const std::vector<std::string> &imgNames) const
    {
        std::ofstream os(filename);

        for (size_t i = 0; i < points.size(); ++i)
        {
            os << "p " << points[i].x << ' ' << points[i].y << ' ' << points[i].w << '\n';
        }
        for (size_t i = 0; i < strokes.size(); ++i)
        {
            os << "s " << strokes[i].pi0 << ' ' << strokes[i].pi1
               << ' ' << strokes[i].brush_id << ' ' << '\n';
        }
        for (size_t i = 0; i < views.size(); ++i)
        {
            os << "v " << views[i].tr.o.x << ' ' << views[i].tr.o.y
               << ' ' << views[i].tr.x.x << ' ' << views[i].tr.x.y
               << ' ' << views[i].si0 << ' ' << views[i].si1
               << ' ' << (!views[i].has_image() ? -1 : static_cast<i32>(views[i].ii))
               << ' ' << views[i].li
               << ' ' << (views[i].ll ? '1' : '0')
               << '\n';
        }
        // TODO: maybe separate codes for regular brush, eraser, etc
        for (size_t i = 0; i < brushes.size(); ++i)
        {
            os << "b " << brushes[i].type << ' ' << brushes[i].diameter << ' '
               << brushes[i].color.r << ' ' << brushes[i].color.g << ' '
               << brushes[i].color.b << ' ' << brushes[i].color.a << '\n';
        }
        // TODO: maybe reuse 'basis' vectors approach here and in views
        for (size_t i = 0; i < images.size(); ++i)
        {
            os << "i \"" << imgNames[images[i].nameidx] << '\"'
                << ' ' << images[i].basis.o.x << ' ' << images[i].basis.o.y
                << ' ' << images[i].basis.x.x << ' ' << images[i].basis.x.y
                << ' ' << images[i].basis.y.x << ' ' << images[i].basis.y.y << '\n';
        }
    }

    Undoee check_undo(bool really)    
    {
        // TODO: view may now contain image, fix these conditions
        if (views.back().si1 == views.back().si0)
        {
            if (views.size() > 1)
            {
                if (!views.back().has_image())
                {
                    if (really)
                    {
                        // TODO: if we maintain parallel view in all layers,
                        // then we pop back in other layers?
                        views.pop_back();
                    }
                    return VIEW;
                }
                else
                {
                    if (really)
                    {
                        images.pop_back();
                        views.back().ii = NPOS;
                    }
                    return IMAGE;
                }
            }
        }
        else if (views.back().has_strokes())
        {
            if (really)
            {
                size_t deleteCnt = strokes.back().pi1 - strokes.back().pi0;
                for (size_t i = 0; i < deleteCnt; ++i)
                {
                    points.pop_back();
                }
                strokes.pop_back();
                --views.back().si1;
                size_t vi = views.size() - 1;
                views.back().cached_bbox = get_strokes_bbox(get_data(), vi);
            }
            return STROKE;
        }
        return NOTHING;
    }

    void change_view(const basis2s &basis, size_t layeridx)
    {
        // TODO: combination of views
        append_new_view(basis, layeridx, false);
    }

    void change_layer_view(const basis2s &basis, size_t layeridx)
    {
        // TODO: combination of views
        append_new_view(basis, layeridx, true);
    }

    void place_image(size_t imageId, const basis2r &basis, size_t layeridx)
    {
        append_new_view(make_pan({ 0.0f, 0.0f }), layeridx, false);

        Image i = { imageId, basis };

        // TODO: why do we keep imgbbox?
        views.back().cached_imgbbox = i.get_bbox();
        views.back().ii = images.size();
        images.push_back(i);
    }

    void add_stroke(size_t layeridx, const Brush &brush, const Stroke &newStroke, const std::vector<Point> &newPoints)
    {
        // TODO: keep only unique?
        if (brushes.empty() ||
            ::memcmp(&brushes.back(), &brush, sizeof(Brush)) != 0)
        {
            brushes.push_back(brush);
        }

        if (views.back().li != layeridx)
        {
            append_new_view(make_pan({ 0.0f, 0.0f }), layeridx, false);
        }

        strokes.push_back(newStroke);
        strokes.back().cached_bbox.grow(0.5f * brush.diameter);
        strokes.back().brush_id = brushes.size() - 1;

        strokes.back().pi0 = points.size();
        std::copy(newPoints.cbegin(), newPoints.cend(),
                  std::back_inserter(points));
        strokes.back().pi1 = points.size();

        views.back().cached_bbox.add_box(strokes.back().cached_bbox);
        views.back().si1 += 1;
    }

    test_data get_data() const
    {
        test_data result =
        {
            points.data(),
            strokes.data(),
            images.data(),
            views.data(),
            views.size()
        };
        return result;
    }

    const Brush &get_brush(size_t i) const { return brushes[i]; }

    size_t num_points() const { return points.size(); }
    size_t num_strokes() const { return strokes.size(); }
    size_t num_views() const { return views.size(); }
    size_t num_images() const { return images.size(); }
    size_t num_brushes() const { return brushes.size(); }
};

// TODO: commited stroke count, non-commited stroke count (for avoiding re-baking after each new stroke)

struct Vandalism
{
    Masterpiece art;

    size_t currentView;

    std::vector<Masterpiece::Point> currentPoints;
    Masterpiece::Stroke currentStroke;
    Masterpiece::Brush currentBrush;

    // TODO: change this
    bool visiblesChanged;
    bool currentChanged;

    // PAN related
    float2 panStart;
    float2 preShift;
    float2 postShift;

    // ZOOM related
    float zoomStartX;
    float zoomCoeff;

    // ROTATE related
    float rotateStartX;
    float rotateAngle;

    // FIRST/SECOND related
    float2 firstPos;
    float2 secondPos0;
    float2 secondPos1;

    // SCROLL related
    float scrollY0;

    // FIT related
    basis2r fitBasis;

    enum TopLevelModeId
    {
        TL_DRAWING   = 0,
        TL_PANNING   = 1,
        TL_ZOOMING   = 2,
        TL_ROTATING  = 3,
        TL_PLACING1  = 4,
        TL_MOVING2   = 5,
        TL_SCROLLING = 6,
        TL_FITTING   = 7,
        TL_MODECNT   = 8,
        TL_IDLE      = TL_MODECNT
    };

    enum FittingSubmodeId
    {
        FS_LOADING   = 0,
        FS_MOVING0   = 1,
        FS_MOVINGX   = 2,
        FS_ACCEPTING = 3,
        FS_MODECNT   = 4,
        FS_IDLE      = FS_MODECNT
    };

    enum Tool
    {
        DRAW   = 0,
        ERASE  = 1,
        PAN    = 2,
        ZOOM   = 3,
        ROTATE = 4,
        FIRST  = 5,
        SECOND = 6,
        FITIMG = 7
    };

    enum FitTool
    {
        FT_START  = 0,
        FT_LOAD   = 1,
        FT_ADJUST = 2,
        FT_ACCEPT = 3
    };

    enum FitGizmo
    {
        FG_NONE   = 0,
        FG_0POINT = 1,
        FG_XPOINT = 2
    };

    enum Smooth
    {
        NONE      = 0,
        HERMITE   = 1,
        FITBEZIER = 2
    };

    struct Input
    {
        float2 mousePos;
        float scrolly;
        bool scrolling;
        bool mousedown;
        bool fakepressure;
        bool simplify;
        Smooth smooth;
        Tool tool;
        color4f brushcolor;
        float brushdiameter;
        float brushspread;
        float brushangle;
        float eraseralpha;
        float negligibledistance;
        u32 fitimageid;
        float fitimageaspectratio;
        FitTool fittool;
        FitGizmo fitgizmo;
        u8 currentlayer;
    };

    typedef void (Vandalism::*CB_FN)(const Input *);
    typedef bool (Vandalism::*PR_FN)(const Input *);

    void set_dirty()
    {
        visiblesChanged = true;
        currentChanged = true;
    }

    bool current_view_is_last() const
    {
        return currentView + 1 == art.num_views();
    }

    bool append_allowed() const
    {
        return current_view_is_last();
    }

    void remove_alterations()
    {
        preShift = { 0.0f, 0.0f };
        postShift = { 0.0f, 0.0f };

        zoomCoeff = 1.0f;
        rotateAngle = 0.0f;
    }

    void start_rotate(const Input *input)
    {
        rotateStartX = input->mousePos.x;
    }
    
    void done_rotate(const Input *input)
    {
        if (append_allowed())
        {
            float dx = input->mousePos.x - rotateStartX;
            float angle = si_pi * dx;

            art.change_view(make_rotate(-angle), input->currentlayer);
            currentView = art.num_views() - 1;
        }

        common_done();
    }
    
    void do_rotate(const Input *input)
    {
        float dx = input->mousePos.x - rotateStartX;
        rotateAngle = si_pi * dx;
    }

    void start_zoom(const Input *input)
    {
        zoomStartX = input->mousePos.x;
    }

    void do_zoom(const Input *input)
    {
        zoomCoeff = input->mousePos.x / zoomStartX;
    }

    void done_zoom(const Input *input)
    {
        if (append_allowed())
        {
            art.change_view(make_zoom(input->mousePos.x, zoomStartX),
                            input->currentlayer);
            currentView = art.num_views() - 1;
        }

        common_done();
    }

    void start_pan(const Input *input)
    {
        panStart = input->mousePos;
    }

    void do_pan(const Input *input)
    {
        postShift = input->mousePos - panStart;
    }

    void done_pan(const Input *input)
    {
        if (append_allowed())
        {
            art.change_view(make_pan(panStart - input->mousePos),
                            input->currentlayer);
            currentView = art.num_views() - 1;
        }
        common_done();
    }

    float pressure_func(size_t i)
    {
        return (i > 100 ? 0.0f : (100.0f - i) / 100.0f);
    }

    void start_draw(const Input *input)
    {
        float2 pos = input->mousePos;
        Masterpiece::Point point;
        point.x = pos.x;
        point.y = pos.y;
        point.w = input->fakepressure ? pressure_func(0) : 1.0f;

        currentPoints.clear();
        currentPoints.push_back(point);
        currentStroke.pi0 = 0;
        currentStroke.pi1 = 1;
        currentStroke.cached_bbox = box2();
        currentStroke.cached_bbox.add(pos);
        currentStroke.brush_id = NPOS;

        currentBrush.color = input->brushcolor;
        if (input->tool == ERASE)
        {
            currentBrush.color.a = input->eraseralpha;
        }

        currentBrush.type = (input->tool == ERASE ? 1 : 0);
        currentBrush.diameter = input->brushdiameter;
        currentBrush.angle = input->brushangle;
        currentBrush.spread = input->brushspread;
    }

    void do_draw(const Input *input)
    {
        float2 pos = input->mousePos;
        Masterpiece::Point point;
        point.x = pos.x;
        point.y = pos.y;

        float dx = point.x - currentPoints.back().x;
        float dy = point.y - currentPoints.back().y;
        float eps = input->negligibledistance * input->negligibledistance;
        if (dx * dx + dy * dy > eps)
        {
            size_t ptIdx = currentPoints.size();
            point.w = input->fakepressure ? pressure_func(ptIdx) : 1.0f;
            currentPoints.push_back(point);
            currentStroke.cached_bbox.add(pos);
            currentStroke.pi1 += 1;
            currentChanged = true;
        }
    }

    // TODO: is point in done_* the same as in previous do_* if there was any?

    void done_draw(const Input *input)
    {
        if (append_allowed())
        {
            if (input->simplify)
            {
                std::vector<test_point> simplified;
                ramer_douglas_peucker(currentPoints.data(),
                    currentPoints.data() +
                    currentPoints.size() - 1,
                    simplified,
                    0.1f * currentBrush.diameter);
                std::cout << "simplify " << currentPoints.size() << " -> " << simplified.size() << std::endl;
                currentPoints = simplified;
            }

            if (input->smooth == FITBEZIER ||
                input->smooth == HERMITE)
            {
                std::vector<test_point> smoothed;
                smooth_stroke(currentPoints.data(),
                    currentPoints.size(),
                    smoothed, input->negligibledistance,
                    input->smooth == FITBEZIER);
                currentPoints = smoothed;
            }

            art.add_stroke(input->currentlayer, currentBrush,
                           currentStroke, currentPoints);
            currentView = art.num_views() - 1;
        }

        currentStroke.pi0 = 0;
        currentStroke.pi1 = 0;
        currentStroke.cached_bbox = box2();

        common_done();
    }

    void place1(const Input *input)
    {
        firstPos = input->mousePos;
    }

    void start_move2(const Input *input)
    {
        secondPos0 = input->mousePos;
    }

    void do_move2(const Input *input)
    {
        secondPos1 = input->mousePos;

        float2 d0 = secondPos0 - firstPos;
        float2 d1 = secondPos1 - firstPos;

        float a0 = static_cast<float>(::atan2f(d0.y, d0.x));
        float a1 = static_cast<float>(::atan2f(d1.y, d1.x));

        preShift = -firstPos;
        postShift = firstPos;

        zoomCoeff = len(d1) / len(d0);
        rotateAngle = a1 - a0;
    }

    void done_move2(const Input *input)
    {
        if (append_allowed())
        {
            secondPos1 = input->mousePos;

            float2 d0 = secondPos0 - firstPos;
            float2 d1 = secondPos1 - firstPos;

            float a0 = static_cast<float>(::atan2f(d0.y, d0.x));
            float a1 = static_cast<float>(::atan2f(d1.y, d1.x));

            art.change_view(make_pan(firstPos), input->currentlayer);
            art.change_view(make_rotate(a0 - a1), input->currentlayer);
            art.change_view(make_zoom(len(d1), len(d0)), input->currentlayer);
            art.change_view(make_pan(-firstPos), input->currentlayer);
            currentView = art.num_views() - 1;
        }

        common_done();
    }

    void start_scroll(const Input *input)
    {
        scrollY0 = input->scrolly;
        preShift = -input->mousePos;
        postShift = input->mousePos;
    }

    void do_scroll(const Input *input)
    {
        float scrollAmount = input->scrolly - scrollY0;
        zoomCoeff = powf(2.0f, scrollAmount);
    }

    void done_scroll(const Input *input)
    {
        float scrollAmount = input->scrolly - scrollY0;
        zoomCoeff = powf(2.0f, scrollAmount);

        if (append_allowed())
        {
            art.change_view(make_pan(postShift), input->currentlayer);
            art.change_view(make_zoom(zoomCoeff, 1.0f), input->currentlayer);
            art.change_view(make_pan(preShift), input->currentlayer);
            currentView = art.num_views() - 1;
        }

        common_done();
    }

    void empty(const Input *) {}

    void load_img(const Input *input)
    {
        // adjust fit basis accordingly to new image (aspect ratio, position, size)
        // should be a 'leave_fn' because at 'enter stage' fit image fields are not yet populated
        fitBasis.y = perp(fitBasis.x) * input->fitimageaspectratio;
    }

    void move0(const Input *input)
    {
        fitBasis.o = input->mousePos;
    }

    void movex(const Input *input)
    {
        fitBasis =
        {
            fitBasis.o,
            input->mousePos - fitBasis.o,
            perp(input->mousePos - fitBasis.o) * input->fitimageaspectratio
        };
    }

    void accept_img(const Input *input)
    {
        if (append_allowed())
        {
            art.place_image(input->fitimageid, fitBasis, input->currentlayer);
            currentView = art.num_views() - 1;
        }

        set_dirty();
    }
    
    void common_done()
    {
        remove_alterations();
        set_dirty();
    }

    struct Mode
    {
        CB_FN enter_fn;
        CB_FN leave_fn;
        CB_FN stay_fn;
        PR_FN check_fn;
    };

    bool check_draw(const Input *input) { return input->mousedown && (input->tool == DRAW || input->tool == ERASE); }
    bool check_pan(const Input *input) { return input->mousedown && input->tool == PAN; }
    bool check_zoom(const Input *input) { return input->mousedown && input->tool == ZOOM; }
    bool check_rotate(const Input *input) { return input->mousedown && input->tool == ROTATE; }
    bool check_place1(const Input *input) { return input->mousedown && input->tool == FIRST; }
    bool check_move2(const Input *input) { return input->mousedown && input->tool == SECOND; }
    bool check_scroll(const Input *input) { return input->scrolling; }

    bool check_load_img(const Input *input) { return input->fittool == FT_LOAD; }
    bool check_move0(const Input *input) { return input->mousedown && input->fittool == FT_ADJUST && input->fitgizmo == FG_0POINT; }
    bool check_movex(const Input *input) { return input->mousedown && input->fittool == FT_ADJUST && input->fitgizmo == FG_XPOINT; }
    bool check_accept_img(const Input *input) { return input->fittool == FT_ACCEPT; }

    bool check_fitting(const Input *input)
    {
        if (input->tool != FITIMG)
        {
            switch_modes<FittingSubmodeId>(input, fitting_modes, FS_MODECNT, currentFittingMode, FS_IDLE);
            return false;
        }

        currentFittingMode = update_modes<FittingSubmodeId>(input, fitting_modes, FS_MODECNT, currentFittingMode);
        return true;
    }

    template <typename M>
    void switch_modes(const Input *input, Mode *modes, M modecnt, M from, M to)
    {
        if (from != modecnt) (this->*modes[from].leave_fn)(input);
        if (to != modecnt) (this->*modes[to].enter_fn)(input);
    }

    template <typename M>
    M update_modes(const Input *input, Mode *modes, M modecnt, M input_mode)
    {
        // NOTE: modecnt == idle in the set
        if (input_mode != modecnt && (this->*modes[input_mode].check_fn)(input))
        {
            // still in current mode
            (this->*modes[input_mode].stay_fn)(input);
            return input_mode;
        }
        else
        {
            // figure out what mode we are in
            for (u32 mi = 0; mi < static_cast<u32>(modecnt); ++mi)
            {
                if ((this->*modes[mi].check_fn)(input))
                {
                    switch_modes<M>(input, modes, modecnt, input_mode, static_cast<M>(mi));
                    return static_cast<M>(mi);
                }
            }
            switch_modes<M>(input, modes, modecnt, input_mode, modecnt);
            return modecnt;
        }
    }

    Mode toplevel_modes[TL_MODECNT];
    Mode fitting_modes[FS_MODECNT];
    TopLevelModeId currentMode;
    FittingSubmodeId currentFittingMode;

    void update(Input *input)
    {
        currentMode = update_modes<TopLevelModeId>(input, toplevel_modes, TL_MODECNT, currentMode);
    }

    void setup()
    {
        // TODO: this should be statically initialized once (works in clang)
        // doesn't work in vs2013
        toplevel_modes[TL_DRAWING]   = {&Vandalism::start_draw,   &Vandalism::done_draw,   &Vandalism::do_draw,   &Vandalism::check_draw};
        toplevel_modes[TL_PANNING]   = {&Vandalism::start_pan,    &Vandalism::done_pan,    &Vandalism::do_pan,    &Vandalism::check_pan};
        toplevel_modes[TL_ZOOMING]   = {&Vandalism::start_zoom,   &Vandalism::done_zoom,   &Vandalism::do_zoom,   &Vandalism::check_zoom};
        toplevel_modes[TL_ROTATING]  = {&Vandalism::start_rotate, &Vandalism::done_rotate, &Vandalism::do_rotate, &Vandalism::check_rotate};
        toplevel_modes[TL_PLACING1]  = {&Vandalism::place1,       &Vandalism::place1,      &Vandalism::place1,    &Vandalism::check_place1};
        toplevel_modes[TL_MOVING2]   = {&Vandalism::start_move2,  &Vandalism::done_move2,  &Vandalism::do_move2,  &Vandalism::check_move2};
        toplevel_modes[TL_SCROLLING] = {&Vandalism::start_scroll, &Vandalism::done_scroll, &Vandalism::do_scroll, &Vandalism::check_scroll};
        toplevel_modes[TL_FITTING]   = {&Vandalism::empty,        &Vandalism::empty,       &Vandalism::empty,     &Vandalism::check_fitting};

        currentMode = TL_IDLE;

        fitting_modes[FS_LOADING]    = {&Vandalism::empty,        &Vandalism::load_img,    &Vandalism::empty,     &Vandalism::check_load_img};
        fitting_modes[FS_MOVING0]    = {&Vandalism::move0,        &Vandalism::move0,       &Vandalism::move0,     &Vandalism::check_move0};
        fitting_modes[FS_MOVINGX]    = {&Vandalism::movex,        &Vandalism::movex,       &Vandalism::movex,     &Vandalism::check_movex};
        fitting_modes[FS_ACCEPTING]  = {&Vandalism::accept_img,   &Vandalism::empty,       &Vandalism::empty,     &Vandalism::check_accept_img};

        currentFittingMode = FS_IDLE;

        // TODO: look for other missing initializations
        currentStroke.pi0 = 0;
        currentStroke.pi1 = 0;

        firstPos = { 0.0f, 0.0f };
        fitBasis = { {0.0f, 0.0f}, {1.0f, 0.0f}, {0.0f, 1.0f} };

        remove_alterations();
        clear();
    }

    const Masterpiece::Brush& get_current_brush() const
    {
        return currentBrush;
    }

    // TODO: does this need to be here?
    size_t get_current_stroke_id() const
    {
        return art.num_strokes();
    }

    test_data get_bake_data() const
    {
        return art.get_data();
    }

    test_data get_current_data() const
    {
        test_data result =
        {
            currentPoints.data(),
            &currentStroke,
            nullptr,
            nullptr,
            0
        };

        return result;
    }

    Undoee undoable()
    {
        return check_undo(false);
    }

    void undo()
    {
        if (check_undo(true) != NOTHING)
        {
            currentView = art.num_views() - 1;
            set_dirty();
        }
    }

    Undoee check_undo(bool really)
    {
        if (!current_view_is_last())
        {
            return NOTHING;
        }
        return art.check_undo(really);
    }

    void set_view(size_t idx)
    {
        if (idx < art.num_views())
        {
            currentView = idx;

            set_dirty();
        }
    }

    void save_data(const char *filename,
                   const std::vector<std::string> &imgNames) const
    {
        art.save(filename, imgNames);
    }

    void load_data(const char *filename, std::vector<std::string> &imgNames)
    {
        Masterpiece newArt;

        if (newArt.load(filename, imgNames))
        {
            art = newArt;

            currentView = art.num_views() - 1;

            set_dirty();
        }
    }

    void clear()
    {
        art.reset();

        currentView = 0;

        set_dirty();
    }

    void show_all(float vpW, float vpH, u8 currentLayer)
    {
        if (art.num_strokes() == 0 && art.num_images() == 0)
            return;

        currentView = art.num_views() - 1;
        box2 all_box = query_bbox(get_bake_data(), currentView);

        float2 center = 0.5f * (all_box.lb + all_box.rt);
        art.change_view(make_pan(center), currentLayer);

        float vpAspect = vpH / vpW;
        float bbAspect = all_box.height() / all_box.width();

        basis2s fit;
        if (vpAspect > bbAspect)
        {
            fit = make_zoom(vpW, all_box.width());
        }
        else
        {
            fit = make_zoom(vpH, all_box.height());
        }
        art.change_view(fit, currentLayer);
        currentView = art.num_views() - 1;

        set_dirty();
    }
};
