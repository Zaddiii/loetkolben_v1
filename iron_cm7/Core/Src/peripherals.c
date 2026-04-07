#include "peripherals.h"

ADC_HandleTypeDef hadc1;
ADC_HandleTypeDef hadc2;
DAC_HandleTypeDef hdac1;
I2C_HandleTypeDef hi2c1;
I2C_HandleTypeDef hi2c3;
SPI_HandleTypeDef hspi1;
SPI_HandleTypeDef hspi2;
TIM_HandleTypeDef htim1;
TIM_HandleTypeDef htim3;
TIM_HandleTypeDef htim5;
TIM_HandleTypeDef htim7;
UART_HandleTypeDef huart3;

static void MX_ADC1_Init(void);
static void MX_ADC2_Init(void);
static void MX_DAC1_Init(void);
static void MX_I2C1_Init(void);
static void MX_I2C3_Init(void);
static void MX_SPI1_Init(void);
static void MX_SPI2_Init(void);
static void MX_TIM1_Init(void);
static void MX_TIM3_Init(void);
static void MX_TIM5_Init(void);
static void MX_TIM7_Init(void);
static void MX_USART3_UART_Init(void);

void MX_StationPeripherals_Init(void)
{
  MX_ADC1_Init();
  MX_ADC2_Init();
  MX_DAC1_Init();
  MX_I2C1_Init();
  MX_I2C3_Init();
  MX_SPI1_Init();
  MX_SPI2_Init();
  MX_TIM1_Init();
  MX_TIM3_Init();
  MX_TIM5_Init();
  MX_TIM7_Init();
  MX_USART3_UART_Init();
}

static void MX_ADC1_Init(void)
{
  ADC_ChannelConfTypeDef channel_config = {0};
  ADC_MultiModeTypeDef multi_mode_config = {0};
  RCC_PeriphCLKInitTypeDef peripheral_clock_init = {0};

  peripheral_clock_init.PeriphClockSelection = RCC_PERIPHCLK_ADC;
  peripheral_clock_init.AdcClockSelection = RCC_ADCCLKSOURCE_CLKP;

  if (HAL_RCCEx_PeriphCLKConfig(&peripheral_clock_init) != HAL_OK)
  {
    Error_Handler();
  }

  hadc1.Instance = ADC1;
  hadc1.Init.ClockPrescaler = ADC_CLOCK_ASYNC_DIV4;
  hadc1.Init.Resolution = ADC_RESOLUTION_16B;
  hadc1.Init.ScanConvMode = ADC_SCAN_DISABLE;
  hadc1.Init.EOCSelection = ADC_EOC_SINGLE_CONV;
  hadc1.Init.LowPowerAutoWait = DISABLE;
  hadc1.Init.ContinuousConvMode = DISABLE;
  hadc1.Init.NbrOfConversion = 1;
  hadc1.Init.DiscontinuousConvMode = DISABLE;
  hadc1.Init.NbrOfDiscConversion = 0;
  hadc1.Init.ExternalTrigConv = ADC_SOFTWARE_START;
  hadc1.Init.ExternalTrigConvEdge = ADC_EXTERNALTRIGCONVEDGE_NONE;
  hadc1.Init.ConversionDataManagement = ADC_CONVERSIONDATA_DR;
  hadc1.Init.Overrun = ADC_OVR_DATA_OVERWRITTEN;
  hadc1.Init.LeftBitShift = ADC_LEFTBITSHIFT_NONE;
  hadc1.Init.OversamplingMode = DISABLE;

  if (HAL_ADC_Init(&hadc1) != HAL_OK)
  {
    Error_Handler();
  }

  multi_mode_config.Mode = ADC_MODE_INDEPENDENT;
  if (HAL_ADCEx_MultiModeConfigChannel(&hadc1, &multi_mode_config) != HAL_OK)
  {
    Error_Handler();
  }

  channel_config.Channel = ADC_CHANNEL_2;
  channel_config.Rank = ADC_REGULAR_RANK_1;
  channel_config.SamplingTime = ADC_SAMPLETIME_32CYCLES_5;
  channel_config.SingleDiff = ADC_SINGLE_ENDED;
  channel_config.OffsetNumber = ADC_OFFSET_NONE;
  channel_config.Offset = 0;
  channel_config.OffsetRightShift = DISABLE;
  channel_config.OffsetSignedSaturation = DISABLE;

  if (HAL_ADC_ConfigChannel(&hadc1, &channel_config) != HAL_OK)
  {
    Error_Handler();
  }

  if (HAL_ADCEx_Calibration_Start(&hadc1, ADC_CALIB_OFFSET_LINEARITY, ADC_SINGLE_ENDED) != HAL_OK)
  {
    Error_Handler();
  }
}

