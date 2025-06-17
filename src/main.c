#include <stdio.h>
#include <stdlib.h>
#include <string.h> // Para memset e snprintf
#include "pico/stdlib.h"
#include "pico/binary_info.h" // Para informações binárias
#include "hardware/gpio.h"
#include "hardware/pwm.h"
#include "hardware/adc.h"
#include "hardware/timer.h" // Para funções de temporização
#include "hardware/i2c.h"   // Para comunicação I2C
#include "inc/ssd1306.h"    // Para controle do display OLED SSD1306
#include "FreeRTOS.h"
#include "task.h"

// Definições dos pinos
#define LED_RED_PIN         13
#define LED_GREEN_PIN       11
#define LED_BLUE_PIN        12
#define BUZZER_B            10
#define BUZZER_A            21
#define BUTTON_A_PIN        5
#define BUTTON_B_PIN        6
#define JOYSTICK_BUTTON     22

// Definições para o OLED
#define I2C_SDA             14  // Pino GPIO 14 para dados I2C (SDA)
#define I2C_SCL             15  // Pino GPIO 15 para clock I2C (SCL)

// Notas musicais
#define NOTE_C4         3000
#define NOTE_D4         3500
#define NOTE_F4         4000
#define NOTE_DURATION   300 // ms

// Variáveis globais PWM
uint buzzer_slice;
uint buzzer_channel;

// Variáveis globais para o jogo
volatile bool game_over = false;
volatile int countdown_seconds = 60; // Tempo inicial da contagem regressiva (ajustável)

// Protótipos das funções
void task_reflex_test(void *params);
void task_countdown_display(void *params);
void play_tone(uint gpio, uint32_t frequency, uint32_t duration_ms);
void play_color_sound(int color, uint buzzer_pin);
void display_two_messages(char *message1, int line1, char *message2, int line2);

// --- Funções de Inicialização e Controle de Periféricos ---

// Inicializa GPIOs (LEDs, Buzzers, Botões)
void init_gpio() {
    gpio_init(LED_RED_PIN);
    gpio_set_dir(LED_RED_PIN, true);

    gpio_init(LED_GREEN_PIN);
    gpio_set_dir(LED_GREEN_PIN, true);

    gpio_init(LED_BLUE_PIN);
    gpio_set_dir(LED_BLUE_PIN, true);

    gpio_init(BUZZER_A);
    gpio_set_function(BUZZER_A, GPIO_FUNC_PWM);

    gpio_init(BUZZER_B);
    gpio_set_function(BUZZER_B, GPIO_FUNC_PWM);

    gpio_init(BUTTON_A_PIN);
    gpio_set_dir(BUTTON_A_PIN, false);
    gpio_pull_up(BUTTON_A_PIN);

    gpio_init(BUTTON_B_PIN);
    gpio_set_dir(BUTTON_B_PIN, false);
    gpio_pull_up(BUTTON_B_PIN);

    gpio_init(JOYSTICK_BUTTON);
    gpio_set_dir(JOYSTICK_BUTTON, false);
    gpio_pull_up(JOYSTICK_BUTTON);
}

// Toca um tom no buzzer especificado com frequência e duração
void play_tone(uint gpio, uint32_t frequency, uint32_t duration_ms) {
    buzzer_slice = pwm_gpio_to_slice_num(gpio);
    buzzer_channel = pwm_gpio_to_channel(gpio);

    uint32_t clock = 125000000;
    uint32_t wrap = clock / frequency;

    pwm_config config = pwm_get_default_config();
    pwm_config_set_clkdiv(&config, 1.0f);
    pwm_config_set_wrap(&config, wrap);
    pwm_init(buzzer_slice, &config, true);

    pwm_set_chan_level(buzzer_slice, buzzer_channel, wrap * 0.3); // Define 30% do ciclo de trabalho

    vTaskDelay(pdMS_TO_TICKS(duration_ms)); // Aguarda a duração do tom

    pwm_set_chan_level(buzzer_slice, buzzer_channel, 0); // Desliga o buzzer
}

// Toca o som correspondente à cor do LED aceso
void play_color_sound(int color, uint buzzer_pin) {
    switch (color) {
        case 0:  // Verde
            play_tone(buzzer_pin, NOTE_C4, NOTE_DURATION);
            break;
        case 1:  // Vermelho
            play_tone(buzzer_pin, NOTE_D4, NOTE_DURATION);
            break;
        case 2:  // Amarelo
            play_tone(buzzer_pin, NOTE_F4, NOTE_DURATION);
            break;
        default:
            break;
    }
}

