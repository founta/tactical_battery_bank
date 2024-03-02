
#include "gpio_defs.h"
#include "control.h"
#include "hardware/gpio.h"
#include "hardware/adc.h"
#include "tusb320.h"

#include <stdio.h>
#include <math.h>
#include <string.h>
tusb320_state current_tusb320_state, updated_tusb320_state;

struct repeating_timer battery_update_timer;

bool battery1_on = false;
bool battery2_on = false;

void control_init()
{
  init_gpio();

  //turn on the TUSB320
  gpio_put(TUSB320EN, true);
  sleep_ms(50);

  //configure up the TUSB320 for 3A current advertisement if needed
  set_i2c(TUSB320I2C);
  set_i2c_address(TUSB320_I2C_ADDR);

  read_state(&current_tusb320_state);
  memcpy(&updated_tusb320_state, &current_tusb320_state, sizeof(current_tusb320_state));

  set_current_mode_advertise(&updated_tusb320_state, 0x10); //advertise 3A
  update_state(&updated_tusb320_state, &current_tusb320_state);

  //and register TUSB320 interrupts
  register_tusb320_interrupt();

  printf("battery control initialized!\n");
}

static void set_gpio_out(int pin, int pd)
{
  gpio_init(pin);
  gpio_set_dir(pin, GPIO_OUT);
  if (pd)
    gpio_pull_down(pin);
  else
    gpio_pull_up(pin);
}

static void set_gpio_in(int pin, int pd, int pu)
{
  gpio_init(pin);
  gpio_set_dir(pin, GPIO_IN);
  if (pd)
    gpio_pull_down(pin);
  else if (pu)
    gpio_pull_up(pin);
}

void init_gpio()
{
  adc_init();

  //TUSB320 GPIO pins
  set_gpio_out(TUSB320EN,     1);
  set_gpio_in (TUSB320INT,    0,1);
  set_gpio_in (TUSB320ID,     0,1);

  //TUSB320 i2c
  i2c_init(TUSB320I2C, 100000);
  gpio_set_function(TUSB320SDA, GPIO_FUNC_I2C);
  gpio_set_function(TUSB320SCL, GPIO_FUNC_I2C);
  gpio_pull_up(TUSB320SDA);
  gpio_pull_up(TUSB320SCL);

  //Battery 1 GPIO
  set_gpio_out(BAT1_EN,       1); //pull down
  set_gpio_in (BAT1_SENSE,    0,1); //pull up
  set_gpio_in (BAT1_DQ_READ,  0,1);
  set_gpio_out(BAT1_DQ_WRITE, 1);

  //Battery 1 ADC
  adc_gpio_init(BAT1_ADC);

  //Battery 2 GPIO
  set_gpio_out(BAT2_EN,       1);
  set_gpio_in (BAT2_SENSE,    0,1); //pull up
  set_gpio_in (BAT2_DQ_READ,  0,1);
  set_gpio_out(BAT2_DQ_WRITE, 1);

  //Battery 2 ADC
  adc_gpio_init(BAT2_ADC);

  //USB C regulator enable GPIO
  set_gpio_out(USBC_PWR_EN, 1); //pull down -- default disabled
}

float get_battery_voltage(int which_battery)
{
  #define ADC_CONVERSION_FACTOR (3.3f / (1 << 12))
  #define ADC_SEL (26)

  #define NUM_ADC_READS (16)

  float battery_voltage_conversion_factor;
  if (which_battery == 1) //battery 1
  {
    adc_select_input(BAT1_ADC - ADC_SEL);
    battery_voltage_conversion_factor = 5.3f; //12.9f; // I don't understand ;.;
  }
  else //battery 2
  {
    adc_select_input(BAT2_ADC - ADC_SEL);
    battery_voltage_conversion_factor = 5.315f; //13.0f;
  }

  float adc_voltage = 0.0f;
  for (int i = 0; i < NUM_ADC_READS; ++i)
  {
    uint16_t reading = adc_read();
    adc_voltage += reading;
    sleep_ms(1);
  }
  adc_voltage *= ADC_CONVERSION_FACTOR / NUM_ADC_READS;

  return adc_voltage * battery_voltage_conversion_factor;
}


