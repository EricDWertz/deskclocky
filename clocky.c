#include <gtk/gtk.h>
#include <gdk/gdkscreen.h>
#include <cairo.h>
#include <time.h>
#include <math.h>
#include <stdint.h>

/*
 * This program shows you how to create semi-transparent windows,
 * without any of the historical screenshot hacks. It requires
 * a modern system, with a compositing manager. I use xcompmgr
 * and the nvidia drivers with RenderAccel, and it works well.
 *
 * I'll take you through each step as we go. Minimal GTK+ knowledge is
 * assumed.
 */

#define UI_SCALE 1.75
 
#define WINDOW_WIDTH 1280
#define WINDOW_HEIGHT 800

#define CLOCK_X WINDOW_WIDTH- ( 32 * UI_SCALE )
#define CLOCK_Y WINDOW_HEIGHT / 2

static void screen_changed(GtkWidget *widget, GdkScreen *old_screen, gpointer user_data);
static gboolean expose(GtkWidget *widget, GdkEventExpose *event, gpointer user_data);
static void clicked(GtkWindow *win, GdkEventButton *event, gpointer user_data);

GtkWidget *window;
cairo_surface_t* blur;

#define DISPLAY_CLOCK 0
#define DISPLAY_TIMESTRING 1

double clock_alpha=1.0f;

int displaymode=0;

char* weekday_names[7]=
{
	"Sunday",
	"Monday",
	"Tuesday",
	"Wednesday",
	"Thursday",
	"Friday",
	"Saturday"
};

char* month_names[12]=
{
	"January",
	"February",
	"March",
	"April",
	"May",
	"June",
	"July",
	"August",
	"September",
	"October",
	"November",
	"December"
};

GdkPixbuf* background_pixbuf;
void load_background_pixbuf( const char* path )
{
    GError* error=NULL;
    background_pixbuf = gdk_pixbuf_new_from_file( path, &error );

	if(!background_pixbuf) 
	{
		printf("Error loading background %s\n%s\n",error->message);
		g_error_free(error);
	}
}

int tick = 0;
gboolean refresh_clock(gpointer data)
{
    tick = !tick;
    gtk_widget_queue_draw(window);
    return TRUE;
}

gboolean update_clock_alpha(gpointer data)
{
    double fillrate=5.0; //half a second delay
    double dt=1.0/60.0;
    if(displaymode==DISPLAY_CLOCK)
    {
    	clock_alpha+=fillrate*dt;
    	if(clock_alpha>1) clock_alpha=1.0;
    	gtk_widget_queue_draw(window);
    	return clock_alpha!=1;
    }
    
    clock_alpha-=fillrate*dt;
    if(clock_alpha<0) clock_alpha=0;
    gtk_widget_queue_draw(window);
    return clock_alpha!=0;
}

#define ARRAY_LENGTH(a) (sizeof (a) / sizeof (a)[0])

