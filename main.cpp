/* OpenSprinkler Unified (AVR/RPI/BBB/LINUX) Firmware
 * Copyright (C) 2015 by Ray Wang (ray@opensprinkler.com)
 *
 * Main loop
 * Feb 2015 @ OpenSprinkler.com
 *
 * This file is part of the OpenSprinkler Firmware
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see
 * <http://www.gnu.org/licenses/>.
 */

#include <limits.h>

#include "OpenSprinkler.h"
#include "program.h"
#include "weather.h"
#include "server.h"

#if defined(ARDUINO)

#include "Wire.h"

#ifdef ESP8266
  #include <FS.h>
  #include "ets_sys.h"
	#include "osapi.h"                                         // for os timers
	#include "os_type.h"
  #include "gpio.h"
  #include "espconnect.h"
  char ether_buffer[ETHER_BUFFER_SIZE];
#else
  #include <SdFat.h>
  byte Ethernet::buffer[ETHER_BUFFER_SIZE]; // Ethernet packet buffer
  SdFat sd;                                 // SD card object
#endif

unsigned long getNtpTime();

#else // header and defs for RPI/BBB

#include <sys/stat.h>
#include <netdb.h>
#include "etherport.h"
#include "gpio.h"
char ether_buffer[ETHER_BUFFER_SIZE];
EthernetServer *m_server = 0;
EthernetClient *m_client = 0;

#endif

void reset_all_stations();
void reset_all_stations_immediate();
void push_message(byte type, uint32_t lval=0, float fval=0.f, const char* sval=NULL);
void manual_start_program(byte, byte);
void httpget_callback(byte, uint16_t, uint16_t);


// Small variations have been added to the timing values below
// to minimize conflicting events
#define NTP_SYNC_INTERVAL       86403L  // NYP sync interval, 24 hrs
#define RTC_SYNC_INTERVAL       60      // RTC sync interval, 60 secs
#define CHECK_NETWORK_INTERVAL  601     // Network checking timeout, 10 minutes
#define CHECK_WEATHER_TIMEOUT   7201    // Weather check interval: 2 hours
#define CHECK_WEATHER_SUCCESS_TIMEOUT 86433L // Weather check success interval: 24 hrs
#define LCD_BACKLIGHT_TIMEOUT   15      // LCD backlight timeout: 15 secs
#define PING_TIMEOUT            200     // Ping test timeout: 200 ms

extern char tmp_buffer[];       // scratch buffer

const unsigned int debounce_period = 1; // milli-seconds. Change as appropriate.
const unsigned int sensor_poll_period = 10;	// milli-seconds
volatile bool debounce_in_progress = false;
volatile bool prev_flow_sensor_state;


#ifdef ESP8266
os_timer_t flow_sensor_poll_timer;
ESP8266WebServerSecure *wifi_server = NULL;
static uint16_t led_blink_ms = LED_FAST_BLINK;
#endif
// ====== Object defines ======
OpenSprinkler os; // OpenSprinkler object
ProgramData pd;   // ProgramdData object


//flow globals
ulong flow_count = 0;
ulong flow_gallons_count = 0;
ulong flow_station_start_gallons = 0;
float flow_station_gallons = 0;
byte prev_flow_state = HIGH;


#ifdef ESP8266
#define SENSOR_DEBOUNCE_COUNT_MAX	5	//5*debounce_period msec
#define SENSOR_INCORRECT_SAMPLES_MIN	2	

unsigned int flow_sensor_high_sample;
unsigned int sensor_debounce_count;

void flowsenseor_poll_timer_cb(void *arg){
	bool flow_sensor_state = digitalReadExt(PIN_FLOWSENSOR);
	if(debounce_in_progress == true){
		//sample pin
		if(flow_sensor_state == HIGH){
			flow_sensor_high_sample++;
		}
		sensor_debounce_count++;
		if(sensor_debounce_count == SENSOR_DEBOUNCE_COUNT_MAX ||
			flow_sensor_high_sample > SENSOR_INCORRECT_SAMPLES_MIN){
			//stop debounce and sampling
			debounce_in_progress = false;
			if(flow_sensor_high_sample < SENSOR_INCORRECT_SAMPLES_MIN){
				//this is real active low pulse
				flow_count++;
			}
		}
	}
	else if(prev_flow_sensor_state == HIGH && flow_sensor_state == LOW && debounce_in_progress == false){
		//falling edge detected.
		//start debounce count and sample
		sensor_debounce_count = 0;
		debounce_in_progress = true;
		flow_sensor_high_sample = 0;
	}
	prev_flow_sensor_state = flow_sensor_state;
}
void setup_flowsensor_polling_timer(void) {
	os_timer_disarm(&flow_sensor_poll_timer);
	os_timer_setfn(&flow_sensor_poll_timer,  (os_timer_func_t *)flowsenseor_poll_timer_cb, NULL);
	os_timer_arm(&flow_sensor_poll_timer, sensor_poll_period, 1);        // perodic timer
}
#endif
void flow_poll() {
  byte curr_flow_state = digitalReadExt(PIN_FLOWSENSOR);
  if(os.options[OPTION_SENSOR1_TYPE]!=SENSOR_TYPE_FLOW) return;

#ifdef ESP8266  
  if(!(prev_flow_state==HIGH && curr_flow_state==LOW)) {
    prev_flow_state = curr_flow_state;
    return;
  }
  prev_flow_state = curr_flow_state;
#endif

  ulong curr = millis();

  if(curr <= os.flowcount_time_ms) return;  // debounce threshold: 1ms
  flow_count++;
  os.flowcount_time_ms = curr;

}

volatile byte flow_isr_flag = false;
/** Flow sensor interrupt service routine */
#ifdef ESP8266
ICACHE_RAM_ATTR void flow_isr() // for ESP8266, ISR must be marked ICACHE_RAM_ATTR
#else
void flow_isr()
#endif
{
  flow_isr_flag = true;

}

//flow logs helper functions
inline bool is_hour(ulong timeSec, int hours){
	int hr = (int) ((timeSec / (60 * 60)) % 24);
	if(hr == hours){
		return true;
	}
	return false;
}

inline bool is_minute(ulong timeSec,int minutes){
	int min = (int) ((timeSec /60) % 60);
	if(min == minutes){
		return true;
	}
	return false;
}

inline bool is_time(ulong timeSec, int hours, int minutes){
	return (is_minute(timeSec, minutes) && is_hour(timeSec, hours));
}

inline bool addition_is_safe(ulong a, ulong b) {
    if (( b > 0) && (a > ULONG_MAX - b)) return false;
    return true;
}

#if defined(ARDUINO)
// ====== UI defines ======
static char ui_anim_chars[3] = {'.', 'o', 'O'};

#define UI_STATE_DEFAULT   0
#define UI_STATE_DISP_IP   1
#define UI_STATE_DISP_GW   2
#define UI_STATE_RUNPROG   3

static byte ui_state = UI_STATE_DEFAULT;
static byte ui_state_runprog = 0;

#ifdef ESP8266
bool ui_confirm(PGM_P str) {
  os.lcd_print_line_clear_pgm(str, 0);
  os.lcd_print_line_clear_pgm(PSTR("(B1:No, B3:Yes)"), 1);
  byte button;
  ulong timeout = millis()+4000;
  do {
    button = os.button_read(BUTTON_WAIT_NONE);
    if((button&BUTTON_MASK)==BUTTON_3 && (button&BUTTON_FLAG_DOWN)) return true;
    if((button&BUTTON_MASK)==BUTTON_1 && (button&BUTTON_FLAG_DOWN)) return false;
    delay(10);
  } while(millis() < timeout);
  return false;
}
#endif

