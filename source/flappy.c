#include <nds.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <math.h>

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
	int width;
	int height;
}CollisionShape;

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

int getPipeHeight(int min, int max){
	int value = rand() % (max - min + 1) + min;
	return value;
}

bool isOverlapping(CollisionShape col1, CollisionShape col2){
	int xmax1 = col1.x + col1.width;
	int xmin1 = col1.x;
	int ymax1 = col1.y + col1.height;
	int ymin1 = col1.y;

	int xmax2 = col2.x + col2.width;
	int xmin2 = col2.x;
	int ymax2 = col2.y + col2.height;
	int ymin2 = col2.y;

	if(xmax1 >= xmin2 && xmax2 >= xmin1){
		if(ymax1 >= ymin2 && ymax2 >= ymin1){
			return true;
		}
	}
	return false;
}

float degreesToRadians(float degrees){
	return degrees * 180.0 / M_PI;
}

XYPair rotatePoint(XYPair xy, XYPair cxy, float angle){
	float rads = degreesToRadians(angle);
	float cosRads = cos(rads);
	float sinRads = sin(rads);

	XYPair rxy = {
		cosRads * (xy.x - cxy.x) - sinRads * (xy.y - cxy.y) + cxy.x,
		sinRads * (xy.x - cxy.x) - cosRads * (xy.y - cxy.y) + cxy.y
	};

	return rxy;
}