/* Performs a simple 2D Gaussian blur of radius @radius on surface @surface. */
void
blur_image_surface (cairo_surface_t *surface, int radius)
{
    cairo_surface_t *tmp;
    int width, height;
    int src_stride, dst_stride;
    int x, y, z, w;
    uint8_t *src, *dst;
    uint32_t *s, *d, a, p;
    int i, j, k;
    uint8_t kernel[17];
    const int size = ARRAY_LENGTH (kernel);
    const int half = size / 2;

    if (cairo_surface_status (surface))
	return;

    width = cairo_image_surface_get_width (surface);
    height = cairo_image_surface_get_height (surface);

    switch (cairo_image_surface_get_format (surface)) {
    case CAIRO_FORMAT_A1:
    default:
	/* Don't even think about it! */
	return;

    case CAIRO_FORMAT_A8:
	/* Handle a8 surfaces by effectively unrolling the loops by a
	 * factor of 4 - this is safe since we know that stride has to be a
	 * multiple of uint32_t. */
	width /= 4;
	break;

    case CAIRO_FORMAT_RGB24:
    case CAIRO_FORMAT_ARGB32:
	break;
    }

    tmp = cairo_image_surface_create (CAIRO_FORMAT_ARGB32, width, height);
    if (cairo_surface_status (tmp))
	return;

    src = cairo_image_surface_get_data (surface);
    src_stride = cairo_image_surface_get_stride (surface);

    dst = cairo_image_surface_get_data (tmp);
    dst_stride = cairo_image_surface_get_stride (tmp);

    a = 0;
    for (i = 0; i < size; i++) {
	double f = i - half;
	a += kernel[i] = exp (- f * f / 30.0) * 80;
    }

    /* Horizontally blur from surface -> tmp */
    for (i = 0; i < height; i++) {
	s = (uint32_t *) (src + i * src_stride);
	d = (uint32_t *) (dst + i * dst_stride);
	for (j = 0; j < width; j++) {
	    if (radius < j && j < width - radius) {
		d[j] = s[j];
		continue;
	    }

	    x = y = z = w = 0;
	    for (k = 0; k < size; k++) {
		if (j - half + k < 0 || j - half + k >= width)
		    continue;

		p = s[j - half + k];

		x += ((p >> 24) & 0xff) * kernel[k];
		y += ((p >> 16) & 0xff) * kernel[k];
		z += ((p >>  8) & 0xff) * kernel[k];
		w += ((p >>  0) & 0xff) * kernel[k];
	    }
	    d[j] = (x / a << 24) | (y / a << 16) | (z / a << 8) | w / a;
	}
    }

    /* Then vertically blur from tmp -> surface */
    for (i = 0; i < height; i++) {
	s = (uint32_t *) (dst + i * dst_stride);
	d = (uint32_t *) (src + i * src_stride);
	for (j = 0; j < width; j++) {
	    if (radius <= i && i < height - radius) {
		d[j] = s[j];
		continue;
	    }

	    x = y = z = w = 0;
	    for (k = 0; k < size; k++) {
		if (i - half + k < 0 || i - half + k >= height)
		    continue;

		s = (uint32_t *) (dst + (i - half + k) * dst_stride);
		p = s[j];

		x += ((p >> 24) & 0xff) * kernel[k];
		y += ((p >> 16) & 0xff) * kernel[k];
		z += ((p >>  8) & 0xff) * kernel[k];
		w += ((p >>  0) & 0xff) * kernel[k];
	    }
	    d[j] = (x / a << 24) | (y / a << 16) | (z / a << 8) | w / a;
	}
    }

    cairo_surface_destroy (tmp);
    cairo_surface_mark_dirty (surface);
}


int main(int argc, char **argv)
{
    /* boilerplate initialization code */
    gtk_init(&argc, &argv);

    GdkScreen* screen = gdk_screen_get_default();
    int screen_width = gdk_screen_get_width( screen );
    int screen_height = gdk_screen_get_height( screen );

    load_background_pixbuf( "test.jpg" );
    
    window = gtk_window_new(GTK_WINDOW_TOPLEVEL);
    gtk_window_set_title(GTK_WINDOW(window), "Alpha Demo");
    g_signal_connect(G_OBJECT(window), "delete-event", gtk_main_quit, NULL);


    /* Tell GTK+ that we want to draw the windows background ourself.
     * If we don't do this then GTK+ will clear the window to the
     * opaque theme default color, which isn't what we want.
     */
    gtk_widget_set_app_paintable(window, TRUE);

    /* We need to handle two events ourself: "expose-event" and "screen-changed".
     *
     * The X server sends us an expose event when the window becomes
     * visible on screen. It means we need to draw the contents.  On a
     * composited desktop expose is normally only sent when the window
     * is put on the screen. On a non-composited desktop it can be
     * sent whenever the window is uncovered by another.
     *
     * The screen-changed event means the display to which we are
     * drawing changed. GTK+ supports migration of running
     * applications between X servers, which might not support the
     * same features, so we need to check each time.
     */

    g_signal_connect(G_OBJECT(window), "expose-event", G_CALLBACK(expose), NULL);
    g_signal_connect(G_OBJECT(window), "screen-changed", G_CALLBACK(screen_changed), NULL);

    /* toggle title bar on click - we add the mask to tell X we are interested in this event */
    gtk_window_set_decorated(GTK_WINDOW(window), FALSE);
    gtk_widget_set_size_request(window,WINDOW_WIDTH,WINDOW_HEIGHT);
    gtk_widget_add_events(window, GDK_BUTTON_PRESS_MASK);
    gtk_window_move(GTK_WINDOW(window),screen_width-WINDOW_WIDTH* 0.5,screen_height-WINDOW_HEIGHT* 0.5);
    gtk_window_set_type_hint(GTK_WINDOW(window),GDK_WINDOW_TYPE_HINT_DIALOG);
    g_signal_connect(G_OBJECT(window), "button-press-event", G_CALLBACK(clicked), NULL);

    g_timeout_add_seconds(1,refresh_clock,NULL);

    /* initialize for the current display */
    screen_changed(window, NULL, NULL);
    blur=cairo_image_surface_create (CAIRO_FORMAT_ARGB32, WINDOW_WIDTH, WINDOW_HEIGHT);

    /* Run the program */
    gtk_widget_show_all(window);
    gtk_main();

    return 0;
}


