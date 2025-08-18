#pragma once
#include "imgui.h"
#include <xhl/debug.h>

typedef enum LayoutType
{
    // Names are similar CSS 'justify-content' and behave the same
    LAYOUT_START, // If horizontal, start == left. If vertical, start == top
    LAYOUT_CENTRE,
    LAYOUT_END,

    LAYOUT_SPACE_EVENLY,
    LAYOUT_SPACE_AROUND,
    LAYOUT_SPACE_BETWEEN,
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

// Fixed padding. Assumes sizes are already set
static void layout_horizontal(imgui_rect* rects, int num_rects, LayoutType layout, float target_x, float padding)
{
    xassert(num_rects > 0);
    xassert(layout == LAYOUT_START || layout == LAYOUT_CENTRE || layout == LAYOUT_END);

    float total_width = 0;
    for (imgui_rect* it = rects; it != rects + num_rects; it++)
        total_width += (rects->r - rects->x);

    total_width += padding * (num_rects - 1);

    float x_acc = target_x;
    if (layout == LAYOUT_CENTRE)
        x_acc -= total_width * 0.5f;
    if (layout == LAYOUT_END)
        x_acc -= total_width;

    for (imgui_rect* it = rects; it != rects + num_rects; it++)
    {
        float w = (it->r - it->x);
        it->x   = x_acc;
        it->r   = x_acc + w;

        x_acc += w + padding;
    }
}

// Dynamic padding. Assumes sizes are already set
static void layout_horizontal_fill(imgui_rect* rects, int num_rects, LayoutType layout, const imgui_rect* box)
{
    xassert(num_rects > 0);
    xassert(layout == LAYOUT_SPACE_EVENLY || layout == LAYOUT_SPACE_AROUND || layout == LAYOUT_SPACE_BETWEEN);

    float total_content_width = 0;
    for (imgui_rect* it = rects; it != rects + num_rects; it++)
        total_content_width += (it->r - it->x);

    const float box_width = box->r - box->x;
    xassert(total_content_width <= box_width); // Oops, you ran out of space!

    if (num_rects == 1)
    {
        // Centre content
        rects->x = box->x + box_width * 0.5f - total_content_width * -0.5f;
        rects->r = box->x + box_width * 0.5f - total_content_width;
    }
    else
    {
        const float available_padding = box_width - total_content_width;
        float       padding           = 0;
        int         padding_denom     = num_rects;
        if (layout == LAYOUT_SPACE_BETWEEN)
            padding_denom--;
        if (layout == LAYOUT_SPACE_EVENLY)
            padding_denom++;
        padding = available_padding / (float)padding_denom;

        float x = box->x; // default space between
        if (layout == LAYOUT_SPACE_AROUND)
            x += padding * 0.5f;
        if (layout == LAYOUT_SPACE_EVENLY)
            x += padding;

        int i = 0;
        for (imgui_rect* it = rects; it != rects + num_rects; it++)
        {
            float w = (it->r - it->x);
            it->x   = x;
            it->r   = x + w;

            x += w + padding;
            xassert(it->r <= box->r);
            i++;
        }
    }
}

// Fixed padding. Assumes sizes are already set
static void layout_vertical(imgui_rect* rects, int num_rects, LayoutType layout, float target_y, float padding)
{
    xassert(num_rects > 0);

    float total_height = 0;
    for (imgui_rect* it = rects; it != rects + num_rects; it++)
        total_height += (rects->b - rects->y);

    total_height += padding * (num_rects - 1);

    xassert(layout == LAYOUT_START || layout == LAYOUT_CENTRE || layout == LAYOUT_END);
    float y_acc = target_y;
    if (layout == LAYOUT_CENTRE)
        y_acc -= total_height * 0.5f;
    if (layout == LAYOUT_END)
        y_acc -= total_height;

    for (imgui_rect* it = rects; it != rects + num_rects; it++)
    {
        float h = (it->b - it->y);
        it->y   = y_acc;
        it->b   = y_acc + h;

        y_acc += h + padding;
    }
}

// Fixed padding. Sets sizes
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

    xassert(layout == LAYOUT_START || layout == LAYOUT_CENTRE || layout == LAYOUT_END);
    float x_acc = target_x;
    if (layout == LAYOUT_CENTRE)
        x_acc -= total_width * 0.5f;
    if (layout == LAYOUT_END)
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

// Fixed padding. Sets sizes
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

    xassert(layout == LAYOUT_START || layout == LAYOUT_CENTRE || layout == LAYOUT_END);
    float y_acc = target_y;
    if (layout == LAYOUT_CENTRE)
        y_acc -= total_height * 0.5f;
    if (layout == LAYOUT_END)
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
