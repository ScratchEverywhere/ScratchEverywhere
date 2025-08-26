#pragma once
#include <math.h>

#ifdef __3DS__
#include <3ds.h>
#include <malloc.h>
#endif

extern u_int8_t* micbuf;
extern u_int32_t micbuf_datasize;
extern bool micInitialized;

int getMicLevel();
bool initMic();
void exitMic();
