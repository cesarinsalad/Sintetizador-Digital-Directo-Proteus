#include <18F4550.h>
#device ADC=10

#fuses HSPLL, NOWDT, NOPROTECT, NOLVP

#use delay(clock=48000000)

#include <stdlib.h>
#include <HDM64GS12.c>
#include <graphics.c>

// Botones
#define BTN_UP      PIN_A0
#define BTN_DOWN    PIN_A1
#define BTN_ENTER   PIN_A2
#define BTN_BACK    PIN_A3

// Constantes del Generador
#define INTERRUPT_FREQ 40000.0
#define SAMPLES_PER_CYCLE 256.0

// Variables Globales
enum {MENU_PRINCIPAL, MENU_ONDA, MENU_FRECUENCIA, MENU_AMPLITUD} menu_estado = MENU_PRINCIPAL;
int8 cursor_posicion = 0;
enum {SENOIDAL, CUADRADA, TRIANGULAR, DIENTE_SIERRA} tipo_onda_actual = SENOIDAL;
float frecuencia = 10.0;
float voltaje_pico = 3.3;
float voltaje_p2p = 1.0;
unsigned int16 phase_accumulator = 0;
unsigned int16 phase_increment = 0;
unsigned int8 dac_min = 0, dac_max = 255;
int1 ui_needs_update = TRUE;
unsigned int8 scaled_sine_table[256]; 

// --- Tabla de Onda Senoidal (16x16) ---
const unsigned int8 SINE_TABLE[16][16] = {
   {128,131,134,137,140,143,146,149,152,155,158,161,164,167,170,173},{176,178,181,184,187,189,192,195,197,200,202,204,207,209,211,213},
   {215,217,219,221,223,225,227,228,230,232,233,235,236,238,239,240},{242,243,244,245,246,247,248,249,250,251,251,252,253,253,254,254},
   {254,255,255,255,254,254,254,253,253,252,251,251,250,249,248,247},{246,245,244,243,242,240,239,238,236,235,233,232,230,228,227,225},
   {223,221,219,217,215,213,211,209,207,204,202,200,197,195,192,189},{187,184,181,178,176,173,170,167,164,161,158,155,152,149,146,143},
   {140,137,134,131,128,125,122,119,116,113,110,107,104,101,98,95},{92,89,86,83,80,78,75,72,70,67,65,63,60,58,56,54},
   {52,50,48,46,44,42,41,39,37,36,34,33,31,30,29,27},{26,25,24,23,22,21,20,19,18,17,16,15,15,14,13,13},
   {12,12,11,11,11,10,10,10,10,10,10,10,11,11,11,12},{12,13,13,14,15,15,16,17,18,19,20,21,22,23,24,25},
   {26,27,29,30,31,33,34,36,37,39,41,42,44,46,48,50},{52,54,56,58,60,63,65,67,70,72,75,78,80,83,86,89}
};

// Función para escribir en el DAC
void write_dac(unsigned int8 value) {
    output_bit(PIN_C0, bit_test(value, 0)); output_bit(PIN_C1, bit_test(value, 1));
    output_bit(PIN_C2, bit_test(value, 2)); output_bit(PIN_C4, bit_test(value, 3));
    output_bit(PIN_C5, bit_test(value, 4)); output_bit(PIN_C6, bit_test(value, 5));
    output_bit(PIN_C7, bit_test(value, 6)); output_bit(PIN_A5, bit_test(value, 7));
}

// Rutina de Servicio de Interrupción (ISR) del Timer1
#int_TIMER1
void timer1_isr() {
    set_timer1(65536 - (48000000 / 4 / INTERRUPT_FREQ));
    unsigned int8 dac_value = 0;
    unsigned int8 phase_8bit = phase_accumulator >> 8;
    switch(tipo_onda_actual) {
        case SENOIDAL: {
            dac_value = scaled_sine_table[phase_8bit];
            break;
        }
        case CUADRADA:
            dac_value = (phase_accumulator < 32768) ? dac_max : dac_min;
            break;
        case TRIANGULAR:
            if (phase_accumulator < 32768) {
                dac_value = dac_min + ( (unsigned int32)(dac_max-dac_min) * (phase_accumulator >> 7) ) / 256;
            } else {
                dac_value = dac_max - ( (unsigned int32)(dac_max-dac_min) * ((phase_accumulator-32768) >> 7) ) / 256;
            }
            break;
        case DIENTE_SIERRA:
            dac_value = dac_max - ( (unsigned int32)(dac_max-dac_min) * phase_accumulator ) / 65535;
            break;
    }
    write_dac(dac_value);
    phase_accumulator += phase_increment;
}

