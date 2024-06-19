#pragma once
/*
 * Created by Jason Hill
 * 2023/06/18
 * 
 * Updated by Peter Edwards
 * 2024/01/02
 * 
 */

/****************************************************
 * The following defines assume default pinout
 * of Arduino Pro Mini or similiar:
 * 
 * PS2_CLOCK = 2; //PD2 - Port D, bit 2
 * PS2_DATA  = 17;//PC3 - Port C, bit 3
 * 
 * RS232_RTS = 3; //PD3 - Port D, bit 3
 * RS232_TX  = 4; //PD4 - Port D, bit 4
 * 
 * JP12      = 11; //PB3 - Port B, bit 3
 * JP24      = 12; //PB4 - Port B, bit 4
 * LED       = 13; //PB5 - Port B, bit 5
 * 
 * The following defines Pin and Port addressing to
 * more quickly set individual pins to their desired
 * states.
*/

/****************************************************/
/* PS/2 data pin operations                         */
#define PS2_DATA_BIT            (_BV(3))
#define PS2_SETDATAHIGH()       ( PORTC |= PS2_DATA_BIT )
#define PS2_SETDATALOW()        ( PORTC &= ~PS2_DATA_BIT )
#define PS2_SETDATA(val)        (val) ? PS2_SETDATAHIGH() : PS2_SETDATALOW()
#define PS2_DIRDATAIN()         ( DDRC  &= ~PS2_DATA_BIT )
#define PS2_DIRDATAIN_UP()      ( DDRC  &= ~PS2_DATA_BIT )
#define PS2_DIRDATAOUT()        ( DDRC  |= PS2_DATA_BIT )
#define PS2_READDATA()          ( (PINC &= PS2_DATA_BIT)>>3 )

#define PS2_CLOCK_BIT           (_BV(2))
#define PS2_SETCLOCKHIGH()      ( PORTD |= PS2_CLOCK_BIT )
#define PS2_SETCLOCKLOW()       ( PORTD &= ~PS2_CLOCK_BIT)
#define PS2_SETCLOCK(val)       (val) ? PS2_SETCLOCKHIGH() : PS2_SETCLOCKLOW()
#define PS2_DIRCLOCKIN()        ( DDRD  &= ~PS2_CLOCK_BIT )
#define PS2_DIRCLOCKIN_UP()     ( DDRD  &= ~PS2_CLOCK_BIT ); PS2_SETCLOCKHIGH()
#define PS2_DIRCLOCKOUT()       ( DDRD  |= PS2_CLOCK_BIT )
#define PS2_READCLOCK()         ( (PIND &= PS2_CLOCK_BIT)>>2 )

/****************************************************/
/* RS-232 pin operations                            */  
#define RS_SETTXHIGH()          (PORTD |=0b00010000)
#define RS_SETTXLOW()           (PORTD &=0b11101111)
#define RS_SETTX(val)           (val) ? RS_SETTXHIGH() : RS_SETTXLOW()
#define RS_DIRTXOUT()           (DDRD  |=0b00010000)

/****************************************************/
/* JUMPER pin operations                            */
#define JP12_SETHIGH()          (PORTB |=0b00001000)
#define JP12_DIRIN()            (DDRB  &=0b11110111)
#define JP12_DIRIN_UP()         (DDRB  &=0b11110111); JP12_SETHIGH()
#define JP12_READ()             ((PINB &=0b00001000)>>3)

#define JP34_SETHIGH()          (PORTB |=0b00010000)
#define JP34_DIRIN()            (DDRB  &=0b11101111)
#define JP34_DIRIN_UP()         (DDRB  &=0b11101111); JP34_SETHIGH()
#define JP34_READ()             ((PINB &=0b00010000)>>4)

/****************************************************/
/* LED pin operations                               */
#define LED_SETHIGH()           (PORTB |=0b00100000)
#define LED_SETLOW()            (PORTB &=0b11011111)
#define LED_SET(val)            (val) ? LED_SETHIGH() : LED_SETLOW()
#define LED_DIROUT()            (DDRB  |=0b00100000)
