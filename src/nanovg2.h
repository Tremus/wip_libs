//
// Copyright (c) 2013 Mikko Mononen memon@inside.org
//
// This software is provided 'as-is', without any express or implied
// warranty.  In no event will the authors be held liable for any damages
// arising from the use of this software.
// Permission is granted to anyone to use this software for any purpose,
// including commercial applications, and to alter it and redistribute it
// freely, subject to the following restrictions:
// 1. The origin of this software must not be misrepresented; you must not
//    claim that you wrote the original software. If you use this software
//    in a product, an acknowledgment in the product documentation would be
//    appreciated but is not required.
// 2. Altered source versions must be plainly marked as such, and must not be
//    misrepresented as being the original software.
// 3. This notice may not be removed or altered from any source distribution.
//

// MIT License

// Copyright (c) 2024 void256

// Permission is hereby granted, free of charge, to any person obtaining a copy
// of this software and associated documentation files (the "Software"), to deal
// in the Software without restriction, including without limitation the rights
// to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
// copies of the Software, and to permit persons to whom the Software is
// furnished to do so, subject to the following conditions:

// The above copyright notice and this permission notice shall be included in all
// copies or substantial portions of the Software.

// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
// AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
// OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
// SOFTWARE.

// Taken from https://github.com/void256/nanovg_sokol.h
// see https://github.com/floooh/sokol/issues/633 for history
//
// @darkuranium
// - initial version
//
// @zeromake
// - sampler support
// - additional changes
//
// @void256 24.10.2024
// - inline shaders
// - fix partial texture updates (and with it text rendering)
// - optional debug trace logging
//
// @void256 25.10.2024
// - embedded original .glsl files and awk command to extract them to single files
// - clean up sgnvg__renderUpdateTexture and fix issues when x0 is not 0
//
// @void256 26.10.2024
// - don't call sg_update_image() when creating texture in sgnvg__renderCreateTexture()
// - renamed debug trace logging macros and added "SGNVG_" prefix there as well
//
// @Tremus 2025
// - Fixed memory leaks
// - Created 'command' list for supporting multiple render passes and custom shaders
// - Merged with nanovg
// - Remove encapsulation of data structures
// - Create 'drity' flag for textures to only update them on new data
// - Add snvgCreateFramebuffer() & snvgDestroyFramebuffer()
// - Move sampler support out of snvgCreateImageFromHandleSokol and into to NVGpaint
// - Remove linear and mipmap image flags
// - Add bloom and blur post processing effects
// - Basic out of order rendering

/*
Notes to self on resolution, DPI, backingScaleFactor, logical/physical pixels etc.

Windows:
    When users go to Start > Settings > Display > Scale > 125%, Windows will send a WM_DPICHANGED event to all top level
    windows, and programmers are expected to scale their content according to HIWORD(wParam) / USER_DEFAULT_SCREEN_DPI.
    Usually this is calculated like this:
        float contentScale = 96 / 96; // 100%
        float contentScale = 120 / 96; // 125%
        float contentScale = 144 / 96; // 150%
    This scaling is useful for users who want to make thing like text bigger. Developers can of course ignore all of
    this an applications will work fine.
    Supporting content scaling can be tricky. Many things like font sizes, heights and widths of padding may all need to
    be multiplied by this scale. This may result in a lot of extra code with a developer scaling every last element by
    this size. It also introduces the risk of forgetting to scale an element, which will result in many large elements
    in a GUI with out of place small elements.

    When querying for the width/height of monitors, Windows will always honestly report the number of pixels
    eg.
        RECT Rect;
        GetWindowRect(GetDesktopWindow(), &Rect);

    On Windows, multisample antialiasing tricks are optional and you have to go out of your way to set it all up

macOS:
    Apple confuse users and developers with jargon like "Logical pixels" and "physical pixels". I will attempt to
    explain my understanding of both, but first, suspend your own understanding of them because I believe most people
    writing about the topic are uninformed or misinformed. Note that I may also be wrong because macOS is largely
    undocumented, making learning about their systems difficult, and Apple are sometimes secretive.

        "Logical pixels"
        When users go to Settings > Displays > "Larger text" or "More space", users think they are resizing/scaling
        their entire screen in pixels. Apps are expected to pretend the screen they are on is this scaled size. At a
        high level this is convenient, apps can tell some graphics library "render this text 12px" and the renderer
        will automagically scale that text under the hood to the appropriate size on the screen the user wants.

        "Physical pixels"
        This is simply `logicalpixels * NSScreen.backingScaleFactor;`. Many Apple devices have a thing they call "Retina
        Screens", which is a fancy way of saying the monitor hardware has a high pixel density and software performance
        mutlisample antiasliasing. Very often `NSScreen.backingScaleFactor` is set to `2`. Others online say this can
        sometimes be set to 1 or 3 depending on the device.

        Some info online may mislead you into thinking this is how you calculate the size in pixels of the monitor. In
        many cases this is not true. For example, my M1 MB Air has a pixel resolution of 2560x1600, but resultion
        options within macOS settings are:
            - 1680x1050
            - 1440x900
            - 1280x800
            - 1024x640
        Only one of those options when multiplied by backingScaleFactor (2) will equal the actual pixel resolution. So
        much advice found elsewhere online is wildly misinformed. Apple do not appear to be using the "physical pixels"
        terminology, only other misinformed developers. Be aware of this when you read anything about it online.

        Apples compositing system for their windows want to deal with actual buffer sizes 1x/2x/3x of whatever logical
        pixels they represent.

    When querying for the width/height of monitors, macOS will report results in logical pixels. It's possible to use
    some clever tricks to obtain the real size in pixels of the monitor without any physical (backing store) or logical
    (user configured scale) pixel confusion, but I'm unsure whether this is useful in many cases.

    All of apples advice is to use their scaling methods, including when using Metal
    https://developer.apple.com/documentation/metal/managing-your-game-window-for-metal-in-macos

    From my own testing, the swapchain in metal expects to be controlled using logical pixels. What's a little
    unintuitive is that framebuffers don't have this backingScaleFactor applied to them, so you have to perform the
    scaling of the framebuffer size, and all drawing and scissoring applied to it yourself.

    By selectively using this backing scale factor, Apple appear to have a built in multisample aliasing system they
    want you to conform to.

Now how can we wrap all of this in a crossplatform way?

It doesn't appear to be a good solution for Windows to similarly choose to use a kind of logical pixels here. Heights of
elements that are 10px tall won't multiply by 1.25 very well if the edges are expected to be sharp (a height of 12.5px
may cause a blurry edge at the top or buttom). If you want sharp edges, you have to floor all these multiplied values.
This can cause elements that are meant to be centred to be 1px off.

I don't have a neat solution to this atm but when I do hopefully I can remember to write about it here
*/

#ifndef NANOVG_H
#define NANOVG_H

#include <sokol_gfx.h>
#include <stb_rect_pack.h>

#include "linked_arena.h"

