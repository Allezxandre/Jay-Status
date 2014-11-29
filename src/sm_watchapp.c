#include <pebble.h>
#include "globals.h"
#include "strings.h"

/*
	This WatchFace is a (deeply) modified version of SmartStatus
	All credits go to Robhh
	© Robhh : https://github.com/robhh/SmartStatus-AppStore
	© Alexandre Jouandin : https://github.com/Allezxandre/Smart-FrenchIze
	Idea by J Dishaw
*/

// #define DEBUG 1 // I don't have enough memory for debug.
#define STRING_LENGTH 255
#define NUM_WEATHER_IMAGES	9
#define VIBE_ON_HOUR true

#ifndef DEBUG
	#pragma message "---- COMPILING IN RELEASE MODE - NO LOGGING WILL BE AVAILABLE ----"
	#undef APP_LOG
	#define APP_LOG(...)
#endif

	// Mes variables
static bool Watch_Face_Initialized = false;
static char last_text[] = "No Title";
static bool phone_is_connected = false;
static int last_run_minute = -1;
enum {CALENDAR_LAYER, MUSIC_LAYER, NUM_LAYERS};

static void reset();
static void animate_layers();
static void auto_switch(void *data);
static void display_Notification(char *text1, char *text2, int time);
static void switch_to_calendar_window(ClickRecognizerRef recognizer, void *context); // Calendar Window implementation
static void destroy_calendar_window(ClickRecognizerRef recognizer, void *context);
static void toggle_music_blink(ClickRecognizerRef recognizer, void *context);

static Window *main_window;
static Window *calendar_window;

static PropertyAnimation *ani_out, *ani_in;

static Layer *animated_layer[NUM_LAYERS], *weather_layer;
static Layer *battery_layer, *battery_pbl_layer;
static Layer *status_layer;

static TextLayer *text_date_layer, *text_time_layer, *text_seconds_layer;

static TextLayer *text_mail_layer, *text_sms_layer, *text_phone_layer;
static TextLayer *text_weather_cond_layer, *text_weather_temp_layer, *text_battery_layer, *text_pebble_battery_layer;
static TextLayer *calendar_date_layer, *calendar_text_layer;
static TextLayer *music_artist_layer, *music_song_layer;
 
static BitmapLayer *background_image, *weather_image, *battery_image_layer, *battery_pbl_image_layer;

static int active_layer;

static char string_buffer[STRING_LENGTH];
static char weather_cond_str[STRING_LENGTH], weather_temp_str[5];
static int weather_img, batteryPercent, batteryPblPercent;

static char *calendar_date_str;
static char calendar_text_str[STRING_LENGTH];
static char music_artist_str1[STRING_LENGTH], music_title_str1[STRING_LENGTH];

GBitmap *bg_image, *battery_image, *battery_pbl_image;
GBitmap *weather_status_imgs[NUM_WEATHER_IMAGES];

static AppTimer *timerUpdateCalendar = NULL;
static AppTimer *timerUpdateWeather = NULL;
static AppTimer *timerUpdateMusic = NULL;
static AppTimer *hideMusicLayer = NULL;
static AppTimer *long_light_timer = NULL;

static Layer *calendar_event_one;
static TextLayer *text_event_layer_one;
static char* text_event_one;

/* Preload the fonts */
GFont font_date;
GFont font_time;
GFont font_secs;

/* Calendar Variables */
static bool event_is_today = false;
static bool event_is_all_day = false;
static bool event_is_past = false;
static int appt_day = -1;
static int appt_month = -1;
static int appt_hour = -1;
static int appt_minute = -1;

const int WEATHER_IMG_IDS[] = {	
  RESOURCE_ID_IMAGE_SUN,
  RESOURCE_ID_IMAGE_RAIN,
  RESOURCE_ID_IMAGE_CLOUD,
  RESOURCE_ID_IMAGE_SUN_CLOUD,
  RESOURCE_ID_IMAGE_FOG,
  RESOURCE_ID_IMAGE_WIND,
  RESOURCE_ID_IMAGE_SNOW,
  RESOURCE_ID_IMAGE_THUNDER,
  RESOURCE_ID_IMAGE_DISCONNECT
};

const bool animated = true;


static uint32_t s_sequence_number = 0xFFFFFFFE;

void load_memories() {
static bool event_is_all_day_int = -1;
	if (persist_exists(PERSIST_IS_ALL_DAY)) {
		APP_LOG(APP_LOG_LEVEL_INFO,"[B] Have DATA");
		event_is_all_day_int = persist_read_int(PERSIST_IS_ALL_DAY);
		event_is_all_day = (event_is_all_day_int == 1) ? 1 : 0;
		appt_day = persist_exists(PERSIST_DAY) ? persist_read_int(PERSIST_DAY) : -1;
		appt_month = persist_exists(PERSIST_MONTH) ? persist_read_int(PERSIST_MONTH) : -1;
		appt_hour = persist_exists(PERSIST_HOUR) ? persist_read_int(PERSIST_HOUR) : -1;
		appt_minute = persist_exists(PERSIST_MINUTE) ? persist_read_int(PERSIST_MINUTE) : -1;
		APP_LOG(APP_LOG_LEVEL_INFO,"[B] appointment : %02i/%02i [%02i:%02i]", appt_day,appt_month, appt_hour,appt_minute);
		if (persist_exists(PERSIST_EVENT_STRG)) {
			persist_read_string(PERSIST_EVENT_STRG, calendar_text_str, STRING_LENGTH);
			text_layer_set_text(calendar_text_layer, calendar_text_str); 	
				// Resize Calendar text if needed
			if (strlen(calendar_text_str) <= 15) {
				text_layer_set_font(calendar_text_layer, fonts_get_system_font(FONT_KEY_GOTHIC_24_BOLD));
			} else if (strlen(calendar_text_str) <= 18) {
					text_layer_set_font(calendar_text_layer, fonts_get_system_font(FONT_KEY_GOTHIC_18_BOLD));
			} else {
				text_layer_set_font(calendar_text_layer, fonts_get_system_font(FONT_KEY_GOTHIC_14_BOLD));
			}

		}
	}
}

void store_for_later() {
	APP_LOG(APP_LOG_LEVEL_DEBUG,"[B] Writing to Persistent Memory: %i/%i",appt_day, appt_month);
	bool event_is_all_day_int = event_is_all_day ? 1 : 0;
	persist_write_int(PERSIST_IS_ALL_DAY, event_is_all_day_int);
	persist_write_int(PERSIST_DAY, appt_day);
	int bufferMonth = persist_exists(PERSIST_MONTH) ? persist_read_int(PERSIST_MONTH) : -1;
	int bufferDay = persist_exists(PERSIST_DAY) ? persist_read_int(PERSIST_DAY) : -1;
	APP_LOG(APP_LOG_LEVEL_DEBUG,"[B] Testing Persistent Memory: %i/%i",bufferDay, bufferMonth);
	persist_write_int(PERSIST_MONTH, appt_month);
	persist_write_int(PERSIST_HOUR, appt_hour);
	persist_write_int(PERSIST_MINUTE, appt_minute);
	persist_write_string(PERSIST_EVENT_STRG, calendar_text_str);
}

// Calendar Appointments

