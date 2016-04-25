#include <gtk/gtk.h>
#include <gdk/gdkscreen.h>
#include <time.h>
#include <math.h>
#include <stdint.h>

#define WINDOW_WIDTH 1280
#define WINDOW_HEIGHT 800

#define FONT_SIZE 64.0
#define GRID_SIZE FONT_SIZE*1.5

char* weekday_names[7] =
{
	"Sunday",
	"Monday",
	"Tuesday",
	"Wednesday",
	"Thursday",
	"Friday",
	"Saturday"
};

char* month_names[12] =
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

GtkWidget* window;
GdkPixbuf* background_pixbuf;

int tick = 0;

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

/*
 * This function draws the time and date strings, I have a feeling thought that it will soon draw everything
 */
void draw_timestring( cairo_t* cr )
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
    
    cairo_set_font_face(cr,cairo_toy_font_face_create("Source Sans Pro ExtraLight",CAIRO_FONT_SLANT_NORMAL,CAIRO_FONT_WEIGHT_NORMAL));
    cairo_text_extents_t extents;
    double text_x, text_y;
    double time_width = 0;


    cairo_set_source_rgba( cr, 0.0, 0.0, 0.0, 0.75 );
    cairo_rectangle( cr, GRID_SIZE, (WINDOW_HEIGHT*0.5) - GRID_SIZE * 3.0, WINDOW_WIDTH - (GRID_SIZE * 2.0), GRID_SIZE * 3.5 );
    cairo_fill( cr );

    cairo_set_source_rgba( cr, 1.0, 1.0, 1.0, 0.75 );

    char timestring[64];
    char ampmstring[6];
    char datestring[64];
    tick = 0;
    sprintf( timestring, "%i%c%02i", hour, tick?' ':':', minute );
    cairo_set_font_size( cr, FONT_SIZE * 6.0 );
    cairo_text_extents( cr, timestring, &extents );
    time_width = extents.x_advance;

    sprintf(ampmstring,"%s",ampm);
    cairo_set_font_size(cr, FONT_SIZE * 3.0 );
    cairo_text_extents( cr, ampmstring, &extents );
    time_width += extents.width;

    text_x = 640.0 - time_width / 2.0;
    text_y = WINDOW_HEIGHT * 0.5;

    //Hour and minute
    cairo_set_font_size(cr, FONT_SIZE * 6.0 );
    cairo_move_to( cr, text_x, text_y );
    cairo_text_path(cr,timestring);
    
    //AM/PM
    text_x += time_width - extents.width;
    cairo_set_font_size( cr, FONT_SIZE * 3.0 );
    cairo_move_to( cr, text_x, text_y );
    cairo_text_path(cr,ampmstring);

    text_y += 64.0 * 1.5;
    
    //Weekday string
    cairo_set_font_size( cr, FONT_SIZE );
    sprintf(datestring,"%s, %s %i, %i",weekday_names[timeinfo->tm_wday],month_names[timeinfo->tm_mon],timeinfo->tm_mday,timeinfo->tm_year+1900);
    cairo_text_extents(cr,datestring,&extents);
    text_x = 640 - extents.width /  2.0;
    cairo_move_to( cr, text_x, text_y );
    cairo_text_path(cr,datestring);

    cairo_fill(cr);
}


gboolean draw( GtkWidget* widget, cairo_t* cr, gpointer user )
{
    int width, height;
    gtk_window_get_size(GTK_WINDOW(widget), &width, &height);

    gdk_cairo_set_source_pixbuf( cr, background_pixbuf, 0, 0 );
    cairo_paint( cr );
    
    cairo_set_operator (cr, CAIRO_OPERATOR_OVER);
    cairo_set_source_rgba (cr, 1.0, 1.0, 1.0, 0.75);
    draw_timestring( cr );
    
    return FALSE;
}

gboolean refresh_clock(gpointer data)
{
    tick = !tick;
    gtk_widget_queue_draw(window);
    return TRUE;
}

/* Click Event Handler */
static void clicked(GtkWindow *win, GdkEventButton *event, gpointer user_data)
{
    gtk_widget_queue_draw(GTK_WIDGET(win));
}

int main( int argc, char **argv )
{
    gtk_init( &argc, &argv );

    //Get screen width
    GdkScreen* screen = gdk_screen_get_default();
    int screen_width = gdk_screen_get_width( screen );
    int screen_height = gdk_screen_get_height( screen );

    /*
     * Load test pixbuf
     * TODO: Make this so it randomly selects images from a directory based on current time of day/sun state
     */
    load_background_pixbuf( "test.jpg" );

    window = gtk_window_new( GTK_WINDOW_TOPLEVEL );
    gtk_window_set_title(GTK_WINDOW(window), "Alpha Demo");
    g_signal_connect(G_OBJECT(window), "delete-event", gtk_main_quit, NULL);

    gtk_widget_set_app_paintable(window, TRUE);
    g_signal_connect( G_OBJECT(window), "draw", G_CALLBACK( draw ), NULL );

    gtk_window_set_decorated(GTK_WINDOW(window), FALSE);
    gtk_widget_set_size_request(window,WINDOW_WIDTH,WINDOW_HEIGHT);
    gtk_widget_add_events(window, GDK_BUTTON_PRESS_MASK);
    gtk_window_move(GTK_WINDOW(window),screen_width-WINDOW_WIDTH* 0.25,screen_height-WINDOW_HEIGHT* 0.25);
    gtk_window_set_type_hint(GTK_WINDOW(window),GDK_WINDOW_TYPE_HINT_DIALOG);
    g_signal_connect(G_OBJECT(window), "button-press-event", G_CALLBACK(clicked), NULL);

    g_timeout_add_seconds(1,refresh_clock,NULL);

    gtk_widget_show_all(window);
    gtk_main();

    return 0;
}
