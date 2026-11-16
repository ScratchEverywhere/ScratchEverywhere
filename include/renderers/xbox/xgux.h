#ifndef __XGUX_H
#define __XGUX_H

#include "xgu.h"

#ifndef XGUX_API
#ifdef __cplusplus
#define XGUX_API inline
#else
#define XGUX_API static inline
#endif
#endif

#define MIN(a,b) ((a)<(b)?(a):(b))
#define MAX_BATCH 120

XGUX_API
void xgux_draw_arrays(XguPrimitiveType mode, unsigned int start, unsigned int count) {
    uint32_t *p = pb_begin();
    p = xgu_begin(p, mode);
    while(count > 0) {

        /* Start next batch */
        pb_end(p);
        p = pb_begin();

        //FIXME: Maximum batch should be 256 elements maximum, will this work fine with all primitive types?
        unsigned int batch_count = MIN(count, MAX_BATCH);
        p = xgu_draw_arrays(p, start, batch_count);

        start += batch_count;
        count -= batch_count;
    }
    p = xgu_end(p);
    pb_end(p);
}

XGUX_API
void xgux_draw_elements16(XguPrimitiveType mode, const uint16_t* elements, unsigned int count) {
    uint32_t *p = pb_begin();
    p = xgu_begin(p, mode);

    /* Submit elements in pairs if possible */
    unsigned int pair_count = count / 2;
    unsigned int i = 0;
    while(i < pair_count) {

        /* Start next batch */
        pb_end(p);
        p = pb_begin();

        unsigned int batch_pair_count = MIN(pair_count - i, MAX_BATCH);
        p = xgu_element16(p, &elements[i * 2], batch_pair_count * 2);

        i += batch_pair_count;
    }

    /* Submit final index if necessary */
    if (count % 2) {
        uint32_t index = elements[count - 1];
        p = xgu_element32(p, &index, 1);
    }

    p = xgu_end(p);
    pb_end(p);
}

XGUX_API
void xgux_draw_elements32(XguPrimitiveType mode, const uint32_t* elements, unsigned int count) {
    uint32_t *p = pb_begin();
    p = xgu_begin(p, mode);

    /* Submit elements */
    while(count > 0) {

        /* Start next batch */
        pb_end(p);
        p = pb_begin();

        unsigned int batch_count = MIN(count, MAX_BATCH);
        p = xgu_element32(p, elements, batch_count);

        elements += batch_count;
        count -= batch_count;
    }

    p = xgu_end(p);
    pb_end(p);
}

XGUX_API
void xgux_set_clear_rect(unsigned int x, unsigned int y,
                         unsigned int width, unsigned int height) {
    uint32_t *p = pb_begin();
    p = xgu_set_clear_rect_horizontal(p, x, x+width);
    p = xgu_set_clear_rect_vertical(p, y, y+height);
    pb_end(p);
}

XGUX_API
void xgux_set_depth_range(float znear, float zfar) {
    uint32_t *p = pb_begin();
    uint32_t control0 = NV097_SET_CONTROL0_TEXTUREPERSPECTIVE | NV097_SET_CONTROL0_STENCIL_WRITE_ENABLE;
    p = push_command_parameter(p, NV097_SET_CONTROL0, control0);
    p = push_command_parameter(p, NV097_SET_ZMIN_MAX_CONTROL, 1);
    p = push_command_parameter(p, NV097_SET_COMPRESS_ZBUFFER_EN, 1);
    p = xgu_set_clip_min(p, znear);
    p = xgu_set_clip_max(p, zfar);
    pb_end(p);
}

XGUX_API
void xgux_set_attrib_pointer(XguVertexArray index, XguVertexArrayType format, unsigned int size, unsigned int stride, const void* data) {
    uint32_t *p = pb_begin();
    p = xgu_set_vertex_data_array_format(p, index, format, size, stride);
    p = xgu_set_vertex_data_array_offset(p, index, (void*)((uint32_t)data & 0x03ffffff));
    pb_end(p);
}

XGUX_API
void xgux_set_transform_program(int location, unsigned int count, const XguTransformProgramInstruction* instructions) {
    uint32_t *p = pb_begin();
    p = xgu_set_transform_program_load(p, location);
    while(count > 0) {

        /* Start next batch */
        pb_end(p);
        p = pb_begin();

        unsigned int batch_count = MIN(count, 8);
        p = xgu_set_transform_program(p, instructions, batch_count);

        instructions += batch_count;
        count -= batch_count;
    }
    pb_end(p);
}

