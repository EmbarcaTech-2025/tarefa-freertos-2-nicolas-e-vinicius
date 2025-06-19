
# Tarefa: Roteiro de FreeRTOS #2 - EmbarcaTech 2025

Autor: **Nícolas e Vinícius**

Curso: Residência Tecnológica em Sistemas Embarcados

Instituição: EmbarcaTech - HBr

Campinas, Junho de 2025

---

# Jogo do Reflexo

Este projeto implementa um jogo de teste de reflexo, apresentando uma sequência de LEDs à qual o jogador deve responder com os botões respectivos a cada cor do LED. Isso ocorre conforme se segue:
- LED Verde - Botão A
- LED Vermelho - Botão B
- LED Amarelo - Botão Joystick
OBS: as cores são acompanhadas por sons característicos de cada uma delas.

O jogo ocorre de forma que uma cor é apresentada no LED principal da BitDogLab por vez e o jogador deve pressionar o botão correspondente. O tempo inicial entre cores é de 1000 ms e é decrescido a cada três acertos consecutivos o tempo entre cores é reduzido em 50 ms até um limite mínimo de 300 ms. Ainda assim, a cada erro, o tempo é acrescido de 50 ms.

---

##  Lista de materiais: 

| Componente            | Conexão na BitDogLab      |
|-----------------------|---------------------------|
| BitDogLab (Pi Pico W - RP2040) | -                |
| Botões (dois)      | GPIOs 5 e 6 (pull-up)        |  
| Buzzer             | PWM: GPIO21                  |
| LED RGB            | GPIOs 11, 12 e 13 (output)   |
---

## Execução - Opção 1

1. Abra o projeto no VS Code, usando o ambiente com suporte ao SDK do Raspberry Pi Pico (CMake + compilador ARM);
2. Compile o projeto normalmente (Ctrl+Shift+B no VS Code ou via terminal com cmake e make);
3. Conecte sua BitDogLab via cabo USB e coloque a Pico no modo de boot (pressione o botão BOOTSEL e conecte o cabo);
4. Copie o arquivo .uf2 gerado para a unidade de armazenamento que aparece (RPI-RP2);
5. A Pico reiniciará automaticamente e começará a executar o código;
<br />
Sugestão: Use a extensão da Raspberry Pi Pico no VScode para importar o programa como projeto Pico, usando o sdk 2.1.0.

## Execução - Opção 2

1. Crie uma pasta build dentro da pasta raiz deste repositório (mkdir build);
2. Entre na pasta criada (cd build);
3. Execute cmake de dentro da pasta criada (cmake ../CMakeLists.txt);
4. Execute o comando make ainda dentro da pasta criada (make);
5. Conecte a placa ao PC em modo de gravação;
6. Copie o arquivo .uf2 gerado na pasta build durante a compilação para o disco da placa.

##  Arquivos

- `src/main.c`: Código principal do projeto;
- `inc/ssd1306_i2c.c`: .c da biblioteca do Display;
- `inc/ssd1306.h`: .h da biblioteca do Display;
- `inc/ssd1306_i2c.h`: .h de tratamento i2c da biblioteca do Display;
- `inc/ssd1306_font.h`: .h da fonte da biblioteca do Display;
- `include/FreeRTOSConfig.h`: .h header para configuração do FreeRTOS;
  
---

## 🖼️ Vídeo do projeto

https://youtube.com/shorts/J1aq4PIZoXM

---

## 📜 Licença
GNU GPL-3.0.
