#pragma once
#include "player.h"
#define nRenderChunks 128

typedef struct
{
	bool active;
	unsigned short x, z;
	void *displayList;
	unsigned int displayListSize;
	unsigned int displayListUsage;
} renderchunk;

extern renderchunk *renderchunks[nRenderChunks];
void chunked_init();
void chunked_rerenderChunk(unsigned short cx, unsigned short cz, bool force);
void chunked_refresh(int renderDistance, player thePlayer);
void chunked_render(player thePlayer);
int chunked_getfifousage();
int chunked_getfifototal();