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

struct kernel_services
{
    typedef uint32 TexID;

    typedef TexID (*CREATE_TEXTURE)(uint32, uint32);
    typedef void (*UPDATE_TEXTURE)(TexID, const uint8*);
    typedef void (*DELETE_TEXTURE)(TexID);

    CREATE_TEXTURE create_texture;
    UPDATE_TEXTURE update_texture;
    DELETE_TEXTURE delete_texture;

    typedef uint32 MeshID;

    typedef MeshID (*CREATE_MESH)();
    typedef void (*UPDATE_MESH)(MeshID, const void*, uint32, const unsigned short*, uint32);
    typedef void (*DELETE_MESH)(MeshID);

    CREATE_MESH create_mesh;
    UPDATE_MESH update_mesh;
    DELETE_MESH delete_mesh;
};

struct input_data
{
    double *pTimeIntervals;
    uint32 nTimeIntervals;

    uint64 nFrames;

    int32 swMouseXPx;
    int32 swMouseYPx;
    bool mouseleft;

    int32 windowWidthPx, windowHeightPx;
    int32 windowWidthPt, windowHeightPt;
    int32 windowPosXPt, windowPosYPt;

    int32 vpWidthPx, vpHeightPx;
    int32 vpWidthPt, vpHeightPt;
    float vpWidthIn, vpHeightIn;

    float rtWidthIn, rtHeightIn;

    int32 swWidthPx, swHeightPx;
    kernel_services services;
};

struct output_data
{
    offscreen_buffer *buffer;
    triangles *bake_tris;
    triangles *curr_tris;
    bool bake_flag;

    // baked texture quad manipulations
    float translateX;
    float translateY;
    float scale;
    float rotate;

    float bg_red;
    float bg_green;
    float bg_blue;

    float grid_bg_color[3];
    float grid_fg_color[3];
    float grid_translation[2];
    float grid_zoom;

    struct drawcall
    {
        kernel_services::TexID texture_id;
        kernel_services::MeshID mesh_id;
        uint32 offset;
        uint32 count;
    };

    drawcall *ui_drawcalls;
    uint32 ui_drawcall_cnt;
};

typedef void (*UPDATE_AND_RENDER_FUNC)(input_data *input, output_data *output);
typedef void (*SETUP_FUNC)(kernel_services *services);
typedef void (*CLEANUP_FUNC)();
