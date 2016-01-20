inline void draw_pixel(offscreen_buffer *buffer,
                       u32 x, u32 y, u32 c)
{
    if (x < buffer->width && y < buffer->height)
    {
        u32 *p = reinterpret_cast<u32*>(buffer->data);
        p += buffer->width * y;
        p += x;
        *p = c;
    }
}

void draw_line(offscreen_buffer *pBuffer,
               u32 x0, u32 y0,
               u32 x1, u32 y1,
               u32 c)
{
    u32 h = (y1 > y0 ? y1 - y0 : y0 - y1);
    u32 w = (x1 > x0 ? x1 - x0 : x0 - x1);

    draw_pixel(pBuffer, x0, y0, c);
    draw_pixel(pBuffer, x1, y1, c);

    if (w > h)
    {
        u32 tmp;
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

        for (u32 x = 1; x < w; ++x)
        {
            u32 y = y0 + static_cast<u32>(k * x);
            draw_pixel(pBuffer, x + x0, y, c);
        }
    }
    else
    {
        u32 tmp;
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

        for (u32 y = 1; y < h; ++y)
        {
            u32 x = x0 + static_cast<u32>(k * y);
            draw_pixel(pBuffer, x, y + y0, c);
        }
    }
}

void fill_debug_gradient(offscreen_buffer *buffer)
{
    u8 *ptr = buffer->data;
    for (u32 row = 0; row < buffer->height; ++row)
    {
        for (u32 col = 0; col < buffer->width; ++col)
        {
            ptr[RO] = col & 0xFF;
            ptr[GO] = row & 0xFF;
            ptr[BO] = 0;
            ptr[AO] = 255;
            ptr += 4;
        }
    }
}

void fill_color(offscreen_buffer *buffer,
                u8 r, u8 g, u8 b, u8 a)
{
    u8 *ptr = buffer->data;
    u8 *end  = ptr + buffer->height * buffer->width * 4;
    while (ptr != end)
    {
        ptr[RO] = r;
        ptr[GO] = g;
        ptr[BO] = b;
        ptr[AO] = a;
        ptr += 4;
    }
}

inline float sample_grayscale_texture(const u8 *pixels,
                                      u32 w, u32 h,
                                      float u, float v)
{
    u32 r = static_cast<u32>(h * v);
    u32 c = static_cast<u32>(w * u);

    u8 l = pixels[r * w + c];

    return static_cast<float>(l) / 255.0f;
}

inline void draw_solid_hor_line(offscreen_buffer *buffer,
                                u32 x0, u32 x1,
                                u32 y, u32 col)
{
    u32 *b = reinterpret_cast<u32*>(buffer->data +
                                          y * buffer->width * 4 +
                                          x0 * 4);
    u32 *e = b + (x1 - x0);
    u32 *p = b;
    while (p != e)
    {
        *p = col;
        ++p;
    }
}

void draw_solid_ver_line(offscreen_buffer *buffer,
                         u32 x, u32 y0, u32 y1,
                         u32 col)
{
    u32 *b = reinterpret_cast<u32*>(buffer->data +
                                          y0 * buffer->width * 4 +
                                          x * 4);
    u32 *e = b + (y1 - y0) * buffer->width;
    u32 *p = b;
    while (p != e)
    {
        *p = col;
        p += buffer->width;
    }
}

void draw_solid_rect(offscreen_buffer *buffer,
                     u32 x0, u32 x1,
                     u32 y0, u32 y1,
                     u32 col)
{
    for (u32 y = y0; y < y1; ++y)
    {
        draw_solid_hor_line(buffer, x0, x1, y, col);
    }
}

void draw_frame_rect(offscreen_buffer *buffer,
                     u32 x0, u32 x1,
                     u32 y0, u32 y1,
                     u32 col)
{
    draw_solid_ver_line(buffer, x0,  y0, y1, col);
    draw_solid_ver_line(buffer, x1,  y0, y1, col);
    draw_solid_hor_line(buffer, x0, x1,  y0, col);
    draw_solid_hor_line(buffer, x0, x1,  y1, col);
}

void draw_grayscale_image(offscreen_buffer *buffer, u32 x0, u32 y0,
                          const u8 *pixels, u32 width, u32 height)
{
    for (u32 r = 0; r < height; ++r)
        for (u32 c = 0; c < width; ++c)
        {
            u8 a = pixels[width * r + c];
            u32 col = pack_color(color(a, a, a, 255));
            draw_pixel(buffer, x0 + c, y0 + r, col);
        }
}

void clear_buffer(offscreen_buffer *buffer, const color &col)
{
    for (u32 y = 0; y < buffer->height; ++y)
    {
        draw_solid_hor_line(buffer, 0, buffer->width, y, pack_color(col));
    }
}


