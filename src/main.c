#include <stdio.h>                   // Inclui a biblioteca padrão de entrada/saída (para printf, etc.)
#include <stdlib.h>                  // Inclui a biblioteca padrão de utilidades (para rand, etc.)
#include <string.h>                  // Inclui a biblioteca de manipulação de strings (para memset, snprintf)
#include "pico/stdlib.h"             // Inclui a biblioteca padrão do SDK do Raspberry Pi Pico
#include "pico/binary_info.h"        // Inclui a biblioteca para informações binárias do Pico (usado em compilação)
#include "pico/rand.h"               // Inclui a biblioteca para aquisição de números aleatórios da Raspberry Pi pico
#include "hardware/gpio.h"           // Inclui a biblioteca para controle de pinos GPIO
#include "hardware/pwm.h"            // Inclui a biblioteca para controle de PWM (Pulse Width Modulation)
//#include "hardware/adc.h"            // Inclui a biblioteca para controle do ADC (Analog-to-Digital Converter), embora não seja usado diretamente neste código
#include "hardware/timer.h"          // Inclui a biblioteca para funções de temporização (usado para get_absolute_time)
#include "hardware/i2c.h"            // Inclui a biblioteca para comunicação I2C (usado pelo display OLED)
#include "inc/ssd1306.h"             // Inclui o arquivo de cabeçalho personalizado para o driver do display OLED SSD1306
#include "FreeRTOS.h"                // Inclui a biblioteca principal do FreeRTOS
#include "task.h"                    // Inclui a biblioteca para gerenciamento de tarefas do FreeRTOS

// Definições dos pinos GPIO utilizados no projeto
#define LED_RED_PIN         13       // Pino GPIO para o LED Vermelho
#define LED_GREEN_PIN       11       // Pino GPIO para o LED Verde
#define LED_BLUE_PIN        12       // Pino GPIO para o LED Azul
#define BUZZER_B            10       // Pino GPIO para o Buzzer B
#define BUZZER_A            21       // Pino GPIO para o Buzzer A
#define BUTTON_A_PIN        5        // Pino GPIO para o Botão A
#define BUTTON_B_PIN        6        // Pino GPIO para o Botão B
#define JOYSTICK_BUTTON     22       // Pino GPIO para o Botão do Joystick (usado para amarelo)

// Definições específicas para a comunicação I2C do display OLED
#define I2C_SDA             14       // Pino GPIO 14 para dados I2C (SDA)
#define I2C_SCL             15       // Pino GPIO 15 para clock I2C (SCL)

// Definições das frequências para as notas musicais e duração padrão
#define NOTE_C4         3000         // Frequência para a nota Dó 4 (arbitrária para o buzzer)
#define NOTE_D4         3500         // Frequência para a nota Ré 4
#define NOTE_F4         4000         // Frequência para a nota Fá 4
#define NOTE_DURATION   300          // Duração padrão das notas em milissegundos (ms)

// Variáveis globais para configuração do PWM dos buzzers
uint buzzer_slice;                   // Variável para armazenar o 'slice' (bloco de hardware PWM) do buzzer
uint buzzer_channel;                 // Variável para armazenar o 'canal' (dentro do slice) do buzzer

// Variáveis globais para controlar o estado do jogo
volatile bool game_over = false;     // Flag booleana que indica se o jogo terminou (volatile para acesso por múltiplas tarefas)
volatile int countdown_seconds = 60; // Tempo inicial da contagem regressiva em segundos (ajustável), volatile para acesso por múltiplas tarefas

// Protótipos das funções utilizadas no código
void task_reflex_test(void *params);        // Protótipo da tarefa FreeRTOS para a lógica do jogo de reflexo
void task_countdown_display(void *params);  // Protótipo da tarefa FreeRTOS para o display OLED e contagem
void play_tone(uint gpio, uint32_t frequency, uint32_t duration_ms); // Protótipo da função para tocar um tom no buzzer
void play_color_sound(int color, uint buzzer_pin); // Protótipo da função para tocar o som de acordo com a cor
void display_two_messages(char *message1, int line1, char *message2, int line2); // Protótipo da função para exibir duas mensagens no OLED

// --- Funções de Inicialização e Controle de Periféricos ---

