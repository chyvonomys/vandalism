#include <vector>
#include <fstream>
#include <sstream>
#include "cascade.cpp"

struct Vandalism
{
    typedef test_point Point;

    std::vector<Point> points;

    typedef test_stroke Stroke;

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

    std::vector<Stroke> strokes;
    std::vector<Brush> brushes;

    std::vector<test_view> views;

    std::vector<size_t> pins;

    size_t currentViewIdx;

    // TODO: change this
    bool visiblesChanged;

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
        PLACING1 = 5,
        MOVING2  = 6,
        MODECNT  = 7
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

    struct Input
    {
        float mousex;
        float mousey;
        bool mousedown;
        bool fakepressure;
        Tool tool;
        float brushred, brushgreen, brushblue, brushalpha;
        float brushdiameter;
        float eraseralpha;
        float negligibledistance;
    };

    typedef void (Vandalism::*MC_FN)(const Input *);

    bool append_allowed() const
    {
        return currentViewIdx + 1 == views.size();
    }

    void append_new_view(const test_transition& tr)
    {
        auto& prev = views[currentViewIdx];
        if (autoOptimizeViews && prev.pin_index == NPOS && prev.si0 == prev.si1 &&
            prev.tr.type == tr.type)
        {
            prev.si0 = strokes.size();
            prev.si1 = strokes.size();
            prev.tr = combine_transitions(prev.tr.a, prev.tr.b,
                                          tr.a, tr.b,
                                          prev.tr.type);
        }
        else
        {
            currentViewIdx = views.size();
            views.push_back(test_view(tr, strokes.size(), strokes.size()));
        }
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
            test_transition trot = {TROTATE, -angle, 0.0f};
            append_new_view(trot);
        }

        remove_alterations();

