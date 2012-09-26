#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <stdarg.h>
#include <ncurses.h>

#include "slatevm.h"
#include "image.h"
#include "ncurses-console.h"

extern int setupterm(char* term, int fildes, int* errret);

// this is a copy paste from boot.c
void
error (const char * message, ...)
{
        va_list args;

        va_start (args, message);
        fprintf (stderr, "Slate: ");
    vfprintf (stderr, message, args);
        fputc ('\n', stderr);
        va_end (args);
    exit (EXIT_FAILURE);
}

int
sc_isAvailable ()
{
  int err;

  if (setupterm((char *)0, 1, &err) == ERR)
    return FALSE;

  return tigetstr("cup") != NULL;
}

int
sc_enterStructuredMode ()
{
  if (initscr() == NULL)
    return FALSE;

  start_color();

  scrollok(stdscr, TRUE);
  immedok(stdscr, FALSE);
  leaveok(stdscr, FALSE);
  idlok(stdscr, TRUE);
  idcok(stdscr, TRUE);
  intrflush(stdscr, FALSE);
  keypad(stdscr, FALSE);

  notimeout(stdscr, FALSE);

  raw();
  noecho();
  nl();

  return TRUE;
}

int
sc_leaveStructuredMode ()
{
  return (endwin() != ERR);
}

// TODO: this assumes string = byte array vm setup
char *
sc_keySequenceOf(char *keyName)
{
  char *keySequence = tigetstr(keyName);
  if ((int)keySequence == -1 || (int)keySequence == 0)
    return FALSE;

  if (strlen(keySequence) > 1 && keySequence[0] == 27)
    keySequence += 1;

  return keySequence;
}

int
sc_columns ()
{
  int cols, lines;
  getmaxyx(stdscr, lines, cols);

  return cols;
}

int
sc_rows ()
{
  int cols, rows;
  getmaxyx(stdscr, rows, cols);
  
  return rows;
}

int
sc_write (void *bytes, int size, int offset)
{
  return (waddnstr(stdscr, ((const char *)bytes) + offset, size) != ERR);
}
  
int
sc_scroll (int lines)
{
  return (wscrl(stdscr, lines) != ERR);
}

int
sc_clear ()
{
  return (wclear(stdscr) != ERR);
}

int
sc_clearToEOS ()
{
  return (wclrtobot(stdscr) != ERR);
}

int
sc_clearToEOL()
{
  return (wclrtoeol(stdscr) != ERR);
}

int
sc_moveToXY (int x, int y)
{
  return (wmove(stdscr, y, x) != ERR);
}

int
sc_cursorX ()
{
  int x, y;
  getyx(stdscr, y, x);
  return x;
}

int
sc_cursorY ()
{
  int x, y;
  getyx(stdscr, y, x);
  return y;
}

int
sc_nextEvent (int blockingMillisecs)
{
  int ch;

  wtimeout(stdscr, blockingMillisecs);
  ch = wgetch(stdscr);
  if (ch == ERR)
  {
    if (blockingMillisecs < 0)
      error("Error reading console?!");
    else
      return -1;
  }
  return ch;
}

int
sc_hasEvent ()
{
  int ch;

  wtimeout(stdscr, 0);
  ch = wgetch(stdscr);
  if (ch == ERR)
  {
    return FALSE;
  }
  else
  {
    ungetch(ch);
    return TRUE;
  }
}

int
sc_flush()
{
  return (wrefresh(stdscr) != ERR);
}

int
sc_deleteChar()
{
  return (wdelch(stdscr) != ERR);
}

int
sc_deleteLines(int lines)
{
  return (winsdelln(stdscr, -lines) != ERR);
}

int
sc_insertLines(int lines)
{
  return (winsdelln(stdscr, lines) != ERR);
}

int
sc_hideCursor()
{
  return curs_set(0);
}

int
sc_showCursor()
{
  return curs_set(1);
}

int
sc_initColorPair(int pair, int fg, int bg)
{
  return (init_pair(pair, fg, bg) != ERR);
}

int
sc_maxColorPairs()
{
  return COLOR_PAIRS;
}

void
sc_setAttributes(int mode, int pair)
{
  wattrset(stdscr, (mode << 16) | COLOR_PAIR(pair));
}

/*
void
sc_setColor(int foreground, int background)
{
  short color;
  init_pair(color, foreground, background);

  wcolor_set(stdscr, color, NULL);
}

void
sc_turnOnAttribute(int attribute)
{
  wattron(stdscr, attribute);
}

void
sc_turnOffAttribute(int attribute)
{
  wattr_off(stdscr, attribute);
}
*/