void ui_state_machine() {
 
#ifdef ESP8266
  // process screen led
  static ulong led_toggle_timeout = 0;
  if(led_blink_ms) {
    if(millis()>led_toggle_timeout) {
      os.toggle_screen_led();
      led_toggle_timeout = millis() + led_blink_ms;
    }
  }
#endif  

  if (!os.button_timeout) {
    os.lcd_set_brightness(0);
    ui_state = UI_STATE_DEFAULT;  // also recover to default state
  }

  // read button, if something is pressed, wait till release
  byte button = os.button_read(BUTTON_WAIT_HOLD);

  if (button & BUTTON_FLAG_DOWN) {   // repond only to button down events
    os.button_timeout = LCD_BACKLIGHT_TIMEOUT;
    os.lcd_set_brightness(1);
  } else {
    return;
  }

  switch(ui_state) {
  case UI_STATE_DEFAULT:
    switch (button & BUTTON_MASK) {
    case BUTTON_1:
      if (button & BUTTON_FLAG_HOLD) {  // holding B1
        if (digitalReadExt(PIN_BUTTON_3)==0) { // if B3 is pressed while holding B1, run a short test (internal test)
          #ifdef ESP8266
          if(!ui_confirm(PSTR("Start 2s test?"))) {ui_state = UI_STATE_DEFAULT; break;}
          #endif
          manual_start_program(255, 0);
        } else if (digitalReadExt(PIN_BUTTON_2)==0) { // if B2 is pressed while holding B1, display gateway IP
          #ifdef ESP8266
          os.lcd.clear(0, 1);
          os.lcd.setCursor(0, 0);
          os.lcd.print(WiFi.gatewayIP());
          #else
          os.lcd.clear();
          os.lcd_print_ip(ether.gwip, 0);
          #endif
          os.lcd.setCursor(0, 1);
          os.lcd_print_pgm(PSTR("(gwip)"));
          ui_state = UI_STATE_DISP_IP;
        } else {  // if no other button is clicked, stop all zones
          #ifdef ESP8266
          if(!ui_confirm(PSTR("Stop all zones?"))) {ui_state = UI_STATE_DEFAULT; break;}
          #endif
          reset_all_stations();
        }
      } else {  // clicking B1: display device IP and port
        #ifdef ESP8266
        os.lcd.clear(0, 1);        
        os.lcd.setCursor(0, 0);
        os.lcd.print(WiFi.localIP());
        os.lcd.setCursor(0, 1);
        os.lcd_print_pgm(PSTR(":"));
        uint16_t httpport = (uint16_t)(os.options[OPTION_HTTPPORT_1]<<8) + (uint16_t)os.options[OPTION_HTTPPORT_0];
        os.lcd.print(httpport);
        #else
        os.lcd.clear();
        os.lcd_print_ip(ether.myip, 0);
        os.lcd.setCursor(0, 1);
        os.lcd_print_pgm(PSTR(":"));
        os.lcd.print(ether.hisport);
        #endif
        os.lcd_print_pgm(PSTR(" (ip:port)"));
        ui_state = UI_STATE_DISP_IP;
      }
      break;
    case BUTTON_2:
      if (button & BUTTON_FLAG_HOLD) {  // holding B2
        if (digitalReadExt(PIN_BUTTON_1)==0) { // if B1 is pressed while holding B2, display external IP
          os.lcd_print_ip((byte*)(&os.nvdata.external_ip), 1);
          os.lcd.setCursor(0, 1);
          os.lcd_print_pgm(PSTR("(eip)"));
          ui_state = UI_STATE_DISP_IP;
        } else if (digitalReadExt(PIN_BUTTON_3)==0) {  // if B3 is pressed while holding B2, display last successful weather call
          //os.lcd.clear(0, 1);
          os.lcd_print_time(os.checkwt_success_lasttime);
          os.lcd.setCursor(0, 1);
          os.lcd_print_pgm(PSTR("(lswc)"));
          ui_state = UI_STATE_DISP_IP;          
        } else {  // if no other button is clicked, reboot
          #ifdef ESP8266
          if(!ui_confirm(PSTR("Reboot device?"))) {ui_state = UI_STATE_DEFAULT; break;}
          #endif
          os.reboot_dev();
        }
      } else {  // clicking B2: display MAC
        #ifdef ESP8266
        os.lcd.clear(0, 1);
        byte mac[6];
        WiFi.macAddress(mac);
        os.lcd_print_mac(mac);
        #else
        os.lcd.clear();
        os.lcd_print_mac(ether.mymac);
        #endif
        ui_state = UI_STATE_DISP_GW;
      }
      break;
    case BUTTON_3:
      if (button & BUTTON_FLAG_HOLD) {  // holding B3
        if (digitalReadExt(PIN_BUTTON_1)==0) {  // if B1 is pressed while holding B3, display up time
          os.lcd_print_time(os.powerup_lasttime);
          os.lcd.setCursor(0, 1);
          os.lcd_print_pgm(PSTR("(lupt)"));
          ui_state = UI_STATE_DISP_IP;              
        } else if(digitalReadExt(PIN_BUTTON_2)==0) {  // if B2 is pressed while holding B3, reset to AP and reboot
          #ifdef ESP8266
          if(!ui_confirm(PSTR("Reset to AP?"))) {ui_state = UI_STATE_DEFAULT; break;}
          os.reset_to_ap();
          #endif
        } else {  // if no other button is clicked, go to Run Program main menu
          os.lcd_print_line_clear_pgm(PSTR("Run a Program:"), 0);
          os.lcd_print_line_clear_pgm(PSTR("Click B3 to list"), 1);
          ui_state = UI_STATE_RUNPROG;
        }
      } else {  // clicking B3: Show pulses count - was "switch board display (cycle through master and all extension boards)"
    	  //os.status.display_board = (os.status.display_board + 1) % (os.nboards);
    	  os.lcd.clear(0, 1);
    	  os.lcd.setCursor(0, 0);
    	  os.lcd_print_pgm(PSTR("(Pulses count)"));
    	  ultoa(flow_count, tmp_buffer, 10);
    	  os.lcd.setCursor(0, 1);
    	  os.lcd.print(tmp_buffer);
    	  ui_state = UI_STATE_DISP_IP;
      }
      break;
    }
    break;
  case UI_STATE_DISP_IP:
  case UI_STATE_DISP_GW:
    ui_state = UI_STATE_DEFAULT;
    break;
  case UI_STATE_RUNPROG:
    if ((button & BUTTON_MASK)==BUTTON_3) {
      if (button & BUTTON_FLAG_HOLD) {
        // start
        manual_start_program(ui_state_runprog, 0);
        ui_state = UI_STATE_DEFAULT;
      } else {
        ui_state_runprog = (ui_state_runprog+1) % (pd.nprograms+1);
        os.lcd_print_line_clear_pgm(PSTR("Hold B3 to start"), 0);
        if(ui_state_runprog > 0) {
          ProgramStruct prog;
          pd.read(ui_state_runprog-1, &prog);
          os.lcd_print_line_clear_pgm(PSTR(" "), 1);
          os.lcd.setCursor(0, 1);
          os.lcd.print((int)ui_state_runprog);
          os.lcd_print_pgm(PSTR(". "));
          os.lcd.print(prog.name);
        } else {
          os.lcd_print_line_clear_pgm(PSTR("0. Test (1 min)"), 1);
        }
      }
    }
    break;
  }
}

// ======================
// Setup Function
// ======================
void do_setup() {
  /* Clear WDT reset flag. */
#ifdef ESP8266
  if(wifi_server) { delete wifi_server; wifi_server = NULL; }
  WiFi.persistent(false);
  led_blink_ms = LED_FAST_BLINK;
#else
  MCUSR &= ~(1<<WDRF);
#endif

  DEBUG_BEGIN(115200);
  os.begin();          // OpenSprinkler init
  os.options_setup();  // Setup options

  pd.init();            // ProgramData init

  setSyncInterval(RTC_SYNC_INTERVAL);  // RTC sync interval
  // if rtc exists, sets it as time sync source
  setSyncProvider(RTC.get);
  os.lcd_print_time(os.now_tz());  // display time to LCD
  os.powerup_lasttime = os.now_tz();
  
#ifndef ESP8266
  // enable WDT
  /* In order to change WDE or the prescaler, we need to
   * set WDCE (This will allow updates for 4 clock cycles).
   */
  WDTCSR |= (1<<WDCE) | (1<<WDE);
  /* set new watchdog timeout prescaler value */
  WDTCSR = 1<<WDP3 | 1<<WDP0;  // 8.0 seconds
  /* Enable the WD interrupt (note no reset). */
  WDTCSR |= _BV(WDIE);
#endif
  if (os.start_network()) {  // initialize network
    os.status.network_fails = 0;
  } else {
    os.status.network_fails = 1;
  }
  os.status.req_network = 0;
  os.status.req_ntpsync = 1;

  os.apply_all_station_bits(); // reset station bits

  os.button_timeout = LCD_BACKLIGHT_TIMEOUT;

#ifdef ESP8266  
//  setup_debounce_timer();
	prev_flow_sensor_state = digitalReadExt(PIN_FLOWSENSOR);
  setup_flowsensor_polling_timer();
#endif
}

