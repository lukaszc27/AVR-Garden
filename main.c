/*
 * main.c
 *
 *  Created on: 22 lip 2019
 *      Author: lukasz
 */
#include <avr/io.h>
#include <avr/interrupt.h>
#include <avr/eeprom.h>
#include <avr/pgmspace.h>
#include <util/delay.h>
#include <stdio.h>
#include <string.h>

#include "libs/uart.h"
#include "libs/ds1307/ds1307.h"
#include "libs/1Wire/ds18x20.h"
#include "vars.h"
#include "defs.h"
#include "commands.h"


volatile uint8_t sec_flag;
volatile uint8_t secCounter;

uint8_t sensorCount;	// ilość czujników DS18x20 podłączonych do magistrali
volatile uint8_t led_flag;
DateTime_t dateTime;
uint8_t subzero, cel, cel_fract_bits;
int temperature;	// temperatura odczytana z czujnika DS18B20
uint16_t avg_wetness;

//// Funkcja superDebounce (ATNEL)
volatile uint16_t Timer1, Timer2;

void atmega_initialize();
void irrigation(void (*area1)(const DateTime_t), void (*area2)(const DateTime_t), void (*area3)(const DateTime_t));
void grassIrrigation(const DateTime_t);
void treesIrrigation(const DateTime_t);
void flowersIrrigation(const DateTime_t);
void handIrrigation();
void factorySettings();
void checkAndLoadDefaults();

uint16_t adc_measurement(uint8_t channel);
uint8_t TimeCompare(const DateTime_t a, const DateTime_t b);

void SuperDebounce(uint8_t * key_state, volatile uint8_t *KPIN,
		uint8_t key_mask, uint16_t rep_time, uint16_t rep_wait,
		void (*push_proc)(void), void (*rep_proc)(void) );


Settings_t settings;				// ustawienia w pamięci RAM
Settings_t ee_settings EEMEM;		// ustawienia zapisane w pamięci EEPROM
const Settings_t flash_settings PROGMEM = {
		5,			// czas nawadniania trawnika (min) +
		10,			// czas nawadiania drzew (min) +
		10,			// czas nawadniania krzewów (min) +
		0, 0, 0,	// flagi nawadnianych sektorów (trawnik, drzewa, krzewy)
		2,			// co który dzięn nawadniać trawnik +
		25,			// min. temperatuda do nawadniania trawnika
		20,			// min. temperatura do nawadniania krzewów
		19,			// po której godzinie rozpocząć nawadnianie +
		500			// minimalna wilgotność do włączenia podlewania +
};


int main(void)
{
	atmega_initialize();

//	led.flag = 0;
//	led.status = BLINK;

	uint8_t keyState;

	checkAndLoadDefaults();

	//// główna pętla programu
	while(1)
	{
		checkCommand();
		SuperDebounce(&keyState, &PIND, BUTTON_PIN, 20, 500, &handIrrigation, &factorySettings);

		if (sec_flag)
		{
			switch (secCounter % 3)
			{
			case 0:	// szukanie czujników DS18x20
				sensorCount = search_sensors();

#ifdef DEBUG
				uart_puts_p(PSTR("Find temperature sensors.\r\n"));
#endif
				break;

			case 1:	// wysłanie rozkazu wykonania pomiaru
				DS18X20_start_meas(DS18X20_POWER_EXTERN, NULL);
				break;

			case 2:	// odczyt danych z czujników oraz podejmowanie decyzji o podlewaniu
#ifdef DEBUG
				uart_puts_p(PSTR("Read data from sensors.\r\n"));
#endif
				DS18X20_read_meas(gSensorIDs[0], &subzero, &cel, &cel_fract_bits);
				avg_wetness = adc_measurement(PA0);
				ds1307_getdate(&dateTime.year, &dateTime.month, &dateTime.day,
						&dateTime.hour, &dateTime.min, &dateTime.sec);

				temperature = cel;
				if (subzero) temperature *= -1;

				if (dateTime.hour >= 0 && dateTime.hour < settings.minHourIrrigation)
				{
					if (settings.grassIrrigation_flag || settings.flowersIrrigation_flag || settings.treesIrrigation_flag)
					{
						settings.grassIrrigation_flag = 0;
						settings.flowersIrrigation_flag = 0;
						settings.treesIrrigation_flag = 0;

						eeprom_write_block(&settings, &ee_settings, sizeof(settings));
					}
				}

				irrigation(&grassIrrigation,
						&treesIrrigation,
						&flowersIrrigation);	// co 3s próba nawodnienia ogródka
				break;
			}

			sec_flag = 0;
		}

		if (led_flag) {
			LED_TOG;
			led_flag = 0;
		}
//		switch(led.status)
//		{
//		case ON: LED_ON; break;
//		case OFF: LED_OFF; break;
//		case BLINK:
//			if(led.flag) {
//				LED_TOG;
//				led.flag = 0;
//			}
//			break;
//		}
	} // while
	return 0;
}

