/*
 * CH32V003J4M6 (SOP-8) drives 5x6 LED matrix
 *
 * Reference:
 *  - https://github.com/cnlohr/ch32v003fun/tree/master/examples/systick_irq
 *  - http://www.technoblogy.com/show?2H0K
 *  - https://en.wikipedia.org/wiki/Charlieplexing
 *
 * Aug 2023 by Li Mingjie
 *  - Email:  limingjie@outlook.com
 *  - GitHub: https://github.com/limingjie/
 */

#include "ch32v003fun.h"
#include "ch32v003_GPIO_branchless.h"

// Bit definitions for systick regs
#define SYSTICK_SR_CNTIF   (1 << 0)
#define SYSTICK_CTLR_STE   (1 << 0)
#define SYSTICK_CTLR_STIE  (1 << 1)
#define SYSTICK_CTLR_STCLK (1 << 2)
#define SYSTICK_CTLR_STRE  (1 << 3)
#define SYSTICK_CTLR_SWIE  (1 << 31)

#define LED_MATRIX_NUM_PINS 4
#define LED_MATRIX_SIZE     LED_MATRIX_NUM_PINS *(LED_MATRIX_NUM_PINS - 1)
#define LED_PWM_CYCLES      16

uint8_t pins[LED_MATRIX_NUM_PINS] = {
    GPIOv_from_PORT_PIN(GPIO_port_C, 1),
    GPIOv_from_PORT_PIN(GPIO_port_C, 2),
    GPIOv_from_PORT_PIN(GPIO_port_C, 4),
    GPIOv_from_PORT_PIN(GPIO_port_A, 1),
};

// PWM duty cycles of LEDs
uint8_t led_duty_cycles[LED_MATRIX_SIZE] = {0, 4, 8, 16, 0, 4, 8, 16, 0, 4, 8, 16};

static inline void led_matrix_run();

// Start up the SysTick IRQ
void systick_init(void)
{
    // Disable default SysTick behavior
    SysTick->CTLR = 0;

    // Enable the SysTick IRQ
    NVIC_EnableIRQ(SysTicK_IRQn);

    // Set the tick interval
    SysTick->CMP = (FUNCONF_SYSTEM_CORE_CLOCK / 50000) - 1;

    // Start at zero
    SysTick->CNT = 0;

    // Enable SysTick counter, IRQ, HCLK/1
    SysTick->CTLR = SYSTICK_CTLR_STE | SYSTICK_CTLR_STIE | SYSTICK_CTLR_STCLK;
}

// SysTick ISR just counts ticks
__attribute__((interrupt)) void SysTick_Handler(void)
{
    // Move the compare further ahead in time. as a warning, if more than this length of time
    // passes before triggering, you may miss your interrupt.
    SysTick->CMP += (FUNCONF_SYSTEM_CORE_CLOCK / 50000);

    // Clear IRQ
    SysTick->SR = 0;

    led_matrix_run();
}

static inline void led_matrix_init()
{
    GPIO_port_enable(GPIO_port_A);
    GPIO_port_enable(GPIO_port_C);

    for (uint8_t i = 0; i < LED_MATRIX_NUM_PINS; i++)
    {
        GPIO_pinMode(pins[i], GPIO_pinMode_I_floating, GPIO_Speed_10MHz);
    }
}

static inline void led_matrix_run()
{
    static uint8_t  cycle = 0, row = 0, column = 0;
    static uint8_t *led = led_duty_cycles;

    // Turn off the LED by put column pin in high impedance mode.
    // Put it at the beginning to avoid blink on cycle 0, although not noticeable.
    if (cycle == *led)
    {
        GPIO_pinMode(pins[column], GPIO_pinMode_I_floating, GPIO_Speed_10MHz);
    }
    else if (cycle == 0)  // Initialization or moved to the next LED.
    {
        // Pull down the row pin and pull up the column pin to turn on the LED.
        GPIO_pinMode(pins[row], GPIO_pinMode_O_pushPull, GPIO_Speed_10MHz);
        GPIO_digitalWrite(pins[row], low);
        GPIO_pinMode(pins[column], GPIO_pinMode_O_pushPull, GPIO_Speed_10MHz);
        GPIO_digitalWrite(pins[column], high);
    }

    // Move to the next LED after LED_PWM_CYCLES
    if (++cycle == LED_PWM_CYCLES)
    {
        cycle = 0;
        led++;

        // The column and row cannot be the same pin.
        if (++column == row)
        {
            column++;
        }

        // Move to the next row.
        if (column == LED_MATRIX_NUM_PINS)
        {
            column = 0;

            // Put the current row pin in high impedance mode.
            GPIO_pinMode(pins[row], GPIO_pinMode_I_floating, GPIO_Speed_10MHz);
            if (++row == LED_MATRIX_NUM_PINS)
            {
                row    = 0;
                column = 1;
                led    = led_duty_cycles;
            }
        }
    }
}

int main()
{
    SystemInit();
    Delay_Ms(100);

    // Init LED matrix
    led_matrix_init();

    // Init systick
    systick_init();

    while (1)
    {
        // Shuffle the LED duty cycles
        uint8_t t = led_duty_cycles[0];
        for (uint8_t i = 0; i < LED_MATRIX_SIZE - 1; i++)
        {
            led_duty_cycles[i] = led_duty_cycles[i + 1];
        }
        led_duty_cycles[LED_MATRIX_SIZE - 1] = t;

        Delay_Ms(100);
    }
}
