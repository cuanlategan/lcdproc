/*  This is the LCDproc driver for ncurses.
    It displays an emulated LCD display at top left of terminal screen
    using ncurses.

    Copyright (C) ?????? ?????????

    This program is free software; you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation; either version 2 of the License, or
    any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program; if not, write to the Free Software
    Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307 */

/*Configfile support added by Rene Wagner (C) 2001*/

/*
Different implementations of (n)curses available on:
OpenBSD:
 	http://www.openbsd.org/cgi-bin/cvsweb/src/lib/libcurses/
	ncurses
NetBSD:
	http://cvsweb.netbsd.org/bsdweb.cgi/basesrc/lib/libcurses/
	curses : does not define ACS_S3, ACS_S7, wcolor_set() or redrawwin().
	it is possible to make a:
		#define ACS_S3 (_acs_char['p'])
		#define ACS_S7 (_acs_char['r'])
FreeBSD:
	http://www.freebsd.org/cgi/cvsweb.cgi/src/
	ncurses
RedHat, Debian, (most distros) Linux:
	ncurses
SunOS (5.5.1):
	curses : does not define ACS_S3, ACS_S7 or wcolor_set().
	it is possible to make a:
		#define ACS_S3 (acs_map['p'])
		#define ACS_S7 (acs_map['r'])
*/

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <termios.h>
#include <fcntl.h>
#include <string.h>
#include <sys/errno.h>
#include <ctype.h>
#ifdef HAVE_NCURSES_H
#include <ncurses.h>
#else
#include <curses.h>
#endif

#include "shared/str.h"

#include "lcd.h"
#include "curses_drv.h"
#include "report.h"
//#include "configfile.h"

// ACS_S9 and ACS_S1 are defined as part of XSI Curses standard, Issue 4.
// However, ACS_S3 and ACS_S7 are not; these definitions were created to support
// commonly available graphics found in many terminfo definitions.  The acsc character
// used for ACS_S3 is 'p'; the acsc character used for ACS_S7 is 'r'.
//
// Other systems may very likely support these characters; however,
// their names were invented for ncurses.
#ifndef ACS_S3
# ifdef CURSES_HAS_ACS_MAP
#  define ACS_S3 (acs_map['p'])
# else
#  ifdef CURSES_HAS__ACS_CHAR
#   define ACS_S3 (_acs_char['p'])
#  else
#   define ACS_S3 ACS_S1    // Last resort
#  endif
# endif
#endif

#ifndef ACS_S7
# ifdef CURSES_HAS_ACS_MAP
#  define ACS_S7 (acs_map['r'])
# else
#  ifdef CURSES_HAS__ACS_CHAR
#   define ACS_S7 (_acs_char['r'])
#  else
#   define ACS_S7 ACS_S9    // Last resort
#  endif
# endif
#endif

// Character used for title bars...
#define PAD '#'
// #define PAD ACS_BLOCK

int ELLIPSIS = 7;  // Should this go in PrivateData ?
void curses_drv_restore_screen (Driver *drvthis);


//////////////////////////////////////////////////////////////////////////
////////////////////// For Curses Terminal Output ////////////////////////
//////////////////////////////////////////////////////////////////////////

static char icon_char = '@';
static WINDOW *lcd_win;

/*this is really ugly ;) but works ;)*/
static char num_icon [10][4][3] =	{{{' ','_',' '}, /*0*/
					  {'|',' ','|'},
					  {'|','_','|'},
					  {' ',' ',' '}},
					  {{' ',' ',' '},/*1*/
					  {' ',' ','|'},
					  {' ',' ','|'},
					  {' ',' ',' '}},
					  {{' ','_',' '},/*2*/
					  {' ','_','|'},
					  {'|','_',' '},
					  {' ',' ',' '}},
					  {{' ','_',' '},/*3*/
					  {' ','_','|'},
					  {' ','_','|'},
					  {' ',' ',' '}},
					  {{' ',' ',' '},/*4*/
					  {'|','_','|'},
					  {' ',' ','|'},
					  {' ',' ',' '}},
					  {{' ','_',' '},/*5*/
					  {'|','_',' '},
					  {' ','_','|'},
					  {' ',' ',' '}},
					  {{' ','_',' '},/*6*/
					  {'|','_',' '},
					  {'|','_','|'},
					  {' ',' ',' '}},
					  {{' ','_',' '},/*7*/
					  {' ',' ','|'},
					  {' ',' ','|'},
					  {' ',' ',' '}},
					  {{' ','_',' '},/*8*/
					  {'|','_','|'},
					  {'|','_','|'},
					  {' ',' ',' '}},
					  {{' ','_',' '},/*9*/
					  {'|','_','|'},
					  {' ','_','|'},
					  {' ',' ',' '}}};