// Prototipos de Funciones 
void display_ui();
void handle_input();
void calculate_isr_parameters();

// Función Principal
void main() {
    disable_interrupts(GLOBAL);
    setup_psp(PSP_DISABLED);
    setup_adc_ports(NO_ANALOGS);
    setup_adc(ADC_OFF);
    delay_ms(50);
    set_tris_b(0x00);
    set_tris_d(0x00);
    glcd_init(ON);
    delay_ms(100);
    glcd_fillScreen(0);
    set_tris_a(0b00011111);
    set_tris_c(0b00001000);
    setup_timer_1(T1_INTERNAL | T1_DIV_BY_1);
    calculate_isr_parameters();
    enable_interrupts(INT_TIMER1);
    enable_interrupts(GLOBAL);

    while(TRUE) {
        handle_input();
        if (ui_needs_update) {
            display_ui();
            ui_needs_update = FALSE;
        }
        delay_ms(20);
    }
}

//  Funciones de Lógica y UI
void calculate_isr_parameters() {
    phase_increment = (unsigned int32)frecuencia * SAMPLES_PER_CYCLE * 65536.0 / INTERRUPT_FREQ;
    float v_ref = 5.0;
    
    switch(tipo_onda_actual) {
        case SENOIDAL: {
            float v_min = 1.15;
            float v_max = v_min + voltaje_p2p;
            unsigned int8 temp_dac_min = (v_min / v_ref) * 255.0;
            unsigned int8 temp_dac_max = (v_max / v_ref) * 255.0;
            if (temp_dac_max > 255) temp_dac_max = 255;

            int16 i, j, k=0;
            for (i=0; i<16; ++i) {
                for (j=0; j<16; ++j) {
                    unsigned int8 raw_sine_value = SINE_TABLE[i][j];
                    scaled_sine_table[k] = temp_dac_min + ((unsigned int32)(temp_dac_max - temp_dac_min) * raw_sine_value) / 255;
                    k++;
                }
            }
            break;
        }
        case CUADRADA:
            dac_min = 0;
            dac_max = (voltaje_pico / v_ref) * 255.0;
            break;
        case TRIANGULAR:
        case DIENTE_SIERRA:
            dac_min = 0;
            dac_max = (voltaje_pico / v_ref) * 255.0;
            break;
    }
}

