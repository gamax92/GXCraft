#include <grrlib.h>

#include "../Block.hpp"
#include "../Render.hpp"

#include "Redshroom.hpp"

static blockTexture *tex_redshroom;

static void render(s16 xPos, s16 yPos, s16 zPos, unsigned char pass) {
	if (pass == 1)
		return;
	Render::drawBlockCrossed(xPos, yPos, zPos, tex_redshroom);
}

void redshroom_init() {
	blockEntry entry;
	entry.renderBlock = render;
	registerBlock(40, entry);
	tex_redshroom = getTexture(12, 1);
}
