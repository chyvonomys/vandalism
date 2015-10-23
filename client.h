typedef unsigned int uint32;
typedef unsigned char uint8;
typedef unsigned long long uint64;
typedef int int32;

struct offscreen_buffer
{
    uint8 *data;
    uint32 width;
    uint32 height;
};

struct triangles
{
    float *data;
    uint32 size;
    uint32 capacity;
};

/*
struct services
{
    typedef uint32 (*CREATE_TEXTURE)(uint32, uint32);
    typedef void (*UPDATE_TEXTURE)(uint32, const uint8*);
    typedef void (*DELETE_TEXTURE)(uint32);
};
*/

struct input_data
{
    double *pTimeIntervals;
    uint32 nTimeIntervals;

    uint64 nFrames;

    int32 mousex;
    int32 mousey;
    bool mouseleft;
};

struct output_data
{
    offscreen_buffer *buffer;
    triangles *bake_tris;
    triangles *curr_tris;
    bool bake_flag;

    float translateX;
    float translateY;

    float bufferWidthIn;
    float bufferHeightIn;
};

typedef void (*UPDATE_AND_RENDER_FUNC)(input_data *input, output_data *output);
typedef void (*SETUP_FUNC)();
typedef void (*CLEANUP_FUNC)();