// Arduino software reset function
void(* sysReset) (void) = 0;

#ifndef ESP8266
volatile byte wdt_timeout = 0;
/** WDT interrupt service routine */
ISR(WDT_vect)
{
  wdt_timeout += 1;
  // this isr is called every 8 seconds
  if (wdt_timeout > 15) {
    // reset after 120 seconds of timeout
    sysReset();
  }
}
#endif

#else

void do_setup() {
  initialiseEpoch();   // initialize time reference for millis() and micros()
  os.begin();          // OpenSprinkler init
  os.options_setup();  // Setup options

  pd.init();            // ProgramData init

  if (os.start_network()) {  // initialize network
    DEBUG_PRINTLN("network established.");
    os.status.network_fails = 0;
  } else {
    DEBUG_PRINTLN("network failed.");
    os.status.network_fails = 1;
  }
  os.status.req_network = 0;
}
#endif

void write_log(byte type, ulong curr_time);
void schedule_all_stations(ulong curr_time);
void turn_off_station(byte sid, ulong curr_time);
void process_dynamic_events(ulong curr_time);
void check_network();
void check_weather();
void perform_ntp_sync();
void delete_log(char *name);
void clear_old_logs(ulong curr_time);

#ifdef ESP8266
void start_server_ap();
void start_server_client();
unsigned long reboot_timer = 0;
#else
void handle_web_request(char *p);
#endif

