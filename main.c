#include <gtk/gtk.h>
#include <gdk/gdkscreen.h>
#include <time.h>
#include <math.h>
#include <stdint.h>
#include <string.h>

#include <curl/curl.h>
#include <jansson.h>

#include "config.h"

int weather_update_timer = WEATHER_REFRESH_TIMEOUT;

cairo_surface_t* temp_surface;


GtkWidget* window;
GdkPixbuf* background_pixbuf;

int tick = 0;

struct write_result
{
    char* data;
    int pos;
};

typedef struct
{
    int hour;
    int temp;
    int code;
    char ampm[4];
} weather_info;

typedef struct
{
    int hour;
    int minute;
    double sun_altitude;
    double moon_altitude;
    double moon_percent;
} astronomy_info;

//Data is for every 10 minutes so this will cover the whole day
astronomy_info astro_info[144]; 

//Hourly weather information
weather_info hourly_info[64];

struct tm * timeinfo;

static size_t write_curl_response( void* ptr, size_t size, size_t nmemb, void* stream )
{
    struct write_result *result = (struct write_result *)stream;

    if(result->pos + size * nmemb >= 1000000 - 1)
    {
        fprintf(stderr, "error: too small buffer\n");
        return 0;
    }

    memcpy(result->data + result->pos, ptr, size * nmemb);
    result->pos += size * nmemb;

    return size * nmemb;
}

char* curl_request_data( const char* url )
{
    CURL* curl = curl_easy_init();
    CURLcode status;
    char *data;
    data = (char*)malloc(1000000);

    if( !curl )
        NULL;

    struct write_result write_result = {
        .data = data,
        .pos = 0,
    };

    curl_easy_setopt( curl, CURLOPT_URL, url );
    curl_easy_setopt( curl, CURLOPT_WRITEFUNCTION, write_curl_response );
    curl_easy_setopt( curl, CURLOPT_WRITEDATA, &write_result );

    status = curl_easy_perform(curl);
    if(status != 0)
    {
        fprintf(stderr, "error: unable to request data from %s:\n", url );
        fprintf(stderr, "%s\n", curl_easy_strerror(status));
        return NULL;
    }

    curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &status);
    if(status != 200)
    {
        fprintf(stderr, "error: server responded with code %ld\n", curl_easy_strerror(status) );
        return NULL;
    }

    curl_easy_cleanup(curl);
    curl_global_cleanup();

    /* zero-terminate the result */
    data[write_result.pos] = '\0';

    return data;
}

char* weather_api_request( const char* request )
{
    char url[512];

    sprintf( url, "%s%s%s", URL_BASE, request, URL_POSTFIX );
    printf( url );

    return curl_request_data( url );
}