static void MX_ADC2_Init(void)
{
  ADC_ChannelConfTypeDef channel_config = {0};
  RCC_PeriphCLKInitTypeDef peripheral_clock_init = {0};

  peripheral_clock_init.PeriphClockSelection = RCC_PERIPHCLK_ADC;
  peripheral_clock_init.AdcClockSelection = RCC_ADCCLKSOURCE_CLKP;

  if (HAL_RCCEx_PeriphCLKConfig(&peripheral_clock_init) != HAL_OK)
  {
    Error_Handler();
  }

  hadc2.Instance = ADC2;
  hadc2.Init.ClockPrescaler = ADC_CLOCK_ASYNC_DIV4;
  hadc2.Init.Resolution = ADC_RESOLUTION_16B;
  hadc2.Init.ScanConvMode = ADC_SCAN_ENABLE;
  hadc2.Init.EOCSelection = ADC_EOC_SINGLE_CONV;
  hadc2.Init.LowPowerAutoWait = DISABLE;
  hadc2.Init.ContinuousConvMode = DISABLE;
  hadc2.Init.NbrOfConversion = 2;
  hadc2.Init.DiscontinuousConvMode = DISABLE;
  hadc2.Init.NbrOfDiscConversion = 0;
  hadc2.Init.ExternalTrigConv = ADC_SOFTWARE_START;
  hadc2.Init.ExternalTrigConvEdge = ADC_EXTERNALTRIGCONVEDGE_NONE;
  hadc2.Init.ConversionDataManagement = ADC_CONVERSIONDATA_DR;
  hadc2.Init.Overrun = ADC_OVR_DATA_OVERWRITTEN;
  hadc2.Init.LeftBitShift = ADC_LEFTBITSHIFT_NONE;
  hadc2.Init.OversamplingMode = DISABLE;

  if (HAL_ADC_Init(&hadc2) != HAL_OK)
  {
    Error_Handler();
  }

  channel_config.Channel = ADC_CHANNEL_3;
  channel_config.Rank = ADC_REGULAR_RANK_1;
  channel_config.SamplingTime = ADC_SAMPLETIME_32CYCLES_5;
  channel_config.SingleDiff = ADC_SINGLE_ENDED;
  channel_config.OffsetNumber = ADC_OFFSET_NONE;
  channel_config.Offset = 0;
  channel_config.OffsetRightShift = DISABLE;
  channel_config.OffsetSignedSaturation = DISABLE;

  if (HAL_ADC_ConfigChannel(&hadc2, &channel_config) != HAL_OK)
  {
    Error_Handler();
  }

  channel_config.Channel = ADC_CHANNEL_4;
  channel_config.Rank = ADC_REGULAR_RANK_2;

  if (HAL_ADC_ConfigChannel(&hadc2, &channel_config) != HAL_OK)
  {
    Error_Handler();
  }

  if (HAL_ADCEx_Calibration_Start(&hadc2, ADC_CALIB_OFFSET_LINEARITY, ADC_SINGLE_ENDED) != HAL_OK)
  {
    Error_Handler();
  }
}

