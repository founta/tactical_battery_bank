
#include "pico/stdlib.h"

#define BATTERY_LOWEST (9) //V
#define BATTERY_WIGGLE (0.3) //V

#define BATTERY_LOW (BATTERY_LOWEST + BATTERY_WIGGLE) //V


void control_init();

void init_gpio();

void register_tusb320_interrupt();
void tusb320_interrupt_handler(uint gpio, uint32_t events);

bool check_and_enable_power();

void connect_battery(int which_battery, bool enable);
float get_battery_voltage(int which_battery);
void set_led(int which_led, bool enable);
void enable_usbc_regulator(bool enable);

void update_leds();
void set_led_blink();
bool led_blink_callback(struct repeating_timer *t);

void gather_voltages();