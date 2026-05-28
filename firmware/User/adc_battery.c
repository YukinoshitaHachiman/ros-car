#include "adc_battery.h"

// PC4 = ADC1_IN14
// 分压: R24=10k, R29=3.3k → V_adc = V_bat * 3.3/(10+3.3)
// V_bat = V_adc * 13.3 / 3.3

void adc_battery_init(void)
{
    GPIO_InitTypeDef  gpio;
    ADC_InitTypeDef   adc;

    RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOC | RCC_APB2Periph_ADC1, ENABLE);

    // PC4 analog input
    gpio.GPIO_Pin  = GPIO_Pin_4;
    gpio.GPIO_Mode = GPIO_Mode_AIN;
    GPIO_Init(GPIOC, &gpio);

    // ADC1 config
    RCC_ADCCLKConfig(RCC_PCLK2_Div6);       // 72/6=12MHz (ADC max 14MHz)
    adc.ADC_Mode                      = ADC_Mode_Independent;
    adc.ADC_ScanConvMode              = DISABLE;
    adc.ADC_ContinuousConvMode        = DISABLE;
    adc.ADC_ExternalTrigConv          = ADC_ExternalTrigConv_None;
    adc.ADC_DataAlign                 = ADC_DataAlign_Right;
    adc.ADC_NbrOfChannel              = 1;
    ADC_Init(ADC1, &adc);

    ADC_RegularChannelConfig(ADC1, ADC_Channel_14, 1, ADC_SampleTime_55Cycles5);

    ADC_Cmd(ADC1, ENABLE);
    ADC_ResetCalibration(ADC1);
    while (ADC_GetResetCalibrationStatus(ADC1));
    ADC_StartCalibration(ADC1);
    while (ADC_GetCalibrationStatus(ADC1));
}

uint16_t adc_battery_read_raw(void)
{
    ADC_SoftwareStartConvCmd(ADC1, ENABLE);
    while (!ADC_GetFlagStatus(ADC1, ADC_FLAG_EOC));
    return ADC_GetConversionValue(ADC1);
}

uint16_t adc_battery_read_mv(void)
{
    // V_adc = raw * 3300 / 4096
    // V_bat = V_adc * 13300 / 3300 = raw * 13300 / 4096
    //       = raw * 3.247... ≈ raw * 3325 / 1024
    uint32_t mv = (uint32_t)adc_battery_read_raw() * 13300 / 4096;
    return (uint16_t)mv;
}