XGUX_API
void xgux_set_transform_constant_vec4(int location, unsigned int count, const XguVec4* v) {
    uint32_t *p = pb_begin();
    p = xgu_set_transform_constant_load(p, 96 + location);
    while(count > 0) {

        /* Start next batch */
        pb_end(p);
        p = pb_begin();

        unsigned int batch_count = MIN(count, 8);
        p = xgu_set_transform_constant(p, v, batch_count);

        v += batch_count;
        count -= batch_count;
    }
    pb_end(p);
}

XGUX_API
void xgux_set_transform_constant_matrix4x4(unsigned int location, unsigned int count, bool transpose, const XguMatrix4x4* m) {
    for(unsigned int i = 0; i < count; i++) {
        if (transpose) {
            XguMatrix4x4 t;
            for(unsigned int row = 0; row < 4; row++) {
                for(unsigned int col = 0; col < 4; col++) {
                    t.col[row].f[col] = m[i].col[col].f[row];
                }
            }
            xgux_set_transform_constant_vec4(location + i*4, 4, &t.col[0]);
        } else {
            xgux_set_transform_constant_vec4(location + i*4, 4, &m[i].col[0]);
        }
    }
}


#if 0
#define GENERIC_ATTRIBUTE_TYPE(suffix, type, name, register, extra_arguments, x, y, z, w) \
XGUX_API \
static void name ## suffix(extra_arguments, type, x, y, z, w) { \
  xgux_set_vertex_data ## suffix(register, x, y, z, w);
}

#define GENERIC_ATTRIBUTE(name, register, extra_arguments, x, y, z, w) \
  GENERIC_ATTRIBUTE_TYPE(1f, float, name, register, extra_arguments, x) \
  GENERIC_ATTRIBUTE_TYPE(2f, float, name, register, extra_arguments, x, y) \
  GENERIC_ATTRIBUTE_TYPE(3f, float, name, register, extra_arguments, x, y, z) \
  GENERIC_ATTRIBUTE_TYPE(4f, float, name, register, extra_arguments, x, y, z, w)



//FIXME: Also support XGU_POSITION_ARRAY?
GENERIC_ATTRIBUTE(weight, XGU_WEIGHT_ARRAY,, x, y, z, w)
GENERIC_ATTRIBUTE(normal, XGU_NORMAL_ARRAY,, x, y, z, w)
GENERIC_ATTRIBUTE(diffuse, XGU_COLOR_ARRAY,, r, g, b, a)
GENERIC_ATTRIBUTE(specular, XGU_COLOR_ARRAY,, r, g, b, a)
GENERIC_ATTRIBUTE(fogcoord, XGU_POINT_SIZE_ARRAY,, x, y, z, w)
GENERIC_ATTRIBUTE(point_size, XGU_POINT_SIZE_ARRAY,, x, y, z, w)
GENERIC_ATTRIBUTE(back_diffuse, XGU_COLOR_ARRAY,, r, g, b, a)
GENERIC_ATTRIBUTE(back_specular, XGU_COLOR_ARRAY,, r, g, b, a)
GENERIC_ATTRIBUTE(texcoord, 9+index, unsigned int index, s, t, r, q)
GENERIC_ATTRIBUTE(vertex_attribute, index, unsigned int index, x, y, z, w)

#undef GENERIC_ATTRIBUTE
#undef GENERIC_ATTRIBUTE_TYPE
#endif

//FIXME: Remove p argument?
XGUX_API
uint32_t* xgux_set_color3f(uint32_t* p, float r, float g, float b) {
    return xgu_set_vertex_data4f(p, XGU_COLOR_ARRAY, r, g, b, 1.0f);
}

XGUX_API
uint32_t* xgux_set_color4f(uint32_t* p, float r, float g, float b, float a) {
    return xgu_set_vertex_data4f(p, XGU_COLOR_ARRAY, r, g, b, a);
}

XGUX_API
uint32_t* xgux_set_color4ub(uint32_t* p, uint8_t r, uint8_t g, uint8_t b, uint8_t a) {
    return xgu_set_vertex_data4ub(p, XGU_COLOR_ARRAY, r, g, b, a);
}

