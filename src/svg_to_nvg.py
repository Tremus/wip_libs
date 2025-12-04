"""
Quick and dirty SVG to NanoVG code generator

Prints to console a C like function for drawing SVG paths

Usage: python {path/to/this_script.py} {path/to/file.svg}

Dependencies: svgpathtools
"""

import sys, math
from pathlib import Path
from svgpathtools import svg2paths, CubicBezier, QuadraticBezier, Arc, Line


def create_function(filename: str):
    paths, attributes, svg_attributes  = svg2paths(filename, return_svg_attributes=True)

    if len(paths) != len(attributes):
        print("ERROR: len(paths) != len(attributes)")
        return

    p = Path(filename)
    name = p.parts[-1].replace('.', '_').replace(' ', '')

    viewBox = svg_attributes.get('viewBox')
    if viewBox:
        print("// viewBox = %s" % viewBox)
    print(
        'void draw_%s(NVGcontext* nvg, const float scale, float x, float y) {' % name)
    print('// clang-format off')
    for path_idx in range(len(paths)):
        path = paths[path_idx]
        att = attributes[path_idx]
        stroke_colour = att.get('stroke')
        stroke_width = att.get('stroke-width', 1)
        fill_colour = att.get('fill')
        is_closed = path.start == path.end

        print('nvgBeginPath(nvg);')
        pen = None # pen uninitialised
        for seg in path:
            if seg.start != pen:
                pen = seg.start
                print('nvgMoveTo(nvg, x + scale * %ff, y + scale * %ff);' % (pen.real, pen.imag))

            if isinstance(seg, CubicBezier):
                a2 = 'x + scale * %ff' % seg.control1.real
                a3 = 'y + scale * %ff' % seg.control1.imag
                a4 = 'x + scale * %ff' % seg.control2.real
                a5 = 'y + scale * %ff' % seg.control2.imag
                a6 = 'x + scale * %ff' % seg.end.real
                a7 = 'y + scale * %ff' % seg.end.imag
                print('nvgBezierTo(nvg, %s, %s, %s, %s, %s, %s);' %
                      (a2, a3, a4, a5, a6, a7))
            elif isinstance(seg, QuadraticBezier):
                a2 = 'x + scale * %ff' % seg.control.real
                a3 = 'y + scale * %ff' % seg.control.imag
                a4 = 'x + scale * %ff' % seg.end.real
                a5 = 'y + scale * %ff' % seg.end.imag
                print('nvgQuadTo(nvg, %s, %s, %s, %s);' % (a2, a3, a4, a5))
            elif isinstance(seg, Arc): # TODO: fix me
                radius = seg.radius.real
                angle = math.radians(seg.theta) - math.pi * 0.5
                cx = seg.center.real
                cy = seg.center.imag
                x1 = cx + radius * math.cos(angle)
                y1 = cy + radius * math.sin(angle)

                a2 = 'x + scale * %ff' % x1
                a3 = 'y + scale * %ff' % y1
                a4 = 'x + scale * %ff' % seg.end.real
                a5 = 'y + scale * %ff' % seg.end.imag
                
                print('nvgArcTo(nvg, %s, %s, %s, %s, %s);' % (a2, a3, a4, a5, radius))
            elif isinstance(seg, Line):
                a2 = 'x + scale * %ff' % seg.end.real
                a3 = 'y + scale * %ff' % seg.end.imag
                print('nvgLineTo(nvg, %s, %s);' % (a2, a3))
            # Update pen
            pen = seg.end

        if is_closed:
            print('nvgClosePath(nvg);')
        if fill_colour:
            print('nvgSetColour(nvg, %s);' % fill_colour)
            print('nvgFill(nvg);')
        if stroke_colour:
            print('nvgSetColour(nvg, %s);' % stroke_colour)
            print('nvgStroke(nvg, {});'.format(stroke_width))
    print('// clang-format on')
    print('}')


if __name__ == '__main__':
    create_function(sys.argv[1])
    sys.stdout.flush()
    exit(0)
