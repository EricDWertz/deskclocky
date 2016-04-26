/* deskclocky config.h file
 * Main program options go here
 */

#define WINDOW_WIDTH 1280
#define WINDOW_HEIGHT 800

#define FONT_SIZE 64.0
#define GRID_SIZE FONT_SIZE*1.5

#define URL_BASE "http://api.wunderground.com/api/565b48be709c806d/"
#define URL_POSTFIX "/q/40.4598,-78.5917.json"

//10 minute timeout to refresh weather info
#define WEATHER_REFRESH_TIMEOUT 600

#define DEBUG_TIMESCALE 0

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
