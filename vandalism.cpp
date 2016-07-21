#include <vector>
#include <iterator>
#include <fstream>
#include <sstream>
#include <iostream>
#include "cascade.cpp"

struct Vandalism
{
    typedef test_point Point;
    typedef test_stroke Stroke;
    typedef test_image Image;

    struct Brush
    {
        float diameter;
        float r, g, b, a;
        i32 type;

        Brush modified(float t) const
        {
            Brush result;
            result.diameter = diameter * t;
            result.r = r;
            result.g = g;
            result.b = b;
            result.a = a * t;
            result.type = type;
            return result;
        }
    };

    struct Pin
    {
        u8 layeridx;
        size_t viewidx;
    };

    std::vector<Point> points;
    std::vector<Stroke> strokes;
    std::vector<test_view> views;
    // One image name can be referenced by many images
    // Image is actually an image + placement.
    // image<->imagename is like stroke<->brush
    std::vector<Image> images;

    std::vector<std::string> imageNames;

    std::vector<Brush> brushes;

    std::vector<Pin> pins;

    Pin currentPin;

    std::vector<Point> currentPoints;
    Stroke currentStroke;
    Brush currentBrush;

    // TODO: change this
    bool visiblesChanged;
    bool currentChanged;

    bool autoOptimizeViews;

    // PAN related

    float panStartX;
    float panStartY;
    float preShiftX;
    float preShiftY;
    float postShiftX;
    float postShiftY;

    // ZOOM related

    float zoomStartX;
    float zoomCoeff;

    // ROTATE related

    float rotateStartX;
    float rotateAngle;

    // FIRST/SECOND related

    float firstX;
    float firstY;

    float secondX0;
    float secondY0;
    float secondX1;
    float secondY1;

    // SCROLL related

    float scrollY0;

    struct Pan
    {
        float dx;
        float dy;
    };

    enum Mode
    {
        IDLE      = 0,
        DRAWING   = 1,
        PANNING   = 2,
        ZOOMING   = 3,
        ROTATING  = 4,
        PLACING1  = 5,
        MOVING2   = 6,
        SCROLLING = 7,
        MODECNT   = 8
    };

    enum Tool
    {
        DRAW   = 0,
        ERASE  = 1,
        PAN    = 2,
        ZOOM   = 3,
        ROTATE = 4,
        FIRST  = 5,
        SECOND = 6
    };

    enum Smooth
    {
        NONE      = 0,
        HERMITE   = 1,
        FITBEZIER = 2
    };

    struct Input
    {
        float mousex;
        float mousey;
        float scrolly;
        bool scrolling;
        bool mousedown;
        bool fakepressure;
        bool simplify;
        Smooth smooth;
        Tool tool;
        float brushred, brushgreen, brushblue, brushalpha;
        float brushdiameter;
        float eraseralpha;
        float negligibledistance;
    };

    typedef void (Vandalism::*MC_FN)(const Input *);

    void set_dirty()
    {
        visiblesChanged = true;
        currentChanged = true;
    }

    void set_current_layer(u8 id)
    {
        currentPin.layeridx = id;
    }

    u8 get_current_layer_id() const
    {
        return currentPin.layeridx;
    }

    bool current_view_is_last() const
    {
        return currentPin.viewidx + 1 == views.size();
    }

    bool append_allowed() const
    {
        return current_view_is_last();
    }

    void append_new_view(const test_transition& tr)
    {
        // TODO: deal with view optimization and pin/view indexes

        /*
        auto& prev = views[currentPin.viewidx];
        if (autoOptimizeViews &&
            !prev.is_pinned() && !prev.has_image() &&
            prev.si0 == prev.si1 &&
            prev.tr.type == tr.type)
        {
            prev.si0 = strokes.size();
            prev.si1 = strokes.size();
            prev.tr = combine_transitions(prev.tr.a, prev.tr.b,
                                          tr.a, tr.b,
                                          prev.tr.type);
        }
        else
        */

        currentPin.viewidx = views.size();
        views.push_back(test_view(tr,
                                  strokes.size(), strokes.size(),
                                  currentPin.layeridx));
    }

