#include <locale.h>
#include <time.h>
#include <curses.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <math.h>
#include <chafa.h>

#define RING_R  1.0f
#define RING_G  1.0f
#define RING_B  1.0f

#define INNER_R 1.0f
#define INNER_G 1.0f
#define INNER_B 0.0f

#define OUTER_R 1.0f
#define OUTER_G 0.0f
#define OUTER_B 1.0f

int          g_frame_counter = 0;
float        g_time_delta    = 0.0f;
float        g_time          = 0.0f;
int          g_mouse_x       = 0;
int          g_mouse_y       = 0;
int          g_mouse_bstate  = 0;
uint32_t*    g_color_buffer  = NULL;
ChafaCanvas* g_canvas        = NULL;
int          g_pixmap_width  = 0;
int          g_pixmap_height = 0;


typedef struct timespec timespec;

static ChafaCanvasMode
detect_canvas_mode (void)
{
    /* COLORS is a global variable defined by ncurses. It depends on termcap
     * for the terminal specified in TERM. In order to test the various modes, you
     * could try running this program with either of these:
     *
     * TERM=xterm
     * TERM=xterm-16color
     * TERM=xterm-256color
     * TERM=xterm-direct
     */
    return COLORS >= (1 << 24) ? CHAFA_CANVAS_MODE_TRUECOLOR
        : COLORS >= (1 << 8) ? CHAFA_CANVAS_MODE_INDEXED_240
        : COLORS >= (1 << 4) ? CHAFA_CANVAS_MODE_INDEXED_16
        : COLORS >= (1 << 3) ? CHAFA_CANVAS_MODE_INDEXED_8
        : CHAFA_CANVAS_MODE_FGBG;
}

static ChafaCanvas *
create_canvas (void)
{
    ChafaSymbolMap *symbol_map;
    ChafaCanvasConfig *config;
    ChafaCanvas *canvas;
    ChafaCanvasMode mode = detect_canvas_mode ();

    /* Specify the symbols we want: Box drawing and block elements are both
     * useful and widely supported. */

    symbol_map = chafa_symbol_map_new ();
    chafa_symbol_map_add_by_tags (symbol_map, CHAFA_SYMBOL_TAG_SPACE);
    chafa_symbol_map_add_by_tags (symbol_map, CHAFA_SYMBOL_TAG_BLOCK);
    chafa_symbol_map_add_by_tags (symbol_map, CHAFA_SYMBOL_TAG_BORDER);

    /* Set up a configuration with the symbols and the canvas size in characters */

    config = chafa_canvas_config_new ();
    chafa_canvas_config_set_canvas_mode (config, mode);
    chafa_canvas_config_set_symbol_map (config, symbol_map);

    /* Reserve one row below canvas for status text */
    chafa_canvas_config_set_geometry (config, COLS, LINES);

    /* Apply tweaks for low-color modes */

    if (mode == CHAFA_CANVAS_MODE_INDEXED_240)
    {
        /* We get better color fidelity using DIN99d in 240-color mode.
         * This is not needed in 16-color mode because it uses an extra
         * preprocessing step instead, which usually performs better. */
        chafa_canvas_config_set_color_space (config, CHAFA_COLOR_SPACE_DIN99D);
    }

    if (mode == CHAFA_CANVAS_MODE_FGBG)
    {
        /* Enable dithering in monochromatic mode so gradients become
         * somewhat legible. */
        chafa_canvas_config_set_dither_mode (config, CHAFA_DITHER_MODE_ORDERED);
    }

    /* Create canvas */

    canvas = chafa_canvas_new (config);

    chafa_symbol_map_unref (symbol_map);
    chafa_canvas_config_unref (config);
    return canvas;
}

timespec timespec_sub(timespec start, timespec end)
{
  timespec temp;
  if ((end.tv_nsec-start.tv_nsec)<0) {
    temp.tv_sec  = end.tv_sec-start.tv_sec-1;
    temp.tv_nsec = 1000000000+end.tv_nsec-start.tv_nsec;
  } else {
    temp.tv_sec  = end.tv_sec-start.tv_sec;
    temp.tv_nsec = end.tv_nsec-start.tv_nsec;
  }
  return temp;
};

