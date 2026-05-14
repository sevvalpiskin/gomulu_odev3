/* USER CODE BEGIN Header */
/**
  * STM32F103C8T6 - Ödev Projesi
  * TIM2 Interrupt, Flash Read/Write, Button Debounce & Factory Reset
  */
/* USER CODE END Header */

/* Includes ------------------------------------------------------------------*/
#include "main.h"

/* Private variables ---------------------------------------------------------*/
TIM_HandleTypeDef htim2;

/* USER CODE BEGIN PV */
volatile uint16_t blink_count = 4;   /* ISR okur; volatile */
volatile uint16_t timer_counter = 0;
volatile uint8_t is_blinking = 1;
uint32_t flash_addr = 0x0800FC00; /* Son flash sayfası (64K: 0x0800FC00) */
static uint32_t s_next_button_ms = 0; /* Açılış gürültüsü / çift tetiklemeyi engelle */

// --- FLASH BELLEK FONKSİYONLARI ---
void Flash_Write(uint16_t data) {
    HAL_FLASH_Unlock();
    FLASH_EraseInitTypeDef EraseInitStruct;
    uint32_t PageError;

    EraseInitStruct.TypeErase = FLASH_TYPEERASE_PAGES;
    EraseInitStruct.PageAddress = flash_addr;
    EraseInitStruct.NbPages = 1;

    if (HAL_FLASHEx_Erase(&EraseInitStruct, &PageError) == HAL_OK) {
        HAL_FLASH_Program(FLASH_TYPEPROGRAM_HALFWORD, flash_addr, data);
    }
    HAL_FLASH_Lock();
}

uint16_t Flash_Read(uint32_t addr) {
    return *(__IO uint16_t*)addr;
}
/* USER CODE END PV */

/* Private function prototypes -----------------------------------------------*/
void SystemClock_Config(void);
static void MX_GPIO_Init(void);
static void MX_TIM2_Init(void);

/**
  * @brief  The application entry point.
  */
int main(void)
{
  HAL_Init();
  SystemClock_Config();
  MX_GPIO_Init();
  MX_TIM2_Init();

  /* USER CODE BEGIN 2 */
  // 1. Yazılım başladığında Flash'tan değeri yükle (e şıkkı)
  blink_count = Flash_Read(flash_addr);

  // 2. Geçersiz değer kontrolü (f şıkkı: 0xFFFF veya menzil dışı ise 4 yap)
  if (blink_count < 4 || blink_count > 7) {
      blink_count = 4;
      Flash_Write(blink_count);
  }

  /* g — fabrika 4: Yalnızca güç verilirken buton basılıyken 3 sn tutulunca */
  if (HAL_GPIO_ReadPin(GPIOA, GPIO_PIN_0) == GPIO_PIN_RESET) {
      uint32_t boot_press_t0 = HAL_GetTick();
      while (HAL_GPIO_ReadPin(GPIOA, GPIO_PIN_0) == GPIO_PIN_RESET) {
          if ((HAL_GetTick() - boot_press_t0) >= 3000U) {
              blink_count = 4;
              Flash_Write(blink_count);
              while (HAL_GPIO_ReadPin(GPIOA, GPIO_PIN_0) == GPIO_PIN_RESET) { }
              break;
          }
      }
  }

  /* TIM2: flash ve isteğe bağlı fabrika adımı bittikten sonra */
  HAL_TIM_Base_Start_IT(&htim2);
  s_next_button_ms = HAL_GetTick() + 400U;
  /* USER CODE END 2 */

  /* Infinite loop */
  /* USER CODE BEGIN WHILE */
  while (1)
  {
    /* Buton: güvenilir serbest = HIGH (pull-up); cooldown ile çift sayım yok */
    if (HAL_GetTick() >= s_next_button_ms
        && HAL_GPIO_ReadPin(GPIOA, GPIO_PIN_0) == GPIO_PIN_RESET)
    {
        uint32_t press_start = HAL_GetTick();

        /* Çalışırken: basılı kalma süresi önemli değil; bırakınca en fazla +1 (d) */
        while (HAL_GPIO_ReadPin(GPIOA, GPIO_PIN_0) == GPIO_PIN_RESET) { }

        uint32_t duration = HAL_GetTick() - press_start;
        /* Bırakma sıçraması geçsin */
        for (uint32_t t0 = HAL_GetTick(); (HAL_GetTick() - t0) < 30U; )
        {
            if (HAL_GPIO_ReadPin(GPIOA, GPIO_PIN_0) != GPIO_PIN_RESET)
            {
                break;
            }
        }

        if (HAL_GPIO_ReadPin(GPIOA, GPIO_PIN_0) == GPIO_PIN_RESET)
        {
            continue;
        }

        if (duration > 50U)
        {
            blink_count++;
            if (blink_count > 7)
            {
                blink_count = 4;
            }
            Flash_Write(blink_count);
            s_next_button_ms = HAL_GetTick() + 350U;
        }
        else
        {
            s_next_button_ms = HAL_GetTick() + 50U;
        }
    }
    /* USER CODE END WHILE */
  }
}

/* USER CODE BEGIN 4 */
/**
  * @brief  TIM2 Interrupt Handler (b ve c şıkları)
  */
