#include <stdlib.h>
#include <stdio.h>
#include <SDL/SDL.h>
#include <SDL/SDL_gfxPrimitives.h>

static int initialized = 0;
static SDL_Surface * screen = NULL;

int
SDL_main(int argc, char ** argv)
{
    fprintf(stderr, "Dummy _SDL_main called.\n");

    return EXIT_FAILURE;
}

int
SDLPortRestart(unsigned int w, unsigned int h, unsigned int bpp, unsigned int fullScreen, unsigned int doubleBuffer)
{
    if(!initialized)
    {
        if(SDL_Init(SDL_INIT_VIDEO) < 0)
        {
            fprintf(stderr, "Failed to initialize SDL\n");
            return -1;
        }
        
        atexit(SDL_Quit);
        initialized = 1;
    }

    unsigned int flags = SDL_RESIZABLE | SDL_HWSURFACE;
    if(fullScreen)
        flags |= SDL_FULLSCREEN;
    if(doubleBuffer)
        flags |= SDL_DOUBLEBUF;

    SDL_Surface * newScreen = SDL_SetVideoMode(w, h, bpp, flags);
    if(newScreen == NULL)
    {
        fprintf(stderr, "Failed to set video mode: w=%d, h=%d, bpp=%d, full screen=%d, double buffer = %d\n", w, h, bpp, fullScreen, doubleBuffer);
        return -1;
    }

    if(screen == NULL)
        SDL_WM_SetCaption("Slate", NULL);

    screen = newScreen;
    return 0;
}

void
SDLPortDestroy(void)
{
}

enum
{
    SDLPORT_EVENT_QUIT               = 0,
    SDLPORT_EVENT_WINDOW_CONFIG      = 1,
    SDLPORT_EVENT_WINDOW_REPAINT     = 2,
    SDLPORT_EVENT_KEY_DOWN           = 3,
    SDLPORT_EVENT_KEY_UP             = 4,
    SDLPORT_EVENT_MOUSE_MOTION       = 5,
    SDLPORT_EVENT_MOUSE_BUTTON_DOWN  = 6,
    SDLPORT_EVENT_MOUSE_BUTTON_UP    = 7,
    SDLPORT_EVENT_MOUSE_ENTER        = 8,
    SDLPORT_EVENT_MOUSE_LEAVE        = 9
};

typedef struct
{
    unsigned char type;
    int param1, param2;
} SDLPortEvent;

static unsigned int
dispatchEvent(SDLPortEvent * eventData, SDL_Event * event)
{
    switch(event->type)
    {
    case SDL_QUIT:
        eventData->type = SDLPORT_EVENT_QUIT;
        break;

    case SDL_VIDEORESIZE:
        eventData->type = SDLPORT_EVENT_WINDOW_CONFIG;
        eventData->param1 = event->resize.w;
        eventData->param2 = event->resize.h;
        break;

    case SDL_VIDEOEXPOSE:
        eventData->type = SDLPORT_EVENT_WINDOW_REPAINT;
        break;

    case SDL_KEYDOWN:
    case SDL_KEYUP:
        eventData->type = (event->type == SDL_KEYDOWN ? SDLPORT_EVENT_KEY_DOWN : SDLPORT_EVENT_KEY_UP);
        eventData->param1 = event->key.keysym.sym;
        eventData->param2 = event->key.keysym.mod;
        break;

    case SDL_MOUSEMOTION:
        eventData->type = SDLPORT_EVENT_MOUSE_MOTION;
        eventData->param1 = event->motion.x;
        eventData->param2 = event->motion.y;
        break;

    case SDL_MOUSEBUTTONDOWN:
    case SDL_MOUSEBUTTONUP:
        eventData->type = (event->type == SDL_MOUSEBUTTONDOWN ? SDLPORT_EVENT_MOUSE_BUTTON_DOWN : SDLPORT_EVENT_MOUSE_BUTTON_UP);
        eventData->param1 = event->button.button;
        break;

    case SDL_ACTIVEEVENT:
        if(event->active.state & SDL_APPMOUSEFOCUS)
        {
            eventData->type = event->active.gain ? SDLPORT_EVENT_MOUSE_ENTER : SDLPORT_EVENT_MOUSE_LEAVE;
            break;
        }
        return 0;

    default:
        return 0;
    }

    return 1;
}