void handle_input() {
    if (input(BTN_UP)) {
        delay_ms(50);
        if (input(BTN_UP)) {
            switch(menu_estado) {
                case MENU_PRINCIPAL: if (cursor_posicion > 0) cursor_posicion--; else cursor_posicion = 2; break;
                case MENU_ONDA: if (cursor_posicion > 0) cursor_posicion--; else cursor_posicion = 3; break;
                case MENU_FRECUENCIA:
                    if (tipo_onda_actual == SENOIDAL && frecuencia < 100) frecuencia += 1.0;
                    if (tipo_onda_actual == CUADRADA && frecuencia < 150) frecuencia += 1.0;
                    if (tipo_onda_actual == TRIANGULAR && frecuencia < 200) frecuencia += 1.0;
                    if (tipo_onda_actual == DIENTE_SIERRA && frecuencia < 300) frecuencia += 1.0;
                    calculate_isr_parameters(); break;
                case MENU_AMPLITUD:
                    if (tipo_onda_actual == SENOIDAL && voltaje_p2p < 2.3) voltaje_p2p += 0.1;
                    if (tipo_onda_actual == CUADRADA && voltaje_pico < 4.5) voltaje_pico += 0.1;
                    if (tipo_onda_actual == TRIANGULAR && voltaje_pico < 4.4) voltaje_pico += 0.1;
                    if (tipo_onda_actual == DIENTE_SIERRA && voltaje_pico < 4.6) voltaje_pico += 0.1;
                    calculate_isr_parameters(); break;
            }
            ui_needs_update = TRUE;
        }
        while(input(BTN_UP));
    }
    if (input(BTN_DOWN)) {
        delay_ms(50);
        if (input(BTN_DOWN)) {
            switch(menu_estado) {
                case MENU_PRINCIPAL: if (cursor_posicion < 2) cursor_posicion++; else cursor_posicion = 0; break;
                case MENU_ONDA: if (cursor_posicion < 3) cursor_posicion++; else cursor_posicion = 0; break;
                case MENU_FRECUENCIA:
                    if (tipo_onda_actual == SENOIDAL && frecuencia > 10) frecuencia -= 1.0;
                    if (tipo_onda_actual == CUADRADA && frecuencia > 15) frecuencia -= 1.0;
                    if (tipo_onda_actual == TRIANGULAR && frecuencia > 20) frecuencia -= 1.0;
                    if (tipo_onda_actual == DIENTE_SIERRA && frecuencia > 30) frecuencia -= 1.0;
                    calculate_isr_parameters(); break;
                case MENU_AMPLITUD:
                    if (tipo_onda_actual == SENOIDAL && voltaje_p2p > 1.0) voltaje_p2p -= 0.1;
                    if (tipo_onda_actual == CUADRADA && voltaje_pico > 3.3) voltaje_pico -= 0.1;
                    if (tipo_onda_actual == TRIANGULAR && voltaje_pico > 2.0) voltaje_pico -= 0.1;
                    if (tipo_onda_actual == DIENTE_SIERRA && voltaje_pico > 1.5) voltaje_pico -= 0.1;
                    calculate_isr_parameters(); break;
            }
            ui_needs_update = TRUE;
        }
        while(input(BTN_DOWN));
    }
    if (input(BTN_ENTER)) {
        delay_ms(50);
        if (input(BTN_ENTER)) {
            switch(menu_estado) {
                case MENU_PRINCIPAL:
                    if (cursor_posicion == 0) menu_estado = MENU_ONDA;
                    else if (cursor_posicion == 1) menu_estado = MENU_FRECUENCIA;
                    else if (cursor_posicion == 2) menu_estado = MENU_AMPLITUD;
                    cursor_posicion = 0;
                    break;
                case MENU_ONDA:
                    tipo_onda_actual = cursor_posicion;
                    switch(tipo_onda_actual) {
                        case SENOIDAL:
                            if (frecuencia < 10) frecuencia = 10; if (frecuencia > 100) frecuencia = 100;
                            break;
                        case CUADRADA:
                            if (frecuencia < 15) frecuencia = 15; if (frecuencia > 150) frecuencia = 150;
                            if (voltaje_pico < 3.3) voltaje_pico = 3.3; if (voltaje_pico > 4.5) voltaje_pico = 4.5;
                            break;
                        case TRIANGULAR:
                            if (frecuencia < 20) frecuencia = 20; if (frecuencia > 200) frecuencia = 200;
                            if (voltaje_pico < 2.0) voltaje_pico = 2.0; if (voltaje_pico > 4.4) voltaje_pico = 4.4;
                            break;
                        case DIENTE_SIERRA:
                            if (frecuencia < 30) frecuencia = 30; if (frecuencia > 300) frecuencia = 300;
                            if (voltaje_pico < 1.5) voltaje_pico = 1.5; if (voltaje_pico > 4.6) voltaje_pico = 4.6;
                            break;
                    }
                    calculate_isr_parameters();
                    menu_estado = MENU_PRINCIPAL;
                    cursor_posicion = 0;
                    break;
            }
            ui_needs_update = TRUE;
        }
        while(input(BTN_ENTER));
    }
    if (input(BTN_BACK)) {
        delay_ms(50);
        if (input(BTN_BACK)) {
            menu_estado = MENU_PRINCIPAL;
            cursor_posicion = 0;
            ui_needs_update = TRUE;
        }
        while(input(BTN_BACK));
    }
}

