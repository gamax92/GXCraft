/*===========================================
    GXCraft by gamax92 and ds84182
============================================*/

#include <cstdlib>
#include <cmath>

#include <grrlib.h>
#include <wiiuse/wpad.h>
#include <ogc/lwp_watchdog.h>

extern "C" {
#include "font_png.h"
#include "inventory_png.h"
#include "inv_select_png.h"
#include "terrain_blocks_png.h"
#include "cursor_png.h"
#include "clouds_png.h"
}

#include "Main.hpp"
#include "Entity.hpp"
#include "Controls.hpp"
#include "Utils.hpp"
#include "Block.hpp"
#include "World.hpp"
#include "Render.hpp"
#include "Fail3D.hpp"
#include "NetcatLogger.hpp"
#include "ChunkedRender.hpp"
#include "BlockTextures.hpp"
#include "BlockIcons.hpp"
#include "block/BlockIncludes.hpp"

#define ticks_to_secsf(ticks) (static_cast<f64>(ticks) / static_cast<f64>(TB_TIMER_CLOCK * 1000))

// GRRLIB_Render does not call GX_Flush, resulting in garbage/old frames
#define GRRLIB_Render() \
	do { \
		GRRLIB_Render(); \
		GX_Flush(); \
	} while (0)

unsigned int seed = 0; // 0 = Generate Seed
GRRLIB_texImg *tex_blockicons[256];

Player thePlayer;
World *theWorld;

inline double to_degrees(double radians) {
	return radians * (180.0 / M_PI);
}

inline double to_radians(double degrees) {
	return (degrees * M_PI) / 180.0;
}

static void initializeBlocks();
static u8 CalculateFrameRate();
static void color85(char *buf, u32 color);

typedef enum { NETCAT, REGISTER, GENERATE, INGAME, NUNCHUK, SCREENSHOT } gamestate;

static bool exitloop = false;
static bool shutdown = false;

static void ResetCallback(u32 irq, void *ctx) {
	shutdown = false;
	exitloop = true;
}

static void PowerCallback() {
	shutdown = true;
	exitloop = true;
}