/**
 * uruchomienie różych modułów/liczników
 */
void atmega_initialize()
{
	uart_init( UART_BAUD_SELECT(9600, F_CPU) );
	ds1307_init();

	// timer0
	TCCR0 |= (1 << WGM01);				// tryb CTC
	TCCR0 |= (1 << CS02) | (1 << CS00);	// preskaler 1024
	OCR0 = 156;
	TIMSK |= (1 << OCIE0);				// Timer/Counter0 output compare match interrupt enable

	LED_DDR |= LED_PIN;															// dioda jako wyjście
	AREAS_SWITCH |= AREA_SWITCH_1_PIN | AREA_SWITCH_2_PIN | AREA_SWITCH_3_PIN;	// wyjścia sterowania dla przekaźników

	// uruchomienie przetwornika ADC
	ADMUX |= (1 << REFS0);									// AVCC with external capacitor at AREF pin
	ADCSRA |= (1 << ADEN) | (1 << ADPS2) | (1 << ADPS1);	// włączenie przetwornika oraz wybór preskalera 64

	BUTTON_PORT |= BUTTON_PIN;	// pull up  na przycisk ręcznego nawadniania


	sei();	// globalne odblokowanie przerwań
}

/**
 * proces nawadniania ogródka
 */
void irrigation(void (*area1)(const DateTime_t), void (*area2)(const DateTime_t), void (*area3)(const DateTime_t))
{
#ifdef DEBUG
	uart_puts_p(PSTR("--- Irrigation function is running ---\r\n"));
#endif

	static uint8_t count;

	if ((dateTime.day % settings.grassDayIrrigation == 0) && (dateTime.hour >= settings.minHourIrrigation) && (temperature > settings.grassMinTemperature) && (count == 0))
	{
		// co drugi dzień po godzinie 12 podlewanie trwanika
		DateTime_t time;	// czas do którego trawnik ma być podlewany
		time.year = dateTime.year;
		time.month = dateTime.month;
		time.day = dateTime.day;
		time.hour = dateTime.hour;
		time.min = dateTime.min + settings.grassTimeIrrigation;
		time.sec = dateTime.sec;

		if (!settings.grassIrrigation_flag) {
#ifdef DEBUG
			uart_puts_p(PSTR("irrigation: grass\t"));
#endif
			area1(time);
			settings.grassIrrigation_flag = 1;
		}
	}

	if ((avg_wetness < settings.minWetness) && (dateTime.hour >= settings.minHourIrrigation) && (temperature > settings.flowerMinTemperature) && (count == 1))
	{
		// gdy wilgotność jest niska oraz temperatura powietrza większa niż 20 st
		// oraz po godzinie 19 podlewanie drzew
		DateTime_t time;
		time.year = dateTime.year;
		time.month = dateTime.month;
		time.day = dateTime.day;
		time.hour = dateTime.hour;
		time.min = dateTime.min + settings.treesTimeIrrigation;
		time.sec = dateTime.sec;

		if (!settings.treesIrrigation_flag) {
#ifdef DEBUG
			uart_puts_p(PSTR("irrigation: trees\t"));
#endif
			area2(time);
			settings.treesIrrigation_flag = 1;
		}
	}

	if ((avg_wetness < settings.minWetness) && (dateTime.hour >= settings.minHourIrrigation) && (temperature > settings.flowerMinTemperature) && (count == 2))
	{
		// gdy wilgotność jest niska oraz temperatura powietrza większa niż 20 st
		// oraz po godzinie 19 podlewanie krzewów
		DateTime_t time;
		time.year = dateTime.year;
		time.month = dateTime.month;
		time.day = dateTime.day;
		time.hour = dateTime.hour;
		time.min = dateTime.min + settings.flowersTimeIrrigation;
		time.sec = dateTime.sec;

		if (!settings.flowersIrrigation_flag) {
#ifdef DEBUG
			uart_puts_p(PSTR("irrigation: flowers\t"));
#endif
			area3(time);
			settings.flowersIrrigation_flag = 1;
		}
	}

	count++;
	if(count > 2) count = 0;

	// zapisujemy stany aby po wyłączeniu zasilanie nie podlewało niepotrzebnie kolejny raz
	eeprom_write_block(&settings, &ee_settings, sizeof(settings));
}

/**
 * nawadnianie trawnika (strefy pierwszej)
 * Podlewanie trawnika odbywa się:
 * 	+ Co 3 dzień
 * 	+ Po godzinie 19
 * 	+ Gdy temperatura powietrza przekracza 25 st.
 */