unsigned int
SDLPortPollEvent(SDLPortEvent * eventData)
{
    SDL_Event event;
    
    if(!initialized)
        return 0;
    
    while(SDL_PollEvent(& event))
    {
        if(dispatchEvent(eventData, & event))
            return 1;
    }

    return 0;
}

unsigned int
SDLPortWaitEvent(SDLPortEvent * eventData)
{
    SDL_Event event;
    
    if(!initialized)
        return 0;

    while(SDL_WaitEvent(& event))
    {
        if(dispatchEvent(eventData, & event))
            return 1;
    }

    return 0;
}

void
SDLPortClip(unsigned int x, unsigned int y, unsigned int w, unsigned int h)
{
    SDL_Rect clip = {x, y, w, h};
    
    SDL_SetClipRect(screen, (x | y | w | h) == 0 ? NULL : & clip);
}

void
SDLPortUpdate(unsigned int x, unsigned int y, unsigned int w, unsigned int h)
{
    SDL_UpdateRect(screen, x, y, w, h);
}

void
SDLPortFlip(void)
{   
    SDL_Flip(screen);
}
    
void
SDLPortDrawPoint(unsigned int x, unsigned int y, unsigned int r, unsigned int g, unsigned int b)
{
    pixelRGBA(screen, x, y, r, g, b, 0xFF);
}

void
SDLPortDrawRectangle(unsigned int x1, unsigned int y1, unsigned int x2, unsigned int y2, unsigned int r, unsigned int g, unsigned int b)
{
    rectangleRGBA(screen, x1, y1, x2, y2, r, g, b, 0xFF);
}

void
SDLPortDrawFilledRectangle(unsigned int x1, unsigned int y1, unsigned int x2, unsigned int y2, unsigned int r, unsigned int g, unsigned int b)
{
    boxRGBA(screen, x1, y1, x2, y2, r, g, b, 0xFF);
}

void
SDLPortDrawLine(unsigned int x1, unsigned int y1, unsigned int x2, unsigned int y2, unsigned int r, unsigned int g, unsigned int b)
{
    aalineRGBA(screen, x1, y1, x2, y2, r, g, b, 0xFF);
}

void
SDLPortDrawCircle(unsigned int x, unsigned int y, unsigned int rad, unsigned int r, unsigned int g, unsigned int b)
{
    aacircleRGBA(screen, x, y, rad, r, g, b, 0xFF);
}

void
SDLPortDrawFilledCircle(unsigned int x, unsigned int y, unsigned int rad, unsigned int r, unsigned int g, unsigned int b)
{
    filledCircleRGBA(screen, x, y, rad, r, g, b, 0xFF);
}

void
SDLPortDrawEllipse(unsigned int x, unsigned int y, unsigned int rx, unsigned int ry, unsigned int r, unsigned int g, unsigned int b)
{
    aaellipseRGBA(screen, x, y, rx, ry, r, g, b, 0xFF);
}
    
void
SDLPortDrawFilledEllipse(unsigned int x, unsigned int y, unsigned int rx, unsigned int ry, unsigned int r, unsigned int g, unsigned int b)
{
    filledEllipseRGBA(screen, x, y, rx, ry, r, g, b, 0xFF);
}

void
SDLPortDrawFilledTriangle(unsigned int x1, unsigned int y1, unsigned int x2, unsigned int y2, unsigned int x3, unsigned int y3, unsigned int r, unsigned int g, unsigned int b)
{
    filledTrigonRGBA(screen, x1, y1, x2, y2, x3, y3, r, g, b, 0xFF);
}
     