void display_ui() {
    const char *opcion1 = "1. Elegir Onda";
    const char *opcion2 = "2. Frecuencia";
    const char *opcion3 = "3. Amplitud";
    const char *elegir = "ELEGIR ONDA";
    const char *senoidal_s = "Senoidal";
    const char *cuadrada_s = "Cuadrada";
    const char *triangular_s = "Triangular";
    const char *diente_sierra_s = "Diente de Sierra";
    const char *ajustar_frecuencia = "AJUSTAR FRECUENCIA";
    const char *ajustar_amplitud = "AJUSTAR AMPLITUD";
    const char *use_arriba_abajo = "USA UP/DOWN";
    const char *atras = "BACK";
    const char *cursor = ">";
    char text_buffer[25];
    glcd_fillScreen(0);
    switch(menu_estado) {
        case MENU_PRINCIPAL:
            switch(tipo_onda_actual){
                case SENOIDAL: sprintf(text_buffer, "Onda: Senoidal"); break;
                case CUADRADA: sprintf(text_buffer, "Onda: Cuadrada"); break;
                case TRIANGULAR: sprintf(text_buffer, "Onda: Triangular"); break;
                case DIENTE_SIERRA: sprintf(text_buffer, "Onda: D. Sierra"); break;
            }
            glcd_text57(5, 2, text_buffer, 1, ON);
            sprintf(text_buffer, "F: %3.1fHz", frecuencia);
            glcd_text57(5, 12, text_buffer, 1, ON);
            if (tipo_onda_actual == SENOIDAL) {
                sprintf(text_buffer, "A: %1.1fV", voltaje_p2p);
            } else {
                sprintf(text_buffer, "A: %1.1f V", voltaje_pico);
            }
            glcd_text57(70, 12, text_buffer, 1, ON);
            glcd_line(0, 22, 127, 22, ON);
            glcd_text57(10, 28, opcion1, 1, ON);
            glcd_text57(10, 38, opcion2, 1, ON);
            glcd_text57(10, 48, opcion3, 1, ON);
            glcd_text57(0, 28 + (cursor_posicion * 10), cursor, 1, ON);
            break;
        case MENU_ONDA:
            glcd_text57(25, 2, elegir, 1, ON);
            glcd_line(0, 12, 127, 12, ON);
            glcd_text57(10, 20, senoidal_s, 1, ON);
            glcd_text57(10, 30, cuadrada_s, 1, ON);
            glcd_text57(10, 40, triangular_s, 1, ON);
            glcd_text57(10, 50, diente_sierra_s, 1, ON);
            glcd_text57(0, 20 + (cursor_posicion * 10), cursor, 1, ON);
            break;
        case MENU_FRECUENCIA:
            glcd_text57(5, 5, ajustar_frecuencia, 1, ON);
            glcd_line(0, 15, 127, 15, ON);
            sprintf(text_buffer, "F: %3.1f Hz", frecuencia);
            glcd_text57(30, 20, text_buffer, 1, ON);
            glcd_text57(5, 40, use_arriba_abajo, 1, ON);
            glcd_text57(80, 40, atras, 1, ON);
            break;
        case MENU_AMPLITUD:
            glcd_text57(10, 5, ajustar_amplitud, 1, ON);
            glcd_line(0, 15, 127, 15, ON);
            if (tipo_onda_actual == SENOIDAL) {
                sprintf(text_buffer, "Vpp: %1.1f V", voltaje_p2p);
            } else {
                sprintf(text_buffer, "Vpp: %1.1f V", voltaje_pico);
            }
            glcd_text57(30, 20, text_buffer, 1, ON);
            glcd_text57(5, 40, use_arriba_abajo, 1, ON);
            glcd_text57(80, 40, atras, 1, ON);
            break;
    }

    #ifdef FAST_GLCD
    glcd_update();
    #endif
}