// Função para exibir duas mensagens no display OLED
void display_two_messages(char *message1, int line1, char *message2, int line2) {
    struct render_area frame_area = {
        .start_column = 0,
        .end_column = ssd1306_width - 1,
        .start_page = 0,
        .end_page = ssd1306_n_pages - 1
    };
    
    calculate_render_area_buffer_length(&frame_area);

    uint8_t ssd[ssd1306_buffer_length];
    memset(ssd, 0, ssd1306_buffer_length); // Limpa o buffer para o display

    ssd1306_draw_string(ssd, 5, line1 * 8, message1); // Desenha a primeira mensagem
    ssd1306_draw_string(ssd, 5, line2 * 8, message2); // Desenha a segunda mensagem
    
    render_on_display(ssd, &frame_area); // Envia o buffer para o display
}

// --- Tarefas FreeRTOS ---

// Tarefa para exibir a contagem regressiva e a pontuação no OLED
void task_countdown_display(void *params) {
    // Configuração do I2C para o display OLED
    i2c_init(i2c1, 400000);             // Inicializa I2C a 400kHz
    gpio_set_function(I2C_SDA, GPIO_FUNC_I2C); // Configura pino SDA
    gpio_set_function(I2C_SCL, GPIO_FUNC_I2C); // Configura pino SCL
    gpio_pull_up(I2C_SDA);              // Habilita pull-up no SDA
    gpio_pull_up(I2C_SCL);              // Habilita pull-up no SCL

    // Inicializa o display OLED
    ssd1306_init();

    char line1_buffer[32];
    char line2_buffer[32];
    int *acertos_ptr = (int*)params; // Ponteiro para a variável de acertos (pontuação)

    // Variável para controlar a contagem de segundos de forma independente da atualização do display
    long last_second_tick = xTaskGetTickCount(); 

    // Loop principal: exibe tempo e acertos enquanto o tempo não zera
    while (countdown_seconds > 0) { 
        // Formata as strings para exibição
        snprintf(line1_buffer, sizeof(line1_buffer), "Tempo: %02d", countdown_seconds);
        snprintf(line2_buffer, sizeof(line2_buffer), "Acertos: %d", *acertos_ptr); // Usa o valor da pontuação

        // Envia as mensagens para o display (atualiza a pontuação rapidamente)
        display_two_messages(line1_buffer, 2, line2_buffer, 4);

        // Atualiza o display mais frequentemente (ex: a cada 100ms)
        vTaskDelay(pdMS_TO_TICKS(100)); 

        // Decrementa o contador de segundos apenas a cada 1000ms (1 segundo real)
        if ((xTaskGetTickCount() - last_second_tick) >= pdMS_TO_TICKS(1000)) {
            countdown_seconds--; // Decrementa o tempo
            last_second_tick = xTaskGetTickCount(); // Atualiza o último tick do segundo
        }
    }

    // --- O tempo acabou, agora exibe a tela de "GAME OVER!" com a pontuação final ---
    game_over = true; // Define a flag global para que a tarefa do jogo pare

    // Loop para manter a mensagem de GAME OVER com score na tela por um tempo (5 segundos)
    for (int i = 0; i < 50; i++) { // 50 iterações * 100ms = 5000ms (5 segundos)
        snprintf(line1_buffer, sizeof(line1_buffer), "GAME OVER!");
        snprintf(line2_buffer, sizeof(line2_buffer), "Score: %d", *acertos_ptr); // Exibe a pontuação final
        display_two_messages(line1_buffer, 2, line2_buffer, 4); 
        vTaskDelay(pdMS_TO_TICKS(100)); // Continua atualizando o display a cada 100ms
    }
    
    // Limpa o display no final, garantindo que não fique com a última mensagem estática
    display_two_messages("", 0, "", 0); 

    // Deleta a própria tarefa ao fim do jogo
    vTaskDelete(NULL); 
}

