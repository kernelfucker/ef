#ifndef ef_h
#define ef_h

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <termios.h>
#include <ctype.h>
#include <sys/ioctl.h>
#include <sys/types.h>

#define clear_screen    "\x1b[2J"
#define cursor_home     "\x1b[H"
#define cursor_pos(y,x) "\x1b["#y";"#x"H"
#define erase_line      "\x1b[K"
#define text_reset      "\x1b[0m"
#define cursor_hide     "\x1b[?25l"
#define cursor_show     "\x1b[?25h"

#define control_key(k) ((k) & 0x1f)
#define tab_stop        4
#define status_height   1
#define line_height	1
#define ef_version      "0.1"

#define block_cursor	"\x1b[2 q"

#define qkey control_key('q')
#define skey control_key('s')

#define max_line_length 8192
#define max_lines       8192

#endif