/* Only some X servers support alpha channels. Always have a fallback */
gboolean supports_alpha = FALSE;

static void screen_changed(GtkWidget *widget, GdkScreen *old_screen, gpointer userdata)
{
    /* To check if the display supports alpha channels, get the colormap */
    GdkScreen *screen = gtk_widget_get_screen(widget);
    GdkColormap *colormap = gdk_screen_get_rgba_colormap(screen);

    if (!colormap)
    {
        printf("Your screen does not support alpha channels!\n");
        colormap = gdk_screen_get_rgb_colormap(screen);
        supports_alpha = FALSE;
    }
    else
    {
        printf("Your screen supports alpha channels!\n");
        supports_alpha = TRUE;
    }

    /* Now we have a colormap appropriate for the screen, use it */
    gtk_widget_set_colormap(widget, colormap);
}

void draw_timestring(cairo_t* cr,int blurpass)
{
    time_t rawtime;
    struct tm * timeinfo;

    time ( &rawtime );
    timeinfo = localtime ( &rawtime );
    //tm_sec tm_min tm_hour
    int hour=timeinfo->tm_hour;
    int minute=timeinfo->tm_min;
    int second=timeinfo->tm_sec;
    //hour+=minute/60;
    char ampm[3]="am";
    if(hour>=12) 
    {
    	hour-=12;
    	ampm[0]='p';
    }
    if(hour==0)
    {
    	hour=12;
    }
    
    if(blurpass==1) cairo_translate(cr,-2.0,0);
    
    cairo_set_font_face(cr,cairo_toy_font_face_create("Source Sans Pro ExtraLight",CAIRO_FONT_SLANT_NORMAL,CAIRO_FONT_WEIGHT_NORMAL));
    cairo_text_extents_t extents;
    double text_x;
    double time_width = 0;


    cairo_set_source_rgba( cr, 0.0, 0.0, 0.0, 0.75 );
    cairo_rectangle( cr, 50, 50, 1180, 600 );
    cairo_fill( cr );

    cairo_set_source_rgba( cr, 1.0, 1.0, 1.0, 0.75 );

    char timestring[64];
    char ampmstring[6];
    char datestring[64];
    tick = 0;
    sprintf( timestring, "%i%c%02i", hour, tick?' ':':', minute );
    cairo_set_font_size( cr, 384.0 );
    cairo_text_extents( cr, timestring, &extents );
    time_width = extents.x_advance;

    sprintf(ampmstring,"%s",ampm);
    cairo_set_font_size(cr, 192.0 );
    cairo_text_extents( cr, ampmstring, &extents );
    time_width += extents.width;

    text_x = 640.0 - time_width / 2.0;

    //Hour and minute
    cairo_set_font_size(cr, 384.0 );
    cairo_move_to( cr, text_x, 400 );
    cairo_text_path(cr,timestring);
    
    //AM/PM
    text_x += time_width - extents.width;
    cairo_set_font_size( cr, 192.0 );
    cairo_move_to( cr, text_x, 400 );
    cairo_text_path(cr,ampmstring);
    
    
    //Weekday string
    cairo_set_font_size( cr, 64.0 );
    sprintf(datestring,"%s, %s %i, %i",weekday_names[timeinfo->tm_wday],month_names[timeinfo->tm_mon],timeinfo->tm_mday,timeinfo->tm_year+1900);
    cairo_text_extents(cr,datestring,&extents);
    text_x = 640 - extents.width /  2.0;
    cairo_move_to( cr, text_x, 480 );
    cairo_text_path(cr,datestring);

    if(blurpass==1) 
    {
    	cairo_set_line_width(cr, 4.0 * UI_SCALE);
    	cairo_stroke_preserve(cr);
    }
    cairo_fill(cr);
}
    