// Inicializa os pinos GPIO configurando sua direção e, para botões, habilitando pull-ups
void init_gpio() {
    gpio_init(LED_RED_PIN);          // Inicializa o pino do LED Vermelho
    gpio_set_dir(LED_RED_PIN, true); // Define o pino como saída

    gpio_init(LED_GREEN_PIN);        // Inicializa o pino do LED Verde
    gpio_set_dir(LED_GREEN_PIN, true); // Define o pino como saída

    gpio_init(LED_BLUE_PIN);         // Inicializa o pino do LED Azul
    gpio_set_dir(LED_BLUE_PIN, true); // Define o pino como saída

    gpio_init(BUZZER_A);             // Inicializa o pino do Buzzer A
    gpio_set_function(BUZZER_A, GPIO_FUNC_PWM); // Define o pino para função PWM (controle de frequência)

    gpio_init(BUZZER_B);             // Inicializa o pino do Buzzer B
    gpio_set_function(BUZZER_B, GPIO_FUNC_PWM); // Define o pino para função PWM

    gpio_init(BUTTON_A_PIN);         // Inicializa o pino do Botão A
    gpio_set_dir(BUTTON_A_PIN, false); // Define o pino como entrada
    gpio_pull_up(BUTTON_A_PIN);      // Habilita o resistor de pull-up interno (botão conectado ao GND)

    gpio_init(BUTTON_B_PIN);         // Inicializa o pino do Botão B
    gpio_set_dir(BUTTON_B_PIN, false); // Define o pino como entrada
    gpio_pull_up(BUTTON_B_PIN);      // Habilita o resistor de pull-up interno

    gpio_init(JOYSTICK_BUTTON);      // Inicializa o pino do Botão do Joystick
    gpio_set_dir(JOYSTICK_BUTTON, false); // Define o pino como entrada
    gpio_pull_up(JOYSTICK_BUTTON);   // Habilita o resistor de pull-up interno
}

// Toca um tom no buzzer especificado com uma dada frequência e duração
void play_tone(uint gpio, uint32_t frequency, uint32_t duration_ms) {
    buzzer_slice = pwm_gpio_to_slice_num(gpio);   // Obtém o número do slice PWM associado ao pino GPIO
    buzzer_channel = pwm_gpio_to_channel(gpio);   // Obtém o número do canal PWM associado ao pino GPIO

    uint32_t clock = 125000000;                   // Frequência do clock do sistema (125 MHz para o Pico)
    uint32_t wrap = clock / frequency;            // Calcula o valor de 'wrap' (período do PWM) para a frequência desejada

    pwm_config config = pwm_get_default_config(); // Obtém a configuração padrão do PWM
    pwm_config_set_clkdiv(&config, 1.0f);         // Define o divisor de clock para 1 (sem divisão)
    pwm_config_set_wrap(&config, wrap);           // Define o valor de 'wrap' (ciclos até reiniciar)
    pwm_init(buzzer_slice, &config, true);        // Inicializa o slice PWM com a configuração e o habilita

    pwm_set_chan_level(buzzer_slice, buzzer_channel, wrap * 0.3); // Define o nível do canal para 30% do 'wrap' (30% de ciclo de trabalho)

    vTaskDelay(pdMS_TO_TICKS(duration_ms));       // Aguarda a duração especificada da nota usando delay do FreeRTOS

    pwm_set_chan_level(buzzer_slice, buzzer_channel, 0); // Desliga o buzzer definindo o nível do canal para 0
}

// Toca o som correspondente à cor do LED aceso
void play_color_sound(int color, uint buzzer_pin) {
    switch (color) {             // Verifica a cor passada como parâmetro
        case 0:                  // Se a cor for 0 (verde)
            play_tone(buzzer_pin, NOTE_C4, NOTE_DURATION); // Toca a nota C4 no buzzer especificado
            break;               // Sai do switch
        case 1:                  // Se a cor for 1 (vermelho)
            play_tone(buzzer_pin, NOTE_D4, NOTE_DURATION); // Toca a nota D4
            break;               // Sai do switch
        case 2:                  // Se a cor for 2 (amarelo)
            play_tone(buzzer_pin, NOTE_F4, NOTE_DURATION); // Toca a nota F4
            break;               // Sai do switch
        default:                 // Para qualquer outra cor (caso de segurança)
            break;               // Não faz nada
    }
}

