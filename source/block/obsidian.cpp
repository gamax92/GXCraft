#include <grrlib.h>

#include "../block.hpp"
#include "../render.hpp"

#include "obsidian.hpp"

static blockTexture *tex_obsidian;

static void render(int xPos, int yPos, int zPos, unsigned char pass) {
	if (pass == 1) return;
	Render::drawBlock(xPos, yPos, zPos, tex_obsidian);
}

void obsidian_init() {
	blockEntry entry;
	entry.renderBlock = render;
	registerBlock(49, entry);
	tex_obsidian = getTexture(5, 2);
}