extern inline timespec timespec_add(timespec start, timespec end)
{
  int64_t sec  = start.tv_sec + end.tv_sec;
  int64_t nsec = start.tv_nsec + end.tv_nsec;
  int64_t extra_sec  = (nsec/1000000000);
  int64_t extra_nsec = extra_sec * 1000000000;
  timespec sum = { sec+extra_sec, nsec-extra_nsec};
  return sum;
}

extern inline float timespec_f( timespec time )
{
  return ((float) time.tv_sec + (time.tv_nsec / 1000000000.0f));
}

extern inline void printw_timespec(const char* title, timespec time )
{
  int64_t sec  = time.tv_sec;
  int64_t msec = time.tv_nsec / 1000000;
  int64_t usec = (time.tv_nsec - (msec*1000000)) / 1000;
  int64_t nsec = time.tv_nsec - (msec*1000000) - (usec*1000);

  printw("%s%ds %dms %dus %dns",title,(int)(sec),(int)(msec),(int)(usec),(int)(nsec));
}

#define COLOR_PALETTE_COUNT 256
//Using default palette expected by Chafa.
// See: https://github.com/hpjansson/chafa/issues/168#issuecomment-1724080130
uint32_t g_color_palette[COLOR_PALETTE_COUNT] = 
{
    /* First 16 colors; these are usually set by the terminal and can vary quite a
     * bit. We try to strike a balance. */

    0x000000, 0x800000, 0x007000, 0x707000, 0x000070, 0x700070, 0x007070, 0xc0c0c0,
    0x404040, 0xff0000, 0x00ff00, 0xffff00, 0x0000ff, 0xff00ff, 0x00ffff, 0xffffff,

    /* 240 universal colors; a 216-entry color cube followed by 24 grays */

    0x000000, 0x00005f, 0x000087, 0x0000af, 0x0000d7, 0x0000ff, 0x005f00, 0x005f5f,
    0x005f87, 0x005faf, 0x005fd7, 0x005fff, 0x008700, 0x00875f, 0x008787, 0x0087af,

    0x0087d7, 0x0087ff, 0x00af00, 0x00af5f, 0x00af87, 0x00afaf, 0x00afd7, 0x00afff,
    0x00d700, 0x00d75f, 0x00d787, 0x00d7af, 0x00d7d7, 0x00d7ff, 0x00ff00, 0x00ff5f,

    0x00ff87, 0x00ffaf, 0x00ffd7, 0x00ffff, 0x5f0000, 0x5f005f, 0x5f0087, 0x5f00af,
    0x5f00d7, 0x5f00ff, 0x5f5f00, 0x5f5f5f, 0x5f5f87, 0x5f5faf, 0x5f5fd7, 0x5f5fff,

    0x5f8700, 0x5f875f, 0x5f8787, 0x5f87af, 0x5f87d7, 0x5f87ff, 0x5faf00, 0x5faf5f,
    0x5faf87, 0x5fafaf, 0x5fafd7, 0x5fafff, 0x5fd700, 0x5fd75f, 0x5fd787, 0x5fd7af,

    0x5fd7d7, 0x5fd7ff, 0x5fff00, 0x5fff5f, 0x5fff87, 0x5fffaf, 0x5fffd7, 0x5fffff,
    0x870000, 0x87005f, 0x870087, 0x8700af, 0x8700d7, 0x8700ff, 0x875f00, 0x875f5f,

    0x875f87, 0x875faf, 0x875fd7, 0x875fff, 0x878700, 0x87875f, 0x878787, 0x8787af,
    0x8787d7, 0x8787ff, 0x87af00, 0x87af5f, 0x87af87, 0x87afaf, 0x87afd7, 0x87afff,

    0x87d700, 0x87d75f, 0x87d787, 0x87d7af, 0x87d7d7, 0x87d7ff, 0x87ff00, 0x87ff5f,
    0x87ff87, 0x87ffaf, 0x87ffd7, 0x87ffff, 0xaf0000, 0xaf005f, 0xaf0087, 0xaf00af,

    0xaf00d7, 0xaf00ff, 0xaf5f00, 0xaf5f5f, 0xaf5f87, 0xaf5faf, 0xaf5fd7, 0xaf5fff,
    0xaf8700, 0xaf875f, 0xaf8787, 0xaf87af, 0xaf87d7, 0xaf87ff, 0xafaf00, 0xafaf5f,

    0xafaf87, 0xafafaf, 0xafafd7, 0xafafff, 0xafd700, 0xafd75f, 0xafd787, 0xafd7af,
    0xafd7d7, 0xafd7ff, 0xafff00, 0xafff5f, 0xafff87, 0xafffaf, 0xafffd7, 0xafffff,

    0xd70000, 0xd7005f, 0xd70087, 0xd700af, 0xd700d7, 0xd700ff, 0xd75f00, 0xd75f5f,
    0xd75f87, 0xd75faf, 0xd75fd7, 0xd75fff, 0xd78700, 0xd7875f, 0xd78787, 0xd787af,

    0xd787d7, 0xd787ff, 0xd7af00, 0xd7af5f, 0xd7af87, 0xd7afaf, 0xd7afd7, 0xd7afff,
    0xd7d700, 0xd7d75f, 0xd7d787, 0xd7d7af, 0xd7d7d7, 0xd7d7ff, 0xd7ff00, 0xd7ff5f,

    0xd7ff87, 0xd7ffaf, 0xd7ffd7, 0xd7ffff, 0xff0000, 0xff005f, 0xff0087, 0xff00af,
    0xff00d7, 0xff00ff, 0xff5f00, 0xff5f5f, 0xff5f87, 0xff5faf, 0xff5fd7, 0xff5fff,

    0xff8700, 0xff875f, 0xff8787, 0xff87af, 0xff87d7, 0xff87ff, 0xffaf00, 0xffaf5f,
    0xffaf87, 0xffafaf, 0xffafd7, 0xffafff, 0xffd700, 0xffd75f, 0xffd787, 0xffd7af,

    0xffd7d7, 0xffd7ff, 0xffff00, 0xffff5f, 0xffff87, 0xffffaf, 0xffffd7, 0xffffff,
    0x080808, 0x121212, 0x1c1c1c, 0x262626, 0x303030, 0x3a3a3a, 0x444444, 0x4e4e4e,

    0x585858, 0x626262, 0x6c6c6c, 0x767676, 0x808080, 0x8a8a8a, 0x949494, 0x9e9e9e,
    0xa8a8a8, 0xb2b2b2, 0xbcbcbc, 0xc6c6c6, 0xd0d0d0, 0xdadada, 0xe4e4e4, 0xeeeeee
};