/** Main Loop */
void do_loop()
{
  /* If flow_isr_flag is on, do flow sensing.
     todo: not the most efficient way, as we can't do I2C inside ISR.
     need to figure out a more efficient way to do flow sensing */
#ifndef ESP8266     
  if(flow_isr_flag) {
    flow_isr_flag = false;
    flow_poll();
  }
#endif

  static ulong last_time = 0;
  static ulong last_minute = 0;

  byte bid, sid, s, pid, qid, bitvalue;
  ProgramStruct prog;

  os.status.mas = os.options[OPTION_MASTER_STATION];
  os.status.mas2= os.options[OPTION_MASTER_STATION_2];
  time_t curr_time = os.now_tz();
  // ====== Process Ethernet packets ======
#if defined(ARDUINO)  // Process Ethernet packets for Arduino
  #ifdef ESP8266
  static ulong connecting_timeout;
  switch(os.state) {
  case OS_STATE_INITIAL:
    if(os.get_wifi_mode()==WIFI_MODE_AP) {
      start_server_ap();
      os.state = OS_STATE_CONNECTED;
      connecting_timeout = 0;
    } else {
      led_blink_ms = LED_SLOW_BLINK;
      start_network_sta(os.wifi_config.ssid.c_str(), os.wifi_config.pass.c_str());
      os.config_ip();
      os.state = OS_STATE_CONNECTING;
      connecting_timeout = millis() + 120000L;
      os.lcd.setCursor(0, -1);
      os.lcd.print(F("Connecting to..."));      
      os.lcd.setCursor(0, 2);
      os.lcd.print(os.wifi_config.ssid);
    }
    break;
    
  case OS_STATE_TRY_CONNECT:
    led_blink_ms = LED_SLOW_BLINK;  
    start_network_sta_with_ap(os.wifi_config.ssid.c_str(), os.wifi_config.pass.c_str());
    os.config_ip();
    os.state = OS_STATE_CONNECTED;
    break;
   
  case OS_STATE_CONNECTING:
    if(WiFi.status() == WL_CONNECTED) {
      led_blink_ms = 0;
      os.set_screen_led(LOW);
      os.lcd.clear();
      start_server_client();
      os.state = OS_STATE_CONNECTED;
      connecting_timeout = 0;
    } else {
      if(millis()>connecting_timeout) {
        os.state = OS_STATE_INITIAL;
        DEBUG_PRINTLN(F("timeout"));
      }
    }
    break;
    
  case OS_STATE_CONNECTED:
    if(os.get_wifi_mode() == WIFI_MODE_AP) {
      wifi_server->handleClient();
      connecting_timeout = 0;
      if(os.get_wifi_mode()==WIFI_MODE_STA) {
        // already in STA mode, waiting to reboot
        break;
      }
      if(WiFi.status()==WL_CONNECTED && WiFi.localIP()) {
        os.wifi_config.mode = WIFI_MODE_STA;
        os.options_save(true);
        os.reboot_dev();
      }
    }
    else {
      if(WiFi.status() == WL_CONNECTED) {
        wifi_server->handleClient();
        connecting_timeout = 0;
      } else {
        os.state = OS_STATE_INITIAL;
      }
    }
    break;
  }
  
  #else // AVR
  
  uint16_t pos=ether.packetLoop(ether.packetReceive());
  if (pos>0) {  // packet received
    handle_web_request((char*)Ethernet::buffer+pos);
  }
  wdt_reset();  // reset watchdog timer
  wdt_timeout = 0;
  #endif
    
  ui_state_machine();

#else // Process Ethernet packets for RPI/BBB
  EthernetClient client = m_server->available();
  if (client) {
    while(true) {
      int len = client.read((uint8_t*) ether_buffer, ETHER_BUFFER_SIZE);
      if (len <=0) {
        if(!client.connected()) {
          break;
        } else {
          continue;
        }
      } else {
        m_client = &client;
        ether_buffer[len] = 0;  // put a zero at the end of the packet
        handle_web_request(ether_buffer);
        m_client = 0;
        break;
      }
    }
  }
#endif  // Process Ethernet packets

  // The main control loop runs once every second
  if (curr_time != last_time) {
    last_time = curr_time;
    if (os.button_timeout) os.button_timeout--;
    
    #ifdef ESP8266
    if(reboot_timer && millis() > reboot_timer) {
      os.reboot_dev();
    }
    #endif
      
#if defined(ARDUINO)
    if (!ui_state)
      os.lcd_print_time(os.now_tz());       // print time
#endif

    // ====== Check raindelay status ======
    if (os.status.rain_delayed) {
      if (curr_time >= os.nvdata.rd_stop_time) {  // rain delay is over
        os.raindelay_stop();
      }
    } else {
      if (os.nvdata.rd_stop_time > curr_time) {   // rain delay starts now
        os.raindelay_start();
      }
    }

    // ====== Check controller status changes and write log ======
    if (os.old_status.rain_delayed != os.status.rain_delayed) {
      if (os.status.rain_delayed) {
        // rain delay started, record time
        os.raindelay_start_time = curr_time;
        push_message(IFTTT_RAINSENSOR, LOGDATA_RAINDELAY, 1);
      } else {
        // rain delay stopped, write log
        write_log(LOGDATA_RAINDELAY, curr_time);
        push_message(IFTTT_RAINSENSOR, LOGDATA_RAINDELAY, 0);
      }
      os.old_status.rain_delayed = os.status.rain_delayed;
    }

    // ====== Check rain sensor status ======
    if (os.options[OPTION_SENSOR1_TYPE] == SENSOR_TYPE_RAIN) { // if a rain sensor is connected
      os.rainsensor_status();
      if (os.old_status.rain_sensed != os.status.rain_sensed) {
        if (os.status.rain_sensed) {
          // rain sensor on, record time
          os.sensor_lasttime = curr_time;
          push_message(IFTTT_RAINSENSOR, LOGDATA_RAINSENSE, 1);
        } else {
          // rain sensor off, write log
          if (curr_time>os.sensor_lasttime+10) {  // add a 10 second threshold
                                                  // to avoid faulty rain sensors generating
                                                  // too many log records
            write_log(LOGDATA_RAINSENSE, curr_time);
            push_message(IFTTT_RAINSENSOR, LOGDATA_RAINSENSE, 0);
          }
        }
        os.old_status.rain_sensed = os.status.rain_sensed;
      }
    }

    // ===== Check program switch status =====
    if (os.programswitch_status(curr_time)) {
      reset_all_stations_immediate(); // immediately stop all stations
      if(pd.nprograms > 0)  manual_start_program(1, 0);
    }

    // ====== Schedule program data ======
    ulong curr_minute = curr_time / 60;
    boolean match_found = false;
    RuntimeQueueStruct *q;
    // since the granularity of start time is minute
    // we only need to check once every minute
    if (curr_minute != last_minute) {
      last_minute = curr_minute;
      // check through all programs
      for(pid=0; pid<pd.nprograms; pid++) {
        pd.read(pid, &prog);
        if(prog.check_match(curr_time)) {
          // program match found
          // process all selected stations
          for(sid=0;sid<os.nstations;sid++) {
            bid=sid>>3;
            s=sid&0x07;
            // skip if the station is a master station (because master cannot be scheduled independently
            if ((os.status.mas==sid+1) || (os.status.mas2==sid+1))
              continue;

            // if station has non-zero water time and the station is not disabled
            if (prog.durations[sid] && !(os.station_attrib_bits_read(ADDR_NVM_STNDISABLE+bid)&(1<<s))) {
              // water time is scaled by watering percentage
              ulong water_time = water_time_resolve(prog.durations[sid]);
              // if the program is set to use weather scaling
              if (prog.use_weather) {
                byte wl = os.options[OPTION_WATER_PERCENTAGE];
                water_time = water_time * wl / 100;
                if (wl < 20 && water_time < 10) // if water_percentage is less than 20% and water_time is less than 10 seconds
                                                // do not water
                  water_time = 0;
              }

              if (water_time) {
                // check if water time is still valid
                // because it may end up being zero after scaling
                q = pd.enqueue();
                if (q) {
                  q->st = 0;
                  q->dur = water_time;
                  q->sid = sid;
                  q->pid = pid+1;
                  match_found = true;
                } else {
                  // queue is full
                }
              }// if water_time
            }// if prog.durations[sid]
          }// for sid
          if(match_found) push_message(IFTTT_PROGRAM_SCHED, pid, prog.use_weather?os.options[OPTION_WATER_PERCENTAGE]:100);
        }// if check_match
      }// for pid

      // calculate start and end time
      if (match_found) {
        schedule_all_stations(curr_time);

        // For debugging: print out queued elements
        /*DEBUG_PRINT("en:");
        for(q=pd.queue;q<pd.queue+pd.nqueue;q++) {
          DEBUG_PRINT("[");
          DEBUG_PRINT(q->sid);
          DEBUG_PRINT(",");
          DEBUG_PRINT(q->dur);
          DEBUG_PRINT(",");
          DEBUG_PRINT(q->st);
          DEBUG_PRINT("]");
        }
        DEBUG_PRINTLN("");*/
      }
    }//if_check_current_minute

    // ====== Run program data ======
    // Check if a program is running currently
    // If so, do station run-time keeping
    if (os.status.program_busy){
      // first, go through run time queue to assign queue elements to stations
      q = pd.queue;
      qid=0;
      for(;q<pd.queue+pd.nqueue;q++,qid++) {
        sid=q->sid;
        byte sqi=pd.station_qid[sid];
        // skip if station is already assigned a queue element
        // and that queue element has an earlier start time
        if(sqi<255 && pd.queue[sqi].st<q->st) continue;
        // otherwise assign the queue element to station
        pd.station_qid[sid]=qid;
      }
      // next, go through the stations and perform time keeping
      for(bid=0;bid<os.nboards; bid++) {
        bitvalue = os.station_bits[bid];
        for(s=0;s<8;s++) {
          byte sid = bid*8+s;

          // skip master station
          if (os.status.mas == sid+1) continue;
          if (os.status.mas2== sid+1) continue;
          if (pd.station_qid[sid]==255) continue;

          q = pd.queue + pd.station_qid[sid];
          // check if this station is scheduled, either running or waiting to run
          if (q->st > 0) {
            // if so, check if we should turn it off
            if (curr_time >= q->st+q->dur) {
              turn_off_station(sid, curr_time);
            }
          }
          // if current station is not running, check if we should turn it on
          if(!((bitvalue>>s)&1)) {
            if (curr_time >= q->st && curr_time < q->st+q->dur) {

              //turn_on_station(sid);
              os.set_station_bit(sid, 1);

              //record gallons count when the station start for consumption logs
              flow_station_start_gallons = flow_gallons_count;

            } //if curr_time > scheduled_start_time
          } // if current station is not running
        }//end_s
      }//end_bid

      // finally, go through the queue again and clear up elements marked for removal
      int qi;
      for(qi=pd.nqueue-1;qi>=0;qi--) {
        q=pd.queue+qi;
        if(!q->dur || curr_time>=q->st+q->dur)  {
          pd.dequeue(qi);
        }
      }

      // process dynamic events
      process_dynamic_events(curr_time);

      // activate / deactivate valves
      os.apply_all_station_bits();

      // check through runtime queue, calculate the last stop time of sequential stations
      pd.last_seq_stop_time = 0;
      ulong sst;
      byte re=os.options[OPTION_REMOTE_EXT_MODE];
      q = pd.queue;
      for(;q<pd.queue+pd.nqueue;q++) {
        sid = q->sid;
        bid = sid>>3;
        s = sid&0x07;
        // check if any sequential station has a valid stop time
        // and the stop time must be larger than curr_time
        sst = q->st + q->dur;
        if (sst>curr_time) {
          // only need to update last_seq_stop_time for sequential stations
          if (os.station_attrib_bits_read(ADDR_NVM_STNSEQ+bid)&(1<<s) && !re) {
            pd.last_seq_stop_time = (sst>pd.last_seq_stop_time ) ? sst : pd.last_seq_stop_time;
          }
        }
      }

      // if the runtime queue is empty
      // reset all stations
      if (!pd.nqueue) {
        // turn off all stations
        os.clear_all_station_bits();
        os.apply_all_station_bits();
        // reset runtime
        pd.reset_runtime();
        // reset program busy bit
        os.status.program_busy = 0;

        // in case some options have changed while executing the program
        os.status.mas = os.options[OPTION_MASTER_STATION]; // update master station
        os.status.mas2= os.options[OPTION_MASTER_STATION_2]; // update master2 station
      }
    }//if_some_program_is_running

    // handle master
    if (os.status.mas>0) {
      int16_t mas_on_adj = water_time_decode_signed(os.options[OPTION_MASTER_ON_ADJ]);
      int16_t mas_off_adj= water_time_decode_signed(os.options[OPTION_MASTER_OFF_ADJ]);
      byte masbit = 0;
      os.station_attrib_bits_load(ADDR_NVM_MAS_OP, (byte*)tmp_buffer);  // tmp_buffer now stores masop_bits
      for(sid=0;sid<os.nstations;sid++) {
        // skip if this is the master station
        if (os.status.mas == sid+1) continue;
        bid = sid>>3;
        s = sid&0x07;
        // if this station is running and is set to activate master
        if ((os.station_bits[bid]&(1<<s)) && (tmp_buffer[bid]&(1<<s))) {
          q=pd.queue+pd.station_qid[sid];
          // check if timing is within the acceptable range
          if (curr_time >= q->st + mas_on_adj &&
              curr_time <= q->st + q->dur + mas_off_adj) {
            masbit = 1;
            break;
          }
        }
      }
      os.set_station_bit(os.status.mas-1, masbit);
    }
    // handle master2
    if (os.status.mas2>0) {
      int16_t mas_on_adj_2 = water_time_decode_signed(os.options[OPTION_MASTER_ON_ADJ_2]);
      int16_t mas_off_adj_2= water_time_decode_signed(os.options[OPTION_MASTER_OFF_ADJ_2]);
      byte masbit2 = 0;
      os.station_attrib_bits_load(ADDR_NVM_MAS_OP_2, (byte*)tmp_buffer);  // tmp_buffer now stores masop2_bits
      for(sid=0;sid<os.nstations;sid++) {
        // skip if this is the master station
        if (os.status.mas2 == sid+1) continue;
        bid = sid>>3;
        s = sid&0x07;
        // if this station is running and is set to activate master
        if ((os.station_bits[bid]&(1<<s)) && (tmp_buffer[bid]&(1<<s))) {
          q=pd.queue+pd.station_qid[sid];
          // check if timing is within the acceptable range
          if (curr_time >= q->st + mas_on_adj_2 &&
              curr_time <= q->st + q->dur + mas_off_adj_2) {
            masbit2 = 1;
            break;
          }
        }
      }
      os.set_station_bit(os.status.mas2-1, masbit2);
    }    

    // process dynamic events
    process_dynamic_events(curr_time);

    // activate/deactivate valves
    os.apply_all_station_bits();

#if defined(ARDUINO)
    // process LCD display
    if (!ui_state) {
      os.lcd_print_station(1, ui_anim_chars[(unsigned long)curr_time%3]);
      #ifdef ESP8266
      if(os.get_wifi_mode()==WIFI_MODE_STA && WiFi.status()==WL_CONNECTED && WiFi.localIP()) {
        os.lcd.setCursor(0, 2);
        os.lcd.clear(2, 2);
        if(os.status.program_busy) {
          os.lcd.print(F("curr: "));
          uint16_t curr = os.read_current();
          os.lcd.print(curr);
          os.lcd.print(F(" mA"));
        }
      }
      #endif
    }
    
    // check safe_reboot condition
    if (os.status.safe_reboot) {
      // if no program is running at the moment
      if (!os.status.program_busy) {
        // and if no program is scheduled to run in the next minute
        bool willrun = false;
        for(pid=0; pid<pd.nprograms; pid++) {
          pd.read(pid, &prog);
          if(prog.check_match(curr_time+60)) {
            willrun = true;
            break;
          }
        }
        if (!willrun) {
          //os.reboot_dev(); //commented on 17 June 2019 to test reboot not happen in between of program
        }
      }
    }
#endif


    // real-time flow count
    static ulong flowcount_rt_start = 0;
    static ulong flow_log_last_time = 0;
    if (os.options[OPTION_SENSOR1_TYPE]==SENSOR_TYPE_FLOW) {
    	if (curr_time % FLOWCOUNT_RT_WINDOW == 0) {
    		os.flowcount_rt = (flow_count > flowcount_rt_start) ? flow_count - flowcount_rt_start: 0;
    		flowcount_rt_start = flow_count;
    		ulong curr_gallons = os.flowcount_rt;

    		if(addition_is_safe(flow_gallons_count, curr_gallons) == false){//prevent overflow
    			//rebase according the smallest count reference
    			ulong minref = (os.flowcount_log_start < flow_station_start_gallons ? os.flowcount_log_start : flow_station_start_gallons);
    			flow_gallons_count = flow_gallons_count - minref;
    			flow_station_start_gallons = flow_station_start_gallons - minref;
    			os.flowcount_log_start = os.flowcount_log_start - minref;
    		}
    		flow_gallons_count += curr_gallons;
    		

    		if(os.sensor_lasttime == 0){
    			os.flowcount_log_start = flow_gallons_count;
    			os.sensor_lasttime = curr_time;
    		}
    		else if((curr_time - flow_log_last_time) > 60 &&//check if 60 seconds passed since last log
    				(is_time(curr_time,FLOW_DAILY_LOG_HOUR,FLOW_DAILY_LOG_MINUTE) //is time for daily consumption log
    						||  (is_minute(curr_time, FLOW_DAILY_LOG_MINUTE) && (flow_gallons_count > os.flowcount_log_start) )))  //or is time for hourly consumption log - if has gallons to log
    		{
    			flow_log_last_time = curr_time;
    			//generate log/message
    			write_log(LOGDATA_FLOWSENSE, curr_time);
    			push_message(IFTTT_FLOWSENSOR, (flow_gallons_count > os.flowcount_log_start)?(flow_gallons_count - os.flowcount_log_start) : 0);
    			//reset for next log
    			os.flowcount_log_start = flow_gallons_count;
    			os.sensor_lasttime = curr_time;

    			if(is_time(curr_time,FLOW_DAILY_LOG_HOUR,FLOW_DAILY_LOG_MINUTE)){
    				//this is the end of the day log - use this time also to clear old logs
    				clear_old_logs(curr_time);
    			}
    		}
    	}
    }




    // perform ntp sync
    // instead of using curr_time, which may change due to NTP sync itself
    // we use Arduino's millis() method
    //if (curr_time % NTP_SYNC_INTERVAL == 0) os.status.req_ntpsync = 1;
    if((millis()/1000) % NTP_SYNC_INTERVAL==0) os.status.req_ntpsync = 1;
    perform_ntp_sync();

    // check network connection
    if (curr_time && (curr_time % CHECK_NETWORK_INTERVAL==0))  os.status.req_network = 1;
    check_network();

    // check weather
    check_weather();

    byte wuf = os.weather_update_flag;
    if(wuf) {
      if((wuf&WEATHER_UPDATE_EIP) | (wuf&WEATHER_UPDATE_WL)) {
        // at the moment, we only send notification if water level or external IP changed
        // the other changes, such as sunrise, sunset changes are ignored for notification
        push_message(IFTTT_WEATHER_UPDATE, (wuf&WEATHER_UPDATE_EIP)?os.nvdata.external_ip:0,
                                         (wuf&WEATHER_UPDATE_WL)?os.options[OPTION_WATER_PERCENTAGE]:-1);
      }
      os.weather_update_flag = 0;
    }
    static byte reboot_notification = 1;
    if(reboot_notification) {
      reboot_notification = 0;
      push_message(IFTTT_REBOOT);
    }

  }

  #if !defined(ARDUINO)
    delay(1); // For OSPI/OSBO/LINUX, sleep 1 ms to minimize CPU usage
  #endif
}

