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

#include <string.h>  // memcpy

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

// 5x6 pixel font
static uint32_t font[] = {
    0b100011000110001111111000101110,  // A
    0b011111000110001011111000101111,  // B
    0b011101000100001000011000101110,  // C
    0b011111000110001100011000101111,  // D
    0b111110000100001011110000111111,  // E
    0b000010000100001011110000111111,  // F
    0b011101000110001111010000101110,  // G
    0b100011000110001111111000110001,  // H
    0b011100010000100001000010001110,  // I
    0b001100100101000010000100011100   // J
};

// Effects
static uint8_t effects[][LED_MATRIX_SIZE] = {
    {0, 4, 8, 16, 0, 4, 8, 16, 0, 4, 8, 16},   // Diagonal wave
    {0, 3, 6, 9, 12, 15, 0, 0, 0, 0, 0, 0},    // Snake
    {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 16},     // Dot
    {0, 0, 16, 0, 0, 16, 0, 0, 16, 0, 0, 16},  // Line
};

// PWM duty cycles of LEDs
uint8_t led_duty_cycles[LED_MATRIX_SIZE];

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

        // Put the current column pin in high impedance mode.
        GPIO_pinMode(pins[column], GPIO_pinMode_I_floating, GPIO_Speed_10MHz);

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

            // Reset to the first LED.
            if (++row == LED_MATRIX_NUM_PINS)
            {
                row    = 0;
                column = 1;
                led    = led_duty_cycles;
            }
        }
    }
}

void led_putchar(uint8_t c)
{
    uint32_t ch  = font[c];
    uint8_t *led = led_duty_cycles;
    for (uint8_t line = 0; line < LED_MATRIX_NUM_PINS; line++)
    {
        for (uint8_t pixel = 0; pixel < LED_MATRIX_NUM_PINS - 1; pixel++)
        {
            *led++ = (ch & (1 << (line * 5 + pixel))) ? 32 : 0;
        }
    }
}

static inline void set_effect(uint8_t i)
{
    memcpy(led_duty_cycles, effects[i], LED_MATRIX_SIZE * sizeof(uint8_t));
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
        // Run snake
        for (uint8_t e = 0; e < sizeof(effects) / sizeof(effects[0]); e++)
        {
            set_effect(e);
            for (uint8_t loop = 0; loop < 12; loop++)
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

        // Show all characters
        for (uint8_t c = 0; c < sizeof(font) / sizeof(font[0]); c++)
        {
            led_putchar(c);
            Delay_Ms(300);
        }
    }
}
