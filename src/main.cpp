#include "stm32f1xx.h"
#include "stm32f1xx_it.h"
#include "system_stm32f1xx.h"
#include "stm32f1xx_hal.h"
#include "stm32f1xx_hal_gpio.h"
#include "stm32f1xx_hal_rcc.h"
#include "stm32f1xx_hal_tim.h"
#include "stm32f1xx_hal_uart.h"
#include "string.h"
#include "stdlib.h"
#include "stdio.h"

#define timeSamples 10
#define speedValue 0
// размер верменного окна
#define windowSizeMillis 250

int delay_counter = 0;

void clocksInit();
void delay(int);

TIM_HandleTypeDef tim4;
TIM_HandleTypeDef tim2;
UART_HandleTypeDef huart;

uint32_t timeValues[timeSamples] = {0};
uint32_t timeCurrentIndex = 0;
uint32_t millis = 0; // 1 тик - миллисекунда
uint32_t wmillis = 0; // счетчик временного окна

double averageMillis = 0;

int main(void) {

	HAL_Init();

	clocksInit();

	// индикация
	GPIO_InitTypeDef gpio;
	gpio.Mode = GPIO_MODE_OUTPUT_PP;
	gpio.Pin = GPIO_PIN_13;
	gpio.Speed = GPIO_SPEED_FREQ_HIGH;
	HAL_GPIO_Init(GPIOC, &gpio);

	// индикация
	gpio.Mode = GPIO_MODE_AF_PP;
	gpio.Pin = GPIO_PIN_9;
	gpio.Speed = GPIO_SPEED_FREQ_HIGH;
	HAL_GPIO_Init(GPIOA, &gpio);

	// катод ик фотодиода
	gpio.Pull = GPIO_PULLDOWN;
	gpio.Mode = GPIO_MODE_IT_FALLING;
	gpio.Pin = GPIO_PIN_12;
	gpio.Speed = GPIO_SPEED_FREQ_HIGH;
	HAL_GPIO_Init(GPIOA, &gpio);

	// таймер для передачи числа
	tim2.Init.ClockDivision = TIM_CLOCKDIVISION_DIV1;
	tim2.Init.CounterMode = TIM_COUNTERMODE_UP;
	tim2.Init.Period = 2000U - 1U;
	tim2.Init.Prescaler = 36000U;
	tim2.Instance = TIM2;
	HAL_TIM_Base_Init(&tim2);

	// таймер для delay и подсчета времени
	tim4.Init.ClockDivision = TIM_CLOCKDIVISION_DIV1;
	tim4.Init.CounterMode = TIM_COUNTERMODE_UP;
	tim4.Init.Period = 1000U -1U;
	tim4.Init.Prescaler = 36U;
	tim4.Instance = TIM4;
	HAL_TIM_Base_Init(&tim4);

	// общение с вайфай модулем
	huart.Init.BaudRate = 115200U;
	huart.Init.HwFlowCtl = UART_HWCONTROL_NONE;
	huart.Init.Mode = UART_MODE_TX;
	huart.Init.OverSampling = 16U;
	huart.Init.Parity = UART_PARITY_NONE;
	huart.Init.StopBits = UART_STOPBITS_1;
	huart.Init.WordLength = UART_WORDLENGTH_8B;
	huart.Instance = USART1;

	HAL_UART_Init(&huart);

	// выключаем прерывания
	__disable_irq();

	HAL_NVIC_SetPriority(EXTI15_10_IRQn, 1, 0);
	HAL_NVIC_EnableIRQ(EXTI15_10_IRQn);

	HAL_NVIC_SetPriority(TIM2_IRQn, 3, 0);
	HAL_NVIC_EnableIRQ(TIM2_IRQn);
	__HAL_TIM_ENABLE_IT(&tim2, TIM_IT_UPDATE);

	HAL_NVIC_SetPriority(TIM4_IRQn, 2, 0);
	HAL_NVIC_EnableIRQ(TIM4_IRQn);
	__HAL_TIM_ENABLE_IT(&tim4, TIM_IT_UPDATE);

	// включаем таймеры
	__HAL_TIM_ENABLE(&tim2);
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
//	RCC->APB1ENR |= RCC_APB1ENR_TIM3EN;
	RCC->APB2ENR |= RCC_APB2ENR_USART1EN;
	RCC->APB1ENR |= RCC_APB1ENR_TIM2EN;
	RCC->APB2ENR |= RCC_APB2ENR_IOPCEN;
	RCC->APB2ENR |= RCC_APB2ENR_IOPAEN;
}

extern "C" void TIM2_IRQHandler() {

	if(__HAL_TIM_GET_FLAG(&tim2, TIM_FLAG_UPDATE)) {
		__HAL_TIM_CLEAR_IT(&tim2, TIM_FLAG_UPDATE);

		double frequency;

		if(millis > 3000) {
			frequency = 0.0;
			for(int i = 0; i < timeSamples; i++) {
				timeValues[i] = 0;
			}
		}
		else {
			frequency = 60.0 / (averageMillis / 1000.0);
		}

		if(frequency > 9999.0 || frequency < 1.0) frequency = 0.0;

		char freq_str[32] = {0};

		sprintf(freq_str, "speed=%d\r\n", (int) frequency);

		HAL_UART_Transmit(&huart, (uint8_t*) freq_str, strlen(freq_str), 1000);

	}
}

extern "C" void TIM4_IRQHandler() {

	if(__HAL_TIM_GET_FLAG(&tim4, TIM_FLAG_UPDATE)) {
		__HAL_TIM_CLEAR_IT(&tim4, TIM_FLAG_UPDATE);
		delay_counter++;
		millis++; // подсчет миллисекунд
		wmillis++;
	}

}

void delay(int millis) {
	delay_counter = 0;
	while(delay_counter < millis);
}

extern "C"  void EXTI15_10_IRQHandler(void)
{
	if(__HAL_GPIO_EXTI_GET_IT(GPIO_PIN_12) != RESET) {
		__HAL_GPIO_EXTI_CLEAR_IT(GPIO_PIN_12);

		// соблюдаем временное окно
		if(wmillis > windowSizeMillis) {
			wmillis = 0;

			//если вышли за массив значит нужно начать сначала
			if(timeCurrentIndex == timeSamples) {
				timeCurrentIndex = 0;
			}

			double timeSum = 0;
			for(int i = 0; i < timeSamples; i++) timeSum += timeValues[i];
			averageMillis = timeSum / (double) timeSamples;

			timeValues[timeCurrentIndex++] = millis;
			millis = 0U;

			HAL_GPIO_TogglePin(GPIOC, GPIO_PIN_13);
		}

	}
}
