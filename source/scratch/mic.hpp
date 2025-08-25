#pragma once
#include <math.h>

#ifdef __3DS__
#include <3ds.h>
#include <malloc.h>
#endif

extern u8* micbuf;
extern u32 micbuf_datasize;
extern bool micInitialized;

int getMicLevel();
bool initMic();
void exitMic();