void
init_color_rgb( int id, uint32_t rgb )
{
  uint32_t b = ((rgb & 0xff) * 1000 ) / 255;
  uint32_t g = (((rgb >> 8) & 0xff) * 1000 ) / 255;
  uint32_t r = (((rgb >> 16) & 0xff) * 1000 ) / 255;

  init_color(id, r, g, b);
  init_pair(id, id, COLOR_BLACK);
}

void
init(void)
{
  // enable support UTF-8
  // confirm available locales with 'locale -a'
  setlocale(LC_ALL, "C.utf8");

  // newterm(getenv("TERM"), stdout, stdin);
  // return stdscr;
  initscr();

  // Disables line buffering and erase/kill character-processing
  cbreak();

  // Characters typed by the user not echoed by getch 
  noecho();

  // The nodelay option causes getch to be a non-blocking call. 
  // If no input is ready, getch returns ERR.
  nodelay(stdscr, TRUE);

  // Enable keypad keys (including arrow keys)
  keypad(stdscr, TRUE);

  // The number of milliseconds to wait after reading an escape character
  // Nonzero delays will block thread while curses waits for escape code.
  set_escdelay(0);

  // Enable mouse event updates (button presses) and position updates.
  mousemask(ALL_MOUSE_EVENTS | REPORT_MOUSE_POSITION, NULL);

  // Except apparently sometimes for xterm.
  // See: sylt/ncurses_mouse_movement.c
  // And: xterm SET_ANY_EVENT_MOUSE
  // https://www.xfree86.org/current/ctlseqs.html#Mouse%20Tracking
  printf("\033[?1003h\n");
  fflush(stdout);

  // The number of milliseconds to wait after a mouse down event.
  // Nonzero delays will block thread while curses waits for mouse-up to
  // resolve to click events. Zero disables click resolution.
  mouseinterval(0);

  // Enable color support.
  start_color();

  // Initialize palette
  for (int i=0;i<COLOR_PALETTE_COUNT;i++)
  {
    init_color_rgb(i,g_color_palette[i]);
  }
  use_default_colors ();
}

void
quit(void)
{
  // Disable mouse movement events, as l = low
  printf("\033[?1003l\n");

  // restore terminal
  endwin();

  free(g_color_buffer);

  exit(0);
}