// Tarefa principal do jogo de reflexo
void task_reflex_test(void *params) {
    int *acertos_ptr = (int*)params; // Ponteiro para a variável de acertos
    int delay_ms = 1000;                // Tempo inicial de espera entre os flashes
    const int min_delay_ms = 300;       // Delay mínimo permitido

    while (!game_over) { // O loop do jogo continua enquanto 'game_over' for falso
        int color = rand() % 3; // Gera uma cor aleatória (0=verde, 1=vermelho, 2=amarelo)
        uint led_pin = 0;
        uint buzzer_pin = 0;

        // Atribui o pino do LED e do buzzer de acordo com a cor escolhida
        switch (color) {
            case 0: // Verde
                led_pin = LED_GREEN_PIN;
                buzzer_pin = BUZZER_A;
                break;
            case 1: // Vermelho
                led_pin = LED_RED_PIN;
                buzzer_pin = BUZZER_B;
                break;
            case 2: // Amarelo (ambos LEDs verde e vermelho acendem)
                led_pin = 0;    // Não usado diretamente, LEDs controlados separadamente
                buzzer_pin = BUZZER_B;
                break;
        }

        // Acende o LED (ou LEDs para amarelo)
        if (color == 2) {
            gpio_put(LED_GREEN_PIN, 1);
            gpio_put(LED_RED_PIN, 1);
        } else {
            gpio_put(led_pin, 1);
        }

        play_color_sound(color, buzzer_pin); // Toca o som correspondente à cor

        bool correct = false;
        long start_time = to_ms_since_boot(get_absolute_time()); // Marca o tempo de início do flash

        // Espera pelo clique do botão (ou até o tempo limite/game over)
        while (!correct && !game_over) { 
            // Verifica qual botão foi pressionado e se corresponde à cor
            if (color == 0 && gpio_get(BUTTON_A_PIN) == 0) { // Verde -> Botão A
                correct = true;
            } else if (color == 1 && gpio_get(BUTTON_B_PIN) == 0) { // Vermelho -> Botão B
                correct = true;
            } else if (color == 2 && gpio_get(JOYSTICK_BUTTON) == 0) { // Amarelo -> Botão do Joystick
                correct = true;
            }
            vTaskDelay(pdMS_TO_TICKS(10)); // Pequeno atraso para evitar leitura de rebote e poupar CPU
            
            // Verifica se o tempo limite para o clique foi excedido
            if (to_ms_since_boot(get_absolute_time()) - start_time > delay_ms + 200) { 
                correct = false; // Clique muito lento ou errado
                break;
            }
        }
        
        // Desliga todos os LEDs após o clique ou tempo limite
        gpio_put(LED_RED_PIN, 0);
        gpio_put(LED_GREEN_PIN, 0);
        gpio_put(LED_BLUE_PIN, 0);

        if (correct) {
            (*acertos_ptr)++; // Incrementa a pontuação se o clique foi correto
            // Acelera o jogo a cada 3 acertos, até um limite mínimo
            if (*acertos_ptr % 3 == 0 && delay_ms > min_delay_ms) {
                delay_ms -= 100;
                if (delay_ms < min_delay_ms) {
                    delay_ms = min_delay_ms;
                }
            }
        } else {
            // Penalidade: torna o jogo um pouco mais lento se houver erro ou demora
            delay_ms += 50; 
        }
        
        // Se o jogo ainda não acabou, espera o delay atual antes do próximo flash
        if (!game_over) {
            vTaskDelay(pdMS_TO_TICKS(delay_ms));
        }
    }
    
    // --- Ao final do jogo (game_over se torna true), desliga tudo e encerra a tarefa ---
    gpio_put(LED_RED_PIN, 0);
    gpio_put(LED_GREEN_PIN, 0);
    gpio_put(LED_BLUE_PIN, 0);
    pwm_set_chan_level(buzzer_slice, buzzer_channel, 0); // Garante que o buzzer esteja desligado
    vTaskDelete(NULL); // Deleta a própria tarefa para liberar recursos
}

// --- Função Principal ---
int main() {
    stdio_init_all(); // Inicializa a comunicação serial
    init_gpio();      // Inicializa todos os pinos GPIO

    // Cria uma variável para armazenar os acertos.
    // 'static' garante que ela seja alocada na memória de dados e persista.
    static int game_acertos = 0; 

    // Cria as tarefas FreeRTOS:
    // 1. task_reflex_test: gerencia a lógica do jogo de reflexo.
    //    - Passa o endereço de 'game_acertos' para a tarefa.
    // 2. task_countdown_display: gerencia o display OLED e a contagem regressiva.
    //    - Também passa o endereço de 'game_acertos' para exibir a pontuação.
    xTaskCreate(task_reflex_test, "Reflex Test", configMINIMAL_STACK_SIZE + 256, (void*)&game_acertos, 1, NULL);
    xTaskCreate(task_countdown_display, "Countdown Display", configMINIMAL_STACK_SIZE + 256, (void*)&game_acertos, 1, NULL);

    vTaskStartScheduler(); // Inicia o agendador FreeRTOS, que começará a executar as tarefas

    // Este loop infinito nunca deve ser alcançado se o scheduler for iniciado com sucesso
    while (1) {} 
    return 0;
}