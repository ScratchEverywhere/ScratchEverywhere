"""Compare the shared C++ transform against Python's independent HSV conversion.

Run: python3 tests/color_effects_test.py
The reference follows Scratch's color/brightness order and grayscale floors:
https://github.com/scratchfoundation/scratch-render/blob/develop/src/shaders/sprite.frag
"""
import colorsys
import math
from pathlib import Path
import random
import subprocess
import tempfile

root=Path(__file__).resolve().parents[1]
source=r'''
#include <color_effects.hpp>
#include <iostream>
int main() {
    int r,g,b; float hue,brightness;
    while (std::cin >> r >> g >> b >> hue >> brightness) {
        uint8_t red=r,green=g,blue=b;
        ScratchColorTransform(hue,brightness).apply(red,green,blue);
        std::cout << int(red) << ' ' << int(green) << ' ' << int(blue) << '\n';
    }
}
'''
def reference(rgb,hue,brightness):
    r,g,b=(c/255 for c in rgb)
    if hue:
        h,s,v=colorsys.rgb_to_hsv(r,g,b)
        if v<.055:h,s,v=0,1,.055
        elif s<.09:h,s=0,.09
        r,g,b=colorsys.hsv_to_rgb((h+hue/200)%1,s,v)
    brightness=max(-100,min(brightness,100))/100
    return tuple(math.floor(max(0,min(1,c+brightness))*255+.5) for c in (r,g,b))

cases=[]
for rgb in [(0,0,0),(255,255,255),(128,128,128),(13,13,13),(14,14,14),(255,0,0),(0,255,0),(0,0,255),(150,140,145)]:
    for hue in [0,17,35,60,105,120,165,-95,200,400,-400,.01]:
        for brightness in [0,50,-50,100,-100,180,-180]:cases.append((rgb,hue,brightness))
rng=random.Random(2048)
for _ in range(1000):cases.append((tuple(rng.randrange(256) for _ in range(3)),rng.uniform(-600,600),rng.uniform(-150,150)))
with tempfile.TemporaryDirectory(prefix='scratch-color-') as directory:
    directory=Path(directory);(directory/'test.cpp').write_text(source)
    subprocess.run(['c++','-std=c++17','-O2','-I',str(root/'include'),str(directory/'test.cpp'),'-o',str(directory/'test')],check=True)
    data=''.join(f'{r} {g} {b} {hue} {brightness}\n' for (r,g,b),hue,brightness in cases)
    result=subprocess.run([str(directory/'test')],input=data,text=True,capture_output=True,check=True)
    actual=[tuple(map(int,line.split())) for line in result.stdout.splitlines()];assert len(actual)==len(cases)
    maximum=0
    for case,got in zip(cases,actual):
        expected=reference(*case);delta=max(abs(a-b) for a,b in zip(expected,got));maximum=max(maximum,delta)
        assert delta<=1,(case,expected,got)
    print(f'{len(cases)} color/brightness cases passed; maximum channel difference {maximum}/255')