/*end of ugly code ;) Rene Wagner*/

chtype get_color (char *colorstr) {
	if (strcasecmp(colorstr, "red") == 0)
		return COLOR_RED;
	else if (strcasecmp(colorstr, "black") == 0)
		return COLOR_BLACK;
	else if (strcasecmp(colorstr, "green") == 0)
		return COLOR_GREEN;
	else if (strcasecmp(colorstr, "yellow") == 0)
		return COLOR_YELLOW;
	else if (strcasecmp(colorstr, "blue") == 0)
		return COLOR_BLUE;
	else if (strcasecmp(colorstr, "magenta") == 0)
		return COLOR_MAGENTA;
	else if (strcasecmp(colorstr, "cyan") == 0)
		return COLOR_CYAN;
	else if (strcasecmp(colorstr, "white") == 0)
		return COLOR_WHITE;
	else
		return -1;
}

/*
 * A few different nice pairs of colors to use...
 *
 */

#define DEFAULT_FOREGROUND_COLOR COLOR_CYAN
#define DEFAULT_BACKGROUND_COLOR COLOR_BLUE

// #define DEFAULT_FOREGROUND_COLOR COLOR_WHITE
// #define DEFAULT_BACKGROUND_COLOR COLOR_BLUE

// #define DEFAULT_FOREGROUND_COLOR COLOR_WHITE
// #define DEFAULT_BACKGROUND_COLOR COLOR_RED

chtype
set_foreground_color (char * buf) {
	chtype color;

	if ((color = get_color(buf)) < 0)
		color = DEFAULT_FOREGROUND_COLOR;

	return color;
}

chtype
set_background_color (char * buf) {
	chtype color;

	if ((color = get_color(buf)) < 0)
		color = DEFAULT_BACKGROUND_COLOR;

	return color;
}

/*
 * What position (X,Y) to start the left top corner at...
 *
 */

#define TOP_LEFT_X 7
#define TOP_LEFT_Y 7

static int current_color_pair, current_border_pair, curses_backlight_state = 0;
static int width, height;


// Vars for the server core
MODULE_EXPORT char *api_version = API_VERSION;
MODULE_EXPORT int stay_in_foreground = 1;
MODULE_EXPORT int supports_multiple = 0;
MODULE_EXPORT char *symbol_prefix = "curses_drv_";