#ifdef __cplusplus
extern "C" {
#endif

#define NVG_PI 3.14159265358979323846264338327f

#ifndef NVG_MAX_STATES
#define NVG_MAX_STATES 32
#endif
// #define NVG_MAX_FONTIMAGES 4

#ifdef _MSC_VER
#pragma warning(push)
#pragma warning(disable : 4201) // nonstandard extension used : nameless struct/union
#endif

#ifndef NDEBUG
#define NVG_LABEL(txt) txt
#else
#define NVG_LABEL(...) 0
#endif

#define NVG_ARRLEN(arr) (sizeof(arr) / sizeof(0 [arr]))

#if !defined(NVG_FONT_STB_TRUETYPE) && !defined(NVG_FONT_FREETYPE_SINGLECHANNEL) &&                                    \
    !defined(NVG_FONT_FREETYPE_MULTICHANNEL)
// #define NVG_FONT_STB_TRUETYPE
// TODO: fix blending in multichannel
// #define NVG_FONT_FREETYPE_MULTICHANNEL
#define NVG_FONT_FREETYPE_SINGLECHANNEL
#endif
#if defined(NVG_FONT_FREETYPE_SINGLECHANNEL) || defined(NVG_FONT_FREETYPE_MULTICHANNEL)
#define NVG_FONT_FREETYPE
#endif

#if defined(NVG_FONT_STB_TRUETYPE)
#include <stb_truetype.h>
#endif

#include <text.glsl.h>

typedef struct NVGcolour
{
    union
    {
        float rgba[4];
        struct
        {
            float r, g, b, a;
        };
    };
} NVGcolour;

typedef struct NVGpaint
{
    float      xform[6];
    float      extent[2];
    float      radius;
    float      feather;
    NVGcolour  innerColour;
    NVGcolour  outerColour;
    sg_view    texview;
    sg_sampler smp;
} NVGpaint;

enum NVGwinding
{
    NVG_CCW = 1, // Winding for solid shapes
    NVG_CW  = 2, // Winding for holes
};

enum NVGsolidity
{
    NVG_SOLID = 1, // CCW
    NVG_HOLE  = 2, // CW
};

enum NVGlineCap
{
    NVG_BUTT,
    NVG_ROUND,
    NVG_SQUARE,
    NVG_BEVEL,
    NVG_MITER,
};

enum NVGalign
{
    // Horizontal align
    NVG_ALIGN_LEFT   = 0,      // Default, align text horizontally to left.
    NVG_ALIGN_CENTRE = 1 << 0, // Align text horizontally to centre.
    NVG_ALIGN_RIGHT  = 1 << 1, // Align text horizontally to right.
                               // Vertical align

    NVG_ALIGN_BASELINE = 0,      // Default, align text vertically to baseline.
    NVG_ALIGN_TOP      = 1 << 2, // Align text vertically to top.
    NVG_ALIGN_MIDDLE   = 1 << 3, // Align text vertically to middle.
    NVG_ALIGN_BOTTOM   = 1 << 4, // Align text vertically to bottom.

    // The following use ascender and descender information from the glyphs in the top & bottom rows to calculate a
    // centre alignment
    NVG_ALIGN_TOP_TIGHT    = 1 << 5,
    NVG_ALIGN_MIDDLE_TIGHT = 1 << 6,
    NVG_ALIGN_BOTTOM_TIGHT = 1 << 7,

    NVG_ALIGN_TL = (NVG_ALIGN_TOP | NVG_ALIGN_LEFT),
    NVG_ALIGN_TC = (NVG_ALIGN_TOP | NVG_ALIGN_CENTRE),
    NVG_ALIGN_TR = (NVG_ALIGN_TOP | NVG_ALIGN_RIGHT),

    NVG_ALIGN_CL = (NVG_ALIGN_MIDDLE | NVG_ALIGN_LEFT),
    NVG_ALIGN_CC = (NVG_ALIGN_MIDDLE | NVG_ALIGN_CENTRE),
    NVG_ALIGN_CR = (NVG_ALIGN_MIDDLE | NVG_ALIGN_RIGHT),

    NVG_ALIGN_BL = (NVG_ALIGN_BOTTOM | NVG_ALIGN_LEFT),
    NVG_ALIGN_BC = (NVG_ALIGN_BOTTOM | NVG_ALIGN_CENTRE),
    NVG_ALIGN_BR = (NVG_ALIGN_BOTTOM | NVG_ALIGN_RIGHT),

    NVG_ALIGN_TL_TIGHT = (NVG_ALIGN_TOP_TIGHT | NVG_ALIGN_LEFT),
    NVG_ALIGN_TC_TIGHT = (NVG_ALIGN_TOP_TIGHT | NVG_ALIGN_CENTRE),
    NVG_ALIGN_TR_TIGHT = (NVG_ALIGN_TOP_TIGHT | NVG_ALIGN_RIGHT),

    NVG_ALIGN_CL_TIGHT = (NVG_ALIGN_MIDDLE_TIGHT | NVG_ALIGN_LEFT),
    NVG_ALIGN_CC_TIGHT = (NVG_ALIGN_MIDDLE_TIGHT | NVG_ALIGN_CENTRE),
    NVG_ALIGN_CR_TIGHT = (NVG_ALIGN_MIDDLE_TIGHT | NVG_ALIGN_RIGHT),

    NVG_ALIGN_BL_TIGHT = (NVG_ALIGN_BOTTOM_TIGHT | NVG_ALIGN_LEFT),
    NVG_ALIGN_BC_TIGHT = (NVG_ALIGN_BOTTOM_TIGHT | NVG_ALIGN_CENTRE),
    NVG_ALIGN_BR_TIGHT = (NVG_ALIGN_BOTTOM_TIGHT | NVG_ALIGN_RIGHT),
};

enum NVGblendFactor
{
    NVG_ZERO                 = 1 << 0,
    NVG_ONE                  = 1 << 1,
    NVG_SRC_COLOUR           = 1 << 2,
    NVG_ONE_MINUS_SRC_COLOUR = 1 << 3,
    NVG_DST_COLOUR           = 1 << 4,
    NVG_ONE_MINUS_DST_COLOUR = 1 << 5,
    NVG_SRC_ALPHA            = 1 << 6,
    NVG_ONE_MINUS_SRC_ALPHA  = 1 << 7,
    NVG_DST_ALPHA            = 1 << 8,
    NVG_ONE_MINUS_DST_ALPHA  = 1 << 9,
    NVG_SRC_ALPHA_SATURATE   = 1 << 10,
};

enum NVGcompositeOperation
{
    NVG_SOURCE_OVER,
    NVG_SOURCE_IN,
    NVG_SOURCE_OUT,
    NVG_ATOP,
    NVG_DESTINATION_OVER,
    NVG_DESTINATION_IN,
    NVG_DESTINATION_OUT,
    NVG_DESTINATION_ATOP,
    NVG_LIGHTER,
    NVG_COPY,
    NVG_XOR,
};

typedef struct NVGcompositeOperationState
{
    int srcRGB;
    int dstRGB;
    int srcAlpha;
    int dstAlpha;
} NVGcompositeOperationState;

/*
typedef struct NVGglyphPosition
{
    // const char* str;        // Position of the glyph in the input string.
    // float       x;          // The x-coordinate of the logical glyph position.
    int16_t minx, maxx; // The bounds of the glyph shape.
} NVGglyphPosition;

typedef struct NVGtextRow
{
    const char* start; // Pointer to the input text where the row starts.
    const char* end;   // Pointer to the input text where the row ends (one past the last character).
    const char* next;  // Pointer to the beginning of the next row.
    float       width; // Logical width of the row.
    float minx, maxx;  // Actual bounds of the row. Logical with and bounds can differ because of kerning and some parts
                       // over extending.
} NVGtextRow;
 */

enum NVGimageFlags
{
    NVG_IMAGE_DIRTY      = 1 << 0, // Force update image on GPU
    NVG_IMAGE_CPU_UPDATE = 1 << 1, // Will you be regularly updating this with data from the CPU?
    NVG_IMAGE_IMMUTABLE  = 1 << 2, // Forbids updating the image
    NVG_IMAGE_NODELETE   = 1 << 4, // Do not delete Sokol image.
};

//
// Internal Render API
//
enum NVGtexture
{
    NVG_TEXTURE_ALPHA = 0x01,
    NVG_TEXTURE_RGBA  = 0x02,
};

typedef struct NVGscissor
{
    float xform[6];
    float extent[2];
} NVGscissor;

typedef struct NVGvertex
{
    float x, y, u, v;
} NVGvertex;

typedef struct NVGpath
{
    int           first;
    int           count;
    unsigned char closed;
    int           nbevel;
    NVGvertex*    fill;
    int           nfill;
    NVGvertex*    stroke;
    int           nstroke;
    int           winding;
    int           convex;
} NVGpath;

enum NVGcommands
{
    NVG_MOVETO   = 0,
    NVG_LINETO   = 1,
    NVG_BEZIERTO = 2,
    NVG_CLOSE    = 3,
    NVG_WINDING  = 4,
};

enum NVGpointFlags
{
    NVG_PT_CORNER     = 0x01,
    NVG_PT_LEFT       = 0x02,
    NVG_PT_BEVEL      = 0x04,
    NVG_PR_INNERBEVEL = 0x08,
};

typedef struct NVGstate
{
    NVGcompositeOperationState compositeOperation;

    int        shapeAntiAlias;
    NVGpaint   paint;
    float      miterLimit;
    int        lineJoin;
    int        lineCap;
    float      xform[6];
    NVGscissor scissor;
    float      fontSize;
    // float                      letterSpacing;
    float lineHeight;
    // float                      fontBlur;
    int textAlign;
    int fontId;
} NVGstate;

typedef struct NVGpoint
{
    float         x, y;
    float         dx, dy;
    float         len;
    float         dmx, dmy;
    unsigned char flags;
} NVGpoint;

typedef struct NVGpathCache
{
    NVGpoint*  points;
    int        npoints;
    int        cpoints;
    NVGpath*   paths;
    int        npaths;
    int        cpaths;
    NVGvertex* verts;
    int        nverts;
    int        cverts;
    float      bounds[4];
} NVGpathCache;

// Create flags

enum NVGcreateFlags
{
    // Flag indicating if geometry based anti-aliasing is used (may not be needed when using MSAA).
    NVG_ANTIALIAS = 1 << 0,
    // Flag indicating if strokes should be drawn using stencil buffer. The rendering will be a little
    // slower, but path overlaps (i.e. self-intersecting or sharp turns) will be drawn just once.
    NVG_STENCIL_STROKES = 1 << 1,
    // Flag indicating that additional debug checks are done.
    NVG_DEBUG = 1 << 2,
};

enum SGNVGshaderType
{
    NSVG_SHADER_FILLGRAD,
    NSVG_SHADER_FILLIMG,
    NSVG_SHADER_SIMPLE,
    NSVG_SHADER_IMG
};

typedef struct SGNVGtexture
{
    sg_image img;
    sg_view  texview;
    int      type;
    int      width, height;
    int      flags;
    uint8_t* imgData;
} SGNVGtexture;

typedef struct SGNVGframebuffer
{
    sg_image img;
    sg_view  img_colview; // View for writing to
    sg_view  img_texview; // View for reading from
    sg_image depth;
    sg_view  depth_view;

    int width;
    int height;
    // In theory this is a double value found on NSWindow and NSScreen, but in practice this is strictly 1, 2, or 3
    int backingScaleFactor;
} SGNVGframebuffer;

typedef struct SGNVGimageFX
{
    SGNVGframebuffer  resolve;
    float             max_radius_px;
    int               max_mip_levels;
    SGNVGframebuffer* mip_levels;
    SGNVGframebuffer* interp_levels;
} SGNVGimageFX;

typedef struct SGNVGblend
{
    sg_blend_factor srcRGB;
    sg_blend_factor dstRGB;
    sg_blend_factor srcAlpha;
    sg_blend_factor dstAlpha;
} SGNVGblend;

enum SGNVGcallType
{
    SGNVG_NONE = 0,
    SGNVG_FILL,
    SGNVG_CONVEXFILL,
    SGNVG_STROKE,
    SGNVG_TRIANGLES,
};

typedef struct SGNVGpath
{
    int fillOffset;
    int fillCount;
    int strokeOffset;
    int strokeCount;
} SGNVGpath;

typedef struct SGNVGattribute
{
    float vertex[2];
    float tcoord[2];
} SGNVGattribute;

typedef struct SGNVGvertUniforms
{
    float viewSize[4];
} SGNVGvertUniforms;

typedef struct SGNVGfragUniforms
{
#define NANOVG_SG_UNIFORMARRAY_SIZE 11
    union
    {
        struct
        {
            float            scissorMat[12]; // matrices are actually 3 vec4s
            float            paintMat[12];
            struct NVGcolour innerCol;
            struct NVGcolour outerCol;
            float            scissorExt[2];
            float            scissorScale[2];
            float            extent[2];
            float            radius;
            float            feather;
            float            strokeMult;
            float            strokeThr;
            float            texType;
            float            type;
        };
        float uniformArray[NANOVG_SG_UNIFORMARRAY_SIZE][4];
    };
} SGNVGfragUniforms;

// LRU cache; keep its size relatively small, as items are accessed via a linear search
#define NANOVG_SG_PIPELINE_CACHE_SIZE 32

typedef struct SGNVGpipelineCacheKey
{
    uint16_t blend;   // cached as `src_factor_rgb | (dst_factor_rgb << 4) | (src_factor_alpha << 8) | (dst_factor_alpha
                      // << 12)`
    uint16_t lastUse; // updated on each read
} SGNVGpipelineCacheKey;

enum SGNVGpipelineType
{
    // used by sgnvg__convexFill, sgnvg__stroke, sgnvg__triangles
    SGNVG_PIP_BASE = 0,

    // used by sgnvg__fill
    SGNVG_PIP_FILL_STENCIL,
    SGNVG_PIP_FILL_ANTIALIAS, // only used if sg->flags & NVG_ANTIALIAS
    SGNVG_PIP_FILL_DRAW,

    // used by sgnvg__stroke
    SGNVG_PIP_STROKE_STENCIL_DRAW,      // only used if sg->flags & NVG_STENCIL_STROKES
    SGNVG_PIP_STROKE_STENCIL_ANTIALIAS, // only used if sg->flags & NVG_STENCIL_STROKES
    SGNVG_PIP_STROKE_STENCIL_CLEAR,     // only used if sg->flags & NVG_STENCIL_STROKES

    SGNVG_PIP_NUM_
};

typedef struct SGNVGpipelineCache
{
    // keys are stored as a separate array for search performance
    SGNVGpipelineCacheKey keys[NANOVG_SG_PIPELINE_CACHE_SIZE];
    sg_pipeline           pipelines[NANOVG_SG_PIPELINE_CACHE_SIZE][SGNVG_PIP_NUM_];
    uint8_t               pipelinesActive[NANOVG_SG_PIPELINE_CACHE_SIZE];
    uint16_t              currentUse; // incremented on each overwrite
} SGNVGpipelineCache;

typedef struct SGNVGcall
{
    enum SGNVGcallType type;
    sg_view            texview;
    sg_sampler         smp;
    int                triangleOffset;
    int                triangleCount;

    SGNVGblend blendFunc;

    int        num_paths;
    SGNVGpath* paths;

    // depending on SGNVGcall.type and NVG_STENCIL_STROKES, this may be 2 consecutive uniforms
    SGNVGfragUniforms* uniforms;

    struct SGNVGcall* next;
} SGNVGcall;

typedef struct SGNVGcommandBeginPass
{
    sg_pass  pass;
    unsigned x, y;
    unsigned width, height;
} SGNVGcommandBeginPass;

typedef struct SGNVGcommandNVG
{
    int               num_calls;
    struct SGNVGcall* calls;
} SGNVGcommandNVG;

typedef struct SGNVGcommandText
{
    int text_buffer_start;
    int text_buffer_end;
    // TODO: support more colours
    NVGcolour colour_fill;
    sg_view   atlas_view;
} SGNVGcommandText;

typedef struct SGNVGcommandImageFX
{
    // These are probably redundant and could be implied by the above params being > 0
    bool apply_lightness_filter;
    bool apply_bloom;

    float lightness_threshold;
    float radius_px;
    float bloom_amount;

    SGNVGframebuffer* src;
    SGNVGimageFX*     fx;
} SGNVGcommandImageFX;

typedef void (*SGNVGcustomFunc)(void* uptr);

typedef struct SGNVGcommandCustom
{
    void*           uptr;
    SGNVGcustomFunc func;
} SGNVGcommandCustom;

enum SGNVGcommandType
{
    SGNVG_CMD_BEGIN_PASS,
    SGNVG_CMD_END_PASS,
    SGNVG_CMD_DRAW_NVG,
    SGNVG_CMD_DRAW_TEXT,
    SGNVG_CMD_IMAGE_FX,
    SGNVG_CMD_CUSTOM,
};

typedef struct SGNVGcommand
{
    enum SGNVGcommandType type;
    const char*           label;
    union
    {
        void* data;

        SGNVGcommandBeginPass* beginPass;
        SGNVGcommandNVG*       drawNVG;
        SGNVGcommandText*      text;
        SGNVGcommandImageFX*   fx;
        SGNVGcommandCustom*    custom;
    } payload;

    struct SGNVGcommand* next;
} SGNVGcommand;

typedef struct NVGfontSlot
{
    void*  kbtr_font_ptr;
    void*  data;
    size_t data_size;
    int    owned;
} NVGfontSlot;

// Used to identify a unique glyph.
// TODO: support multiple fonts
typedef union NVGatlasRectHeader
{
    struct
    {
        uint32_t glyph_index;
        // TODO: this could probably be packed into an integer. To support sizes like 12.25, multiply & divide by 4
        // This could make room for font ids in the header
        float font_size;
    };
    uint64_t data;
} NVGatlasRectHeader;

typedef struct NVGatlasRect
{
    union NVGatlasRectHeader header;

    uint8_t x, y, w, h;

    int16_t advance_x;
    int16_t advance_y;

    int8_t bearing_x;
    int8_t bearing_y;

    sg_view img_view;
} NVGatlasRect;

typedef struct NVGatlas
{
    sg_view img_view;
    bool    dirty;
    bool    full;
} NVGatlas;

typedef struct NVGcontext
{
    LinkedArena* arena;
    void*        arena_top;

    float*       commands;
    int          ccommands;
    int          ncommands;
    float        commandx, commandy;
    NVGstate     state;
    int          edgeAntiAlias;
    NVGpathCache cache;
    float        tessTol;
    float        distTol;
    float        fringeWidth;
    int          backingScaleFactor;

    // Old
    // struct FONScontext* fs;
    // int fontImages[NVG_MAX_FONTIMAGES];
    // int fontImageIdx;

    // New

    struct kbts_shape_context* kbts;

#ifdef NVG_FONT_FREETYPE
    struct FT_LibraryRec_* ft_lib;
    struct FT_FaceRec_*    ft_face;
    int                    space_advance;
#endif

#ifdef NVG_FONT_STB_TRUETYPE
    stbtt_fontinfo stbtt;
#endif

#ifndef NVG_MAX_FONT_SLOTS
#define NVG_MAX_FONT_SLOTS 4
#endif // NVG_MAX_FONTS
    // NOTE: integer IDs for fonts are handed to the user that represent idx+1
    // This leaves 0 and <0 as invalid ids
    NVGfontSlot fonts[NVG_MAX_FONT_SLOTS];

    NVGatlas*     glyph_atlases;
    NVGatlasRect* rects;

    struct
    {
        int            idx;
        stbrp_context  ctx;
        stbrp_node*    nodes;
        unsigned char* img_data;
    } current_atlas;

    // Text pipeline
    sg_pipeline text_pip;
    sg_buffer   text_sbo;
    sg_view     text_sbv;
    sg_sampler  text_smp;

#ifndef NVG_MAX_GLYPHS
#define NVG_MAX_GLYPHS 1024
#endif
    size_t        text_buffer_len;
    text_buffer_t text_buffer[NVG_MAX_GLYPHS];

    struct
    {
        int drawCallCount;
        int fillTriCount;
        int strokeTriCount;
        int textTriCount;
        // Track how much data is uploaded to GPU
        size_t uploaded_bytes;
    } frame_stats;

    // SGNVGcontext....

    sg_shader          shader;
    SGNVGtexture*      textures;
    SGNVGvertUniforms  view;
    int                flags;
    int                ntextures;
    int                ctextures;
    sg_buffer          vertBuf;
    sg_buffer          indexBuf;
    SGNVGpipelineCache pipelineCache;

    // Image post processing FX (blur & bloom)
    sg_pipeline pip_texread;
    sg_pipeline pip_lightness_filter;
    sg_pipeline pip_downsample;
    sg_pipeline pip_upsample;
    sg_pipeline pip_upsample_mix;
    sg_pipeline pip_bloom;

    // Per frame buffers
    SGNVGattribute* verts;
    int             cverts;
    int             nverts;
    int             cverts_gpu;
    uint32_t*       indexes;
    int             cindexes;
    int             nindexes;
    int             cindexes_gpu;

    sg_sampler sampler_linear;
    sg_sampler sampler_nearest;

    // Feel free to allocate anything you want with this at any time in a frame after nvgBeginFrame() is called
    // Note all allocations are dropped when nvgBeginFrame() is called
    // It is also unadvised to release anything you allocate with this.
    // If these rules/guidelines are okay with you, go ahead
    LinkedArena* frame_arena;

    SGNVGcall*       current_call;     // linked list current position
    SGNVGcommandNVG* current_nvg_draw; // linked list current position
    SGNVGcommand*    current_command;  // linked list current position
    SGNVGcommand*    first_command;    // linked list start

    // state
    int            pipelineCacheIndex;
    sg_blend_state blend;

    int     dummyTex;
    sg_view dummyTexView;
} NVGcontext;

typedef struct NVGglyphPosition2
{
    NVGatlasRect rect;
    int          x, y;
} NVGglyphPosition2;

typedef struct NVGtextLayoutRow
{
    // Indexes into glyphs array in struct NVGtextLayout below
    short begin_idx, end_idx;
    short ymin, ymax;
    short xmin, xmax;
    int   cursor_y_px;
} NVGtextLayoutRow;

// Glyphs are shaped and aligned from left > right along the baseline of row one
// Alignment and translation on a screen should be applied at draw time
// This design is to help reduce the amount of work kbts has do to, and avoid doing multiple runs across the text
// Hopefully there is enough data here to make this possible.
// Handling multiple languages is an aftertought here and this design may prove to be bad.
typedef struct NVGtextLayout
{
    // WARNING: this values are scaled accorting to ctx->backingScaleFactor
    // You are free to use them, however you may need to remember to divide by backingScaleFactor to your work in your
    // own pixel space
    short ascender, descender;
    short line_height;
    short xmax; // The right edge of the longest (in pixels) row

    int total_height;
    int total_height_tight;

    int num_rows, cap_rows;
    int num_glyphs, cap_glyphs;
    int offset_rows;
    int offset_glyphs;
} NVGtextLayout;

static NVGtextLayoutRow* nvgLayoutGetRows(const NVGtextLayout* l)
{
    return (NVGtextLayoutRow*)((char*)l + l->offset_rows);
}
static void nvgLayoutSetRows(NVGtextLayout* l, NVGtextLayoutRow* r) { l->offset_rows = ((char*)r - (char*)l); }
static NVGglyphPosition2* nvgLayoutGetGlyphs(const NVGtextLayout* l)
{
    return (NVGglyphPosition2*)((char*)l + l->offset_glyphs);
}
static void nvgLayoutSetGlyphs(NVGtextLayout* l, NVGglyphPosition2* g) { l->offset_glyphs = ((char*)g - (char*)l); }

NVGcontext* nvgCreateContext(int flags);
void        nvgDestroyContext(NVGcontext* ctx);

// Debug function to dump cached path data.
void nvgDebugDumpPathCache(NVGcontext* ctx);

// Begin drawing a new frame
// Calls to nanovg drawing API should be wrapped in nvgBeginFrame() & nvgEndFrame()
// nvgBeginFrame() defines the size of the window to render to in relation currently
// set viewport (i.e. glViewport on GL backends). Device pixel ration allows to
// control the rendering on Hi-DPI devices.
// For example, GLFW returns two dimension for an opened window: window size and
// frame buffer size. In that case you would set windowWidth/Height to the window size
// backingScaleFactor to: frameBufferWidth / windowWidth.
void nvgBeginFrame(NVGcontext* ctx, int backingScaleFactor);

// Ends drawing flushing remaining render state.
void nvgEndFrame(NVGcontext* ctx);

//
// Composite operation
//
// The composite operations in NanoVG are modeled after HTML Canvas API, and
// the blend func is based on OpenGL (see corresponding manuals for more info).
// The colours in the blending state have premultiplied alpha.

// Sets the composite operation. The op parameter should be one of NVGcompositeOperation.
void nvgSetGlobalCompositeOperation(NVGcontext* ctx, int op);

// Sets the composite operation with custom pixel arithmetic. The parameters should be one of NVGblendFactor.
void nvgSetGlobalCompositeBlendFunc(NVGcontext* ctx, int sfactor, int dfactor);

// Sets the composite operation with custom pixel arithmetic for RGB and alpha components separately. The parameters
// should be one of NVGblendFactor.
void nvgSetGlobalCompositeBlendFuncSeparate(NVGcontext* ctx, int srcRGB, int dstRGB, int srcAlpha, int dstAlpha);

//
// Colour utils
//
// Colours in NanoVG are stored as unsigned ints in ABGR format.

// Returns a colour value from red, green, blue and alpha values.
NVGcolour nvgRGBA(unsigned char r, unsigned char g, unsigned char b, unsigned char a);

// Returns a colour value from red, green, blue and alpha values.
NVGcolour nvgRGBAf(float r, float g, float b, float a);

// Linearly interpolates from colour c0 to c1, and returns resulting colour value.
NVGcolour nvgLerpRGBA(NVGcolour c0, NVGcolour c1, float u);

// Returns colour value specified by hue, saturation and lightness.
// HSL values are all in range [0..1], alpha will be set to 255.
NVGcolour nvgHSL(float h, float s, float l);

// Returns colour value specified by hue, saturation and lightness and alpha.
// HSL values are all in range [0..1], alpha in range [0..255]
NVGcolour nvgHSLA(float h, float s, float l, unsigned char a);

// clang-format off
#define nvgHexColour(hex) (NVGcolour){( hex >> 24)         / 255.0f,\
                                      ((hex >> 16) & 0xff) / 255.0f,\
                                      ((hex >>  8) & 0xff) / 255.0f,\
                                      ( hex        & 0xff) / 255.0f}