void HAL_TIM_PeriodElapsedCallback(TIM_HandleTypeDef *htim) {
    if (htim->Instance == TIM2) {
        timer_counter++;

        if (is_blinking) {
            // c şıkkı: blink_count kadar yan-sön döngüsü
            // (Her yan-sön 2 saniye sürer: 1sn ON, 1sn OFF)
            if (timer_counter <= (blink_count * 2)) {
                HAL_GPIO_TogglePin(GPIOC, GPIO_PIN_13);
            } else {
                // Blink bitti, 5 saniye sönük kalma evresine geç
                is_blinking = 0;
                timer_counter = 0;
                HAL_GPIO_WritePin(GPIOC, GPIO_PIN_13, GPIO_PIN_SET); // LED Kapalı (PC13 Pull-up)
            }
        } else {
            // 5 saniye sönük durma kontrolü
            if (timer_counter >= 5) {
                is_blinking = 1;
                timer_counter = 0;
            }
        }
    }
}
/* USER CODE END 4 */

/**
  * @brief TIM2 Initialization Function (8 MHz HSI Clock için güncellendi)
  */
static void MX_TIM2_Init(void)
{
  TIM_ClockConfigTypeDef sClockSourceConfig = {0};
  TIM_MasterConfigTypeDef sMasterConfig = {0};

  htim2.Instance = TIM2;
  htim2.Init.Prescaler = 799;  // 8 MHz / 800 = 10,000 Hz
  htim2.Init.CounterMode = TIM_COUNTERMODE_UP;
  htim2.Init.Period = 9999;    // 10,000 / 10,000 = 1 Hz (1 Saniye)
  htim2.Init.ClockDivision = TIM_CLOCKDIVISION_DIV1;
  htim2.Init.AutoReloadPreload = TIM_AUTORELOAD_PRELOAD_ENABLE;
  HAL_TIM_Base_Init(&htim2);
  
  sClockSourceConfig.ClockSource = TIM_CLOCKSOURCE_INTERNAL;
  HAL_TIM_ConfigClockSource(&htim2, &sClockSourceConfig);
  
  sMasterConfig.MasterOutputTrigger = TIM_TRGO_RESET;
  sMasterConfig.MasterSlaveMode = TIM_MASTERSLAVEMODE_DISABLE;
  HAL_TIMEx_MasterConfigSynchronization(&htim2, &sMasterConfig);
}

/**
  * @brief GPIO Initialization Function
  */
static void MX_GPIO_Init(void)
{
  GPIO_InitTypeDef GPIO_InitStruct = {0};

  __HAL_RCC_GPIOC_CLK_ENABLE();
  __HAL_RCC_GPIOA_CLK_ENABLE();

  // PC13 LED (Default: OFF)
  HAL_GPIO_WritePin(GPIOC, GPIO_PIN_13, GPIO_PIN_SET);

  // PA1 Output (Default: Low - h şıkkı)
  HAL_GPIO_WritePin(GPIOA, GPIO_PIN_1, GPIO_PIN_RESET);

  // PC13 Konfigürasyonu
  GPIO_InitStruct.Pin = GPIO_PIN_13;
  GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
  HAL_GPIO_Init(GPIOC, &GPIO_InitStruct);

  // PA0 Input Konfigürasyonu (Buton)
  GPIO_InitStruct.Pin = GPIO_PIN_0;
  GPIO_InitStruct.Mode = GPIO_MODE_INPUT;
  /* İç pull-up: açılışta pin float LOW görünüp yanlışlıkla +1 flash yazılmasını önler */
  GPIO_InitStruct.Pull = GPIO_PULLUP;
  HAL_GPIO_Init(GPIOA, &GPIO_InitStruct);

  // PA1 Output Konfigürasyonu (Sabit 0)
  GPIO_InitStruct.Pin = GPIO_PIN_1;
  GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
  HAL_GPIO_Init(GPIOA, &GPIO_InitStruct);
}

/**
  * @brief System Clock Configuration (HSI 8 MHz — TIM2 ayarları bu frekansa göre)
  */
void SystemClock_Config(void)
{
  RCC_OscInitTypeDef RCC_OscInitStruct = {0};
  RCC_ClkInitTypeDef RCC_ClkInitStruct = {0};

  RCC_OscInitStruct.OscillatorType = RCC_OSCILLATORTYPE_HSI;
  RCC_OscInitStruct.HSIState = RCC_HSI_ON;
  RCC_OscInitStruct.HSICalibrationValue = RCC_HSICALIBRATION_DEFAULT;
  RCC_OscInitStruct.PLL.PLLState = RCC_PLL_NONE;
  if (HAL_RCC_OscConfig(&RCC_OscInitStruct) != HAL_OK)
  {
    Error_Handler();
  }

  RCC_ClkInitStruct.ClockType = RCC_CLOCKTYPE_HCLK | RCC_CLOCKTYPE_SYSCLK
                              | RCC_CLOCKTYPE_PCLK1 | RCC_CLOCKTYPE_PCLK2;
  RCC_ClkInitStruct.SYSCLKSource = RCC_SYSCLKSOURCE_HSI;
  RCC_ClkInitStruct.AHBCLKDivider = RCC_SYSCLK_DIV1;
  RCC_ClkInitStruct.APB1CLKDivider = RCC_HCLK_DIV1;
  RCC_ClkInitStruct.APB2CLKDivider = RCC_HCLK_DIV1;

  if (HAL_RCC_ClockConfig(&RCC_ClkInitStruct, FLASH_LATENCY_0) != HAL_OK)
  {
    Error_Handler();
  }
  SystemCoreClockUpdate();
}

void Error_Handler(void)
{
  __disable_irq();
  while (1) { }
}