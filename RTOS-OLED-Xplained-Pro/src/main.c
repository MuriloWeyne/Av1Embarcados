#include <asf.h>
#include "conf_board.h"

#include "gfx_mono_ug_2832hsweg04.h"
#include "gfx_mono_text.h"
#include "sysfont.h"

/* Botao da placa */
#define BUT_PIO PIOA
#define BUT_PIO_ID ID_PIOA
#define BUT_PIO_PIN 11
#define BUT_PIO_PIN_MASK (1 << BUT_PIO_PIN)

#define CLK_PIO PIOD
#define CLK_PIO_ID ID_PIOD
#define CLK_PIO_IDX 30
#define CLK_PIO_IDX_MASK (1 << CLK_PIO_IDX)

#define DT_PIO PIOC
#define DT_PIO_ID ID_PIOC
#define DT_PIO_IDX 13
#define DT_PIO_IDX_MASK (1 << DT_PIO_IDX)

#define SW_PIO PIOA
#define SW_PIO_ID ID_PIOA
#define SW_PIO_IDX 6
#define SW_PIO_IDX_MASK (1 << SW_PIO_IDX)

#define LED_PIO PIOC
#define LED_PIO_ID ID_PIOC
#define LED_IDX 8u
#define LED_IDX_MASK (1u << LED_IDX)

/** RTOS  */

#define TASK_SENTIDO_STACK_SIZE (1024 * 6 / sizeof(portSTACK_TYPE))
#define TASK_SENTIDO_STACK_PRIORITY (tskIDLE_PRIORITY)

#define TASK_INCREMENTA_STACK_SIZE (1024 * 8 / sizeof(portSTACK_TYPE))
#define TASK_INCREMENTA_STACK_PRIORITY (tskIDLE_PRIORITY)

#define TASK_ZERO_STACK_SIZE (1024 * 4 / sizeof(portSTACK_TYPE))
#define TASK_ZERO_STACK_PRIORITY (tskIDLE_PRIORITY)

extern void vApplicationStackOverflowHook(xTaskHandle *pxTask, signed char *pcTaskName);
extern void vApplicationIdleHook(void);
extern void vApplicationTickHook(void);
extern void vApplicationMallocFailedHook(void);
extern void xPortSysTickHandler(void);

volatile char flag_rtt = 0;
volatile char flag_clk = 0;
volatile char flag_sw = 0;
volatile char is_first = 0;
volatile char flag_dt = 0;

int contagens = 0;

/** prototypes */
void but_callback(void);
void clk_callback(void);
void dt_callback(void);
void sw_callback(void);
static void BUT_init(void);

/* Structs*/

typedef struct
{
	uint incremento;
} sw_dados;

/* Queues */

QueueHandle_t xQueueIncremento;
QueueHandle_t xQueueZera;

SemaphoreHandle_t xSemaphore;

/************************************************************************/
/* RTOS application funcs                                               */
/************************************************************************/

extern void vApplicationStackOverflowHook(xTaskHandle *pxTask, signed char *pcTaskName)
{
	printf("stack overflow %x %s\r\n", pxTask, (portCHAR *)pcTaskName);
	for (;;)
	{
	}
}

extern void vApplicationIdleHook(void) {}

extern void vApplicationTickHook(void) {}

extern void vApplicationMallocFailedHook(void)
{
	configASSERT((volatile void *)NULL);
}

static void RTT_init(float freqPrescale, uint32_t IrqNPulses, uint32_t rttIRQSource);

/************************************************************************/
/* handlers / callbacks                                                 */
/************************************************************************/

void but_callback(void)
{
}

void clk_callback(void)
{
	flag_clk = 1;
	if (flag_dt == 0)
	{
		is_first = 1;
	}
}

void dt_callback(void)
{
	flag_dt = 1;
}

void sw_callback(void)
{
	// Inicia o timer ao pressionar o botao SW
	if(!pio_get(SW_PIO, PIO_INPUT, SW_PIO_IDX_MASK))
	{
		RTT_init(1,0,0);
		sw_dados sw;
		sw.incremento = 1;
		BaseType_t xHigherPriorityTaskWoken = pdTRUE;
		xQueueSendFromISR(xQueueIncremento, &sw, &xHigherPriorityTaskWoken);
	}
	else {
		contagens = rtt_read_timer_value(RTT);
		flag_sw = 1;
	}

}

