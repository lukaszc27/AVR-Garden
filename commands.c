/*
 * commands.c
 *
 *  Created on: 17 sie 2019
 *      Author: lukasz
 */
#include "commands.h"
#include "defs.h"
#include "libs/ds1307/ds1307.h"
#include <math.h>
#include <stdlib.h>

extern Settings_t settings;				// ustawienia w pamięci RAM
extern Settings_t ee_settings EEMEM;		// ustawienia zapisane w pamięci EEPROM
extern volatile uint8_t led_flag;

/**
 * podstawowe polecenie sprawdzające komunikację
 */
void at_response(const char* params) {
	uart_puts_p(PSTR("\r\nOK\r\n\n"));
}

void at_setGrassTime(const char* params) {
	settings.grassTimeIrrigation = atoi(params);
	eeprom_write_block(&settings, &ee_settings, sizeof(settings));
}

void at_setTreesTime(const char* params) {
	settings.treesTimeIrrigation = atoi(params);
	eeprom_write_block(&settings, &ee_settings, sizeof(settings));
}

void at_setFlowersTime(const char* params) {
	settings.flowersTimeIrrigation = atoi(params);
	eeprom_write_block(&settings, &ee_settings, sizeof(settings));
}

void at_setGrassDay(const char* params) {
	settings.grassDayIrrigation = atoi(params);
	eeprom_write_block(&settings, &ee_settings, sizeof(settings));
}

void at_setIrrigationHour(const char* params) {
	settings.minHourIrrigation = atoi(params);
	eeprom_write_block(&settings, &ee_settings, sizeof(settings));
}

void at_setWetness(const char* params) {
	settings.minWetness = atoi(params);
	eeprom_write_block(&settings, &ee_settings, sizeof(settings));
}

void at_setGrassTemp(const char* params) {
	settings.grassMinTemperature = atoi(params);
	eeprom_write_block(&settings, &ee_settings, sizeof(settings));
}

void at_setFlowersTemp(const char* params) {
	settings.flowerMinTemperature = atoi(params);
	eeprom_write_block(&settings, &ee_settings, sizeof(settings));
}

void at_status(const char* param)
{
	eeprom_read_block(&settings, &ee_settings, sizeof(settings));
	_delay_ms(80);

	uart_puts_p(PSTR("\r\n+++ AutoGarden Status +++\r\n\n"));
	_delay_ms(20);

	uart_puts_p(PSTR("Czas nawadniania trawnika [min]: "));
	uart_putint(settings.grassTimeIrrigation);
	uart_puts_p(PSTR("\r\nCzas nawadniania drzew [min]: "));
	uart_putint(settings.treesTimeIrrigation);
	uart_puts_p(PSTR("\r\nCzas nawadniania krzewów [min]: "));
	uart_putint(settings.flowersTimeIrrigation);
	uart_puts_p(PSTR("\r\nNawadnianie trawnika co "));
	uart_putint(settings.grassDayIrrigation);
	uart_puts_p(PSTR(" dzień\r\n"));
	uart_puts_p(PSTR("Min. temp. nawadniania trawnika: "));
	uart_putint(settings.grassMinTemperature);
	uart_puts_p(PSTR("\r\nMin. temp. nawdaniania krzewow: "));
	uart_putint(settings.flowerMinTemperature);
	uart_puts_p(PSTR("\r\nNawadnianie po godzinie: "));
	uart_putint(settings.minHourIrrigation);
	uart_puts_p(PSTR("\r\nMin. wilgotnosc do rozpoczecia nawadniania: "));
	uart_putint(settings.minWetness);
	uart_puts_p(PSTR("\r\n\n"));
}

void at_help(const char* params) {
	uart_puts_p(PSTR("\r\n\n *** AutoGarden v1.0 by lukaszc27 ***\r\n"));
	uart_puts_p(PSTR(" + MCU: ATMega16 [16MHz]\r\n"));
	uart_puts_p(PSTR(" + Programing date: 23.07-20.08.2019r\r\n"));
	uart_puts_p(PSTR(" + Author: Lukasz Ciesla & Aleksandra Dziuba\r\n\n"));
	uart_puts_p(PSTR("COMMANDS: \r\n"));
	uart_puts_p(PSTR("AT+DATETIME - ustawienie daty oraz czasu (AT+DATETIME=YY/MM/DD;hh:mm:ss)\r\n"));
	uart_puts_p(PSTR("AT+GRASS_TIME - ustawienie czasu nawadniania trawnika w min.\r\n"));
	uart_puts_p(PSTR("AT+TREES_TIME - ustawienie czasu nawadniania drzew w min.\r\n"));
	uart_puts_p(PSTR("AT+FLOWERS_TIME - ustawienie czasu nawadniana krzewow w min.\r\n"));
	uart_puts_p(PSTR("AT+GRASS_DAY - co ktory dzien ma byc nawadniany trawnik [0-31]\r\n"));
	uart_puts_p(PSTR("AT+WETNESS - minimalna wilgotnosc gleby do nawadniania\r\n"));
	uart_puts_p(PSTR("AT+IRRIGATION_HOUR - godzina o ktorej nawadnianie jest rozpoczynane [1-23]\r\n"));
	uart_puts_p(PSTR("AT+GRASS_TEMP - minimalna temperatura powietrza do nawadniania trawnika\r\n"));
	uart_puts_p(PSTR("AT+FLOWERS_TEMP - minimalna temperatura do nawadniania krzewow\r\n"));
	uart_puts_p(PSTR("AT+ON - podlewanie okreslonego sektora (np. AT+ON=[GRASS/FLOWERS/TREES];czas [min]\r\n"));
}