// clang-format on

static uint32_t nvgCompressColour(NVGcolour col)
{
    uint32_t r, g, b, a;
    r  = col.r * 255;
    g  = col.g * 255;
    b  = col.b * 255;
    a  = col.a * 255;
    r &= 255;
    g &= 255;
    b &= 255;
    a &= 255;
    return (r << 24) | (g << 16) | (b << 8) | (a << 0);
}

//
// Transforms
//
// The paths, gradients, patterns and scissor region are transformed by an transformation
// matrix at the time when they are passed to the API.
// The current transformation matrix is a affine matrix:
//   [sx kx tx]
//   [ky sy ty]
//   [ 0  0  1]
// Where: sx,sy define scaling, kx,ky skewing, and tx,ty translation.
// The last row is assumed to be 0,0,1 and is not stored.
//
// Apart from nvgResetTransform(), each transformation function first creates
// specific transformation matrix and pre-multiplies the current transformation by it.
//
// Current coordinate system (transformation) can be saved and restored using nvgSave() and nvgRestore().

// Resets current transform to a identity matrix.
void nvgResetTransform(NVGcontext* ctx);

// Premultiplies current coordinate system by specified matrix.
// The parameters are interpreted as matrix as follows:
//   [a c e]
//   [b d f]
//   [0 0 1]
void nvgTransform(NVGcontext* ctx, float a, float b, float c, float d, float e, float f);

