"""Exercise real LunaSVG bounds/pixels and costume origins (ASan/UBSan).

Requires a native LunaSVG build, default build-xbox/scratch2-svg-native;
override SE_LUNASVG_NATIVE_BUILD when using another native build directory.
"""
import os
from pathlib import Path
import subprocess
import tempfile

root = Path(__file__).resolve().parents[1]
native = Path(os.environ.get("SE_LUNASVG_NATIVE_BUILD", root / "build-xbox/scratch2-svg-native"))
headers = list((root / "build-xbox/.cache/catalog").glob("src_*/src/include/lunasvg.h"))
ryu_sources = list((root / "build-xbox/.cache/catalog").glob("src_*/src/ryu/d2s.c"))
if len(headers) != 1 or len(ryu_sources) != 1 or not (native / "liblunasvg.a").is_file():
    raise SystemExit("Build native LunaSVG and set SE_LUNASVG_NATIVE_BUILD first.")
with tempfile.TemporaryDirectory(prefix="scratch2-bounds-") as temporary:
    work = Path(temporary)
    source = work / "bounds.cpp"
    source.write_text(r'''
#include "scratch2_svg_bounds.hpp"
#include "image_geometry.hpp"
#include <cassert>
#include <cmath>
#include <iostream>
#include <string>

void near(double a, double b) { if(std::abs(a-b)>=.001) std::cerr<<"actual "<<a<<" expected "<<b<<"\n"; assert(std::abs(a-b)<.001); }
auto load(const std::string &body) {
    auto doc=lunasvg::Document::loadFromData("<svg xmlns='http://www.w3.org/2000/svg' width='10' height='10' viewBox='0 0 10 10'>"+body+"</svg>");
    assert(doc); return doc;
}
void rgba(lunasvg::Bitmap &b,int x,int y,int r,int g,int blue) {
    assert(x>=0 && y>=0 && x<b.width() && y<b.height());
    auto p=b.data()+b.stride()*y+x*4;
    assert(p[0]==r && p[1]==g && p[2]==blue && p[3]==255);
}
int main() {
    // A whole drawing outside an exported viewport must survive rasterization.
    auto d=load("<rect x='100' y='150' width='20' height='10' fill='red'/>");
    auto offset=Scratch2Svg::normalizeMeasurements(*d);
    near(offset.first,100); near(offset.second,150); near(d->width(),20); near(d->height(),10);
    auto b=d->renderToBitmap(); b.convertToRGBA(); rgba(b,10,5,255,0,0);
    double cx=110,cy=155,oldx=0,oldy=0;
    ImageGeometry::updateRotationCenter(cx,oldx,offset.first,10);
    ImageGeometry::updateRotationCenter(cy,oldy,offset.second,5);
    near(cx,10);near(cy,5);
    // Cache hits and clones retain the same image-to-world placement.
    for(int i=0;i<100;i++) ImageGeometry::updateRotationCenter(cx,oldx,100,10);
    near(cx,10); double cloneCenter=cx,cloneOffset=oldx;
    ImageGeometry::updateRotationCenter(cloneCenter,cloneOffset,100,10);near(cloneCenter,10);
    // A missing-image fallback and subsequent retry must not accumulate crops.
    ImageGeometry::updateRotationCenter(cx,oldx,0,10);near(cx,110);
    ImageGeometry::updateRotationCenter(cx,oldx,100,10);near(cx,10);
    double missing=-6767.6767,missingOffset=0;
    ImageGeometry::updateRotationCenter(missing,missingOffset,100,10);near(missing,10);
    ImageGeometry::updateRotationCenter(missing,missingOffset,100,10);near(missing,10);

    d=load("<rect x='-30' y='-20' width='20' height='10'/><rect x='30' y='40' width='10' height='10' fill='red'/>");
    offset=Scratch2Svg::normalizeMeasurements(*d);
    near(offset.first,-30);near(offset.second,-20);near(d->width(),70);near(d->height(),70);
    b=d->renderToBitmap();b.convertToRGBA();rgba(b,2,2,0,0,0);rgba(b,65,65,255,0,0);
    d=load("<rect x='0' y='0' width='10' height='10' fill='none' stroke='black' stroke-width='4'/>");
    offset=Scratch2Svg::normalizeMeasurements(*d);
    // LunaSVG conservatively expands sharp joins by the miter limit. Test
    // visible stroke retention, rather than assuming an exact padding box.
    assert(offset.first<=-2 && offset.second<=-2);
    assert(offset.first+d->width()>=12 && offset.second+d->height()>=12);
    b=d->renderToBitmap();b.convertToRGBA();
    rgba(b,int(5-offset.first),int(-1-offset.second),0,0,0);
    rgba(b,int(5-offset.first),int(11-offset.second),0,0,0);
    d=load("<g transform='translate(50 10) rotate(90)'><rect width='20' height='10'/></g>");
    offset=Scratch2Svg::normalizeMeasurements(*d);
    near(offset.first,40);near(offset.second,10);near(d->width(),10);near(d->height(),20);
    // Stable dimensions/offsets after normalization, and harmless empty SVGs.
    auto again=Scratch2Svg::normalizeMeasurements(*d);
    near(again.first,offset.first);near(again.second,offset.second);
    d=load("");offset=Scratch2Svg::normalizeMeasurements(*d);
    near(offset.first,0);near(offset.second,0);near(d->width(),10);near(d->height(),10);
    std::cout<<"SVG bounds, pixels, transforms and cached/clone/retry origins passed.\n";
}
''')
    exe = work / "bounds"
    ryu_object = work / "d2s.o"
    subprocess.run(["clang", "-std=c99", "-fsanitize=address,undefined", "-I", str(ryu_sources[0].parent.parent),
                    "-c", str(ryu_sources[0]), "-o", str(ryu_object)], check=True)
    subprocess.run(["clang++", "-std=c++17", "-fsanitize=address,undefined",
                    "-I", str(root / "include"), "-I", str(headers[0].parent),
                    "-I", str(ryu_sources[0].parent.parent), str(source), str(ryu_object),
                    str(native / "liblunasvg.a"), str(native / "plutovg/libplutovg.a"), "-o", str(exe)], check=True)
    subprocess.run([str(exe)], check=True)