/* Convert letter to digit */
int letter2digit(char letter) {
	if (letter == '\0') {
		APP_LOG(APP_LOG_LEVEL_ERROR, "[/] letter2digit failed!");
		return -1;
	}
	if((letter >= 48) && (letter <=57)) {
		return letter - 48;
	}
	APP_LOG(APP_LOG_LEVEL_ERROR, "[/] letter2digit(%c) failed", letter);
	return -1;
}

/* Convert string to number */
static int string2number(char *string) {
	int result = 0;
	static int32_t offset;
		offset = strlen(string) - 1;
	static int32_t digit = -1;
	static int32_t unit = 1;
	static int8_t letter;	

	for(unit = 1; offset >= 0; unit = unit * 10) {
		letter = string[offset];
		digit = letter2digit(letter);
		if(digit == -1){
			APP_LOG(APP_LOG_LEVEL_ERROR, "[/] string2number had to deal with '%s' as an argument and failed",string);
			return -1;
		}
		result = result + (unit * digit);
		offset--;
	}
	return result;
}

static void apptDisplay(char *appt_string) {
	
	// Init some variables
	static char date_time_for_appt[20]; // = "Le XX XXXX à ##h##";
	static char stringBuffer[]="XX";
	time_t now;
	struct tm *t;
	now = time(NULL);
	t = localtime(&now);
  if (phone_is_connected) {
  	APP_LOG(APP_LOG_LEVEL_DEBUG,"    Determining variables");

  	if (appt_string[0] == '\0') {
		APP_LOG(APP_LOG_LEVEL_WARNING, "[/] appt_string is empty! ABORTING apptDisplay");
		return;
	} else if (sizeof(appt_string) != 4) {
		APP_LOG(APP_LOG_LEVEL_WARNING, "[?] appt_string is too small (%i characters)! ABORTING apptDisplay", (int)(sizeof(appt_string)));
			text_layer_set_text(calendar_date_layer, appt_string);
		return;
	}
	// reset variables

	event_is_today = false;
	event_is_all_day = false;
	event_is_past = false;
	appt_day = -1;
	appt_month = -1;
	appt_hour = -1;
	appt_minute = -1;

		//	Determine the variables
					strncpy(stringBuffer, appt_string,2);
					appt_day = string2number(stringBuffer);

					strncpy(stringBuffer, appt_string+3,2);
					appt_month = string2number(stringBuffer);

	if (appt_month > 12) {
		APP_LOG(APP_LOG_LEVEL_WARNING,"[!] Please set DATE FORMAT to DD/MM");
		vibes_short_pulse();
		display_Notification("Please set FORMAT", "to DD/MM",10000);
		return;
	}

					if (appt_string[7] == ':'){
						strncpy(stringBuffer, appt_string+5,2);
						stringBuffer[0]='0';
						appt_hour = string2number(stringBuffer);
					} else if (appt_string[8] == ':') {
						strncpy(stringBuffer, appt_string+6,2);
						appt_hour = string2number(stringBuffer);
					} else {
						APP_LOG(APP_LOG_LEVEL_DEBUG,"    Event is ALL DAY");
						event_is_all_day = true;
					}

					if (appt_string[7] == ':'){
						strncpy(stringBuffer, appt_string+8,2);
						appt_minute = string2number(stringBuffer);
					} else if (appt_string[8] == ':') {
						strncpy(stringBuffer, appt_string+9,2);
						appt_minute = string2number(stringBuffer);
					} else {APP_LOG(APP_LOG_LEVEL_ERROR, "[?] appt_minute cannot be determined...");}
  } else {
  		APP_LOG(APP_LOG_LEVEL_WARNING, "[/] Phone isn't connected :-/");
		if (appt_day == -1 || appt_month == -1){
			APP_LOG(APP_LOG_LEVEL_WARNING,"[!] There is no backup -> ABORT");
			return;
		}
		APP_LOG(APP_LOG_LEVEL_INFO,"[-] I have a BACKUP!");
  }
	APP_LOG(APP_LOG_LEVEL_INFO,"[-] Time        : %02i/%02i [%02i:%02i]", t->tm_mday,t->tm_mon+1, t->tm_hour, t->tm_min);
	APP_LOG(APP_LOG_LEVEL_INFO,"[X] appointment : %02i/%02i", appt_day,appt_month);
	if (!event_is_all_day) 
		{APP_LOG(APP_LOG_LEVEL_INFO,"[X]             :       [%02i:%02i]", appt_hour,appt_minute);}

	 static int hour_now;
	 static int min_now;
	 static int mday_now;
	 static int mon_now;
	 hour_now = t->tm_hour;
	 min_now = t->tm_min;
	 mday_now = t->tm_mday;
	 mon_now = t->tm_mon + 1;
		// Check the DAY and Month of Appointment and write it in date_of_appt
	static char date_of_appt[30];
	int interm = (appt_month - 1);
	static int days_difference = 0;
	if (mon_now != appt_month) {
		if ((mon_now - appt_month > 1) || (mon_now - appt_month < -1)) {
			days_difference = 40; // Set a high value to display the date
		} else if (appt_month < mon_now){ // Event has begun last month
			days_difference = ((mday_now) + (days_per_month[(appt_month + 1)] - appt_day));
			event_is_past = true;
		} else if (appt_month > mon_now){ // Event will begin next month
			days_difference = ((days_per_month[(mon_now + 1)] - mday_now) + appt_day);
		}
	} else {
		days_difference = (appt_day - mday_now);
		if (days_difference < 0) { // That means appointment day is before today
			event_is_past = true;
		}
	}
	static char time_string[20];
	APP_LOG(APP_LOG_LEVEL_DEBUG,"    'days_difference' = %i",days_difference);
				if (event_is_past) {
					snprintf(date_of_appt,30, STRING_EVENT_IS_PAST,appt_day, month_of_year[interm]);
					event_is_all_day = true;
					APP_LOG(APP_LOG_LEVEL_DEBUG,"    Event has started in the past, not today");
				} else if (days_difference > 4) {
					snprintf(date_of_appt, 30, STRING_EVENT_FUTURE_GLOBAL,appt_day, month_of_year[interm]);
					event_is_today = false; // Just so we don't write the time again
					time_string[0] = '\0';
				} else if (days_difference >= 0) {
					snprintf(date_of_appt, 30, STRING_EVENT_FUTURE_SOON, days_from_today[days_difference]);
					event_is_today = false; // Just so we don't write the time again
					time_string[0] = '\0';
					event_is_today = (days_difference == 0) ? true : false;
				} else {
					APP_LOG(APP_LOG_LEVEL_ERROR, "[/] days_difference tests failed :(");
					return;
				}
		// Check the Hour and write it in time_string

		// Check the Hour and write it in time_string
	 void display_hour (int hour_since, int minutes_since, int quand) {
	 	date_of_appt[0]='\0';
	 	if ((minutes_since == 0) && hour_since == 0) {
						snprintf(time_string,20, STRING_NOW);
						if (last_run_minute != t->tm_min) {
							vibes_short_pulse();
						}
					} else if (minutes_since == 0) {
						if (hour_since == 1){
							snprintf(time_string,20, STRING_EVENT_HOUR,before_after[quand]);
						} else {
							snprintf(time_string,20, STRING_EVENT_HOURS,before_after[quand], hour_since); // For plural
						}
					} else if (hour_since == 0) {
						if (minutes_since == 1){
							snprintf(time_string,20, STRING_EVENT_MINUTE,before_after[quand]);
						} else {
							snprintf(time_string,20, STRING_EVENT_MINUTES,before_after[quand],minutes_since); // For plural
						}
					} else {
						snprintf(time_string,20, STRING_EVENT_MIXED, before_after[quand], hour_since, minutes_since);
					}
	  }

				if (event_is_all_day) {
					time_string[0] = '\0';
					APP_LOG(APP_LOG_LEVEL_DEBUG, "    Do nothing with hour and minutes");
				} else if (!event_is_today){
					snprintf(time_string,20,STRING_DEFAULT_HOUR_MIN, appt_hour, appt_minute);
				} else if (((hour_now) > appt_hour) || (((hour_now) == appt_hour) && (min_now >= appt_minute))) {
					int hour_since = 0;
					int minutes_since = 0;
					minutes_since = ((min_now) - appt_minute);
					hour_since = ((hour_now) - appt_hour);
					if (minutes_since < 0) {
						hour_since -= 1;
						minutes_since += 60;
					}
					
					display_hour(hour_since,minutes_since,0);

				} else if (((hour_now) < appt_hour) || (((hour_now) == appt_hour) && (min_now < appt_minute))) {
					int hour_difference = 0;
					int minutes_difference = 0;
					minutes_difference = (appt_minute - (min_now));
					hour_difference = (appt_hour - (hour_now));
					if (minutes_difference < 0) {
						hour_difference -= 1;
						minutes_difference += 60;
					}
					
					if (hour_difference < 6) {
						display_hour(hour_difference,minutes_difference,1);
					} else {
						snprintf(time_string,20,STRING_DEFAULT_HOUR_MIN, appt_hour, appt_minute);
					}
				}

	strcpy (date_time_for_appt,date_of_appt);
  	strcat (date_time_for_appt,time_string);

	text_layer_set_text(calendar_date_layer, date_time_for_appt);
}

