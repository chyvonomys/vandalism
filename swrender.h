inline void draw_pixel(offscreen_buffer *buffer,
                       uint32 x, uint32 y, const color& c)
{
    uint8 *p = buffer->data + 4 * (buffer->width * y + x);
    p[RO] = c.r;
    p[GO] = c.g;
    p[BO] = c.b;
    p[AO] = c.a;
}

void draw_line(offscreen_buffer *pBuffer,
               uint32 x0, uint32 y0,
               uint32 x1, uint32 y1,
               const color &c)
{
    uint32 h = (y1 > y0 ? y1 - y0 : y0 - y1);
    uint32 w = (x1 > x0 ? x1 - x0 : x0 - x1);

    draw_pixel(pBuffer, x0, y0, c);
    draw_pixel(pBuffer, x1, y1, c);

    if (w > h)
    {
        uint32 tmp;
        if (x0 > x1)
        {
            tmp = x0;
            x0 = x1;
            x1 = tmp;

            tmp = y0;
            y0 = y1;
            y1 = tmp;
        }

        float k = static_cast<float>(h) / static_cast<float>(w);
        if (y0 > y1) k = -k;

        for (uint32 x = 1; x < w; ++x)
        {
            uint32 y = y0 + static_cast<uint32>(k * x);
            draw_pixel(pBuffer, x + x0, y, c);
        }
    }
    else
    {
        uint32 tmp;
        if (y0 > y1)
        {
            tmp = x0;
            x0 = x1;
            x1 = tmp;

            tmp = y0;
            y0 = y1;
            y1 = tmp;
        }

        float k = static_cast<float>(w) / static_cast<float>(h);
        if (x0 > x1) k = -k;

        for (uint32 y = 1; y < h; ++y)
        {
            uint32 x = x0 + static_cast<uint32>(k * y);
            draw_pixel(pBuffer, x, y + y0, c);
        }
    }
}

void fill_debug_gradient(offscreen_buffer *buffer)
{
    uint8 *ptr = buffer->data;
    uint32 pitch = buffer->width * 4;
    for (uint32 row = 0; row < buffer->height; ++row)
    {
        for (uint32 col = 0; col < buffer->width; ++col)
        {
            ptr[RO] = col % 256;
            ptr[GO] = row % 256;
            ptr[BO] = 0;
            ptr[AO] = 255;
            ptr += 4;
        }
    }
}

void fill_color(offscreen_buffer *buffer,
                uint8 r, uint8 g, uint8 b, uint8 a)
{
    uint8 *ptr = buffer->data;
    uint8 *end  = ptr + buffer->height * buffer->width * 4;
    while (ptr != end)
    {
        ptr[RO] = r;
        ptr[GO] = g;
        ptr[BO] = b;
        ptr[AO] = a;
        ptr += 4;
    }
}

inline float sample_grayscale_texture(const uint8 *pixels,
                                      uint32 w, uint32 h,
                                      float u, float v)
{
    uint32 r = static_cast<uint32>(h * v);
    uint32 c = static_cast<uint32>(w * u);

    uint8 l = pixels[r * w + c];

    return static_cast<float>(l) / 255.0f;
}

inline void draw_solid_hor_line(offscreen_buffer *buffer,
                                uint32 x0, uint32 x1,
                                uint32 y, uint32 col)
{
    uint32 *b = reinterpret_cast<uint32*>(buffer->data +
                                          y * buffer->width * 4 +
                                          x0 * 4);
    uint32 *e = b + (x1 - x0);
    uint32 *p = b;
    while (p != e)
    {
        *p = col;
        ++p;
    }
}

void draw_solid_ver_line(offscreen_buffer *buffer,
                         uint32 x, uint32 y0, uint32 y1,
                         uint32 col)
{
    uint32 *b = reinterpret_cast<uint32*>(buffer->data +
                                          y0 * buffer->width * 4 +
                                          x * 4);
    uint32 *e = b + (y1 - y0) * buffer->width;
    uint32 *p = b;
    while (p != e)
    {
        *p = col;
        p += buffer->width;
    }
}

void draw_solid_rect(offscreen_buffer *buffer,
                     uint32 x0, uint32 x1,
                     uint32 y0, uint32 y1,
                     uint32 col)
{
    for (uint32 y = y0; y < y1; ++y)
    {
        draw_solid_hor_line(buffer, x0, x1, y, col);
    }
}

void draw_frame_rect(offscreen_buffer *buffer,
                     uint32 x0, uint32 x1,
                     uint32 y0, uint32 y1,
                     uint32 col)
{
    draw_solid_ver_line(buffer, x0,  y0, y1, col);
    draw_solid_ver_line(buffer, x1,  y0, y1, col);
    draw_solid_hor_line(buffer, x0, x1,  y0, col);
    draw_solid_hor_line(buffer, x0, x1,  y1, col);
}

void draw_grayscale_image(offscreen_buffer *buffer, uint32 x0, uint32 y0,
                          const uint8 *pixels, uint32 width, uint32 height)
{
    for (uint32 r = 0; r < height; ++r)
        for (uint32 c = 0; c < width; ++c)
        {
            uint8 a = pixels[width * r + c];
            if (x0 + c < buffer->width && y0 + r < buffer->height)
                draw_pixel(buffer, x0 + c, y0 + r, color(a, a, a, 255));
        }
}

void clear_buffer(offscreen_buffer *buffer, const color &col)
{
    for (uint32 y = 0; y < buffer->height; ++y)
    {
        draw_solid_hor_line(buffer, 0, buffer->width, y, pack_color(col));
    }
}