// Translates current coordinate system.
void nvgTranslate(NVGcontext* ctx, float x, float y);

// Rotates current coordinate system. Angle is specified in radians.
void nvgRotate(NVGcontext* ctx, float angle);

// Skews the current coordinate system along X axis. Angle is specified in radians.
void nvgSkewX(NVGcontext* ctx, float angle);

// Skews the current coordinate system along Y axis. Angle is specified in radians.
void nvgSkewY(NVGcontext* ctx, float angle);

// Scales the current coordinate system.
void nvgScale(NVGcontext* ctx, float x, float y);

// Stores the top part (a-f) of the current transformation matrix in to the specified buffer.
//   [a c e]
//   [b d f]
//   [0 0 1]
// There should be space for 6 floats in the return buffer for the values a-f.
void nvgCurrentTransform(NVGcontext* ctx, float* xform);

// The following functions can be used to make calculations on 2x3 transformation matrices.
// A 2x3 matrix is represented as float[6].

// Sets the transform to identity matrix.
void nvgTransformIdentity(float* dst);

// Sets the transform to translation matrix matrix.
void nvgTransformTranslate(float* dst, float tx, float ty);

// Sets the transform to scale matrix.
void nvgTransformScale(float* dst, float sx, float sy);

// Sets the transform to rotate matrix. Angle is specified in radians.
void nvgTransformRotate(float* dst, float a);

