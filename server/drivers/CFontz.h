#ifndef CFONTZ_H
#define CFONTZ_H

#include "lcd.h"

#define DEFAULT_CELL_WIDTH 6
#define DEFAULT_CELL_HEIGHT 8
#define DEFAULT_CONTRAST 560
#define DEFAULT_DEVICE "/dev/lcd"
#define DEFAULT_SPEED B9600
#define DEFAULT_BRIGHTNESS 60
#define DEFAULT_OFFBRIGHTNESS 0
#define DEFAULT_SIZE "20x4"


              int  CFontz_init (Driver * drvthis, char *device);
MODULE_EXPORT void CFontz_close (Driver * drvthis);
MODULE_EXPORT int  CFontz_width (Driver * drvthis);
MODULE_EXPORT int  CFontz_height (Driver * drvthis);
MODULE_EXPORT void CFontz_clear (Driver * drvthis);
MODULE_EXPORT void CFontz_flush (Driver * drvthis);
MODULE_EXPORT void CFontz_string (Driver * drvthis, int x, int y, char string[]);
MODULE_EXPORT void CFontz_chr (Driver * drvthis, int x, int y, char c);

MODULE_EXPORT void CFontz_vbar (Driver * drvthis, int x, int len);
MODULE_EXPORT void CFontz_hbar (Driver * drvthis, int x, int y, int len);
MODULE_EXPORT void CFontz_num (Driver * drvthis, int x, int num);
MODULE_EXPORT void CFontz_heartbeat (Driver *drvthis, int type);
MODULE_EXPORT void CFontz_icon (Driver * drvthis, int which, char dest);

MODULE_EXPORT void CFontz_set_char (Driver * drvthis, int n, char *dat);

MODULE_EXPORT int  CFontz_get_contrast (Driver * drvthis);
MODULE_EXPORT void CFontz_set_contrast (Driver * drvthis, int contrast);
MODULE_EXPORT void CFontz_backlight (Driver * drvthis, int on);

MODULE_EXPORT void CFontz_init_vbar (Driver * drvthis);
MODULE_EXPORT void CFontz_init_hbar (Driver * drvthis);

#endif
