#pragma once
#include "imgui.h"
#include <xhl/debug.h>

typedef enum LayoutType
{
    LAYOUT_HL,
    LAYOUT_HC,
    LAYOUT_HR,
    LAYOUT_VT,
    LAYOUT_VC,
    LAYOUT_VB,
} LayoutType;

static void layout_set_size(imgui_rect* rects, int num_rects, float w, float h)
{
    for (imgui_rect* it = rects; it != rects + num_rects; it++)
    {
        it->r = it->x + w;
        it->b = it->y + h;
    }
}

static void layout_translate(imgui_rect* rects, int num_rects, float x, float y)
{
    for (imgui_rect* it = rects; it != rects + num_rects; it++)
    {
        it->x += x;
        it->r += x;
        it->y += y;
        it->b += y;
    }
}

static void layout_translate_x(imgui_rect* rects, int num_rects, float x)
{
    for (imgui_rect* it = rects; it != rects + num_rects; it++)
    {
        it->x += x;
        it->r += x;
    }
}

static void layout_translate_y(imgui_rect* rects, int num_rects, float y)
{
    for (imgui_rect* it = rects; it != rects + num_rects; it++)
    {
        it->y += y;
        it->b += y;
    }
}

static void layout_horizontal(imgui_rect* rects, int num_rects, float target_x, LayoutType layout, float padding)
{
    xassert(num_rects > 0);

    float total_width = 0;
    for (imgui_rect* it = rects; it != rects + num_rects; it++)
        total_width += (rects->r - rects->x);

    total_width += padding * (num_rects - 1);

    xassert(layout == LAYOUT_HL || layout == LAYOUT_HC || layout == LAYOUT_HR);
    float x_acc = target_x;
    if (layout == LAYOUT_HC)
        x_acc -= total_width * 0.5f;
    if (layout == LAYOUT_HR)
        x_acc -= total_width;

    for (imgui_rect* it = rects; it != rects + num_rects; it++)
    {
        float w = (it->r - it->x);
        it->x   = x_acc;
        it->r   = x_acc + w;

        x_acc += w + padding;
    }
}

static void layout_vertical(imgui_rect* rects, int num_rects, float target_y, LayoutType layout, float padding)
{
    xassert(num_rects > 0);

    float total_height = 0;
    for (imgui_rect* it = rects; it != rects + num_rects; it++)
        total_height += (rects->b - rects->y);

    total_height += padding * (num_rects - 1);

    xassert(layout == LAYOUT_VT || layout == LAYOUT_VC || layout == LAYOUT_VB);
    float y_acc = target_y;
    if (layout == LAYOUT_VC)
        y_acc -= total_height * 0.5f;
    if (layout == LAYOUT_VB)
        y_acc -= total_height;

    for (imgui_rect* it = rects; it != rects + num_rects; it++)
    {
        float h = (it->b - it->y);
        it->y   = y_acc;
        it->b   = y_acc + h;

        y_acc += h + padding;
    }
}

static void layout_uniform_horizontal(
    imgui_rect* rects,
    int         num_rects,
    float       target_x,
    float       target_y,
    float       target_width,
    float       target_height,
    LayoutType  layout,
    float       padding)
{
    float total_width = target_width * num_rects + padding * (num_rects - 1);

    xassert(layout == LAYOUT_HL || layout == LAYOUT_HC || layout == LAYOUT_HR);
    float x_acc = target_x;
    if (layout == LAYOUT_HC)
        x_acc -= total_width * 0.5f;
    if (layout == LAYOUT_HR)
        x_acc -= total_width;

    for (imgui_rect* it = rects; it != rects + num_rects; it++)
    {
        it->x = x_acc;
        it->y = target_y;
        it->r = x_acc + target_width;
        it->b = target_y + target_height;

        x_acc += target_width + padding;
    }
}

static void layout_uniform_vertical(
    imgui_rect* rects,
    int         num_rects,
    float       target_x,
    float       target_y,
    float       target_width,
    float       target_height,
    LayoutType  layout,
    float       padding)
{
    float total_height = target_height * num_rects + padding * (num_rects - 1);

    xassert(layout == LAYOUT_VT || layout == LAYOUT_VC || layout == LAYOUT_VB);
    float y_acc = target_y;
    if (layout == LAYOUT_VC)
        y_acc -= total_height * 0.5f;
    if (layout == LAYOUT_VB)
        y_acc -= total_height;

    for (imgui_rect* it = rects; it != rects + num_rects; it++)
    {
        it->x = target_x;
        it->y = y_acc;
        it->r = target_x + target_width;
        it->b = y_acc + target_height;

        y_acc += target_height + padding;
    }
}