XGUX_API
uint32_t* xgux_set_texcoord3f(uint32_t* p, unsigned int index, float s, float t, float r) {
    return xgu_set_vertex_data4f(p, (XguVertexArray)(9+index), s, t, r, 1.0f);
}














// Wether we allow the MS D3D code
#if 1

#include <math.h>

static inline float mix(float a, float b, float t) {
  return a * (1.0f - t) + b * t;
}


#if 1
// This preprocessor block has been RE'd from Futurama (PAL) default.xbe.
// Any other D3D XBE would have worked; but I had this one readily available.

// In PAL Futurama, this is at offset 0x233360 in the binary
static const float ln64 = 4.158883094787598f; // ln(64)
// In PAL Futurama, this is at offset 0x233364 in the binary
static const float inv_ln2 = 1.4426950216293335f; // 1/ln(2)
// In PAL Futurama, this is at offset 0x233368 in the binary
static float table_a[32] = {
  -0.49459201097488403f, -0.49459201097488403f, -0.5707749724388123f, -0.8558430075645447f,
  -1.152451992034912f,   -1.436777949333191f,   -1.7059179544448853f, -1.948315978050232f,
  -2.1675729751586914f,  -2.3619871139526367f,  -2.5122361183166504f, -2.6528730392456055f,
  -2.78129506111145f,    -2.8909060955047607f,  -2.938739061355591f,  -3.017491102218628f,
  -3.0777618885040283f,  -3.0990869998931885f,  -3.14497709274292f,   -3.1009860038757324f,
  -3.1516079902648926f,  -3.2126359939575195f,  -3.219419002532959f,  -3.079401969909668f,
  -3.174921989440918f,   -3.4697060585021973f,  -2.8956680297851562f, -2.959918975830078f,
  -2.9171500205993652f,  -3.6003010272979736f,  -3.0249900817871094f, -3.299999952316284f
};
// In PAL Futurama, this is at offset 0x2333E8 in the binary
static float table_b[32] = {
  0.0f,                -0.02335299924015999f, -0.09511999785900116f, -0.17020800709724426f,
 -0.2510380148887634f, -0.3362079858779907f,  -0.4215390086174011f,  -0.503633975982666f,
 -0.5795919895172119f, -0.6476600170135498f,  -0.7085800170898438f,  -0.760208010673523f,
 -0.8036730289459229f, -0.8401650190353394f,  -0.8713439702987671f,  -0.8961049914360046f,
 -0.9164569973945618f, -0.9332619905471802f,  -0.9465069770812988f,  -0.9577550292015076f,
 -0.9661650061607361f, -0.9728479981422424f,  -0.9784129858016968f,  -0.9832170009613037f,
 -0.9864709973335266f, -0.9887779951095581f,  -0.9918370246887207f,  -0.9934520125389099f,
 -0.9948390126228333f, -0.9954339861869812f,  -0.9966899752616882f,  -1.0f
};

// Called from the cutoff calculation and the specular parameter setter
// This has been refactored a lot since being RE'd.
// The original did not use mix for exapmle.
static void magic_power_func(float x, float* a, float* b) {

  if (x < 1.0f) {
    //FIXME: Not 100% sure if this is expf or exp2f, but expf is much more likely due to ln64
    float t = (x == 0.0f) ? 0.0f : expf(-ln64 / x); // This effectively gets us powf(64, -1/x)
    *a = -t;
    *b = 1.0f - (1.0f - t) * x; //FIXME: mix(1.0f, t, x); ?
    return;
  }

  float index = 3.0f * logf(x) * inv_ln2; // This effectively gets us 3.0f * log2f(n)

  unsigned int index_i = truncf(index);
  float index_f = index - index_i;

  *a = mix(table_a[index_i + 0], table_a[index_i + 1], index_f);
  *b = mix(table_b[index_i + 0], table_b[index_i + 1], index_f);
}
#endif

