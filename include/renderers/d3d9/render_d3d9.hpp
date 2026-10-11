#pragma once
#define WIN32_MEAN_AND_LEAN
#include <d3d9.h>
#include <windows.h>

#define FVF_IMGVERTEX (D3DFVF_XYZRHW | D3DFVF_DIFFUSE | D3DFVF_TEX1)
typedef struct imgvertex {
    float x, y, z;
    float rhw;
    D3DCOLOR color;
    float tu, tv;
} IMGVERTEX;

#define FVF_PENVERTEX (D3DFVF_XYZRHW | D3DFVF_DIFFUSE)
typedef struct penvertex {
    float x, y, z;
    float rhw;
    D3DCOLOR color;
} PENVERTEX;

extern IDirect3D9 *pD3D;
extern IDirect3DDevice9 *pDev;