int
curses_drv_init (Driver *drvthis, char *args)
{
	char buf[256];
	int w, h;

	// Colors....
	chtype	back_color = DEFAULT_BACKGROUND_COLOR,
		fore_color = DEFAULT_FOREGROUND_COLOR,
		backlight_color = DEFAULT_BACKGROUND_COLOR;

	// Screen position (top left)
	int	screen_begx = CONF_DEF_TOP_LEFT_X,
		screen_begy = CONF_DEF_TOP_LEFT_Y;

	// Set display sizes
	if( drvthis->request_display_width() > 0
	&& drvthis->request_display_height() > 0 ) {
		// Use size from primary driver
		width = drvthis->request_display_width();
		height = drvthis->request_display_height();
	}
	else {
		// Use default size
		width = LCD_DEFAULT_WIDTH;
		height = LCD_DEFAULT_HEIGHT;
	}

	/*Get settings from config file*/

	/*Get color settings*/
	/*foreground color*/
	strncpy(buf, drvthis->config_get_string ( drvthis->name , "foreground" , 0 , CONF_DEF_FOREGR),sizeof(buf));
	buf[sizeof(buf)-1]=0;
	fore_color = set_foreground_color(buf);
	debug( RPT_DEBUG, "CURSES: using foreground color: %s", buf);
	/*background color*/
	strncpy(buf, drvthis->config_get_string ( drvthis->name , "background" , 0 , CONF_DEF_BACKGR),sizeof(buf));
	buf[sizeof(buf)-1]=0;
	back_color = set_background_color(buf);
	debug( RPT_DEBUG, "CURSES: using background color: %s", buf);
	/*backlight color*/
	strncpy(buf, drvthis->config_get_string ( drvthis->name , "backlight" , 0 , CONF_DEF_BACKLIGHT), sizeof(buf));
	buf[sizeof(buf)-1]=0;
	backlight_color = set_background_color(buf);
	debug( RPT_DEBUG, "CURSES: using backlight color: %s", buf);

	//TODO: Make it possible to configure the backlight's "off" color and its "on" color
	//      Or maybe don't do so? - Rene Wagner

	/*Get size settings*/
	strncpy(buf, drvthis->config_get_string ( drvthis->name , "size" , 0 , CONF_DEF_SIZE), sizeof(buf));
	buf[sizeof(buf)-1]=0;
	if( sscanf(buf , "%dx%d", &w, &h ) != 2
	|| (w <= 0)
	|| (h <= 0)) {
		report (RPT_WARNING, "CURSES: Cannot read size: %s. Using default value.\n", buf);
		//sscanf( CONF_DEF_SIZE , "%dx%d", &width, &height );
		// default value is already set
	}
	else {
		width = w;
		height = h;
	}

	/*Get position settings*/
	if (0<=drvthis->config_get_int ( drvthis->name , "topleftx" , 0 , CONF_DEF_TOP_LEFT_X) && drvthis->config_get_int ( drvthis->name , "topleftx" , 0 , CONF_DEF_TOP_LEFT_X) <= 255) {
		screen_begx = drvthis->config_get_int ( drvthis->name , "topleftx" , 0 , CONF_DEF_TOP_LEFT_X);
	} else {
		report (RPT_WARNING, "CURSES: topleftx must between 0 and 255. Using default value %d.\n",CONF_DEF_TOP_LEFT_X);
	}
	if (0<=drvthis->config_get_int ( drvthis->name , "toplefty" , 0 , CONF_DEF_TOP_LEFT_Y) && drvthis->config_get_int ( drvthis->name , "toplefty" , 0 , CONF_DEF_TOP_LEFT_Y) <= 255) {
		screen_begy = drvthis->config_get_int ( drvthis->name , "toplefty" , 0 , CONF_DEF_TOP_LEFT_Y);
	} else {
		report (RPT_WARNING, "CURSES: toplefty must between 0 and 255. Using default value %d.\n",CONF_DEF_TOP_LEFT_Y);
	}

	//debug: sleep(1);

	// Init curses...
	initscr ();
	cbreak ();
	noecho ();
	nonl ();

	nodelay (stdscr, TRUE);
	intrflush (stdscr, FALSE);
	keypad (stdscr, TRUE);

	lcd_win = newwin(height + 2,
			 width + 2,
			 screen_begy,
			 screen_begx);

	//nodelay (lcd_win, TRUE);
	//intrflush (lcd_win, FALSE);
	//keypad (lcd_win, TRUE);

	curs_set(0);

	if (has_colors()) {
		start_color();
		init_pair(1, back_color, fore_color);
		init_pair(2, fore_color, back_color);
		init_pair(3, COLOR_WHITE, back_color);
		init_pair(4, fore_color, backlight_color);
		init_pair(5, COLOR_WHITE, backlight_color);

		current_color_pair = 2;
		current_border_pair = 3;
	}

	curses_drv_clear (drvthis);

	// Set variables for server
	drvthis->api_version = api_version;
	drvthis->stay_in_foreground = &stay_in_foreground;
	drvthis->supports_multiple = &supports_multiple;

	// Set the functions the driver supports
	drvthis->init = curses_drv_init;
	drvthis->close = curses_drv_close;
	drvthis->width = curses_drv_width;
	drvthis->height = curses_drv_height;
	drvthis->clear = curses_drv_clear;
	drvthis->flush = curses_drv_flush;
	drvthis->string = curses_drv_string;
	drvthis->chr = curses_drv_chr;

	drvthis->old_vbar = curses_drv_vbar;
	//drvthis->init_vbar = NULL;
	drvthis->old_hbar = curses_drv_hbar;
	//drvthis->init_hbar = NULL;
	drvthis->num = curses_drv_num;
	//drvthis->init_num = curses_drv_init_num;

	drvthis->backlight = curses_drv_backlight;
	//drvthis->set_char = NULL;
	drvthis->old_icon = curses_drv_icon;  // NEEDS TO BE CHANGED !

	drvthis->getkey = curses_drv_getkey;
	drvthis->heartbeat = curses_drv_heartbeat;

	// Change the character used for "..."
	ELLIPSIS = '~';

	return 0;
}

