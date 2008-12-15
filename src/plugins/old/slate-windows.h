// Definitions common to multiple window backends.
// Windows.slate can use any plugin that conforms to this
// and is named "slate-windows".

#ifndef EXPORT
#  ifdef WIN32
#    define EXPORT __declspec(dllexport)
#  else
#    define EXPORT
#  endif
#endif

#include <stdint.h>
typedef  intptr_t slate_int_t;

enum MessageType
{
    repaintWindowMessageType        = 0,
    positionWindowMessageType       = 1,
    closeWindowMessageType          = 2,
    quitMessageType                 = 3,

    leftDownMessageType             = 100,
    leftUpMessageType               = 101,
    leftDoubleClickMessageType      = 102,

    rightDownMessageType            = 110,
    rightUpMessageType              = 111,
    rightDoubleClickMessageType     = 112,

    middleDownMessageType           = 120,
    middleUpMessageType             = 121,
    middleDoubleClickMessageType    = 122,

    x1DownMessageType               = 130,
    x1UpMessageType                 = 131,
    x1DoubleClickMessageType        = 132,

    x2DownMessageType               = 140,
    x2UpMessageType                 = 141,
    x2DoubleClickMessageType        = 142,

    mouseMoveMessageType            = 150,
    mouseEnterMessageType           = 151,
    mouseLeaveMessageType           = 152,

    keyDownMessageType              = 160,
    keyUpMessageType                = 161,
    charMessageType                 = 162,
};

enum ButtonStates
{
    leftShiftKeyDown =      0x00000001,
    rightShiftKeyDown =     0x00000002,
    leftCtrlKeyDown =       0x00000004,
    rightCtrlKeyDown =      0x00000008,
    leftAltKeyDown =        0x00000010,
    rightAltKeyDown =       0x00000020,
    /*fixme.. i changed this because ISO C restricts enum to int values*/
    leftButtonDown =        0x01000000,
    middleButtonDown =      0x02000000,
    rightButtonDown =       0x04000000,
    x1ButtonDown =          0x08000000,
    x2ButtonDown =          0x09000000,
};

EXPORT void init();
EXPORT void shutdown();
EXPORT Window *createWindow(slate_int_t width, slate_int_t height);
EXPORT slate_int_t haveEvent();
EXPORT slate_int_t waitForEvent();
EXPORT void popEvent();
EXPORT slate_int_t getEventType();
EXPORT Window *getEventWindow();
EXPORT slate_int_t getEventMouseMotionX();
EXPORT slate_int_t getEventMouseMotionY();
EXPORT slate_int_t getEventMouseButtonX();
EXPORT slate_int_t getEventMouseButtonY();
EXPORT slate_int_t getKeyboardKey();
EXPORT slate_int_t getKeyboardMod();
EXPORT void blit(
    Window *dest, slate_int_t destX, slate_int_t destY, slate_int_t destWidth, slate_int_t destHeight,
    void *src, slate_int_t srcX, slate_int_t srcY, slate_int_t srcWidth, slate_int_t srcHeight, slate_int_t srcStride);
