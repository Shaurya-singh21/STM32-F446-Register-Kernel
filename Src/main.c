#include "stm32f446xx.h"
#include "string.h"
#include "stdlib.h"
#include "stdio.h"
typedef enum {
	MODE_IDLE = 0,
	MODE_PWM = (1<<1),
	MODE_BLINK = (1<<2),
	MODE_BREATHE = (1<<3)
}sys_options;

typedef struct{
	uint16_t time_period;
	uint8_t duty_cycle;
} system_status;
system_status g_sys = {0};

void clock_config(void){
	RCC->AHB1ENR |= (1 << 0) | (1<<2) | (1<<1); // pa5 and pc13 clock and pa1 and pb0
	RCC->APB1ENR |= (1 << 0) | (1 << 17) | (1<<4); //tim2 clk and usart2 clk
	RCC->APB2ENR |= (1<<14); //syscfg clk
}
void GPIO_config(void){
	//pa5 for breathe
	GPIOA->MODER &= ~(3<<10);
	GPIOA->MODER |= (1 << 11);
	GPIOA->AFR[0] &= ~(15<<20);
	GPIOA->AFR[0] |= (1 << 20);
	//pa1 for pwm
	GPIOA->MODER &= ~(3<<2);
	GPIOA->MODER |= (1<<3);
	GPIOA->AFR[0] &= ~(15<<4);
	GPIOA->AFR[0] |= (1<<4);
	//pb0 for blink
	GPIOB->MODER &= ~(3<<0);
	GPIOB->MODER |= (1<<0);
	GPIOB->OTYPER &= ~(1<<0);
	//pc13
	GPIOC->MODER &= ~((1<<26) | (1<<27));
	SYSCFG->EXTICR[3] &= ~(0xF << 4);
	SYSCFG->EXTICR[3] |= (2<<4);
	EXTI->IMR |= (1 << 13);
	EXTI->FTSR |= (1<<13);
	NVIC->ISER[1] |= (1<<8);
}



//
void timer_config(void){
	//tim2 for pwm and breathe
	TIM2->PSC = 15;
	TIM2->ARR = 999;
	TIM2->EGR |= (1<<0);
	TIM2->SR &= ~(1<<0);

	TIM2->CCMR1 |= (3<<5) | (1<<3) | (1<<11) | (3<<13);
	TIM2->CCR2 = 0;
	TIM2->CCR1 = 0;

	TIM2->DIER |= (1<<0);
	NVIC->ISER[0] |= (1<<28);

	TIM2->CR1 |= (1<<7) | (1<<0);
	//tim6 for blink
	TIM6->PSC = 999;
	TIM6->ARR = 159;
	TIM6->EGR |= (1 << 0);
	TIM6->SR = 0;
	TIM6->CR1 |= (1<<7);
	TIM6->DIER |= (1<<0);
	NVIC->ISER[1] |= (1<<22);
}

void uart_config(void){
	//pa2 for trasnsmit and pa3 for receive
	GPIOA->MODER |= (2<<4) | (2<<6);
	GPIOA->AFR[0] |= (7<<8) | (7<<12);
	NVIC->ISER[1] |= (1 << 6);
	USART2->CR1 |= (1<<5) | (1<<13) | (1<<2) | (1<<3);
	USART2->BRR |= 0x008B;
}

