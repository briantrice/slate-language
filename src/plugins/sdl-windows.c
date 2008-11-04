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
slate_int_t eventValid = 0;

EXPORT void init()
{
    SDL_Init(SDL_INIT_VIDEO);
}

EXPORT void shutdown()
{
    SDL_Quit();
}

EXPORT SDL_Surface *createWindow(slate_int_t width, slate_int_t height)
{
    return surface = SDL_SetVideoMode(width, height, 32, SDL_HWSURFACE | SDL_RESIZABLE);
}

EXPORT slate_int_t haveEvent()
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
EXPORT slate_int_t waitForEvent()
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

EXPORT slate_int_t getEventType()
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

EXPORT slate_int_t getEventMouseMotionX()
{
    return event.motion.x;
}

EXPORT slate_int_t getEventMouseMotionY()
{
    return event.motion.y;
}

EXPORT slate_int_t getEventMouseButtonX()
{
    return event.button.x;
}

EXPORT slate_int_t getEventMouseButtonY()
{
    return event.button.y;
}

EXPORT slate_int_t getKeyboardKey()
{
    return event.key.keysym.sym;
}

EXPORT slate_int_t getKeyboardMod()
{
    return event.key.keysym.mod;
}


EXPORT void blit(
    SDL_Surface *dest, slate_int_t destX, slate_int_t destY, slate_int_t destWidth, slate_int_t destHeight,
    void *src, slate_int_t srcX, slate_int_t srcY, slate_int_t srcWidth, slate_int_t srcHeight, slate_int_t srcStride)
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
