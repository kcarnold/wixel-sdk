/*! \file sleep.h
* This file provides basic functions for putting the processor to sleep and
* switching oscillators. See the datasheet or design note DN106 for more
* information about the different power modes and their impact.
*/

#ifndef _WIXEL_SLEEP_H
#define _WIXEL_SLEEP_H

#include <cc2511_map.h>
#include <cc2511_types.h>

#define SLEEP_INTERRUPT_PORT0 0x01
#define SLEEP_INTERRUPT_PORT1 0x02
#define SLEEP_INTERRUPT_PORT2 0x04

ISR(ST, 1);

/*! Enable sleep timer interrupts and set the timer resolution */
void sleepInit(void);

/*! Helper function to switch oscillator to RC OSC from HS XOSC */
void switchToRCOSC(void);

/*! Enters sleep mode 1 for x seconds
* This will not disable any other interrupts */
void sleepMode1(uint16 seconds);

/*! Enters sleep mode 2 for x seconds
* This will disable all interrupts except the sleep timer
* and restore them after sleeping
*/
void sleepMode2(uint16 seconds, uint8 port_interrupts);

/*! Enters sleep mode 3 until an external interrupt occurs
* Note that the sleep timer cannot be used to wake up from PM3
*/
void sleepMode3();

#endif
