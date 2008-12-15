#ifndef __SLATE_NCURSES_CONSOLE_H__
#define __SLATE_NCURSES_CONSOLE_H__

#include "env.h"

extern int sc_isAvailable();

extern int sc_enterStructuredMode();
extern int sc_leaveStructuredMode();

extern char *sc_keySequenceOf(char *keyName);

extern int sc_columns();
extern int sc_rows();

extern int sc_write(void *buffer, int size, int start);
extern int sc_clear();
extern int sc_clearToEOS();
extern int sc_clearToEOL();
extern int sc_scroll(int lines);
extern int sc_moveToXY(int x, int y);
extern int sc_nextEvent(int blockingMillisecs);
extern int sc_hasEvent();
extern int sc_flush();
extern int sc_deleteChar();	// delete char and move the rest of the line one left
extern int sc_deleteLines(int lines);
extern int sc_insertLines(int lines);
extern int sc_hideCursor();
extern int sc_showCursor();

extern int sc_maxColorPairs();
extern int sc_initColorPair(int colorPair, int fg, int bg);
extern void sc_setAttributes(int mode, int colorPair);

#endif /* __SLATE_NCURSES_CONSOLE_H__ */