void
drain_events(void)
{
  int event_key = 0;
  do
  {
    event_key = getch();
    switch (event_key)
    {
      case KEY_LEFT:
      break;
      case KEY_RIGHT:
      break;
      case KEY_UP:
      break;
      case KEY_DOWN:
      break;
      case 'q':
        quit();
      break;
      case KEY_MOUSE:
      {
        MEVENT event;
        if (getmouse(&event) == OK)
        {
          g_mouse_x      = event.x;
          g_mouse_y      = event.y;
          g_mouse_bstate = event.bstate;

          if ( g_mouse_bstate & BUTTON1_RELEASED )
          {
          }
          if ( g_mouse_bstate & BUTTON1_PRESSED )
          {
          }
        }
      }
      break;
      case KEY_RESIZE:
      {
        g_pixmap_width  = COLS * 2;
        g_pixmap_height = LINES * 4;
        g_color_buffer  = (uint32_t*)realloc( g_color_buffer, g_pixmap_width * g_pixmap_height * 4 );

        chafa_canvas_unref(g_canvas);
        g_canvas = create_canvas();
      }
      break;
    }
  } while (event_key != ERR);
}

void
clear_color_buffer( void )
{
  memset( g_color_buffer, 0, g_pixmap_width * g_pixmap_height * 4 );
}

timespec
chafa_render( void )
{
  timespec end_time;
  timespec start_time;

  clock_gettime(CLOCK_REALTIME, &start_time);

  chafa_canvas_draw_all_pixels (g_canvas,
                                CHAFA_PIXEL_RGBA8_UNASSOCIATED,
                                (uint8_t*)g_color_buffer,
                                g_pixmap_width,
                                g_pixmap_height,
                                g_pixmap_width * 4 );

  clock_gettime(CLOCK_REALTIME, &end_time);
  return timespec_sub( start_time, end_time );
}

timespec
present( void )
{
  timespec end_time;
  timespec start_time;

  clock_gettime(CLOCK_REALTIME, &start_time);

  ChafaCanvasMode mode = detect_canvas_mode ();
  int pair = 256;  /* Reserve lower pairs for application in direct-color mode */
  int x, y;

  for (y = 0; y < LINES; y++)
  {
      for (x = 0; x < COLS; x++)
      {
          wchar_t wc [2];
          cchar_t cch;
          int fg, bg;

          /* wchar_t is 32-bit in glibc, but this may not work on e.g. Windows */
          wc [0] = chafa_canvas_get_char_at (g_canvas, x, y);
          wc [1] = 0;

          if (mode == CHAFA_CANVAS_MODE_TRUECOLOR)
          {
              chafa_canvas_get_colors_at (g_canvas, x, y, &fg, &bg);
              init_extended_pair (pair, fg, bg);
          }
          else if (mode == CHAFA_CANVAS_MODE_FGBG)
          {
              pair = 0;
          }
          else
          {
              /* In indexed color mode, we've probably got enough pairs
               * to just let ncurses allocate and reuse as needed. */
              chafa_canvas_get_raw_colors_at (g_canvas, x, y, &fg, &bg);
              pair = alloc_pair (fg, bg);
          }

          setcchar (&cch, wc, A_NORMAL, -1, &pair);
          mvadd_wch (y, x, &cch);
          pair++;
      }
  }

  clock_gettime(CLOCK_REALTIME, &end_time);
  return timespec_sub( start_time, end_time );
}

// see: https://iquilezles.org/articles/distfunctions2d
float sdCircle( float x, float y, float r )
{
  return sqrtf((x*x)+(y*y)) - r;
}

float clamp( float x, float min_x, float max_x )
{
  return fminf(fmaxf(x, min_x), max_x);
}

// see: https://thebookofshaders.com/glossary/?search=smoothstep
float smoothstep( float edge0, float edge1, float x )
{
  float t = clamp((x - edge0) / (edge1 - edge0), 0.0f, 1.0f);
  return t * t * (3.0f - 2.0f * t);
}

// see: https://thebookofshaders.com/glossary/?search=mix
float mix( float x, float y, float a )
{
  return x*(1.0f-a)+y*a;
}