static void RTT_init(float freqPrescale, uint32_t IrqNPulses, uint32_t rttIRQSource)
{

	uint16_t pllPreScale = (int)(((float)32768) / freqPrescale);

	rtt_sel_source(RTT, false);
	rtt_init(RTT, pllPreScale);

	if (rttIRQSource & RTT_MR_ALMIEN)
	{
		uint32_t ul_previous_time;
		ul_previous_time = rtt_read_timer_value(RTT);
		while (ul_previous_time == rtt_read_timer_value(RTT))
			;
		rtt_write_alarm_time(RTT, IrqNPulses + ul_previous_time);
	}

	/* config NVIC */
	NVIC_DisableIRQ(RTT_IRQn);
	NVIC_ClearPendingIRQ(RTT_IRQn);
	NVIC_SetPriority(RTT_IRQn, 4);
	NVIC_EnableIRQ(RTT_IRQn);

	/* Enable RTT interrupt */
	if (rttIRQSource & (RTT_MR_RTTINCIEN | RTT_MR_ALMIEN))
		rtt_enable_interrupt(RTT, rttIRQSource);
	else
		rtt_disable_interrupt(RTT, RTT_MR_RTTINCIEN | RTT_MR_ALMIEN);
}

// Handler do RTT

void RTT_Handler(void)
{
	uint32_t ul_status;
	ul_status = rtt_get_status(RTT);

	/* IRQ due to Alarm */
	if ((ul_status & RTT_SR_ALMS) == RTT_SR_ALMS)
	{
		flag_rtt = 1;
	}
}

/************************************************************************/
/* TASKS                                                                */
/************************************************************************/

static void task_incrementa(void *pvParameters)
{
	gfx_mono_draw_filled_rect(0, 0, 128, 32, GFX_PIXEL_CLR);
	char buffer[128];
	unsigned char char1 = 0;
	unsigned char char2 = 0;
	unsigned char char3 = 0;
	unsigned char char4 = 0;
	sw_dados sw;
	volatile int selected_char = 0;
	for (;;)
	{	
		if (selected_char == 0)
		{
			sprintf(buffer, "%X %X %X %X", char1, char2, char3, char4);
			gfx_mono_draw_string(buffer, 0, 0, &sysfont);
			gfx_mono_draw_filled_rect(0, 0, 5, 32, GFX_PIXEL_CLR);
			vTaskDelay(100 / portTICK_PERIOD_MS);
			gfx_mono_draw_string(buffer, 0, 0, &sysfont);
		}
		else if (selected_char == 1)
		{
			sprintf(buffer, "%X %X %X %X", char1, char2, char3, char4);
			gfx_mono_draw_string(buffer, 0, 0, &sysfont);
			gfx_mono_draw_filled_rect(12, 0, 5, 32, GFX_PIXEL_CLR);
			vTaskDelay(100 / portTICK_PERIOD_MS);
			gfx_mono_draw_string(buffer, 0, 0, &sysfont);
		}
		else if (selected_char == 2)
		{
			sprintf(buffer, "%X %X %X %X", char1, char2, char3, char4);
			gfx_mono_draw_string(buffer, 0, 0, &sysfont);
			gfx_mono_draw_filled_rect(24, 0, 5, 32, GFX_PIXEL_CLR);
			vTaskDelay(100 / portTICK_PERIOD_MS);
			gfx_mono_draw_string(buffer, 0, 0, &sysfont);
		}
		else if (selected_char == 3)
		{
			sprintf(buffer, "%X %X %X %X", char1, char2, char3, char4);
			gfx_mono_draw_string(buffer, 0, 0, &sysfont);
			gfx_mono_draw_filled_rect(36, 0, 5, 32, GFX_PIXEL_CLR);
			vTaskDelay(100 / portTICK_PERIOD_MS);
			gfx_mono_draw_string(buffer, 0, 0, &sysfont);
		}
		if (selected_char > 3)
		{
			selected_char = 0;
		}
		if (flag_sw) {
			flag_sw = 0;
			if (contagens >= 5) {
				char1 = 0;
				char2 = 0;
				char3 = 0;
				char4 = 0;
				for(int i = 0; i < 10; i++) {
					pio_clear(LED_PIO, LED_IDX_MASK);
					vTaskDelay(100 / portTICK_PERIOD_MS);
					pio_set(LED_PIO, LED_IDX_MASK);
					vTaskDelay(100 / portTICK_PERIOD_MS);
				}	
			}
		}
		if (xQueueReceive(xQueueIncremento, &sw, 1000))
		{
			selected_char += sw.incremento;
		}
		if (flag_clk && is_first)
		{
			flag_clk = 0;
			flag_dt = 0;
			is_first = 0;
			if (selected_char == 0)
			{
				char1++;
				if (char1 > 15)
				{
					char1 = 0;
				}
			}
			else if (selected_char == 1)
			{
				char2++;
				if (char2 > 15)
				{
					char2 = 0;
				}
			}
			else if (selected_char == 2)
			{
				char3++;
				if (char3 > 15)
				{
					char3 = 0;
				}
			}
			else if (selected_char == 3)
			{
				char4++;
				if (char4 > 15)
				{
					char4 = 0;
				}
			}
		}
		if (flag_dt && !is_first)
		{
			flag_dt = 0;
			flag_clk = 0;
			if (selected_char == 0)
			{
				char1--;
				if (char1 > 15)
				{
					char1 = 15;
				}
			}
			else if (selected_char == 1)
			{
				char2--;
				if (char2 > 15)
				{
					char2 = 15;
				}
			}
			else if (selected_char == 2)
			{
				char3--;
				if (char3 > 15)
				{
					char3 = 15;
				}
			}
			else if (selected_char == 3)
			{
				char4--;
				if (char4 > 15)
				{
					char4 = 15;
				}
			}
		}
	}
}

