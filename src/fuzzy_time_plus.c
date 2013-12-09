/*
  Fuzzy Time +
  Inspired by Fuzzy Time
  With Date, 24H display and Day of the Year
  Removed: Hour background inverts depending on AM or PM
 */

#include <pebble.h>
#include <english_time.h>

#define ANIMATION_DURATION 800
#define LINE_BUFFER_SIZE 50
#define WINDOW_NAME "fuzzy_time_plus"

Window *window;

typedef struct {
  TextLayer *layer[2];
  PropertyAnimation *layer_animation[2];
  char cur_time[LINE_BUFFER_SIZE];
  char new_time[LINE_BUFFER_SIZE];
} TextLine;

TextLayer *topbarLayer;
TextLayer *bottombarLayer;

TextLine line1;
TextLine line2;
TextLine line3;

static char str_topbar[LINE_BUFFER_SIZE];
static char str_bottombar[LINE_BUFFER_SIZE];

static char battery_text[] = "100%";

static bool busy_animating_in = false;
static bool busy_animating_out = false;
const int line1_y = 25;
const int line2_y = 58;
const int line3_y = 93;

GFont text_font;
GFont text_font_light;
GFont bar_font;

void animationInStoppedHandler(struct Animation *animation, bool finished, void *context) {
  busy_animating_in = false;
  //reset cur_time
  TextLine *line = (TextLine *) context;
  strcpy(line->cur_time, line->new_time);
}

void animationOutStoppedHandler(struct Animation *animation, bool finished, void *context) {
  //reset out layer to x=144
  TextLayer *outside = (TextLayer *)context;
  GRect rect = layer_get_frame(text_layer_get_layer(outside));
  rect.origin.x = 144;
  layer_set_frame(text_layer_get_layer(outside), rect);

  busy_animating_out = false;
}

void updateLayer(TextLine *animating_line) {
  
  TextLayer *inside, *outside;
  GRect rect = layer_get_frame(text_layer_get_layer(animating_line->layer[0]));

  inside = (rect.origin.x == 0) ? animating_line->layer[0] : animating_line->layer[1];
  outside = (inside == animating_line->layer[0]) ? animating_line->layer[1] : animating_line->layer[0];

  GRect in_rect = layer_get_frame(text_layer_get_layer(outside));
  GRect out_rect = layer_get_frame(text_layer_get_layer(inside));

  in_rect.origin.x -= 144;
  out_rect.origin.x -= 144;

 //animate out current layer
  busy_animating_out = true;
  animating_line->layer_animation[1] = property_animation_create_layer_frame(text_layer_get_layer(inside), NULL, &out_rect);
  animation_set_duration(&animating_line->layer_animation[1]->animation, ANIMATION_DURATION);
  animation_set_curve(&animating_line->layer_animation[1]->animation, AnimationCurveEaseOut);
  animation_set_handlers(&animating_line->layer_animation[1]->animation, (AnimationHandlers) {
    .stopped = (AnimationStoppedHandler)animationOutStoppedHandler
  }, (void *)inside);
  animation_schedule(&animating_line->layer_animation[1]->animation);

  text_layer_set_text(outside, animating_line->new_time);
  text_layer_set_text(inside, animating_line->cur_time);
  
  
  //animate in new layer
  busy_animating_in = true;
  animating_line->layer_animation[0] = property_animation_create_layer_frame(text_layer_get_layer(outside), NULL, &in_rect);
  animation_set_duration(&animating_line->layer_animation[0]->animation, ANIMATION_DURATION);
  animation_set_curve(&animating_line->layer_animation[0]->animation, AnimationCurveEaseOut);
  animation_set_handlers(&animating_line->layer_animation[0]->animation, (AnimationHandlers) {
    .stopped = (AnimationStoppedHandler)animationInStoppedHandler
  }, (void *)animating_line);
  animation_schedule(&animating_line->layer_animation[0]->animation);
}
void updateBottombar(struct tm *t) {
  //Let's get the new time and date
  strftime(str_bottombar, sizeof(str_bottombar), " %H:%M | Day %j | ", t);
  
  BatteryChargeState charge_state = battery_state_service_peek();
  if (charge_state.is_charging) {
    snprintf(battery_text, sizeof(battery_text), "--%%");
  } else {
    snprintf(battery_text, sizeof(battery_text), "%d%% ", charge_state.charge_percent);
  }
  
  strncat(str_bottombar, battery_text, LINE_BUFFER_SIZE - strlen(str_bottombar));
  //Let's update the bottom bar anyway
  text_layer_set_text(bottombarLayer, str_bottombar);
}
void update_watch(struct tm *t, TimeUnits units_changed) {
 
  updateBottombar(t);
  
  if(!(t->tm_min %5) || t->tm_min == 58 || t->tm_min == 1) {
	fuzzy_time(t->tm_hour, t->tm_min, line1.new_time, line2.new_time, line3.new_time);
	   //update hour only if changed
	if(strcmp(line1.new_time,line1.cur_time) != 0){
		updateLayer(&line1);
	}
	  //update min1 only if changed
	if(strcmp(line2.new_time,line2.cur_time) != 0){
		updateLayer(&line2);
	}
	  //update min2 only if changed happens on
	if(strcmp(line3.new_time,line3.cur_time) != 0){
		updateLayer(&line3);
	}
    if(units_changed & DAY_UNIT) {
	  strftime(str_topbar, sizeof(str_topbar), "%A | %e %b", t);
	  text_layer_set_text(topbarLayer, str_topbar);
	}

  }
 }