void register_tusb320_interrupt()
{
  gpio_set_irq_enabled_with_callback(TUSB320INT, GPIO_IRQ_EDGE_FALL, true, &tusb320_interrupt_handler);
  //gpio_set_irq_enabled_with_callback(TUSB320ID, GPIO_IRQ_EDGE_RISE | GPIO_IRQ_EDGE_FALL, true, &tusb320_interrupt_handler);
}

static void turn_off_power()
{
  enable_usbc_regulator(false);

  cancel_repeating_timer(&battery_update_timer);
  connect_battery(1, false);
  connect_battery(2, false);
}

void tusb320_interrupt_handler(uint gpio, uint32_t events)
{
  if (gpio == TUSB320INT)
  {
    //TODO just clear the "we have data to read" i2c register for now
    printf("Saw TUSB320 interrupt! handling ...\n");
    fflush(stdout);
    clear_interrupt_status(&updated_tusb320_state);
    update_state(&updated_tusb320_state, &current_tusb320_state);
    printf("TUSB320 interrupt handled!\n");
  }
  // if (gpio == TUSB320ID)
  // {
  //   if (events & GPIO_IRQ_EDGE_RISE) // usb-c disconnected, turn off regulator and relays and such
  //   {
  //     printf("USB-C was disconnected! Disabling power...\n");
  //     turn_off_power();
  //   }
  //   else if (events & GPIO_IRQ_EDGE_FALL) // usb-c connected
  //   {
  //     printf("USB-C connected!\n");
  //     fflush(stdout);
  //     check_and_enable_power();
  //     printf("Starting to monitor battery voltages...\n");
  //     fflush(stdout);
  //     start_battery_monitoring();
  //   }
  // }
}

bool check_and_enable_power()
{
  float battery1_voltage = get_battery_voltage(1);
  float battery2_voltage = get_battery_voltage(2);

  printf("battery 1 voltage is %.3f, battery 2 voltage is %.3f\n", battery1_voltage, battery2_voltage);

  if (gpio_get(TUSB320ID)) //then there is no USB cable connected
  {
    printf("No cable detected!\n");
    turn_off_power();
    return false;
  }

  if (battery1_voltage < BATTERY_LOW && battery2_voltage < BATTERY_LOW)
  {
    printf("Both battery voltages are too low! Turning off power...\n");
    turn_off_power();
    return false;
  }

  if (fabs(battery1_voltage - battery2_voltage) < BATTERY_WIGGLE)
  {
    if (battery1_on || battery2_on)
    {
      printf("Battery voltages are very close! Doing nothing...\n");
      return true;
    }
  }

  int battery_to_enable;
  int battery_to_disable;
  if (battery1_voltage > battery2_voltage)
  {
    printf("Connecting battery 1...\n");
    battery_to_enable = 1;
    battery_to_disable = 2;
  }
  else
  {
    printf("Connecting battery 2...\n");
    battery_to_enable = 2;
    battery_to_disable = 1;
  }

  connect_battery(battery_to_enable, true);
  sleep_ms(100);
  connect_battery(battery_to_disable, false);

  battery1_on = (battery_to_enable == 1);
  battery2_on = (battery_to_enable == 2);

  printf("Connected!\n");

  enable_usbc_regulator(true);
  return true;
}

void start_battery_monitoring()
{
  add_repeating_timer_ms(1000, battery_monitor_callback, NULL, &battery_update_timer);
}

bool battery_monitor_callback(struct repeating_timer *t)
{
  check_and_enable_power();
  return true;
}

void connect_battery(int which_battery, bool enable)
{
  int relay_gpio = (which_battery == 1) ? BAT1_EN : BAT2_EN;
  gpio_put(relay_gpio, enable);

  set_led(which_battery, enable);
}

void set_led(int which_led, bool enable)
{
  int led_gpio = (which_led == 1) ? LED1_EN : LED2_EN;
  gpio_put(led_gpio, enable);
}

void enable_usbc_regulator(bool enable)
{
  printf("Setting USB-C power regulator %s!\n", enable ? "on" : "off");
  gpio_put(USBC_PWR_EN, enable);
}