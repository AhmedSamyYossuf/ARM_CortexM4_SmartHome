/*
 * App.c
 *
 *  Created on: Nov 25, 2020
 *      Author: asmy2
 */
#include "stm32f4xx.h"
#include "board.h"
#include "keypad.h"
#include "LCD.h"
#include "delay.h"
#include "EEPROM.h"
#include "Servo.h"
#include "ADC.h"
#include "ultrasonic.h"
#include "WIFI.h"

#define PASS_LEN 			4
#define CONFIRM_SUCCESS		1
#define CONFIRM_FAIL		0
#define PASSWORD_SAVED		0x51
#define InDoorDis			20

char g_myPassword[ PASS_LEN ];


void getPassword( char * password )
{
	uint8_t i;
	for( i = 0 ; i<PASS_LEN; i++ )
	{
		while( Keypad_GetKey() == 0 );
		password[i] = Keypad_GetKey();
		delayMs(1000);
		LCD_DispCharXY(1,i,'*');
	}
}

uint8_t confirmPassword( char const * password )
{
	uint8_t i = 0;
	uint8_t result = CONFIRM_SUCCESS;
	uint8_t new_key;

	for( i = 0; i<PASS_LEN; i++ )
	{
		while( Keypad_GetKey() == 0 );
		new_key = Keypad_GetKey();
		delayMs(1000);
		LCD_DispCharXY(1,i,'*');
		if( new_key != password[i] )
		{
			result = CONFIRM_FAIL;
		}
	}
	return result;
}


void FirstTimePassword(void)
{
	uint8_t i = 0;
	do
	{
		LCD_Clear();
		LCD_DispStrXY(0,0,"Enter pass: ");
		getPassword( g_myPassword );
		LCD_Clear();
		LCD_DispStrXY(0,0,"Confirm pass: ");
	}
	while( confirmPassword( g_myPassword ) != CONFIRM_SUCCESS );

	LCD_Clear();
	LCD_DispStrXY(0,0,"Success ");
	delayMs(2000);
	EEPROM_WriteByte(0xFF, PASSWORD_SAVED );
	/* Add the new password to the EEPROM */
	for(i=0; i<PASS_LEN; i++)
	{
		EEPROM_WriteByte(0xF0+i,g_myPassword[i]);
	}
}


void SavedPassword(void)
{
	uint8_t i = 0;
	uint8_t Password_count = 0;
	for(i=0; i<PASS_LEN; i++)
	{
		EEPROM_ReadByte(0xF0+i,&g_myPassword[i]);
	}
	LCD_Clear();
	LCD_DispStrXY(0,0,"Check pass: ");
	while( (confirmPassword(g_myPassword) != CONFIRM_SUCCESS ) )
	{
		/* Check if the user entered it incorrectly or not in 3 times */
		if( Password_count >=3 )
		{
			LCD_Clear();
			LCD_DispStrXY(0,0,"Unauthorized ");
			Buz_On();
			delayMs(5000);
			Buz_Off();
			Password_count = 0;
		}
		Password_count++;
		LCD_Clear();
		LCD_DispStrXY(0,0,"Check pass: ");
	}
	LCD_Clear();
	LCD_DispStrXY(0,0,"Confirmed ");
	delayMs(2000);

}

void ChangePassword(void)
{
	SavedPassword();
	FirstTimePassword();
}

void DoorLock(void){
	uint8_t key = 0;

	LCD_Clear();
	LCD_DispStrXY(0,0,"1. Enter PW");
	LCD_DispStrXY(1,0,"2. Change PW");

	while( Keypad_GetKey() == 0);
	key = Keypad_GetKey();
	delayMs(1000);
	switch(key)
	{
	case 1: SavedPassword();
	Servo_SetPosition(POS_DEG_90);
	delayMs(5000);
	Servo_SetPosition(POS_DEG_0);
	Led_On(LED0);
	break;
	case 2: ChangePassword();
	break;
	default:
		break;
	}



}
//////////////////////////////////////////////////////////////////////////////////////////////////////
void Temp_Sensor(void){
	uint32_t adcValue = 0;
	uint8_t str[30];
	ADC1_SelectChannel(CH1);
	LCD_DispStrXY(0,0,"Room Temp = ");

	adcValue = ADC1_Read();
	adcValue = adcValue*(330)/4096;
	LCD_DispStrXY(1,5,"    ");
	LCD_DispIntXY(1,5,adcValue);
	if(adcValue>25){
		Buz_On();
		if(WIFI_SendCmd("AT+CIPSEND=26\r\n","OK",1000)){
			delayMs(10);
			sprintf(str,"The Temp. is too high  %d  C",adcValue);
			Uart_SendString(USART1,str);
		}
	delayMs(3000);
	Buz_Off();
	}else{
		Buz_Off();
	}
	delayMs(5000);

}
/////////////////////////////////////////////////////////////////////////////////////////////////////
void InDoor_Ultra(void){
	uint16_t Distance = Ultra_GetDistance();
	if(Distance < InDoorDis){
		LCD_Clear();
		LCD_DispInt(Distance);
		Relay_On();
		delayMs(2000);
		Relay_Off();
		Led_On(LED1);
	}
}
////////////////////////////////////////////////////////////////////////////////////////////////////
#if 0
volatile uint32_t tickCount = 0;