// End of calendar appointment utilities


AppMessageResult sm_message_out_get(DictionaryIterator **iter_out) {
    AppMessageResult result = app_message_outbox_begin(iter_out);
    if(result != APP_MSG_OK) return result;
    dict_write_int32(*iter_out, SM_SEQUENCE_NUMBER_KEY, ++s_sequence_number);
    if(s_sequence_number == 0xFFFFFFFF) {
        s_sequence_number = 1;
    }
    return APP_MSG_OK;
}

void reset_sequence_number() {
    DictionaryIterator *iter = NULL;
    app_message_outbox_begin(&iter);
    if(!iter) return;
    dict_write_int32(iter, SM_SEQUENCE_NUMBER_KEY, 0xFFFFFFFF);
    app_message_outbox_send();
}


void sendCommand(int key) {
	DictionaryIterator* iterout;
	sm_message_out_get(&iterout);
    if(!iterout) return;
	
	dict_write_int8(iterout, key, -1);
	app_message_outbox_send();
}


void sendCommandInt(int key, int param) {
	DictionaryIterator* iterout;
	sm_message_out_get(&iterout);
    if(!iterout) return;
	
	dict_write_int8(iterout, key, param);
	app_message_outbox_send();
}

static void display_Notification(char *text1, char *text2, int time) {
		if (hideMusicLayer != NULL) 
			app_timer_cancel(hideMusicLayer);
		hideMusicLayer = app_timer_register(time , auto_switch, NULL);
		if (active_layer != MUSIC_LAYER) 
				animate_layers(NULL,NULL);
		text_layer_set_text(music_artist_layer, text1);
		text_layer_set_text(music_song_layer, text2);
		strncpy(last_text,"12345678",8);
	}


/*static void select_click_down_handler(ClickRecognizerRef recognizer, void *context) {
	//show the weather condition instead of temperature while center button is pressed
	layer_set_hidden(text_layer_get_layer(text_weather_temp_layer), true);
	layer_set_hidden(text_layer_get_layer(text_weather_cond_layer), false);
} */

static void update_all(ClickRecognizerRef recognizer, void *context) {
	//update all data
	strncpy(last_text,"12345678",8); // Hack to display the music Layer without using more than one line of code
	if (phone_is_connected) {
		reset();
		sendCommandInt(SM_SCREEN_ENTER_KEY, STATUS_SCREEN_APP);
	} else {
		display_Notification("iPhone", STRING_DISCONNECTED, 2000);
	}
}

static void call_siri(ClickRecognizerRef recognizer, void *context) {
	sendCommand(SM_OPEN_SIRI_KEY);
}

static void notif_calendar_window(ClickRecognizerRef recognizer, void *context) {
	display_Notification("Fenetre", "Activator", 1000);
}

static void activator_bottom(ClickRecognizerRef recognizer, void *context) {
	sendCommandInt(SM_ACTIVATOR_KEY_PRESSED, ACTIVATOR_KEY_PRESSED_DOWN);
}

static void activator_middle(ClickRecognizerRef recognizer, void *context) {
	sendCommandInt(SM_ACTIVATOR_KEY_PRESSED, ACTIVATOR_KEY_PRESSED_SELECT);
}

static void activator_top(ClickRecognizerRef recognizer, void *context) {
	sendCommandInt(SM_ACTIVATOR_KEY_PRESSED, ACTIVATOR_KEY_PRESSED_UP);
}

static void activator_bottom_held(ClickRecognizerRef recognizer, void *context) {
	sendCommandInt(SM_ACTIVATOR_KEY_PRESSED, ACTIVATOR_KEY_HELD_DOWN);
}

static void activator_middle_held(ClickRecognizerRef recognizer, void *context) {
	sendCommandInt(SM_ACTIVATOR_KEY_PRESSED, ACTIVATOR_KEY_HELD_SELECT);
}

static void activator_top_held(ClickRecognizerRef recognizer, void *context) {
	sendCommandInt(SM_ACTIVATOR_KEY_PRESSED, ACTIVATOR_KEY_HELD_UP);
}

static void next_track_action(ClickRecognizerRef recognizer, void *context) {
	if (phone_is_connected) {sendCommand(SM_NEXT_TRACK_KEY);} else {light_enable(false);}
	if (hideMusicLayer != NULL) {
			app_timer_cancel(hideMusicLayer);
			hideMusicLayer = NULL;
	}
	auto_switch(NULL);
}

static void previous_track_action(ClickRecognizerRef recognizer, void *context) {
	if (phone_is_connected) {sendCommand(SM_PREVIOUS_TRACK_KEY);} else {light_enable(false);}
	if (hideMusicLayer != NULL) {
			app_timer_cancel(hideMusicLayer);
			hideMusicLayer = NULL;
	}
	auto_switch(NULL);
}

static void notif_find_my_iphone(ClickRecognizerRef recognizer, void *context) {
	vibes_short_pulse();
	if (phone_is_connected) {
		display_Notification(STRING_FIND_IPHONE_CONF_1, STRING_FIND_IPHONE_CONF_2, 5000);
	} else {
		display_Notification("iPhone", STRING_DISCONNECTED, 2000);
	}
}

static void find_my_iphone(ClickRecognizerRef recognizer, void *context) {
	if (phone_is_connected) {
		vibes_long_pulse();
	}
	sendCommand(SM_FIND_MY_PHONE_KEY);
}

static void turn_off_the_light(void *data) {
		light_enable(false);
		if (long_light_timer != NULL) {
			app_timer_cancel(long_light_timer);
			long_light_timer = NULL;
		}
}

static void volume_increase(ClickRecognizerRef recognizer, void *context) {
	sendCommand(SM_VOLUME_UP_KEY);
}

