/*
 * vtclock.c
 *
 * Program to display a large digital clock.
 *
 * Font "stolen" from figlet
 *
 * Original code by Rob Hoeft.
 * Enhancements by Darren Embry.
 *
 * TODO: handle resize
 */

#include <ncurses.h>
#include <time.h>
#include <unistd.h>
#include <sys/time.h>
#include <sys/types.h>
#include <unistd.h>
#include <time.h>
#include <ctype.h>
#include <stdio.h>
#include <getopt.h>

#include "font0.h"
#include "font1.h"
#include "font2.h"
#include "digitalfont0.h"

#define AT_LEAST(a,b) do { if (a < b) a = b; } while(0)

#define MAKE_DIGIT_WINDOW(sw,type) \
  do { \
    if (config->type) { \
      sw = subwin(cl,config->type->digit_height,config->type->digit_width, \
                  starty + (VTCLOCK_ALIGN * (cl_height - config->type->digit_height) / 2),startx); \
      startx += config->type->digit_width; \
    } else { \
      sw = NULL; \
    } \
  } while(0)
#define MAKE_COLON_WINDOW(sw,type) \
  do { \
    if (config->type) { \
      sw = subwin(cl,config->type->digit_height,config->type->colon_width,\
                  starty + (VTCLOCK_ALIGN * (cl_height - config->type->digit_height) / 2),startx); \
      startx += config->type->colon_width; \
    } else { \
      sw = NULL; \
    } \
  } while(0)
#define DRAW_DIGIT(sw,type,digit) \
  do { \
    if (sw) { \
      vtclock_print_string(sw,0,0,config->type->digits[digit]); \
    } \
  } while(0)
#define DRAW_COLON(sw,type) \
  do { \
    if (sw) { \
      vtclock_print_string(sw,0,0,config->type->colon); \
    } \
  } while(0)

typedef struct {
  vtclock_font *hour;
  vtclock_font *minute;
  vtclock_font *second;
  vtclock_font *colon1;
  vtclock_font *colon2;
} vtclock_config;

vtclock_config vtclock_config_1 = {
  &vtclock_font_0,&vtclock_font_0,&vtclock_font_0,
  &vtclock_font_0,&vtclock_font_0
};

vtclock_config vtclock_digital_config_1 = {
  &vtclock_digital_font_0,&vtclock_digital_font_0,&vtclock_digital_font_0,
  &vtclock_digital_font_0,&vtclock_digital_font_0
};

vtclock_config vtclock_config_2 = {
  &vtclock_font_1,&vtclock_font_1,&vtclock_font_2,
  &vtclock_font_1,NULL
};

/* 0 = top, 1 = middle, 2 = bottom */
#define VTCLOCK_ALIGN 0

/* The vtclock_inverse code is buggy right now. */
static int vtclock_inverse = 0;

void
mydelay()
     /* sleep until second changes.  does this by polling every
      * 1/10 second.  close enough for government work. */
{
  static struct timeval timeout;
  static time_t prevsecs = (time_t)0;
  time_t secs;
  while (1) {
    time(&secs);
    if (prevsecs != secs) {
      prevsecs = secs;
      return;
    }
    timeout.tv_sec = 0;
    timeout.tv_usec = 100000;   /* 0.1 secs */
    select(0,NULL,NULL,NULL,&timeout);
    prevsecs = secs;
  }
}

void
vtclock_print_string(WINDOW *win, int y, int x,
                     char *str)
{
  if (vtclock_inverse)
    {
      char *p;
      mvwin(win,y,x);
      for (p = str; *p; ++p) {
        if (iscntrl(*p)) {
          waddch(win,*p);
        } else {
          waddch(win,' '|(isspace(*p)?A_NORMAL:A_REVERSE));
        }
      }
    }
  else
    {
      mvwprintw(win,y,x,str);
    }
}

void
usage() {
  fprintf(stderr,
          "usage: vtclock [option ...]\n"
          "  -h  help\n"
          "  -b  turn bouncing on (default)\n"
          "  -B  turn bouncing off\n"
          "  -d  # seconds between each bouncing step (default 30)\n"
          "  -1, -2, -3  select a font\n"
	  "  -v  use inverse video for character drawing\n"
	  "  -V  turn off inverse video\n"
          );
}

