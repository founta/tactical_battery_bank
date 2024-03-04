
#include <stdio.h>
#include "control.h"

int main() {
  stdio_init_all();
  control_init();

  while (1)
  {
    check_and_enable_power();
    update_leds();
    sleep_ms(1000);
  }
}