/** Make weather query */
void check_weather() {
  // do not check weather if
  // - network check has failed, or
  // - the controller is in remote extension mode
  if (os.status.network_fails>0 || os.options[OPTION_REMOTE_EXT_MODE]) return;

#ifdef ESP8266
  if (os.get_wifi_mode()!=WIFI_MODE_STA || WiFi.status()!=WL_CONNECTED || os.state!=OS_STATE_CONNECTED) return;
#endif

  ulong ntz = os.now_tz();
  if (os.checkwt_success_lasttime && (ntz > os.checkwt_success_lasttime + CHECK_WEATHER_SUCCESS_TIMEOUT)) {
    // if weather check has failed to return for too long, restart network
    os.checkwt_success_lasttime = 0;
    // mark for safe restart
    os.status.safe_reboot = 1;
    return;
  }
  if (!os.checkwt_lasttime || (ntz > os.checkwt_lasttime + CHECK_WEATHER_TIMEOUT)) {
    os.checkwt_lasttime = ntz;
    GetWeather();
  }
}

/** Turn off a station
 * This function turns off a scheduled station
 * and writes log record
 */
void turn_off_station(byte sid, ulong curr_time) {
  os.set_station_bit(sid, 0);

  byte qid = pd.station_qid[sid];
  // ignore if we are turning off a station that's not running or scheduled to run
  if (qid>=pd.nqueue)  return;

  //calculate gallons consumed during station run
   flow_station_gallons = (flow_gallons_count - flow_station_start_gallons);

  RuntimeQueueStruct *q = pd.queue+qid;

  // check if the current time is past the scheduled start time,
  // because we may be turning off a station that hasn't started yet
  if (curr_time > q->st) {
    // record lastrun log (only for non-master stations)
    if(os.status.mas!=(sid+1) && os.status.mas2!=(sid+1)) {
      pd.lastrun.station = sid;
      pd.lastrun.program = q->pid;
      pd.lastrun.duration = curr_time - q->st;
      pd.lastrun.endtime = curr_time;

      // log station run
      write_log(LOGDATA_STATION, curr_time);
      push_message(IFTTT_STATION_RUN, sid, pd.lastrun.duration);
    }
  }

  // dequeue the element
  pd.dequeue(qid);
  pd.station_qid[sid] = 0xFF;
}