void update_astronomy()
{
    char url[512];
    char* data;
    char* line;
    char temp[64];

    int hour, minute;
    int i, j;

    //clear existing data
    for( i = 0; i < 144; i ++ )
    {
        astro_info[i].sun_altitude = -180.0;
        astro_info[i].moon_altitude = -180.0;
    }

    int found_table = 0;
    int found_altitude;
    // http://aa.usno.navy.mil/cgi-bin/aa_altazw.pl?form=1&body=10&year=2016&month=4&day=25&intv_mag=10&state=PA&place=Cresson
    
    //Sun data
    sprintf( url, "http://aa.usno.navy.mil/cgi-bin/aa_altazw.pl?form=1&body=10&year=%i&month=%i&day=%i&intv_mag=10&state=PA&place=Cresson",
            timeinfo->tm_year + 1900,
            timeinfo->tm_mon + 1,
            timeinfo->tm_mday );

    data = curl_request_data( url );

    line = strtok( data, "\n" );
    while( line )
    {
        if( found_table )
        {
            memset( temp, 0, 64 );
            strncpy( temp, line, strlen( "     " ) );
            if( strcmp( temp, "     " ) == 0 )
            {
                line = strtok( NULL, "\n" );
                continue;
            }

            memset( temp, 0, 64 );
            strncpy( temp, line, strlen( "</pre>" ) );
            if( strcmp( temp, "</pre>" ) == 0 )
            {
                break;
            }

            memset( temp, 0, 10 );
            strncpy( temp, line, 2 ); //copy hour info
            hour = atoi( temp );
            memset( temp, 0, 10 );
            strncpy( temp, line + 3, 2 ); //copy minute info
            minute = atoi( temp );

            memset( temp, 0, 10 );
            j = 0;
            found_altitude = 0;
            for( i = 5; line[i] != '\n'; i++ )
            {
                if( line[i] != ' ' ) 
                {
                    if( !found_altitude )
                        found_altitude = 1;

                    temp[j++] = line[i];
                }
                else
                {
                    if( found_altitude )
                        break;
                }
            }

            int index = hour * 6 + ( minute / 10 );
            astro_info[index].sun_altitude = strtod( temp, NULL );
            printf( "Time: %i Altitude: %f\n", index, astro_info[index].sun_altitude );
        }
        else
        {
            memset( temp, 0, 64 );
            strncpy( temp, line, strlen( " h  m" ) );
            if( strcmp( temp, " h  m" ) == 0 )
            {
                found_table = 1;
            }
        }

        line = strtok( NULL, "\n" );
    }
    free( data );

    sprintf( url, "http://aa.usno.navy.mil/cgi-bin/aa_altazw.pl?form=1&body=11&year=%i&month=%i&day=%i&intv_mag=10&state=PA&place=Cresson",
            timeinfo->tm_year + 1900,
            timeinfo->tm_mon + 1,
            timeinfo->tm_mday );

    data = curl_request_data( url );

    line = strtok( data, "\n" );
    found_table = 0;
    int found_value = 0;
    int break_value = 0;
    while( line )
    {
        if( found_table )
        {
            memset( temp, 0, 64 );
            strncpy( temp, line, strlen( "     " ) );
            if( strcmp( temp, "     " ) == 0 )
            {
                line = strtok( NULL, "\n" );
                continue;
            }

            memset( temp, 0, 64 );
            strncpy( temp, line, strlen( "</pre>" ) );
            if( strcmp( temp, "</pre>" ) == 0 )
            {
                break;
            }

            memset( temp, 0, 10 );
            strncpy( temp, line, 2 ); //copy hour info
            hour = atoi( temp );
            memset( temp, 0, 10 );
            strncpy( temp, line + 3, 2 ); //copy minute info
            minute = atoi( temp );

            int index = hour * 6 + ( minute / 10 );

            memset( temp, 0, 10 );
            j = 0;
            found_value = 0;
            for( i = 5; line[i] != '\n'; i++ )
            {
                if( line[i] != ' ' ) 
                {
                    if( !found_value )
                        found_value = 1;

                    temp[j++] = line[i];
                }
                else
                {
                    if( found_value )
                    {
                        break_value = i;
                        break;
                    }
                }
            }
            astro_info[index].moon_altitude = strtod( temp, NULL );

            memset( temp, 0, 10 );
            j = 0;
            found_value = 0;
            for( i = break_value; line[i] != '\n'; i++ )
            {
                if( line[i] != ' ' ) 
                {
                    if( !found_value )
                        found_value = 1;

                    temp[j++] = line[i];
                }
                else
                {
                    if( found_value )
                    {
                        break_value = i;
                        break;
                    }
                }
            }

            memset( temp, 0, 10 );
            j = 0;
            found_value = 0;
            for( i = break_value; line[i] != '\n'; i++ )
            {
                if( line[i] != ' ' ) 
                {
                    if( !found_value )
                        found_value = 1;

                    temp[j++] = line[i];
                }
                else
                {
                    if( found_value )
                    {
                        break_value = i;
                        break;
                    }
                }
            }

            astro_info[index].moon_percent = strtod( temp, NULL );
            printf( "Moon Time: %i Altitude: %f Percent %f\n", index, astro_info[index].moon_altitude, astro_info[index].moon_percent );
        }
        else
        {
            memset( temp, 0, 64 );
            strncpy( temp, line, strlen( " h  m" ) );
            if( strcmp( temp, " h  m" ) == 0 )
            {
                found_table = 1;
            }
        }
        line = strtok( NULL, "\n" );
    }
}

