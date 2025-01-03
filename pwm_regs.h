// bits del CSR
#define PH_ADV      7
#define PH_RET      6
#define DIVMODE     4 // 5:4
#define B_INV       3
#define A_INV       2
#define PH_CORRECT  1
#define EN          0

typedef struct {
    uint32_t CSR;  // Control and Status Register (desplazamiento 0x00)
    uint32_t DIV;  // Divider Register (desplazamiento 0x04)
    uint32_t CTR;  // Counter Register (desplazamiento 0x08)
    uint32_t CC;   // Compare/Capture Register (desplazamiento 0x0C)
    uint32_t TOP;  // Top Register (desplazamiento 0x10)
} pwm_channel_t;

typedef struct {
    pwm_channel_t CH[12];  // 12 canales PWM (CH0 a CH11)
} pwm_t;

#define PWM ((volatile pwm_t *)0x400A8000u)  // Dirección base del controlador PWM

