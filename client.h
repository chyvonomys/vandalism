#include <cstdint>

typedef uint64_t u64;
typedef uint32_t u32;
typedef uint16_t u16;
typedef uint8_t u8;

typedef int64_t i64;
typedef int32_t i32;
typedef int16_t i16;
typedef int8_t i8;

struct VertexLayout;

struct kernel_services
{
    typedef u32 TexID;

    TexID create_texture(u32, u32, u32);
    void update_texture(TexID, const u8*);
    void delete_texture(TexID);

    const VertexLayout *ui_vertex_layout;
    const VertexLayout *stroke_vertex_layout;
    static const TexID default_texid = 0;

    typedef u32 MeshID;

    MeshID create_mesh(const VertexLayout&, u32, u32);
    MeshID create_quad_mesh(const VertexLayout&, u32);
    void update_mesh(MeshID, const void*, u32, const u16*, u32);
    void update_mesh_vb(MeshID, const void*, u32);
    void delete_mesh(MeshID);

    bool check_file(const char* path);

    const u8 *get_capture_data(u32&, u32&);
};

struct input_data
{
    double *pTimeIntervals;
    u32 nTimeIntervals;

    u32 nFrames;

    float rawMouseXPt, rawMouseYPt;
    float vpMouseXPt, vpMouseYPt;
    bool mouseleft;
    bool shiftkey;
    bool scrolling;
    float scrollY;

    i32 windowWidthPx, windowHeightPx;
    i32 windowWidthPt, windowHeightPt;
    i32 windowPosXPt, windowPosYPt;

    i32 vpWidthPx, vpHeightPx;
    i32 vpWidthPt, vpHeightPt;

	float vpWidthIn, vpHeightIn;
    float rtWidthIn, rtHeightIn;

    i32 swWidthPx, swHeightPx;

    bool forceUpdate;
    std::string debugstr;
};

#include "math.h"

struct GuiDrawcall
{
    kernel_services::TexID texture;
    kernel_services::MeshID mesh;
    u32 offset;
    u32 count;
};

struct StrokesDrawcall
{
    kernel_services::MeshID mesh;
    u32 offset;
    u32 count;
};

struct ImageDrawcall
{
    kernel_services::TexID texture;
    basis2r basis;
};

struct LayerRecipe
{
    struct ItemSpec
    {
        enum { STROKEBATCH, IMAGE } type;
        u32 index;
    };
    std::vector<ItemSpec> items;
};

// we don't talk layer ids here, just above cur below and distinguish above layers to bake separately within that range
struct RenderRecipe
{
    std::vector<StrokesDrawcall> strokeDCs;
    std::vector<ImageDrawcall> imageDCs;

    std::vector<LayerRecipe> belowBakery; // this should always contain actual info needed to reconstruct below layers.
    std::vector<LayerRecipe> aboveBakery;
    LayerRecipe currentBakery;

    bool bakeBelow; // need this, because emptinness of bakery is also an update
    bool bakeAbove;
    bool bakeCurrent;

    StrokesDrawcall currentStroke;
    bool capture;
    bool drawGrid;
    bool blitBelow;
    bool blitCurrent;
    bool blitAbove;
    bool fitImage;
    ImageDrawcall fitDC;
    std::vector<GuiDrawcall> guiDCs;

    void clear()
    {
        strokeDCs.clear();
        imageDCs.clear();
        belowBakery.clear();
        aboveBakery.clear();
        currentBakery.items.clear();
        bakeBelow = bakeAbove = bakeCurrent = true;
        capture = drawGrid = blitBelow = blitCurrent = blitAbove = fitImage = false;
        guiDCs.clear();
    }
};

// moving towards `statelessness`
// this object's contents should be enough for building a frame, no matter if baked RT contain or not
// e.g. 

// TODO: test case: all layers are turned off, drawing, what should happen?

struct output_data
{
    // TODO: should be somehow generated from layout
    struct Vertex
    {
        float x, y, z;
        float u, v, e;
        float r, g, b, a;
    };

    bool quit_flag;

    // baked texture quad manipulations
    float2 preTranslate;
    float2 postTranslate;
    float scale;
    float rotate;

    color3f bg_color;
    color3f grid_bg_color;
    color3f grid_fg_color;

    float zbandwidth;

    float capture_width;
    float capture_height;

    const RenderRecipe *recipe;
};

void update_and_render(input_data *input, output_data *output);
void setup(kernel_services *services);
void cleanup();
