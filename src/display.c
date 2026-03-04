/***************************************************************************
--          Commodore 64 Vi Editor - screen rendering
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
/*
 * Screen layout (40x24 visible area used):
 *   Rows 0 .. TEXT_ROWS-1  : text content
 *   Row  STATUS_ROW        : status / command bar (reverse video)
 */
#include <string.h>
#include <stdio.h>
#include <conio.h>
#include "editor.h"

/* ---- helpers ---- */

/* Clear from current position to column SCREEN_COLS-1 */
static void clear_to_eol (int from_col)
{
    int n = SCREEN_COLS - from_col;
    if (n > 0)
        cclear((unsigned char)n);
}

/* ============================================================
 * Public display functions
 * ============================================================ */

void display_init (void)
{
    clrscr();
    /* Show brief welcome in the status bar area */
    revers(1);
    gotoxy(0, STATUS_ROW);
    cputs("C64 Vi " PROG_VERSION " -- :e<file> :w<file> :q");
    cclear((unsigned char)(SCREEN_COLS - 26));
    revers(0);
}

/* Draw a single line of the file onto the screen.
 * lnum  = index into lines[].
 * If lnum >= num_lines, draw a '~' (no-file marker). */
void display_line (int lnum)
{
    int scr_row = lnum - top_row;
    int col = 0;
    char *p;

    if (scr_row < 0 || scr_row >= TEXT_ROWS)
        return;

    gotoxy(0, (unsigned char)scr_row);

    if (lnum < num_lines) {
        p = lines[lnum];
        while (*p && col < SCREEN_COLS) {
            cputc(*p++);
            col++;
        }
    } else {
        cputc('~');
        col = 1;
    }
    clear_to_eol(col);
}

/* Redraw all text rows from lnum downwards (inclusive). */
void display_from (int lnum)
{
    int r;
    int scr_start = lnum - top_row;
    if (scr_start < 0) scr_start = 0;

    for (r = scr_start; r < TEXT_ROWS; r++)
        display_line(r + top_row);
}

/* Full screen redraw (text area + status bar). */
void display_full (void)
{
    int r;
    for (r = 0; r < TEXT_ROWS; r++)
        display_line(r + top_row);
    display_status_bar();
}

/* Draw the status / command bar at STATUS_ROW. */
void display_status_bar (void)
{
    char buf[SCREEN_COLS + 1];
    int  len;
    int  pad;

    revers(1);
    gotoxy(0, STATUS_ROW);

    if (mode == MODE_COMMAND) {
        /* Show ':' + command being typed */
        buf[0] = ':';
        strncpy(buf + 1, cmd_buf, SCREEN_COLS - 1);
        buf[SCREEN_COLS] = '\0';
        len = (int)strlen(buf);
        cputs(buf);
        clear_to_eol(len);

    } else if (status_msg[0] != '\0') {
        /* One-shot message */
        len = (int)strlen(status_msg);
        if (len > SCREEN_COLS) len = SCREEN_COLS;
        strncpy(buf, status_msg, (unsigned int)len);
        buf[len] = '\0';
        cputs(buf);
        clear_to_eol(len);
        /* Clear message after display so it only shows once */
        status_msg[0] = '\0';

    } else {
        /* Normal status: MODE  FILENAME  [+]  ROW,COL */
        char pos[12];
        char modestr[5];
        char fnbuf[21];
        int  used = 0;

        /* Mode indicator */
        switch (mode) {
            case MODE_NORMAL:  strcpy(modestr, "NOR "); break;
            case MODE_INSERT:  strcpy(modestr, "INS "); break;
            default:           strcpy(modestr, "    "); break;
        }
        cputs(modestr);
        used += 4;

        /* Filename (truncated to 20 chars) */
        if (filename[0] != '\0') {
            strncpy(fnbuf, filename, 20);
            fnbuf[20] = '\0';
        } else {
            strcpy(fnbuf, "[no file]");
        }
        len = (int)strlen(fnbuf);
        cputs(fnbuf);
        used += len;

        /* Modified flag */
        if (modified) {
            cputs(" *");
            used += 2;
        }

        /* Row/col position, right-aligned */
        sprintf(pos, "%d,%d", cur_row + 1, cur_col + 1);
        pad = SCREEN_COLS - used - (int)strlen(pos) - 1;
        if (pad < 1) pad = 1;
        while (pad-- > 0) cputc(' ');
        cputs(pos);
    }

    revers(0);
}

/* Position the hardware cursor at the current editor position. */
void display_set_cursor (void)
{
    int scr_col, scr_row;

    if (mode == MODE_COMMAND) {
        /* Cursor sits after the ':' + command text in the status bar */
        scr_col = 1 + (int)strlen(cmd_buf);
        if (scr_col >= SCREEN_COLS) scr_col = SCREEN_COLS - 1;
        gotoxy((unsigned char)scr_col, STATUS_ROW);
    } else {
        scr_row = cur_row - top_row;
        scr_col = cur_col;
        if (scr_col >= SCREEN_COLS) scr_col = SCREEN_COLS - 1;
        if (scr_row < 0)          scr_row = 0;
        if (scr_row >= TEXT_ROWS) scr_row = TEXT_ROWS - 1;
        gotoxy((unsigned char)scr_col, (unsigned char)scr_row);
    }
}