static void volume_decrease(ClickRecognizerRef recognizer, void *context) {
	sendCommand(SM_VOLUME_DOWN_KEY);
}

static void play_pause_action(ClickRecognizerRef recognizer, void *context) {
	if (phone_is_connected) { // If phone is connected, we play music
		strncpy(last_text,"12345678",8); // We kind of force a new displaying of song
		sendCommand(SM_PLAYPAUSE_KEY);
	} else {
		light_enable(true);
		if (long_light_timer != NULL) {
			app_timer_cancel(long_light_timer);
			long_light_timer = NULL;
		}
		long_light_timer = app_timer_register(120000 , turn_off_the_light, NULL);
	}
}

static void animate_layers() {
	//slide layers in/out
	property_animation_destroy((PropertyAnimation*)ani_in);
	property_animation_destroy((PropertyAnimation*)ani_out);

	ani_out = property_animation_create_layer_frame(animated_layer[active_layer], &GRect(0, 100, 143, 45), &GRect(-138, 100, 143, 45)); // &GRect(0, 124, 143, 45), &GRect(-138, 124, 143, 45));
	animation_schedule((Animation*)ani_out);

	active_layer = (active_layer + 1) % (NUM_LAYERS);

	ani_in = property_animation_create_layer_frame(animated_layer[active_layer], &GRect(138, 100, 144, 45), &GRect(0, 100, 144, 45)); // &GRect(138, 124, 144, 45), &GRect(0, 124, 144, 45));
	animation_schedule((Animation*)ani_in);
}


static void click_config_provider_main(void *context) {
  window_long_click_subscribe(BUTTON_ID_UP, 3000, notif_find_my_iphone, find_my_iphone);
  window_single_click_subscribe(BUTTON_ID_UP, volume_increase);
  window_single_click_subscribe(BUTTON_ID_SELECT, play_pause_action);
  window_long_click_subscribe(BUTTON_ID_UP, 750, notif_calendar_window, switch_to_calendar_window);
  window_single_click_subscribe(BUTTON_ID_BACK, volume_decrease);
  window_multi_click_subscribe(BUTTON_ID_BACK, 2, 0, 250, true, update_all);
  	/*
			void window_multi_click_subscribe	(	ButtonId 	button_id,
													uint8_t 	min_clicks,
													uint8_t 	max_clicks,
													uint16_t 	timeout,
													bool 	last_click_only,
													ClickHandler 	handler 
												)	
  	*/
  window_single_click_subscribe(BUTTON_ID_DOWN, next_track_action);
  window_long_click_subscribe(BUTTON_ID_SELECT, 750, call_siri, NULL);
  window_long_click_subscribe(BUTTON_ID_DOWN, 250, previous_track_action, NULL);
}

static void click_config_provider_calendar(void *context) {
  window_single_click_subscribe(BUTTON_ID_UP, activator_top);
  window_long_click_subscribe(BUTTON_ID_UP, 750, activator_top_held, NULL);
  window_single_click_subscribe(BUTTON_ID_SELECT, activator_middle);
  window_long_click_subscribe(BUTTON_ID_SELECT, 750, activator_middle_held, NULL);
  window_single_click_subscribe(BUTTON_ID_BACK, destroy_calendar_window);
  window_multi_click_subscribe(BUTTON_ID_BACK, 2, 0, 250, true, toggle_music_blink);
  	/*
			void window_multi_click_subscribe	(	ButtonId 	button_id,
													uint8_t 	min_clicks,
													uint8_t 	max_clicks,
													uint16_t 	timeout,
													bool 	last_click_only,
													ClickHandler 	handler 
												)	
  	*/
  window_single_click_subscribe(BUTTON_ID_DOWN, activator_bottom);
  window_long_click_subscribe(BUTTON_ID_DOWN, 750, activator_bottom_held, NULL);
}

static void window_load(Window *window) {
  Layer *window_layer = window_get_root_layer(window);
  GRect bounds = layer_get_bounds(window_layer);

}

static void window_unload(Window *window) {
	
	
}

static void window_appear(Window *window)
{
	sendCommandInt(SM_SCREEN_ENTER_KEY, STATUS_SCREEN_APP);
	  
}


static void window_disappear(Window *window)
{
	sendCommandInt(SM_SCREEN_EXIT_KEY, STATUS_SCREEN_APP);
}


void battery_layer_update_callback(Layer *me, GContext* ctx) {
	
	//draw the remaining battery percentage
	graphics_context_set_stroke_color(ctx, GColorBlack);
	graphics_context_set_fill_color(ctx, GColorWhite);

	graphics_fill_rect(ctx, GRect(2+16-(int)((batteryPercent/100.0)*16.0), 2, (int)((batteryPercent/100.0)*16.0), 8), 0, GCornerNone);
	
}

void battery_pbl_layer_update_callback(Layer *me, GContext* ctx) {
	
	//draw the remaining pebble battery percentage
	graphics_context_set_stroke_color(ctx, GColorBlack);
	graphics_context_set_fill_color(ctx, GColorWhite);

	graphics_fill_rect(ctx, GRect(2+16-(int)((batteryPblPercent/100.0)*16.0), 2, (int)((batteryPblPercent/100.0)*16.0), 8), 0, GCornerNone);

	static char battery_text[STRING_LENGTH];
	layer_mark_dirty(battery_pbl_layer);
	snprintf(battery_text, sizeof(battery_text), "%d", batteryPblPercent);
	text_layer_set_text(text_pebble_battery_layer, battery_text ); 
}


void reset() {
	
/*	layer_set_hidden(text_layer_get_layer(text_weather_temp_layer), true);
	layer_set_hidden(text_layer_get_layer(text_weather_cond_layer), false);
*/	text_layer_set_text(text_weather_cond_layer, STRING_UPDATING); 	
	
}


void handle_second_tick(struct tm *tick_time, TimeUnits units_changed) {
		
	// Seconds Display
	static char seconds_text[] = "00";
	strftime(seconds_text, sizeof(seconds_text), "%S", tick_time);
	text_layer_set_text(text_seconds_layer, seconds_text);

	if ((((units_changed & MINUTE_UNIT) == MINUTE_UNIT) || (!Watch_Face_Initialized))) {apptDisplay(calendar_date_str);}
if (((units_changed & MINUTE_UNIT) == MINUTE_UNIT) || (!Watch_Face_Initialized) ){
	// Need to be static because they're used by the system later.
	static char time_text[] = "00:00";
	static char *time_format;

	static int heure;
	heure = tick_time->tm_hour;

	
  // TODO: Only update the date when it's changed. // DONE ! Even with SECOND ticks
	if ((units_changed & DAY_UNIT) == DAY_UNIT|| (!Watch_Face_Initialized) ){
		  Watch_Face_Initialized = true;
  	static char date_text[] = "DAY 00 MOIS";
  		// Get the day and month as int
  	static int day_int;
	 day_int = tick_time->tm_wday;
	static int month_int;
	 month_int = tick_time->tm_mon;
	 	// Print the result
   snprintf(date_text, sizeof(date_text), "%s %i %s", day_of_week[day_int], tick_time->tm_mday, month_of_year[month_int]);
   text_layer_set_text(text_date_layer, date_text);
	  APP_LOG(APP_LOG_LEVEL_INFO, "[-] Displayed date : [%s %i %s]", day_of_week[day_int], tick_time->tm_mday, month_of_year[month_int]);
  	}

  if (clock_is_24h_style()) {
    time_format = "%R";
  } else {
    time_format = "%I:%M";
  }

  strftime(time_text, sizeof(time_text), time_format, tick_time);

  // Kludge to handle lack of non-padded hour format string
  // for twelve hour clock.
  if (!clock_is_24h_style() && (time_text[0] == '0')) {
    memmove(time_text, &time_text[1], sizeof(time_text) - 1);
  }

	
	// Don't forget the "heure" variable if you copy this small paragraph
  if (((units_changed & HOUR_UNIT) == HOUR_UNIT) && ((heure > 9) && (heure < 23))){
    vibes_double_pulse();
    APP_LOG(APP_LOG_LEVEL_DEBUG, "    Hour changed -> Vibration complete");
  }
	
  text_layer_set_text(text_time_layer, time_text);
}
}


