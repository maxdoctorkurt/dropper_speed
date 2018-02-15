#include "stm32f1xx_hal.h"
#include "stm32f1xx_hal_gpio.h"
#include "stm32f1xx_hal_rcc.h"
#include "stm32f1xx_hal_tim.h"

#define timeSamples 10
#define speedValue 0

int delay_counter = 0;

void clocksInit();
void resetDigitLEDBits();
void resetDigitSelection();
void selectDigit(int);
void selectDigit1();
void selectDigit2();
void selectDigit3();
void selectDigit4();
void display(int);
void display0();
void display1();
void display2();
void display3();
void display4();
void display5();
void display6();
void display7();
void display8();
void display9();
void displayPoint();
void delay(int);

TIM_HandleTypeDef tim4;
TIM_HandleTypeDef tim2;
TIM_HandleTypeDef tim3;

uint32_t timeValues[timeSamples] = {};
uint32_t timeCurrentIndex = 0;

float averageTicks = 0;

int main(void) {

	clocksInit();

	// просто лампочка для индикации
	GPIO_InitTypeDef gpio;
	gpio.Mode = GPIO_MODE_OUTPUT_PP;
	gpio.Pin = GPIO_PIN_13;
	gpio.Speed = GPIO_SPEED_FREQ_LOW;
	HAL_GPIO_Init(GPIOC, &gpio);

	// катод ик фотодиода
	gpio.Pull = GPIO_PULLDOWN;
	gpio.Mode = GPIO_MODE_IT_FALLING;
	gpio.Pin = GPIO_PIN_12;
	gpio.Speed = GPIO_SPEED_FREQ_LOW;
	HAL_GPIO_Init(GPIOA, &gpio);

	// восьмисегментный экран (катоды)
	gpio.Pull = GPIO_NOPULL;
	gpio.Mode = GPIO_MODE_OUTPUT_PP;
	gpio.Pin = GPIO_PIN_0 | GPIO_PIN_1 | GPIO_PIN_2 | GPIO_PIN_3 | GPIO_PIN_4 | GPIO_PIN_5 | GPIO_PIN_6 | GPIO_PIN_7;
	gpio.Speed = GPIO_SPEED_FREQ_LOW;
	HAL_GPIO_Init(GPIOA, &gpio);

	// восьмисегментный экран (аноды выбора текущей цифры)
	gpio.Pull = GPIO_NOPULL;
	gpio.Mode = GPIO_MODE_OUTPUT_PP;
	gpio.Pin = GPIO_PIN_8 | GPIO_PIN_9 | GPIO_PIN_10 | GPIO_PIN_11;
	gpio.Speed = GPIO_SPEED_FREQ_LOW;
	HAL_GPIO_Init(GPIOA, &gpio);

	// таймер для отображения числа
	tim2.Init.ClockDivision = TIM_CLOCKDIVISION_DIV1;
	tim2.Init.CounterMode = TIM_COUNTERMODE_UP;
	tim2.Init.Period = 100U;
	tim2.Init.Prescaler = 36000U;
	tim2.Instance = TIM2;
	HAL_TIM_Base_Init(&tim2);

	// таймер для подсчета времени (он должен постоянно сбрасываться)
	tim3.Init.ClockDivision = TIM_CLOCKDIVISION_DIV1;
	tim3.Init.CounterMode = TIM_COUNTERMODE_UP;
	tim3.Init.Period = 30000U;
	tim3.Init.Prescaler = 36000U;
	tim3.Instance = TIM3;
	HAL_TIM_Base_Init(&tim3);

	// таймер для delay
	tim4.Init.ClockDivision = TIM_CLOCKDIVISION_DIV1;
	tim4.Init.CounterMode = TIM_COUNTERMODE_UP;
	tim4.Init.Period = 1U;
	tim4.Init.Prescaler = 36000U;
	tim4.Instance = TIM4;
	HAL_TIM_Base_Init(&tim4);

	// прерывания
	__disable_irq();


	HAL_NVIC_SetPriority(EXTI15_10_IRQn, 1, 0);
	HAL_NVIC_EnableIRQ(EXTI15_10_IRQn);

	HAL_NVIC_SetPriority(TIM2_IRQn, 3, 0);
	HAL_NVIC_EnableIRQ(TIM2_IRQn);
	__HAL_TIM_ENABLE_IT(&tim2, TIM_IT_UPDATE);

	HAL_NVIC_SetPriority(TIM3_IRQn, 2, 1);
	HAL_NVIC_EnableIRQ(TIM3_IRQn);
	__HAL_TIM_ENABLE_IT(&tim3, TIM_IT_UPDATE);

	HAL_NVIC_SetPriority(TIM4_IRQn, 2, 0);
	HAL_NVIC_EnableIRQ(TIM4_IRQn);
	__HAL_TIM_ENABLE_IT(&tim4, TIM_IT_UPDATE);

	// включаем таймеры
	__HAL_TIM_ENABLE(&tim2);
	__HAL_TIM_ENABLE(&tim3);
	__HAL_TIM_ENABLE(&tim4);

	__enable_irq();

	HAL_GPIO_WritePin(GPIOC, GPIO_PIN_13, GPIO_PIN_RESET);


	while(1);

}

