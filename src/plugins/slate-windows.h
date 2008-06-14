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
EXPORT Window *createWindow(int width, int height);
EXPORT int haveEvent();
EXPORT int waitForEvent();
EXPORT void popEvent();
EXPORT int getEventType();
EXPORT Window *getEventWindow();
EXPORT int getEventMouseMotionX();
EXPORT int getEventMouseMotionY();
EXPORT int getEventMouseButtonX();
EXPORT int getEventMouseButtonY();
EXPORT int getKeyboardKey();
EXPORT int getKeyboardMod();
EXPORT void blit(
    Window *dest, int destX, int destY, int destWidth, int destHeight,
    void *src, int srcX, int srcY, int srcWidth, int srcHeight, int srcStride);
