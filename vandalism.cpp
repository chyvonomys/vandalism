#include <vector>

struct Vandalism
{
    struct Point
    {
        float x;
        float y;
        float t;
    };

    std::vector<Point> points;

    struct Stroke
    {
        uint32 startIdx;
        uint32 endIdx;
        float size;
    };

    // a subrange of some stroke
    struct Visible
    {
        uint32 startIdx;
        uint32 endIdx;
    };

    std::vector<Stroke> strokes;
    std::vector<Visible> visibles;

    // TODO: change this
    bool visiblesChanged;

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
    };

    typedef void (Vandalism::*MC_FN)(const Input *);


    void idle(const Input *) {}
    void start_pan(const Input *) {}
    void start_zoom(const Input *) {}
    void start_rotate(const Input *) {}
    void done_pan(const Input *) {}
    void done_zoom(const Input *) {}
    void done_rotate(const Input *) {}
    void do_pan(const Input *) {}
    void do_zoom(const Input *) {}
    void do_rotate(const Input *) {}

    void illegal(const Input *) {}

    void start_draw(const Input *input)
    {
        Stroke stroke;
        stroke.startIdx = points.size();
        {
            Point point;
            // TODO: transform from mouse to local space
            point.x = input->mousex;
            point.y = input->mousey;
            // TODO: fill t properly
            point.t = 0.0f;
            points.push_back(point);
        }
        stroke.endIdx = points.size();
        // TODO: stroke size and possibly other params
        stroke.size = 5.0f;
        strokes.push_back(stroke);
    }

    void do_draw(const Input *input)
    {
        Point point;
        // TODO: transform from mouse to local space
        point.x = input->mousex;
        point.y = input->mousey;
        // TODO: fill t properly
        point.t = 0.0f;
        points.push_back(point);

        strokes.back().endIdx = points.size();
    }

    void done_draw(const Input *)
    {
        Visible vis;
        vis.startIdx = strokes.back().startIdx;
        vis.endIdx = strokes.back().endIdx;
        visibles.push_back(vis);
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
    }

    void cleanup()
    {
    }

    void get_current_stroke(size_t &from, size_t &to)
    {
        if (currentMode == DRAWING)
        {
            from = strokes.back().startIdx;
            to = strokes.back().endIdx;
        }
        else
        {
            from = 0;
            to = 0;
        }
    }
};