void clocksInit() {
	RCC->CR |= RCC_CR_HSEON;

	while(!(RCC->CR & RCC_CR_HSERDY));

	FLASH->ACR |= FLASH_ACR_PRFTBE;

	FLASH->ACR &= ~FLASH_ACR_LATENCY;
	FLASH->ACR |= FLASH_ACR_LATENCY_2;

	/* HCLK = SYSCLK */
	RCC->CFGR |= RCC_CFGR_HPRE_DIV1;

	/* PCLK2 = HCLK */
	RCC->CFGR |= RCC_CFGR_PPRE2_DIV1;

	/* PCLK1 = HCLK */
	RCC->CFGR |= RCC_CFGR_PPRE1_DIV2;

	RCC->CFGR &= ~(RCC_CFGR_PLLSRC | RCC_CFGR_PLLXTPRE | RCC_CFGR_PLLMULL);
	RCC->CFGR |= RCC_CFGR_PLLXTPRE_HSE | RCC_CFGR_PLLMULL9;

	RCC->CR |= RCC_CR_PLLON;

	/* Ожидаем, пока PLL выставит бит готовности */
	while(!(RCC->CR & RCC_CR_PLLRDY));

	/* Выбираем PLL как источник системной частоты */
	RCC->CFGR &= ~RCC_CFGR_SW;
	RCC->CFGR |= RCC_CFGR_SW_PLL;

	/* Ожидаем, пока PLL выберется как источник системной частоты */
	while ((RCC->CFGR & (uint32_t)RCC_CFGR_SWS) != (uint32_t)0x08);

	RCC->APB1ENR |= RCC_APB1ENR_TIM4EN;
	RCC->APB1ENR |= RCC_APB1ENR_TIM3EN;
	RCC->APB1ENR |= RCC_APB1ENR_TIM2EN;
	RCC->APB2ENR |= RCC_APB2ENR_IOPCEN;
	RCC->APB2ENR |= RCC_APB2ENR_IOPAEN;
}

void TIM2_IRQHandler() {

	if(__HAL_TIM_GET_FLAG(&tim2, TIM_FLAG_UPDATE)) {
		__HAL_TIM_CLEAR_IT(&tim2, TIM_FLAG_UPDATE);

		float frequency = 1.0 / (averageTicks / 36000000.0);
//
//		double a, b; // a - целая часть, b - дробная
//		b = modf(frequency, &a);

//		int dig1 = ((int) a / 10) % 10;
//		int dig2 = (int) a % 10;
//
//		int dig3 = ((int) (b * 100) / 10) % 10;
//		int dig4 = (int) (b * 100) % 10;

		int dig1, dig2, dig3, dig4;

//		dig1 = 1; dig2 = 2; dig3 = 3; dig4 = 4;

		dig1 = (int) (frequency / 1000.0) % 10;
		dig2 = (int)  (frequency / 100.0) % 10;
		dig3 = (int)  (frequency / 10.0) % 10;
		dig4 = (int) frequency % 10;

		selectDigit(1); display(dig1);
		delay(5);
		selectDigit(2); display(dig2); displayPoint();
		delay(5);
		selectDigit(3); display(dig3);
		delay(5);
		selectDigit(4); display(dig4);
		delay(5);
	}
}