// Função para exibir duas mensagens em linhas diferentes no display OLED
void display_two_messages(char *message1, int line1, char *message2, int line2) {
    // Define a área de renderização na tela do OLED (tela inteira)
    struct render_area frame_area = {
        .start_column = 0,                       // Coluna inicial (0)
        .end_column = ssd1306_width - 1,         // Coluna final (largura máxima do display)
        .start_page = 0,                         // Página inicial (0)
        .end_page = ssd1306_n_pages - 1          // Página final (número máximo de páginas)
    };
    
    calculate_render_area_buffer_length(&frame_area); // Calcula o tamanho do buffer necessário para a área definida

    uint8_t ssd[ssd1306_buffer_length];             // Declara um array (buffer) para armazenar os dados de pixel
    memset(ssd, 0, ssd1306_buffer_length);          // Limpa todo o buffer, preenchendo-o com zeros (apaga a tela)

    // Desenha a primeira string no buffer, em X=5 e Y=line1*8 (cada linha de texto tem 8 pixels de altura)
    ssd1306_draw_string(ssd, 5, line1 * 8, message1); 
    // Desenha a segunda string no buffer, em X=5 e Y=line2*8
    ssd1306_draw_string(ssd, 5, line2 * 8, message2); 
    
    render_on_display(ssd, &frame_area);            // Envia o conteúdo do buffer para o display OLED, atualizando a tela
}

// --- Tarefas FreeRTOS ---

// Tarefa para exibir a contagem regressiva e a pontuação no display OLED
void task_countdown_display(void *params) {
    // Configuração do barramento I2C para comunicação com o display OLED
    i2c_init(i2c1, 400000);             // Inicializa o I2C1 a 400kHz (frequência comum para OLED)
    gpio_set_function(I2C_SDA, GPIO_FUNC_I2C); // Configura o pino SDA (Dados) para a função I2C
    gpio_set_function(I2C_SCL, GPIO_FUNC_I2C); // Configura o pino SCL (Clock) para a função I2C
    gpio_pull_up(I2C_SDA);              // Habilita o resistor de pull-up no SDA (necessário para I2C)
    gpio_pull_up(I2C_SCL);              // Habilita o resistor de pull-up no SCL

    // Inicializa o driver do display OLED
    ssd1306_init();

    char line1_buffer[32];              // Buffer para armazenar a string da primeira linha (Tempo)
    char line2_buffer[32];              // Buffer para armazenar a string da segunda linha (Acertos)
    int *acertos_ptr = (int*)params;    // Ponteiro para a variável global 'game_acertos', que contém a pontuação atual

    // Variável para controlar a contagem de segundos de forma independente da frequência de atualização do display
    long last_second_tick = xTaskGetTickCount(); // Armazena o "tick" (tempo do FreeRTOS) da última vez que um segundo foi decrementado

    // Loop principal: executa enquanto houver tempo restante no jogo
    while (countdown_seconds > 0) { 
        // Formata as strings para exibição no display
        snprintf(line1_buffer, sizeof(line1_buffer), "Tempo: %02d", countdown_seconds); // Exibe o tempo restante
        snprintf(line2_buffer, sizeof(line2_buffer), "Acertos: %d", *acertos_ptr);     // Exibe a pontuação atual

        // Envia as mensagens formatadas para o display (atualiza a pontuação rapidamente)
        display_two_messages(line1_buffer, 2, line2_buffer, 4); // Exibe na linha 2 e 4 do display

        // Atualiza o display mais frequentemente, por exemplo, a cada 100ms
        vTaskDelay(pdMS_TO_TICKS(100)); // Causa um atraso de 100ms na tarefa (libera CPU para outras tarefas)

        // Verifica se 1 segundo real se passou para decrementar a contagem regressiva
        if ((xTaskGetTickCount() - last_second_tick) >= pdMS_TO_TICKS(1000)) {
            countdown_seconds--; // Decrementa o contador de segundos
            last_second_tick = xTaskGetTickCount(); // Atualiza o último tick para o próximo segundo
        }
    }

    // --- O tempo do jogo acabou, agora entra na fase de exibição da tela de "GAME OVER!" ---
    game_over = true; // Define a flag global 'game_over' para true, sinalizando que o jogo terminou para outras tarefas

    // Loop para manter a mensagem de "GAME OVER!" com a pontuação final na tela por um tempo determinado (5 segundos)
    for (int i = 0; i < 50; i++) { // Este loop executará 50 vezes
        snprintf(line1_buffer, sizeof(line1_buffer), "GAME OVER!");    // Primeira linha: "GAME OVER!"
        snprintf(line2_buffer, sizeof(line2_buffer), "Score: %d", *acertos_ptr); // Segunda linha: "Score: [Pontuação Final]"
        display_two_messages(line1_buffer, 2, line2_buffer, 4);         // Atualiza o display com a mensagem final
        vTaskDelay(pdMS_TO_TICKS(100));                                 // Espera 100ms antes de redesenhar (para manter visível)
    }
    
    // Limpa o display completamente após a exibição final
    display_two_messages("", 0, "", 0); // Envia mensagens vazias para limpar todas as linhas

    // Deleta a própria tarefa, liberando seus recursos na memória do FreeRTOS
    vTaskDelete(NULL); 
}