char heading[] = "\r\n===== STM32 CLI Ready =====\r\nType HELP for commands\r\n>";
volatile uint8_t busy = 0;
char *text;
char buffer[64];
volatile uint8_t ptr = 0; //ptr of msg buffer
volatile uint8_t reflect = 0; //reflect back the word received thorugh uart
volatile uint8_t state = 0; //parsing or not
volatile uint8_t opt=0; //options available like pwm,blink etc (0 means off 1 means on)
volatile uint8_t switch_flag = 0; //flag for switching between  off and on
//breathing variables
volatile uint8_t breathe_tick = 0;
volatile uint8_t dir = 0;
volatile int16_t value_of_dc = 0;
void USART2_IRQHandler(void){
	if((USART2->SR & (1<<7)) && (USART2->CR1 & (1 << 7))){
		if(*text == '\0'){
			USART2->CR1 &= ~(1<<7);
			busy = 0;
		}else{
			USART2->DR = *text++;
		}
	}
	if(USART2->SR & (1<<5)){
		buffer[ptr] = USART2->DR;
		if(buffer[ptr] == '\r'){
			buffer[ptr] = '\0';
			state = 1;
		}
		++ptr;
		reflect = 1;
	}
}

void TIM6_DAC_IRQHandler(void){
	if(TIM6->SR & (1<<0)){
		TIM6->SR = 0;
		if(GPIOB->ODR & (1<<0)) GPIOB->BSRR = (1<<16);
		else GPIOB->BSRR = (1<<0);
	}
}

void TIM2_IRQHandler(void){
	if(TIM2->SR & (1<<0)){
		TIM2->SR &= ~(1<<0);
		if(opt & (1<<3)) {
			breathe_tick++;
			if(breathe_tick >=  10){
				if (dir == 0) { // Fading IN
					value_of_dc += 20;
					if (value_of_dc >= 1000) {
						value_of_dc = 1000;
						dir = 1; // Switch to Fade OUT
					}
					} else { // Fading OUT
						value_of_dc -= 20;
						if (value_of_dc <= 0) {
							value_of_dc = 0;
							dir = 0; // Switch to Fade IN
						}
					}
				TIM2->CCR1 = value_of_dc;
				breathe_tick = 0;
			}
		}
	}
}
void EXTI15_10_IRQHandler(void){
	if(EXTI->PR & (1<<13)){
		EXTI->PR |= (1<<13);
		state = 1;
		opt |= (1<<7);
	}
}
void send(char *msg){
	if(busy) return;
	busy = 1;
	text = (char*)msg;
	USART2->CR1 |= (1<<7);
}


char menu[] = "\r\nCommands:-\r\n"
		      "SET PWM <1 until 100>        - Set LED brightness %\r\n"
			  "SET BLINK <ms>               - Blink LED with period(10-2000) in ms\r\n"
			  "SET BREATHE ON				- Breathing effect on\r\n"
			  "STATUS                       - Show system status\r\n"
			  "SET (PWM/BREATHE/BLINK) OFF  - Turn respective mode off\r\n"
			  "PRESS THE BLUE BUTTON 	    - Set to idle mode\r\n>";

char error[] ="\r\nType Correct Command\r\n>";

void help(void){
	send((char*)menu);
}

void set_pwm(long lvl){
	if(switch_flag){
		if(!(opt & (1<<1))) {
			send((char*)"\r\nAlready Off\r\n>");
		}else{
			opt &= ~(1<<1);
			TIM2->CCER &= ~(1<<4);
			lvl = 0;
			g_sys.duty_cycle = 0;
			send((char*)"\r\nTurned Off! :)\r\n>");
		}
		switch_flag = 0;
		return;
	}
	if(lvl > 0 && lvl <=100){
		//pwm
		opt |= (1<<1);
		TIM2->CCR2 = lvl*((TIM2->ARR) + 1)/100;
		if(!(TIM2->CCER & (1<<4))) TIM2->CCER |= (1<<4);
		g_sys.duty_cycle = lvl;
		send((char*)"\r\nDONE! :)\r\n>");
	}
	else send((char*)"\r\nEnter correct duty cycle value\r\n>");
}