// Sets the transform to skew-x matrix. Angle is specified in radians.
void nvgTransformSkewX(float* dst, float a);

// Sets the transform to skew-y matrix. Angle is specified in radians.
void nvgTransformSkewY(float* dst, float a);

// Sets the transform to the result of multiplication of two transforms, of A = A*B.
void nvgTransformMultiply(float* dst, const float* src);

// Sets the transform to the result of multiplication of two transforms, of A = B*A.
void nvgTransformPremultiply(float* dst, const float* src);

// Sets the destination to inverse of specified transform.
// Returns 1 if the inverse could be calculated, else 0.
int nvgTransformInverse(float* dst, const float* src);

// Transform a point by given transform.
void nvgTransformPoint(float* dstx, float* dsty, const float* xform, float srcx, float srcy);

// Converts degrees to radians and vice versa.
float nvgDegToRad(float deg);
float nvgRadToDeg(float rad);

//
// Render styles
//
// Fill and stroke render style can be either a solid colour or a paint which is a gradient or a pattern.
// Solid colour is simply defined as a colour value, different kinds of paints can be created
// using nvgLinearGradient(), nvgBoxGradient(), nvgRadialGradient() and nvgImagePattern().
//

// Sets whether to draw antialias for nvgStroke() and nvgFill(). It's enabled by default.
static inline void nvgSetShapeAntiAlias(NVGcontext* ctx, int enabled) { ctx->state.shapeAntiAlias = enabled; }