// Tarefa principal que gerencia a lógica do jogo de reflexo
void task_reflex_test(void *params) {
    int *acertos_ptr = (int*)params;     // Ponteiro para a variável 'game_acertos' (pontuação)
    int delay_ms = 1000;                 // Tempo inicial de espera entre os flashes de LEDs (1000 ms = 1 segundo)
    const int min_delay_ms = 300;        // Tempo mínimo de espera entre os flashes (para não ficar impossível)

    // Loop principal do jogo: continua enquanto a flag 'game_over' for falsa
    while (!game_over) { 
        uint32_t color = get_rand_32() % 3;  // Gera um número aleatório (0, 1 ou 2) para escolher a cor (verde, vermelho, amarelo)
        uint led_pin = 0;                    // Variável para armazenar o pino do LED a ser aceso
        uint buzzer_pin = 0;                 // Variável para armazenar o pino do buzzer a ser usado

        // Atribui o pino do LED e do buzzer de acordo com a cor escolhida
        switch (color) {
            case 0: // Cor 0: Verde
                led_pin = LED_GREEN_PIN; // Define o LED verde
                buzzer_pin = BUZZER_A;   // Define o buzzer A
                break;                   // Sai do switch
            case 1: // Cor 1: Vermelho
                led_pin = LED_RED_PIN;   // Define o LED vermelho
                buzzer_pin = BUZZER_B;   // Define o buzzer B
                break;                   // Sai do switch
            case 2: // Cor 2: Amarelo (representado por ambos LED Verde e Vermelho acesos)
                led_pin = 0;             // Pino não é usado diretamente aqui, os LEDs são controlados em separado
                buzzer_pin = BUZZER_B;   // Define o buzzer B
                break;                   // Sai do switch
        }

        // Acende o(s) LED(s) correspondente(s) à cor escolhida
        if (color == 2) { // Se a cor for amarelo
            gpio_put(LED_GREEN_PIN, 1); // Acende o LED verde
            gpio_put(LED_RED_PIN, 1);   // Acende o LED vermelho
        } else { // Se for verde ou vermelho
            gpio_put(led_pin, 1);       // Acende apenas o LED correspondente (verde ou vermelho)
        }

        play_color_sound(color, buzzer_pin); // Toca o som associado à cor

        bool correct = false; // Flag para indicar se o jogador acertou a cor
        // Captura o tempo atual para medir o tempo de reação
        long start_time = to_ms_since_boot(get_absolute_time()); 

        // Loop para esperar o clique do botão correspondente, ou até o tempo limite/game over
        while (!correct && !game_over) {  
            // Verifica se o botão A (para verde) foi pressionado
            if (color == 0 && gpio_get(BUTTON_A_PIN) == 0) { 
                correct = true; // Acertou
            } 
            // Verifica se o botão B (para vermelho) foi pressionado
            else if (color == 1 && gpio_get(BUTTON_B_PIN) == 0) { 
                correct = true; // Acertou
            } 
            // Verifica se o botão do joystick (para amarelo) foi pressionado
            else if (color == 2 && gpio_get(JOYSTICK_BUTTON) == 0) { 
                correct = true; // Acertou
            }
            vTaskDelay(pdMS_TO_TICKS(10)); // Pequeno atraso para evitar leituras falsas (debouncing básico) e ceder CPU

            // Verifica se o jogador demorou demais para reagir (tempo limite para o clique)
            if (to_ms_since_boot(get_absolute_time()) - start_time > delay_ms + 200) { 
                correct = false; // Não acertou a tempo ou clicou errado
                break;           // Sai do loop de espera por clique
            }
        }
        
        // Desliga todos os LEDs após a rodada (clique ou tempo limite)
        gpio_put(LED_RED_PIN, 0);
        gpio_put(LED_GREEN_PIN, 0);
        gpio_put(LED_BLUE_PIN, 0);

        if (correct) {                   // Se o jogador acertou
            (*acertos_ptr)++;            // Incrementa a pontuação
            // Se a pontuação for múltipla de 3 e o jogo não estiver no delay mínimo, acelera o jogo
            if (*acertos_ptr % 3 == 0 && delay_ms > min_delay_ms) {
                delay_ms -= 100;         // Diminui o tempo de espera em 100ms
                if (delay_ms < min_delay_ms) { // Garante que o delay não seja menor que o mínimo
                    delay_ms = min_delay_ms;
                }
            }
        } else {                         // Se o jogador errou ou não clicou a tempo
            delay_ms += 50;              // Aumenta o tempo de espera (penalidade)
        }
        
        // Se o jogo ainda não acabou, espera o delay atual antes de iniciar a próxima rodada
        if (!game_over) {
            vTaskDelay(pdMS_TO_TICKS(delay_ms)); 
        }
    }
    
    // --- Ao final do jogo (quando 'game_over' se torna true), desliga todos os componentes e encerra a tarefa ---
    gpio_put(LED_RED_PIN, 0);            // Garante que o LED Vermelho esteja desligado
    gpio_put(LED_GREEN_PIN, 0);          // Garante que o LED Verde esteja desligado
    gpio_put(LED_BLUE_PIN, 0);           // Garante que o LED Azul esteja desligado
    pwm_set_chan_level(buzzer_slice, buzzer_channel, 0); // Garante que o buzzer esteja desligado
    vTaskDelete(NULL);                   // Deleta a própria tarefa, liberando seus recursos
}

