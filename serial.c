#include "serial.h"
#include <avr/pgmspace.h>
#include <stdlib.h>
#include <util/delay_basic.h>

#if F_CPU == 16000000

static const PROGMEM uint16_t table[][5] = 
{
  //  baud    rxcenter   rxintra    rxstop    tx
  { 300,      3804,      7617,      7617,     7614,  },
  { 1200,     947,       1902,      1902,     1899,  },
  { 2400,     471,       950,       950,      947,   },
  { 4800,     233,       474,       474,      471,   },
  { 9600,     114,       236,       236,      233,   },
  { 14400,    74,        156,       156,      153,   },
  { 19200,    54,        117,       117,      114,   },
  { 28800,    34,        77,        77,       74,    },
  { 31250,    31,        70,        70,       68,    },
  { 38400,    25,        57,        57,       54,    },
  { 57600,    10,        37,        37,       33,    },
  { 115200,   1,         17,        17,       12,    },
};

const int XMIT_START_ADJUSTMENT = 5;

#elif F_CPU == 8000000

static const PROGMEM uint16_t table[][5] = 
{
  //  baud    rxcenter    rxintra    rxstop  tx
  { 300,      1895,       3805,      3805,   3802,   },
  { 1200,     467,        948,       948,    945,    },
  { 2400,     229,        472,       472,    469,    },
  { 4800,     110,        233,       233,    230,    },
  { 9600,     50,         114,       114,    112,    },
  { 14400,    30,         75,        75,     72,     },
  { 19200,    20,         55,        55,     52,     },
  { 28800,    11,         35,        35,     32,     },
  { 31250,    7,          32,        33,     29,     },
  { 38400,    2,          25,        26,     23,     },
  { 57600,    1,          15,        15,     13,     },
  { 115200,   1,          5,         5,      3,      },
};

const int XMIT_START_ADJUSTMENT = 4;
#endif

serial_t* serial_create() 
{
  serial_t* sp = malloc(sizeof *sp);

  return sp;
}

void serial_destroy(serial_t *sp)
{
  free(sp);
}

int8_t serial_config(serial_t *sp, baud_t baudrate, 
    volatile uint8_t *rx_port, uint8_t tx_pin, volatile uint8_t *tx_port, uint8_t rx_pin)
{
  uint8_t result = 1;

  sp->debug = 0;
  sp->rx_index = 0;

  sp->rx_port = rx_port;
  sp->rx_pin = rx_pin;
  sp->tx_port = tx_port;
  sp->tx_pin = tx_pin;

  sp->baudrate = pgm_read_word(&table[baudrate][0]);
  sp->delay_rxcenter = pgm_read_word(&table[baudrate][1]);
  sp->delay_rxintra = pgm_read_word(&table[baudrate][2]);
  sp->delay_rxstop = pgm_read_word(&table[baudrate][3]);
  sp->delay_tx = pgm_read_word(&table[baudrate][4]);

  return result;
}

int8_t serial_debug(serial_t *sp, volatile uint8_t *debug_port, uint8_t debug_pin)
{
  sp->debug = 1;
  sp->debug_port = debug_port;
  sp->debug_pin = debug_pin;

  return 1;
}

void debug_write(serial_t *sp, uint8_t state)
{
  if (sp->debug) {
    if (state) {
      *(sp->debug_port) |= (1 << sp->debug_pin);
    }
    else {
      *(sp->debug_port) &= ~(1 << sp->debug_pin);
    }
  }
}

uint8_t rx_read(serial_t *sp)
{
  return *(sp->rx_port) & (1 << sp->rx_pin);
}

void tx_write(serial_t *sp, uint8_t state)
{
  if (state > 0) {
    *(sp->tx_port) |= (1 << sp->tx_pin);
  }
  else {
    *(sp->tx_port) &= ~(1 << sp->tx_pin);
  }
}

inline void tuned_delay(uint16_t delay) { 
  uint8_t tmp=0;

  asm volatile("sbiw    %0, 0x01 \n\t"
    "ldi %1, 0xFF \n\t"
    "cpi %A0, 0xFF \n\t"
    "cpc %B0, %1 \n\t"
    "brne .-10 \n\t"
    : "+r" (delay), "+a" (tmp)
    : "0" (delay)
    );
}


int8_t serial_recv(serial_t *sp)
{
  uint8_t result = 0;

  if (!rx_read(sp)) {
    uint8_t d = 0;

    tuned_delay(sp->delay_rxcenter);

    debug_write(sp, 1);

    /* Read each of the 8 bits */
    for (uint8_t mask=0x1; mask; mask <<= 1) {
      tuned_delay(sp->delay_rxintra);
      if (rx_read(sp)) 
        d |= mask;
      else
        d &= ~mask;
    }

    debug_write(sp, 0);
    tuned_delay(sp->delay_rxstop);

    /* Save byte in buffer */
    if (sp->rx_index < MAX_RX_BUFF_SIZE) {
      sp->rx_buff[sp->rx_index] = d;
      sp->rx_index++;
    }
    else {
      debug_write(sp, 1);
    }

  }
    
  return result;
}

int8_t serial_handle(serial_t *sp)
{
  return serial_recv(sp);
}

void serial_write(serial_t *sp, uint8_t byte)
{
  uint8_t save_SREG = SREG;
  SREG &= 0x78; 

  tx_write(sp, 0);
  tuned_delay(sp->delay_tx + XMIT_START_ADJUSTMENT);

  debug_write(sp, 1);
  /* Send each bits */
  for (uint8_t mask = 0x01; mask; mask <<= 1) {
    tx_write(sp, mask & byte);
    tuned_delay(sp->delay_tx);
  }
  debug_write(sp, 0);

  tx_write(sp, 1);
  tuned_delay(sp->delay_tx);

  SREG = save_SREG;
}

int8_t serial_write_bytes(serial_t *sp, uint8_t* buff, uint8_t size)
{
  for (uint8_t i=0; i < size; ++i) {
    serial_write(sp, buff[i]);
  }
  return 1;
}