void update_weather()
{
    update_astronomy(); 

    char* data = weather_api_request( "hourly" );

    //Now do the JSON stuff
    json_error_t error;
    json_t* root = json_loads( data, 0, &error );
    free( data );

    //JSON structure
    // hourly_forecast
    //  - FCTTIME
    //     - hour
    //     - min
    //  - temp
    //     - english: fahrenheit
    //     - condition
    json_t* forecasts = json_object_get( root, "hourly_forecast" );

    json_t *temp, *english, *array_data, *time;
    for( int i=0; i < json_array_size( forecasts ); i++ )
    {
        array_data = json_array_get( forecasts, i );

        temp = json_object_get( array_data, "temp" );
        english = json_object_get( temp, "english" );

        int temp = atoi( json_string_value( english ) );

        time = json_object_get( array_data, "FCTTIME" );
        int hour = atoi( json_string_value( json_object_get( time, "hour" ) ) ); 
        if( hour >= 12 )
            hour -= 12;
        if( hour == 0 ) hour = 12;
        int minute = atoi( json_string_value( json_object_get( time, "min" ) ) ); 
        const char* ampm = json_string_value( json_object_get( time, "ampm" ) );
        hourly_info[i+1].ampm[1] = 'm'; hourly_info[i+1].ampm[2] = '\0';
        if( ampm[0] == 'A' )
            hourly_info[i+1].ampm[0] = 'a';
        else
            hourly_info[i+1].ampm[0] = 'p';

        printf( "time: %i%s temp: %i\n", hour, hourly_info[i+1].ampm, temp );
        hourly_info[i+1].hour = hour;
        hourly_info[i+1].temp = temp;
    }

    data = weather_api_request( "conditions" );
    printf( data );
    root = json_loads( data, 0, &error );
    free( data );

    json_t* observation = json_object_get( root, "current_observation" );
    hourly_info[0].temp = (int)json_real_value( json_object_get( observation, "temp_f" ) );

}

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

void draw_sun_icon( cairo_t* cr, double x, double y, double r )
{
    int i;
    cairo_set_line_width( cr, 2.0 );
    cairo_set_operator( cr, CAIRO_OPERATOR_SOURCE );
    cairo_set_source_rgba( cr, 1.0, 1.0, 1.0, 0.0 );
    cairo_arc( cr, x, y, r + 8.0, 0, 2.0 * M_PI );
    cairo_fill( cr );

    cairo_set_operator( cr, CAIRO_OPERATOR_OVER );
    cairo_set_source_rgba( cr, 1.0, 1.0, 1.0, 0.75 );
    cairo_arc( cr, x, y, r * 0.5, 0, 2.0 * M_PI );

    double r1, r2, a;
    r1 = r * 0.625;
    r2 = r;
    for( i = 0; i < 8; i++ )
    {
        a = i * M_PI / 4.0;
        cairo_move_to( cr, x + cos( a ) * r1, y + sin( a ) * r1 );
        cairo_line_to( cr, x + cos( a ) * r, y + sin( a ) * r );
    }

    cairo_stroke( cr );
}

void draw_weather_icon( cairo_t* cr, double x, double y, int info_index )
{
    char buffer[64];
    int text_x, text_y;
    cairo_text_extents_t extents;
    weather_info* info = &hourly_info[info_index];

    cairo_set_font_size( cr, FONT_SIZE * 0.375 );
    cairo_move_to( cr, x, y );
    sprintf( buffer, "%i%s", info->hour, info->ampm );
    if( info_index == 0 ) strcpy( buffer, "now" );
    cairo_text_path( cr, buffer );
    cairo_fill( cr );

    text_x = x + GRID_SIZE * 0.75;
    text_y = y - GRID_SIZE * 0.5;

    cairo_set_font_size( cr, FONT_SIZE * 0.5 );
    sprintf( buffer,"%i", info->temp);
    cairo_text_extents( cr, buffer, &extents );
    cairo_move_to( cr, text_x, text_y );
    cairo_text_path( cr, buffer );
    cairo_fill( cr );

    text_x += extents.width + extents.x_bearing;
    text_y -= extents.height;

    strcpy( buffer, "°F" );
    cairo_set_font_size( cr, FONT_SIZE * 0.25 );
    cairo_text_extents( cr, buffer, &extents );
    text_y += extents.height; 
    cairo_move_to( cr, text_x, text_y );
    cairo_text_path( cr, buffer );
    cairo_fill( cr );
}

void draw_sun_line( cairo_t* cr )
{
    int i;
    double x = 0;
    double y = WINDOW_HEIGHT - GRID_SIZE;
    int line_start = 0;
    cairo_move_to( cr, x, y );
    for( i = 0; i < 144; i++ )
    {
        if( astro_info[i].sun_altitude < -20.0 )
            continue;
        else
        {
            if( !line_start )
            {
                x = (double)i * ((double)WINDOW_WIDTH / 143.0);
                y = WINDOW_HEIGHT - GRID_SIZE - ( astro_info[i].sun_altitude / 90.0 ) * ( WINDOW_HEIGHT - GRID_SIZE * 2.0 );
                cairo_move_to( cr, x, y );
            }
            line_start = 1;
        }

        x = (double)i * ((double)WINDOW_WIDTH / 143.0);
        y = WINDOW_HEIGHT - GRID_SIZE - ( astro_info[i].sun_altitude / 90.0 ) * ( WINDOW_HEIGHT - GRID_SIZE * 2.0 );
        cairo_line_to( cr, x, y );
    } 
}