int
main(int argc, char **argv) {
  WINDOW *cl;                   /* used to draw the clock */
  WINDOW *h1, *h2, *m1, *m2, *s1, *s2, *c1, *c2;
                                /* subcomponents of cl */
  WINDOW *cld;                  /* used to erase the clock */

  vtclock_config *config = &vtclock_config_2;

  int cl_height, cl_width;
  int y, x;                     /* position */
  int startx, starty;           /* for placing sub-windows */
  int updown, leftright;        /* dy, dx */
  int futurex, futurey;         /* temp. for bounds checking */
  int waitfor = 0;              /* bouncing-related counter */

  time_t t_time;
  struct tm *tm_time;           /* extract HH:MM:SS from here */

  int vtclock_bounce = 1;
  int vtclock_bounce_delay = 30;

  {
    int ch;
    extern char *optarg;
    extern int optind;
    extern int optopt;
    extern int opterr;
    opterr = 1;
    optind = 1;
    while ((ch = getopt(argc, argv, "hbBd:123vV")) != -1) {

      switch (ch) {
      case 'h':
        usage();
        exit(0);
      case 'b':
        vtclock_bounce = 1;
        break;
      case 'B':
        vtclock_bounce = 0;
        break;
      case 'd':
        vtclock_bounce_delay = atoi(optarg);
        break;
      case '1':
        config = &vtclock_config_2;
        break;
      case '2':
        config = &vtclock_config_1;
        break;
      case '3':
        config = &vtclock_digital_config_1;
        break;
      case 'v':
	vtclock_inverse = 1;
	break;
      case 'V':
	vtclock_inverse = 0;
	break;
      case '?':
      default:
        usage();
        exit(2);
      }

    }
  }

  initscr();

  cl_height = 0;
  if (config->hour)   AT_LEAST(cl_height,config->hour->digit_height);
  if (config->minute) AT_LEAST(cl_height,config->minute->digit_height);
  if (config->second) AT_LEAST(cl_height,config->second->digit_height);
  if (config->colon1) AT_LEAST(cl_height,config->colon1->digit_height);
  if (config->colon2) AT_LEAST(cl_height,config->colon2->digit_height);

  cl_width
    = (config->hour   ? config->hour->digit_width * 2 : 0)
    + (config->minute ? config->minute->digit_width * 2 : 0)
    + (config->second ? config->second->digit_width * 2 : 0)
    + (config->colon1 ? config->colon1->colon_width : 0)
    + (config->colon2 ? config->colon2->colon_width : 0);

  if ((LINES < cl_height) || (COLS < cl_width)) {
    endwin();
    fprintf(stderr,"(LINES=%d COLS=%d) screen too small!\n",
            LINES,COLS);
    exit(3);
  }

  y = (LINES - cl_height) / 2;
  x = (COLS - cl_width) / 2;

  startx = x;
  starty = y;

  updown = (LINES > cl_height) ? 1 : 0;
  leftright = (COLS > cl_width) ? 1 : 0;

  cl  = newwin(cl_height, cl_width, y, x);
  cld = newwin(cl_height, cl_width, y, x);

  MAKE_DIGIT_WINDOW(h1,hour);
  MAKE_DIGIT_WINDOW(h2,hour);
  MAKE_COLON_WINDOW(c1,colon1);
  MAKE_DIGIT_WINDOW(m1,minute);
  MAKE_DIGIT_WINDOW(m2,minute);
  MAKE_COLON_WINDOW(c2,colon2);
  MAKE_DIGIT_WINDOW(s1,second);
  MAKE_DIGIT_WINDOW(s2,second);

  curs_set(0);

  while (1) {
    time(&t_time);
    tm_time = localtime(&t_time);

    DRAW_DIGIT(h1,hour,tm_time->tm_hour / 10);
    DRAW_DIGIT(h2,hour,tm_time->tm_hour % 10);
    DRAW_DIGIT(m1,minute,tm_time->tm_min / 10);
    DRAW_DIGIT(m2,minute,tm_time->tm_min % 10);
    DRAW_DIGIT(s1,second,tm_time->tm_sec / 10);
    DRAW_DIGIT(s2,second,tm_time->tm_sec % 10);
    DRAW_COLON(c1,colon1);
    DRAW_COLON(c2,colon2);

    if (vtclock_bounce) {
      if (waitfor >= vtclock_bounce_delay) {
        /* erase old */
        mvwin(cld, y, x);
        wnoutrefresh(cld);

        /* bouncy bouncy */
        futurex = x + leftright;
        futurey = y + updown;
        if ((futurex < 0) || (futurex > (COLS - cl_width))) {
          futurex = x + (leftright *= -1);
        }
        if ((futurey < 0) || (futurey > (LINES - cl_height))) {
          futurey = y + (updown *= -1);
        }
        x = futurex;
        y = futurey;

        waitfor = 0;
      }
    }

    mvwin(cl,y,x);
    wnoutrefresh(cl);
    refresh();
    mydelay();
    ++waitfor;
  }

  endwin();
  return 0;
}