static void MX_DAC1_Init(void)
{
  DAC_ChannelConfTypeDef channel_config = {0};

  hdac1.Instance = DAC1;

  if (HAL_DAC_Init(&hdac1) != HAL_OK)
  {
    Error_Handler();
  }

  channel_config.DAC_SampleAndHold = DAC_SAMPLEANDHOLD_DISABLE;
  channel_config.DAC_Trigger = DAC_TRIGGER_NONE;
  channel_config.DAC_OutputBuffer = DAC_OUTPUTBUFFER_ENABLE;
  channel_config.DAC_ConnectOnChipPeripheral = DAC_CHIPCONNECT_DISABLE;
  channel_config.DAC_UserTrimming = DAC_TRIMMING_FACTORY;

  if (HAL_DAC_ConfigChannel(&hdac1, &channel_config, DAC_CHANNEL_1) != HAL_OK)
  {
    Error_Handler();
  }

  if (HAL_DAC_Start(&hdac1, DAC_CHANNEL_1) != HAL_OK)
  {
    Error_Handler();
  }

  if (HAL_DAC_SetValue(&hdac1, DAC_CHANNEL_1, DAC_ALIGN_12B_R, 0U) != HAL_OK)
  {
    Error_Handler();
  }
}

static void MX_I2C1_Init(void)
{
  hi2c1.Instance = I2C1;
  hi2c1.Init.Timing = 0x10C0ECFF;
  hi2c1.Init.OwnAddress1 = 0;
  hi2c1.Init.AddressingMode = I2C_ADDRESSINGMODE_7BIT;
  hi2c1.Init.DualAddressMode = I2C_DUALADDRESS_DISABLE;
  hi2c1.Init.OwnAddress2 = 0;
  hi2c1.Init.OwnAddress2Masks = I2C_OA2_NOMASK;
  hi2c1.Init.GeneralCallMode = I2C_GENERALCALL_DISABLE;
  hi2c1.Init.NoStretchMode = I2C_NOSTRETCH_DISABLE;

  if (HAL_I2C_Init(&hi2c1) != HAL_OK)
  {
    Error_Handler();
  }

  if (HAL_I2CEx_ConfigAnalogFilter(&hi2c1, I2C_ANALOGFILTER_ENABLE) != HAL_OK)
  {
    Error_Handler();
  }

  if (HAL_I2CEx_ConfigDigitalFilter(&hi2c1, 0) != HAL_OK)
  {
    Error_Handler();
  }
}

static void MX_I2C3_Init(void)
{
  hi2c3.Instance = I2C3;
  hi2c3.Init.Timing = 0x10C0ECFF;
  hi2c3.Init.OwnAddress1 = 0;
  hi2c3.Init.AddressingMode = I2C_ADDRESSINGMODE_7BIT;
  hi2c3.Init.DualAddressMode = I2C_DUALADDRESS_DISABLE;
  hi2c3.Init.OwnAddress2 = 0;
  hi2c3.Init.OwnAddress2Masks = I2C_OA2_NOMASK;
  hi2c3.Init.GeneralCallMode = I2C_GENERALCALL_DISABLE;
  hi2c3.Init.NoStretchMode = I2C_NOSTRETCH_DISABLE;

  if (HAL_I2C_Init(&hi2c3) != HAL_OK)
  {
    Error_Handler();
  }

  if (HAL_I2CEx_ConfigAnalogFilter(&hi2c3, I2C_ANALOGFILTER_ENABLE) != HAL_OK)
  {
    Error_Handler();
  }

  if (HAL_I2CEx_ConfigDigitalFilter(&hi2c3, 0) != HAL_OK)
  {
    Error_Handler();
  }
}

