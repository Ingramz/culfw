#ifndef _BOARD_H
#define _BOARD_H

#define BOARD_ID_STR            "RFbee"
#define BOARD_ID_USTR           L"RFbee"

#define HAS_FHT_80b                     // PROGMEM: 1374b, RAM: 90b
#define HAS_RF_ROUTER                   // PROGMEM: 1248b  RAM: 44b
#define RFR_FILTER                      // PROGMEM:   90b  RAM:  4b
#define HAS_HOERMANN
#define HAS_HOERMANN_SEND               // PROGMEM:  220
#define HAS_CC1101_RX_PLL_LOCK_CHECK_TASK_WAIT	// PROGMEM: 118b
#define HAS_CC1101_PLL_LOCK_CHECK_MSG		// PROGMEM:  22b
#define HAS_CC1101_PLL_LOCK_CHECK_MSG_SW	// PROGMEM:  22b

#undef  RFR_DEBUG                       // PROGMEM:  354b  RAM: 14b
#undef  HAS_FASTRF                      // PROGMEM:  468b  RAM:  1b

#define HAS_FHT_8v                    // PROGMEM:  586b  RAM: 23b
#define HAS_FHT_TF
#define FHTBUF_SIZE          174      //                 RAM: 174b
#define RCV_BUCKETS            4      //                 RAM: 25b * bucket
#define FULL_CC1100_PA                // PROGMEM:  108b
#define HAS_RAWSEND                   //
#define HAS_ASKSIN                    // PROGMEM: 1314
#define HAS_ASKSIN_FUP                // PROGMEM:   78
#define HAS_MORITZ                    // PROGMEM: 1696
#define HAS_ESA                       // PROGMEM:  286
#define HAS_TX3                       // PROGMEM:  168
#define HAS_INTERTECHNO               // PROGMEM: 1352
#define HAS_TCM97001                  // PROGMEM:  264
#define HAS_UNIROLL                   // PROGMEM:   92
#define HAS_MEMFN                     // PROGMEM:  168
#define HAS_SOMFY_RTS                 // PROGMEM: 1716
#define HAS_BELFOX                    // PROGMEM:  214
#define HAS_ZWAVE                     // PROGMEM:  882
#define TTY_BUFSIZE          128      // RAM: TTY_BUFSIZE*4
#define HAS_MBUS                      // PROGMEM: 2536
#define MBUS_NO_TX                       // PROGMEM:  962
#define HAS_RFNATIVE                  // PROGMEM:  580
#define HAS_KOPP_FC                   // PROGMEM: 3370
#define HAS_ZWAVE                     // PROGMEM:  882
#define LACROSSE_HMS_EMU              // PROGMEM: 2206
#define HAS_EVOHOME

// No features to define below

/*
 * Board definition according to
 * http://www.seeedstudio.com/depot/datasheet/RFBee%20User%20Manual%20v1.1.pdf
 */

#define SPI_PORT		PORTB
#define SPI_DDR			DDRB
#define SPI_SS			PB2
#define SPI_MISO		PB4
#define SPI_MOSI		PB3
#define SPI_SCLK		PB5

#define CC1100_CS_DDR		SPI_DDR
#define CC1100_CS_PORT        SPI_PORT
#define CC1100_CS_PIN		SPI_SS
#define CC1100_OUT_DDR        DDRD
#define CC1100_OUT_PORT       PORTD
#define CC1100_OUT_PIN        PD2
#define CC1100_OUT_IN         PIND
#define CC1100_IN_DDR		DDRD
#define CC1100_IN_PORT        PIND
#define CC1100_IN_PIN         PD3
#define CC1100_IN_IN          PIND
#define CC1100_INT		INT1
#define CC1100_INTVECT        INT1_vect
#define CC1100_ISC		ISC10
#define CC1100_EICR           EICRA
#define LED_DDR               DDRD
#define LED_PORT              PORTD
#define LED_PIN               PD6

#define HAS_UART
#define UART_BAUD_RATE          38400

#endif
