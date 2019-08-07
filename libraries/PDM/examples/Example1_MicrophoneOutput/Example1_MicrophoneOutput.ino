/* Author: Nathan Seidle
  Created: July 24, 2019
  License: MIT. See SparkFun Arduino Apollo3 Project for more information

  This example demonstrates how to use the PDM microphone on Artemis boards.
*/

#define ARM_MATH_CM4
#include <arm_math.h>

//*****************************************************************************
//
// Example parameters.
//
//*****************************************************************************
#define PDM_FFT_SIZE                4096
//uint32_t pdmDataBufferSize = 4096;
#define pdmDataBufferSize 4096

//#define PDM_FFT_BYTES               (PDM_FFT_SIZE * 2)
#define PRINT_PDM_DATA              0
#define PRINT_FFT_DATA              0

//*****************************************************************************
//
// Global variables.
//
//*****************************************************************************
//uint32_t g_ui32PDMDataBuffer[pdmDataBufferSize];
uint32_t pdmDataBuffer[pdmDataBufferSize];
float g_fPDMTimeDomain[pdmDataBufferSize * 2];
float g_fPDMFrequencyDomain[pdmDataBufferSize * 2];
float g_fPDMMagnitudes[pdmDataBufferSize * 2];

uint32_t sampleFreq;

#include <PDM.h> //Include PDM library included with the Aruino_Apollo3 core

AP3_PDM myPDM;

void setup()
{
  Serial.begin(9600);
  Serial.println("SparkFun PDM Example");

  // Turn on the PDM, set it up for our chosen recording settings, and start
  // the first DMA transaction.
  if (myPDM.begin(22, 23) == false) //Data, clock - These are the pin names from variant file, not pad names
  {
    Serial.println("PDM Init failed. Are you sure these pins are PDM capable?");
    while (1);
  }
  Serial.println("PDM Initialized");

  //myPDM.setClockSpeed(AM_HAL_PDM_CLK_3MHZ);
  //myPDM.setClockDivider(AM_HAL_PDM_MCLKDIV_1);
  //myPDM.setGain(AM_HAL_PDM_GAIN_P210DB);
  //myPDM.setChannel(AM_HAL_PDM_CHANNEL_STEREO);

  pdm_config_print();
  am_hal_pdm_fifo_flush(myPDM._PDMhandle);
  myPDM.getData(pdmDataBuffer, pdmDataBufferSize);
}

void loop()
{
  am_hal_interrupt_master_disable();

  if (myPDM.available())
  {
    pcm_fft_print();

    while (PRINT_PDM_DATA || PRINT_FFT_DATA);

    // Start converting the next set of PCM samples.
    myPDM.getData(pdmDataBuffer, pdmDataBufferSize);
  }

  // Go to Deep Sleep.
  am_hal_sysctrl_sleep(AM_HAL_SYSCTRL_SLEEP_DEEP);

  am_hal_interrupt_master_enable();
}

//*****************************************************************************
//
// Analyze and print frequency data.
//
//*****************************************************************************
void
pcm_fft_print(void)
{
  float fMaxValue;
  uint32_t ui32MaxIndex;
  int16_t *pi16PDMData = (int16_t *) pdmDataBuffer;
  uint32_t ui32LoudestFrequency;

  //
  // Convert the PDM samples to floats, and arrange them in the format
  // required by the FFT function.
  //
  for (uint32_t i = 0; i < PDM_FFT_SIZE; i++)
  {
    if (PRINT_PDM_DATA)
    {
      Serial.printf("%d\n", pi16PDMData[i]);
    }

    g_fPDMTimeDomain[2 * i] = pi16PDMData[i] / 1.0;
    g_fPDMTimeDomain[2 * i + 1] = 0.0;
  }

  if (PRINT_PDM_DATA)
  {
    Serial.printf("END\n");
  }

  //
  // Perform the FFT.
  //
  arm_cfft_radix4_instance_f32 S;
  arm_cfft_radix4_init_f32(&S, PDM_FFT_SIZE, 0, 1);
  arm_cfft_radix4_f32(&S, g_fPDMTimeDomain);
  arm_cmplx_mag_f32(g_fPDMTimeDomain, g_fPDMMagnitudes, PDM_FFT_SIZE);

  if (PRINT_FFT_DATA)
  {
    for (uint32_t i = 0; i < PDM_FFT_SIZE / 2; i++)
    {
      Serial.printf("%f\n", g_fPDMMagnitudes[i]);
    }

    Serial.printf("END\n");
  }

  //
  // Find the frequency bin with the largest magnitude.
  //
  arm_max_f32(g_fPDMMagnitudes, PDM_FFT_SIZE / 2, &fMaxValue, &ui32MaxIndex);

  ui32LoudestFrequency = (sampleFreq * ui32MaxIndex) / PDM_FFT_SIZE;

  if (PRINT_FFT_DATA)
  {
    Serial.printf("Loudest frequency bin: %d\n", ui32MaxIndex);
  }

  Serial.printf("Loudest frequency: %d         \n", ui32LoudestFrequency);
}

//*****************************************************************************
//
// Print PDM configuration data.
//
//*****************************************************************************
void
pdm_config_print(void)
{
  uint32_t PDMClk;
  uint32_t MClkDiv;
  float frequencyUnits;

  //
  // Read the config structure to figure out what our internal clock is set
  // to.
  //
  switch (myPDM.getClockDivider())
  {
    case AM_HAL_PDM_MCLKDIV_4: MClkDiv = 4; break;
    case AM_HAL_PDM_MCLKDIV_3: MClkDiv = 3; break;
    case AM_HAL_PDM_MCLKDIV_2: MClkDiv = 2; break;
    case AM_HAL_PDM_MCLKDIV_1: MClkDiv = 1; break;

    default:
      MClkDiv = 0;
  }

  switch (myPDM.getClockSpeed())
  {
    case AM_HAL_PDM_CLK_12MHZ:  PDMClk = 12000000; break;
    case AM_HAL_PDM_CLK_6MHZ:   PDMClk =  6000000; break;
    case AM_HAL_PDM_CLK_3MHZ:   PDMClk =  3000000; break;
    case AM_HAL_PDM_CLK_1_5MHZ: PDMClk =  1500000; break;
    case AM_HAL_PDM_CLK_750KHZ: PDMClk =   750000; break;
    case AM_HAL_PDM_CLK_375KHZ: PDMClk =   375000; break;
    case AM_HAL_PDM_CLK_187KHZ: PDMClk =   187000; break;

    default:
      PDMClk = 0;
  }

  //
  // Record the effective sample frequency. We'll need it later to print the
  // loudest frequency from the sample.
  //
  sampleFreq = (PDMClk / (MClkDiv * 2 * myPDM.getDecimationRate()));

  frequencyUnits = (float) sampleFreq / (float) PDM_FFT_SIZE;

  Serial.printf("Settings:\n");
  Serial.printf("PDM Clock (Hz):         %12d\n", PDMClk);
  Serial.printf("Decimation Rate:        %12d\n", myPDM.getDecimationRate());
  Serial.printf("Effective Sample Freq.: %12d\n", sampleFreq);
  Serial.printf("FFT Length:             %12d\n\n", PDM_FFT_SIZE);
  Serial.printf("FFT Resolution: %15.3f Hz\n", frequencyUnits);
}