void reconnect(void *data) {
	reset();

	sendCommandInt(SM_SCREEN_ENTER_KEY, STATUS_SCREEN_APP);
	
}

void bluetoothChanged(bool connected) {

	if (connected) {
		app_timer_register(5000, reconnect, NULL);
		layer_set_hidden(text_layer_get_layer(text_weather_cond_layer), true);
		if (!phone_is_connected) {vibes_short_pulse();} 
		/* Pebble has two channels for connection : Bluetooth-LE and Bluetooth ADP, it's a workaround 
		   to prevent the watch from vibrating twice*/
		display_Notification("iPhone", STRING_CONNECTED, 5000);
		// Display Battery layers or it won't show
/*		layer_set_hidden(bitmap_layer_get_layer(battery_image_layer), false);
		layer_set_hidden(battery_layer, false);
		layer_set_hidden(text_layer_get_layer(text_battery_layer), false); */
		phone_is_connected = true;
	} else {
		bitmap_layer_set_bitmap(weather_image, weather_status_imgs[NUM_WEATHER_IMAGES-1]);
		if (phone_is_connected) {vibes_short_pulse();}
		display_Notification("iPhone", STRING_DISCONNECTED, 5000);
		phone_is_connected = false;
		layer_set_hidden(bitmap_layer_get_layer(battery_image_layer), true);
		layer_set_hidden(battery_layer, true);
		layer_set_hidden(text_layer_get_layer(text_battery_layer), true);
		store_for_later();
	}
	
}


void batteryChanged(BatteryChargeState batt) {
	
	batteryPblPercent = batt.charge_percent;
	layer_mark_dirty(battery_layer);
	
}


