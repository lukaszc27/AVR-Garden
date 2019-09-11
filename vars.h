/*
 * vars.h
 *
 *  Created on: 23 lip 2019
 *      Author: lukasz
 */

#ifndef VARS_H_
#define VARS_H_

typedef struct {
	uint8_t day;
	uint8_t month;
	uint8_t year;

	uint8_t hour;
	uint8_t min;
	uint8_t sec;
} DateTime_t;

/**
 * struktura konfiguracyjna
 */
typedef struct {
	uint8_t grassTimeIrrigation;		// czas nawadniania trawnika
	uint8_t treesTimeIrrigation;		// czas nawadniania dużych drzew
	uint8_t flowersTimeIrrigation;		// czas nawadniania krzewów

	uint8_t grassIrrigation_flag;		// flagi nawadniania
	uint8_t treesIrrigation_flag;		//
	uint8_t flowersIrrigation_flag;		//

	uint8_t grassDayIrrigation;			// co który dzień nawadniać trawnik
	uint8_t grassMinTemperature;		// minimalna temperatura powietrza do nawadniania trawnika
	uint8_t flowerMinTemperature;		// minimalna temperatura do nawadniania drzew i krzewów
	uint8_t minHourIrrigation;			// po której godzinie rozpocząć nawadnianie
	uint16_t minWetness;					// minimalna wilgoność do włączenia podlewania
} Settings_t;


/**
 * struktura opisująca diodę informacyjną
 */
//typedef enum {
//	OFF = 0,
//	ON,
//	BLINK
//}LedStatus;

//typedef struct {
//	LedStatus status;
//	uint8_t flag;
//}LED_t;

//volatile LED_t led;


#endif /* VARS_H_ */
