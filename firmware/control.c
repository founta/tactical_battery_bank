
#include "gpio_defs.h"
#include "control.h"
#include "hardware/gpio.h"
#include "hardware/adc.h"
#include "tusb320.h"

#include <stdio.h>
#include <math.h>
#include <string.h>
tusb320_state current_tusb320_state, updated_tusb320_state;

struct repeating_timer led_blink_timer;
int blink_led = -1;
int blink_hold = 500; //ms
int blink_on_duration = 2;
int blink_on_count = 0;
bool blink_on = false;

int blink_off_duration = 1;
int blink_off_count = 0;
bool blinking = false;

bool battery_initial = true;
bool battery1_on = false;
bool battery2_on = false;

float battery1_voltage = 0.0f;
float battery2_voltage = 0.0f;

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

static void set_gpio_out(int pin, int pd, int pu)
{
  gpio_init(pin);
  gpio_set_dir(pin, GPIO_OUT);
  if (pd)
    gpio_pull_down(pin);
  else if (pu)
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
  set_gpio_out(TUSB320EN,     1,0);
  set_gpio_in (TUSB320INT,    0,1);
  set_gpio_in (TUSB320ID,     0,1);

  //TUSB320 i2c
  i2c_init(TUSB320I2C, 100000);
  gpio_set_function(TUSB320SDA, GPIO_FUNC_I2C);
  gpio_set_function(TUSB320SCL, GPIO_FUNC_I2C);
  gpio_pull_up(TUSB320SDA);
  gpio_pull_up(TUSB320SCL);

  //Battery 1 GPIO
  set_gpio_out(BAT1_EN,       1,0); //pull down
  set_gpio_in (BAT1_SENSE,    0,1); //pull up
  set_gpio_in (BAT1_DQ_READ,  0,1);
  set_gpio_out(BAT1_DQ_WRITE, 1,0);

  //Battery 1 ADC
  adc_gpio_init(BAT1_ADC);

  //Battery 2 GPIO
  set_gpio_out(BAT2_EN,       1,0);
  set_gpio_in (BAT2_SENSE,    0,1); //pull up
  set_gpio_in (BAT2_DQ_READ,  0,1);
  set_gpio_out(BAT2_DQ_WRITE, 1,0);

  //Battery 2 ADC
  adc_gpio_init(BAT2_ADC);

  //USB C regulator enable GPIO
  set_gpio_out(USBC_PWR_EN, 1,0); //pull down -- default disabled

  //LED gpio
  set_gpio_out(LED1_EN, 0,0);
  set_gpio_out(LED2_EN, 0,0);
}

float get_battery_voltage(int which_battery, bool busy_wait)
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
    if (busy_wait)
      busy_wait_ms(1);
    else
      sleep_ms(1);
  }
  adc_voltage *= ADC_CONVERSION_FACTOR / NUM_ADC_READS;

  return adc_voltage * battery_voltage_conversion_factor;
}


void register_tusb320_interrupt()
{
  gpio_set_irq_enabled_with_callback(TUSB320INT, GPIO_IRQ_EDGE_FALL, true, &tusb320_interrupt_handler);
  gpio_set_irq_enabled_with_callback(TUSB320ID, GPIO_IRQ_EDGE_RISE | GPIO_IRQ_EDGE_FALL, true, &tusb320_interrupt_handler);
}

static void turn_off_power()
{
  enable_usbc_regulator(false);

  connect_battery(1, false);
  connect_battery(2, false);

  update_leds();

  printf("Power disabled!\n");
}

void tusb320_interrupt_handler(uint gpio, uint32_t events)
{
  if (gpio == TUSB320INT)
  {
    //TODO just clear the "we have data to read" i2c register for now
    printf("Saw TUSB320 interrupt! handling ...\n");
    clear_interrupt_status(&updated_tusb320_state);
    update_state(&updated_tusb320_state, &current_tusb320_state);
    printf("TUSB320 interrupt handled!\n");
  }
  if (gpio == TUSB320ID)
  {
    printf("Saw TUSB320 ID change! handling ...\n");
    check_and_enable_power(true);
    printf("ID change handled!\n");
  }
}