void SysTick_Handler(void){
	/*  TimingDelay_Decrement(); */
	tickCount++;
	if(tickCount%100 == 0){
		//InDoor_Ultra();
		//LCD_Clear();
		//LCD_DispInt(Ultra_GetDistance());
	}
	/*
	else{
		LCD_Clear();
		LCD_DispInt(Ultra_GetDistance());
	}
	 */
}
#endif


/////////////////////////////////////////////////////////////////////////////////////////////////////
void EXTI1_IRQHandler(void){
	EXTI->PR	|=  EXTI_PR_PR1;
	DoorLock();
}
void EXTI15_10_IRQHandler(void){
	if(EXTI->PR & EXTI_PR_PR11){
		EXTI->PR	|=  EXTI_PR_PR11;
		Leds_Toggle(0x0F);
	}
	else if(EXTI->PR & EXTI_PR_PR12){
		EXTI->PR	|=  EXTI_PR_PR12;
		Leds_Toggle(0xF0);
	}
}
///////////////////////////////////////////////////////////////////////////////////////////////////
void WifiApp (void){

	if(WIFI_SendCmd("AT\r\n","OK",1000)){
		Uart_SendString(USART2,"WIFI is Up\n");
	}else{
		Uart_SendString(USART2,"WIFI is Down\n");
	}
	delayMs(100);
	/* Echo Command */
	if(WIFI_SendCmd("ATE0\r\n","OK",1000)){
		Uart_SendString(USART2,"Echo Disabled\n");
	}else{
		Uart_SendString(USART2,"Echo not Disabled\n");
	}
	delayMs(100);
	/* Station Mode Command */
	if(WIFI_SendCmd("AT+CWMODE=1\r\n","OK",1000)){
		Uart_SendString(USART2,"Station Mode Done\n");
	}else{
		Uart_SendString(USART2,"Station Mode ERROR\n");
	}
	delayMs(100);
	/* WI-FI Network Connection */
	if(WIFI_SendCmd("AT+CWJAP=\"Ahmed Samy\",\"ahmedsamy1463\"\r\n","OK",10000)){
		Uart_SendString(USART2,"Network Connected\n");
	}else{
		Uart_SendString(USART2,"Network ERROR\n");
	}
	delayMs(100);
	/* TCP Connection */
	if(WIFI_SendCmd("AT+CIPSTART=\"TCP\",\"192.168.0.101\",4444\r\n","OK",10000)){
		Uart_SendString(USART2,"TCP Connected\n");
	}else{
		Uart_SendString(USART2,"TCP ERROR\n");
	}
	delayMs(100);
	/*
	if(WIFI_SendCmd("AT+CIPSEND=2\r\n","OK",1000)){
		delayMs(10);
		Uart_SendString(USART1,"Hi");
	}
	*/

	/*
		if(Uart_ReceiveByte_Unblock(USART2,&data)){
			LCD_DispStr(data);
		}
		delayMs(100);
	 */
}
///////////////////////////////////////////////////////////////////////////////////////////////////
void System_Init(void)
{
	uint8_t EEPROM_checkByte = 0;

	RCC_DeInit();				/* Adapt PLL to the internal 16 MHz RC oscillator */
	SystemCoreClockUpdate();	/* Update SystemCoreClock */
	LCD_Init();
	Keypad_Init();
	EEPROM_Init();
	Buz_Init();
	Servo_Init();
	ADC1_Init();
	Ultra_Init();
	Relay_Init();
	Leds_Init(0xFF);
	Uart_Init(USART1,115200);
	Uart_Init(USART2,115200);
	WifiApp();
	Btn_Init_EXTI(BTN_UP,EXTI1_IRQHandler);
	Btn_Init_EXTI(BTN_RIGHT,EXTI15_10_IRQHandler);
	Btn_Init_EXTI(BTN_LEFT,EXTI15_10_IRQHandler);


	EEPROM_ReadByte(0xFF,&EEPROM_checkByte);
	/* If its first time initializing the system, ask the user for a new password */
	if( EEPROM_checkByte != PASSWORD_SAVED )
	{
		FirstTimePassword();
	}
}
