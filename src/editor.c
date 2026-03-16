/***************************************************************************
--      Commodore 64 Vi Editor - buffer operations and key handling
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
#include <string.h>
#include <ctype.h>
#include <stdio.h>
#include <conio.h>
#include "editor.h"

/* ---- Global state definitions ---- */

char  lines[MAX_LINES][MAX_LINE_LEN + 1];
int   num_lines   = 1;
int   cur_row     = 0;
int   cur_col     = 0;
int   top_row     = 0;
int   mode        = MODE_NORMAL;
char  filename[40]  = "";
char  modified    = 0;
int   running     = 1;
char  status_msg[41] = "";

/* command-mode input buffer */
char  cmd_buf[40];
static int  cmd_len = 0;

/* pending normal-mode command (for dd, etc.) */
static unsigned char pending = 0;

/* ============================================================
 * Low-level buffer helpers
 * ============================================================ */

/* Clamp cursor to valid position for the current mode */
static void clamp_cursor (void)
{
    int len;
    if (cur_row < 0)          cur_row = 0;
    if (cur_row >= num_lines) cur_row = num_lines - 1;

    len = (int)strlen(lines[cur_row]);

    if (mode == MODE_INSERT) {
        /* Insert: cursor may sit one past last char */
        if (cur_col > len) cur_col = len;
    } else {
        /* Normal: cursor must be on an existing char */
        if (len == 0) {
            cur_col = 0;
        } else if (cur_col >= len) {
            cur_col = len - 1;
        }
    }
    if (cur_col < 0) cur_col = 0;
}

/* Adjust top_row so cur_row is visible */
static void scroll_into_view (void)
{
    if (cur_row < top_row)
        top_row = cur_row;
    else if (cur_row >= top_row + TEXT_ROWS)
        top_row = cur_row - TEXT_ROWS + 1;
}

/* Insert character c at (lnum, col) */
static void insert_char_at (int lnum, int col, unsigned char c)
{
    int len = (int)strlen(lines[lnum]);
    if (len >= MAX_LINE_LEN) return;
    memmove(&lines[lnum][col + 1], &lines[lnum][col],
            (unsigned int)(len - col + 1));
    lines[lnum][col] = (char)c;
    modified = 1;
}

/* Delete character at (lnum, col) */
static void delete_char_at (int lnum, int col)
{
    int len = (int)strlen(lines[lnum]);
    if (col >= len) return;
    memmove(&lines[lnum][col], &lines[lnum][col + 1],
            (unsigned int)(len - col));
    modified = 1;
}

/* Insert a blank line AFTER lnum (-1 = before first line) */
static void insert_line_after (int lnum)
{
    int i;
    if (num_lines >= MAX_LINES) return;
    for (i = num_lines; i > lnum + 1; i--)
        memcpy(lines[i], lines[i - 1], MAX_LINE_LEN + 1);
    lines[lnum + 1][0] = '\0';
    num_lines++;
    modified = 1;
}

/* Delete line lnum */
static void delete_line (int lnum)
{
    int i;
    if (num_lines == 1) {
        lines[0][0] = '\0';
        modified = 1;
        return;
    }
    for (i = lnum; i < num_lines - 1; i++)
        memcpy(lines[i], lines[i + 1], MAX_LINE_LEN + 1);
    num_lines--;
    modified = 1;
}

/* Split line lnum at col: everything from col onwards goes to lnum+1 */
static void split_line_at (int lnum, int col)
{
    int i;
    if (num_lines >= MAX_LINES) return;
    for (i = num_lines; i > lnum + 1; i--)
        memcpy(lines[i], lines[i - 1], MAX_LINE_LEN + 1);
    strcpy(lines[lnum + 1], &lines[lnum][col]);
    lines[lnum][col] = '\0';
    num_lines++;
    modified = 1;
}

/* Join line lnum with lnum+1 */
static void join_lines (int lnum)
{
    int i;
    int len1, len2;
    if (lnum >= num_lines - 1) return;
    len1 = (int)strlen(lines[lnum]);
    len2 = (int)strlen(lines[lnum + 1]);
    if (len1 + len2 <= MAX_LINE_LEN) {
        strcat(lines[lnum], lines[lnum + 1]);
    } else {
        strncat(lines[lnum], lines[lnum + 1],
                (unsigned int)(MAX_LINE_LEN - len1));
        lines[lnum][MAX_LINE_LEN] = '\0';
    }
    for (i = lnum + 1; i < num_lines - 1; i++)
        memcpy(lines[i], lines[i + 1], MAX_LINE_LEN + 1);
    num_lines--;
    modified = 1;
}

/* ============================================================
 * Command-mode execution
 * ============================================================ */