// This is reversed from the D3D driver by MS, so this expects the D3D inputs
XGUX_API
void xgux_set_light_spot_d3d(unsigned int light_index, float theta, float phi, float falloff, XguVec3 direction) {
  uint32_t* p = pb_begin();

  //FIXME: WTF?!
  {
    XguVec3 v;
    magic_power_func(falloff, &v.x, &v.y);
    v.z = 1.0f + v.x - v.y;
    p = xgu_set_light_spot_falloff(p, light_index, v);
  }

  //FIXME: Clamp theta and phi first?
  float c_theta = cosf(0.5f * theta);
  float c_phi = cosf(0.5f * phi);
  if (c_phi >= c_theta) {        
    c_phi = 0.999f * c_theta;
  }
  float unk0 = 1.0f / (c_theta - c_phi);
  float unk1 = -c_phi * unk0;
  {
    XguVec4 v;
    //FIXME: Normalize spot-direction first?
    v.x = direction.x * unk0;
    v.y = direction.y * unk0;
    v.z = direction.z * unk0;
    v.w = unk1;
    p = xgu_set_light_spot_direction(p, light_index, v);
  }

  pb_end(p);
}



static void _set_specular_d3d(bool back, float power) {
  float k[6];

  magic_power_func(power, &k[0], &k[1]);
  k[2] = 1.0f + k[0] - k[1];

  magic_power_func(0.5f * power, &k[3], &k[4]);
  k[5] = 1.0f + k[3] - k[4];

  uint32_t *p = pb_begin();
  if (!back) {
    p = xgu_set_specular(p, k[0], k[1], k[2], k[3], k[4], k[5]);
  } else {
    p = xgu_set_back_specular(p, k[0], k[1], k[2], k[3], k[4], k[5]);
  }
  pb_end(p);
}

XGUX_API
void xgux_set_specular_d3d(float power) {
  _set_specular_d3d(false, power);
}

XGUX_API
void xgux_set_back_specular_d3d(float power) {
  _set_specular_d3d(true, power);
}

#endif
// End of MS D3D











// Wether we allow MESA GL code
#if 1

#include <math.h>

static inline float clamp(float x, float min, float max) {
  if (x < min) { x = min; }
  if (x > max) { x = max; }
  return x;  
}

static inline float max(float a, float b) {
  return (a > b) ? a : b;
}

#if 1
// This preprocessor block is:

/*
 * Copyright (C) 2009-2010 Francisco Jerez.
 * All Rights Reserved.
 *
 * Permission is hereby granted, free of charge, to any person obtaining
 * a copy of this software and associated documentation files (the
 * "Software"), to deal in the Software without restriction, including
 * without limitation the rights to use, copy, modify, merge, publish,
 * distribute, sublicense, and/or sell copies of the Software, and to
 * permit persons to whom the Software is furnished to do so, subject to
 * the following conditions:
 *
 * The above copyright notice and this permission notice (including the
 * next paragraph) shall be included in all copies or substantial
 * portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
 * EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.
 * IN NO EVENT SHALL THE COPYRIGHT OWNER(S) AND/OR ITS SUPPLIERS BE
 * LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION
 * OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION
 * WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
 *
 */

static float
get_shine(const float p[], float x)
{
	const int n = 15;
	const float *y = &p[1];
	float f = (n - 1) * (1 - 1 / (1 + p[0] * x))
		/ (1 - 1 / (1 + p[0] * 1024));
	int i = f;

	/* Linear interpolation in f-space (Faster and somewhat more
	 * accurate than x-space). */
	if (x == 0)
		return y[0];
	else if (i > n - 2)
		return y[n - 1];
	else
		return y[i] + (y[i + 1] - y[i]) * (f - i);
}

static const float nv10_spot_params[2][16] = {
	{ 0.02, -3.80e-05, -1.77, -2.41, -2.71, -2.88, -2.98, -3.06,
	  -3.11, -3.17, -3.23, -3.28, -3.37, -3.47, -3.83, -5.11 },
	{ 0.02, -0.01, 1.77, 2.39, 2.70, 2.87, 2.98, 3.06,
	  3.10, 3.16, 3.23, 3.27, 3.37, 3.47, 3.83, 5.11 },
};

