#pragma once
#define WIN32_MEAN_AND_LEAN
#include <d3d9.h>
#include <d3dx9.h>
#include <d3dx9math.h>
#include <windows.h>

#define FVF_TEXVERTEX (D3DFVF_XYZRHW | D3DFVF_DIFFUSE | D3DFVF_TEX1)
typedef struct texvertex {
    float x, y, z;
    float rhw;
    D3DCOLOR color;
    float tu, tv;
} TEXVERTEX;

#define FVF_NOTEXVERTEX (D3DFVF_XYZRHW | D3DFVF_DIFFUSE)
typedef struct notexvertex {
    float x, y, z;
    float rhw;
    D3DCOLOR color;
} NOTEXVERTEX;

extern IDirect3D9 *pD3D;
extern IDirect3DDevice9 *pDev;
extern ID3DXConstantTable *pPixelShaderConstantTable;
