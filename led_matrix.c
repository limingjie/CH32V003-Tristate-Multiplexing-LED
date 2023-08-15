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

#define LED_MATRIX_NUM_PINS 6
#define LED_MATRIX_SIZE     (LED_MATRIX_NUM_PINS * (LED_MATRIX_NUM_PINS - 1))
#define LED_PWM_CYCLES      16

uint8_t pins[LED_MATRIX_NUM_PINS] = {
    GPIOv_from_PORT_PIN(GPIO_port_C, 1),  // IO1
    GPIOv_from_PORT_PIN(GPIO_port_C, 2),  // IO2
    GPIOv_from_PORT_PIN(GPIO_port_C, 4),  // IO3
    GPIOv_from_PORT_PIN(GPIO_port_D, 5),  // IO4
    GPIOv_from_PORT_PIN(GPIO_port_A, 1),  // IO5
    GPIOv_from_PORT_PIN(GPIO_port_A, 2),  // IO6
};

// 5x6 pixel font
static uint32_t font[] = {
    0b00001000111011111111111111101010,  // heart
    0b11111111111111111111111111111111,  // full square
    0b00111111000110001100011000111111,  // square
    0b00000000111001010010100111000000,  // smaller square
    0b00000000000000100001000000000000,  // micro square
    0b00000000000000000000000000000000,  // 0x20 space
    0b00001000000000100001000010000100,  // 0x21 '!'
    0b00000000000000000000000101001010,  // 0x22 '"'
    0b00010101111101010010101111101010,  // 0x23 '#'
    0b00001000111110100001011111000100,  // 0x24 '$'
    0b00100011001000100010011000100000,  // 0x25 '%'
    0b00101100100110101000100010100010,  // 0x26 '&'
    0b00000000000000000000000010000100,  // 0x27 '''
    0b00010000010000100001000010001000,  // 0x28 '('
    0b00000100010000100001000010000010,  // 0x29 ')'
    0b00001001010101110101010010000000,  // 0x2A '*'
    0b00001000010011111001000010000000,  // 0x2B '+'
    0b00000010011000110000000000000000,  // 0x2C ','
    0b00000000000011111000000000000000,  // 0x2D '-'
    0b00000000011000110000000000000000,  // 0x2E '.'
    0b00000010001000100010001000000000,  // 0x2F '/'
    0b00011101000110101101011000101110,  // 0x30 '0'
    0b00011100010000100001000011000100,  // 0x31 '1'
    0b00111110001000100010001000101110,  // 0x32 '2'
    0b00011101000110000010001000101110,  // 0x33 '3'
    0b00010000100011111010100110001000,  // 0x34 '4'
    0b00011111000010000011110000101111,  // 0x35 '5'
    0b00011101000110001011110001001100,  // 0x36 '6'
    0b00001000010000100010001000011111,  // 0x37 '7'
    0b00011101000110001011101000101110,  // 0x38 '8'
    0b00011001000011110100011000101110,  // 0x39 '9'
    0b00000000010000000000000010000000,  // 0x3A ':'
    0b00000100010000000000000010000000,  // 0x3B ';'
    0b00010000010000010001000100000000,  // 0x3C '<'
    0b00000001111100000111110000000000,  // 0x3D '='
    0b00000100010001000001000001000000,  // 0x3E '>'
    0b00001000000001100100001000101110,  // 0x3F '?'
    0b00000100110110101111011000101110,  // 0x40 '@'
    0b00100011000110001111111000101110,  // 0x41 'A'
    0b00011111000110001011111000101111,  // 0x42 'B'
    0b00011101000100001000011000101110,  // 0x43 'C'
    0b00011111000110001100011000101111,  // 0x44 'D'
    0b00111110000100001011110000111111,  // 0x45 'E'
    0b00000010000100001011110000111111,  // 0x46 'F'
    0b00011101000110001110010000101110,  // 0x47 'G'
    0b00100011000110001111111000110001,  // 0x48 'H'
    0b00011100010000100001000010001110,  // 0x49 'I'
    0b00001100100101000010000100011100,  // 0x4A 'J'
    0b00100010100100101000110010101001,  // 0x4B 'K'
    0b00111110000100001000010000100001,  // 0x4C 'L'
    0b00100011000110001101011101110001,  // 0x4D 'M'
    0b00100011000111001101011001110001,  // 0x4E 'N'
    0b00011101000110001100011000101110,  // 0x4F 'O'
    0b00000010000100001011111000101111,  // 0x50 'P'
    0b00101100100110001100011000101110,  // 0x51 'Q'
    0b00100010100100101011111000101111,  // 0x52 'R'
    0b00011111000010000011100000111110,  // 0x53 'S'
    0b00001000010000100001000010011111,  // 0x54 'T'
    0b00011101000110001100011000110001,  // 0x55 'U'
    0b00001000101010001100011000110001,  // 0x56 'V'
    0b00010101010110101100011000110001,  // 0x57 'W'
    0b00100011000101010001000101010001,  // 0x58 'X'
    0b00001000010000100010101000110001,  // 0x59 'Y'
    0b00111110000100010001000100011111,  // 0x5A 'Z'
    0b00011000010000100001000010001100,  // 0X5B '['
    0b00100000100000100000100000100000,  // 0X5C '\'
    0b00001100010000100001000010000110,  // 0X5D ']'
    0b00000000000010001010100010000000,  // 0X5E '^'
    0b00111110000000000000000000000000,  // 0X5F '_'
    0b00000000000000000000000010000010,  // 0X60 '`'
    0b00101100100101001011100000000000,  // 0x61 'a'
    0b00001110100101001001110000100001,  // 0x62 'b'
    0b00011100000100001011100000000000,  // 0x63 'c'
    0b00011100100101001011100100001000,  // 0x64 'd'
    0b00011100000101111100010111000000,  // 0x65 'e'
    0b00000100001000010001110001001100,  // 0x66 'f'
    0b00001110100001110010010111000000,  // 0x67 'g'
    0b00010010100101001001110000100001,  // 0x68 'b'
    0b00000100001000010000110000000010,  // 0x69 'i'
    0b00000110010000100001100000000100,  // 0x6A 'j'
    0b00010010010100011001010100100001,  // 0x6B 'k'
    0b00000100001000010000100001000010,  // 0x6C 'l'
    0b00101011010110101010110000000000,  // 0x6D 'm'
    0b00010010100101001001110000000000,  // 0x6E 'n'
    0b00001100100101001001100000000000,  // 0x6F 'o'
    0b00000010000100111010010011100000,  // 0x70 'p'
    0b00010000100001110010010111000000,  // 0x71 'q'
    0b00000100001010010011010000000000,  // 0x72 'r'
    0b00001110100000110000010111000000,  // 0x73 's'
    0b00011000001000010001110001000000,  // 0x74 't'
    0b00001100100101001010010000000000,  // 0x75 'u'
    0b00001000101010001100010000000000,  // 0x76 'v'
    0b00010101010110101100010000000000,  // 0x57 'w'
    0b00100010101000100010101000100000,  // 0x78 'x'
    0b00011101000011100100101001000000,  // 0x79 'y'
    0b00011110001000100011110000000000,  // 0x7A 'z'
    0b00110000010000110001001100000000,  // 0X7B '{'
    0b00001000010000000001000010000000,  // 0X7C '|'
    0b00000110010001100001000001100000,  // 0X7D '}'
    0b00000000100010101000100000000000,  // 0X7E '~'
};