void draw_moon_line( cairo_t* cr )
{
    int i;
    double x = 0;
    double y = WINDOW_HEIGHT - GRID_SIZE;
    int line_start = 0;
    cairo_move_to( cr, x, y );
    for( i = 0; i < 144; i++ )
    {
        if( astro_info[i].moon_altitude < -20.0 )
            continue;
        else
        {
            if( !line_start )

            {
                x = (double)i * ((double)WINDOW_WIDTH / 143.0);
                y = WINDOW_HEIGHT - GRID_SIZE - ( astro_info[i].moon_altitude / 90.0 ) * ( WINDOW_HEIGHT - GRID_SIZE * 2.0 );
                cairo_move_to( cr, x, y );
            }
            line_start = 1;
        }

        x = (double)i * ((double)WINDOW_WIDTH / 143.0);
        y = WINDOW_HEIGHT - GRID_SIZE - ( astro_info[i].moon_altitude / 90.0 ) * ( WINDOW_HEIGHT - GRID_SIZE * 2.0 );
        cairo_line_to( cr, x, y );
    } 

}
/*
 * This function draws the moon and sun lines
 */
void draw_astronomy_lines( cairo_t* cr )
{
    cairo_set_operator( cr, CAIRO_OPERATOR_SOURCE );
    cairo_set_line_width( cr, 8.0 );
    cairo_set_source_rgba( cr, 1.0, 1.0, 1.0, 0.0 );
    draw_sun_line( cr );
    cairo_stroke( cr );

    cairo_set_operator( cr, CAIRO_OPERATOR_OVER );
    cairo_set_line_width( cr, 4.0 );
    cairo_set_source_rgba( cr, 1.0, 1.0, 1.0, 0.5 );
    draw_sun_line( cr );
    cairo_stroke( cr );

    //Draw the sun icon
    double hour = (double)timeinfo->tm_hour * 6.0 + ( (double)timeinfo->tm_min / 10.0 );
    int hour_min = floor( hour );
    int hour_max = ceil( hour );
    if( hour_max > 143 ) hour_max = 143;


    cairo_set_operator( cr, CAIRO_OPERATOR_SOURCE );
    cairo_set_line_width( cr, 8.0 );
    cairo_set_source_rgba( cr, 1.0, 1.0, 1.0, 0.0 );
    draw_moon_line( cr );
    cairo_stroke( cr );

    cairo_set_operator( cr, CAIRO_OPERATOR_OVER );
    cairo_set_line_width( cr, 4.0 );
    cairo_set_source_rgba( cr, 1.0, 1.0, 1.0, 0.5 );
    draw_moon_line( cr );
    cairo_stroke( cr );

    cairo_set_operator( cr, CAIRO_OPERATOR_SOURCE );
    cairo_set_source_rgba( cr, 1.0, 1.0, 1.0, 0.0 );
    cairo_rectangle( cr, 0, WINDOW_HEIGHT - GRID_SIZE, WINDOW_WIDTH, GRID_SIZE );
    cairo_fill( cr );

    double sun_x, sun_y, altitude;
    altitude = astro_info[hour_min].sun_altitude + ( astro_info[hour_max].sun_altitude - astro_info[hour_min].sun_altitude ) * (hour - hour_min );
    sun_x = hour * ((double)WINDOW_WIDTH / 143.0);
    sun_y = WINDOW_HEIGHT - GRID_SIZE - ( altitude / 90.0 ) * ( WINDOW_HEIGHT - GRID_SIZE * 2.0 );
    draw_sun_icon( cr, sun_x, sun_y, FONT_SIZE * 0.5 );

    cairo_set_operator( cr, CAIRO_OPERATOR_OVER );
}

/*
 * This function draws the time and date strings, I have a feeling thought that it will soon draw everything
 */