// Sets current paint style to a solid colour.
void nvgSetColour(NVGcontext* ctx, NVGcolour colour);

// Sets current paint style to a gradient or pattern.
static inline void nvgSetPaint(NVGcontext* ctx, NVGpaint paint)
{
    ctx->state.paint = paint;
    nvgTransformMultiply(ctx->state.paint.xform, ctx->state.xform);
}

// Sets the miter limit of the stroke style.
// Miter limit controls when a sharp corner is beveled.
static inline void nvgSetMiterLimit(NVGcontext* ctx, float limit) { ctx->state.miterLimit = limit; }

// Sets how the end of the line (cap) is drawn,
// Can be one of: NVG_BUTT (default), NVG_ROUND, NVG_SQUARE.
static inline void nvgSetLineCap(NVGcontext* ctx, int cap) { ctx->state.lineCap = cap; }

// Sets how sharp path corners are drawn.
// Can be one of NVG_MITER (default), NVG_ROUND, NVG_BEVEL.
static inline void nvgSetLineJoin(NVGcontext* ctx, int join) { ctx->state.lineJoin = join; }

//
// Images
//
// NanoVG allows you to load jpg, png, psd, tga, pic and gif files to be used for rendering.
// In addition you can upload your own image. The image loading is provided by stb_image.
// The parameter imageFlags is combination of flags defined in NVGimageFlags.

// Creates image by loading it from the disk from specified file name.
// Returns handle to the image.
int nvgCreateImage(NVGcontext* ctx, const char* filename, int imageFlags);

// Creates image by loading it from the specified chunk of memory.
// Returns handle to the image.
int nvgCreateImageMem(NVGcontext* ctx, int imageFlags, unsigned char* data, int ndata);

// Creates image from specified image data.
// Returns handle to the image.
int nvgCreateImageRGBA(NVGcontext* ctx, int w, int h, int imageFlags, const unsigned char* data);

// Updates image data specified by image handle.
// void nvgUpdateImage(NVGcontext* ctx, int image, const unsigned char* data);

// Gets the dimensions of a created image. Returns true on success
// bool nvgGetImageSize(NVGcontext* ctx, int image, int* w, int* h);

// Deletes created image.
void nvgDeleteImage(NVGcontext* ctx, int image);

// Returns handle to the image.
int nvgCreateTexture(NVGcontext* ctx, enum NVGtexture type, int w, int h, int imageFlags, const unsigned char* data);

// int nvgUpdateTexture(NVGcontext* ctx, int image, int x, int y, int w, int h, const unsigned char* data);

//
// Paints
//
// NanoVG supports four types of paints: linear gradient, box gradient, radial gradient and image pattern.
// These can be used as paints for strokes and fills.

// Creates and returns a linear gradient. Parameters (sx,sy)-(ex,ey) specify the start and end coordinates
// of the linear gradient, icol specifies the start colour and ocol the end colour.
// The gradient is transformed by the current transform when it is passed to nvgSetPaint().
NVGpaint nvgLinearGradient(NVGcontext* ctx, float sx, float sy, float ex, float ey, NVGcolour icol, NVGcolour ocol);

// Creates and returns a box gradient. Box gradient is a feathered rounded rectangle, it is useful for rendering
// drop shadows or highlights for boxes. Parameters (x,y) define the top-left corner of the rectangle,
// (w,h) define the size of the rectangle, r defines the corner radius, and f feather. Feather defines how blurry
// the border of the rectangle is. Parameter icol specifies the inner colour and ocol the outer colour of the gradient.
// The gradient is transformed by the current transform when it is passed to nvgSetPaint().
NVGpaint
nvgBoxGradient(NVGcontext* ctx, float x, float y, float w, float h, float r, float f, NVGcolour icol, NVGcolour ocol);

// Creates and returns a radial gradient. Parameters (cx,cy) specify the centre, inr and outr specify
// the inner and outer radius of the gradient, icol specifies the start colour and ocol the end colour.
// The gradient is transformed by the current transform when it is passed to nvgSetPaint().
NVGpaint nvgRadialGradient(NVGcontext* ctx, float cx, float cy, float inr, float outr, NVGcolour icol, NVGcolour ocol);

// Creates and returns an image pattern. The gradient is transformed by the current transform when it is passed to
// nvgSetPaint().
NVGpaint nvgImagePattern(
    NVGcontext* ctx,
    float       x,
    float       y,
    float       w,
    float       h,
    float       angle, // angle rotation around the top-left corner
    sg_view     texview,
    float       alpha,
    sg_sampler  smp);

//
// Scissoring
//
// Scissoring allows you to clip the rendering into a rectangle. This is useful for various
// user interface cases like rendering a text edit or a timeline.

// Sets the current scissor rectangle.
// The scissor rectangle is transformed by the current transform.
void nvgSetScissor(NVGcontext* ctx, float x, float y, float w, float h);

// Intersects current scissor rectangle with the specified rectangle.
// The scissor rectangle is transformed by the current transform.
// Note: in case the rotation of previous scissor rect differs from
// the current one, the intersection will be done between the specified
// rectangle and the previous scissor rectangle transformed in the current
// transform space. The resulting shape is always rectangle.
void nvgIntersectScissor(NVGcontext* ctx, float x, float y, float w, float h);