    void remove_alterations()
    {
        preShiftX = 0.0f;
        preShiftY = 0.0f;
        postShiftX = 0.0f;
        postShiftY = 0.0f;

        zoomCoeff = 1.0f;
        rotateAngle = 0.0f;
    }

    void idle(const Input *) {}
    void illegal(const Input *) {}


    void start_rotate(const Input *input)
    {
        rotateStartX = input->mousex;
    }
    
    void done_rotate(const Input *input)
    {
        if (append_allowed())
        {
            float dx = input->mousex - rotateStartX;
            float angle = si_pi * dx;

            append_new_view(test_transition{TROTATE, -angle, 0.0f});
        }

        common_done();
    }
    
    void do_rotate(const Input *input)
    {
        float dx = input->mousex - rotateStartX;
        rotateAngle = si_pi * dx;
    }

    void start_zoom(const Input *input)
    {
        zoomStartX = input->mousex;
    }

    void do_zoom(const Input *input)
    {
        zoomCoeff = input->mousex / zoomStartX;
    }

    void done_zoom(const Input *input)
    {
        if (append_allowed())
        {
            append_new_view(test_transition{TZOOM, input->mousex, zoomStartX});
        }

        common_done();
    }

    void start_pan(const Input *input)
    {
        panStartX = input->mousex;
        panStartY = input->mousey;
    }

    void do_pan(const Input *input)
    {
        postShiftX = input->mousex - panStartX;
        postShiftY = input->mousey - panStartY;
    }

    void done_pan(const Input *input)
    {
        if (append_allowed())
        {
            test_transition tpan = {TPAN,
                                    panStartX - input->mousex,
                                    panStartY - input->mousey};
            append_new_view(tpan);
        }
        common_done();
    }

    float pressure_func(size_t i)
    {
        return (i > 100 ? 0.0f : (100.0f - i) / 100.0f);
    }

    void start_draw(const Input *input)
    {
        Point point;
        point.x = input->mousex;
        point.y = input->mousey;
        point.w = input->fakepressure ? pressure_func(0) : 1.0f;

        currentPoints.clear();
        currentPoints.push_back(point);
        currentStroke.pi0 = 0;
        currentStroke.pi1 = 1;
        currentStroke.bbox = test_box();
        currentStroke.bbox.add(point);
        currentStroke.brush_id = NPOS;

        currentBrush.r = input->brushred;
        currentBrush.g = input->brushgreen;
        currentBrush.b = input->brushblue;
        currentBrush.a = (input->tool == ERASE ?
                          input->eraseralpha : input->brushalpha);
        currentBrush.type = (input->tool == ERASE ? 1 : 0);
        currentBrush.diameter = input->brushdiameter;
    }

    void do_draw(const Input *input)
    {
        Point point;
        point.x = input->mousex;
        point.y = input->mousey;

        float dx = point.x - currentPoints.back().x;
        float dy = point.y - currentPoints.back().y;
        float eps = input->negligibledistance * input->negligibledistance;
        if (dx * dx + dy * dy > eps)
        {
            size_t ptIdx = currentPoints.size();
            point.w = input->fakepressure ? pressure_func(ptIdx) : 1.0f;
            currentPoints.push_back(point);
            currentStroke.bbox.add(point);
            currentStroke.pi1 += 1;
            currentChanged = true;
        }
    }

    // TODO: is point in done_* the same as in previous do_* if there was any?

    void done_draw(const Input *input)
    {
        if (append_allowed())
        {
            // TODO: keep only unique?
            if (brushes.empty() ||
                ::memcmp(&brushes.back(), &currentBrush, sizeof(Brush)) != 0)
            {
                brushes.push_back(currentBrush);
            }

            currentStroke.bbox.grow(0.5f * currentBrush.diameter);

            if (views[currentPin.viewidx].li != currentPin.layeridx)
            {
                append_new_view(test_transition{TPAN, 0.0f, 0.0f});
            }

            strokes.push_back(currentStroke);
            strokes.back().brush_id = brushes.size() - 1;
            strokes.back().pi0 = points.size();

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
                smooth_stroke(currentPoints.data(),
                              currentPoints.size(),
                              points, input->negligibledistance,
                              input->smooth == FITBEZIER);
            }
            else if (input->smooth == NONE)
            {
                std::copy(currentPoints.cbegin(), currentPoints.cend(),
                          std::back_inserter(points));
            }

            strokes.back().pi1 = points.size();

            views[currentPin.viewidx].bbox.add_box(currentStroke.bbox);
            views[currentPin.viewidx].si1 += 1;
        }

