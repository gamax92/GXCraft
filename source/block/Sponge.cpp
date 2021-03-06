#include <grrlib.h>

#include "../Block.hpp"
#include "../Render.hpp"

#include "Sponge.hpp"

static blockTexture *tex_sponge;

static void render(s16 xPos, s16 yPos, s16 zPos, unsigned char pass) {
	if (pass == 1)
		return;
	Render::drawBlock(xPos, yPos, zPos, tex_sponge);
}

void sponge_init() {
	blockEntry entry;
	entry.renderBlock = render;
	registerBlock(19, entry);
	tex_sponge = getTexture(0, 3);
}