void set_blink(long tp){
	if(switch_flag){
		if(!(opt & (1<<2))){
			send((char*)"\r\nAlready Off\r\n>");
		}else{
			TIM6->CR1 &= ~(1<<0);
			GPIOB->BSRR |= (1<<16);
			opt &= ~(1<<2);
			tp = 0;
			g_sys.time_period = 0;
			send((char*)"\r\nTurned Off! :)\r\n>");
		}
		switch_flag = 0;
		return;
	}
	if(tp>=10 && tp<=2000){
		opt |= (1<<2);
		TIM6->ARR = (tp*16) - 1;
		if(!(TIM6->CR1 & (1<<0))) TIM6->CR1 |= (1<<0);
		g_sys.time_period=tp;
		send((char*)"\r\nDONE! :)\r\n>");
	}
	else send((char*)"\r\nEnter correct time period value\r\n>");
}

void set_breathe(void){
	if(switch_flag){
		if(!(opt & (1<<3))) send((char*)"\r\nAlready Off\r\n>");
		else{
			TIM2->CCER &= ~(1<<0);
			opt &= ~(1<<3);
		}
		switch_flag = 0;
		send((char*)"\r\nTurned Off! :)\r\n>");
		return;
	}
	opt |= (1<<3);
	if(!(TIM2->CCER & (1<<0))) TIM2->CCER |= (1<<0);
	send((char*)"\r\nDONE! :)\r\n>");
}

void set_status(void){
	char send_buf[128];
	char breathe[8];
	(opt & (1<<3)) ? snprintf(breathe,sizeof(breathe),"ON") : snprintf(breathe,sizeof(breathe),"OFF");
	snprintf(send_buf,sizeof(send_buf),
			"\r\n"
			"TIME-PERIOD: %u\r\n"
			"DUTY-CYCLE:  %u\r\n"
			"BREATHING:   %s\r\n> ",g_sys.time_period,g_sys.duty_cycle,breathe);
	send((char*)send_buf);

}

void set_idle(void){
	TIM6->CR1 &= ~(1<<0);
	TIM2->CCER &= ~((1<<0) | (1<<4));
	GPIOA->BSRR |= (1<<21) | (1<<20);
	GPIOB->BSRR |= (1<<16);
	g_sys.duty_cycle = 0;
	g_sys.time_period = 0;
	opt = 0;
	send((char*)"IDLE MODE ON\r\n>");
}

void parse_cmd(void){
	if(!strcmp((char*)buffer, "HELP"))help();
	else if(!strncmp((char*)buffer,"SET PWM ",8)){
		long dc;
		if(!strncmp(&buffer[8],"OFF",3) && buffer[8+3] == '\0') switch_flag = 1;
		else{
			char *end = NULL;
			dc = strtol((char*)&buffer[8],&end,10);
			if(*end != '\0' ) send((char*)"\r\nInvalid Value\r\n>");
		}
		set_pwm(dc);
	}
	else if(!strncmp((char*)buffer,"SET BLINK ",10)){
		long tp;
		if(!strncmp(&buffer[10],"OFF",3) && buffer[10+3] == '\0') switch_flag = 1;
		else{
			char *end = NULL;
			tp = strtol((char*)&buffer[10],&end,10);
			if(*end != '\0' ) send((char*)"\r\nInvalid Value\r\n>");
		}
		set_blink(tp);
	}
	else if(!strncmp((char*)buffer,"SET BREATHE",11)){
		if(!strncmp(&buffer[11]," OFF",4) && buffer[11+4] == '\0') switch_flag = 1;
		else if(strncmp(&buffer[11], " ON", 3) && buffer[11+3] == '\0') {
			send((char*)error);
			return;
		}
		set_breathe();
	}else if(!strncmp((char*)buffer,"STATUS",6) && ptr == 7)set_status();
	else send((char*)error);
}




int main(void){
	clock_config();
	GPIO_config();
	uart_config();
	timer_config();
	send(heading);
	for(;;){
		if(reflect){
					send(&buffer[ptr-1]);
					reflect = 0;
				}
				if(state){
					if(opt & (1<<7)) set_idle();
					else{
						parse_cmd();
						memset((void*)buffer,0,sizeof(buffer));
						ptr = 0;
					}
					state = 0;
				}
	}
}