/** Process dynamic events
 * such as rain delay, rain sensing
 * and turn off stations accordingly
 */
void process_dynamic_events(ulong curr_time) {
  // check if rain is detected
  bool rain = false;
  bool en = os.status.enabled ? true : false;
  if (os.status.rain_delayed || (os.status.rain_sensed && os.options[OPTION_SENSOR1_TYPE] == SENSOR_TYPE_RAIN)) {
    rain = true;
  }

  byte sid, s, bid, qid, rbits;
  for(bid=0;bid<os.nboards;bid++) {
    rbits = os.station_attrib_bits_read(ADDR_NVM_IGNRAIN+bid);
    for(s=0;s<8;s++) {
      sid=bid*8+s;

      // ignore master stations because they are handled separately      
      if (os.status.mas == sid+1) continue;
      if (os.status.mas2== sid+1) continue;      
      // If this is a normal program (not a run-once or test program)
      // and either the controller is disabled, or
      // if raining and ignore rain bit is cleared
      // FIX ME
      qid = pd.station_qid[sid];
      if(qid==255) continue;
      RuntimeQueueStruct *q = pd.queue + qid;

      if ((q->pid<99) && (!en || (rain && !(rbits&(1<<s)))) ) {
        turn_off_station(sid, curr_time);
      }
    }
  }
}

/** Scheduler
 * This function loops through the queue
 * and schedules the start time of each station
 */
void schedule_all_stations(ulong curr_time) {

  ulong con_start_time = curr_time + 1;   // concurrent start time
  ulong seq_start_time = con_start_time;  // sequential start time

  int16_t station_delay = water_time_decode_signed(os.options[OPTION_STATION_DELAY_TIME]);
  // if the sequential queue has stations running
  if (pd.last_seq_stop_time > curr_time) {
    seq_start_time = pd.last_seq_stop_time + station_delay;
  }

  RuntimeQueueStruct *q = pd.queue;
  byte re = os.options[OPTION_REMOTE_EXT_MODE];
  // go through runtime queue and calculate start time of each station
  for(;q<pd.queue+pd.nqueue;q++) {
    if(q->st) continue; // if this queue element has already been scheduled, skip
    if(!q->dur) continue; // if the element has been marked to reset, skip
    byte sid=q->sid;
    byte bid=sid>>3;
    byte s=sid&0x07;

    // if this is a sequential station and the controller is not in remote extension mode
    // use sequential scheduling. station delay time apples
    if (os.station_attrib_bits_read(ADDR_NVM_STNSEQ+bid)&(1<<s) && !re) {
      // sequential scheduling
      q->st = seq_start_time;
      seq_start_time += q->dur;
      seq_start_time += station_delay; // add station delay time
    } else {
      // otherwise, concurrent scheduling
      q->st = con_start_time;
      // stagger concurrent stations by 1 second
      con_start_time++;
    }
    /*DEBUG_PRINT("[");
    DEBUG_PRINT(sid);
    DEBUG_PRINT(":");
    DEBUG_PRINT(q->st);
    DEBUG_PRINT(",");
    DEBUG_PRINT(q->dur);
    DEBUG_PRINT("]");
    DEBUG_PRINTLN(pd.nqueue);*/
    if (!os.status.program_busy) {
      os.status.program_busy = 1;  // set program busy bit
    }
  }
}

/** Immediately reset all stations
 * No log records will be written
 */
void reset_all_stations_immediate() {
  os.clear_all_station_bits();
  os.apply_all_station_bits();
  pd.reset_runtime();
}

/** Reset all stations
 * This function sets the duration of
 * every station to 0, which causes
 * all stations to turn off in the next processing cycle.
 * Stations will be logged
 */
void reset_all_stations() {
  RuntimeQueueStruct *q = pd.queue;
  // go through runtime queue and assign water time to 0
  for(;q<pd.queue+pd.nqueue;q++) {
    q->dur = 0;
  }
}


/** Manually start a program
 * If pid==0, this is a test program (1 minute per station)
 * If pid==255, this is a short test program (2 second per station)
 * If pid > 0. run program pid-1
 */
void manual_start_program(byte pid, byte uwt) {
  boolean match_found = false;
  reset_all_stations_immediate();
  ProgramStruct prog;
  ulong dur;
  byte sid, bid, s;
  if ((pid>0)&&(pid<255)) {
    pd.read(pid-1, &prog);
    push_message(IFTTT_PROGRAM_SCHED, pid-1, uwt?os.options[OPTION_WATER_PERCENTAGE]:100, "");
  }
  for(sid=0;sid<os.nstations;sid++) {
    bid=sid>>3;
    s=sid&0x07;
    // skip if the station is a master station (because master cannot be scheduled independently
    if ((os.status.mas==sid+1) || (os.status.mas2==sid+1))
      continue;    
    dur = 60;
    if(pid==255)  dur=2;
    else if(pid>0)
      dur = water_time_resolve(prog.durations[sid]);
    if(uwt) {
      dur = dur * os.options[OPTION_WATER_PERCENTAGE] / 100;
    }
    if(dur>0 && !(os.station_attrib_bits_read(ADDR_NVM_STNDISABLE+bid)&(1<<s))) {
      RuntimeQueueStruct *q = pd.enqueue();
      if (q) {
        q->st = 0;
        q->dur = dur;
        q->sid = sid;
        q->pid = 254;
        match_found = true;
      }
    }
  }
  if(match_found) {
    schedule_all_stations(os.now_tz());
  }
}

// ==========================================
// ====== PUSH NOTIFICATION FUNCTIONS =======
// ==========================================
void ip2string(char* str, byte ip[4]) {
  for(byte i=0;i<4;i++) {
    itoa(ip[i], str+strlen(str), 10);
    if(i!=3) strcat(str, ".");
  }
}

