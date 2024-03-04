
#include <stdio.h>
#include "control.h"

int main() {
  stdio_init_all();
  control_init();

  while (1)
  {
    check_and_enable_power(false); //poll to see if battery has depleted and switch to the other
    sleep_ms(1000);
  }
}