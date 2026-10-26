"""Check SB2 source-over, clipping and cache identities with ASan/UBSan."""
from pathlib import Path
import subprocess
import tempfile

root = Path(__file__).resolve().parents[1]
with tempfile.TemporaryDirectory(prefix="sb2-layers-") as temporary:
    work = Path(temporary)
    source = work / "layers.cpp"
    source.write_text(r'''
#include "sb2_bitmap_layers.hpp"
#include <array>
#include <cassert>
#include <cmath>
#include <iostream>
#include <random>

int main() {
    using namespace Sb2BitmapLayers;
    auto a=key("3.png","4.png"), b=key("3.png","5.png");
    assert(a!=b && a!="3.png");
    assert(split(a)->first=="3.png" && split(a)->second=="4.png");
    assert(!split("3.png") && !split("__sb2text__3.png__") && !split("__sb2text____4.png"));
    std::mt19937 random(37);
    for(int i=0;i<10000;++i) {
        std::array<uint8_t,4> dst,src;
        for(auto &v:dst)v=random()%256;
        for(auto &v:src)v=random()%256;
        const auto original=dst;
        composite(dst.data(),1,1,src.data(),1,1);
        if(!src[3]) { assert(dst==original);continue; }
        double sa=src[3]/255., da=original[3]/255.;
        double alpha=sa+da*(1-sa);
        assert(dst[3]==std::round(alpha*255));
        for(int c=0;c<3;++c) {
            int expected=std::round((src[c]*sa+original[c]*da*(1-sa))/alpha);
            assert(std::abs(int(dst[c])-expected)<=1);
        }
    }
    std::array<uint8_t,12> dst={0,0,255,255, 0,0,255,255, 91,92,93,94};
    const std::array<uint8_t,24> text={255,0,0,255, 0,255,0,128, 255,255,255,255,
                                      255,255,255,255, 255,255,255,255, 255,255,255,255};
    composite(dst.data(),2,1,text.data(),3,2);
    assert((std::array<uint8_t,4>{dst[0],dst[1],dst[2],dst[3]}==std::array<uint8_t,4>{255,0,0,255}));
    assert((std::array<uint8_t,4>{dst[4],dst[5],dst[6],dst[7]}==std::array<uint8_t,4>{0,128,127,255}));
    assert(dst[8]==91 && dst[9]==92 && dst[10]==93 && dst[11]==94);
    // Empty overlays leave the original image intact.
    const auto unchanged=dst;
    composite(dst.data(),2,1,text.data(),0,2);assert(dst==unchanged);
    std::cout<<"10,000 RGBA blends, clipping, empty layers and distinct cache identities passed.\n";
}
''')
    exe = work / "layers"
    subprocess.run(["clang++", "-std=c++17", "-fsanitize=address,undefined",
                    "-I", str(root / "include"), str(source), "-o", str(exe)], check=True)
    subprocess.run([str(exe)], check=True)