static void exec_command (void)
{
    char *p = cmd_buf;
    char *fn;
    char  lc;

    /* skip leading spaces */
    while (*p == ' ') p++;
    if (*p == '\0') return;

    lc = (char)tolower((unsigned char)*p);

    if (lc == 'q') {
        /* :q  or  :q! */
        if (p[1] == '!' || !modified) {
            running = 0;
        } else {
            strcpy(status_msg, "Unsaved! Use :q! to force");
        }

    } else if (lc == 'w') {
        /* :w [filename]  or  :wq [filename] */
        fn = p + 1;
        /* skip optional 'q' that belongs to :wq */
        if (tolower((unsigned char)*fn) == 'q') fn++;
        while (*fn == ' ') fn++;

        if (*fn != '\0') {
            strncpy(filename, fn, 39);
            filename[39] = '\0';
            /* Keep filename as-is: unshifted keys give PETSCII 65-90
             * which is exactly what CBM DOS expects for filenames */
        }
        if (filename[0] == '\0') {
            strcpy(status_msg, "No filename - use :w <name>");
        } else if (file_save(filename) == 0) {
            modified = 0;
            sprintf(status_msg, "Saved: %s", filename);
        } else {
            strcpy(status_msg, "Save error!");
        }

        /* :wq -> also quit after a successful save */
        {
            char *q = p + 1;
            while (*q == ' ') q++;
            if (tolower((unsigned char)*q) == 'q' && modified == 0)
                running = 0;
        }

    } else if (lc == 'e') {
        /* :e <filename> */
        fn = p + 1;
        while (*fn == ' ') fn++;
        if (*fn == '\0') {
            strcpy(status_msg, "Usage: :e <filename>");
        } else {
            strncpy(filename, fn, 39);
            filename[39] = '\0';
            /* Keep filename as typed (PETSCII 65-90 from unshifted keys) */
            if (file_load(filename) == 0) {
                cur_row = 0; cur_col = 0; top_row = 0;
                modified = 0;
                sprintf(status_msg, "Loaded: %s", filename);
            } else {
                sprintf(status_msg, "Cannot open: %s", filename);
            }
        }

    } else {
        sprintf(status_msg, "Unknown command: %s", cmd_buf);
    }
}

/* ============================================================
 * Key handlers per mode
 * ============================================================ */

static void handle_insert (unsigned char c)
{
    int len;

    switch (c) {
        case KEY_STOP:
        case KEY_F1:            /* F1 also exits insert mode */
            /* Return to normal mode */
            mode = MODE_NORMAL;
            clamp_cursor();     /* in normal mode cursor can't be past eol */
            display_status_bar();
            display_set_cursor();
            break;

        case KEY_RETURN:
            split_line_at(cur_row, cur_col);
            cur_row++;
            cur_col = 0;
            scroll_into_view();
            display_full();
            display_status_bar();
            display_set_cursor();
            break;

        case KEY_DEL:
            if (cur_col > 0) {
                /* delete char to the left */
                cur_col--;
                delete_char_at(cur_row, cur_col);
                display_line(cur_row);
            } else if (cur_row > 0) {
                /* at start of line: join with previous */
                int prev_len = (int)strlen(lines[cur_row - 1]);
                join_lines(cur_row - 1);
                cur_row--;
                cur_col = prev_len;
                scroll_into_view();
                display_from(cur_row);
            }
            display_status_bar();
            display_set_cursor();
            break;

        case KEY_UP:
            if (cur_row > 0) {
                cur_row--;
                clamp_cursor();
                scroll_into_view();
                display_status_bar();
                display_set_cursor();
            }
            break;

        case KEY_DOWN:
            if (cur_row < num_lines - 1) {
                cur_row++;
                clamp_cursor();
                scroll_into_view();
                display_status_bar();
                display_set_cursor();
            }
            break;

        case KEY_LEFT:
            if (cur_col > 0) {
                cur_col--;
                display_set_cursor();
            }
            break;

        case KEY_RIGHT:
            len = (int)strlen(lines[cur_row]);
            if (cur_col < len) {
                cur_col++;
                display_set_cursor();
            }
            break;

        default:
            /* printable PETSCII character */
            if (c >= 32 && c != 127) {
                insert_char_at(cur_row, cur_col, c);
                cur_col++;
                display_line(cur_row);
                display_status_bar();
                display_set_cursor();
            }
            break;
    }
}