static void MX_SPI1_Init(void)
{
  RCC_PeriphCLKInitTypeDef peripheral_clock_init = {0};

  peripheral_clock_init.PeriphClockSelection = RCC_PERIPHCLK_SPI123;
  peripheral_clock_init.Spi123ClockSelection = RCC_SPI123CLKSOURCE_CLKP;

  if (HAL_RCCEx_PeriphCLKConfig(&peripheral_clock_init) != HAL_OK)
  {
    Error_Handler();
  }

  hspi1.Instance = SPI1;
  hspi1.Init.Mode = SPI_MODE_MASTER;
  hspi1.Init.Direction = SPI_DIRECTION_2LINES;
  hspi1.Init.DataSize = SPI_DATASIZE_8BIT;
  hspi1.Init.CLKPolarity = SPI_POLARITY_LOW;
  hspi1.Init.CLKPhase = SPI_PHASE_1EDGE;
  hspi1.Init.NSS = SPI_NSS_SOFT;
  hspi1.Init.BaudRatePrescaler = SPI_BAUDRATEPRESCALER_16;
  hspi1.Init.FirstBit = SPI_FIRSTBIT_MSB;
  hspi1.Init.TIMode = SPI_TIMODE_DISABLE;
  hspi1.Init.CRCCalculation = SPI_CRCCALCULATION_DISABLE;
  hspi1.Init.CRCPolynomial = 7;
  hspi1.Init.CRCLength = SPI_CRC_LENGTH_DATASIZE;
  hspi1.Init.NSSPMode = SPI_NSS_PULSE_DISABLE;
  hspi1.Init.NSSPolarity = SPI_NSS_POLARITY_LOW;
  hspi1.Init.FifoThreshold = SPI_FIFO_THRESHOLD_01DATA;
  hspi1.Init.TxCRCInitializationPattern = SPI_CRC_INITIALIZATION_ALL_ZERO_PATTERN;
  hspi1.Init.RxCRCInitializationPattern = SPI_CRC_INITIALIZATION_ALL_ZERO_PATTERN;
  hspi1.Init.MasterSSIdleness = SPI_MASTER_SS_IDLENESS_00CYCLE;
  hspi1.Init.MasterInterDataIdleness = SPI_MASTER_INTERDATA_IDLENESS_00CYCLE;
  hspi1.Init.MasterReceiverAutoSusp = SPI_MASTER_RX_AUTOSUSP_DISABLE;
  hspi1.Init.MasterKeepIOState = SPI_MASTER_KEEP_IO_STATE_DISABLE;
  hspi1.Init.IOSwap = SPI_IO_SWAP_DISABLE;

  if (HAL_SPI_Init(&hspi1) != HAL_OK)
  {
    Error_Handler();
  }
}

static void MX_SPI2_Init(void)
{
  RCC_PeriphCLKInitTypeDef peripheral_clock_init = {0};

  peripheral_clock_init.PeriphClockSelection = RCC_PERIPHCLK_SPI123;
  peripheral_clock_init.Spi123ClockSelection = RCC_SPI123CLKSOURCE_CLKP;

  if (HAL_RCCEx_PeriphCLKConfig(&peripheral_clock_init) != HAL_OK)
  {
    Error_Handler();
  }

  hspi2.Instance = SPI2;
  hspi2.Init.Mode = SPI_MODE_MASTER;
  hspi2.Init.Direction = SPI_DIRECTION_2LINES;
  hspi2.Init.DataSize = SPI_DATASIZE_8BIT;
  hspi2.Init.CLKPolarity = SPI_POLARITY_LOW;
  hspi2.Init.CLKPhase = SPI_PHASE_1EDGE;
  hspi2.Init.NSS = SPI_NSS_SOFT;
  hspi2.Init.BaudRatePrescaler = SPI_BAUDRATEPRESCALER_8;
  hspi2.Init.FirstBit = SPI_FIRSTBIT_MSB;
  hspi2.Init.TIMode = SPI_TIMODE_DISABLE;
  hspi2.Init.CRCCalculation = SPI_CRCCALCULATION_DISABLE;
  hspi2.Init.CRCPolynomial = 7;
  hspi2.Init.CRCLength = SPI_CRC_LENGTH_DATASIZE;
  hspi2.Init.NSSPMode = SPI_NSS_PULSE_DISABLE;
  hspi2.Init.NSSPolarity = SPI_NSS_POLARITY_LOW;
  hspi2.Init.FifoThreshold = SPI_FIFO_THRESHOLD_01DATA;
  hspi2.Init.TxCRCInitializationPattern = SPI_CRC_INITIALIZATION_ALL_ZERO_PATTERN;
  hspi2.Init.RxCRCInitializationPattern = SPI_CRC_INITIALIZATION_ALL_ZERO_PATTERN;
  hspi2.Init.MasterSSIdleness = SPI_MASTER_SS_IDLENESS_00CYCLE;
  hspi2.Init.MasterInterDataIdleness = SPI_MASTER_INTERDATA_IDLENESS_00CYCLE;
  hspi2.Init.MasterReceiverAutoSusp = SPI_MASTER_RX_AUTOSUSP_DISABLE;
  hspi2.Init.MasterKeepIOState = SPI_MASTER_KEEP_IO_STATE_DISABLE;
  hspi2.Init.IOSwap = SPI_IO_SWAP_DISABLE;

  if (HAL_SPI_Init(&hspi2) != HAL_OK)
  {
    Error_Handler();
  }
}

