
#include "pico/stdlib.h"

#define BATTERY_LOW (9) //V
#define BATTERY_WIGGLE (0.3) //V

void control_init();

void init_gpio();

void register_tusb320_interrupt();
void tusb320_interrupt_handler(uint gpio, uint32_t events);

bool check_and_enable_power();
void start_battery_monitoring();
bool battery_monitor_callback(struct repeating_timer *t);

void connect_battery(int which_battery, bool enable);
float get_battery_voltage(int which_battery);
void set_led(int which_led, bool enable);
void enable_usbc_regulator(bool enable);