void grassIrrigation(const DateTime_t time)
{
	AREA_SWITCH_1_ON;

	DateTime_t current;
	do {
		ds1307_getdate(&current.year, &current.month, &current.day,
				&current.hour, &current.min, &current.sec);

		if (led_flag) {
			LED_TOG;
			led_flag = 0;
		}

//		switch(led.status)
//		{
//		case ON: LED_ON; break;
//		case OFF: LED_OFF; break;
//		case BLINK:
//			if (led.flag) {
//				LED_TOG;
//				led.flag = 0;
//			}
//			break;
//		}
	} while(TimeCompare(current, time) == 2);

	AREA_SWITCH_1_OFF;

#ifdef DEBUG
	uart_puts_p(PSTR("OK\r\n"));
#endif
}

/**
 * podlewanie drzew (area2)
 * podlewanie odbywa się:
 * 	+ po godzinie 19
 */
void treesIrrigation(const DateTime_t time)
{
	AREA_SWITCH_2_ON;

	DateTime_t current;
	do {
		ds1307_getdate(&current.year, &current.month, &current.day,
				&current.hour, &current.min, &current.sec);

		if (led_flag) {
			LED_TOG;
			led_flag = 0;
		}

//		switch(led.status)
//		{
//		case ON: LED_ON; break;
//		case OFF: LED_OFF; break;
//		case BLINK:
//			if (led.flag) {
//				LED_TOG;
//				led.flag = 0;
//			}
//			break;
//		}
	} while(TimeCompare(current, time) == 2);

	AREA_SWITCH_2_OFF;

#ifdef DEBUG
	uart_puts_p(PSTR("OK\r\n"));
#endif
}

void flowersIrrigation(const DateTime_t time)
{
	AREA_SWITCH_3_ON;

	DateTime_t current;
	do {
		ds1307_getdate(&current.year, &current.month, &current.day,
				&current.hour, &current.min, &current.sec);

		if (led_flag) {
			LED_TOG;
			led_flag = 0;
		}

//		switch(led.status)
//		{
//		case ON: LED_ON; break;
//		case OFF: LED_OFF; break;
//		case BLINK:
//			if (led.flag) {
//				LED_TOG;
//				led.flag = 0;
//			}
//			break;
//		}
	} while(TimeCompare(current, time) == 2);

	AREA_SWITCH_3_OFF;

#ifdef DEBUG
	uart_puts_p(PSTR("OK\r\n"));
#endif
}

/**
 * Uruchamia proces nawadniania ogrodu na życzenie klienta
 * (krótkie przytrzymanie przycisku)
 */
void handIrrigation()
{
	for(uint8_t index = 0; index < 3; index++)
	{
		DateTime_t time;
		ds1307_getdate(&time.year, &time.month, &time.day,
				&time.hour, &time.min, &time.sec);

		switch (index)
		{
		case 0:	// co 3 dzień podlewanie trawnika przez 5 min
			time.min += settings.grassTimeIrrigation;
			if ((time.day % settings.grassDayIrrigation) == 0)
				grassIrrigation(time);
			break;

		case 1:	// podlewanie dużych drzew przez 10 min
			time.min += settings.treesTimeIrrigation;
			treesIrrigation(time);
			break;

		case 2:	// podlewanie krzewów przez 10 min
			time.min += settings.flowersTimeIrrigation;
			flowersIrrigation(time);
			break;
		}
	}
}

/**
 * przywraca domyślne ustawienia czasów nawadniania
 * (dłuższe przytrzymanie klawisza)
 */
void factorySettings()
{
//	led.status = OFF;

	for(uint8_t i = 0; i < 32; i++) {
		LED_TOG;
		_delay_ms(100);
	}
	static uint8_t flag;
	if (!flag) {
		flag++;
		memcpy_P(&settings, &flash_settings, sizeof(settings));			// wczytanie ustawień fabrycznych do pamięci RAM
		_delay_ms(20);
		eeprom_write_block(&settings, &ee_settings, sizeof(settings));	// zapis ustawień fabrycznych w pamięci EEPROM
	}

	flag = 0;
//	led.status = BLINK;
}

/**
 * sprawdza czy dane są zapisane w pamięci EEPROM
 * jeśli nie ma to ładuje dane fabryczne
 */
void checkAndLoadDefaults()
{
#ifdef DEBUG
	uart_puts_p(PSTR("--- checkAndLoadDefaults function ---\r\n"));
#endif
	uint8_t i, length = sizeof(settings);
	uint8_t* pointer = (uint8_t*)&settings;

	eeprom_read_block(&settings, &ee_settings, sizeof(settings));	// eeprom to ram
	for (i = 0; i < length; i++) {
		if (*pointer++ == 0xFF)
			continue;

		break;
	}

	if (length == i) {
		factorySettings();
#ifdef DEBUG
		uart_puts_p(PSTR("[1] Load factory settings\tOK\r\n"));
#endif
	}
}

