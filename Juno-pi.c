#include "pico/stdlib.h"
#include "hardware/uart.h"
#include "hardware/gpio.h"
#include "hardware/irq.h"
#include "pwm_regs.h"

#define UART_ID uart0
#define BAUD_RATE 31250
#define LED_PIN 25

volatile uint8_t MIDI_byte = 0, MIDI_msgFlag = 0;
volatile uint8_t MIDI_byte0 = 0x90, MIDI_byte1 = 0, MIDI_byte2 = 0;

//					     B    C    C#   D    D#   E    F    F#   G    G#   A    A#   b
uint8_t MIDI_freqs[] = {252, 238, 224, 212, 200, 189, 178, 168, 158, 149, 141, 133, 126};

// Función que se llama cuando se recibe un byte por UART
void uart_irq_handler() {
        uint8_t byte = uart_getc(UART_ID);  // Leer el byte recibido
        uint8_t byte_channel = byte & 0xf0; // byte sin el canal MIDI

        if(byte_channel == 0xf0) return;    // si es un mensaje de sistema lo omito (start, stop, beat)
        if(byte > 0x7f) MIDI_byte = 0;      // si es un note on o note off reinicio la máquina de estados
        

        switch(MIDI_byte){
            case 0:
                MIDI_byte0 = byte_channel;
                MIDI_byte = 1;
                return;
            
            case 1:
                MIDI_byte1 = byte;
                MIDI_byte = 2;
                return;

            case 2:
                MIDI_byte2 = byte;
                MIDI_byte = 1; // running-mode

                MIDI_msgFlag = 1;
                return;
        }

        // si llegó hasta acá es porque hubo un error
        MIDI_byte = 1;

    
}

void setup_uart() {
    uart_init(UART_ID, BAUD_RATE);  // Configurar el UART con la velocidad de 31,250 baudios
    gpio_set_function(1, GPIO_FUNC_UART);  // Configurar el pin 1 como RX
    
    // Habilitar interrupciones en el UART para recibir datos
    uart_set_irq_enables(UART_ID, true, false);  // Habilitar interrupciones cuando hay datos disponibles

    // Configurar la interrupción
    irq_set_exclusive_handler(UART0_IRQ, uart_irq_handler);  // Asociar la interrupción con el manejador
    irq_set_enabled(UART0_IRQ, true);  // Habilitar la interrupción en el sistema
}

void setup_gpio() {
    gpio_init(LED_PIN);  // Inicializar el pin del LED (NOTE ON - NOTE OFF)
    gpio_set_dir(LED_PIN, GPIO_OUT);  // Configurar el pin como salida
}

void setup_pwm(){
    // Pin de  salida del PWM que genera el audio
    gpio_set_function(9, GPIO_FUNC_PWM);
    // PWM 7a en pin 14
    gpio_set_function(14, GPIO_FUNC_PWM);
    
    PWM->CH[4].CSR = (1 << EN);
    PWM->CH[4].DIV = (1 << 4);             // prescaler de 1
    PWM->CH[4].TOP = 0x03ff;        // top en 2^10 = 1.024
    PWM->CH[4].CC = (0x7fff << 16); // B = 0xffff/2

    PWM->CH[7].CSR = (1 << EN) | (1 << PH_CORRECT);
    PWM->CH[7].DIV = (32 << 4);             // prescaler de 64
    PWM->CH[7].TOP = 5326;        // top
    PWM->CH[7].CC = 5326 >> 1;

    PWM->CH[8].CSR = (1 << EN) | (1 << PH_CORRECT);
    PWM->CH[8].DIV = (32 << 4);             // prescaler de 64
    PWM->CH[8].TOP = 4227;        // top
}

int main() {

    setup_gpio();      // Configurar el pin del LED
    setup_uart();      // Configurar el UART con interrupciones
    setup_pwm();       // inicializa los 3 PWMs (1 salida + 2 generadores)

    while (true) {
        
        if(MIDI_msgFlag){
            switch(MIDI_byte0){
                case 0x90: // note ON
                    gpio_put(LED_PIN, 1);

                    volatile uint16_t nota = MIDI_freqs[MIDI_byte1%12] << 5;
                    PWM->CH[7].TOP = nota;
                    break;
                case 0x80: // note OFF
                    gpio_put(LED_PIN, 0);
                    break;
            }
            MIDI_msgFlag = 0;
        }

        uint16_t value = (uint32_t) (PWM->CH[7].CTR + PWM->CH[8].CTR) >> 4;// + (uint32_t)PWM->CH[8].CTR;

        PWM->CH[4].CC = (value << 16);

    }

    return 0;
}