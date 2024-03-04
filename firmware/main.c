
#include <stdio.h>
#include "control.h"

int main() {
  stdio_init_all();
  control_init();

  while (1)
  {
    gather_voltages();
    sleep_ms(1000);
  }
}