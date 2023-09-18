#include <locale.h>
#include <time.h>
#include <curses.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <math.h>
#include <chafa.h>

// REPRO: Change to any non-gray color
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
uint32_t g_color_palette[COLOR_PALETTE_COUNT] = 
{
  // xterm default colors.
  0x000000, // COLOR_BLACK
  0xcd0000, // COLOR_BLUE
  0x00cd00, // COLOR_GREEN
  0xcdcd00, // COLOR_CYAN
  0x0000cd, // COLOR_RED
  0xcd00cd, // COLOR_MAGENTA
  0x00cdcd, // COLOR_YELLOW
  0xe5e5e5, // COLOR_WHITE
  0x4d4d4d, // HI COLOR_BLACK
  0xff0000, // HI COLOR_BLUE
  0x00ff00, // HI COLOR_GREEN
  0xffff00, // HI COLOR_CYAN
  0x0000ff, // HI COLOR_RED
  0xff00ff, // HI COLOR_MAGENTA
  0x00ffff, // HI COLOR_YELLOW
  0xffffff, // HI COLOR_WHITE

  // Netscape palette
  0x000000, 0x000033, 0x000066, 0x000099, 0x0000CC, 0x0000FF, 
  0x003300, 0x003333, 0x003366, 0x003399, 0x0033CC, 0x0033FF, 
  0x006600, 0x006633, 0x006666, 0x006699, 0x0066CC, 0x0066FF, 
  0x009900, 0x009933, 0x009966, 0x009999, 0x0099CC, 0x0099FF, 
  0x00CC00, 0x00CC33, 0x00CC66, 0x00CC99, 0x00CCCC, 0x00CCFF, 
  0x00FF00, 0x00FF33, 0x00FF66, 0x00FF99, 0x00FFCC, 0x00FFFF, 
  0x330000, 0x330033, 0x330066, 0x330099, 0x3300CC, 0x3300FF, 
  0x333300, 0x333333, 0x333366, 0x333399, 0x3333CC, 0x3333FF, 
  0x336600, 0x336633, 0x336666, 0x336699, 0x3366CC, 0x3366FF, 
  0x339900, 0x339933, 0x339966, 0x339999, 0x3399CC, 0x3399FF, 
  0x33CC00, 0x33CC33, 0x33CC66, 0x33CC99, 0x33CCCC, 0x33CCFF, 
  0x33FF00, 0x33FF33, 0x33FF66, 0x33FF99, 0x33FFCC, 0x33FFFF, 
  0x660000, 0x660033, 0x660066, 0x660099, 0x6600CC, 0x6600FF, 
  0x663300, 0x663333, 0x663366, 0x663399, 0x6633CC, 0x6633FF, 
  0x666600, 0x666633, 0x666666, 0x666699, 0x6666CC, 0x6666FF, 
  0x669900, 0x669933, 0x669966, 0x669999, 0x6699CC, 0x6699FF, 
  0x66CC00, 0x66CC33, 0x66CC66, 0x66CC99, 0x66CCCC, 0x66CCFF, 
  0x66FF00, 0x66FF33, 0x66FF66, 0x66FF99, 0x66FFCC, 0x66FFFF, 
  0x990000, 0x990033, 0x990066, 0x990099, 0x9900CC, 0x9900FF, 
  0x993300, 0x993333, 0x993366, 0x993399, 0x9933CC, 0x9933FF, 
  0x996600, 0x996633, 0x996666, 0x996699, 0x9966CC, 0x9966FF, 
  0x999900, 0x999933, 0x999966, 0x999999, 0x9999CC, 0x9999FF, 
  0x99CC00, 0x99CC33, 0x99CC66, 0x99CC99, 0x99CCCC, 0x99CCFF, 
  0x99FF00, 0x99FF33, 0x99FF66, 0x99FF99, 0x99FFCC, 0x99FFFF, 
  0xCC0000, 0xCC0033, 0xCC0066, 0xCC0099, 0xCC00CC, 0xCC00FF, 
  0xCC3300, 0xCC3333, 0xCC3366, 0xCC3399, 0xCC33CC, 0xCC33FF, 
  0xCC6600, 0xCC6633, 0xCC6666, 0xCC6699, 0xCC66CC, 0xCC66FF, 
  0xCC9900, 0xCC9933, 0xCC9966, 0xCC9999, 0xCC99CC, 0xCC99FF, 
  0xCCCC00, 0xCCCC33, 0xCCCC66, 0xCCCC99, 0xCCCCCC, 0xCCCCFF, 
  0xCCFF00, 0xCCFF33, 0xCCFF66, 0xCCFF99, 0xCCFFCC, 0xCCFFFF, 
  0xFF0000, 0xFF0033, 0xFF0066, 0xFF0099, 0xFF00CC, 0xFF00FF, 
  0xFF3300, 0xFF3333, 0xFF3366, 0xFF3399, 0xFF33CC, 0xFF33FF, 
  0xFF6600, 0xFF6633, 0xFF6666, 0xFF6699, 0xFF66CC, 0xFF66FF, 
  0xFF9900, 0xFF9933, 0xFF9966, 0xFF9999, 0xFF99CC, 0xFF99FF, 
  0xFFCC00, 0xFFCC33, 0xFFCC66, 0xFFCC99, 0xFFCCCC, 0xFFCCFF, 
  0xFFFF00, 0xFFFF33, 0xFFFF66, 0xFFFF99, 0xFFFFCC, 0xFFFFFF, 

  // Unused
  0x000000, 0x000000, 0x000000, 0x000000,
  0x000000, 0x000000, 0x000000, 0x000000,
  0x000000, 0x000000, 0x000000, 0x000000,
  0x000000, 0x000000, 0x000000, 0x000000,
  0x000000, 0x000000, 0x000000, 0x000000,
  0x000000, 0x000000, 0x000000, 0x000000,
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