        currentStroke.pi0 = 0;
        currentStroke.pi1 = 0;
        currentStroke.bbox = test_box();

        common_done();
    }

    void place1(const Input *input)
    {
        firstX = input->mousex;
        firstY = input->mousey;
    }

    void start_move2(const Input *input)
    {
        secondX0 = input->mousex;
        secondY0 = input->mousey;
    }

    void do_move2(const Input *input)
    {
        secondX1 = input->mousex;
        secondY1 = input->mousey;

        float2 f = {firstX, firstY};
        float2 s0 = {secondX0, secondY0};
        float2 s1 = {secondX1, secondY1};

        float2 d0 = s0 - f;
        float2 d1 = s1 - f;

        float a0 = static_cast<float>(::atan2f(d0.y, d0.x));
        float a1 = static_cast<float>(::atan2f(d1.y, d1.x));

        preShiftX = -f.x;
        preShiftY = -f.y;
        postShiftX = f.x;
        postShiftY = f.y;

        zoomCoeff = len(d1) / len(d0);
        rotateAngle = a1 - a0;
    }

    void done_move2(const Input *input)
    {
        if (append_allowed())
        {
            secondX1 = input->mousex;
            secondY1 = input->mousey;

            float2 f = {firstX, firstY};
            float2 s0 = {secondX0, secondY0};
            float2 s1 = {secondX1, secondY1};

            float2 d0 = s0 - f;
            float2 d1 = s1 - f;

            float a0 = static_cast<float>(::atan2f(d0.y, d0.x));
            float a1 = static_cast<float>(::atan2f(d1.y, d1.x));

            append_new_view(test_transition{TPAN, f.x, f.y});
            append_new_view(test_transition{TROTATE, a0 - a1, 0.0f});
            append_new_view(test_transition{TZOOM, len(d1), len(d0)});
            append_new_view(test_transition{TPAN, -f.x, -f.y});
        }

        common_done();
    }

    void start_scroll(const Input *input)
    {
        scrollY0 = input->scrolly;
        preShiftX = -input->mousex;
        preShiftY = -input->mousey;
        postShiftX = input->mousex;
        postShiftY = input->mousey;
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
            append_new_view(test_transition{TPAN, postShiftX, postShiftY});
            append_new_view(test_transition{TZOOM, zoomCoeff, 1.0f});
            append_new_view(test_transition{TPAN, preShiftX, preShiftY});
        }

        common_done();
    }

    size_t image_name_idx(const char *name)
    {
        size_t result = 0;
        for (; result < imageNames.size(); ++result)
        {
            if (imageNames[result] == name)
                break;
        }
        return result;
    }
    
    void place_image(const char *name, float imageW, float imageH)
    {
        if (append_allowed())
        {
            append_new_view(test_transition{TPAN, 0.0f, 0.0f});

            size_t imageId = image_name_idx(name);
            if (imageId == imageNames.size())
            {
                imageNames.push_back(name);
            }

            test_image i = {imageId,
                            -0.5f * imageW, -0.5f * imageH,
                            imageW, 0.0f,
                            0.0f, imageH};

            views.back().imgbbox = i.get_bbox();
            views.back().ii = images.size();
            images.push_back(i);
        }

        set_dirty();
    }

    void common_done()
    {
        remove_alterations();
        set_dirty();
    }

	MC_FN from_idle_handlers[MODECNT];
	MC_FN to_idle_handlers[MODECNT];
	MC_FN same_mode_handlers[MODECNT];

    MC_FN mode_change_handlers(Mode from, Mode to)
    {
        if (from == IDLE)
            return from_idle_handlers[to];
        if (to == IDLE)
            return to_idle_handlers[from];
        if (from == to)
            return same_mode_handlers[from];

        return &Vandalism::illegal;
    }

    void handle_mode_change(Mode prevMode, Mode currMode, Input *input)
    {
        MC_FN fn = mode_change_handlers(prevMode, currMode);
        if (fn == &Vandalism::illegal)
        {
            MC_FN toIdle = mode_change_handlers(prevMode, IDLE);
            MC_FN fromIdle = mode_change_handlers(IDLE, currMode);

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
        if (input->mousedown)
        {
            switch (input->tool)
            {
            case DRAW:
            case ERASE:
                return DRAWING;
            case PAN:
                return PANNING;
            case ZOOM:
                return ZOOMING;
            case ROTATE:
                return ROTATING;
            case FIRST:
                return PLACING1;
            case SECOND:
                return MOVING2;
            }
        }
        else if (input->scrolling)
        {
            return SCROLLING;
        }
        return IDLE;
    }

    Mode currentMode;
    u32 layersCnt;

    void update(Input *input)
    {
        Mode mode = current_mode(input);
        handle_mode_change(currentMode, mode, input);
        currentMode = mode;
    }

    void setup()
    {
		// TODO: this should be statically initialized once (works in clang)
		// doesn't work in vs2013
		from_idle_handlers[IDLE] = &Vandalism::idle;
		from_idle_handlers[DRAWING] = &Vandalism::start_draw;
		from_idle_handlers[PANNING] = &Vandalism::start_pan;
		from_idle_handlers[ZOOMING] = &Vandalism::start_zoom;
		from_idle_handlers[ROTATING] = &Vandalism::start_rotate;
		from_idle_handlers[PLACING1] = &Vandalism::place1;
		from_idle_handlers[MOVING2] = &Vandalism::start_move2;
		from_idle_handlers[SCROLLING] = &Vandalism::start_scroll;

		to_idle_handlers[IDLE] = &Vandalism::idle;
		to_idle_handlers[DRAWING] = &Vandalism::done_draw;
		to_idle_handlers[PANNING] = &Vandalism::done_pan;
		to_idle_handlers[ZOOMING] = &Vandalism::done_zoom;
		to_idle_handlers[ROTATING] = &Vandalism::done_rotate;
		to_idle_handlers[PLACING1] = &Vandalism::place1;
		to_idle_handlers[MOVING2] = &Vandalism::done_move2;
		to_idle_handlers[SCROLLING] = &Vandalism::done_scroll;

		same_mode_handlers[IDLE] = &Vandalism::idle;
		same_mode_handlers[DRAWING] = &Vandalism::do_draw;
		same_mode_handlers[PANNING] = &Vandalism::do_pan;
		same_mode_handlers[ZOOMING] = &Vandalism::do_zoom;
		same_mode_handlers[ROTATING] = &Vandalism::do_rotate;
		same_mode_handlers[PLACING1] = &Vandalism::place1;
		same_mode_handlers[MOVING2] = &Vandalism::do_move2;
		same_mode_handlers[SCROLLING] = &Vandalism::do_scroll;

        currentMode = IDLE;

        // TODO: look for other missing initializations
        currentStroke.pi0 = 0;
        currentStroke.pi1 = 0;

        setup_default_view();

        autoOptimizeViews = true;

        firstX = 0.0f;
        firstY = 0.0f;

        remove_alterations();
        set_dirty();
    }

    const Brush& get_current_brush() const
    {
        return currentBrush;
    }

    // TODO: does this need to be here?
    size_t get_current_stroke_id() const
    {
        return strokes.size();
    }

    test_data get_bake_data() const
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

    test_transition combine_transitions(float lefta, float leftb,
                                        float righta, float rightb,
                                        transition_type ty)
    {
        test_transition result;
        result.type = ty;
        if (ty == TPAN)
        {
            result.a = lefta + righta;
            result.b = leftb + rightb;
        }
        else if (ty == TZOOM)
        {
            result.a = lefta * righta;
            result.b = leftb * rightb;

            result.a = result.a / result.b;
            result.b = 1.0f;
        }
        else if (ty == TROTATE)
        {
            result.a = lefta + righta;
            result.b = 0.0f;
        }
        return result;
    }

    void optimize_views()
    {
        // TODO: bring this back (?)
        /*
        if (views.size() > 1)
        {
            std::vector<test_view> optimized;
            optimized.reserve(views.size());

            optimized.push_back(views[0]);

            for (size_t i = 1; i < views.size(); ++i)
            {
                auto& prev = optimized.back();
                const auto& curr = views[i];
                if (!prev.is_pined() && !prev.has_strokes() &&
                    prev.tr.type == curr.tr.type && i != currentViewIdx)
                {
                    prev.bbox = curr.bbox;
                    prev.pin_index = curr.pin_index;
                    prev.si0 = curr.si0;
                    prev.si1 = curr.si1;
                    prev.tr = combine_transitions(prev.tr.a, prev.tr.b,
                                                  curr.tr.a, curr.tr.b,
                                                  prev.tr.type);
                }
                else
                {
                    if (i == currentViewIdx)
                    {
                        currentViewIdx = optimized.size();
                    }
                    optimized.push_back(curr);
                }
                // indexes change while optimizing, update
                if (curr.is_pinned())
                {
                    pins[curr.pin_index] = optimized.size() - 1;
                }
            }
            views = optimized;
            set_dirty();
        }
        */
    }

    enum Undoee
    {
        NOTHING,
        STROKE,
        IMAGE,
        VIEW
    };

    Undoee undoable()
    {
        return check_undo(false);
    }

    void undo()
    {
        if (check_undo(true))
        {
            set_dirty();
        }
    }

    Undoee check_undo(bool really)
    {
        if (!current_view_is_last())
        {
            return NOTHING;
        }
        // TODO: view may now contain image, fix these conditions
        else if (views.back().si1 == views.back().si0)
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
                        --currentPin.viewidx;
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
                views.back().bbox = get_strokes_bbox(get_bake_data(), vi);
            }
            return STROKE;
        }
        return NOTHING;
    }

    void set_view(size_t idx)
    {
        if (idx < views.size())
        {
            currentPin.viewidx = idx;

            set_dirty();
        }
    }

    const char *tr_type_str(transition_type tt) const
    {
        if (tt == TZOOM) return "zoom";
        if (tt == TPAN) return "pan";
        if (tt == TROTATE) return "rotate";
        return "error";
    }

    void save_data(const char *filename) const
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
            os << "v " << tr_type_str(views[i].tr.type)
               << ' ' << views[i].tr.a << ' ' << views[i].tr.b
               << ' ' << views[i].si0 << ' ' << views[i].si1
               << ' ' << (!views[i].has_image() ? -1 : static_cast<i32>(views[i].ii))
               << '\n';
        }
        // TODO: maybe separate codes for regular brush, eraser, etc
        for (size_t i = 0; i < brushes.size(); ++i)
        {
            os << "b " << brushes[i].type << ' ' << brushes[i].diameter << ' '
               << brushes[i].r << ' ' << brushes[i].g << ' '
               << brushes[i].b << ' ' << brushes[i].a << '\n';
        }
        // TODO: maybe reuse 'basis' vectors approach here and in views
        for (size_t i = 0; i < images.size(); ++i)
        {
            os << "i \"" << imageNames[images[i].nameidx] << '\"'
                << ' ' << images[i].tx << ' ' << images[i].ty
                << ' ' << images[i].xx << ' ' << images[i].xy
                << ' ' << images[i].yx << ' ' << images[i].yy << '\n';
        }
    }

    void load_data(const char *filename)
    {
        std::ifstream is(filename);
        std::string line;

        std::vector<Point> newPoints;
        std::vector<Stroke> newStrokes;
        std::vector<Image> newImages;
        std::vector<test_view> newViews;
        std::vector<Brush> newBrushes;
        std::vector<std::string> newImageNames;

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
                    test_point pt;
                    ok = !(ss >> pt.x >> pt.y >> pt.w).fail();
                    if (ok)
                    {
                        newPoints.push_back(pt);
                    }
                }
                else if (*line.c_str() == 's')
                {
                    std::istringstream ss(line);
                    ss.seekg(2);
                    test_stroke st;
                    ok = !(ss >> st.pi0 >> st.pi1 >> st.brush_id).fail();
                    if (ok)
                    {
                        newStrokes.push_back(st);
                    }
                }
                else if (*line.c_str() == 'v')
                {
                    std::istringstream ss(line);
                    ss.seekg(2);
                    test_view vi({TPAN, 0.0f, 0.0f}, 0, 0, 0);
                    std::string ttype;
                    i32 imgidx;
                    ok = !(ss >> ttype >> vi.tr.a >> vi.tr.b
                           >> vi.si0 >> vi.si1 >> imgidx >> vi.li).fail();
                    if (ok)
                    {
                        if (ttype == "pan")
                        {
                            vi.tr.type = TPAN;
                        }
                        else if (ttype == "zoom")
                        {
                            vi.tr.type = TZOOM;
                        }
                        else if (ttype == "rotate")
                        {
                            vi.tr.type = TROTATE;
                        }
                        else
                        {
                            ok = false;
                            break;
                        }
                        vi.ii = (imgidx == -1 ? NPOS : static_cast<u32>(imgidx));
                        newViews.push_back(vi);
                    }
                }
                else if (*line.c_str() == 'b')
                {
                    std::istringstream ss(line);
                    ss.seekg(2);
                    Brush br;
                    ok = !(ss >> br.type >> br.diameter
                           >> br.r >> br.g >> br.b >> br.a).fail();
                    if (ok)
                    {
                        newBrushes.push_back(br);
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
                        ok = !(ss >> img.tx >> img.ty
                               >> img.xx >> img.xy >> img.yx >> img.yy).fail();
                        if (ok)
                        {
                            img.nameidx = 0;
                            for (; img.nameidx < newImageNames.size(); ++img.nameidx)
                            {
                                if (newImageNames[img.nameidx] == name)
                                    break;
                            }
                            if (img.nameidx == newImageNames.size())
                            {
                                // 'did not found'
                                newImageNames.push_back(name);
                            }
                            newImages.push_back(img);
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
        for (size_t si = 0; si < newStrokes.size(); ++si)
        {
            Stroke& ns = newStrokes[si];
            for (size_t pi = ns.pi0; pi < ns.pi1; ++pi)
            {
                ns.bbox.add(newPoints[pi]);
            }
            // TODO: invalid for non circular brushes
            // diameter should be max bbox of drawn point shape
            ns.bbox.grow(0.5f * newBrushes[ns.brush_id].diameter);
        }

        test_data newData;
        newData.points = newPoints.data();
        newData.strokes = newStrokes.data();
        newData.images = newImages.data();
        newData.views = newViews.data();
        newData.nviews = newViews.size();

        // calculate bboxes of views
        for (size_t vi = 0; vi < newViews.size(); ++vi)
        {
            newViews[vi].bbox = get_strokes_bbox(newData, vi);
            if (newViews[vi].has_image())
            {
                newViews[vi].imgbbox = newImages[newViews[vi].ii].get_bbox();
            }
        }

        if (ok)
        {
            // TODO: support for multiple layers
            clear();

            imageNames = newImageNames;
            brushes = newBrushes;
            views = newViews;
            images = newImages;
            points = newPoints;
            strokes = newStrokes;

            currentPin.viewidx = views.size() - 1;

            set_dirty();
        }
    }

    void setup_default_view()
    {
        Pin p0;
        p0.layeridx = 0;
        p0.viewidx = 0;
        pins.push_back(p0);

        test_transition none = {TPAN, 0.0f, 0.0f};
        views.push_back(test_view(none, 0, 0, 0));
        views.back().pi = 0;

        currentPin = {0, 0};
    }

    void clear()
    {
        brushes.clear();
        pins.clear();
        imageNames.clear();
        points.clear();
        strokes.clear();
        images.clear();
        views.clear();

        setup_default_view();

        set_dirty();
    }

    void show_all(float vpW, float vpH)
    {
        if (strokes.empty())
            return;

        currentPin.viewidx = views.size() - 1;
        test_box all_box = query_bbox(get_bake_data(), currentPin.viewidx);

        test_transition center = {TPAN,
                                  0.5f * (all_box.x1 + all_box.x0),
                                  0.5f * (all_box.y1 + all_box.y0)};

        append_new_view(center);

        float vpAspect = vpH / vpW;
        float bbAspect = all_box.height() / all_box.width();

        test_transition fit;
        if (vpAspect > bbAspect)
        {
            fit = {TZOOM, vpW, all_box.width()};
        }
        else
        {
            fit = {TZOOM, vpH, all_box.height()};
        }
        append_new_view(fit);

        set_dirty();
    }
};