static void handle_normal (unsigned char c)
{
    int len;

    /* ---- Resolve pending commands (dd, gg) ---- */
    if (pending != 0) {
        unsigned char prev = pending;
        pending = 0;

        if (prev == 'd' && c == 'd') {
            /* dd: delete current line */
            delete_line(cur_row);
            if (cur_row >= num_lines) cur_row = num_lines - 1;
            clamp_cursor();
            scroll_into_view();
            display_full();
            display_status_bar();
            display_set_cursor();
            return;
        }
        if (prev == 'g' && c == 'g') {
            /* gg: go to first line */
            cur_row = 0; cur_col = 0; top_row = 0;
            display_full();
            display_status_bar();
            display_set_cursor();
            return;
        }
        /* Unknown sequence: fall through and process c normally */
    }

    switch (c) {

        /* ---- Cursor movement ---- */
        case 'h': case KEY_LEFT:
            if (cur_col > 0) {
                cur_col--;
                display_set_cursor();
            }
            break;

        case 'l': case KEY_RIGHT:
            len = (int)strlen(lines[cur_row]);
            if (len > 0 && cur_col < len - 1) {
                cur_col++;
                display_set_cursor();
            }
            break;

        case 'k': case KEY_UP:
            if (cur_row > 0) {
                cur_row--;
                clamp_cursor();
                scroll_into_view();
                display_full();
                display_status_bar();
                display_set_cursor();
            }
            break;

        case 'j': case KEY_DOWN:
            if (cur_row < num_lines - 1) {
                cur_row++;
                clamp_cursor();
                scroll_into_view();
                display_full();
                display_status_bar();
                display_set_cursor();
            }
            break;

        case '0': case KEY_HOME:
            /* beginning of line */
            cur_col = 0;
            display_set_cursor();
            break;

        case '$':
            /* end of line */
            len = (int)strlen(lines[cur_row]);
            cur_col = (len > 0) ? len - 1 : 0;
            display_set_cursor();
            break;

        case 'G':
            /* G (Shift+G): go to last line */
            cur_row = num_lines - 1;
            cur_col = 0;
            scroll_into_view();
            display_full();
            display_status_bar();
            display_set_cursor();
            break;

        case 'g':
            /* first 'g' - wait for second to form 'gg' */
            pending = 'g';
            break;

        /* ---- Mode switches ---- */
        case 'i':
            /* i: insert before cursor */
            mode = MODE_INSERT;
            status_msg[0] = '\0';
            display_status_bar();
            display_set_cursor();
            break;

        case 'a':
            /* a: insert after cursor */
            len = (int)strlen(lines[cur_row]);
            cur_col = len;
            mode = MODE_INSERT;
            status_msg[0] = '\0';
            display_status_bar();
            display_set_cursor();
            break;

        case 'o':
            /* o: open new line below, enter insert */
            insert_line_after(cur_row);
            cur_row++;
            cur_col = 0;
            scroll_into_view();
            mode = MODE_INSERT;
            display_full();
            display_status_bar();
            display_set_cursor();
            break;

        case 'O':
            /* O (Shift+O): open new line above, enter insert */
            insert_line_after(cur_row - 1);
            cur_col = 0;
            scroll_into_view();
            mode = MODE_INSERT;
            display_full();
            display_status_bar();
            display_set_cursor();
            break;

        /* ---- Edit commands ---- */
        case 'x':
            /* x: delete character under cursor */
            delete_char_at(cur_row, cur_col);
            clamp_cursor();
            display_line(cur_row);
            display_status_bar();
            display_set_cursor();
            break;

        case 'd':
            /* first 'd' - wait for second to form 'dd' */
            pending = 'd';
            break;

        /* ---- Command mode ---- */
        case ':':
            mode = MODE_COMMAND;
            cmd_buf[0] = '\0';
            cmd_len = 0;
            display_status_bar();
            display_set_cursor();
            break;

        default:
            break;
    }
}

static void handle_command_char (unsigned char c)
{
    switch (c) {
        case KEY_STOP:
            /* Cancel command, return to normal */
            mode = MODE_NORMAL;
            status_msg[0] = '\0';
            display_status_bar();
            display_set_cursor();
            break;

        case KEY_RETURN:
            /* Execute command */
            exec_command();
            mode = MODE_NORMAL;
            display_full();
            display_status_bar();
            display_set_cursor();
            break;

        case KEY_DEL:
            /* Backspace in command line */
            if (cmd_len > 0) {
                cmd_len--;
                cmd_buf[cmd_len] = '\0';
                display_status_bar();
            }
            break;

        default:
            /* Printable: append to command buffer */
            if (c >= 32 && c != 127 && cmd_len < 38) {
                cmd_buf[cmd_len++] = (char)c;
                cmd_buf[cmd_len]   = '\0';
                display_status_bar();
            }
            break;
    }
}

/* ============================================================
 * Public API
 * ============================================================ */

void editor_init (void)
{
    lines[0][0] = '\0';
    num_lines = 1;
    cur_row   = 0;
    cur_col   = 0;
    top_row   = 0;
    mode      = MODE_NORMAL;
    filename[0] = '\0';
    modified  = 0;
    running   = 1;
    status_msg[0] = '\0';
    pending   = 0;
    cmd_buf[0] = '\0';
    cmd_len   = 0;
}

void editor_process_key (unsigned char c)
{
    switch (mode) {
        case MODE_NORMAL:  handle_normal(c);       break;
        case MODE_INSERT:  handle_insert(c);       break;
        case MODE_COMMAND: handle_command_char(c); break;
    }
}