static void init(void) {
  main_window = window_create();
  window_set_fullscreen(main_window, true);
  window_set_click_config_provider(main_window, click_config_provider_main);
  window_set_window_handlers(main_window, (WindowHandlers) {
    .load = window_load,
    .unload = window_unload,
	.appear = window_appear,
	.disappear = window_disappear
  });
  window_stack_push(main_window, animated);
  // Choose fonts
font_date = fonts_load_custom_font(resource_get_handle(RESOURCE_ID_FONT_CLEARSANS_REGULAR_21));
font_time = fonts_load_custom_font(resource_get_handle(RESOURCE_ID_FONT_BOLD_46));
font_secs = fonts_load_custom_font(resource_get_handle(RESOURCE_ID_FONT_BOLD_20));
	//init weather images
	for (int i=0; i<NUM_WEATHER_IMAGES; i++) {
	  	weather_status_imgs[i] = gbitmap_create_with_resource(WEATHER_IMG_IDS[i]);
	}
	
  	bg_image = gbitmap_create_with_resource(RESOURCE_ID_IMAGE_BACKGROUND);


  	Layer *window_layer = window_get_root_layer(main_window);

	//init background image
  	GRect bg_bounds = layer_get_frame(window_layer);

	background_image = bitmap_layer_create(bg_bounds);
	layer_add_child(window_layer, bitmap_layer_get_layer(background_image));
	bitmap_layer_set_bitmap(background_image, bg_image);
	

	//init weather layer and add weather image, weather condition, temperature, and battery indicator
	weather_layer = layer_create(GRect(0, 0, 144, 45)); // GRect(0, 78, 144, 45));
	layer_add_child(window_layer, weather_layer);

	battery_image = gbitmap_create_with_resource(RESOURCE_ID_IMAGE_BATTERY_PHONE);
	battery_pbl_image = gbitmap_create_with_resource(RESOURCE_ID_IMAGE_BATTERY_PEBBLE);

	battery_image_layer = bitmap_layer_create(GRect(68, 5, 37, 14)); // GRect(100, 7, 37, 14));
	layer_add_child(weather_layer, bitmap_layer_get_layer(battery_image_layer));
	bitmap_layer_set_bitmap(battery_image_layer, battery_image);

	battery_pbl_image_layer = bitmap_layer_create(GRect(105, 5, 37, 14)); // GRect(100, 23, 37, 14));
	layer_add_child(weather_layer, bitmap_layer_get_layer(battery_pbl_image_layer));
	bitmap_layer_set_bitmap(battery_pbl_image_layer, battery_pbl_image);

status_layer = layer_create(GRect(0, 148, 144, 20));
layer_add_child(window_layer, status_layer);

		text_mail_layer = text_layer_create(GRect(24, -3, 24, 20));
		text_layer_set_text_alignment(text_mail_layer, GTextAlignmentCenter);
		text_layer_set_text_color(text_mail_layer, GColorWhite);
		text_layer_set_background_color(text_mail_layer, GColorClear);
		text_layer_set_font(text_mail_layer, fonts_get_system_font(FONT_KEY_GOTHIC_18));
		layer_add_child(status_layer, text_layer_get_layer(text_mail_layer));
		text_layer_set_text(text_mail_layer, "--"); 	


		text_sms_layer = text_layer_create(GRect(72, -3, 24, 20));
		text_layer_set_text_alignment(text_sms_layer, GTextAlignmentCenter);
		text_layer_set_text_color(text_sms_layer, GColorWhite);
		text_layer_set_background_color(text_sms_layer, GColorClear);
		text_layer_set_font(text_sms_layer,  fonts_get_system_font(FONT_KEY_GOTHIC_18));
		layer_add_child(status_layer, text_layer_get_layer(text_sms_layer));
		text_layer_set_text(text_sms_layer, "--"); 	

		text_phone_layer = text_layer_create(GRect(120, -3, 24, 20));
		text_layer_set_text_alignment(text_phone_layer, GTextAlignmentCenter);
		text_layer_set_text_color(text_phone_layer, GColorWhite);
		text_layer_set_background_color(text_phone_layer, GColorClear);
		text_layer_set_font(text_phone_layer,  fonts_get_system_font(FONT_KEY_GOTHIC_18));
		layer_add_child(status_layer, text_layer_get_layer(text_phone_layer));
		text_layer_set_text(text_phone_layer, "--"); 

		battery_layer = layer_create(GRect(70, 6, 19, 30)); // GRect(102, 8, 19, 11)); GRect(105, -1, 37, 14)
	layer_set_update_proc(battery_layer, battery_layer_update_callback);
	layer_add_child(weather_layer, battery_layer);

	text_battery_layer = text_layer_create(GRect(67, 15, 25, 19)); // GRect(99, 20, 40, 60));
	text_layer_set_text_alignment(text_battery_layer, GTextAlignmentCenter);
	text_layer_set_text_color(text_battery_layer, GColorWhite);
	text_layer_set_background_color(text_battery_layer, GColorClear);
	text_layer_set_font(text_battery_layer,  fonts_get_system_font(FONT_KEY_GOTHIC_18_BOLD));
	layer_add_child(weather_layer, text_layer_get_layer(text_battery_layer));
	text_layer_set_text(text_battery_layer, "--");

	if (!phone_is_connected) {
		layer_set_hidden(bitmap_layer_get_layer(battery_image_layer), true);
		layer_set_hidden(battery_layer, true);
		layer_set_hidden(text_layer_get_layer(text_battery_layer), true);
	}

	batteryPercent = 100;
	layer_mark_dirty(battery_layer);

	battery_pbl_layer = layer_create(GRect(107, 6, 19, 30)); // GRect(102, 24, 19, 11)); GRect(68, -1, 37, 14)
	layer_set_update_proc(battery_pbl_layer, battery_pbl_layer_update_callback);
	layer_add_child(weather_layer, battery_pbl_layer);

	BatteryChargeState pbl_batt = battery_state_service_peek();
	batteryPblPercent = pbl_batt.charge_percent;
	layer_mark_dirty(battery_pbl_layer);

	text_pebble_battery_layer = text_layer_create(GRect(104, 15, 25, 19)); // GRect(99, 20, 40, 60));
	text_layer_set_text_alignment(text_pebble_battery_layer, GTextAlignmentCenter);
	text_layer_set_text_color(text_pebble_battery_layer, GColorWhite);
	text_layer_set_background_color(text_pebble_battery_layer, GColorClear);
	text_layer_set_font(text_pebble_battery_layer,  fonts_get_system_font(FONT_KEY_GOTHIC_18_BOLD));
	layer_add_child(weather_layer, text_layer_get_layer(text_pebble_battery_layer));
	text_layer_set_text(text_pebble_battery_layer, "100");


	text_weather_cond_layer = text_layer_create(GRect(5, 15, 62, 30)); // GRect(5, 2, 47, 40)
	text_layer_set_text_alignment(text_weather_cond_layer, GTextAlignmentLeft);
	text_layer_set_text_color(text_weather_cond_layer, GColorWhite);
	text_layer_set_background_color(text_weather_cond_layer, GColorClear);
	text_layer_set_font(text_weather_cond_layer, fonts_get_system_font(FONT_KEY_GOTHIC_18));
	layer_add_child(weather_layer, text_layer_get_layer(text_weather_cond_layer));

	layer_set_hidden(text_layer_get_layer(text_weather_cond_layer), true); 
	text_layer_set_text(text_weather_cond_layer, STRING_UPDATING); 	
	
	if (bluetooth_connection_service_peek()) {
		weather_img = 0;
	} else {
		weather_img = NUM_WEATHER_IMAGES - 1;
	}

	weather_image = bitmap_layer_create(GRect(5, 2, 20, 20));  // GRect(5, 2, 40, 40)); 
	layer_add_child(weather_layer, bitmap_layer_get_layer(weather_image));
	bitmap_layer_set_bitmap(weather_image, weather_status_imgs[weather_img]);


	text_weather_temp_layer = text_layer_create(GRect(28, 0, 100, 20));  // GRect(48, 3, 48, 40)); 
	text_layer_set_text_alignment(text_weather_temp_layer, GTextAlignmentLeft);
	text_layer_set_text_color(text_weather_temp_layer, GColorWhite);
	text_layer_set_background_color(text_weather_temp_layer, GColorClear);
	text_layer_set_font(text_weather_temp_layer, fonts_get_system_font(FONT_KEY_GOTHIC_18));
	layer_add_child(weather_layer, text_layer_get_layer(text_weather_temp_layer));
	text_layer_set_text(text_weather_temp_layer, "-°"); 	

/*	layer_set_hidden(text_layer_get_layer(text_weather_temp_layer), false); // changed from true to false */

	
	//init layers for time and date
	text_date_layer = text_layer_create(bg_bounds);
	text_layer_set_text_alignment(text_date_layer, GTextAlignmentCenter);
	text_layer_set_text_color(text_date_layer, GColorWhite);
	text_layer_set_background_color(text_date_layer, GColorClear);
	layer_set_frame(text_layer_get_layer(text_date_layer), GRect(0, 75, 144, 30)); // GRect(0, 50, 144, 30));
	text_layer_set_font(text_date_layer, font_date);
	layer_add_child(window_layer, text_layer_get_layer(text_date_layer));


	text_time_layer = text_layer_create(bg_bounds);
	text_layer_set_text_alignment(text_time_layer, GTextAlignmentLeft); //Previous was center
	text_layer_set_text_color(text_time_layer, GColorWhite);
	text_layer_set_background_color(text_time_layer, GColorClear);
	layer_set_frame(text_layer_get_layer(text_time_layer), GRect(5, 30, 115, 55)); // GRect(0, -5, 144, 55));
	text_layer_set_font(text_time_layer, font_time);
	layer_add_child(window_layer, text_layer_get_layer(text_time_layer));

	text_seconds_layer = text_layer_create(bg_bounds);
	text_layer_set_text_alignment(text_seconds_layer, GTextAlignmentLeft); //Previous was center
	text_layer_set_text_color(text_seconds_layer, GColorWhite);
	text_layer_set_background_color(text_seconds_layer, GColorClear);
	layer_set_frame(text_layer_get_layer(text_seconds_layer), GRect(120, 39, 24, 45)); // GRect(0, -5, 144, 55));
	text_layer_set_font(text_seconds_layer, font_secs);
	layer_add_child(window_layer, text_layer_get_layer(text_seconds_layer));


	//init calendar layer
	animated_layer[CALENDAR_LAYER] = layer_create(GRect(0, 100, 144, 45)); //(GRect(0, 124, 144, 45)); 
		//										 _with_data to make sure it can be allocated dynamically
	layer_add_child(window_layer, animated_layer[CALENDAR_LAYER]);
	
	calendar_date_layer = text_layer_create(GRect(6, 0, 132, 21)); //(GRect(6, 0, 132, 21));
	text_layer_set_text_alignment(calendar_date_layer, GTextAlignmentLeft);
	text_layer_set_text_color(calendar_date_layer, GColorWhite);
	text_layer_set_background_color(calendar_date_layer, GColorClear);
	text_layer_set_font(calendar_date_layer, fonts_get_system_font(FONT_KEY_GOTHIC_18));
	layer_add_child(animated_layer[CALENDAR_LAYER], text_layer_get_layer(calendar_date_layer));
	text_layer_set_text(calendar_date_layer, STRING_DEFAULT_CALENDAR_1); 	


	calendar_text_layer = text_layer_create(GRect(6, 15, 132, 28)); //(GRect(6, 15, 132, 28));
	text_layer_set_text_alignment(calendar_text_layer, GTextAlignmentLeft);
	text_layer_set_text_color(calendar_text_layer, GColorWhite);
	text_layer_set_background_color(calendar_text_layer, GColorClear);
	text_layer_set_font(calendar_text_layer, fonts_get_system_font(FONT_KEY_GOTHIC_24_BOLD));
	layer_add_child(animated_layer[CALENDAR_LAYER], text_layer_get_layer(calendar_text_layer));
	text_layer_set_text(calendar_text_layer, STRING_DEFAULT_CALENDAR_2);
	
	
	
	//init music layer
	animated_layer[MUSIC_LAYER] = layer_create(GRect(144, 100, 144, 45)); // (GRect(144, 124, 144, 45));
	layer_add_child(window_layer, animated_layer[MUSIC_LAYER]);
	
	music_artist_layer = text_layer_create(GRect(6, 0, 132, 21)); // (GRect(6, 0, 132, 21));
	text_layer_set_text_alignment(music_artist_layer, GTextAlignmentLeft);
	text_layer_set_text_color(music_artist_layer, GColorWhite);
	text_layer_set_background_color(music_artist_layer, GColorClear);
	text_layer_set_font(music_artist_layer, fonts_get_system_font(FONT_KEY_GOTHIC_18));
	layer_add_child(animated_layer[MUSIC_LAYER], text_layer_get_layer(music_artist_layer));
	text_layer_set_text(music_artist_layer, STRING_DEFAULT_MUSIC_1); 	


	music_song_layer = text_layer_create(GRect(6, 15, 132, 28)); // (GRect(6, 15, 132, 28));
	text_layer_set_text_alignment(music_song_layer, GTextAlignmentLeft);
	text_layer_set_text_color(music_song_layer, GColorWhite);
	text_layer_set_background_color(music_song_layer, GColorClear);
	text_layer_set_font(music_song_layer, fonts_get_system_font(FONT_KEY_GOTHIC_24_BOLD));
	layer_add_child(animated_layer[MUSIC_LAYER], text_layer_get_layer(music_song_layer));
	text_layer_set_text(music_song_layer, STRING_DEFAULT_MUSIC_2);


	active_layer = CALENDAR_LAYER;

	reset();

	load_memories();
  	//tick_timer_service_subscribe(MINUTE_UNIT, handle_minute_tick);
	tick_timer_service_subscribe(SECOND_UNIT, handle_second_tick);

	bluetooth_connection_service_subscribe(bluetoothChanged);
	battery_state_service_subscribe(batteryChanged);

}