/**
 * @desc: funkcja porównująca dwa czasy
 * @return: 0 - jeśli czasy są identyczne
 * 			1 - jeśli czas a > b
 * 			2 - jeśli czas a < b
 */
uint8_t TimeCompare(const DateTime_t a,const DateTime_t b)
{
	if (a.hour > b.hour)
		return 1;
	else if (a.hour < b.hour)
		return 2;
	else if (a.hour == b.hour)
	{
		if (a.min > b.min)
			return 1;
		else if (a.min < b.min)
			return 2;
		else if (a.min == b.min)
		{
			if (a.sec > b.sec)
				return 1;
			else if (a.sec < b.sec)
				return 2;
		}
	}

	return 0;
}

/**
 * wykonuje pomiar na żadanie na wybranym kanale ADC
 */
uint16_t adc_measurement(uint8_t channel)
{
	ADMUX = (ADMUX & 0xF8) | channel;	// ustawienie wybranego kanału
	ADCSRA |= (1 << ADSC);				// rozpoczęcie wykonywania pomiaru

	while( ADCSRA & (1 << ADSC) );		// czekamy na zakończenie pomiaru ADC

	return ADCW;	// wynik pomiaru (połączenie rejestrów ADCH, ADCL)
}


ISR(TIMER0_COMP_vect)

{
	static uint8_t ledCounter;
	if(++ledCounter > 60) {
		led_flag = 1;
		ledCounter = 0;
	}

	// primitywne odliczanie czasu
	static uint8_t second_cnt;
	if (++second_cnt > 100) {
		sec_flag = 1;
		secCounter++;
		if (secCounter > 59) secCounter = 0;

		second_cnt = 0;
	}

	uint16_t n;

	n = Timer1;		/* 100Hz Timer1 */
	if (n) Timer1 = --n;
	n = Timer2;		/* 100Hz Timer2 */
	if (n) Timer2 = --n;
}

/************** funkcja SuperDebounce do obsługi pojedynczych klawiszy ***************
 * 							AUTOR: Mirosław Kardaś
 * ZALETY:
 * 		- nie wprowadza najmniejszego spowalnienia
 * 		- posiada funkcję REPEAT (powtarzanie akcji dla dłużej wciniętego przycisku)
 * 		- można przydzielić różne akcje dla trybu REPEAT i pojedynczego kliknięcia
 * 		- można przydzielić tylko jednš akcję wtedy w miejsce drugiej przekazujemy 0 (NULL)
 *
 * Wymagania:
 * 	Timer programowy utworzony w oparciu o Timer sprzętowy (przerwanie 100Hz)
 *
 * 	Parametry wejściowe:
 *
 * 	*key_state - wskaźnik na zmiennš w pamięci RAM (1 bajt) - do przechowywania stanu klawisza
 *  *KPIN - nazwa PINx portu na którym umieszczony jest klawisz, np: PINB
 *  key_mask - maska klawisza np: (1<<PB3)
 *  rep_time - czas powtarzania funkcji rep_proc w trybie REPEAT
 *  rep_wait - czas oczekiwania do przejścia do trybu REPEAT
 *  push_proc - wskaźnik do własnej funkcji wywoływanej raz po zwolenieniu przycisku
 *  rep_proc - wskaźnik do własnej funkcji wykonywanej w trybie REPEAT
 **************************************************************************************/
void SuperDebounce(uint8_t * key_state, volatile uint8_t *KPIN,
		uint8_t key_mask, uint16_t rep_time, uint16_t rep_wait,
		void (*push_proc)(void), void (*rep_proc)(void) ) {

	enum {idle, debounce, go_rep, wait_rep, rep};

	if(!rep_time) rep_time=20;
	if(!rep_wait) rep_wait=150;

	uint8_t key_press = !(*KPIN & key_mask);

	if( key_press && !*key_state ) {
		*key_state = debounce;
		Timer1 = 15;
	} else
	if( *key_state  ) {

		if( key_press && debounce==*key_state && !Timer1 ) {
			*key_state = 2;
			Timer1=5;
		} else
		if( !key_press && *key_state>1 && *key_state<4 ) {
			if(push_proc) push_proc();						/* KEY_UP */
			*key_state=idle;
		} else
		if( key_press && go_rep==*key_state && !Timer1 ) {
			*key_state = wait_rep;
			Timer1=rep_wait;
		} else
		if( key_press && wait_rep==*key_state && !Timer1 ) {
			*key_state = rep;
		} else
		if( key_press && rep==*key_state && !Timer1 ) {
			Timer1 = rep_time;
			if(rep_proc) rep_proc();						/* KEY_REP */
		}
	}
	if( *key_state>=3 && !key_press ) *key_state = idle;
}


