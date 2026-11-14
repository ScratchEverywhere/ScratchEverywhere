#!/bin/sh
set -eu

prefix=${1:-/opt/msys}
repo=https://repo.msys2.org/mingw/mingw64

packages="
mingw-w64-x86_64-llvm-23.1.5-1 4f22a9c6a6bf99fd59ab6511ef947b3af37679e6b92a8dbbf8b376d7165632ae
mingw-w64-x86_64-llvm-libs-23.1.5-1 570fefeced592d99c65236a9fc6655fd0bc88620e196ac9ac29df90b1b899a85
mingw-w64-x86_64-libgcc-16.2.0-6 e0815432c4225892b0eef57055fe507fa0b9a036436f046605f6629696bd1b88
mingw-w64-x86_64-libstdc++-16.2.0-6 924b47e8c3e20b6448e29d3333acc638a4f4783c1c45230e09673532f9c4f198
mingw-w64-x86_64-libwinpthread-14.0.0.r598.ge3d18511f-1 942a62a3ecafbb4f5a80016685a0e3002a8de903012eed49c213e4f5b693668a
mingw-w64-x86_64-libffi-3.8.0-1 6dad7f3c6c900d3c64a4fe0c2764b405e678b22df3521d2284ffaeff0198b84b
mingw-w64-x86_64-libxml2-2.15.4-3 5939674e6519082e2cb990f70ff0156a08876f85722d1178a3fdd7839fc38fb3
mingw-w64-x86_64-libiconv-1.19.1-1 50e915aa36fdbe6c84acc8954e939275609ba051cdb4eeeff3884453c28efde0
mingw-w64-x86_64-zlib-1.4-2 2a065fb3311d6c08bd2c7431a092b912ade08a83954474e06ea4eb0479d908cd
mingw-w64-x86_64-zstd-1.5.7-2 1add6705b344664f6aca108c85f79ab5bdd9e1162662bb06a4cf40a34f6e0907
"

download=$(mktemp -d)
mkdir -p "$prefix"
echo "$packages" | while read -r name sum; do
    [ -n "$name" ] || continue
    file="$download/$name-any.pkg.tar.zst"
    wget -q -O "$file" "$repo/$(echo "$name" | sed 's/+/%2B/g')-any.pkg.tar.zst"
    echo "$sum  $file" | sha256sum -c - >/dev/null
    bsdtar -xf "$file" -C "$prefix"
done
rm -rf "$download"

cd "$prefix/mingw64"
for dll in libLLVM-23; do
    (cd lib && gendef "../bin/$dll.dll" >/dev/null && x86_64-w64-mingw32-dlltool -d "$dll.def" -l "$dll.dll.a" -D "$dll.dll" && rm "$dll.def")
done

dlls="libLLVM-23 libstdc++-6 libgcc_s_seh-1 libwinpthread-1 libffi-8 libxml2-16 libiconv-2 zlib1 libzstd"
list=""
for dll in $dlls; do
    list="$list${list:+;}$prefix/mingw64/bin/$dll.dll"
done
echo "$list" > "$prefix/runtime-dlls.txt"