//---------------------------------------------------------------------------------
int main(void) {
	//---------------------------------------------------------------------------------
	int i = 0;
	touchPosition touch;
	Sprite bird = {0,0};
	Sprite pipe = {0,0};


	int BOTTOM_PIPE_OFFSET = 48+64;
	int xSpaceBtPipes = 100;
	int PIPE_COUNT = 4;

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

	XYPair PIPE_FRAME_DIMENSIONS = {32, 64};
	int PIPE_FRAME_OFFSET = PIPE_FRAME_DIMENSIONS.x + 55;
	int MIN_PIPE_H = -44;
	int MAX_PIPE_H = 44;
	int frontPipe = 0;

	int ADD_PIPES_IND = 2 + PIPE_COUNT * 2;
	int add_pipes_count = 0;
	int old_add_pipes_count = 0;

	XYPair pipes[] ={
		{SCREEN_RIGHT,getPipeHeight(MIN_PIPE_H, MAX_PIPE_H)},
		{SCREEN_RIGHT + PIPE_FRAME_OFFSET * 1,getPipeHeight(MIN_PIPE_H, MAX_PIPE_H)},
		{SCREEN_RIGHT + PIPE_FRAME_OFFSET * 2,getPipeHeight(MIN_PIPE_H, MAX_PIPE_H)},
		{SCREEN_RIGHT + PIPE_FRAME_OFFSET * 3,getPipeHeight(MIN_PIPE_H, MAX_PIPE_H)},
	};

	CollisionShape cols[] = {
		{0,0,8,12},
		{0,0,32,64},
		{0,0,32,64}
	};

	int X_POS = 50;
	int START_Y_POS = SCREEN_BOTTOM/2;

	float y_height = START_Y_POS;
	float y_speed = 0;
	float GRAVITY = 0.3f/2;
	float JUMP_FORCE = 5.0f/2;
	float MAX_Y_SPEED = 8.0f/2;
	XYPair BIRD_COL_OFFSET = {8,11};
	XYPair PIPE_COL_OFFSET = {3,0};

	int angle = 0;
	int angleVelocity = 0;
	float delta = 1.0f/60.0f;
	int MAX_ANGLE = 20.0;
	int MIN_ANGLE = -90.0;

	bool isDead = false;
	int score = 0;
	int lastScoredPipe = -1;

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


		y_speed -= GRAVITY;
		if(y_speed > MAX_Y_SPEED){
			y_speed = MAX_Y_SPEED;
		}
		if(y_speed < -MAX_Y_SPEED){
			y_speed = -MAX_Y_SPEED;
		}
		y_height -= y_speed;
		if(y_height > SCREEN_BOTTOM - BOTTOM_OFFSET){
			y_height = SCREEN_BOTTOM - BOTTOM_OFFSET;
			isDead = true;
			if(isDead){
				y_height = pipes[frontPipe].y + PIPE_FRAME_DIMENSIONS.y;
				isDead = false;
			}
		}


		if(held & KEY_A & !isDead){
			if(y_height > SCREEN_TOP){
				y_speed = JUMP_FORCE;
				flapStartFrame = frame;
				angleVelocity = angleToRotation(10.0f);
			}
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

		add_pipes_count = 0;
		for(int i=0;i<PIPE_COUNT;i++){
			if(pipes[i].x > SCREEN_LEFT - PIPE_FRAME_DIMENSIONS.x){
				if(pipes[i].x < SCREEN_LEFT){
					frontPipe = i + 1;
					if(frontPipe > PIPE_COUNT - 1){
						frontPipe = 0;
					}
				}

				if(i == frontPipe){
					if(lastScoredPipe != i){
						if(pipes[i].x  < X_POS){
							lastScoredPipe = frontPipe;
							score++;
							printf("score: %d\n", score);
						}
					}
				}

				if(!isDead){
					pipes[i].x -= 1;

				}
			}
			else{
				int j = i + PIPE_COUNT - 1;
				if( j > PIPE_COUNT - 1){
					j -= PIPE_COUNT;
				}

				pipes[i].x = pipes[j].x + PIPE_FRAME_OFFSET;
				pipes[i].y = getPipeHeight(MIN_PIPE_H, MAX_PIPE_H);
			}
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

			if(pipes[i].y > SCREEN_TOP){
				oamSet(&oamMain, //main graphics engine context
					ADD_PIPES_IND + add_pipes_count,           //oam index (0 to 127)
					pipes[i].x, pipes[i].y - PIPE_FRAME_DIMENSIONS.y,   //x and y pixel location of the sprite
					0,                    //priority, lower renders last (on top)
					1,					  //this is the palette index if multiple palettes or the alpha value if bmp sprite
					SpriteSize_32x64,
					SpriteColorFormat_16Color,
					pipe.sprite_gfx_mem,                  //pointer to the loaded graphics
					-1,                  //sprite rotation data
					false,               //double the size when rotating?
					false,			//hide the sprite?
					false, true, //hflip, vflip
					false	//apply mosaic
					);
				add_pipes_count++;
			}

			oamSet(&oamMain, //main graphics engine context
				2+ i * 2,           //oam index (0 to 127)
				pipes[i].x, pipes[i].y + BOTTOM_PIPE_OFFSET,   //x and y pixel location of the sprite
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
			
			if(pipes[i].y + BOTTOM_PIPE_OFFSET + PIPE_FRAME_DIMENSIONS.y < SCREEN_BOTTOM){
				oamSet(&oamMain, //main graphics engine context
					ADD_PIPES_IND + add_pipes_count,           //oam index (0 to 127)
					pipes[i].x, pipes[i].y + BOTTOM_PIPE_OFFSET + PIPE_FRAME_DIMENSIONS.y,   //x and y pixel location of the sprite
					0,                    //priority, lower renders last (on top)
					1,					  //this is the palette index if multiple palettes or the alpha value if bmp sprite
					SpriteSize_32x64,
					SpriteColorFormat_16Color,
					pipe.sprite_gfx_mem,                  //pointer to the loaded graphics
					-1,                  //sprite rotation data
					false,               //double the size when rotating?
					false,			//hide the sprite?
					false, false, //hflip, vflip
					false	//apply mosaic
					);
				add_pipes_count++;
			}
		}

		if(old_add_pipes_count != add_pipes_count){
			if(old_add_pipes_count > add_pipes_count){
				oamClear(
					&oamMain,
					ADD_PIPES_IND + add_pipes_count,
					old_add_pipes_count - add_pipes_count
				);
			}
		}
		old_add_pipes_count = add_pipes_count;
		

		cols[0].x = X_POS + BIRD_COL_OFFSET.x;
		cols[0].y = (int)y_height + BIRD_COL_OFFSET.y;
		cols[1].x = pipes[frontPipe].x + PIPE_COL_OFFSET.x;
		cols[1].y = SCREEN_TOP;
		cols[1].height = pipes[frontPipe].y + PIPE_COL_OFFSET.y + PIPE_FRAME_DIMENSIONS.y;
		cols[2].x = pipes[frontPipe].x + PIPE_COL_OFFSET.x;
		cols[2].y = pipes[frontPipe].y + PIPE_COL_OFFSET.y + BOTTOM_PIPE_OFFSET;
		cols[2].height = SCREEN_BOTTOM - (pipes[frontPipe].y + PIPE_COL_OFFSET.y + BOTTOM_PIPE_OFFSET);

		for(int i =1;i<3;i++){
			if(isOverlapping(cols[0], cols[i])){
				isDead = true;
			}
		}



		swiWaitForVBlank();

		if(frame % 1 == 0){

		}

		oamUpdate(&oamMain);
		oamUpdate(&oamSub);
	}

	return 0;
}
