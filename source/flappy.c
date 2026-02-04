#include <nds.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#include <flappy32.h>
#include <pipe.h>

static volatile int frame = 0;

static void Vblank() {
	frame++;
}

#define FRAMES_PER_ANIMATION 3
#define FRAME_SPEED 3

typedef struct
{
	int x;
	int y;

	u16* sprite_gfx_mem;
	u8*  frame_gfx;

	int state;
	int anim_frame;
}Sprite;

typedef struct
{
	int x;
	int y;
}XYPair;

enum {SCREEN_TOP = 0, SCREEN_BOTTOM = 192, SCREEN_LEFT = 0, SCREEN_RIGHT = 256};

void initSprite(Sprite *sprite, u8* gfx, SpriteSize sprite_size, int sy)
{
	sprite->sprite_gfx_mem = oamAllocateGfx(&oamMain, sprite_size, SpriteColorFormat_16Color);
	sprite->frame_gfx = (u8*)gfx;
	u8* offset = sprite->frame_gfx + frame * sy*32;
	dmaCopy(offset, sprite->sprite_gfx_mem, sy*32);
}
void animateSprite(Sprite *sprite)
{
	int frame = sprite->anim_frame + sprite->state * FRAMES_PER_ANIMATION;
	u8* offset = sprite->frame_gfx + frame * 16*32;
	dmaCopy(offset, sprite->sprite_gfx_mem, 16*32);
}

int angleToRotation(float angle){
	return (int)(32767.0f/360.0f * angle);
}

void flap(){
	
}

//---------------------------------------------------------------------------------
int main(void) {
	//---------------------------------------------------------------------------------
	int i = 0;
	touchPosition touch;
	Sprite bird = {0,0};
	Sprite pipe = {0,0};

	XYPair pipes[] ={
		{10,0},
		{30,0},
		{50,0},
	};

	int bottomPipeOffset = 132;
	int xSpaceBtPipes = 100;
	int pipeCount = 0;

	pipe.state = 0;
	bird.state = 0;
	pipe.anim_frame = 0;

	int flapStartFrame = 0;

	videoSetMode(MODE_0_2D);
	videoSetModeSub(MODE_0_2D);

	vramSetBankA(VRAM_A_MAIN_SPRITE);
	vramSetBankD(VRAM_D_SUB_SPRITE);

	oamInit(&oamMain, SpriteMapping_1D_128, false);
	oamInit(&oamSub, SpriteMapping_1D_128, false);


	initSprite(&bird, (u8*)flappy32Tiles, SpriteSize_32x32, 16);
	initSprite(&pipe, (u8*)pipeTiles, SpriteSize_32x64, 32);

	dmaCopy(flappy32Pal, SPRITE_PALETTE, 32);
	dmaCopy(pipePal, SPRITE_PALETTE + 16, 32);

	float y_height = SCREEN_BOTTOM - 16;
	float y_speed = 0;
	float GRAVITY = 0.3f;
	float JUMP_FORCE = 5.0f;
	float MAX_Y_SPEED = 8.0f;
	int X_POS = 50;

	int angle = 0;
	int angleVelocity = 0;
	float delta = 1.0f/60.0f;
	int ANGLE_CHANGE = 100;
	int MAX_ANGLE = 20.0;
	int MIN_ANGLE = -90.0;
	bool isDead = false;

	float ANGLE_ACCELERATION = 0.4f;


	int BOTTOM_OFFSET = 16;

	MIN_ANGLE = angleToRotation((float)MIN_ANGLE);
	MAX_ANGLE = angleToRotation((float)MAX_ANGLE);
	ANGLE_ACCELERATION = angleToRotation(ANGLE_ACCELERATION);

	consoleDemoInit();
	irqSet(IRQ_VBLANK, Vblank);

	while(pmMainLoop()) {

		scanKeys();

		int held = keysDown();

		if(held & KEY_TOUCH)
			touchRead(&touch);

		if(held & KEY_START) break;


		y_height -= y_speed;
		y_speed -= GRAVITY;
		if(y_height > SCREEN_BOTTOM - BOTTOM_OFFSET){
			y_speed = 0;
			y_height = SCREEN_BOTTOM - BOTTOM_OFFSET;
			// iprintf("y_height less than 0");
		}


		if(held & KEY_A){
			if(y_height > SCREEN_TOP){
				y_speed = JUMP_FORCE;
				flapStartFrame = frame;
				angleVelocity = angleToRotation(10.0f);
			}
		}

		if(y_speed > MAX_Y_SPEED){
			y_speed = MAX_Y_SPEED;
		}

		if(y_speed > 0){
			if(flapStartFrame != -1){
				if((flapStartFrame - frame) % FRAME_SPEED == 0){
					bird.anim_frame++;
				}
			}
		}
		else{
			bird.anim_frame = 1;
		}
		if(bird.anim_frame >= FRAMES_PER_ANIMATION){
			bird.anim_frame = 0;
		}

		animateSprite(&bird);

		angle += angleVelocity;
		angleVelocity -= ANGLE_ACCELERATION;

		if(angle <= MIN_ANGLE){
			angle = MIN_ANGLE;
		}
		if(angle >= MAX_ANGLE){
			angle = MAX_ANGLE;
		}

		oamRotateScale(&oamMain, 0, angle, (1<<8), (1<<8));
		oamSet(&oamMain, //main graphics engine context
			0,           //oam index (0 to 127)
			X_POS, (int)y_height,   //x and y pixel location of the sprite
			0,                    //priority, lower renders last (on top)
			0,					  //this is the palette index if multiple palettes or the alpha value if bmp sprite
			SpriteSize_32x32,
			SpriteColorFormat_16Color,
			bird.sprite_gfx_mem,                  //pointer to the loaded graphics
			0,                  //sprite rotation data
			false,               //double the size when rotating?
			false,			//hide the sprite?
			false, false, //vflip, hflip
			false	//apply mosaic
			);

		for(int i=0;i<3;i++){
			oamSet(&oamMain, //main graphics engine context
				1 + i * 2,           //oam index (0 to 127)
				pipes[i].x, pipes[i].y,   //x and y pixel location of the sprite
				0,                    //priority, lower renders last (on top)
				1,					  //this is the palette index if multiple palettes or the alpha value if bmp sprite
				SpriteSize_32x64,
				SpriteColorFormat_16Color,
				pipe.sprite_gfx_mem,                  //pointer to the loaded graphics
				-1,                  //sprite rotation data
				false,               //double the size when rotating?
				false,			//hide the sprite?
				false, false, //vflip, hflip
				false	//apply mosaic
				);

			oamSet(&oamMain, //main graphics engine context
				2+ i * 2,           //oam index (0 to 127)
				pipes[i].x, pipes[i].y + bottomPipeOffset,   //x and y pixel location of the sprite
				0,                    //priority, lower renders last (on top)
				1,					  //this is the palette index if multiple palettes or the alpha value if bmp sprite
				SpriteSize_32x64,
				SpriteColorFormat_16Color,
				pipe.sprite_gfx_mem,                  //pointer to the loaded graphics
				-1,                  //sprite rotation data
				false,               //double the size when rotating?
				false,			//hide the sprite?
				false, true, //vflip, hflip
				false	//apply mosaic
				);
		}



		swiWaitForVBlank();

		if(frame % 1 == 0){
			printf(" angle = %d\nvel = %d\nacc = %f", angle, angleVelocity, ANGLE_ACCELERATION);
		}

		oamUpdate(&oamMain);
		oamUpdate(&oamSub);
	}

	return 0;
}
