/*
MIT No Attribution

Copyright 2025 Tré Dudman

Permission is hereby granted, free of charge, to any person obtaining a copy of this
software and associated documentation files (the "Software"), to deal in the Software
without restriction, including without limitation the rights to use, copy, modify,
merge, publish, distribute, sublicense, and/or sell copies of the Software, and to
permit persons to whom the Software is furnished to do so.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED,
INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A
PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT
HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION
OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE
SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
*/

// Dual filter blur. Marius Bjorge - Bandwith-Efficient Rendering, Siggraph 2015
// https://community.arm.com/cfs-file/__key/communityserver-blogs-components-weblogfiles/00-00-00-20-66/siggraph2015_2D00_mmg_2D00_marius_2D00_notes.pdf

@vs fullscreen_triangle_vs
const vec2 positions[3] = { vec2(-1, -1), vec2(3, -1), vec2(-1, 3), };

out vec2 uv;

void main() {
    vec2 pos = positions[gl_VertexIndex];
    gl_Position = vec4(pos, 0, 1);
    uv = (pos * vec2(1, -1) + 1) * 0.5;
}
@end

@fs texread_fs
layout(binding=0)uniform texture2D tex;
layout(binding=0)uniform sampler smp;

in vec2 uv;
out vec4 frag_color;

void main() {
    frag_color = texture(sampler2D(tex, smp), uv);
}
@end

@fs lightfilter_fs
layout(binding=0)uniform texture2D tex;
layout(binding=0)uniform sampler smp;
layout(binding=0) uniform fs_lightfilter {
    float u_threshold;
};

in vec2 uv;
out vec4 frag_color;

// https://en.wikipedia.org/wiki/Relative_luminance
float get_luminance(vec3 col)
{
    return 0.2126 * col.r + 0.7152 * col.g + 0.0722 * col.b;
}

void main() {
    vec4 col = texture(sampler2D(tex, smp), uv);
    float l = get_luminance(col.rgb);
    if (l < u_threshold)
    {
        col.rgb = vec3(0);
    }
    frag_color = col;
}
@end

@fs kawase_blur_fs
in vec2 uv;
out vec4 frag_colour;
layout(binding=0) uniform texture2D tex;
layout(binding=0) uniform sampler smp;
layout(binding=0) uniform fs_kawase_blur {
    vec2 u_offset;
};

void main() {
    frag_colour = texture(sampler2D(tex, smp), vec2(uv.x + u_offset.x, uv.y + u_offset.y));
    frag_colour += texture(sampler2D(tex, smp), vec2(uv.x - u_offset.x, uv.y + u_offset.y));
    frag_colour += texture(sampler2D(tex, smp), vec2(uv.x + u_offset.x, uv.y - u_offset.y));
    frag_colour += texture(sampler2D(tex, smp), vec2(uv.x - u_offset.x, uv.y - u_offset.y));
    frag_colour /= vec4(4);
}
@end

@fs downsample_fs
in vec2 uv;
out vec4 frag_colour;
layout(binding=0) uniform texture2D tex;
layout(binding=0) uniform sampler smp;
layout(binding=0) uniform fs_downsample {
    vec2 u_offset;
};

void main() {
    // Downsample
    vec4 sum = texture(sampler2D(tex, smp), uv) * 4.0;
    sum += texture(sampler2D(tex, smp), uv - u_offset.xy);
    sum += texture(sampler2D(tex, smp), uv + u_offset.xy);
    sum += texture(sampler2D(tex, smp), uv + vec2(u_offset.x, -u_offset.y));
    sum += texture(sampler2D(tex, smp), uv - vec2(u_offset.x, -u_offset.y));
    sum /= 8.0;
    frag_colour = sum;
}
@end

@fs upsample_fs
in vec2 uv;
out vec4 frag_colour;
layout(binding=0) uniform texture2D tex;
layout(binding=0) uniform sampler smp;
layout(binding=0) uniform fs_upsample {
    vec2 u_offset;
};

void main() {
    // Upsample
    vec4 sum = texture(sampler2D(tex, smp), uv + vec2(-u_offset.x * 2.0, 0.0));
    sum += texture(sampler2D(tex, smp), uv + vec2(-u_offset.x, u_offset.y)) * 2.0;
    sum += texture(sampler2D(tex, smp), uv + vec2(0.0, u_offset.y * 2.0));
    sum += texture(sampler2D(tex, smp), uv + vec2(u_offset.x, u_offset.y)) * 2.0;
    sum += texture(sampler2D(tex, smp), uv + vec2(u_offset.x * 2.0, 0.0));
    sum += texture(sampler2D(tex, smp), uv + vec2(u_offset.x, -u_offset.y)) * 2.0;
    sum += texture(sampler2D(tex, smp), uv + vec2(0.0, -u_offset.y * 2.0));
    sum += texture(sampler2D(tex, smp), uv + vec2(-u_offset.x, -u_offset.y)) * 2.0;
    sum /= 12.0;

    frag_colour = sum;
}
@end

@fs upsample_mix_fs
in vec2 uv;
out vec4 frag_colour;
layout(binding=0) uniform texture2D tex_wet;
layout(binding=0) uniform sampler smp_wet;
layout(binding=1) uniform texture2D tex_dry;
layout(binding=1) uniform sampler smp_dry;
layout(binding=0) uniform fs_upsample_mix {
    vec2 u_offset;
    float u_amount;
};

void main() {
    vec4 dry = texture(sampler2D(tex_dry, smp_dry), uv);

    // Upsample
    vec4 wet = texture(sampler2D(tex_wet, smp_wet), uv + vec2(-u_offset.x * 2.0, 0.0));
    wet += texture(sampler2D(tex_wet, smp_wet), uv + vec2(-u_offset.x, u_offset.y)) * 2.0;
    wet += texture(sampler2D(tex_wet, smp_wet), uv + vec2(0.0, u_offset.y * 2.0));
    wet += texture(sampler2D(tex_wet, smp_wet), uv + vec2(u_offset.x, u_offset.y)) * 2.0;
    wet += texture(sampler2D(tex_wet, smp_wet), uv + vec2(u_offset.x * 2.0, 0.0));
    wet += texture(sampler2D(tex_wet, smp_wet), uv + vec2(u_offset.x, -u_offset.y)) * 2.0;
    wet += texture(sampler2D(tex_wet, smp_wet), uv + vec2(0.0, -u_offset.y * 2.0));
    wet += texture(sampler2D(tex_wet, smp_wet), uv + vec2(-u_offset.x, -u_offset.y)) * 2.0;
    wet /= 12.0;

    // Blend
    frag_colour = mix(dry, wet, u_amount);}
@end

@fs bloom_fs
in vec2 uv;
out vec4 frag_colour;
layout(binding=0) uniform texture2D tex_wet;
layout(binding=1) uniform texture2D tex_dry;
layout(binding=0) uniform sampler smp;
layout(binding=0) uniform fs_bloom {
    float u_amount;
};

void main() {
    vec4 wet = texture(sampler2D(tex_wet, smp), uv);
    vec4 dry = texture(sampler2D(tex_dry, smp), uv);

    frag_colour = dry + wet * u_amount;
}
@end

@program lightfilter fullscreen_triangle_vs lightfilter_fs
@program texread fullscreen_triangle_vs texread_fs
@program kawase_blur fullscreen_triangle_vs kawase_blur_fs
@program downsample fullscreen_triangle_vs downsample_fs
@program upsample fullscreen_triangle_vs upsample_fs
@program upsample_mix fullscreen_triangle_vs upsample_mix_fs
@program bloom fullscreen_triangle_vs bloom_fs