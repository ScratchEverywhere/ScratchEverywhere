#!/usr/bin/env python3
"""Check Scratch 2 text semantics, including original exported SVG markup."""
import pathlib
import subprocess
import tempfile
import xml.etree.ElementTree as ET

ROOT = pathlib.Path(__file__).resolve().parents[1]
with tempfile.TemporaryDirectory(prefix="scratch2-svg-") as work:
    work = pathlib.Path(work)
    source = work / "normalize.cpp"
    source.write_text('''#include "scratch2_svg.hpp"
#include <iostream>
#include <iterator>
int main() {
    std::string svg((std::istreambuf_iterator<char>(std::cin)), {});
    std::cout << Scratch2Svg::normalizeText(svg);
}
''')
    exe = work / "normalize"
    subprocess.run(["clang++", "-std=c++17", "-fsanitize=address,undefined",
                    "-I", str(ROOT / "include"), str(source), "-o", str(exe)], check=True)

    def normalize(svg):
        return subprocess.run([str(exe)], input=svg, text=True, capture_output=True, check=True).stdout

    # The source positions must not be added to the exported transform.
    svg = '''<svg xmlns="http://www.w3.org/2000/svg" xmlns:xml="http://www.w3.org/XML/1998/namespace">
<text x="83" y="116.25" font-family="Helvetica" font-size="22"
 transform="matrix(2.663622856 0 0 2.663622856 71.75 77.1)">Game Over</text>
<text x="90" y="266.25" transform="translate(92 239.15)">First &amp; last
           the highscore!

End</text></svg>'''
    root = ET.fromstring(normalize(svg))
    text, multiline = list(root)
    assert text.get("x") is None and text.get("y") is None
    assert text.get("alignment-baseline") == "text-before-edge"
    assert text.get("transform").startswith("matrix(2.663622856 0 0 2.663622856 71.75 77.1)")
    assert text.get("transform").endswith("translate(2 0.000000)")
    assert text[0].text == "Game Over" and text[0].get("dy") == "1.2em"
    assert multiline.get("font-size") == "18"
    assert [x.text for x in multiline] == ["First & last", "           the highscore!", " ", "End"]
    assert all(x.get("x") == "0" and x.get("style") == "white-space: pre" for x in multiline)
    t = ET.fromstring(normalize('<text>First\rSecond\r\nThird\nFourth</text>'))
    assert [x.text for x in t] == ["First", "Second", "Third", "Fourth"]

    for family, spacing, offset in [("Handwriting", "2", -11), ("Scratch", "0.89", -3),
                                    ("Curly", "1.38", -6), ("Marker", "1.45", -6),
                                    ("Sans Serif", "1.13", -3), ("Serif", "1.25", -4)]:
        t = ET.fromstring(normalize(f'<text font-family="{family}" font-size="44">Hi</text>'))
        assert t[0].get("dy") == spacing + "em"
        assert t.get("transform").endswith(f"translate(2 {offset * 2:.6f})")

    # Retain existing tspans, entities, quoted delimiters and non-text geometry.
    svg = """<svg><!-- <text x='99'>ignored</text> -->
<![CDATA[<text>ignored</text>]]><path title='a > b' d='M0 0L1 1'/>
<text x = '52' y='34' title='a > b'><tspan x='7' dy='2em'>A &lt; B</tspan></text>
<textPath>untouched</textPath></svg>"""
    out = normalize(svg)
    assert "<!-- <text x='99'>ignored</text> -->" in out
    assert "<![CDATA[<text>ignored</text>]]>" in out
    assert "<path title='a > b' d='M0 0L1 1'/>" in out
    assert "<tspan x='7' dy='2em'>A &lt; B</tspan>" in out
    assert "<textPath>untouched</textPath>" in out
    ET.fromstring(out)
    for incomplete in ["", "<text", '<text x="unterminated>', '<text x="1">no end', "<!-- no end"]:
        assert normalize(incomplete) == incomplete

print("Scratch 2 SVG text checks passed (address/undefined sanitizers enabled).")