void at_on(const char* params)
{
	char area[16];
	char number[8];

	uint8_t index = 0;
	while (params[index] != ';') {
		area[index] = params[index];
		index++;
	}

	index++;
	uint8_t pointer = 0;
	while(params[index] != '\r') {
		if (params[index] >= '0' && params[index] <= '9')
			number[pointer++] = params[index++];
		else break;
	}

	uint8_t time = atoi(number);
	DateTime_t t;

	ds1307_getdate(&t.year, &t.month, &t.day,
			&t.hour, &t.min, &t.sec);

	t.min += time;

	if ( strstr(area, "GRASS") )
	{
		AREA_SWITCH_1_ON;

		DateTime_t newTime;
		do {
			ds1307_getdate(&newTime.year, &newTime.month, &newTime.day,
					&newTime.hour, &newTime.min, &newTime.sec);

			if (led_flag) {
				LED_TOG;
				led_flag = 0;
			}
		} while (TimeCompare(newTime, t) == 2);

		AREA_SWITCH_1_OFF;
	}
	else if ( strstr(area, "TREES") )
	{
		AREA_SWITCH_2_ON;

		DateTime_t newTime;
		do {
			ds1307_getdate(&newTime.year, &newTime.month, &newTime.day,
					&newTime.hour, &newTime.min, &newTime.sec);

			if (led_flag) {
				LED_TOG;
				led_flag = 0;
			}
		} while (TimeCompare(newTime, t) == 2);

		AREA_SWITCH_2_OFF;
	}
	else if ( strstr(area, "FLOWERS") )
	{
		AREA_SWITCH_3_ON;

		DateTime_t newTime;
		do {
			ds1307_getdate(&newTime.year, &newTime.month, &newTime.day,
					&newTime.hour, &newTime.min, &newTime.sec);

			if (led_flag) {
				LED_TOG;
				led_flag = 0;
			}
		} while (TimeCompare(newTime, t) == 2);

		AREA_SWITCH_3_OFF;
	}
}

/**
 * ustawianie daty i czasu
 */
void at_setDateTime(const char* params)
{
	uint8_t year, month, day, hour, min, sec;
	uint8_t tmp[2];

	tmp[0] = params[0] - 0x30;
	tmp[1] = params[1] - 0x30;
	year = 10*tmp[0]+tmp[1];	// wyliczenie roku

	tmp[0] = params[3] - 0x30;
	tmp[1] = params[4] - 0x30;
	month = 10*tmp[0]+tmp[1];	// miesiąc

	tmp[0] = params[6] - 0x30;
	tmp[1] = params[7] - 0x30;
	day = 10*tmp[0]+tmp[1];		// dzień

	//// czas
	tmp[0] = params[9] - 0x30;
	tmp[1] = params[10] - 0x30;
	hour = 10*tmp[0]+tmp[1];	// godzina

	tmp[0] = params[12] - 0x30;
	tmp[1] = params[13] - 0x30;
	min = 10*tmp[0]+tmp[1];		// minuta

	tmp[0] = params[15] - 0x30;
	tmp[1] = params[16] - 0x30;
	sec = 10*tmp[0]+tmp[1];		// sekunda

#ifndef DEBUG
	ds1307_setdate(year, month, day, hour, min, sec);
#endif

	char buf[256];
	sprintf(buf, "\r\nSet datetime:\r\n +Date: %d/%d/%d\r\n +Time: %d:%d:%d\r\n\n", year, month, day, hour, min, sec);
	uart_puts(buf);
}

/**
 * tablica z poleceniami AT
 */
const Command_t commands[MAX_COMMANDS] = {
		{ "AT+DATETIME",		at_setDateTime },
		{ "AT+STATUS",			at_status },
		{ "AT+GRASS_TIME",		at_setGrassTime },
		{ "AT+TREES_TIME",		at_setTreesTime },
		{ "AT+FLOWERS_TIME",	at_setFlowersTime },
		{ "AT+GRASS_DAY",		at_setGrassDay },
		{ "AT+WETNESS",			at_setWetness },
		{ "AT+IRRIGATION_HOUR",	at_setIrrigationHour },
		{ "AT+GRASS_TEMP",		at_setGrassTemp },
		{ "AT+FLOWERS_TEMP",	at_setFlowersTemp },
		{ "AT+HELP",			at_help },
		{ "AT+ON",				at_on },
		{ "AT",					at_response }
};

/**
 * Sprawdza czy przychodzi polecenie konfiguracji
 * poprzez UART
 */
void checkCommand()
{
	char data;
	char cmd[64];
	uint8_t index = 0;

	data = uart_getc();
	if (data == '@') {
		memset(cmd, 0, sizeof(cmd));
		do {
			data = uart_getc();
			if (data != 0x00) {
				cmd[index++] = data;
				uart_putc(data);
			}
		} while (data != '\r');
		cmd[index++] = '\r';

		for(uint8_t i = 0; i < MAX_COMMANDS; i++) {
			if ( strstr(cmd, commands[i].name) )
			{
				uint8_t start = 0;
				uint8_t counter = 0;
				char param[32];
				for (uint8_t i = 0; i < 32; i++)
					if (cmd[i] == '=')
						start = i;
				start++;

				for(uint8_t i = start; i < 32-start; i++)
					param[counter++] = cmd[i];

				commands[i].action(param);
			}
		}
	}
}
