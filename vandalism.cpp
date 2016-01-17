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
        float r, g, b, a, e;
    };

    std::vector<Stroke> strokes;
    std::vector<Brush> brushes;

    std::vector<test_view> views;

    size_t pin;

    // TODO: change this
    bool visiblesChanged;


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
        Tool tool;
        float brushred, brushgreen, brushblue, brushalpha;
        float brushdiameter;
        float eraseralpha;
        float negligibledistance;
    };

    typedef void (Vandalism::*MC_FN)(const Input *);


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
        pin = views.size();
        float dx = input->mousex - rotateStartX;
        float angle = M_PI * dx;
        views.push_back({TROTATE,
                -angle, 0.0f,
                strokes.size(), strokes.size()});

        remove_alterations();

        visiblesChanged = true; // TODO: really? depends on boundary shape
    }
    
    void do_rotate(const Input *input)
    {
        float dx = input->mousex - rotateStartX;
        rotateAngle = M_PI * dx;
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
        pin = views.size();
        views.push_back({TZOOM,
                input->mousex, zoomStartX,
                strokes.size(), strokes.size()});

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
        pin = views.size();
        views.push_back({TPAN,
                -(input->mousex - panStartX),
                -(input->mousey - panStartY),
                strokes.size(), strokes.size()});

        remove_alterations();

        visiblesChanged = true;
    }

    void start_draw(const Input *input)
    {
        Stroke stroke;
        stroke.pi0 = points.size();
        {
            Point point;
            point.x = input->mousex;
            point.y = input->mousey;
            points.push_back(point);
        }
        stroke.pi1 = points.size();
        stroke.brush_id = brushes.size();
        // TODO: multiple strokes use one brush_id
        Brush brush;
        brush.r = input->brushred;
        brush.g = input->brushgreen;
        brush.b = input->brushblue;
        brush.a = (input->tool == ERASE ? 0.0f : input->brushalpha);
        brush.e = (input->tool == ERASE ? input->eraseralpha : 0.0f);
        brush.diameter = input->brushdiameter;
        brushes.push_back(brush);
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
            points.push_back(point);
            strokes.back().bbox.add(point);
            strokes.back().pi1 = points.size();
        }
    }

    void done_draw(const Input *)
    {
        strokes.back().bbox.grow(0.5f * brushes.back().diameter);
        views[pin].bbox.add_box(strokes.back().bbox);

        views[pin].si1 = strokes.size();

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

        float a0 = atan2(d0.y, d0.x);
        float a1 = atan2(d1.y, d1.x);

        preShiftX = -f.x;
        preShiftY = -f.y;
        postShiftX = f.x;
        postShiftY = f.y;

        zoomCoeff = len(d1) / len(d0);
        rotateAngle = a1 - a0;
    }

    void done_move2(const Input *input)
    {
        secondX1 = input->mousex;
        secondY1 = input->mousey;

        float2 f = {firstX, firstY};
        float2 s0 = {secondX0, secondY0};
        float2 s1 = {secondX1, secondY1};

        float2 d0 = s0 - f;
        float2 d1 = s1 - f;

        pin = views.size();
        views.push_back({TPAN,
                f.x, f.y,
                strokes.size(), strokes.size()});

        pin = views.size();
        float a0 = atan2(d0.y, d0.x);
        float a1 = atan2(d1.y, d1.x);

        views.push_back({TROTATE,
                a0 - a1, 0.0f,
                strokes.size(), strokes.size()});

        pin = views.size();
        views.push_back({TZOOM,
                len(d1), len(d0),
                strokes.size(), strokes.size()});

        pin = views.size();
        views.push_back({TPAN,
                -f.x, -f.y,
                strokes.size(), strokes.size()});

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

        views.push_back({TPAN, 0.0f, 0.0f, 0, 0});

        pin = 0;

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
            os << "b " << brushes[i].e << ' ' << brushes[i].diameter << ' '
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
                    test_view vi;
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
                    ok = !(ss >> br.e >> br.diameter
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

            visiblesChanged = true;
        }
    }
};
