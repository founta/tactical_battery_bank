
#include "hardware/i2c.h"

typedef struct {
  // address 0x08
  char csr_08;

  //address 0x09
  char csr_09;

  //address 0x0A
  char csr_0a;

  //addresses 0x45 and 0xA0
  char csr_45;
  char csr_a0;
} tusb320_state;

///////// functions to read/modify the TUSB320 state struct
//CSR register 0x08 functions
char get_current_mode_advertise(tusb320_state *s);
void set_current_mode_advertise(tusb320_state *s, char); //0x00 for default (500/900mA), 0x01 for mid (1.5A), 0x10 for high (3A)

char get_current_mode_detect(tusb320_state *s);
char get_accessory_connected(tusb320_state *s);
char get_active_cable_connection(tusb320_state *s);

//CSR register 0x09 functions
char get_attached_state(tusb320_state *s);
char get_cable_dir(tusb320_state *s);

char get_interrupt_status(tusb320_state *s);
void clear_interrupt_status(tusb320_state *s);

char get_drp_duty_cycle(tusb320_state *s);
void set_drp_duty_cycle(tusb320_state *s, char); //0x00 for 30% (default), 0x01 for 40%, 0x10 for 50%, 0x11 for 60%

char get_disable_ufp_accessory(tusb320_state *s);
void set_disable_ufp_accessory(tusb320_state *s, char); //flag -- 0 for enabled, 1 for disabled

//CSR register 0x0A functions
char get_debounce(tusb320_state *s);
void set_debounce(tusb320_state *s, char);

char get_mode_select(tusb320_state *s);
void set_mode_select(tusb320_state *s, char);

char get_i2c_soft_reset(tusb320_state *s);
void set_i2c_soft_reset(tusb320_state *s, char);

char get_source_pref(tusb320_state *s);
void set_source_pref(tusb320_state *s, char);

char get_disable_term(tusb320_state *s);
void set_disable_term(tusb320_state *s, char);

//CSR registers 0x45 and 0xA0 functions
char get_disable_rd_rp(tusb320_state *s);
void set_disable_rd_rp(tusb320_state *s, char);
char get_revision(tusb320_state *s);


///////// functions to apply the TUSB320 state struct to the device
void set_i2c_address(char i2c_address);
void set_i2c(i2c_inst_t *i2c);
void read_state(tusb320_state *state);
void update_state(tusb320_state *new_state, tusb320_state *current_state);