// Effects
static uint8_t effects[][LED_MATRIX_SIZE] = {
    {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 16},       // Dot
    {15, 12, 9, 6, 3, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0},      // Snake
    {0, 0, 0, 0, 16, 0, 0, 0, 0, 16, 0, 0, 0, 0, 16, 0, 0, 0, 0, 16, 0, 0, 0, 0, 16, 0, 0, 0, 0, 16},  // Line
    {0, 4, 8, 12, 16, 0, 4, 8, 12, 16, 0, 4, 8, 12, 16,
     0, 4, 8, 12, 16, 0, 4, 8, 12, 16, 0, 4, 8, 12, 16},  // Diagonal wave
};

// PWM duty cycles of LEDs
uint8_t led_duty_cycles[LED_MATRIX_SIZE];

static inline void led_matrix_run();

#define BOARD 0

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
    GPIO_port_enable(GPIO_port_D);

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
    uint32_t ch  = font[c - 27];
    uint8_t *led = led_duty_cycles;
    for (uint8_t line = 0; line < LED_MATRIX_NUM_PINS; line++)
    {
        for (uint8_t pixel = 0; pixel < LED_MATRIX_NUM_PINS - 1; pixel++)
        {
            *led++ = (ch & 0x01) ? 16 : 0;
            ch >>= 1;
        }
    }
}

void led_show_array(const char *arr, uint8_t size)
{
    for (uint8_t i = 0; i < size; i++)
    {
        led_putchar(arr[i]);
        Delay_Ms(300);
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
        const char *start = "\x1c\x1d\x1e\x1f";
        led_show_array(start, strlen(start));
        const char *count_down = "543210";
        led_show_array(count_down, strlen(count_down));

        // Run snake
        for (uint8_t e = 0; e < sizeof(effects) / sizeof(effects[0]); e++)
        {
            set_effect(e);
            for (uint8_t loop = 0; loop < LED_MATRIX_SIZE; loop++)
            {
                // Shuffle the LED duty cycles
                uint8_t t = led_duty_cycles[0];
                for (uint8_t i = 0; i < LED_MATRIX_SIZE - 1; i++)
                {
                    led_duty_cycles[i] = led_duty_cycles[i + 1];
                }
                led_duty_cycles[LED_MATRIX_SIZE - 1] = t;

                Delay_Ms(50);
            }
        }

        const char *msg[] = {"Hello", "World", "!!!!!", "LoveU", "Good ", "Night", "\x1b\x1b\x1b\x1b\x1b"};
        for (uint8_t i = 0; i < 7; i++)
        {
            led_putchar(msg[i][BOARD]);
            Delay_Ms(800);
        }
        const char *end = "\x1f\x1e\x1d\x1c";
        led_show_array(end, strlen(end));
    }
}