// Calendar Window
static void toggle_music_blink(ClickRecognizerRef recognizer, void *context){
	vibes_short_pulse();
	/*for (int i = 0; i < 50; ++i)
	{
		window_set_background_color(calendar_window, GColorWhite);
		psleep(468);
		window_set_background_color(calendar_window, GColorBlack);
		psleep(468);
	} */
}

static void switch_to_calendar_window(ClickRecognizerRef recognizer, void *context){
	calendar_window = window_create();
	window_set_fullscreen(calendar_window, true);
  	window_set_click_config_provider(calendar_window, click_config_provider_calendar);
 	/* window_set_window_handlers(calendar_window, (WindowHandlers) {
    	.load = window_load,
    	.unload = window_unload,
		.appear = window_appear,
		.disappear = window_disappear
  	}); */
	window_set_background_color(calendar_window, GColorBlack);
	
	// Layers
	Layer *calendar_window_layer = window_get_root_layer(calendar_window);

  	calendar_event_one = layer_create(GRect(0, 0, 144, 45)); // GRect(0, 78, 144, 45));
	layer_add_child(calendar_window_layer, calendar_event_one);
	text_event_layer_one = text_layer_create(GRect(0, 0, 144, 45));
	text_layer_set_background_color(text_event_layer_one,GColorBlack);
	layer_add_child(calendar_event_one,text_layer_get_layer(text_event_layer_one));
	text_layer_set_text(text_event_layer_one, text_event_one);
  	window_stack_push(calendar_window, animated);
}

static void destroy_calendar_window(ClickRecognizerRef recognizer, void *context){
	if (calendar_window != NULL) {
		text_layer_destroy(text_event_layer_one);
		layer_destroy(calendar_event_one);
		window_stack_remove(calendar_window,animated);
		window_destroy(calendar_window);
		calendar_window = NULL;
	}
}


// Goodbye
static void deinit(void) {
	store_for_later();
	
	destroy_calendar_window(NULL,NULL);

	property_animation_destroy((PropertyAnimation*)ani_in);
	property_animation_destroy((PropertyAnimation*)ani_out);
	
	APP_LOG(APP_LOG_LEVEL_DEBUG,"    Destroy EVERYTHING");
	
	if (timerUpdateCalendar != NULL)
		app_timer_cancel(timerUpdateCalendar);
	timerUpdateCalendar = NULL;
	
	if (timerUpdateWeather != NULL)	
		app_timer_cancel(timerUpdateWeather);
	timerUpdateWeather = NULL;
	
	if (timerUpdateMusic != NULL)
		app_timer_cancel(timerUpdateMusic);
	timerUpdateMusic = NULL;

	if (hideMusicLayer != NULL)
		app_timer_cancel(hideMusicLayer);
	hideMusicLayer = NULL;

	if (long_light_timer != NULL)
		app_timer_cancel(long_light_timer);
	long_light_timer = NULL;

	bitmap_layer_destroy(background_image);
	layer_destroy(weather_layer);
	bitmap_layer_destroy(battery_image_layer);
	bitmap_layer_destroy(battery_pbl_image_layer);
	text_layer_destroy(text_battery_layer);
	layer_destroy(battery_layer);
	layer_destroy(battery_pbl_layer);
	text_layer_destroy(text_weather_cond_layer);
	bitmap_layer_destroy(weather_image);
	text_layer_destroy(text_weather_temp_layer);
	text_layer_destroy(text_date_layer);
	text_layer_destroy(text_time_layer);
	text_layer_destroy(calendar_date_layer);
	text_layer_destroy(calendar_text_layer);
	text_layer_destroy(music_artist_layer);
	text_layer_destroy(music_song_layer);
	text_layer_destroy(text_mail_layer);
	text_layer_destroy(text_sms_layer);
	text_layer_destroy(text_phone_layer);
	text_layer_destroy(text_pebble_battery_layer);
	text_layer_destroy(text_seconds_layer);
	layer_destroy(status_layer);
	
	if (calendar_date_str != NULL) {
 		free(calendar_date_str);
 		APP_LOG(APP_LOG_LEVEL_DEBUG,"[F] calendar_date_str memory is now free");
 	}
 	fonts_unload_custom_font(font_date);
	fonts_unload_custom_font(font_time);
	fonts_unload_custom_font(font_secs);

	for (int i=0; i<NUM_LAYERS; i++) {
		if (animated_layer[i]!=NULL)
			layer_destroy(animated_layer[i]);
	}

	for (int i=0; i<NUM_WEATHER_IMAGES; i++) {
	  	gbitmap_destroy(weather_status_imgs[i]);
	}
	

	gbitmap_destroy(bg_image);
	gbitmap_destroy(battery_image);
	gbitmap_destroy(battery_pbl_image);

	tick_timer_service_unsubscribe();
	bluetooth_connection_service_unsubscribe();
	battery_state_service_unsubscribe();

  
  window_destroy(main_window);
}


static void updateWeather(void *data) {
	sendCommand(SM_STATUS_UPD_WEATHER_KEY);	
}

static void updateCalendar(void *data) {
	sendCommand(SM_STATUS_UPD_CAL_KEY);	
}

static void updateMusic(void *data) {
	sendCommand(SM_SONG_LENGTH_KEY);	
}

static void auto_switch(void *data){
	hideMusicLayer = NULL;
	if (active_layer == MUSIC_LAYER) animate_layers();
}