void TIM3_IRQHandler() {

	if(__HAL_TIM_GET_FLAG(&tim3, TIM_FLAG_UPDATE)) {
		__HAL_TIM_CLEAR_IT(&tim3, TIM_FLAG_UPDATE);

		TIM3->CNT = 0U; // сброс таймера

	}

}

void TIM4_IRQHandler() {

	if(__HAL_TIM_GET_FLAG(&tim4, TIM_FLAG_UPDATE)) {
		__HAL_TIM_CLEAR_IT(&tim4, TIM_FLAG_UPDATE);

		delay_counter++;

	}

}

void delay(int millis) {
	delay_counter = 0;
	while(delay_counter < millis);
}

void EXTI15_10_IRQHandler(void)
{
	if(__HAL_GPIO_EXTI_GET_IT(GPIO_PIN_12) != RESET) {
		__HAL_GPIO_EXTI_CLEAR_IT(GPIO_PIN_12);

		//если вышли за массив значит нужно начать сначала
		if(timeCurrentIndex == timeSamples) {
			timeCurrentIndex = 0;
		}

		float timeSum = 0;
		for(int i = 0; i < timeSamples; i++) timeSum += timeValues[i];
		averageTicks = timeSum / (float) timeSamples;

		timeValues[timeCurrentIndex++] = TIM3->CNT;
		TIM3->CNT = 0U;

		HAL_GPIO_TogglePin(GPIOC, GPIO_PIN_13);
	}
}

void resetDigitLEDBits() {
	HAL_GPIO_WritePin(GPIOA, GPIO_PIN_0, GPIO_PIN_SET);
	HAL_GPIO_WritePin(GPIOA, GPIO_PIN_1, GPIO_PIN_SET);
	HAL_GPIO_WritePin(GPIOA, GPIO_PIN_2, GPIO_PIN_SET);
	HAL_GPIO_WritePin(GPIOA, GPIO_PIN_3, GPIO_PIN_SET);
	HAL_GPIO_WritePin(GPIOA, GPIO_PIN_4, GPIO_PIN_SET);
	HAL_GPIO_WritePin(GPIOA, GPIO_PIN_5, GPIO_PIN_SET);
	HAL_GPIO_WritePin(GPIOA, GPIO_PIN_6, GPIO_PIN_SET);
	HAL_GPIO_WritePin(GPIOA, GPIO_PIN_7, GPIO_PIN_SET);
}

void resetDigitSelection() {
	HAL_GPIO_WritePin(GPIOA, GPIO_PIN_8, GPIO_PIN_SET);
	HAL_GPIO_WritePin(GPIOA, GPIO_PIN_9, GPIO_PIN_SET);
	HAL_GPIO_WritePin(GPIOA, GPIO_PIN_10, GPIO_PIN_SET);
	HAL_GPIO_WritePin(GPIOA, GPIO_PIN_11, GPIO_PIN_SET);
}

void selectDigit1() {
	resetDigitSelection();
	HAL_GPIO_WritePin(GPIOA, GPIO_PIN_8, GPIO_PIN_RESET);
	HAL_GPIO_WritePin(GPIOA, GPIO_PIN_9, GPIO_PIN_SET);
	HAL_GPIO_WritePin(GPIOA, GPIO_PIN_10, GPIO_PIN_SET);
	HAL_GPIO_WritePin(GPIOA, GPIO_PIN_11, GPIO_PIN_SET);
}

void selectDigit2() {
	resetDigitSelection();
	HAL_GPIO_WritePin(GPIOA, GPIO_PIN_8, GPIO_PIN_SET);
	HAL_GPIO_WritePin(GPIOA, GPIO_PIN_9, GPIO_PIN_RESET);
	HAL_GPIO_WritePin(GPIOA, GPIO_PIN_10, GPIO_PIN_SET);
	HAL_GPIO_WritePin(GPIOA, GPIO_PIN_11, GPIO_PIN_SET);
}

