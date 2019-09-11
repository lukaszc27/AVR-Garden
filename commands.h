/*
 * commands.h
 *
 *  Created on: 17 sie 2019
 *      Author: lukasz
 */

#ifndef COMMANDS_H_
#define COMMANDS_H_

#include <avr/io.h>
#include <avr/interrupt.h>
#include <avr/eeprom.h>
#include <avr/pgmspace.h>
#include <util/delay.h>
#include <stdio.h>
#include <string.h>

#include "libs/uart.h"
#include "vars.h"

#define MAX_COMMANDS	13


typedef struct {
	char* name;
	void (*action)(const char* params);
} Command_t;

void at_response(const char* params);
void at_setDateTime(const char* params);	// ustawienie daty i czasu
void at_status(const char* param);

void at_setGrassTime(const char* params);
void at_setTreesTime(const char* params);
void at_setFlowersTime(const char* params);
void at_setGrassDay(const char* params);
void at_setIrrigationHour(const char* params);
void at_setWetness(const char* params);
void at_setGrassTemp(const char* params);
void at_setFlowersTemp(const char* params);
void at_help(const char* params);
void at_on(const char* params);

void checkCommand();


#endif /* COMMANDS_H_ */