int main(int argc, char *argv[]) {
	bool netcat = false;
	for (int i = 0; i < argc; i++) {
		if (!strcmp(argv[i], "netcat")) {
			netcat = true;
			break;
		}
	}
	if (netcat) {
		Netcat::console();
	}

	if (seed == 0) {
		srand(time(NULL));
		seed = rand();
	}
	srand(seed);

	int renderDistance = 96;

	gamestate status;
	if (Netcat::init)
		status = NETCAT;
	else
		status = REGISTER;

	u8 FPS = 0;

	bool rerenderDisplayList = false;
	unsigned int scr_scanY = 0;
	size_t dluse = 0;
	size_t dlsize = 0;

	// Initialize the player
	thePlayer.posX = floorf(worldX / 2.0f) + 0.5f;
	thePlayer.posZ = floorf(worldZ / 2.0f) + 0.5f;
	thePlayer.motionX = 0;
	thePlayer.motionY = 0;
	thePlayer.motionZ = 0;
	thePlayer.lookX = 0;
	thePlayer.lookY = 0;
	thePlayer.lookZ = 0;
	unsigned char startinv[10] = {1, 4, 45, 3, 5, 17, 18, 20, 43, 0};
	memcpy(thePlayer.inventory, startinv, sizeof(thePlayer.inventory));
	thePlayer.flying = true;
	thePlayer.select = false;
	thePlayer.timer = 0;

	bool wasUnder = false;

	int displistX = 256;
	int displistZ = 256;

	GRRLIB_Init();
	WPAD_Init();
	Chunked::init();
	initTextures();
	Fail3D::init(416);

	WPADData *data;
	ir_t IR_0;

	GRRLIB_SetAntiAliasing(false);

	// clang-format off
	GRRLIB_texImg *tex_font       = GRRLIB_LoadTexture(font_png);
	GRRLIB_texImg *tex_inventory  = GRRLIB_LoadTexture(inventory_png);
	GRRLIB_texImg *tex_inv_select = GRRLIB_LoadTexture(inv_select_png);
	GRRLIB_texImg *tex_terrain    = GRRLIB_LoadTexture(terrain_blocks_png);
	GRRLIB_texImg *tex_cursor     = GRRLIB_LoadTexture(cursor_png);
	GRRLIB_texImg *tex_clouds     = GRRLIB_LoadTexture(clouds_png);
	GRRLIB_texImg *tex_tmpscreen  = GRRLIB_CreateEmptyTexture(rmode->fbWidth, rmode->efbHeight);
	// clang-format on

	//GRRLIB_texImg *tex_blockicons[256];

	GRRLIB_SetMidHandle(tex_cursor, true);

	GRRLIB_InitTileSet(tex_font, 16, 16, 32);

	GRRLIB_SetBackgroundColour(0x00, 0x00, 0x00, 0xFF);

	WPAD_SetDataFormat(WPAD_CHAN_0, WPAD_FMT_BTNS_ACC_IR);

	SYS_SetResetCallback(ResetCallback);
	SYS_SetPowerCallback(PowerCallback);

	// clang-format off
	u8 inv_blocks[42] = {
	 1,  4, 45,  3,  5, 17, 18, 20, 43,
	48,  6, 37, 38, 39, 40, 12, 13, 19,
	21, 22, 23, 24, 25, 26, 27, 28, 29,
	30, 31, 32, 33, 34, 35, 36, 16, 15,
	14, 42, 41, 47, 46, 49 };
	// clang-format on

	f64 lastTime = ticks_to_secsf(gettime());

	float cloudPos = 0;

	size_t memusage;
	double xLook, yLook, zLook;
	struct mallinfo meminfo;

	bool showDebug = false;

	while (!exitloop) {
		f64 thisTime = ticks_to_secsf(gettime());
		f64 deltaTime = (thisTime - lastTime);
		switch (status) {
		case NETCAT: { // Listen for clients
			GRRLIB_2dMode();
			GRRLIB_Printf(152, 232, tex_font, 0xFFFFFFFF, 1, "WAITING FOR CLIENT...");
			GRRLIB_Render();
			Netcat::accept();
			status = REGISTER;
			break;
		}
		case REGISTER: { // Register blocks
			GRRLIB_2dMode();
			GRRLIB_Printf(152, 232, tex_font, 0xFFFFFFFF, 1, "REGISTERING BLOCKS...");
			GRRLIB_Render();
			Netcat::log("registering blocks\n");
			initializeBlocks();
			load_bi();
			status = GENERATE;
			break;
		}
		case GENERATE: { // Generate the world
			GRRLIB_2dMode();
			GRRLIB_Printf(160, 232, tex_font, 0xFFFFFFFF, 1, "GENERATING WORLD ...");
			GRRLIB_Render();
			Netcat::log("generating world\n");
			theWorld = new World(seed);
			int y;
			for (y = worldY - 1; y >= 0; y--) {
				if (theWorld->getBlock(static_cast<int>(thePlayer.posX), y, static_cast<int>(thePlayer.posZ)) != 0) {
					thePlayer.posY = y + 1;
					break;
				}
			}
			GRRLIB_SetBackgroundColour(0x9E, 0xCE, 0xFF, 0xFF);
			GX_SetLineWidth(15, GX_TO_ONE);

			// Generate all initial chunks
			Chunked::refresh(renderDistance);
			Chunked::rerenderChunkUpdates(true);
			Chunked::getfifousage(&dluse, &dlsize);
			status = INGAME;
			break;
		}
		case INGAME: { // Main loop
			// Reset Motion
			thePlayer.motionX = 0;
			thePlayer.motionY = 0;
			thePlayer.motionZ = 0;

			if (thePlayer.timer > 0) {
				thePlayer.timer -= 60.0 / static_cast<double>(FPS);
			}

			// Handle Controller Input
			WPAD_ScanPads();
			WPAD_IR(WPAD_CHAN_0, &IR_0);

			data = WPAD_Data(WPAD_CHAN_0);
			if (data->exp.type != WPAD_EXP_NUNCHUK)
				status = NUNCHUK;

			if (WPAD_ButtonsDown(WPAD_CHAN_0) & WPAD_BUTTON_HOME) {
				shutdown = false;
				exitloop = true;
				break;
			}
			if (WPAD_ButtonsHeld(WPAD_CHAN_0) & WPAD_BUTTON_UP) {
				thePlayer.motionX += sin(to_radians(thePlayer.lookX)) * 6;
				thePlayer.motionZ -= cos(to_radians(thePlayer.lookX)) * 6;
			}
			if (WPAD_ButtonsHeld(WPAD_CHAN_0) & WPAD_BUTTON_DOWN) {
				thePlayer.motionX -= sin(to_radians(thePlayer.lookX)) * 6;
				thePlayer.motionZ += cos(to_radians(thePlayer.lookX)) * 6;
			}
			if (WPAD_ButtonsHeld(WPAD_CHAN_0) & WPAD_BUTTON_LEFT) {
				thePlayer.motionX -= cos(to_radians(thePlayer.lookX)) * 6;
				thePlayer.motionZ -= sin(to_radians(thePlayer.lookX)) * 6;
			}
			if (WPAD_ButtonsHeld(WPAD_CHAN_0) & WPAD_BUTTON_RIGHT) {
				thePlayer.motionX += cos(to_radians(thePlayer.lookX)) * 6;
				thePlayer.motionZ += sin(to_radians(thePlayer.lookX)) * 6;
			}
			if (WPAD_ButtonsDown(WPAD_CHAN_0) & WPAD_BUTTON_1) {
				thePlayer.flying = !thePlayer.flying;
			}
			if (WPAD_ButtonsDown(WPAD_CHAN_0) & WPAD_NUNCHUK_BUTTON_C) {
				thePlayer.select = !thePlayer.select;
			}
			if (WPAD_ButtonsDown(WPAD_CHAN_0) & WPAD_NUNCHUK_BUTTON_Z) {
				showDebug = !showDebug;
			}
			thePlayer.lookX += WPAD_StickX(WPAD_CHAN_0, 0) * deltaTime / 2.0f;
			thePlayer.lookY -= WPAD_StickY(WPAD_CHAN_0, 0) * deltaTime / 2.0f;

			if (thePlayer.flying) {
				if (WPAD_ButtonsHeld(WPAD_CHAN_0) & WPAD_BUTTON_PLUS)
					thePlayer.motionY += 6;
				if (WPAD_ButtonsHeld(WPAD_CHAN_0) & WPAD_BUTTON_MINUS)
					thePlayer.motionY -= 6;
			} else {
				if (WPAD_ButtonsDown(WPAD_CHAN_0) & WPAD_BUTTON_PLUS) {
					thePlayer.inventory[9] += 1;
					thePlayer.inventory[9] %= 9;
				}
				if (WPAD_ButtonsDown(WPAD_CHAN_0) & WPAD_BUTTON_MINUS) {
					if (thePlayer.inventory[9] == 0)
						thePlayer.inventory[9] = 9;
					thePlayer.inventory[9] -= 1;
				}
			}

			// Limit motion speed
			if (wasUnder) {
				thePlayer.motionX = thePlayer.motionX / 2;
				thePlayer.motionZ = thePlayer.motionZ / 2;
				// Restrict Y
			} else {
				// Restrict Y
			}

			// Apply motion to player
			thePlayer.moveEntity(deltaTime);

			// Keep values in bound
			if (thePlayer.lookX > 180)
				thePlayer.lookX -= 360;
			else if (thePlayer.lookX < -180)
				thePlayer.lookX += 360;
			if (thePlayer.lookY > 90)
				thePlayer.lookY = 90;
			else if (thePlayer.lookY < -90)
				thePlayer.lookY = -90;
			if (thePlayer.lookZ > 180)
				thePlayer.lookZ -= 360;
			else if (thePlayer.lookZ < -180)
				thePlayer.lookZ += 360;

			//Netcat::log("switching 3d\n");
			GRRLIB_3dMode(0.1f, 1000.0f, 60.0f, true, false);
			GX_SetZCompLoc(GX_FALSE);

			if (theWorld->getBlock(floorf(thePlayer.posX), floorf(thePlayer.posY + 1.625f), floorf(thePlayer.posZ)) == 8) {
				if (!wasUnder) {
					GRRLIB_SetBackgroundColour(0x05, 0x05, 0x33, 0xFF);
					wasUnder = true;
				}
				GXColor c = {0x05, 0x05, 0x33};
				GX_SetFog(GX_FOG_LIN, 0.0f, 32.0f, 0.1f, 1000.0f, c);
			} else {
				if (wasUnder) {
					GRRLIB_SetBackgroundColour(0x9E, 0xCE, 0xFF, 0xFF);
					wasUnder = false;
				}
				GXColor c = {0x9E, 0xCE, 0xFF};
				GX_SetFog(GX_FOG_LIN, renderDistance - 32, renderDistance - chunkSize, 0.1f, 1000.0f, c);
			}

			GRRLIB_ObjectViewBegin();
			GRRLIB_ObjectViewTrans(-thePlayer.posX, -thePlayer.posY - 1.625f, -thePlayer.posZ);
			GRRLIB_ObjectViewRotate(0, thePlayer.lookX, 0);
			GRRLIB_ObjectViewRotate(thePlayer.lookY, 0, 0);
			GRRLIB_ObjectViewEnd();

			if (abs(displistX - thePlayer.posX) + abs(displistZ - thePlayer.posZ) > 8) {
				Netcat::log("rerender display list because player too far from last render point\n");
				rerenderDisplayList = true;
			}

			xLook = sin(to_radians(thePlayer.lookX)) * cos(to_radians(thePlayer.lookY));
			yLook = -sin(to_radians(thePlayer.lookY));
			zLook = -cos(to_radians(thePlayer.lookX)) * cos(to_radians(thePlayer.lookY));

			int selBlockX, selBlockY, selBlockZ, faceBlockX, faceBlockY, faceBlockZ;
			if (Utils::voxelCollisionRay(thePlayer.posX, thePlayer.posY + 1.625f, thePlayer.posZ, xLook * 7.0, yLook * 7.0, zLook * 7.0, &selBlockX, &selBlockY, &selBlockZ, &faceBlockX, &faceBlockY, &faceBlockZ)) {
				GRRLIB_SetTexture(tex_inventory, false);
				Render::drawSelectionBlock(selBlockX, selBlockY, selBlockZ);

				if (WPAD_ButtonsHeld(WPAD_CHAN_0) & WPAD_BUTTON_B && !thePlayer.select && thePlayer.timer <= 0 && theWorld->getBlock(selBlockX, selBlockY, selBlockZ) != 7) {
					theWorld->setBlock(selBlockX, selBlockY, selBlockZ, 0);
					thePlayer.timer = 18;
				} else if (WPAD_ButtonsHeld(WPAD_CHAN_0) & WPAD_BUTTON_A && !thePlayer.select && thePlayer.timer <= 0 && thePlayer.inventory[thePlayer.inventory[9]] != 0) {
					if (faceBlockX != 0 || faceBlockY != 0 || faceBlockZ != 0) {
						selBlockX += faceBlockX;
						selBlockY += faceBlockY;
						selBlockZ += faceBlockZ;

						uint8_t oldBlock = theWorld->getBlock(selBlockX, selBlockY, selBlockZ);
						bool wasStuck = thePlayer.isStuck();
						theWorld->setBlock(selBlockX, selBlockY, selBlockZ, thePlayer.inventory[thePlayer.inventory[9]]);
						if (!wasStuck && thePlayer.isStuck())
							theWorld->setBlock(selBlockX, selBlockY, selBlockZ, oldBlock);
						else
							thePlayer.timer = 18;
					}
				}
			}

			// Tick the world
			theWorld->updateWorld(renderDistance);

			// Draw Clouds
			GRRLIB_SetTexture(tex_clouds, true);

			GX_Begin(GX_QUADS, GX_VTXFMT0, 4);
			GX_Position3f32(-worldX, worldY + 3, -worldZ);
			GX_Color1u32(0xFFFFFFFF);
			GX_TexCoord2f32(cloudPos, 0.0f);
			GX_Position3f32(-worldX, worldY + 3, worldZ * 2);
			GX_Color1u32(0xFFFFFFFF);
			GX_TexCoord2f32(cloudPos, 1.0f);
			GX_Position3f32(worldX * 2, worldY + 3, worldZ * 2);
			GX_Color1u32(0xFFFFFFFF);
			GX_TexCoord2f32(cloudPos + 1.0f, 1.0f);
			GX_Position3f32(worldX * 2, worldY + 3, -worldZ);
			GX_Color1u32(0xFFFFFFFF);
			GX_TexCoord2f32(cloudPos + 1.0f, 0.0f);
			GX_End();

			cloudPos = fmodf(cloudPos + deltaTime / 1000.0f, 1.0f);

			if (rerenderDisplayList) {
				Netcat::log("rerendering display list\n");
				rerenderDisplayList = false;
				displistX = thePlayer.posX;
				displistZ = thePlayer.posZ;
				Chunked::refresh(renderDistance);
			}
			if (Chunked::rerenderChunkUpdates(false))
				Chunked::getfifousage(&dluse, &dlsize);

			GRRLIB_SetTexture(tex_terrain, false);
			Chunked::render();

			// Draw 2D elements
			//Netcat::log("switching 2d\n");
			GRRLIB_2dMode();

			GRRLIB_SetBlend(GRRLIB_BLEND_INV);
			GRRLIB_Rectangle(308, 239, 24, 2, 0xFFFFFFFF, true);
			GRRLIB_Rectangle(319, 228, 2, 11, 0xFFFFFFFF, true);
			GRRLIB_Rectangle(319, 241, 2, 11, 0xFFFFFFFF, true);
			GRRLIB_SetBlend(GRRLIB_BLEND_ALPHA);

			GRRLIB_DrawImg(138, 436, tex_inventory, 0, 2, 2, 0xFFFFFFFF);

			int b;
			for (b = 0; b < 9; b++) {
				GRRLIB_DrawImg(b * 40 + 144, 442, tex_blockicons[thePlayer.inventory[b]], 0, 1, 1, 0xFFFFFFFF);
			}

			GRRLIB_DrawImg(thePlayer.inventory[9] * 40 + 136, 434, tex_inv_select, 0, 2, 2, thePlayer.flying ? 0xBFBFBFFF : 0xFFFFFFFF);

			if (thePlayer.select) {
				GRRLIB_Rectangle(80, 60, 480, 300, 0x0000007F, true);

				Render::drawText(224, 80, tex_font, "SELECT BLOCK");

				// Draw selection box (52x52)
				signed short sx, sy, sb;
				sx = (static_cast<short>(IR_0.sx) - 52) / 48 - 1;
				sy = (static_cast<short>(IR_0.sy) - 58) / 48 - 1;
				sb = sy * 9 + sx;
				if (sx >= 0 && sx < 9 && sy >= 0 && sb < 42)
					GRRLIB_Rectangle(sx * 48 + 98, sy * 48 + 104, 52, 52, 0xFFFFFF7F, true);

				int x, y;
				for (b = 0; b < 42; b++) {
					x = b % 9;
					y = floor(b / 9);
					GRRLIB_DrawImg(x * 48 + 108, y * 48 + 114, tex_blockicons[inv_blocks[b]], 0, 1, 1, 0xFFFFFFFF);
				}
				GRRLIB_SetAntiAliasing(true);
				GRRLIB_DrawImg(IR_0.sx, IR_0.sy, tex_cursor, IR_0.angle, 1, 1, 0xFFFFFFFF);
				GRRLIB_SetAntiAliasing(false);

				if (WPAD_ButtonsHeld(WPAD_CHAN_0) & WPAD_BUTTON_A) {
					if (sx >= 0 && sx < 9 && sy >= 0 && sb < 42)
						thePlayer.inventory[thePlayer.inventory[9]] = inv_blocks[sb];
					thePlayer.select = false;
					thePlayer.timer = 18;
				}
			}

			meminfo = mallinfo();
			memusage = meminfo.uordblks;
			if (memusage > 0xE800000) // Correct gap between MEM1 and MEM2
				memusage -= 0xE800000;

			size_t flsize = theWorld->getLiquidsSize();
			size_t flcapacity = theWorld->getLiquidsCapacity();

			// Draw debugging elements
			if (showDebug) {
				// clang-format off
				Render::drawText(10,  25, tex_font, "FPS: %u", FPS);
				Render::drawText(10,  40, tex_font, "PX:% 7.2f", thePlayer.posX);
				Render::drawText(10,  55, tex_font, "PY:% 7.2f", thePlayer.posY);
				Render::drawText(10,  70, tex_font, "PZ:% 7.2f", thePlayer.posZ);
				Render::drawText(10,  85, tex_font, "LX:% 7.2f", thePlayer.lookX);
				Render::drawText(10, 100, tex_font, "LY:% 7.2f", thePlayer.lookY);
				Render::drawText(10, 115, tex_font, "LZ:% 7.2f", thePlayer.lookZ);
				Render::drawText(10, 130, tex_font, "DLSize: %u/%u (%u%%)", dluse, dlsize, dluse * 100 / dlsize);
				Render::drawText(10, 145, tex_font, "MemUsage: %u (%.1fMiB)", memusage, memusage / 1024.0 / 1024.0);
				Render::drawText(10, 160, tex_font, "AFB: %u/%u (%u%%)", flsize, flcapacity, flsize * 100 / flcapacity);
				Render::drawText(406, 25, tex_font, "Seed: %08X", seed);
				// clang-format on
			}

			if (WPAD_ButtonsDown(WPAD_CHAN_0) & WPAD_BUTTON_2) {
				if (Netcat::init) {
					GRRLIB_Screen2Texture(0, 0, tex_tmpscreen, false); // This is giving me the last content?
					GRRLIB_Screen2Texture(0, 0, tex_tmpscreen, false);
					Netcat::log("-- START SCREENSHOT --\n");
					Netcat::logf("W: %d, H: %d\n", tex_tmpscreen->w, tex_tmpscreen->h);
					scr_scanY = 0;
					status = SCREENSHOT;
				} else {
					GRRLIB_ScrShot("GXCraft.png");
				}
			}

			GRRLIB_Render();
			FPS = CalculateFrameRate();
			break;
		}
		case NUNCHUK: {
			WPAD_ScanPads();

			data = WPAD_Data(WPAD_CHAN_0);
			if (data->exp.type == WPAD_EXP_NUNCHUK)
				status = INGAME;

			GRRLIB_3dMode(0.1f, 1000.0f, 60.0f, true, false);

			if (theWorld->getBlock(floorf(thePlayer.posX), floorf(thePlayer.posY + 1.625f), floorf(thePlayer.posZ)) == 8) {
				GXColor c = {0x05, 0x05, 0x33};
				GX_SetFog(GX_FOG_LIN, 0.0, 32.0f, 0.1f, 1000.0f, c);
			} else {
				GXColor c = {0x9E, 0xCE, 0xFF};
				GX_SetFog(GX_FOG_LIN, static_cast<float>(renderDistance - 32), static_cast<float>(renderDistance - chunkSize), 0.1f, 1000.0f, c);
			}

			GRRLIB_ObjectViewBegin();
			GRRLIB_ObjectViewTrans(-thePlayer.posX, -thePlayer.posY - 1.625f, -thePlayer.posZ);
			GRRLIB_ObjectViewRotate(0, thePlayer.lookX, 0);
			GRRLIB_ObjectViewRotate(thePlayer.lookY, 0, 0);
			GRRLIB_ObjectViewEnd();

			// GRRLIB clears the vertex formats on mode switch
			GX_SetVtxAttrFmt(GX_VTXFMT1, GX_VA_POS, GX_POS_XYZ, GX_S16, 0);
			GX_SetVtxAttrFmt(GX_VTXFMT1, GX_VA_CLR0, GX_CLR_RGB, GX_RGBA4, 0);
			GX_SetVtxAttrFmt(GX_VTXFMT1, GX_VA_TEX0, GX_TEX_ST, GX_U8, 0);

			GRRLIB_SetTexture(tex_terrain, false);
			Chunked::render();

			// Complain to user
			GRRLIB_2dMode();
			GRRLIB_Rectangle(0, 0, 640, 480, 0x0000007F, true);
			if (data->data_present == 0)
				Render::drawText(144, 232, tex_font, "PLEASE CONNECT WIIMOTE");
			else
				Render::drawText(144, 232, tex_font, "PLEASE CONNECT NUNCHUK");
			GRRLIB_Render();
			break;
		}
		case SCREENSHOT: {
			WPAD_ScanPads();

			if (WPAD_ButtonsDown(WPAD_CHAN_0) & WPAD_BUTTON_2) { // Cancel screenshot
				Netcat::log("-- ABORT SCREENSHOT --\n");
				status = INGAME;
				break;
			}

			Netcat::logf("%03d: ", scr_scanY);
			char c85[5];
			memset(c85, 0, sizeof c85);
			u32 lastcol = 0x00000000;
			int times = 1;
			unsigned int x;
			for (x = 0; x < tex_tmpscreen->w; x++) {
				u32 pixel = GRRLIB_GetPixelFromtexImg(x, scr_scanY, tex_tmpscreen);
				if (lastcol == 0x00000000) {
					lastcol = pixel;
					times = 1;
				} else if (pixel != lastcol) {
					color85(c85, lastcol);
					Netcat::log(c85);
					if (times > 1) {
						Netcat::logf("~%03d", times);
					}
					lastcol = pixel;
					times = 1;
				} else {
					times++;
				}
			}
			// Dump whats left
			color85(c85, lastcol);
			Netcat::log(c85);
			if (times > 1) {
				Netcat::logf("~%03d", times);
			}
			Netcat::log("\n");
			scr_scanY++;
			if (scr_scanY >= tex_tmpscreen->h) {
				status = INGAME;
				Netcat::log("-- END SCREENSHOT --\n");
			}

			// Draw fake scanner
			GRRLIB_2dMode();
			GRRLIB_DrawImg(0, 0, tex_tmpscreen, 0, 1, 1, 0xFFFFFFFF);
			GRRLIB_Line(0, scr_scanY, tex_tmpscreen->w, scr_scanY, 0xFF0000FF);
			GRRLIB_Render();
			break;
		}
		} // switch (status)
		lastTime = thisTime;
	}
	Netcat::log("ending...\n");
	Netcat::close();
	Chunked::deallocall();

	delete theWorld;

	GRRLIB_FreeTexture(tex_font);
	GRRLIB_FreeTexture(tex_inventory);
	GRRLIB_FreeTexture(tex_inv_select);
	GRRLIB_FreeTexture(tex_terrain);
	GRRLIB_FreeTexture(tex_cursor);
	GRRLIB_FreeTexture(tex_tmpscreen);

	GRRLIB_Exit();

	if (shutdown)
		SYS_ResetSystem(SYS_POWEROFF, 0, 0);

	return 0;
}