void selectDigit3() {
	resetDigitSelection();
	HAL_GPIO_WritePin(GPIOA, GPIO_PIN_8, GPIO_PIN_SET);
	HAL_GPIO_WritePin(GPIOA, GPIO_PIN_9, GPIO_PIN_SET);
	HAL_GPIO_WritePin(GPIOA, GPIO_PIN_10, GPIO_PIN_RESET);
	HAL_GPIO_WritePin(GPIOA, GPIO_PIN_11, GPIO_PIN_SET);
}

void selectDigit4() {
	resetDigitSelection();
	HAL_GPIO_WritePin(GPIOA, GPIO_PIN_8, GPIO_PIN_SET);
	HAL_GPIO_WritePin(GPIOA, GPIO_PIN_9, GPIO_PIN_SET);
	HAL_GPIO_WritePin(GPIOA, GPIO_PIN_10, GPIO_PIN_SET);
	HAL_GPIO_WritePin(GPIOA, GPIO_PIN_11, GPIO_PIN_RESET);
}

void selectDigit(int digitNum) {
	switch(digitNum) {
		case 1: selectDigit1(); break;
		case 2: selectDigit2(); break;
		case 3: selectDigit3(); break;
		case 4: selectDigit4(); break;
	}
}

void display0() {
	resetDigitLEDBits();
	HAL_GPIO_WritePin(GPIOA, GPIO_PIN_0, GPIO_PIN_RESET);
	HAL_GPIO_WritePin(GPIOA, GPIO_PIN_1, GPIO_PIN_RESET);
	HAL_GPIO_WritePin(GPIOA, GPIO_PIN_2, GPIO_PIN_RESET);
	HAL_GPIO_WritePin(GPIOA, GPIO_PIN_3, GPIO_PIN_RESET);
	HAL_GPIO_WritePin(GPIOA, GPIO_PIN_4, GPIO_PIN_RESET);
	HAL_GPIO_WritePin(GPIOA, GPIO_PIN_5, GPIO_PIN_RESET);
}

void display1() {
	resetDigitLEDBits();
	HAL_GPIO_WritePin(GPIOA, GPIO_PIN_1, GPIO_PIN_RESET);
	HAL_GPIO_WritePin(GPIOA, GPIO_PIN_2, GPIO_PIN_RESET);
}

void display2() {
	resetDigitLEDBits();
	HAL_GPIO_WritePin(GPIOA, GPIO_PIN_0, GPIO_PIN_RESET);
	HAL_GPIO_WritePin(GPIOA, GPIO_PIN_1, GPIO_PIN_RESET);
	HAL_GPIO_WritePin(GPIOA, GPIO_PIN_6, GPIO_PIN_RESET);
	HAL_GPIO_WritePin(GPIOA, GPIO_PIN_4, GPIO_PIN_RESET);
	HAL_GPIO_WritePin(GPIOA, GPIO_PIN_3, GPIO_PIN_RESET);
}


void display3() {
	resetDigitLEDBits();
	HAL_GPIO_WritePin(GPIOA, GPIO_PIN_0, GPIO_PIN_RESET);
	HAL_GPIO_WritePin(GPIOA, GPIO_PIN_1, GPIO_PIN_RESET);
	HAL_GPIO_WritePin(GPIOA, GPIO_PIN_2, GPIO_PIN_RESET);
	HAL_GPIO_WritePin(GPIOA, GPIO_PIN_3, GPIO_PIN_RESET);
	HAL_GPIO_WritePin(GPIOA, GPIO_PIN_6, GPIO_PIN_RESET);
}

void display4() {
	resetDigitLEDBits();
	HAL_GPIO_WritePin(GPIOA, GPIO_PIN_1, GPIO_PIN_RESET);
	HAL_GPIO_WritePin(GPIOA, GPIO_PIN_2, GPIO_PIN_RESET);
	HAL_GPIO_WritePin(GPIOA, GPIO_PIN_6, GPIO_PIN_RESET);
	HAL_GPIO_WritePin(GPIOA, GPIO_PIN_5, GPIO_PIN_RESET);
}