static void
curses_drv_wborder (WINDOW *win) {
	//int x, y;
	//char buf[128];

#ifdef CURSES_HAS_WCOLOR_SET
	if (has_colors()) {
		wcolor_set(win, current_border_pair, NULL);
		//wattron(win, COLOR_PAIR(current_border_pair) | A_BOLD);
		wattron(win, A_BOLD);
	}
#endif

	box(win, 0, 0);

#ifdef CURSES_HAS_WCOLOR_SET
	if (has_colors()) {
		wcolor_set(win, current_color_pair, NULL);
		//wattron(win, COLOR_PAIR(current_color_pair));
		wattroff(win, A_BOLD);
	}
#endif
}

MODULE_EXPORT void
curses_drv_close (Driver *drvthis)
{
	// Note that the program leaves a screen on
	// the display to be left behind after closing;
	// so don't clear...
	//
	// Close curses
	wrefresh (lcd_win);
	delwin (lcd_win);

	move (0, 0);
	endwin ();
	curs_set(1);
}

/////////////////////////////////////////////////////////////////
// Returns the display width
//
MODULE_EXPORT int
curses_drv_width (Driver *drvthis)
{
	return width;
}

/////////////////////////////////////////////////////////////////
// Returns the display height
//
MODULE_EXPORT int
curses_drv_height (Driver *drvthis)
{
	return height;
}

/////////////////////////////////////////////////////////////////
// Clears the LCD screen
//
MODULE_EXPORT void
curses_drv_clear (Driver *drvthis)
{
	wbkgdset(lcd_win, COLOR_PAIR(current_color_pair) | ' ');
	curses_drv_wborder (lcd_win);
	werase (lcd_win);
}

#define ValidX(x) { if ((x) > width) { (x) = width; } else (x) = (x) < 1 ? 1 : x; }
#define ValidY(y) { if ((y) > height) { (y) = height; } else (y) = (y) < 1 ? 1 : y; }

MODULE_EXPORT void
curses_drv_backlight (Driver *drvthis, int promille)
{
	if (curses_backlight_state == promille)
		return;

	// no backlight: pairs 2, 3
	// backlight:    pairs 4, 5

	curses_backlight_state = promille;

	if (promille) {
		current_color_pair = 4;
		current_border_pair = 5;
	}
	else {
		current_color_pair = 2;
		current_border_pair = 3;
	}

	curses_drv_clear(drvthis);
}

/////////////////////////////////////////////////////////////////
// Prints a string on the lcd display, at position (x,y).  The
// upper-left is (1,1), and the lower right should be (20,4).
//
MODULE_EXPORT void
curses_drv_string (Driver *drvthis, int x, int y, char *string)
{
	//int i;
	unsigned char *p;

	ValidX(x);
	ValidY(y);

	p = string;

	// Convert NULLs and 0xFF in string to
	// valid printables...
	while (*p) {
		switch (*p) {
			//case 0: -- can never happen?
			//	*p = icon_char;
			//	break;
			case 255:
				*p = PAD;
				break;
		}
		p++;
	}

	mvwaddstr (lcd_win, y, x, string);
}

/////////////////////////////////////////////////////////////////
// Prints a character on the lcd display, at position (x,y).  The
// upper-left is (1,1), and the lower right should be (20,4).
//
MODULE_EXPORT void
curses_drv_chr (Driver *drvthis, int x, int y, char c)
{
	int ch;

	ValidX(x);
	ValidY(y);

	switch (c) {
		case 0:
			c = icon_char;
			break;
		case -1:	// translates to 255 (ouch!)
			c = PAD;
			break;
		//default:
		//	normal character...
	}

	if ((ch = getch ()) != ERR)
		if (ch == 0x0C) {
			curses_drv_restore_screen(drvthis);
			ungetch(ch);
		}

	mvwaddch (lcd_win, y, x, c);
}

/////////////////////////////////////////////////////////////////
// Sets up for big numbers.
//
MODULE_EXPORT void
curses_drv_init_num (Driver *drvthis)
{
	;
}

/////////////////////////////////////////////////////////////////
// Writes a big number.
//
MODULE_EXPORT void
curses_drv_num (Driver *drvthis, int x, int num)
{
	int y, dx;

	for (y = 1; y < 5; y++)
		for (dx = 0; dx < 3; dx++)
			curses_drv_chr (drvthis, x + dx, y, num_icon[num][y-1][dx]);
//   printf("%1d",num);
}