static
void
nv10_get_spot_coeff(float SpotExponent, float SpotCutoff, XguVec3 _NormSpotDirection, float k[7])
{
	float e = SpotExponent;
	float a0, b0, a1, a2, b2, a3;

	if (e > 0)
		a0 = -1 - 5.36e-3 / sqrtf(e);
	else
		a0 = -1;
	b0 = 1 / (1 + 0.273 * e);

	a1 = get_shine(nv10_spot_params[0], e);

	a2 = get_shine(nv10_spot_params[1], e);
	b2 = 1 / (1 + 0.273 * e);

	a3 = 0.9 + 0.278 * e;

	if (SpotCutoff > 0) {

		// Moved for use without mesa
		float _CosCutoff = cosf(SpotCutoff);
		if (_CosCutoff < 0.0f) { _CosCutoff = 0.0f; }

		float cutoff = max(a3, 1 / (1 - _CosCutoff));

		k[0] = max(0, a0 + b0 * cutoff);
		k[1] = a1;
		k[2] = a2 + b2 * cutoff;
		k[3] = - cutoff * _NormSpotDirection.x;
		k[4] = - cutoff * _NormSpotDirection.y;
		k[5] = - cutoff * _NormSpotDirection.z;
		k[6] = 1 - cutoff;

	} else {
		k[0] = b0;
		k[1] = a1;
		k[2] = a2 + b2;
		k[3] = - _NormSpotDirection.x;
		k[4] = - _NormSpotDirection.y;
		k[5] = - _NormSpotDirection.z;
		k[6] = -1;
	}
}



static const float nv10_shininess_param[6][16] = {
	{ 0.70, 0.00, 0.06, 0.06, 0.05, 0.04, 0.02, 0.00,
	  -0.06, -0.13, -0.24, -0.36, -0.51, -0.66, -0.82, -1.00 },
	{ 0.01, 1.00, -2.29, -2.77, -2.96, -3.06, -3.12, -3.18,
	  -3.24, -3.29, -3.36, -3.43, -3.51, -3.75, -4.33, -5.11 },
	{ 0.02, 0.00, 2.28, 2.75, 2.94, 3.04, 3.1, 3.15,
	  3.18, 3.22, 3.27, 3.32, 3.39, 3.48, 3.84, 5.11 },
	{ 0.70, 0.00, 0.05, 0.06, 0.06, 0.06, 0.05, 0.04,
	  0.02, 0.01, -0.03, -0.12, -0.25, -0.43, -0.68, -0.99 },
	{ 0.01, 1.00, -1.61, -2.35, -2.67, -2.84, -2.96, -3.05,
	  -3.08, -3.14, -3.2, -3.26, -3.32, -3.42, -3.54, -4.21 },
	{ 0.01, 0.00, 2.25, 2.73, 2.92, 3.03, 3.09, 3.15,
	  3.16, 3.21, 3.25, 3.29, 3.35, 3.43, 3.56, 4.22 },
};

static
void
nv10_get_shininess_coeff(float s, float k[6])
{
	int i;

	for (i = 0; i < 6; i++)
		k[i] = get_shine(nv10_shininess_param[i], s);
}


#endif

// This uses code from nouveau which was meant for GL
XGUX_API
void xgux_set_light_spot_gl(unsigned int light_index, float exponent, float cutoff, XguVec3 direction) {
  uint32_t* p = pb_begin();

  float k[7];
  //FIXME: Normalize spot-direction first?
  nv10_get_spot_coeff(exponent, cutoff, direction, k);

  {
    XguVec3 v;
    v.x = k[0];
    v.y = k[1];
    v.z = k[2];
    p = xgu_set_light_spot_falloff(p, light_index, v);
  }
  {
    XguVec4 v;
    v.x = k[3];
    v.y = k[4];
    v.z = k[5];
    v.w = k[6];
    p = xgu_set_light_spot_direction(p, light_index, v);
  }

  pb_end(p);
}

static void _set_specular_gl(bool back, float shininess) {
  float k[6];
  nv10_get_shininess_coeff(clamp(shininess, 0.0f, 1024.0f), k);

  uint32_t *p = pb_begin();
  if (!back) {
    p = xgu_set_specular(p, k[0], k[1], k[2], k[3], k[4], k[5]);
  } else {
    p = xgu_set_back_specular(p, k[0], k[1], k[2], k[3], k[4], k[5]);
  }
  pb_end(p);
}

XGUX_API
void xgux_set_specular_gl(float shininess) {
  _set_specular_gl(false, shininess);
}

XGUX_API
void xgux_set_back_specular_gl(float shininess) {
  _set_specular_gl(true, shininess);
}

#endif
// End of MESA GL


#endif