void display5() {
	resetDigitLEDBits();
	HAL_GPIO_WritePin(GPIOA, GPIO_PIN_0, GPIO_PIN_RESET);
	HAL_GPIO_WritePin(GPIOA, GPIO_PIN_5, GPIO_PIN_RESET);
	HAL_GPIO_WritePin(GPIOA, GPIO_PIN_6, GPIO_PIN_RESET);
	HAL_GPIO_WritePin(GPIOA, GPIO_PIN_2, GPIO_PIN_RESET);
	HAL_GPIO_WritePin(GPIOA, GPIO_PIN_3, GPIO_PIN_RESET);
}

void display6() {
	resetDigitLEDBits();
	HAL_GPIO_WritePin(GPIOA, GPIO_PIN_0, GPIO_PIN_RESET);
	HAL_GPIO_WritePin(GPIOA, GPIO_PIN_5, GPIO_PIN_RESET);
	HAL_GPIO_WritePin(GPIOA, GPIO_PIN_6, GPIO_PIN_RESET);
	HAL_GPIO_WritePin(GPIOA, GPIO_PIN_2, GPIO_PIN_RESET);
	HAL_GPIO_WritePin(GPIOA, GPIO_PIN_3, GPIO_PIN_RESET);
	HAL_GPIO_WritePin(GPIOA, GPIO_PIN_4, GPIO_PIN_RESET);
}

void display7() {
	resetDigitLEDBits();
	HAL_GPIO_WritePin(GPIOA, GPIO_PIN_0, GPIO_PIN_RESET);
	HAL_GPIO_WritePin(GPIOA, GPIO_PIN_1, GPIO_PIN_RESET);
	HAL_GPIO_WritePin(GPIOA, GPIO_PIN_2, GPIO_PIN_RESET);
}

void display8() {
	resetDigitLEDBits();
	HAL_GPIO_WritePin(GPIOA, GPIO_PIN_0, GPIO_PIN_RESET);
	HAL_GPIO_WritePin(GPIOA, GPIO_PIN_1, GPIO_PIN_RESET);
	HAL_GPIO_WritePin(GPIOA, GPIO_PIN_2, GPIO_PIN_RESET);
	HAL_GPIO_WritePin(GPIOA, GPIO_PIN_3, GPIO_PIN_RESET);
	HAL_GPIO_WritePin(GPIOA, GPIO_PIN_4, GPIO_PIN_RESET);
	HAL_GPIO_WritePin(GPIOA, GPIO_PIN_5, GPIO_PIN_RESET);
	HAL_GPIO_WritePin(GPIOA, GPIO_PIN_6, GPIO_PIN_RESET);
}

void display9() {
	resetDigitLEDBits();
	HAL_GPIO_WritePin(GPIOA, GPIO_PIN_0, GPIO_PIN_RESET);
	HAL_GPIO_WritePin(GPIOA, GPIO_PIN_1, GPIO_PIN_RESET);
	HAL_GPIO_WritePin(GPIOA, GPIO_PIN_2, GPIO_PIN_RESET);
	HAL_GPIO_WritePin(GPIOA, GPIO_PIN_3, GPIO_PIN_RESET);
	HAL_GPIO_WritePin(GPIOA, GPIO_PIN_5, GPIO_PIN_RESET);
	HAL_GPIO_WritePin(GPIOA, GPIO_PIN_6, GPIO_PIN_RESET);
}

void displayPoint() {
	HAL_GPIO_WritePin(GPIOA, GPIO_PIN_7, GPIO_PIN_RESET);
}

void display(int num) {
	switch(num) {
		case 0: display0(); break;
		case 1: display1(); break;
		case 2: display2(); break;
		case 3: display3(); break;
		case 4: display4(); break;
		case 5: display5(); break;
		case 6: display6(); break;
		case 7: display7(); break;
		case 8: display8(); break;
		case 9: display9(); break;
	}
}
