/***************************************************************************
--          Commodore 64 Vi Editor - shared definitions
--
--           Copyright (C) 2026 By Ulrik Hørlyk Hjort
--
-- Permission is hereby granted, free of charge, to any person obtaining
-- a copy of this software and associated documentation files (the
-- "Software"), to deal in the Software without restriction, including
-- without limitation the rights to use, copy, modify, merge, publish,
-- distribute, sublicense, and/or sell copies of the Software, and to
-- permit persons to whom the Software is furnished to do so, subject to
-- the following conditions:
--
-- The above copyright notice and this permission notice shall be
-- included in all copies or substantial portions of the Software.
--
-- THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
-- EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
-- MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
-- NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE
-- LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION
-- OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION
-- WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
-- ***************************************************************************/
#ifndef EDITOR_H
#define EDITOR_H

#define PROG_VERSION    "0.1"

/* Buffer limits */
#define MAX_LINES       200
#define MAX_LINE_LEN     79   /* max chars per line (not counting null) */

/* Screen layout */
#define SCREEN_COLS      40
#define TEXT_ROWS        23   /* rows 0-22: text content */
#define STATUS_ROW       23   /* row 23: status / command bar */

/* Editor modes */
#define MODE_NORMAL      0
#define MODE_INSERT      1
#define MODE_COMMAND     2

/*
 * PETSCII key codes returned by cgetc() on C64.
 * Unshifted letter keys return PETSCII 65-90, which equals ASCII 'A'-'Z'.
 * So vi commands typed with unshifted keys map to uppercase C literals.
 */
#define KEY_RETURN      13    /* Return / Enter          */
#define KEY_STOP         3    /* RUN/STOP  (= Escape)    */
#define KEY_DEL         20    /* DEL key   (backspace)   */
#define KEY_INS        148    /* Shift+DEL (insert)      */
#define KEY_UP         145    /* Cursor Up               */
#define KEY_DOWN        17    /* Cursor Down             */
#define KEY_LEFT       157    /* Cursor Left             */
#define KEY_RIGHT       29    /* Cursor Right            */
#define KEY_HOME        19    /* HOME                    */
#define KEY_CLRHOME    147    /* Shift+HOME (CLR)        */

/*
 * cc65 maps C char literals to PETSCII for CBM targets:
 *   Lowercase 'a'-'z' (ASCII 97-122) -> PETSCII 65-90  (unshifted keys)
 *   Uppercase 'A'-'Z' (ASCII 65-90)  -> PETSCII 193-218 (shifted keys)
 *
 * So  vi 'h'  = press H unshifted = cgetc() returns 72 = cc65 'h'
 *     vi 'G'  = press G shifted   = cgetc() returns 199 = cc65 'G'
 */

/* F-key codes from c64.h / conio.h */
#define KEY_F1         133    /* F1 key  (use as extra Escape)       */

/* ---- Global editor state ---- */

extern char  lines[MAX_LINES][MAX_LINE_LEN + 1];
extern int   num_lines;
extern int   cur_row;        /* cursor row  (index into lines[])   */
extern int   cur_col;        /* cursor col  (0-based)              */
extern int   top_row;        /* first visible line                 */
extern int   mode;           /* current mode                       */
extern char  filename[40];   /* current filename                   */
extern char  modified;       /* dirty flag                         */
extern int   running;        /* 0 = quit                           */
extern char  status_msg[41]; /* one-shot status message            */
extern char  cmd_buf[40];    /* command-mode input buffer          */

/* ---- Function prototypes ---- */

/* editor.c */
void editor_init   (void);
void editor_process_key (unsigned char c);

/* display.c */
void display_init       (void);
void display_full       (void);
void display_line       (int lnum);
void display_from       (int lnum);
void display_status_bar (void);
void display_set_cursor (void);

/* fileio.c */
int  file_load (const char *name);
int  file_save (const char *name);

#endif /* EDITOR_H */
