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

//					            A     A#    B     C     C#    D     D#    E     F     F#    G     G#
const uint8_t MIDI_freqs[] = {10054, 9490, 8957, 8455, 7980, 7532, 7109, 6710, 6334, 5978, 5642, 5326};
//const uint8_t MIDI_freqs[] = {252, 238, 224, 212, 200, 189, 178, 168, 158, 149, 141, 133, 126};

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

// Manejo de frecuencia
void PWM_setNote(uint8_t slice, uint8_t nota, uint8_t detune){

    // Selector de octava
    if(nota < 33)
        PWM->CH[slice].DIV = 0; // 256
    else if(nota < 45)
        PWM->CH[slice].DIV = (128 << 4);
    else if(nota < 57)
        PWM->CH[slice].DIV = (64 << 4);
    else if(nota < 69)
        PWM->CH[slice].DIV = (32 << 4);
    else if(nota < 81)
        PWM->CH[slice].DIV = (16 << 4);
    else if(nota < 93)
        PWM->CH[slice].DIV = (8 << 4);
    else if(nota < 105)
        PWM->CH[slice].DIV = (4 << 4);
    else
        PWM->CH[slice].DIV = (2 << 4);


    // frecuencia base
    uint16_t freq = (nota>=33)? MIDI_freqs[(nota-33) % 12] : 0;
    // detune = 128 es NO desafinar
    if(detune > 128)
        // 430 es un semitono aprox
        freq += (106*(detune-128)) >> 7;
    else if(detune < 128)
        freq -= (106*(128-detune)) >> 7;

    // asigno la frecuencia al pwm slice
    PWM->CH[slice].TOP = freq;
}

// Manejo de voces
bool VOICE_on[5];
uint8_t VOICE_note[5];

int main() {

    setup_gpio();      // Configurar el pin del LED
    setup_uart();      // Configurar el UART con interrupciones
    setup_pwm();       // inicializa los 3 PWMs (1 salida + 2 generadores)

    while (true) {
        
        if(MIDI_msgFlag){
            switch(MIDI_byte0){
                case 0x90: // note ON
                    gpio_put(LED_PIN, 1);

                    VOICE_on[0] = true;
                    VOICE_note[0] = MIDI_byte1;
                    PWM_setNote(7, MIDI_byte1, 128);
                    
                    break;
                case 0x80: // note OFF
                    if(MIDI_byte1 != VOICE_note[0])
                        break;
                    
                    gpio_put(LED_PIN, 0);
                    VOICE_on[0] = false;

                    break;
            }
            MIDI_msgFlag = 0;
        }

        // Calculo la salida de audio
        uint16_t value = (uint32_t) ((VOICE_on[0]? PWM->CH[7].CTR : 0) + PWM->CH[8].CTR) >> 4;

        // Saco el audio por el PWM 4b
        PWM->CH[4].CC = (value << 16);

    }

    return 0;
}