/////////////////////////////////////////////////////////////////
// Draws a vertical bar; erases entire column onscreen.
//
MODULE_EXPORT void
curses_drv_vbar (Driver *drvthis, int x, int len)
{
	int y;
	char map[] = { ACS_S9, ACS_S9, ACS_S7, ACS_S7, ACS_S3, ACS_S3, ACS_S1, ACS_S1 };

	ValidX(x);

#define MAX_LINES (LCD_DEFAULT_CELLHEIGHT * height)

	len = len > (MAX_LINES - 1) ? (MAX_LINES - 1) : len;
	len = len < 0 ? 0 : len;

	// len is the length of the bar (in pixels/scanlines)
	// y is one character line (cellheight pixels/scanlines)

	for (y = height; y > 0 && len > 0; y--) {

		if (len >= LCD_DEFAULT_CELLHEIGHT) {
			// write a "full" block to the screen...
			//curses_drv_chr (x, y, '8');
			curses_drv_chr (drvthis, x, y, ACS_BLOCK);
			len -= LCD_DEFAULT_CELLHEIGHT;
		}
		else {
			// write a partial block...
			curses_drv_chr (drvthis, x, y, map[len-1]);
			break;
		}

	}

//  move(4-len/lcd.cellhgt, x-1);
//  vline(0, len/lcd.cellhgt);

}

/////////////////////////////////////////////////////////////////
// Draws a horizontal bar to the right.
//
MODULE_EXPORT void
curses_drv_hbar (Driver *drvthis, int x, int y, int len)
{
	for (; x <= width && len > 0; x++) {
		if (len >= LCD_DEFAULT_CELLWIDTH)
			curses_drv_chr (drvthis, x, y, '=');
		else
			curses_drv_chr (drvthis, x, y, '-');

		len -= LCD_DEFAULT_CELLWIDTH;
	}

//  move(y-1, x-1);
//  hline(0, len/lcd.cellwid);
}

/////////////////////////////////////////////////////////////////
// Sets character 0 to an icon...
//
MODULE_EXPORT void
curses_drv_icon (Driver *drvthis, int which, char dest)
{
	if (dest == 0)
		switch (which) {
			case 0:
				icon_char = '+';
				break;
			case 1:
				icon_char = '*';
				break;
			default:
				icon_char = PAD;
				break;
		}

}

/////////////////////////////////////////////////////////////
// Does the heartbeat...
//
MODULE_EXPORT void
curses_drv_heartbeat (Driver *drvthis, int type)
{
	static int timer = 0;
	int whichIcon;
	static int saved_type = HEARTBEAT_ON;

	if (type)
		saved_type = type;

	if (type == HEARTBEAT_ON) {
		// Set this to pulsate like a real heart beat...
		whichIcon = (! ((timer + 4) & 5));

		// This defines a custom character EVERY time...
		// not efficient... is this necessary?
		curses_drv_icon (drvthis, whichIcon, 0);

		// Put character on screen...
		curses_drv_chr (drvthis, width, 1, 0);

		// change display...
		curses_drv_flush (drvthis);
	}

	timer++;
	timer &= 0x0f;
}

//////////////////////////////////////////////////////////////////
// Flushes all output to the lcd...
//
MODULE_EXPORT void
curses_drv_flush (Driver *drvthis)
{
	int c;

	if ((c = getch ()) != ERR)
		if (c == 0x0C) {
			curses_drv_restore_screen(drvthis);
			ungetch (c);
		}

	curses_drv_wborder (lcd_win);
	wrefresh (lcd_win);
}


MODULE_EXPORT char
curses_drv_getkey (Driver *drvthis)
{
	int i;

	i = getch ();

	switch(i) {
		case 0x0C:
			curses_drv_restore_screen(drvthis);
			return 0;
			break;
		case KEY_LEFT:
			return 'D';
			break;
		case KEY_UP:
			return 'B';
			break;
		case KEY_DOWN:
			return 'C';
			break;
		case KEY_RIGHT:
			return 'A';
			break;
		case ERR:
			return 0;
			break;
		default:
			return i;
			break;
	}
}

void
curses_drv_restore_screen (Driver *drvthis) {

	erase();
	refresh();
#ifdef CURSES_HAS_REDRAWWIN
	redrawwin(lcd_win);
#endif
	wrefresh(lcd_win);
}