bool check_and_enable_power(bool busy_wait)
{
  battery1_voltage = get_battery_voltage(1, busy_wait);
  battery2_voltage = get_battery_voltage(2, busy_wait);
  printf("battery 1 voltage is %.3f, battery 2 voltage is %.3f\n", battery1_voltage, battery2_voltage);

  if (gpio_get(TUSB320ID)) //then there is no USB cable connected
  {
    printf("No cable detected!\n");
    turn_off_power();
    return false;
  }

  if (battery1_voltage < BATTERY_LOWEST && battery2_voltage < BATTERY_LOWEST)
  {
    printf("Both battery voltages are too low! Turning off power...\n");
    turn_off_power();
    return false;
  }
  
  int battery_to_enable = -1;
  int battery_to_disable = -1;
  if (!battery1_on && !battery2_on)
  {
    printf("Both batteries were off ... \n");
    float thresh = battery_initial ? BATTERY_LOW : BATTERY_LOWEST;
    if (battery1_voltage > battery2_voltage && battery1_voltage > thresh)
    {
      printf("\tconnecting battery 1 ...\n");
      battery_to_enable = 1;
      battery_to_disable = 2;
    }
    else if (battery2_voltage > thresh)
    {
      printf("\tconnecting battery 2 ...\n");
      battery_to_enable = 2;
      battery_to_disable = 1;
    }
    else
    {
      printf("Leaving batteries off -- both are too low\n");
      turn_off_power();
      return false;
    }
  }
  else
  {
    //check if battery went under threshold and switch to the other
    if (battery1_on && battery1_voltage < BATTERY_LOWEST)
    {
      printf("Battery 1 voltage became too low!\n");
      if (battery2_voltage > BATTERY_LOW)
      {
        printf("\tswitching to battery 2...\n");
        battery_to_disable = 1;
        battery_to_enable = 2;
      }
      else //both batteries are too low now
      {
        printf("\tdisabling both batteries...\n");
        turn_off_power();
        return false;
      }
    }
    else if (battery2_on && battery2_voltage < BATTERY_LOWEST)
    {
      printf("Battery 2 voltage became too low!\n");
      if (battery1_voltage > BATTERY_LOW)
      {
        printf("\tswitching to battery 1...\n");
        battery_to_disable = 2;
        battery_to_enable = 1;
      }
      else //both batteries are too low now
      {
        printf("\tdisabling both batteries...\n");
        turn_off_power();
        return false;
      }
    }
  }

  if (battery_to_disable != -1 && battery_to_enable != -1)
  {
    battery_initial = false;

    connect_battery(battery_to_enable, true);
    if (busy_wait) //you cannot sleep in an interrupt handler
      busy_wait_ms(100);
    else
      sleep_ms(100);
    connect_battery(battery_to_disable, false);

    printf("Connected!\n");

    enable_usbc_regulator(true);
  }
  update_leds();
  return true;
}

void update_leds()
{
  if (!battery1_on && blinking && blink_led == 1)
  {
    cancel_repeating_timer(&led_blink_timer);
    blinking = false;
    blink_led = -1;
  }
  if (!battery2_on && blinking && blink_led == 2)
  {
    cancel_repeating_timer(&led_blink_timer);
    blinking = false;
    blink_led = -1;
  }
  if (battery1_on && !blinking)
  {
    set_led_blink(1);
    blinking = true;
  }
  if (battery2_on && !blinking)
  {
    set_led_blink(2);
    blinking = true;
  }

  if (blink_led != 1)
  {
    set_led(1, battery1_voltage > BATTERY_LOW);
  }
  if (blink_led != 2)
  {
    set_led(2, battery2_voltage > BATTERY_LOW);
  }
}

void set_led_blink(int which_led)
{
  blink_led = which_led;
  add_repeating_timer_ms(-blink_hold, led_blink_callback, NULL, &led_blink_timer);
}

bool led_blink_callback(struct repeating_timer *t)
{
  if (blink_on)
  {
    blink_on_count += 1;
    if (blink_on_count >= blink_on_duration)
    {
      blink_on = false;
      blink_on_count = 0;
    }
  }
  else
  {
    blink_off_count += 1;
    if (blink_off_count >= blink_off_duration)
    {
      blink_on = true;
      blink_off_count = 0;
    }
  }

  set_led(blink_led, blink_on);

  return true;
}

void connect_battery(int which_battery, bool enable)
{
  int relay_gpio;
  if (which_battery == 1)
  {
    battery1_on = enable;
    relay_gpio = BAT1_EN;
  }
  else //battery 2
  {
    battery2_on = enable;
    relay_gpio = BAT2_EN;
  }
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