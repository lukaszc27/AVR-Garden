/*
 * defs.h
 *
 *  Created on: 31 lip 2019
 *      Author: lukasz
 */

#ifndef DEFS_H_
#define DEFS_H_

//#define DEBUG

#define LED_DDR		DDRB
#define LED_PIN		(1 << PB4)
#define LED_ON		PORTB &= ~LED_PIN
#define LED_OFF		PORTB |= LED_PIN
#define LED_TOG		PORTB ^= LED_PIN

#define AREAS_SWITCH 		DDRC
#define AREAS_PORT			PORTC
#define AREA_SWITCH_1_PIN	(1 << PC6)
#define AREA_SWITCH_2_PIN	(1 << PC5)
#define AREA_SWITCH_3_PIN	(1 << PC4)
#define AREA_SWITCH_1_ON	AREAS_PORT |= AREA_SWITCH_1_PIN
#define AREA_SWITCH_2_ON	AREAS_PORT |= AREA_SWITCH_2_PIN
#define AREA_SWITCH_3_ON	AREAS_PORT |= AREA_SWITCH_3_PIN
#define AREA_SWITCH_1_OFF	AREAS_PORT &= ~AREA_SWITCH_1_PIN
#define AREA_SWITCH_2_OFF	AREAS_PORT &= ~AREA_SWITCH_2_PIN
#define AREA_SWITCH_3_OFF	AREAS_PORT &= ~AREA_SWITCH_3_PIN

#define BUTTON_PIN			(1 << PD2)
#define BUTTON_DDR			DDRD
#define BUTTON_PORT			PORTD

uint8_t TimeCompare(const DateTime_t a, const DateTime_t b);

#endif /* DEFS_H_ */