static void MX_TIM1_Init(void)
{
  TIM_OC_InitTypeDef channel_config = {0};
  TIM_BreakDeadTimeConfigTypeDef dead_time_config = {0};

  htim1.Instance = TIM1;
  htim1.Init.Prescaler = 0;
  htim1.Init.CounterMode = TIM_COUNTERMODE_UP;
  htim1.Init.Period = 3199;
  htim1.Init.ClockDivision = TIM_CLOCKDIVISION_DIV1;
  htim1.Init.RepetitionCounter = 0;
  htim1.Init.AutoReloadPreload = TIM_AUTORELOAD_PRELOAD_DISABLE;

  if (HAL_TIM_PWM_Init(&htim1) != HAL_OK)
  {
    Error_Handler();
  }

  channel_config.OCMode = TIM_OCMODE_PWM1;
  channel_config.Pulse = 0;
  channel_config.OCPolarity = TIM_OCPOLARITY_HIGH;
  channel_config.OCNPolarity = TIM_OCNPOLARITY_HIGH;
  channel_config.OCFastMode = TIM_OCFAST_DISABLE;
  channel_config.OCIdleState = TIM_OCIDLESTATE_RESET;
  channel_config.OCNIdleState = TIM_OCNIDLESTATE_RESET;

  if (HAL_TIM_PWM_ConfigChannel(&htim1, &channel_config, TIM_CHANNEL_1) != HAL_OK)
  {
    Error_Handler();
  }

  dead_time_config.OffStateRunMode = TIM_OSSR_DISABLE;
  dead_time_config.OffStateIDLEMode = TIM_OSSI_DISABLE;
  dead_time_config.LockLevel = TIM_LOCKLEVEL_OFF;
  dead_time_config.DeadTime = 0;
  dead_time_config.BreakState = TIM_BREAK_DISABLE;
  dead_time_config.BreakPolarity = TIM_BREAKPOLARITY_HIGH;
  dead_time_config.BreakFilter = 0;
  dead_time_config.Break2State = TIM_BREAK2_DISABLE;
  dead_time_config.Break2Polarity = TIM_BREAK2POLARITY_HIGH;
  dead_time_config.Break2Filter = 0;
  dead_time_config.AutomaticOutput = TIM_AUTOMATICOUTPUT_DISABLE;

  if (HAL_TIMEx_ConfigBreakDeadTime(&htim1, &dead_time_config) != HAL_OK)
  {
    Error_Handler();
  }

  HAL_TIM_MspPostInit(&htim1);
}

static void MX_TIM3_Init(void)
{
  TIM_OC_InitTypeDef channel_config = {0};

  htim3.Instance = TIM3;
  htim3.Init.Prescaler = 63;
  htim3.Init.CounterMode = TIM_COUNTERMODE_UP;
  htim3.Init.Period = 999;
  htim3.Init.ClockDivision = TIM_CLOCKDIVISION_DIV1;
  htim3.Init.AutoReloadPreload = TIM_AUTORELOAD_PRELOAD_DISABLE;

  if (HAL_TIM_PWM_Init(&htim3) != HAL_OK)
  {
    Error_Handler();
  }

  channel_config.OCMode = TIM_OCMODE_PWM1;
  channel_config.Pulse = 0;
  channel_config.OCPolarity = TIM_OCPOLARITY_HIGH;
  channel_config.OCFastMode = TIM_OCFAST_DISABLE;

  if (HAL_TIM_PWM_ConfigChannel(&htim3, &channel_config, TIM_CHANNEL_2) != HAL_OK)
  {
    Error_Handler();
  }

  HAL_TIM_MspPostInit(&htim3);
}