void draw_timestring( cairo_t* cr )
{
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
    
    cairo_set_font_face(cr,cairo_toy_font_face_create("Source Sans Pro Light",CAIRO_FONT_SLANT_NORMAL,CAIRO_FONT_WEIGHT_NORMAL));
    cairo_text_extents_t extents;
    double text_x, text_y;
    double time_width = 0;


    cairo_t* tempcr = cairo_create( temp_surface );
    cairo_set_operator( tempcr, CAIRO_OPERATOR_SOURCE );
    cairo_set_source_rgba( tempcr, 0.0, 0.0, 0.0, 0.0 );
    cairo_paint( tempcr );
    draw_astronomy_lines( tempcr );
    cairo_destroy( tempcr );

    cairo_set_source_surface( cr, temp_surface, 0, 0 );
    cairo_paint( cr );

    /*
     * This part draws the background rectangles
     * TODO: make it so the shade rectangles are drawn on their own surface then overlayed on the background pixbuf
     */
    cairo_set_source_rgba( cr, 0.0, 0.0, 0.0, 0.75 );
    cairo_rectangle( cr, GRID_SIZE, (WINDOW_HEIGHT*0.5) - GRID_SIZE * 3.0, WINDOW_WIDTH - (GRID_SIZE * 2.0), GRID_SIZE * 4.5 );
    //Draw Bottom rect
    cairo_rectangle( cr, 0, WINDOW_HEIGHT - GRID_SIZE, WINDOW_WIDTH, GRID_SIZE );
    cairo_fill( cr );

    cairo_set_source_rgba( cr, 1.0, 1.0, 1.0, 0.75 );

    double x_move = GRID_SIZE * 2.0;
    double x = WINDOW_WIDTH*0.5 - ( GRID_SIZE * 5.0 );
    int i;
    int delta_hour = 3;
    for( i = 0; i < 5; i++ )
    {
        draw_weather_icon( cr, x, WINDOW_HEIGHT - FONT_SIZE * 0.25, i*delta_hour );
        x+= x_move;
    }

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
    time_t rawtime;

    time ( &rawtime );
    //timeinfo = localtime ( &rawtime );
    timeinfo->tm_min+=1;
    if( timeinfo->tm_min > 59 )
    {
        timeinfo->tm_min = 0;
        timeinfo->tm_hour += 1;
        if( timeinfo->tm_hour > 23 )
            timeinfo->tm_hour = 0;
    }

    weather_update_timer--;
    if( weather_update_timer < 0 )
    {
        weather_update_timer = WEATHER_REFRESH_TIMEOUT;
        update_weather();
    }
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
    time_t rawtime;

    gtk_init( &argc, &argv );

    //Get screen width
    GdkScreen* screen = gdk_screen_get_default();
    int screen_width = gdk_screen_get_width( screen );
    int screen_height = gdk_screen_get_height( screen );

    /*
     * Load test pixbuf
     * TODO: Make this so it randomly selects images from a directory based on current time of day/sun state
     */
    load_background_pixbuf( "/home/eric/deskclocky/test.jpg" );

    window = gtk_window_new( GTK_WINDOW_TOPLEVEL );
    gtk_window_set_title(GTK_WINDOW(window), "Alpha Demo");
    g_signal_connect(G_OBJECT(window), "delete-event", gtk_main_quit, NULL);

    gtk_widget_set_app_paintable(window, TRUE);
    g_signal_connect( G_OBJECT(window), "draw", G_CALLBACK( draw ), NULL );

    gtk_window_set_decorated(GTK_WINDOW(window), FALSE);
    gtk_widget_set_size_request(window,WINDOW_WIDTH,WINDOW_HEIGHT);
    gtk_widget_add_events(window, GDK_BUTTON_PRESS_MASK);
    gtk_window_move( GTK_WINDOW(window), 0, 0 );
    gtk_window_set_type_hint(GTK_WINDOW(window),GDK_WINDOW_TYPE_HINT_DIALOG);
    g_signal_connect(G_OBJECT(window), "button-press-event", G_CALLBACK(clicked), NULL);

    time ( &rawtime );
    timeinfo = localtime ( &rawtime );

    //g_timeout_add_seconds(1,refresh_clock,NULL);
    g_timeout_add( 100, refresh_clock, NULL );

    update_weather();
    temp_surface = cairo_image_surface_create( CAIRO_FORMAT_ARGB32, WINDOW_WIDTH, WINDOW_HEIGHT );

    gtk_widget_show_all(window);
    gtk_main();

    return 0;
}
