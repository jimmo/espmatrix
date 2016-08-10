// Simple serial -> 8x8 matrix driver.
// Jim Mussared - Aug 2016.

// Based on https://github.com/arachnidlabs/minimatrix/
// By Nick Johnson - Arachnid Labs

#define F_CPU 8000000UL  // 8 MHz

#include <inttypes.h>
#include <avr/eeprom.h>
#include <avr/io.h>
#include <avr/interrupt.h>
#include <avr/pgmspace.h>
#include <avr/sleep.h>
#include <util/delay.h>
#include <stdio.h>

#include "hardware.h"

FUSES = {
    // Internal 8mhz oscillator; short startup; enable ISP programming.
    .low = FUSE_SUT0 & FUSE_CKSEL3 & FUSE_CKSEL1 & FUSE_CKSEL0,
    .high = FUSE_SPIEN,
};

volatile uint8_t display[8] = {0};

volatile uint8_t rx_value = 1;
volatile uint16_t rx_edge_time = 0;

const uint8_t MESSAGE_LEN = 2 + 8 + 1;
uint8_t buffer[MESSAGE_LEN] = {0};

const uint8_t HEADER[] = { 0x55, 0xaa };

const char row_pins[] PROGMEM = {_BV(PD6), _BV(PD5), _BV(PD4), _BV(PD3),
                                 _BV(PD1), _BV(PD0), _BV(PA0), _BV(PA1)};


uint8_t message_offset = 0;
uint8_t buffer_byte = 0;

enum class State {
  IDLE,
  START,
  DATA
};

ISR(INT0_vect) {
  rx_edge_time = TCNT1;
  TCNT1 = 0;
  rx_value = PIND & _BV(RX_PIN);
}

ISR(TIMER0_COMPA_vect) {
  static uint8_t row = 0;

  // Turn off the old row
  PORTD |= PORTD_ROWS;
  PORTA |= PORTA_ROWS;

  // Set the column data
  row = (row + 1) & 0x7;
  COLUMN_PORT = display[row];

  // Turn on the new row
  if (row < 6) {
    PORTD &= ~pgm_read_byte(&row_pins[row]);
  } else {
    PORTA &= ~pgm_read_byte(&row_pins[row]);
  }
}

// Search the ringbuffer for a valid message. 0x55 0xaa data*8 checksum.
void find_message() {
  uint8_t x = buffer_byte;
  uint8_t checksum = 0;

  for (uint8_t i = 0; i < sizeof(HEADER); ++i, ++x) {
    if (buffer[x % MESSAGE_LEN] != HEADER[i]) {
      return;
    }
    checksum ^= buffer[x % MESSAGE_LEN];
  }

  uint8_t m = x;
  for (uint8_t i = 0; i < 8; ++i) {
    checksum ^= buffer[x++ % MESSAGE_LEN];
  }

  if (checksum != buffer[x % MESSAGE_LEN]) {
    return;
  }

  for (uint8_t i = 0; i < 8; ++i) {
    display[i] = buffer[m++ % MESSAGE_LEN];
  }

  for (uint8_t i = 0; i < MESSAGE_LEN; ++i) {
    buffer[i] = 0;
  }
}

void serial_loop() {
  State state = State::IDLE;
  uint16_t bit_width = 0;
  uint8_t data = 0;
  uint8_t bits = 0;
  uint8_t pv = 0;

  for (;;) {
    uint8_t v = rx_value;
    uint16_t t = rx_edge_time;
    if (v == pv) {
      continue;
    }
    pv = v;

    // ----    --||||||||||  ------
    //     ----  ||||||||||--
    //    start     data    stop
    //
    // ---_-_-_---_-_--------
    //    SS12345678S

    if (state == State::IDLE) {
      if (!v) {
        state = State::START;
        bit_width = 0;
      }
      continue;
    }

    if (state == State::START) {
      if (v) {
        data = 0;
        bits = 0;
        bit_width = t;
        state = State::DATA;
      }
      continue;
    }

    if (state == State::DATA) {
      uint16_t bit_time = t;
      for (uint16_t x = 0; x < bit_time - bit_width/2; x += bit_width) {
        data <<= 1;
        data |= (v ? 0 : 1);
        ++bits;
        if (bits == 9) {
          buffer[buffer_byte] = data;
          buffer_byte = (buffer_byte+1) % MESSAGE_LEN;
	  find_message();
        }
        if (bits > 9) {
          break;
        }
      }
      if (bits > 9 && v) {
        state = State::IDLE;
      }
      continue;
    }
  }
}

void ioinit() {
  // Enable timer 0: display refresh
  OCR0A = 250;          // 8 megahertz / 64 / 250 = 500Hz
  // COM*=0 OC0A/OC0B disconnected, WGM=010 CTC(OCR0A), CS=011 /64 prescaler
  TCCR0A = _BV(WGM01);
  TCCR0B = _BV(CS01) | _BV(CS00);

  // Enable timer 1: bit width counter
  // COM*=0 OC1A/OC1B disconnected, WGM=000 MAX, CS=011 /64 prescaler
  // 8 MHz / 64 = 125kHz
  // Transmit known to work with 300 microseconds bit width.
  TCCR1A = 0;
  TCCR1B = _BV(CS01) | _BV(CS00);

  TIMSK = _BV(OCIE0A);  // Interrupt on timer 0 counter reset A (OCR0A)

  // Enable pin change interrupt.
  MCUCR &= ~_BV(ISC01);
  MCUCR |= _BV(ISC00);
  GIMSK = _BV(INT0);

  // PORTB is all output for columns
  COLUMN_DDR = 0xff;

  // Enable pullup on IR receiver pin
  PORTD |= _BV(RX_PIN);

  DDRD |= PORTD_ROWS;
  DDRA |= PORTA_ROWS;

  sei();
}

int main() {
  ioinit();

  serial_loop();
  return 0;
}