void init_watch(struct tm* t) {
  fuzzy_time(t->tm_hour, t->tm_min, line1.new_time, line2.new_time, line3.new_time);
  strftime(str_topbar, sizeof(str_topbar), "%A | %e %b", t);
  
  updateBottombar(t);
  
  text_layer_set_text(topbarLayer, str_topbar);
  text_layer_set_text(bottombarLayer, str_bottombar);

  strcpy(line1.cur_time, line1.new_time);
  strcpy(line2.cur_time, line2.new_time);
  strcpy(line3.cur_time, line3.new_time);

 
  text_layer_set_text(line1.layer[0], line1.cur_time);
  text_layer_set_text(line2.layer[0], line2.cur_time);
  text_layer_set_text(line3.layer[0], line3.cur_time);
}

static void handle_minute_tick(struct tm *tick_time, TimeUnits units_changed) {
  if (busy_animating_out || busy_animating_in) return;

  update_watch(tick_time, units_changed);  	
}

// Handle the start-up of the app
void handle_init_app() {

  // Create our app's base window
  window = window_create();
  window_stack_push(window, true);
  window_set_background_color(window, GColorBlack);
  
  Layer *window_layer = window_get_root_layer(window);

  // Init the text layers used to show the time

  // Set the fonts
  text_font = fonts_load_custom_font(resource_get_handle(RESOURCE_ID_FONT_TWCENMT_36_BOLD));
  text_font_light = fonts_load_custom_font(resource_get_handle(RESOURCE_ID_FONT_TWCENMT_39));
  bar_font = fonts_get_system_font(FONT_KEY_GOTHIC_14);
  
  // line1
  line1.layer[0] = text_layer_create(GRect(0, line1_y, 144, 42));
  text_layer_set_text_color(line1.layer[0], GColorWhite);
  text_layer_set_background_color(line1.layer[0], GColorClear);
  text_layer_set_font(line1.layer[0], text_font_light);
  text_layer_set_text_alignment(line1.layer[0], GTextAlignmentLeft);
  
  line1.layer[1] = text_layer_create(GRect(144, line1_y, 144, 42));
  text_layer_set_text_color(line1.layer[1], GColorWhite);
  text_layer_set_background_color(line1.layer[1], GColorClear);
  text_layer_set_font(line1.layer[1], text_font_light);
  text_layer_set_text_alignment(line1.layer[1], GTextAlignmentLeft);

  // line2
  line2.layer[0] = text_layer_create(GRect(0, line2_y, 144, 42));
  text_layer_set_text_color(line2.layer[0], GColorWhite);
  text_layer_set_background_color(line2.layer[0], GColorClear);
  text_layer_set_font(line2.layer[0], text_font_light);
  text_layer_set_text_alignment(line2.layer[0], GTextAlignmentLeft);

  line2.layer[1] = text_layer_create(GRect(144, line2_y, 144, 42));
  text_layer_set_text_color(line2.layer[1], GColorWhite);
  text_layer_set_background_color(line2.layer[1], GColorClear);
  text_layer_set_font(line2.layer[1], text_font_light);
  text_layer_set_text_alignment(line2.layer[1], GTextAlignmentLeft);
  
  // line3
  line3.layer[0] = text_layer_create(GRect(0, line3_y, 144, 52));
  text_layer_set_text_color(line3.layer[0], GColorWhite);
  text_layer_set_background_color(line3.layer[0], GColorClear);
  text_layer_set_font(line3.layer[0], text_font);
  text_layer_set_text_alignment(line3.layer[0], GTextAlignmentLeft);

  line3.layer[1] = text_layer_create(GRect(144, line3_y, 144, 52));
  text_layer_set_text_color(line3.layer[1], GColorWhite);
  text_layer_set_background_color(line3.layer[1], GColorClear);
  text_layer_set_font(line3.layer[1], text_font);
  text_layer_set_text_alignment(line3.layer[1], GTextAlignmentLeft);

  //text_layer_init(line3_bg, GRect(144, line3_y, 144, 52));

  // top bar
  topbarLayer = text_layer_create(GRect(0, 0, 144, 18));
  text_layer_set_text_color(topbarLayer, GColorWhite);
  text_layer_set_background_color(topbarLayer, GColorClear);
  text_layer_set_font(topbarLayer, bar_font);
  text_layer_set_text_alignment(topbarLayer, GTextAlignmentCenter);

  // bottom bar bottombarLayer
  bottombarLayer = text_layer_create(GRect(0, 150, 144, 18));
  text_layer_set_text_color(bottombarLayer, GColorWhite);
  text_layer_set_background_color(bottombarLayer, GColorClear);
  text_layer_set_font(bottombarLayer, bar_font);
  text_layer_set_text_alignment(bottombarLayer, GTextAlignmentCenter);

  // Ensures time is displayed immediately (will break if NULL tick event accessed).
  // (This is why it's a good idea to have a separate routine to do the update itself.)
 
  time_t now = time(NULL);
  struct tm *t = localtime(&now);
  init_watch(t);

  //layer_add_child(&window.layer, line3_bg.layer);
  layer_add_child(window_layer, text_layer_get_layer(line3.layer[0]));
  layer_add_child(window_layer, text_layer_get_layer(line3.layer[1]));
  layer_add_child(window_layer, text_layer_get_layer(line2.layer[0]));
  layer_add_child(window_layer, text_layer_get_layer(line2.layer[1])); 
  layer_add_child(window_layer, text_layer_get_layer(line1.layer[0]));
  layer_add_child(window_layer, text_layer_get_layer(line1.layer[1]));
  layer_add_child(window_layer, text_layer_get_layer(bottombarLayer)); 
  layer_add_child(window_layer, text_layer_get_layer(topbarLayer));
  
  tick_timer_service_subscribe(MINUTE_UNIT, handle_minute_tick);
}


void destroyTextLineLayers(TextLine *textLine) {
	text_layer_destroy(textLine->layer[0]);
	text_layer_destroy(textLine->layer[1]);
}

void handle_deinit(void) {
  text_layer_destroy(topbarLayer);
  text_layer_destroy(bottombarLayer);
  destroyTextLineLayers(&line1);
  destroyTextLineLayers(&line2);
  destroyTextLineLayers(&line3);
  window_destroy(window);
}
int main(void) {
  handle_init_app();
  app_event_loop();
  handle_deinit();
}