static void color85(char *buf, u32 color) {
	color >>= 8;
	int cnt;
	for (cnt = 3; cnt >= 0; cnt--) {
		u8 val = color % 85;
		color /= 85;
		buf[cnt] = val + 33;
	}
}

static void initializeBlocks() {
	bedrock_init();
	brick_init();
	cloth_aqua_init();
	cloth_black_init();
	cloth_blue_init();
	cloth_cyan_init();
	cloth_gray_init();
	cloth_green_init();
	cloth_indigo_init();
	cloth_lime_init();
	cloth_magenta_init();
	cloth_orange_init();
	cloth_pink_init();
	cloth_purple_init();
	cloth_red_init();
	cloth_violet_init();
	cloth_white_init();
	cloth_yellow_init();
	cobble_init();
	dirt_init();
	flower_init();
	glass_init();
	gold_init();
	grass_init();
	gravel_init();
	iron_init();
	lava_init();
	leaves_init();
	log_init();
	mossy_init();
	mushroom_init();
	obsidian_init();
	ore_coal_init();
	ore_gold_init();
	ore_iron_init();
	redshroom_init();
	rose_init();
	sand_init();
	sapling_init();
	shelf_init();
	slab_init();
	sponge_init();
	stone_init();
	tnt_init();
	water_init();
	wood_init();
}

static u8 CalculateFrameRate() {
	static u8 frameCount = 0;
	static u32 lastTime;
	static u8 FPS = 60;
	u32 currentTime = ticks_to_millisecs(gettime());

	frameCount++;
	if (currentTime - lastTime > 1000) {
		lastTime = currentTime;
		FPS = frameCount;
		frameCount = 0;
	}
	return FPS;
}
