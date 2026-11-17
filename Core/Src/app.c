#include "app.h"
#include "oled.h"
#include <stdio.h>
#include <string.h>

UART_HandleTypeDef huart1;

static volatile uint8_t key_dir_event = 0;
static volatile uint8_t key_pause_event = 0;
static volatile uint8_t rx_byte = 0;
static volatile uint8_t packet_ready = 0;
static uint8_t packet[6];
static volatile uint8_t rx_index = 0;
static volatile uint32_t last_rx_tick = 0;

static uint8_t run_flag = 1;
static uint8_t dir_flag = 0;
static uint16_t step_ms = 300;
static uint8_t led_index = 0;
static uint32_t last_led_tick = 0;

static uint32_t last_dir_key_tick = 0;
static uint32_t last_pause_key_tick = 0;

static char tx_buf[64];
static volatile uint8_t tx_busy = 0;

static void Set_LEDs(uint8_t idx) {
    HAL_GPIO_WritePin(LED0_GPIO_Port, LED0_Pin, GPIO_PIN_RESET);
    HAL_GPIO_WritePin(LED1_GPIO_Port, LED1_Pin, GPIO_PIN_RESET);
    HAL_GPIO_WritePin(LED2_GPIO_Port, LED2_Pin, GPIO_PIN_RESET);
    if (idx == 0) HAL_GPIO_WritePin(LED0_GPIO_Port, LED0_Pin, GPIO_PIN_SET);
    else if (idx == 1) HAL_GPIO_WritePin(LED1_GPIO_Port, LED1_Pin, GPIO_PIN_SET);
    else HAL_GPIO_WritePin(LED2_GPIO_Port, LED2_Pin, GPIO_PIN_SET);
}

static void Update_OLED(void) {
    /* 不整屏清除，只覆盖固定宽度字段，刷新更快、不拖慢流水灯 */
    OLED_ShowString(0, 0, "STATE: ");
    OLED_ShowString(42, 0, run_flag ? "RUN  " : "PAUSE");
    OLED_ShowString(0, 2, "DIR: ");
    OLED_ShowString(30, 2, dir_flag ? "REV" : "FWD");
    OLED_ShowString(0, 4, "STEP: ");
    OLED_ShowNum(36, 4, step_ms, 4);
    OLED_ShowString(72, 4, "ms");
    OLED_ShowString(0, 6, "LED: ");
    OLED_ShowNum(30, 6, led_index + 1, 1);
}

/* 流水灯每走一步只刷新 LED 那一行，避免整屏重写拖慢回传 */
static void Update_OLED_LED(void) {
    OLED_ShowNum(30, 6, led_index + 1, 1);
}

static void Send_Feedback(void) {
    if (tx_busy) return;
    int n = snprintf(tx_buf, sizeof(tx_buf),
                     "STATE=%s,DIR=%s,STEP=%u,LED=%u\r\n",
                     run_flag ? "RUN" : "PAUSE",
                     dir_flag ? "REV" : "FWD",
                     (unsigned)step_ms,
                     (unsigned)(led_index + 1));
    if (n > 0 && n < (int)sizeof(tx_buf)) {
        tx_busy = 1;
        HAL_UART_Transmit_IT(&huart1, (uint8_t *)tx_buf, (uint16_t)n);
    }
}

void HAL_GPIO_EXTI_Callback(uint16_t GPIO_Pin) {
    uint32_t now = HAL_GetTick();
    if (GPIO_Pin == GPIO_PIN_11) {
        if (now - last_dir_key_tick > 200) {
            last_dir_key_tick = now;
            key_dir_event = 1;
        }
    } else if (GPIO_Pin == GPIO_PIN_12) {
        if (now - last_pause_key_tick > 200) {
            last_pause_key_tick = now;
            key_pause_event = 1;
        }
    }
}

void HAL_UART_RxCpltCallback(UART_HandleTypeDef *huart) {
    if (huart->Instance == USART1) {
        if (rx_index == 0 && rx_byte != 0xAA) {
            /* drop until frame header */
        } else {
            packet[rx_index++] = rx_byte;
            last_rx_tick = HAL_GetTick();
            if (rx_index >= 6) {
                rx_index = 0;
                packet_ready = 1;
            }
        }
        HAL_UART_Receive_IT(&huart1, (uint8_t *)&rx_byte, 1);
    }
}

