#ifndef __SLATE_SDL_H__
#define __SLATE_SDL_H__

#include <SDL/SDL.h>
#include <SDL/SDL_types.h>
#include <SDL/SDL_gfxPrimitives.h>

struct ByteArray;

/** Initialize this module, should this do anything ? */
extern void initSDLModule();

/** Initialize SDL. */
extern int sdlInit();

/** Quit SDL. */
extern void sdlQuit();

/** Set video mode. */
extern int sdlSetVideoMode(int width, int height, int bpp);

/** Updates the screen. It is needed to run after any moidification. */
extern void sdlUpdateScreen();

/** Draw a pixel. */
extern void sdlDrawPixel(Sint16 x, Sint16 y, Uint8 r, Uint8 g, Uint8 b, Uint8 a);

/** Draw a line. */
extern void sdlDrawLine(Sint16 x1, Sint16 y1, Sint16 x2, Sint16 y2, Uint8 r, Uint8 g, Uint8 b, Uint8 a, int aa);

/** Draw a rectangle. */
extern void sdlDrawRectangle(Sint16 x0, Sint16 y0, Sint16 x1, Sint16 y1, Uint8 r, Uint8 g, Uint8 b, Uint8 a);

/** Draw a filled rectangle, or box. */
extern void sdlDrawFilledRectangle(Sint16 x0, Sint16 y0, Sint16 x1, Sint16 y1, Uint8 r, Uint8 g, Uint8 b, Uint8 a);

/** Draw a circle. */
extern void sdlDrawCircle(Sint16 x, Sint16 y, Sint16 rad, Uint8 r, Uint8 g, Uint8 b, Uint8 a, int aa);

/** Draw a filled circle, or disk. */
extern void sdlDrawFilledCircle(Sint16 x, Sint16 y, Sint16 rad, Uint8 r, Uint8 g, Uint8 b, Uint8 a);

/** Draw an ellipse. */
extern void sdlDrawEllipse(Sint16 x, Sint16 y, Sint16 rx, Sint16 ry, Uint8 r, Uint8 g, Uint8 b, Uint8 a, int aa);

/** Draw a filled epplipse. */
extern void sdlDrawFilledEllipse(Sint16 x, Sint16 y, Sint16 rx, Sint16 ry, Uint8 r, Uint8 g, Uint8 b, Uint8 a);

/** Draw a pie, don't eat it. */
extern void sdlDrawPie(Sint16 x, Sint16 y, Sint16 rad, Sint16 start, Sint16 end, Uint8 r, Uint8 g, Uint8 b, Uint8 a);

/** Draw a filled pie, don't eat it. */
extern void sdlDrawFilledPie(Sint16 x, Sint16 y, Sint16 rad, Sint16 start, Sint16 end, Uint8 r, Uint8 g, Uint8 b, Uint8 a);

/** Draw a trigon. */
//extern void sdlDrawTrigon(Sint16 x0, Sint16 y0, Sint16 x1, Sint16 y1, Sint16 x2, Sint16 y2, Uint8 r, Uint8 g, Uint8 b, Uint8 a, int aa)

#endif /* __SLATE_SDL_H__ */