void draw_clock(cairo_t* cr,double base_width)
{
    time_t rawtime;
    struct tm * timeinfo;

    time ( &rawtime );
    timeinfo = localtime ( &rawtime );
    //tm_sec tm_min tm_hour
    double hour=timeinfo->tm_hour;
    double minute=timeinfo->tm_min;
    double second=timeinfo->tm_sec;
    hour+=minute/60;
    if(hour>12) hour-=12;
    if(hour==0) hour=12;

    double hour_angle=((12-hour)/12)*6.28+1.57;
    hour_angle=-hour_angle;

    double minute_angle=((60-minute)/60)*6.28+1.57;
    minute_angle=-minute_angle;

    double second_angle=((60-second)/60)*6.28+1.57;
    second_angle=-second_angle;

    cairo_set_line_width(cr,base_width * UI_SCALE);
    cairo_arc(cr,CLOCK_X,CLOCK_Y, 24 * UI_SCALE ,0,6.28);
    cairo_stroke(cr);   

    cairo_move_to(cr,CLOCK_X,CLOCK_Y);
    cairo_line_to(cr,CLOCK_X+cos(minute_angle)*20*UI_SCALE,CLOCK_Y+sin(minute_angle)*UI_SCALE);
    cairo_stroke(cr);

    cairo_set_line_width(cr, (base_width - 1.0) * UI_SCALE);
    cairo_move_to(cr,CLOCK_X,CLOCK_Y);
    cairo_line_to(cr,CLOCK_X+cos(second_angle)*20*UI_SCALE,CLOCK_Y+sin(second_angle)*20*UI_SCALE);
    cairo_stroke(cr);

    //HOUR HAND
    cairo_set_line_width(cr, (base_width + 1.0) * UI_SCALE );
    cairo_move_to(cr,CLOCK_X,CLOCK_Y);
    cairo_line_to(cr,CLOCK_X+cos(hour_angle)*15*UI_SCALE,CLOCK_Y+sin(hour_angle)*15*UI_SCALE);
    cairo_stroke(cr);
}

/* This is called when we need to draw the windows contents */
static gboolean expose(GtkWidget *widget, GdkEventExpose *event, gpointer userdata)
{
    cairo_t *cr = gdk_cairo_create(widget->window);

    if (supports_alpha)
        cairo_set_source_rgba (cr, 1.0, 1.0, 1.0, 0.0); /* transparent */
    else
        cairo_set_source_rgb (cr, 1.0, 1.0, 1.0); /* opaque white */

    /* draw the background */
    cairo_set_operator (cr, CAIRO_OPERATOR_SOURCE);
    cairo_paint (cr);

    int width, height;
    gtk_window_get_size(GTK_WINDOW(widget), &width, &height);

    cairo_set_source_rgba (cr, 1.0, 1.0, 1.0, clock_alpha);
    if(clock_alpha!=0) draw_clock(cr,2.0);  

    gdk_cairo_set_source_pixbuf( cr, background_pixbuf, 0, 0 );
    cairo_paint( cr );
    
    cairo_set_operator (cr, CAIRO_OPERATOR_OVER);
    cairo_set_source_rgba (cr, 1.0, 1.0, 1.0, 0.75);
    draw_timestring(cr,0);
    
    cairo_destroy(cr);

    return FALSE;
}

static void clicked(GtkWindow *win, GdkEventButton *event, gpointer user_data)
{
    if(displaymode==DISPLAY_CLOCK) 
    {
    	displaymode=DISPLAY_TIMESTRING;
    	//clock_alpha=0.99;
    }
    else 
    {
    	displaymode=DISPLAY_CLOCK;
    	//clock_alpha=0.01;
    }
    g_timeout_add(16,update_clock_alpha,NULL);
    gtk_widget_queue_draw(GTK_WIDGET(win));
}