void HAL_UART_TxCpltCallback(UART_HandleTypeDef *huart) {
    if (huart->Instance == USART1) tx_busy = 0;
}

static void App_GPIO_Init(void) {
    GPIO_InitTypeDef g = {0};

    __HAL_RCC_GPIOA_CLK_ENABLE();
    __HAL_RCC_GPIOB_CLK_ENABLE();

    /* Keys PA11 / PA12, falling edge, internal pull-up */
    g.Pin = GPIO_PIN_11 | GPIO_PIN_12;
    g.Mode = GPIO_MODE_IT_FALLING;
    g.Pull = GPIO_PULLUP;
    HAL_GPIO_Init(GPIOA, &g);

    HAL_NVIC_SetPriority(EXTI15_10_IRQn, 2, 0);
    HAL_NVIC_EnableIRQ(EXTI15_10_IRQn);

    /* USART1 PA9 TX, PA10 RX */
    g.Pin = GPIO_PIN_9;
    g.Mode = GPIO_MODE_AF_PP;
    g.Speed = GPIO_SPEED_FREQ_HIGH;
    HAL_GPIO_Init(GPIOA, &g);

    g.Pin = GPIO_PIN_10;
    g.Mode = GPIO_MODE_INPUT;
    g.Pull = GPIO_NOPULL;
    HAL_GPIO_Init(GPIOA, &g);

    __HAL_RCC_USART1_CLK_ENABLE();
    huart1.Instance = USART1;
    huart1.Init.BaudRate = 115200;
    huart1.Init.WordLength = UART_WORDLENGTH_8B;
    huart1.Init.StopBits = UART_STOPBITS_1;
    huart1.Init.Parity = UART_PARITY_NONE;
    huart1.Init.Mode = UART_MODE_TX_RX;
    huart1.Init.HwFlowCtl = UART_HWCONTROL_NONE;
    huart1.Init.OverSampling = UART_OVERSAMPLING_16;
    if (HAL_UART_Init(&huart1) != HAL_OK) {
        Error_Handler();
    }

    HAL_NVIC_SetPriority(USART1_IRQn, 1, 0);
    HAL_NVIC_EnableIRQ(USART1_IRQn);
}

void App_Init(void) {
    App_GPIO_Init();

    run_flag = 1;
    dir_flag = 0;
    step_ms = 300;
    led_index = 0;
    Set_LEDs(led_index);
    last_led_tick = HAL_GetTick();

    /* Arm UART RX before slow OLED init so boot-time frame is not lost */
    HAL_UART_Receive_IT(&huart1, (uint8_t *)&rx_byte, 1);
    Send_Feedback();

    OLED_Init();
    Update_OLED();
}

void App_Loop(void) {
    /* Drop a partial frame after 100ms so parser re-syncs on next header */
    if (rx_index != 0 && (HAL_GetTick() - last_rx_tick) > 100) {
        rx_index = 0;
    }

    if (key_dir_event) {
        key_dir_event = 0;
        dir_flag = !dir_flag;
        Send_Feedback();
        Update_OLED();
    }

    if (key_pause_event) {
        key_pause_event = 0;
        run_flag = !run_flag;
        Send_Feedback();
        Update_OLED();
    }

    if (packet_ready) {
        packet_ready = 0;
        uint8_t sum = (uint8_t)(packet[0] + packet[1] + packet[2] + packet[3] + packet[4]);
        uint16_t new_step = (uint16_t)(packet[3] | ((uint16_t)packet[4] << 8));
        if (packet[0] == 0xAA && sum == packet[5] &&
            packet[1] <= 1 && packet[2] <= 1 &&
            new_step >= 100 && new_step <= 2000) {
            run_flag = packet[1];
            dir_flag = packet[2];
            step_ms = new_step;
            Send_Feedback();
            Update_OLED();
        }
    }

    if (run_flag && (HAL_GetTick() - last_led_tick >= step_ms)) {
        last_led_tick += step_ms;
        if (dir_flag) led_index = (led_index + 2) % 3;
        else led_index = (led_index + 1) % 3;
        Set_LEDs(led_index);
        Update_OLED_LED();
    }
}
