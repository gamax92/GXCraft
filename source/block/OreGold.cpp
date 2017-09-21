#include <grrlib.h>

#include "../Block.hpp"
#include "../Render.hpp"

#include "OreGold.hpp"

static blockTexture *tex_ore_gold;

static void render(int xPos, int yPos, int zPos, unsigned char pass) {
	if (pass == 1) return;
	Render::drawBlock(xPos, yPos, zPos, tex_ore_gold);
}

void ore_gold_init() {
	blockEntry entry;
	entry.renderBlock = render;
	registerBlock(14, entry);
	tex_ore_gold = getTexture(0, 2);
}