/************************************************************************/
/* funcoes                                                              */
/************************************************************************/

static void configure_console(void)
{
	const usart_serial_options_t uart_serial_options = {
		.baudrate = CONF_UART_BAUDRATE,
		.charlength = CONF_UART_CHAR_LENGTH,
		.paritytype = CONF_UART_PARITY,
		.stopbits = CONF_UART_STOP_BITS,
	};

	/* Configure console UART. */
	stdio_serial_init(CONF_UART, &uart_serial_options);

	/* Specify that stdout should not be buffered. */
	setbuf(stdout, NULL);
}

static void BUT_init(void)
{

	WDT->WDT_MR = WDT_MR_WDDIS;

	gfx_mono_ssd1306_init();

	NVIC_EnableIRQ(BUT_PIO_ID);
	NVIC_SetPriority(BUT_PIO_ID, 4);

	NVIC_EnableIRQ(DT_PIO_ID);
	NVIC_SetPriority(DT_PIO_ID, 4);

	NVIC_EnableIRQ(CLK_PIO_ID);
	NVIC_SetPriority(CLK_PIO_ID, 4);

	NVIC_EnableIRQ(SW_PIO_ID);
	NVIC_SetPriority(SW_PIO_ID, 4);

	pio_configure(DT_PIO, PIO_INPUT, DT_PIO_IDX_MASK, PIO_PULLUP | PIO_DEBOUNCE);
	pio_set_debounce_filter(DT_PIO, DT_PIO_IDX_MASK, 60);
	pio_enable_interrupt(DT_PIO, DT_PIO_IDX_MASK);
	pio_handler_set(DT_PIO, DT_PIO_ID, DT_PIO_IDX_MASK, PIO_IT_FALL_EDGE, dt_callback);

	pio_configure(CLK_PIO, PIO_INPUT, CLK_PIO_IDX_MASK, PIO_PULLUP | PIO_DEBOUNCE);
	pio_set_debounce_filter(CLK_PIO, CLK_PIO_IDX_MASK, 60);
	pio_enable_interrupt(CLK_PIO, CLK_PIO_IDX_MASK);
	pio_handler_set(CLK_PIO, CLK_PIO_ID, CLK_PIO_IDX_MASK, PIO_IT_FALL_EDGE, clk_callback);

	pio_configure(SW_PIO, PIO_INPUT, SW_PIO_IDX_MASK, PIO_PULLUP | PIO_DEBOUNCE);
	pio_set_debounce_filter(SW_PIO, SW_PIO_IDX_MASK, 60);
	pio_enable_interrupt(SW_PIO, SW_PIO_IDX_MASK);
	pio_handler_set(SW_PIO, SW_PIO_ID, SW_PIO_IDX_MASK, PIO_IT_EDGE, sw_callback);
}

/************************************************************************/
/* main                                                                 */
/************************************************************************/

int main(void)
{
	/* Initialize the SAM system */
	sysclk_init();
	board_init();
	BUT_init();

	/* Initialize the console uart */
	configure_console();

	/* Create task to control oled */
	if (xTaskCreate(task_incrementa, "incrementa", TASK_INCREMENTA_STACK_SIZE, NULL, TASK_INCREMENTA_STACK_PRIORITY, NULL) != pdPASS)
	{
		printf("Failed to create incrementa task\r\n");
	}
	// if (xTaskCreate(task_zero, "zero", TASK_ZERO_STACK_SIZE, NULL, TASK_ZERO_STACK_PRIORITY, NULL) != pdPASS) {
	//   printf("Failed to create zero task\r\n");
	// }
	xQueueIncremento = xQueueCreate(100, sizeof(sw_dados));
	if (xQueueIncremento == NULL)
	{
		printf("Falha ao criar fila de incremento\r\n");
	}

	xSemaphore = xSemaphoreCreateBinary();

	/* Start the scheduler. */
	vTaskStartScheduler();

	/* RTOS não deve chegar aqui !! */
	while (1)
	{
	}

	/* Will only get here if there was insufficient memory to create the idle task. */
	return 0;
}