timespec
draw( void )
{
  timespec end_time;
  timespec start_time;

  clock_gettime(CLOCK_REALTIME, &start_time);

  int cx = g_pixmap_width/2;
  int cy = g_pixmap_height/2;

  for (int y=0;y<g_pixmap_height;y++)
  {
    for (int x=0;x<g_pixmap_width;x++)
    {
      // see: https://www.shadertoy.com/view/3tfXWN
      int   px = x-(g_mouse_x * 2);
      int   py = y-(g_mouse_y * 4);
      float d  = sdCircle( (float)px, (float)py, (float)(g_pixmap_width/8) ) / (float)(g_pixmap_width);

      float w  = clamp(cosf( 140.0*d + 7.0*g_time ),0.0f, 1.0f);
      float r  = w * (( d < 0.0f ) ? INNER_R : OUTER_R);
      float g  = w * (( d < 0.0f ) ? INNER_G : OUTER_G);
      float b  = w * (( d < 0.0f ) ? INNER_B : OUTER_B);

      float s = 1.0f-smoothstep( 0.0f, 0.02f, fabsf(d) );

      r = mix( r, RING_R, s );
      g = mix( g, RING_G, s );
      b = mix( b, RING_B, s );

      uint32_t r8    = (uint32_t)(r*255.0f);
      uint32_t g8    = (uint32_t)(g*255.0f);
      uint32_t b8    = (uint32_t)(b*255.0f);
      uint32_t rgb24 = r8 | (g8 << 8) | (b8 << 16);

      g_color_buffer[ (y*g_pixmap_width) + x ] = rgb24 | 0xff000000;
    }
  }

  clock_gettime(CLOCK_REALTIME, &end_time);
  return timespec_sub( start_time, end_time );
}

int
main( void )
{
  init();

  timespec frame_time;
  timespec sleep_time;
  timespec remaining_time;
  timespec target_time = { 0, 16666666 };

  g_pixmap_width  = COLS * 2;
  g_pixmap_height = LINES * 4;
  g_color_buffer  = (uint32_t*)malloc( g_pixmap_width * g_pixmap_height * 4 );
  g_canvas        = create_canvas();

  while (1)
  {
    timespec start_time;
    clock_gettime(CLOCK_REALTIME, &start_time);

    printf("\033P=1s\033\\\n");

    //
    // update
    //

    drain_events();
    clear_color_buffer();

    timespec draw_time         = draw();
    timespec chafa_render_time = chafa_render();
    timespec present_time      = present();

    // draw mouse cursor
    move(g_mouse_y,g_mouse_x);
    attron( COLOR_PAIR(COLOR_WHITE) );
    addwstr(L"\u25fc");

    // text overlay

    int line = 0;
    attron( COLOR_PAIR(COLOR_WHITE) );
    move(line++,0); printw("character_resolution: %d x %d",COLS,LINES);
    move(line++,0); printw("time:                 %f",g_time);
    move(line++,0); printw("frame_counter:        %d",g_frame_counter);
    move(line++,0); printw_timespec("draw_time:            ", draw_time);
    move(line++,0); printw_timespec("chafa_render_time:    ", chafa_render_time);
    move(line++,0); printw_timespec("present_time:         ", present_time);
    move(line++,0); printw_timespec("sleep_time:           ", sleep_time);
    move(line++,0); printw("mouse                 x:%d y:%d bstate:0x%08x", g_mouse_x, g_mouse_y, g_mouse_bstate);

    // print palette
    for (int i=0;i<COLOR_PALETTE_COUNT;i++)
    {
      move(line+(i>>4),i&0x0f);
      attron( COLOR_PAIR(i) );
      addwstr(L"\u25fc");
    }
    line += COLOR_PALETTE_COUNT >> 4;

    // 
    // refresh display and sleep
    //

    g_frame_counter++;
    refresh();

    printf("\033P=2s\033\\\n");

    timespec end_time;
    clock_gettime(CLOCK_REALTIME, &end_time);

    frame_time     = timespec_sub( start_time, end_time );
    remaining_time = timespec_sub( frame_time, target_time );

    nanosleep( &remaining_time, NULL );

    timespec end_sleep_time;
    clock_gettime(CLOCK_REALTIME, &end_sleep_time);
    sleep_time = timespec_sub( end_time, end_sleep_time );

    timespec delta_time  = timespec_add( frame_time, sleep_time );
    g_time_delta         = timespec_f( delta_time );
    g_time              += g_time_delta;
  }    
  
  return 0;
}
