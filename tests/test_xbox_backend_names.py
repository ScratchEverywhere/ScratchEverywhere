#!/usr/bin/env python3
"""Check saved-name migration and the single Xbox backend registration."""
import pathlib
import re
import subprocess
import tempfile

ROOT = pathlib.Path(__file__).resolve().parents[1]
with tempfile.TemporaryDirectory(prefix="xbox-backend-names-") as directory:
    work = pathlib.Path(directory)
    source = work / "test.cpp"
    source.write_text('''#include <cassert>
#include <platforms/xbox/backend_names.hpp>
int main() {
    for (const char *name : {"Xbox", "NV2A", "SDL3"}) {
        assert(XboxBackend::rendererName(name)=="Xbox");
        assert(XboxBackend::windowingName(name)=="Xbox");
    }
    for (const char *name : {"Headless", "SDL2", "unknown", ""}) {
        assert(XboxBackend::rendererName(name)==name);
        assert(XboxBackend::windowingName(name)==name);
    }
}
''')
    subprocess.run(["c++", "-std=c++17", "-D__XBOX__", "-I"+str(ROOT/"include"),
                    str(source), "-o", str(work/"test")], check=True)
    subprocess.run([str(work/"test")], check=True)

for path, enum_name, names in [("include/renderer.hpp", "Renderers", "RendererNames"),
                               ("include/window.hpp", "WindowingBackends", "WindowingBackendNames")]:
    text = (ROOT/path).read_text()
    enum = re.search(r"enum class .*?\b"+enum_name+r"\s*:[^{]+\{([^}]+)\}", text, re.S).group(1)
    entries = [entry.strip() for entry in enum.split(",") if entry.strip() and not entry.strip().startswith("_")]
    labels = re.findall(r'"([^"]+)"', re.search(names+r"\s*=\s*\{([^}]+)\}", text, re.S).group(1))
    assert len(entries) == len(labels), (enum_name, entries, labels)
    assert entries.count("Xbox") == labels.count("Xbox") == 1
    assert "NV2A" not in entries and "NV2A" not in labels

assert not (ROOT/"source/renderers/nv2a").exists()
assert not (ROOT/"cmake/renderers/nv2a.cmake").exists()
for path in (ROOT/"source/renderers/xbox").glob("*.cpp"):
    assert not re.search(r"\bSDL_(?:CreateRenderer|CreateTexture|RenderTexture|UpdateTexture)\b", path.read_text()), path
print("PASS: legacy settings migrate, backend names align, and Xbox uses the direct renderer only")
