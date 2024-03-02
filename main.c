
#include <stdio.h>
#include "control.h"

int main() {
  stdio_init_all();
  control_init();

  while (1)
  {
    //TODO remove this
    check_and_enable_power();
    sleep_ms(1000);
  }
}