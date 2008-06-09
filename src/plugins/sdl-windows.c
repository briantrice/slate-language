// SDL interface that is compatible with Windows.slate

#include <SDL.h>
typedef SDL_Surface Window;
#include "slate-windows.h"

#ifdef WIN32
#  define EXPORT __declspec(dllexport)
#else
#  define EXPORT
#endif

SDL_Surface *surface; // Ick! SDL only supports 1 window
SDL_Event event;
int eventValid = 0;

EXPORT void init()
{
    SDL_Init(SDL_INIT_VIDEO);
}

EXPORT void shutdown()
{
    SDL_Quit();
}

EXPORT SDL_Surface *createWindow(int width, int height)
{
    return surface = SDL_SetVideoMode(width, height, 32, SDL_HWSURFACE | SDL_RESIZABLE);
}

EXPORT int haveEvent()
{
    if(eventValid)
        return 1;
    else if(SDL_PollEvent(&event))
    {
        eventValid = 1;
        return 1;
    }
    else
        return 0;
}

// Returns 1 if there is a message. Returns 0 if error
EXPORT int waitForEvent()
{
    if(haveEvent())
        return 1;
    else if(SDL_WaitEvent(&event))
    {
        eventValid = 1;
        return 1;
    }
    else
        return 0;
}

EXPORT void popEvent()
{
    eventValid = 0;
}

EXPORT int getEventType()
{
    switch(event.type)
    {
        case SDL_KEYDOWN:
            return keyDownMessageType;

        case SDL_KEYUP:
            return keyUpMessageType;

        case SDL_MOUSEMOTION:
            return mouseMoveMessageType;

        case SDL_MOUSEBUTTONDOWN:
            switch(event.button.button)
            {
                case SDL_BUTTON_LEFT: 
                    return leftDownMessageType;
                case SDL_BUTTON_MIDDLE: 
                    return middleDownMessageType;
                case SDL_BUTTON_RIGHT: 
                    return rightDownMessageType;
                default: 
                    return 0;
            }

        case SDL_MOUSEBUTTONUP:
            switch(event.button.button)
            {
                case SDL_BUTTON_LEFT: 
                    return leftUpMessageType;
                case SDL_BUTTON_MIDDLE: 
                    return middleUpMessageType;
                case SDL_BUTTON_RIGHT: 
                    return rightUpMessageType;
                default: 
                    return 0;
            }

        case SDL_QUIT:
            return closeWindowMessageType;

        case SDL_VIDEORESIZE:
            return positionWindowMessageType;

        case SDL_VIDEOEXPOSE:
            return repaintWindowMessageType;

        default:
            return 0;
    } // switch(event.type)
}

EXPORT SDL_Surface *getEventWindow()
{
    return surface; // Ick! SDL only supports 1 window
}

EXPORT int getEventMouseMotionX()
{
    return event.motion.x;
}

EXPORT int getEventMouseMotionY()
{
    return event.motion.y;
}

EXPORT int getEventMouseButtonX()
{
    return event.button.x;
}

EXPORT int getEventMouseButtonY()
{
    return event.button.y;
}

EXPORT int getKeyboardKey()
{
    return event.key.keysym.sym;
}

EXPORT int getKeyboardMod()
{
    return event.key.keysym.mod;
}


EXPORT void blit(
    SDL_Surface *dest, int destX, int destY, int destWidth, int destHeight,
    void *src, int srcX, int srcY, int srcWidth, int srcHeight, int srcStride)
{
    SDL_Rect destRect = {destX, destY, destWidth, destHeight};
    SDL_Rect srcRect = {srcX, srcY, srcWidth, srcHeight};
    SDL_Surface *srcSurf = SDL_CreateRGBSurfaceFrom(
        src, srcWidth, srcHeight, 32, srcStride, 0x00FF0000, 0x0000FF00, 0x000000FF, 0xFF000000);
    if(!srcSurf)
        return;

    SDL_SetAlpha(dest, 0, 0);
    SDL_SetAlpha(srcSurf, 0, 0);
    SDL_BlitSurface(srcSurf, &srcRect, dest, &destRect);
    SDL_UpdateRect(dest, destX, destY, destWidth, destHeight);
    SDL_FreeSurface(srcSurf);
}
