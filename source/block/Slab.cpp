#include <grrlib.h>

#include "../Block.hpp"
#include "../Render.hpp"

#include "Slab.hpp"

static blockTexture *tex_slab;
static blockTexture *tex_slab_side;

static void render(s16 xPos, s16 yPos, s16 zPos, unsigned char pass) {
	if (pass == 1)
		return;
	Render::drawMultiTexBlock(xPos, yPos, zPos, tex_slab, tex_slab_side, tex_slab);
}

void slab_init() {
	blockEntry entry;
	entry.renderBlock = render;
	registerBlock(43, entry);
	tex_slab = getTexture(6, 0);
	tex_slab_side = getTexture(5, 0);
}