void push_message(byte type, uint32_t lval, float fval, const char* sval) {

#if !defined(ARDUINO) || defined(__AVR_ATmega1284P__) || defined(__AVR_ATmega1284__) || defined(ESP8266)

  static const char* server = DEFAULT_IFTTT_URL;
  static char key[IFTTT_KEY_MAXSIZE];
  static char postval[TMP_BUFFER_SIZE];

  // check if this type of event is enabled for push notification
  if((os.options[OPTION_IFTTT_ENABLE]&type) == 0) return;
  key[0] = 0;
  read_from_file(ifkey_filename, key);
  key[IFTTT_KEY_MAXSIZE-1]=0;

  if(strlen(key)==0) return;

  #if defined(ARDUINO) && !defined(ESP8266)
    uint16_t _port = ether.hisport; // make a copy of the original port
    ether.hisport = 80;
  #endif

  strcpy_P(postval, PSTR("{\"value1\":\""));

  switch(type) {

    case IFTTT_STATION_RUN:
      
      strcat_P(postval, PSTR("Station "));
      os.get_station_name(lval, postval+strlen(postval));
      strcat_P(postval, PSTR(" closed. It ran for "));
      itoa((int)fval/60, postval+strlen(postval), 10);
      strcat_P(postval, PSTR(" minutes "));
      itoa((int)fval%60, postval+strlen(postval), 10);
      strcat_P(postval, PSTR(" seconds."));
      if(os.options[OPTION_SENSOR1_TYPE]==SENSOR_TYPE_FLOW) {
    	  strcat_P(postval, PSTR(" Flow rate: "));
#if defined(ARDUINO)
    	  dtostrf(flow_station_gallons,5,2,postval+strlen(postval));
#else
    	  sprintf(tmp_buffer+strlen(tmp_buffer), "%5.2f", flow_station_gallons);
#endif
      }
      break;

    case IFTTT_PROGRAM_SCHED:

      if(sval) strcat_P(postval, PSTR("Manually scheduled "));
      else strcat_P(postval, PSTR("Automatically scheduled "));
      strcat_P(postval, PSTR("Program "));
      {
        ProgramStruct prog;
        pd.read(lval, &prog);
        if(lval<pd.nprograms) strcat(postval, prog.name);
      }
      strcat_P(postval, PSTR(" with "));
      itoa((int)fval, postval+strlen(postval), 10);
      strcat_P(postval, PSTR("% water level."));
      break;

    case IFTTT_RAINSENSOR:

      strcat_P(postval, (lval==LOGDATA_RAINDELAY) ? PSTR("Rain delay ") : PSTR("Rain sensor "));
      strcat_P(postval, ((int)fval)?PSTR("activated."):PSTR("de-activated"));

      break;

    case IFTTT_FLOWSENSOR:
      strcat_P(postval, PSTR("Flow count: "));
      itoa(lval, postval+strlen(postval), 10);
      strcat_P(postval, PSTR(", volume: "));
      {
      uint32_t volume = os.options[OPTION_PULSE_RATE_1];
      volume = (volume<<8)+os.options[OPTION_PULSE_RATE_0];
      volume = lval*volume;
      itoa(volume/100, postval+strlen(postval), 10);
      strcat(postval, ".");
      itoa(volume%100, postval+strlen(postval), 10);
      }
      break;

    case IFTTT_WEATHER_UPDATE:
      if(lval>0) {
        strcat_P(postval, PSTR("External IP updated: "));
        byte ip[4] = {(byte)((lval>>24)&0xFF),
                      (byte)((lval>>16)&0xFF),
                      (byte)((lval>>8)&0xFF),
                      (byte)(lval&0xFF)};
        ip2string(postval, ip);
      }
      if(fval>=0) {
        strcat_P(postval, PSTR("Water level updated: "));
        itoa((int)fval, postval+strlen(postval), 10);
        strcat_P(postval, PSTR("%."));
      }
        
      break;

    case IFTTT_REBOOT:
      #if defined(ARDUINO)
        strcat_P(postval, PSTR("Rebooted. Device IP: "));
        #ifdef ESP8266
        {
          IPAddress _ip = WiFi.localIP();
          byte ip[4] = {_ip[0], _ip[1], _ip[2], _ip[3]};
          ip2string(postval, ip);
        }
        #else
        ip2string(postval, ether.myip);
        #endif
        //strcat(postval, ":");
        //itoa(_port, postval+strlen(postval), 10);
      #else
        strcat_P(postval, PSTR("Process restarted."));
      #endif
      break;
  }

  strcat_P(postval, PSTR("\"}"));

  //DEBUG_PRINTLN(postval);

#if defined(ARDUINO)

  #ifdef ESP8266
  WiFiClient client;
  if(!client.connect(server, 80)) return;
  
  char postBuffer[1500];
  sprintf(postBuffer, "POST /trigger/sprinkler/with/key/%s HTTP/1.0\r\n"
                      "Host: %s\r\n"
                      "Accept: */*\r\n"
                      "Content-Length: %d\r\n"
                      "Content-Type: application/json\r\n"
                      "\r\n%s", key, server, strlen(postval), postval);
  client.write((uint8_t *)postBuffer, strlen(postBuffer));

  time_t timeout = os.now_tz() + 5; // 5 seconds timeout
  while(!client.available() && os.now_tz() < timeout) {
  }

  bzero(ether_buffer, ETHER_BUFFER_SIZE);
  
  while(client.available()) {
    client.read((uint8_t*)ether_buffer, ETHER_BUFFER_SIZE);
  }
  client.stop();
  //DEBUG_PRINTLN(ether_buffer);
    
  #else
  if(!ether.dnsLookup(server, true)) {
    // if DNS lookup fails, use default IP
    ether.hisip[0] = 54;
    ether.hisip[1] = 172;
    ether.hisip[2] = 244;
    ether.hisip[3] = 116;
  }

  ether.httpPostVar(PSTR("/trigger/sprinkler/with/key/"), PSTR(DEFAULT_IFTTT_URL), key, postval, httpget_callback);
  for(int l=0;l<100;l++)  ether.packetLoop(ether.packetReceive());
  ether.hisport = _port;
  #endif
  
#else

  EthernetClient client;
  struct hostent *host;

  host = gethostbyname(server);
  if (!host) {
    DEBUG_PRINT("can't resolve http station - ");
    DEBUG_PRINTLN(server);
    return;
  }

  if (!client.connect((uint8_t*)host->h_addr, 80)) {
    client.stop();
    return;
  }

  char postBuffer[1500];
  sprintf(postBuffer, "POST /trigger/sprinkler/with/key/%s HTTP/1.0\r\n"
                      "Host: %s\r\n"
                      "Accept: */*\r\n"
                      "Content-Length: %d\r\n"
                      "Content-Type: application/json\r\n"
                      "\r\n%s", key, host->h_name, strlen(postval), postval);
  client.write((uint8_t *)postBuffer, strlen(postBuffer));

  bzero(ether_buffer, ETHER_BUFFER_SIZE);

  time_t timeout = now() + 5; // 5 seconds timeout
  while(now() < timeout) {
    int len=client.read((uint8_t *)ether_buffer, ETHER_BUFFER_SIZE);
    if (len<=0) {
      if(!client.connected())
        break;
      else
        continue;
    }
    httpget_callback(0, 0, ETHER_BUFFER_SIZE);
  }

  client.stop();

#endif
  
#endif
}

// ================================
// ====== LOGGING FUNCTIONS =======
// ================================
#if defined(ARDUINO)
char LOG_PREFIX[] = "/logs/";
#else
char LOG_PREFIX[] = "./logs/";
#endif

/** Generate log file name
 * Log files will be named /logs/xxxxx.txt
 */
void make_logfile_name(char *name) {
#if defined(ARDUINO)
  #ifndef ESP8266
  sd.chdir("/");
  #endif
#endif
  strcpy(tmp_buffer+TMP_BUFFER_SIZE-10, name);
  strcpy(tmp_buffer, LOG_PREFIX);
  strcat(tmp_buffer, tmp_buffer+TMP_BUFFER_SIZE-10);
  strcat_P(tmp_buffer, PSTR(".txt"));
}

/* To save RAM space, we store log type names
 * in program memory, and each name
 * must be strictly two characters with an ending 0
 * so each name is 3 characters total
 */
static const char log_type_names[] PROGMEM =
    "  \0"
    "rs\0"
    "rd\0"
    "wl\0"
    "fl\0";


void clear_old_logs(ulong curr_time){
#ifdef ESP8266

	//get the first old log to delete
	ulong OldLogTime = curr_time - (LOGS_HISTORY * 86400);
	//continue to search for logs to delete untill we canot find logs 7 days in a raw
	int continuesMissingLogCount  = 0;
	while(continuesMissingLogCount < 7 && OldLogTime > 0){
		ultoa(OldLogTime / 86400, tmp_buffer, 10);
		make_logfile_name(tmp_buffer);
		if(!SPIFFS.exists(tmp_buffer)){
			continuesMissingLogCount++;
		}
		else{
			continuesMissingLogCount = 0;
			SPIFFS.remove(tmp_buffer);
		}
		//next log to delete on prev day
		OldLogTime = OldLogTime - 86400;
	}
#endif

}