        visiblesChanged = true; // TODO: really? depends on boundary shape
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
            test_transition tzoom = {TZOOM, input->mousex, zoomStartX};
            append_new_view(tzoom);
        }

        remove_alterations();

        visiblesChanged = true;
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

        remove_alterations();

        visiblesChanged = true;
    }

    float pressure_func(size_t i)
    {
        return (i > 100 ? 0.0f : (100.0f - i) / 100.0f);
    }

    void start_draw(const Input *input)
    {
        Stroke stroke;
        stroke.pi0 = points.size();
        {
            Point point;
            point.x = input->mousex;
            point.y = input->mousey;
            point.t = input->fakepressure ? pressure_func(0) : 1.0f;
            points.push_back(point);
        }
        stroke.pi1 = points.size();

        Brush brush;
        brush.r = input->brushred;
        brush.g = input->brushgreen;
        brush.b = input->brushblue;
        brush.a = (input->tool == ERASE ? input->eraseralpha : input->brushalpha);
        brush.type = (input->tool == ERASE ? 1 : 0);
        brush.diameter = input->brushdiameter;

        if (brushes.empty() ||
            ::memcmp(&brushes.back(), &brush, sizeof(Brush)) != 0)
        {
            brushes.push_back(brush);
        }

        stroke.brush_id = brushes.size() - 1;
        strokes.push_back(stroke);
    }

    void do_draw(const Input *input)
    {
        Point point;
        point.x = input->mousex;
        point.y = input->mousey;

        float dx = point.x - points.back().x;
        float dy = point.y - points.back().y;
        float eps = input->negligibledistance * input->negligibledistance;
        if (dx * dx + dy * dy > eps)
        {
            size_t ptIdx = points.size() - strokes.back().pi0;
            point.t = input->fakepressure ? pressure_func(ptIdx) : 1.0f;
            points.push_back(point);
            strokes.back().bbox.add(point);
            strokes.back().pi1 = points.size();
        }
    }

    void done_draw(const Input *)
    {
        if (append_allowed())
        {
            strokes.back().bbox.grow(0.5f * brushes.back().diameter);
            views[currentViewIdx].bbox.add_box(strokes.back().bbox);

            views[currentViewIdx].si1 = strokes.size();
        }
        else
        {
            for (size_t pi = strokes.back().pi0; pi < strokes.back().pi1; ++pi)
            {
                points.pop_back();
            }
            strokes.pop_back();
            brushes.pop_back();
        }

        visiblesChanged = true;
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

        float a0 = static_cast<float>(::atan2(d0.y, d0.x));
        float a1 = static_cast<float>(::atan2(d1.y, d1.x));

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

            float a0 = static_cast<float>(::atan2(d0.y, d0.x));
            float a1 = static_cast<float>(::atan2(d1.y, d1.x));

            currentViewIdx = views.size();
            test_transition tpre = {TPAN, f.x, f.y};
            views.push_back(test_view(tpre, strokes.size(), strokes.size()));

            currentViewIdx = views.size();
            test_transition trot = {TROTATE, a0 - a1, 0.0f};
            views.push_back(test_view(trot, strokes.size(), strokes.size()));

            currentViewIdx = views.size();
            test_transition tzoom = {TZOOM, len(d1), len(d0)};
            views.push_back(test_view(tzoom, strokes.size(), strokes.size()));

            currentViewIdx = views.size();
            test_transition tpost = {TPAN, -f.x, -f.y};
            views.push_back(test_view(tpost, strokes.size(), strokes.size()));
        }

        remove_alterations();

        visiblesChanged = true;
    }

    MC_FN from_idle_handlers[MODECNT] =
    {&Vandalism::idle, &Vandalism::start_draw, &Vandalism::start_pan, &Vandalism::start_zoom,
     &Vandalism::start_rotate, &Vandalism::place1, &Vandalism::start_move2};

    MC_FN to_idle_handlers[MODECNT] =
    {&Vandalism::idle, &Vandalism::done_draw, &Vandalism::done_pan, &Vandalism::done_zoom,
     &Vandalism::done_rotate, &Vandalism::place1, &Vandalism::done_move2};

    MC_FN same_mode_handlers[MODECNT] =
    {&Vandalism::idle, &Vandalism::do_draw, &Vandalism::do_pan, &Vandalism::do_zoom,
     &Vandalism::do_rotate, &Vandalism::place1, &Vandalism::do_move2};

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

        pins.push_back(0);

        test_transition none = {TPAN, 0.0f, 0.0f};
        views.push_back(test_view(none, 0, 0));
        views.back().pin_index = 0;

        currentViewIdx = 0;

        autoOptimizeViews = true;

        firstX = 0.0f;
        firstY = 0.0f;

        remove_alterations();
    }

    void cleanup()
    {
    }

    size_t get_current_stroke(size_t &from, size_t &to)
    {
        if (currentMode == DRAWING)
        {
            from = strokes.back().pi0;
            to = strokes.back().pi1;
        }
        else
        {
            from = 0;
            to = 0;
        }
        return strokes.size() - 1;
    }

    bool get_current_brush(Brush &brush) const
    {
        if (currentMode == DRAWING)
        {
            brush = brushes[strokes.back().brush_id];
            return true;
        }
        return false;
    }

    test_data get_debug_data() const
    {
        test_data result =
        {
            points.data(),
            strokes.data(),
            views.data(),
            views.size()
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
        if (views.size() > 1)
        {
            std::vector<test_view> optimized;
            optimized.reserve(views.size());

            optimized.push_back(views[0]);

            for (size_t i = 1; i < views.size(); ++i)
            {
                auto& prev = optimized.back();
                const auto& curr = views[i];
                if (prev.pin_index == NPOS && prev.si0 == prev.si1 &&
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
                if (curr.pin_index != NPOS)
                {
                    pins[curr.pin_index] = optimized.size() - 1;
                }
            }
            views = optimized;
            visiblesChanged = true;
        }
    }

    bool undoable()
    {
        return check_undo(false);
    }

    void undo()
    {
        if (check_undo(true))
        {
            visiblesChanged = true;
        }
    }

    bool check_undo(bool really)
    {
        if (currentViewIdx != views.size() - 1)
        {
            return false;
        }
        else if (views.back().si1 == views.back().si0)
        {
            if (views.size() > 1)
            {
                if (really)
                {
                    views.pop_back();
                    --currentViewIdx;
                }
                return true;
            }
        }
        else if (views.back().si1 > views.back().si0)
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
            }
            return true;
        }
        return false;
    }

    void set_view(size_t idx)
    {
        if (idx < views.size())
        {
            currentViewIdx = idx;
            visiblesChanged = true;
        }
    }

    void save_data(const char *filename)
    {
        std::ofstream os(filename);

        for (size_t i = 0; i < points.size(); ++i)
        {
            os << "p " << points[i].x << ' ' << points[i].y << '\n';
        }
        for (size_t i = 0; i < strokes.size(); ++i)
        {
            os << "s " << strokes[i].pi0 << ' ' << strokes[i].pi1 << ' ' << strokes[i].brush_id << ' ' << '\n';
        }
        for (size_t i = 0; i < views.size(); ++i)
        {
            os << "v " << (views[i].tr.type == TZOOM ? "zoom" :
                          (views[i].tr.type == TPAN ? "pan" :
                           (views[i].tr.type == TROTATE ? "rotate" : "error")))
               << ' ' << views[i].tr.a << ' ' << views[i].tr.b << ' ' << views[i].si0 << ' ' << views[i].si1 << '\n';
        }
        for (size_t i = 0; i < brushes.size(); ++i)
        {
            os << "b " << brushes[i].type << ' ' << brushes[i].diameter << ' '
               << brushes[i].r << ' ' << brushes[i].g << ' ' << brushes[i].b << ' ' << brushes[i].a << '\n';
        }
    }

    void load_data(const char *filename)
    {
        std::ifstream is(filename);
        std::string line;
        std::vector<test_point> newPoints;
        std::vector<test_stroke> newStrokes;
        std::vector<test_view> newViews;
        std::vector<Brush> newBrushes;

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
                    ok = !(ss >> pt.x >> pt.y).fail();
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
                    test_view vi({TPAN, 0.0f, 0.0f}, 0, 0);
                    std::string ttype;
                    ok = !(ss >> ttype >> vi.tr.a >> vi.tr.b
                           >> vi.si0 >> vi.si1).fail();
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
            for (size_t pi = newStrokes[si].pi0; pi < newStrokes[si].pi1; ++pi)
            {
                newStrokes[si].bbox.add(newPoints[pi]);
            }
            newStrokes[si].bbox.grow(0.5f * newBrushes[newStrokes[si].brush_id].diameter);
        }

        // calculate bboxes of views
        for (size_t vi = 0; vi < newViews.size(); ++vi)
        {
            for (size_t si = newViews[vi].si0; si < newViews[vi].si1; ++si)
            {
                newViews[vi].bbox.add_box(newStrokes[si].bbox);
            }
        }

        if (ok)
        {
            brushes = newBrushes;
            views = newViews;
            strokes = newStrokes;
            points = newPoints;
            currentViewIdx = views.size() - 1;

            visiblesChanged = true;
        }
    }
};