// Reset and disables scissoring.
void nvgResetScissor(NVGcontext* ctx);

//
// Paths
//
// Drawing a new shape starts with nvgBeginPath(), it clears all the currently defined paths.
// Then you define one or more paths and sub-paths which describe the shape. The are functions
// to draw common shapes like rectangles and circles, and lower level step-by-step functions,
// which allow to define a path curve by curve.
//
// NanoVG uses even-odd fill rule to draw the shapes. Solid shapes should have counter clockwise
// winding and holes should have counter clockwise order. To specify winding of a path you can
// call nvgSetPathWinding(). This is useful especially for the common shapes, which are drawn CCW.
//
// Finally you can fill the path using current fill style by calling nvgFill(), and stroke it
// with current stroke style by calling nvgStroke().
//
// The curve segments and sub-paths are transformed by the current transform.

// Clears the current path and sub-paths.
void nvgBeginPath(NVGcontext* ctx);

void nvg__appendCommands(NVGcontext* ctx, float* vals, int nvals);

// Starts new sub-path with specified point as first point.
static void nvgMoveTo(NVGcontext* ctx, float x, float y)
{
    float vals[] = {NVG_MOVETO, x, y};
    nvg__appendCommands(ctx, vals, NVG_ARRLEN(vals));
}

// Adds line segment from the last point in the path to the specified point.
static void nvgLineTo(NVGcontext* ctx, float x, float y)
{
    float vals[] = {NVG_LINETO, x, y};
    nvg__appendCommands(ctx, vals, NVG_ARRLEN(vals));
}

// Adds cubic bezier segment from last point in the path via two control points to the specified point.
static void nvgBezierTo(NVGcontext* ctx, float c1x, float c1y, float c2x, float c2y, float x, float y)
{
    float vals[] = {NVG_BEZIERTO, c1x, c1y, c2x, c2y, x, y};
    nvg__appendCommands(ctx, vals, NVG_ARRLEN(vals));
}

// Adds quadratic bezier segment from last point in the path via a control point to the specified point.
void nvgQuadTo(NVGcontext* ctx, float cx, float cy, float x, float y);

// Adds an arc segment at the corner defined by the last path point, and two specified points.
void nvgArcTo(NVGcontext* ctx, float x1, float y1, float x2, float y2, float radius);

// Closes current sub-path with a line segment.
void nvgClosePath(NVGcontext* ctx);

// Sets the current sub-path winding, see NVGwinding and NVGsolidity.
void nvgSetPathWinding(NVGcontext* ctx, int dir);

// Creates new circle arc shaped sub-path. The arc centre is at cx,cy, the arc radius is r,
// and the arc is drawn from angle a0 to a1, and swept in direction dir (NVG_CCW, or NVG_CW).
// Angles are specified in radians.
void nvgArc(NVGcontext* ctx, float cx, float cy, float r, float a0, float a1, int dir);

// Creates new rectangle shaped sub-path.
void nvgRect(NVGcontext* ctx, float x, float y, float w, float h);
void nvgRect2(NVGcontext* ctx, float left, float top, float right, float bottom);

// Creates new rounded rectangle shaped sub-path.
void nvgRoundedRect(NVGcontext* ctx, float x, float y, float w, float h, float r);
void nvgRoundedRect2(NVGcontext* ctx, float x, float y, float r, float b, float rad);

// Creates new rounded rectangle shaped sub-path with varying radii for each corner.
void nvgRoundedRectVarying(
    NVGcontext* ctx,
    float       l,
    float       t,
    float       w,
    float       h,
    float       radTopLeft,
    float       radTopRight,
    float       radBottomRight,
    float       radBottomLeft);
void nvgRoundedRectVarying2(
    NVGcontext* ctx,
    float       l,
    float       t,
    float       r,
    float       b,
    float       radTopLeft,
    float       radTopRight,
    float       radBottomRight,
    float       radBottomLeft);

// Creates new ellipse shaped sub-path.
void nvgEllipse(NVGcontext* ctx, float cx, float cy, float rx, float ry);

// Creates new circle shaped sub-path.
void nvgCircle(NVGcontext* ctx, float cx, float cy, float r);

// Fills the current path with current fill style.
void nvgFill(NVGcontext* ctx);

// Fills the current path with current stroke style.
void nvgStroke(NVGcontext* ctx, float stroke_width);

//
// Text
//
// NanoVG allows you to load .ttf files and use the font to render text.
//
// The appearance of the text can be defined by setting the current text style
// and by specifying the fill colour. Common text and font settings such as
// font size, letter spacing and text align are supported. Font blur allows you
// to create simple text effects such as drop shadows.
//
// At render time the font face can be set based on the font handles or name.
//
// Font measure functions return values in local space, the calculations are
// carried in the same resolution as the final rendering. This is done because
// the text glyph positions are snapped to the nearest pixels sharp rendering.
//
// The local space means that values are not rotated or scale as per the current
// transformation. For example if you set font size to 12, which would mean that
// line height is 16, then regardless of the current scaling and rotation, the
// returned line height is always 16. Some measures may vary because of the scaling
// since aforementioned pixel snapping.
//
// While this may sound a little odd, the setup allows you to always render the
// same way regardless of scaling. I.e. following works regardless of scaling:
//
//		const char* txt = "Text me up.";
//		nvgTextBounds(vg, x,y, txt, NULL, bounds);
//		nvgBeginPath(vg);
//		nvgRect(vg, bounds[0],bounds[1], bounds[2]-bounds[0], bounds[3]-bounds[1]);
//		nvgFill(vg);
//
// Note: currently only solid colour fill is supported for text.

// TODO: load font

// Sets the font size of current text style.
static void nvgSetFontSize(NVGcontext* ctx, float size) { ctx->state.fontSize = size; }

// Sets the proportional line height of current text style. The line height is specified as multiple of font size.
static void nvgSetTextLineHeight(NVGcontext* ctx, float lineHeight) { ctx->state.lineHeight = lineHeight; }

// Sets the text align of current text style, see NVGalign for options.
static void nvgSetTextAlign(NVGcontext* ctx, int align) { ctx->state.textAlign = align; }

// Sets the font face based on specified id of current text style.
void nvgSetFontFaceById(NVGcontext* ctx, int font_id);
// static void nvgSetFontFaceById(NVGcontext* ctx, int font_id) { ctx->state.fontId = font_id;}

// Creates font by loading it from the disk from specified file name.
// Returns handle to the font.
// Note: the library takes ownership of the data
int nvgCreateFont(NVGcontext* ctx, const char* filename);
int nvgCreateFontAtIndex(NVGcontext* ctx, const char* filename, const int fontIndex);

// Creates font by loading it from the specified memory chunk.
// Returns handle to the font.
// Note: you are responsible for persisting the data for the duration of use in this program
int nvgCreateFontMem(NVGcontext* ctx, unsigned char* data, int ndata);
int nvgCreateFontMemAtIndex(NVGcontext* ctx, unsigned char* data, int ndata, const int fontIndex);

// TODO: remove these features
// Finds a loaded font of specified name, and returns handle to it, or -1 if the font is not found.
// int nvgFindFont(NVGcontext* ctx, const char* name);