/** write run record to log on SD card */
void write_log(byte type, ulong curr_time) {

  //if (!os.options[OPTION_ENABLE_LOGGING]) return; - FORCE LOGGING

  // file name will be logs/xxxxx.tx where xxxxx is the day in epoch time
  ultoa(curr_time / 86400, tmp_buffer, 10);
  make_logfile_name(tmp_buffer);

  // Step 1: open file if exists, or create new otherwise, 
  // and move file pointer to the end  
#if defined(ARDUINO) // prepare log folder for Arduino
  if (!os.status.has_sd)  return;

  #ifdef ESP8266
  File file = SPIFFS.open(tmp_buffer, "r+");
  if(!file) {
    file = SPIFFS.open(tmp_buffer, "w");
    if(!file) return;
  }
  file.seek(0, SeekEnd);
  #else
  sd.chdir("/");
  if (sd.chdir(LOG_PREFIX) == false) {
    // create dir if it doesn't exist yet
    if (sd.mkdir(LOG_PREFIX) == false) {
      return;
    }
  }
  SdFile file;
  int ret = file.open(tmp_buffer, O_CREAT | O_WRITE );
  file.seekEnd();
  if(!ret) {
    return;
  }
  #endif
  
#else // prepare log folder for RPI/BBB
  struct stat st;
  if(stat(get_filename_fullpath(LOG_PREFIX), &st)) {
    if(mkdir(get_filename_fullpath(LOG_PREFIX), S_IRUSR | S_IWUSR | S_IXUSR | S_IRGRP | S_IWGRP | S_IXGRP | S_IROTH | S_IWOTH | S_IXOTH)) {
      return;
    }
  }
  FILE *file;
  file = fopen(get_filename_fullpath(tmp_buffer), "rb+");
  if(!file) {
    file = fopen(get_filename_fullpath(tmp_buffer), "wb");
    if (!file)  return;
  }
  fseek(file, 0, SEEK_END);
#endif  // prepare log folder
  
  // Step 2: prepare data buffer
  strcpy_P(tmp_buffer, PSTR("["));

  if(type == LOGDATA_STATION) {
    itoa(pd.lastrun.program, tmp_buffer+strlen(tmp_buffer), 10);
    strcat_P(tmp_buffer, PSTR(","));
    itoa(pd.lastrun.station, tmp_buffer+strlen(tmp_buffer), 10);
    strcat_P(tmp_buffer, PSTR(","));
    // duration is unsigned integer
    ultoa((ulong)pd.lastrun.duration, tmp_buffer+strlen(tmp_buffer), 10);
  } else {
    ulong lvalue=0;
    if(type==LOGDATA_FLOWSENSE) {
    	lvalue = (flow_gallons_count>os.flowcount_log_start)?(flow_gallons_count-os.flowcount_log_start):0;
    	float fgal = lvalue*1.0;
#if defined(ARDUINO)
    	dtostrf(fgal,5,2,tmp_buffer+strlen(tmp_buffer));
#else
    	sprintf(tmp_buffer+strlen(tmp_buffer), "%5.2f", fgal);
#endif
    } else {
    	lvalue = 0;
    	ultoa(lvalue, tmp_buffer+strlen(tmp_buffer), 10);
    }
    strcat_P(tmp_buffer, PSTR(",\""));
    strcat_P(tmp_buffer, log_type_names+type*3);
    strcat_P(tmp_buffer, PSTR("\","));

    switch(type) {
    case LOGDATA_RAINSENSE:
    case LOGDATA_FLOWSENSE:
    	lvalue = (curr_time>os.sensor_lasttime)?(curr_time-os.sensor_lasttime):0;
    	break;
    case LOGDATA_RAINDELAY:
    	lvalue = (curr_time>os.raindelay_start_time)?(curr_time-os.raindelay_start_time):0;
    	break;
    case LOGDATA_WATERLEVEL:
    	lvalue = os.options[OPTION_WATER_PERCENTAGE];
    	break;
    }
    ultoa(lvalue, tmp_buffer+strlen(tmp_buffer), 10);
  }
  strcat_P(tmp_buffer, PSTR(","));
  ultoa(curr_time, tmp_buffer+strlen(tmp_buffer), 10);
  if((os.options[OPTION_SENSOR1_TYPE]==SENSOR_TYPE_FLOW) && (type==LOGDATA_STATION)) {
	  // log gallons consumed
	  strcat_P(tmp_buffer, PSTR(","));
#if defined(ARDUINO)
	  dtostrf(flow_station_gallons,5,2,tmp_buffer+strlen(tmp_buffer));
#else
	  sprintf(tmp_buffer+strlen(tmp_buffer), "%5.2f", flow_station_gallons);
#endif
  }
  strcat_P(tmp_buffer, PSTR("]\r\n"));

#if defined(ARDUINO)
#ifdef ESP8266
  file.write((byte*)tmp_buffer, strlen(tmp_buffer));
#else
  file.write(tmp_buffer);
#endif
  file.close();
#else
  fwrite(tmp_buffer, 1, strlen(tmp_buffer), file);
  fclose(file);
#endif
}


/** Delete log file
 * If name is 'all', delete all logs
 */
void delete_log(char *name) {
//  if (!os.options[OPTION_ENABLE_LOGGING]) return;
#if defined(ARDUINO)
  if (!os.status.has_sd) return;

  #ifdef ESP8266
  if (strncmp(name, "all", 3) == 0) {
    // delete all log files
    Dir dir = SPIFFS.openDir(LOG_PREFIX);
    while (dir.next()) {
      SPIFFS.remove(dir.fileName());
    }
  } else {
    // delete a single log file
    make_logfile_name(name);
    if(!SPIFFS.exists(tmp_buffer)) return;
    SPIFFS.remove(tmp_buffer);
  }
  #else
  if (strncmp(name, "all", 3) == 0) {
    // delete the log folder
    SdFile file;

    if (sd.chdir(LOG_PREFIX)) {
      // delete the whole log folder
      sd.vwd()->rmRfStar();
    }
  } else {
    // delete a single log file
    make_logfile_name(name);
    if (!sd.exists(tmp_buffer))  return;
    sd.remove(tmp_buffer);
  }
  #endif
  
#else // delete_log implementation for RPI/BBB
  if (strncmp(name, "all", 3) == 0) {
    // delete the log folder
    rmdir(get_filename_fullpath(LOG_PREFIX));
    return;
  } else {
    make_logfile_name(name);
    remove(get_filename_fullpath(tmp_buffer));
  }
#endif
}

/** Perform network check
 * This function pings the router
 * to check if it's still online.
 * If not, it re-initializes Ethernet controller.
 */
void check_network() {
#if defined(ARDUINO) && !defined(ESP8266)
  // do not perform network checking if the controller has just started, or if a program is running
  if (os.status.program_busy) {return;}

  // check network condition periodically
  if (os.status.req_network) {
    os.status.req_network = 0;
    // change LCD icon to indicate it's checking network
    if (!ui_state) {
      os.lcd.setCursor(15, 1);
      os.lcd.write(4);
    }

    // ping gateway ip
    ether.clientIcmpRequest(ether.gwip);

    ulong start = millis();
    boolean failed = true;
    // wait at most PING_TIMEOUT milliseconds for ping result
    do {
      ether.packetLoop(ether.packetReceive());
      if (ether.packetLoopIcmpCheckReply(ether.gwip)) {
        failed = false;
        break;
      }
    } while(millis() - start < PING_TIMEOUT);
    if (failed)  {
      if(os.status.network_fails<3)  os.status.network_fails++;
      // clamp it to 6
      //if (os.status.network_fails > 6) os.status.network_fails = 6;
    }
    else os.status.network_fails=0;
    // if failed more than 3 times, restart
    if (os.status.network_fails==3) {
      // mark for safe restart
      os.status.safe_reboot = 1;
    } else if (os.status.network_fails>2) {
      // if failed more than twice, try to reconnect    
      if (os.start_network())
        os.status.network_fails=0;
    }
  }
#else
  // nothing to do for other platforms
#endif
}

/** Perform NTP sync */
void perform_ntp_sync() {
#if defined(ARDUINO)
  // do not perform sync if this option is disabled, or if network is not available, or if a program is running
  if (!os.options[OPTION_USE_NTP] || os.status.program_busy) return;
  #ifdef ESP8266
  if (os.get_wifi_mode()!=WIFI_MODE_STA || WiFi.status()!=WL_CONNECTED || os.state!=OS_STATE_CONNECTED) return;
  #else
  if (os.status.network_fails>0) return;
  #endif

  if (os.status.req_ntpsync) {
    // check if rtc is uninitialized
    // 978307200 is Jan 1, 2001, 00:00:00
    boolean rtc_zero = (now()<=978307200L);
    
    os.status.req_ntpsync = 0;
    if (!ui_state) {
      os.lcd_print_line_clear_pgm(PSTR("NTP Syncing..."),1);
    }
    ulong t = getNtpTime();
    if (t>0) {
      setTime(t);
      RTC.set(t);
      #ifndef ESP8266
      // if rtc was uninitialized and now it is, restart
      if(rtc_zero && now()>978307200L) {
        os.reboot_dev();
      }
      #endif
    }
  }
#else
  // nothing to do here
  // Linux will do this for you
#endif
}

#if !defined(ARDUINO) // main function for RPI/BBB
int main(int argc, char *argv[]) {
  do_setup();

  while(true) {
    do_loop();
  }
  return 0;
}
#endif
