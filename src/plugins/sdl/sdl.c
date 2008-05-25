#include <stdio.h>

#include "slatevm.h"
#include "image.h"
#include "sdl.h"

SDL_Surface *sdlScreen;

void initSDLModule(){
	sdlScreen = NULL;
}

int sdlInit(){
	if((SDL_Init(SDL_INIT_VIDEO)==-1)) {
		printf("Could not initialize SDL: %s.\n", SDL_GetError());
		return -1;
	}
	return 0;
}

void sdlQuit(){
	SDL_Quit();
}

int sdlSetVideoMode(int width, int height, int bpp){
	sdlScreen = SDL_SetVideoMode(width, height, bpp, SDL_SWSURFACE);
	if(sdlScreen == NULL){
		fprintf(stderr, "Couldn't set %dx%dx%d video mode: %s\n", width, height, bpp, SDL_GetError());
		return -1;
	}
	return 0;
}

void sdlUpdateScreen(){
	SDL_UpdateRect(sdlScreen, 0, 0, 0, 0);
}

void sdlDrawPixel(Sint16 x, Sint16 y, Uint8 r, Uint8 g, Uint8 b, Uint8 a){
	pixelRGBA(sdlScreen, x, y, r, g, b, a);
}

void sdlDrawLine(Sint16 x0, Sint16 y0, Sint16 x1, Sint16 y1, Uint8 r, Uint8 g, Uint8 b, Uint8 a, int aa){
	if(aa){
			aalineRGBA(sdlScreen, x0, y0, x1, y1, r, g, b, a);
	} else {
			lineRGBA(sdlScreen, x0, y0, x1, y1, r, g, b, a);
	}
}

void sdlDrawRectangle(Sint16 x0, Sint16 y0, Sint16 x1, Sint16 y1, Uint8 r, Uint8 g, Uint8 b, Uint8 a){
	
	rectangleRGBA(sdlScreen, x0, y0, x1, y1, r, g, b, a);
}

void sdlDrawFilledRectangle(Sint16 x0, Sint16 y0, Sint16 x1, Sint16 y1, Uint8 r, Uint8 g, Uint8 b, Uint8 a){	
	boxRGBA(sdlScreen, x0, y0, x1, y1, r, g, b, a);
}

void sdlDrawCircle(Sint16 x, Sint16 y, Sint16 rad, Uint8 r, Uint8 g, Uint8 b, Uint8 a, int aa){
	if(aa){
			aacircleRGBA(sdlScreen, x, y, rad, r, g, b, a);
	} else {
			circleRGBA(sdlScreen, x, y, rad, r, g, b, a);
	}
}

void sdlDrawFilledCircle(Sint16 x, Sint16 y, Sint16 rad, Uint8 r, Uint8 g, Uint8 b, Uint8 a){
	filledCircleRGBA(sdlScreen, x, y, rad, r, g, b, a);
}

void sdlDrawEllipse(Sint16 x, Sint16 y, Sint16 rx, Sint16 ry, Uint8 r, Uint8 g, Uint8 b, Uint8 a, int aa){
	if(aa){
			aaellipseRGBA(sdlScreen, x, y, rx, ry, r, g, b, a);
	} else {
			ellipseRGBA(sdlScreen, x, y, rx, ry, r, g, b, a);
	}
}

void sdlDrawFilledEllipse(Sint16 x, Sint16 y, Sint16 rx, Sint16 ry, Uint8 r, Uint8 g, Uint8 b, Uint8 a){
	filledEllipseRGBA(sdlScreen, x, y, rx, ry, r, g, b, a);
}

void sdlDrawPie(Sint16 x, Sint16 y, Sint16 rad, Sint16 start, Sint16 end, Uint8 r, Uint8 g, Uint8 b, Uint8 a){
	pieRGBA(sdlScreen, x, y, rad, start, end, r, g, b, a);
}

void sdlDrawFilledPie(Sint16 x, Sint16 y, Sint16 rad, Sint16 start, Sint16 end, Uint8 r, Uint8 g, Uint8 b, Uint8 a){
	filledPieRGBA(sdlScreen, x, y, rad, start, end, r, g, b, a);
}

void sdlDrawTrigon(Sint16 x0, Sint16 y0, Sint16 x1, Sint16 y1, Sint16 x2, Sint16 y2, Uint8 r, Uint8 g, Uint8 b, Uint8 a, int aa){
	if(aa){
		trigonRGBA(sdlScreen, x0, y0, x1, y1, x2, y2, r, g, b, a);
	} else {
		aatrigonRGBA(sdlScreen, x0, y0, x1, y1, x2, y2, r, g, b, a);
	}
}