// Adds a fallback font by handle.
// int nvgAddFallbackFontId(NVGcontext* ctx, int baseFont, int fallbackFont);

// Adds a fallback font by name.
// int nvgAddFallbackFont(NVGcontext* ctx, const char* baseFont, const char* fallbackFont);

// Resets fallback fonts by handle.
// void nvgResetFallbackFontsId(NVGcontext* ctx, int baseFont);

// Resets fallback fonts by name.
// void nvgResetFallbackFonts(NVGcontext* ctx, const char* baseFont);

// Sets the blur of current text style.
// static void nvgSetFontBlur(NVGcontext* ctx, float blur) { ctx->state.fontBlur = blur; }

// Sets the letter spacing of current text style.
// static void nvgSetLetterSpacing(NVGcontext* ctx, float spacing) { ctx->state.letterSpacing = spacing;}

// Sets the font face based on specified name of current text style.
// void nvgSetFontFaceByName(NVGcontext* ctx, const char* font);

// Draws text string at specified location. If end is specified only the sub-string up to the end is drawn.
void nvgText(NVGcontext* ctx, float x, float y, const char* string, const char* end);

// Draws multi-line text string at specified location wrapped at the specified width. If end is specified only the
// sub-string up to the end is drawn. White space is stripped at the beginning of the rows, the text is split at word
// boundaries or when new-line characters are encountered. Words longer than the max width are slit at nearest character
// (i.e. no hyphenation).
void nvgTextBox(NVGcontext* ctx, float x, float y, float breakRowWidth, const char* string, const char* end);

// Measures the specified text string. Parameter bounds should be a pointer to float[4],
// if the bounding box of the text should be returned. The bounds value are [xmin,ymin, xmax,ymax]
// Returns the horizontal advance of the measured text (i.e. where the next character should drawn).
// Measured values are returned in local coordinate space.
// void nvgTextBounds(NVGcontext* ctx, float x, float y, const char* string, const char* end, float* bounds);

// Measures the specified multi-text string. Parameter bounds should be a pointer to float[4],
// if the bounding box of the text should be returned. The bounds value are [xmin,ymin, xmax,ymax]
// Measured values are returned in local coordinate space.
void nvgTextBoxBounds(
    NVGcontext* ctx,
    float       x,
    float       y,
    float       breakRowWidth,
    const char* string,
    const char* end,
    float*      bounds);

// Calculates the glyph x positions of the specified text. If end is specified only the sub-string will be used.
// Measured values are returned in local coordinate space.
// int nvgTextGlyphPositions(
//     NVGcontext*       ctx,
//     float             x,
//     float             y,
//     const char*       string,
//     const char*       end,
//     NVGglyphPosition* positions,
//     int               maxPositions);

// Returns the vertical metrics based on the current text style.
// Measured values are returned in local coordinate space.
// NVGtextMetrics nvgTextMetrics(NVGcontext* ctx);

// Breaks the specified text into lines. If end is specified only the sub-string will be used.
// White space is stripped at the beginning of the rows, the text is split at word boundaries or when new-line
// characters are encountered. Words longer than the max width are slit at nearest character (i.e. no hyphenation).
// int nvgTextBreakLines(
//     NVGcontext* ctx,
//     const char* string,
//     const char* end,
//     float       breakRowWidth,
//     NVGtextRow* rows,
//     int         maxRows);

// High quality layout using kb_text_shaping. This is slow so you should probably cache the results
const NVGtextLayout*
nvgMakeLayout(NVGcontext* ctx, const char* text_start, const char* text_end, float font_size, float breakRowWidth);
// Lower quality, but still looks fairly good.
// Suitable for realtime text layout, like short updating labels (eg. -3.14 dB)
const NVGtextLayout*
nvgMakeLayoutFast(NVGcontext* ctx, const char* start, const char* end, float font_size, float breakRowWidth);

void        nvgDrawLayout(NVGcontext* ctx, const NVGtextLayout* layout, int x, int y);
static void nvgReleaseLayout(NVGcontext* ctx, const NVGtextLayout* layout) { linked_arena_release(ctx->arena, layout); }

sg_image sg_make_image_with_mipmaps(const sg_image_desc* desc_);

int snvgCreateImageFromHandleSokol(NVGcontext* ctx, sg_image imageSokol, enum NVGtexture type, int w, int h, int flags);

SGNVGframebuffer snvgCreateFramebuffer(NVGcontext* ctx, int width, int height);
void             snvgDestroyFramebuffer(NVGcontext* ctx, SGNVGframebuffer* renderTarget);

// If you pass a blur radius less than a power of two, it will be rounded up to a power of two
SGNVGimageFX* snvgCreateImageFX(NVGcontext* ctx, int width, int height, int max_blur_radius);
void          snvgDestroyImageFX(NVGcontext* ctx, SGNVGimageFX* fx);

void snvg_command_begin_pass(
    NVGcontext* ctx,
    const sg_pass*,
    unsigned    x,
    unsigned    y,
    unsigned    width,
    unsigned    height,
    const char* label);
void snvg_command_end_pass(NVGcontext* ctx, const char* label);
void snvg_command_draw_nvg(NVGcontext* ctx, const char* label);
// 'radius_px' can be animated each frame. For best performance, finish your animations with radius at a power of 2,
// and a minimum of 8px
void snvg_command_fx(
    NVGcontext*       ctx,
    bool              apply_lightness_filter,
    bool              apply_bloom,
    float             lightness_threshold,
    float             radius_px,
    float             bloom_amount,
    SGNVGframebuffer* src,
    SGNVGimageFX*     fx,
    const char*       label);
void snvg_command_custom(NVGcontext* ctx, void* uptr, SGNVGcustomFunc func, const char* label);

typedef struct SNVGcallState
{
    SGNVGcall* start;
    SGNVGcall* end;
    int        count;
} SNVGcallState;

static SNVGcallState snvg_calls_pop(NVGcontext* ctx)
{
    SNVGcallState calls;
    calls.start                      = ctx->current_nvg_draw->calls;
    calls.end                        = ctx->current_call;
    calls.count                      = ctx->current_nvg_draw->num_calls;
    ctx->current_nvg_draw->calls     = NULL;
    ctx->current_call                = NULL;
    ctx->current_nvg_draw->num_calls = 0;
    return calls;
}
static void snvg_calls_set(NVGcontext* ctx, const SNVGcallState* calls)
{
    ctx->current_nvg_draw->calls     = calls->start;
    ctx->current_nvg_draw->num_calls = calls->count;
    ctx->current_call                = calls->end;
}
static void snvg_calls_join(NVGcontext* ctx, const SNVGcallState* calls)
{
    if (ctx->current_call)
        ctx->current_call->next = calls->start;
    ctx->current_call                 = calls->end;
    ctx->current_nvg_draw->num_calls += calls->count;
}

#ifdef _MSC_VER
#pragma warning(pop)
#endif

#define NVG_NOTUSED(v)                                                                                                 \
    for (;;)                                                                                                           \
    {                                                                                                                  \
        (void)(1 ? (void)0 : ((void)(v)));                                                                             \
        break;                                                                                                         \
    }

#ifdef __cplusplus
}
#endif

#endif // NANOVG_H