void rcv(DictionaryIterator *received, void *context) {
	// Got a message callback
	Tuple *t;

	t=dict_find(received, SM_WEATHER_COND_KEY); 
	if (t!=NULL) {
		memcpy(weather_cond_str, t->value->cstring, strlen(t->value->cstring));
        weather_cond_str[strlen(t->value->cstring)] = '\0';
		text_layer_set_text(text_weather_cond_layer, weather_cond_str); 	
	}

	t=dict_find(received, SM_WEATHER_TEMP_KEY); 
	if (t!=NULL) {
		memcpy(weather_temp_str, t->value->cstring, strlen(t->value->cstring));
        weather_temp_str[strlen(t->value->cstring)] = '\0';
		text_layer_set_text(text_weather_temp_layer, weather_temp_str); 
		layer_set_hidden(text_layer_get_layer(text_weather_cond_layer), false);
/*		layer_set_hidden(text_layer_get_layer(text_weather_cond_layer), true);
		layer_set_hidden(text_layer_get_layer(text_weather_temp_layer), false);
			
*/	}

	t=dict_find(received, SM_WEATHER_ICON_KEY); 
	if (t!=NULL) {
		bitmap_layer_set_bitmap(weather_image, weather_status_imgs[t->value->uint8]);	  	
	}

	t=dict_find(received, SM_COUNT_MAIL_KEY); 
	if (t!=NULL) {
		static char mail_count_str[5];
		memcpy(mail_count_str, t->value->cstring, strlen(t->value->cstring));
        mail_count_str[strlen(t->value->cstring)] = '\0';
		text_layer_set_text(text_mail_layer, mail_count_str); 	
	}

	t=dict_find(received, SM_COUNT_SMS_KEY); 
	if (t!=NULL) {
		static char sms_count_str[5];
		memcpy(sms_count_str, t->value->cstring, strlen(t->value->cstring));
        sms_count_str[strlen(t->value->cstring)] = '\0';
		text_layer_set_text(text_sms_layer, sms_count_str); 	
	}

	t=dict_find(received, SM_COUNT_PHONE_KEY); 
	if (t!=NULL) {
		static char phone_count_str[5];
		memcpy(phone_count_str, t->value->cstring, strlen(t->value->cstring));
        phone_count_str[strlen(t->value->cstring)] = '\0';
		text_layer_set_text(text_phone_layer, phone_count_str); 	
	}

	t=dict_find(received, SM_COUNT_BATTERY_KEY); 
	if (t!=NULL) {
		layer_set_hidden(bitmap_layer_get_layer(battery_image_layer), false);
		layer_set_hidden(battery_layer, false);
		layer_set_hidden(text_layer_get_layer(text_battery_layer), false);
		batteryPercent = t->value->uint8;
		layer_mark_dirty(battery_layer);
		snprintf(string_buffer, sizeof(string_buffer), "%d", batteryPercent);
		text_layer_set_text(text_battery_layer, string_buffer ); 	
	}

	t=dict_find(received, SM_STATUS_CAL_TIME_KEY);   // I changed this if that's what you're looking for <-------------------------
 	if (t!=NULL) {
 		if (calendar_date_str != NULL) {
 			free(calendar_date_str);
 		}
 		static int num_chars;
 		num_chars = strlen(t->value->cstring);
 		calendar_date_str = (char *)malloc(sizeof(char) * num_chars);
 		if (calendar_date_str == NULL) {
 			APP_LOG(APP_LOG_LEVEL_ERROR,"[/] Malloc wasn't able to allocate memory (num_chars = %i)",num_chars);
 			return;
 		} else {
 			APP_LOG(APP_LOG_LEVEL_INFO,"[M] Malloc succesfully allocated memory (num_chars * sizeof(char) = %i * %i)",num_chars, (int)(sizeof(char)));
 			phone_is_connected = true;
 		}
 		memcpy(calendar_date_str, t->value->cstring, strlen(t->value->cstring));
        calendar_date_str[strlen(t->value->cstring)] = '\0';
 		text_layer_set_text(calendar_date_layer, calendar_date_str);
		APP_LOG(APP_LOG_LEVEL_DEBUG, "[R] Received DATA for Calendar, launching Appointment Module [apptDisplay]");
		apptDisplay(calendar_date_str);
  	}
	
	t=dict_find(received, SM_STATUS_CAL_TEXT_KEY); 
	if (t!=NULL) {
		memcpy(calendar_text_str, t->value->cstring, strlen(t->value->cstring));
        calendar_text_str[strlen(t->value->cstring)] = '\0';
		text_layer_set_text(calendar_text_layer, calendar_text_str); 	
		// Resize Calendar text if needed
		if(strlen(calendar_text_str) <= 15)
			text_layer_set_font(calendar_text_layer, fonts_get_system_font(FONT_KEY_GOTHIC_24_BOLD));
		else
			if(strlen(calendar_text_str) <= 18)
				text_layer_set_font(calendar_text_layer, fonts_get_system_font(FONT_KEY_GOTHIC_18_BOLD));
			else 
				text_layer_set_font(calendar_text_layer, fonts_get_system_font(FONT_KEY_GOTHIC_14_BOLD));
	}

	t=dict_find(received, SM_STATUS_MUS_ARTIST_KEY); 
	if (t!=NULL) {
		memcpy(music_artist_str1, t->value->cstring, strlen(t->value->cstring));
        music_artist_str1[strlen(t->value->cstring)] = '\0';
		text_layer_set_text(music_artist_layer, music_artist_str1); 	
	}

	t=dict_find(received, SM_STATUS_MUS_TITLE_KEY); 
	if (t!=NULL) {
		memcpy(music_title_str1, t->value->cstring, strlen(t->value->cstring));
        music_title_str1[strlen(t->value->cstring)] = '\0';
		APP_LOG(APP_LOG_LEVEL_DEBUG,"[R] New music title received is %s",music_title_str1);
		text_layer_set_text(music_song_layer, music_title_str1);
		if ((strncmp(last_text,music_title_str1,8) != 0) && (strncmp(music_title_str1,"No Title",8) != 0)) {
			strncpy(last_text,music_title_str1,8);
			if (active_layer != MUSIC_LAYER) 
				animate_layers();
			if (hideMusicLayer != NULL) 
				app_timer_cancel(hideMusicLayer);
			hideMusicLayer = app_timer_register(5000 , auto_switch, NULL);
		}
	}


	t=dict_find(received, SM_STATUS_UPD_WEATHER_KEY); 
	if (t!=NULL) {
		int interval = t->value->int32 * 1000;

		if (timerUpdateWeather != NULL)
			app_timer_cancel(timerUpdateWeather);
		timerUpdateWeather = app_timer_register(interval , updateWeather, NULL);
	}

	t=dict_find(received, SM_STATUS_UPD_CAL_KEY); 
	if (t!=NULL) {
		int interval = t->value->int32 * 1000;

		if (timerUpdateCalendar != NULL)
			app_timer_cancel(timerUpdateCalendar);
		timerUpdateCalendar = app_timer_register(interval , updateCalendar, NULL);
	}

	t=dict_find(received, SM_SONG_LENGTH_KEY); 
	if (t!=NULL) {
		int interval = t->value->int32 * 1000;
		
		if (timerUpdateMusic != NULL)
			app_timer_cancel(timerUpdateMusic);
		timerUpdateMusic = app_timer_register(interval , updateMusic, NULL);

	}

}

int main(void) {
	app_message_open(app_message_inbox_size_maximum(), app_message_outbox_size_maximum() );
	app_message_register_inbox_received(rcv);
	
  init();


  app_event_loop();
  app_message_deregister_callbacks();

  deinit();

}