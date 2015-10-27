#include <vector>
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
    float shiftX;
    float shiftY;

    // ZOOM related;

    float zoomStartX;
    float zoomCoeff;

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
        float brushred, brushgreen, brushblue, brushalpha;
        float brushdiameter;
    };

    typedef void (Vandalism::*MC_FN)(const Input *);


    void idle(const Input *) {}
    void start_rotate(const Input *) {}
    void done_rotate(const Input *) {}
    void do_rotate(const Input *) {}

    void illegal(const Input *) {}

    void start_zoom(const Input *input)
    {
        zoomStartX = input->mousex;
    }

    void do_zoom(const Input *input)
    {
        zoomCoeff = zoomStartX / input->mousex;
    }

    void done_zoom(const Input *input)
    {
        pin = views.size();
        views.push_back({TZOOM,
                zoomStartX, input->mousex,
                strokes.size(), strokes.size()});
        zoomCoeff = 1.0f;

        visiblesChanged = true;
    }

    void start_pan(const Input *input)
    {
        panStartX = input->mousex;
        panStartY = input->mousey;
    }

    void do_pan(const Input *input)
    {
        shiftX = input->mousex - panStartX;
        shiftY = input->mousey - panStartY;
    }

    void done_pan(const Input *input)
    {
        pin = views.size();
        views.push_back({TPAN,
                -(input->mousex - panStartX),
                -(input->mousey - panStartY),
                strokes.size(), strokes.size()});

        shiftX = 0.0f;
        shiftY = 0.0f;

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
        brush.a = input->brushalpha;
        brush.diameter = input->brushdiameter;
        brushes.push_back(brush);
        strokes.push_back(stroke);
    }

    void do_draw(const Input *input)
    {
        Point point;
        point.x = input->mousex;
        point.y = input->mousey;
        points.push_back(point);

        strokes.back().pi1 = points.size();
    }

    void done_draw(const Input *)
    {
        views[pin].si1 = strokes.size();

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

        views.push_back({TPAN, 0.0f, 0.0f, 0, 0});

        pin = 0;

        shiftX = 0.0f;
        shiftY = 0.0f;
        zoomCoeff = 1.0f;
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
};
