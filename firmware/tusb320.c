
#include "tusb320.h"
#include <stdio.h>

i2c_inst_t *i2c = NULL;
char i2c_addr = 0x67;

static char read_register(char addr)
{
  if (i2c == NULL)
  {
    printf("i2c device not set!\n");
    return 0;
  }

  //set address to read
  i2c_write_blocking(i2c, i2c_addr, &addr, 1, false); 

  //read the 1-byte register
  char data;
  int num_read = i2c_read_blocking(i2c, i2c_addr, &data, 1, false);

  if (num_read <= 0)
  {
    printf("i2c read error!\n");
    return 0;
  }

  return data;
}

static void write_register(char addr, char data)
{
  if (i2c == NULL)
  {
    printf("i2c device not set!\n");
    return;
  }

  uint8_t all_data[2] = {addr, data};
  i2c_write_blocking(i2c, i2c_addr, all_data, 2, false); 
}

void set_i2c_address(char i2c_address)
{
  i2c_addr = i2c_address;
}
void set_i2c(i2c_inst_t *i2c_dev)
{
  i2c = i2c_dev;
}

void read_state(tusb320_state *state)
{
  state->csr_08 = read_register(0x08);
  state->csr_09 = read_register(0x09);
  state->csr_0a = read_register(0x0a);
  state->csr_45 = read_register(0x45);
  state->csr_a0 = read_register(0xa0);
}

void update_state(tusb320_state *new_state, tusb320_state *current_state)
{
  #define UPDATE_REGISTER(REG_NAME, REG_VAL)             \
  do {                                                   \
    if (current_state->REG_NAME != new_state->REG_NAME) {\
      write_register(REG_VAL, new_state->REG_NAME);      \
      current_state->REG_NAME=new_state->REG_NAME;       \
    }                                                    \
  } while (0)
  UPDATE_REGISTER(csr_08, 0x08);
  UPDATE_REGISTER(csr_09, 0x09);
  UPDATE_REGISTER(csr_0a, 0x0a);
  UPDATE_REGISTER(csr_45, 0x45);
}

static int get_shift_from_mask(char mask)
{
  int shift_amt = 0;
  char not_mask = ~mask;
  for (int i = 0; i < 8; ++i)
  {
    if (not_mask & (1<<i) == 0)
    {
      return i;
    }
  }
}

static char get_masked(char data, char mask)
{
  return ((data & mask) >> get_shift_from_mask(mask));
}
static void set_masked(char *data, char val, char mask)
{
  *data &= ~mask; //clear bits from field
  *data |= (val & mask) << get_shift_from_mask(mask);
}

//register 0x08
char get_current_mode_advertise(tusb320_state *s)
{
  return get_masked(s->csr_08, 0b11000000);
}
void set_current_mode_advertise(tusb320_state *s, char val) //0x00 for default (500/900mA), 0x01 for mid (1.5A), 0x10 for high (3A)
{
  set_masked(&s->csr_08, val, 0b11000000);
}

char get_current_mode_detect(tusb320_state *s)
{
  return get_masked(s->csr_08, 0b00110000);
}
char get_accessory_connected(tusb320_state *s)
{
  return get_masked(s->csr_08, 0b00001110);
}
char get_active_cable_connection(tusb320_state *s)
{
  return get_masked(s->csr_08, 0b00000001);
}

//CSR register 0x09 functions
char get_attached_state(tusb320_state *s)
{
  return get_masked(s->csr_09, 0b11000000);
}
char get_cable_dir(tusb320_state *s)
{
  return get_masked(s->csr_09, 0b00100000);
}

char get_interrupt_status(tusb320_state *s)
{
  return get_masked(s->csr_09, 0b00010000);
}
void clear_interrupt_status(tusb320_state *s)
{
  set_masked(&s->csr_09, 1, 0b00010000);
}

char get_drp_duty_cycle(tusb320_state *s)
{
  return get_masked(s->csr_09, 0b00000110);
}
void set_drp_duty_cycle(tusb320_state *s, char val) //0x00 for 30% (default), 0x01 for 40%, 0x10 for 50%, 0x11 for 60%
{
  set_masked(&s->csr_09, val, 0b00000110);
}

char get_disable_ufp_accessory(tusb320_state *s)
{
  return get_masked(s->csr_09, 0b00000001);
}
void set_disable_ufp_accessory(tusb320_state *s, char val) //flag -- 0 for enabled, 1 for disabled
{
  set_masked(&s->csr_09, val, 0b00000001);
}

//CSR register 0x0A functions
char get_debounce(tusb320_state *s)
{
  return get_masked(s->csr_0a, 0b11000000);
}
void set_debounce(tusb320_state *s, char val)
{
  set_masked(&s->csr_0a, val, 0b11000000);
}

char get_mode_select(tusb320_state *s)
{
  return get_masked(s->csr_0a, 0b00110000);
}
void set_mode_select(tusb320_state *s, char val)
{
  set_masked(&s->csr_0a, val, 0b00110000);
}

char get_i2c_soft_reset(tusb320_state *s)
{
  return get_masked(s->csr_0a, 0b00001000);
}
void set_i2c_soft_reset(tusb320_state *s, char val)
{
  set_masked(&s->csr_0a, val, 0b00001000);
}

char get_source_pref(tusb320_state *s)
{
  return get_masked(s->csr_0a, 0b00000110);
}
void set_source_pref(tusb320_state *s, char val)
{
  set_masked(&s->csr_0a, val, 0b00000110);
}

char get_disable_term(tusb320_state *s)
{
  return get_masked(s->csr_0a, 0b00000001);
}
void set_disable_term(tusb320_state *s, char val)
{
  set_masked(&s->csr_0a, val, 0b00000001);
}

//CSR registers 0x45 and 0xA0 functions
char get_disable_rd_rp(tusb320_state *s)
{
  return get_masked(s->csr_45, 0b00000100);
}
void set_disable_rd_rp(tusb320_state *s, char val)
{
  set_masked(&s->csr_45, val, 0b00000100);
}

char get_revision(tusb320_state *s)
{
  return s->csr_a0;
}