// --- Função Principal do Programa ---
int main() {
    stdio_init_all();                    // Inicializa todas as configurações de I/O padrão (como serial/USB)
    init_gpio();                         // Chama a função para inicializar todos os pinos GPIO

    // Cria uma variável estática para armazenar a pontuação do jogo.
    // 'static' garante que esta variável tenha um tempo de vida durante toda a execução do programa
    // e que seu endereço possa ser passado com segurança para as tarefas.
    static int game_acertos = 0;         

    // Cria as tarefas do FreeRTOS:
    // xTaskCreate(Função_da_tarefa, "Nome_da_tarefa", Tamanho_da_pilha, Parâmetro, Prioridade, Handle_da_tarefa);
    // 1. task_reflex_test: Lógica principal do jogo de reflexo.
    //    - configMINIMAL_STACK_SIZE + 256: Define o tamanho da pilha da tarefa.
    //    - (void*)&game_acertos: Passa o endereço da variável 'game_acertos' como parâmetro para a tarefa.
    //    - 1: Define a prioridade da tarefa (prioridades mais altas executam primeiro).
    //    - NULL: Não precisamos de um 'handle' para esta tarefa aqui.
    xTaskCreate(task_reflex_test, "Reflex Test", configMINIMAL_STACK_SIZE + 256, (void*)&game_acertos, 1, NULL);
    
    // 2. task_countdown_display: Gerencia o display OLED e a contagem regressiva/pontuação.
    //    - Também passa o endereço de 'game_acertos' para que possa exibir a pontuação.
    xTaskCreate(task_countdown_display, "Countdown Display", configMINIMAL_STACK_SIZE + 256, (void*)&game_acertos, 1, NULL);

    vTaskStartScheduler();               // Inicia o agendador do FreeRTOS. A partir daqui, as tarefas criadas começarão a ser executadas.

    // Este loop infinito nunca deve ser alcançado em um sistema FreeRTOS funcionando corretamente,
    // pois o controle é passado para o agendador de tarefas.
    while (1) {
        // O scheduler FreeRTOS irá gerenciar a execução das tarefas criadas.
    }
    return 0; // Retorna 0 em caso de encerramento normal (teoricamente nunca alcançado aqui)
}