static void MX_TIM5_Init(void)
{
  TIM_Encoder_InitTypeDef encoder_config = {0};
  TIM_MasterConfigTypeDef master_config = {0};

  htim5.Instance = TIM8;
  htim5.Init.Prescaler = 0;
  htim5.Init.CounterMode = TIM_COUNTERMODE_UP;
  htim5.Init.Period = 0xFFFF;
  htim5.Init.ClockDivision = TIM_CLOCKDIVISION_DIV1;
  htim5.Init.AutoReloadPreload = TIM_AUTORELOAD_PRELOAD_DISABLE;

  encoder_config.EncoderMode = TIM_ENCODERMODE_TI12;
  encoder_config.IC1Polarity = TIM_ICPOLARITY_RISING;
  encoder_config.IC1Selection = TIM_ICSELECTION_DIRECTTI;
  encoder_config.IC1Prescaler = TIM_ICPSC_DIV1;
  encoder_config.IC1Filter = 10;
  encoder_config.IC2Polarity = TIM_ICPOLARITY_RISING;
  encoder_config.IC2Selection = TIM_ICSELECTION_DIRECTTI;
  encoder_config.IC2Prescaler = TIM_ICPSC_DIV1;
  encoder_config.IC2Filter = 10;

  if (HAL_TIM_Encoder_Init(&htim5, &encoder_config) != HAL_OK)
  {
    Error_Handler();
  }

  master_config.MasterOutputTrigger = TIM_TRGO_RESET;
  master_config.MasterOutputTrigger2 = TIM_TRGO2_RESET;
  master_config.MasterSlaveMode = TIM_MASTERSLAVEMODE_DISABLE;

  if (HAL_TIMEx_MasterConfigSynchronization(&htim5, &master_config) != HAL_OK)
  {
    Error_Handler();
  }
}

static void MX_TIM7_Init(void)
{
  TIM_MasterConfigTypeDef master_config = {0};

  htim7.Instance = TIM7;
  htim7.Init.Prescaler = 63;
  htim7.Init.CounterMode = TIM_COUNTERMODE_UP;
  htim7.Init.Period = 0xFFFF;
  htim7.Init.AutoReloadPreload = TIM_AUTORELOAD_PRELOAD_DISABLE;

  if (HAL_TIM_Base_Init(&htim7) != HAL_OK)
  {
    Error_Handler();
  }

  master_config.MasterOutputTrigger = TIM_TRGO_RESET;
  master_config.MasterSlaveMode = TIM_MASTERSLAVEMODE_DISABLE;

  if (HAL_TIMEx_MasterConfigSynchronization(&htim7, &master_config) != HAL_OK)
  {
    Error_Handler();
  }
}

static void MX_USART3_UART_Init(void)
{
  RCC_PeriphCLKInitTypeDef peripheral_clock_init = {0};

  peripheral_clock_init.PeriphClockSelection = RCC_PERIPHCLK_USART3;
  peripheral_clock_init.Usart234578ClockSelection = RCC_USART3CLKSOURCE_D2PCLK1;

  if (HAL_RCCEx_PeriphCLKConfig(&peripheral_clock_init) != HAL_OK)
  {
    Error_Handler();
  }

  huart3.Instance = USART3;
  huart3.Init.BaudRate = 115200;
  huart3.Init.WordLength = UART_WORDLENGTH_8B;
  huart3.Init.StopBits = UART_STOPBITS_1;
  huart3.Init.Parity = UART_PARITY_NONE;
  huart3.Init.Mode = UART_MODE_TX_RX;
  huart3.Init.HwFlowCtl = UART_HWCONTROL_NONE;
  huart3.Init.OverSampling = UART_OVERSAMPLING_16;
  huart3.Init.OneBitSampling = UART_ONE_BIT_SAMPLE_DISABLE;
  huart3.Init.ClockPrescaler = UART_PRESCALER_DIV1;
  huart3.AdvancedInit.AdvFeatureInit = UART_ADVFEATURE_NO_INIT;

  if (HAL_UART_Init(&huart3) != HAL_OK)
  {
    Error_Handler();
  }

  if (HAL_UARTEx_SetTxFifoThreshold(&huart3, UART_TXFIFO_THRESHOLD_1_8) != HAL_OK)
  {
    Error_Handler();
  }

  if (HAL_UARTEx_SetRxFifoThreshold(&huart3, UART_RXFIFO_THRESHOLD_1_8) != HAL_OK)
  {
    Error_Handler();
  }

  if (HAL_UARTEx_DisableFifoMode(&huart3) != HAL_OK)
  {
    Error_Handler();
  }
}