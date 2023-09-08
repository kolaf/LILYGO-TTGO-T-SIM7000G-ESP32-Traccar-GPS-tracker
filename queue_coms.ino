#include "sdkconfig.h"
#include "esp32/ulp.h"
#include "driver/rtc_io.h"
#include "esp_system.h" //This inclusion configures the peripherals in the ESP system.
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/timers.h"
#include "freertos/event_groups.h"
#include <Adafruit_NeoPixel.h>
#include "AudioAnalyzer.h"
////
/* define event group and event bits */
EventGroupHandle_t eg;
#define evtDo_AudioReadFreq       ( 1 << 0 ) // 1
////
TickType_t xTicksToWait0 = 0;
////
QueueHandle_t xQ_LED_Info;
////
const int NeoPixelPin = 26;
const int LED_COUNT = 24; //total number of leds in the strip
const int NOISE = 0; // noise that you want to chop off
const int SEG = 6; // how many parts you want to separate the led strip into
const int Priority4 = 4;
const int TaskStack40K = 40000;
const int TaskCore1  = 1;
const int TaskCore0 = 0;
const int AudioSampleSize = 6;
const int Brightness = 180;
const int A_D_ConversionBits = 4096; // arduino use 1024, ESP32 use 4096
const float LED0_Multiplier = 2.0;
////
Analyzer Audio = Analyzer( 5, 15, 36 );//Strobe pin ->15  RST pin ->2 Analog Pin ->36
// When we setup the NeoPixel library, we tell it how many pixels, and which pin to use to send signals.
Adafruit_NeoPixel leds = Adafruit_NeoPixel( LED_COUNT, NeoPixelPin, NEO_GRB + NEO_KHZ800 );
////
int FreqVal[7];//create an array to store the value of different freq
////
void ULP_BLINK_RUN(uint32_t us);
////
void setup()
{
  ULP_BLINK_RUN(100000);
  eg = xEventGroupCreate();
  Audio.Init(); // start the audio analyzer
  leds.begin(); // Call this to start up the LED strip.
  clearLEDs();  // This function, defined below, de-energizes all LEDs...
  leds.show();  // ...but the LEDs don't actually update until you call this.
  ////
  xQ_LED_Info = xQueueCreate ( 1, sizeof(FreqVal) );
  //////////////////////////////////////////////////////////////////////////////////////////////
  xTaskCreatePinnedToCore( fDo_AudioReadFreq, "fDo_ AudioReadFreq", TaskStack40K, NULL, Priority4, NULL, TaskCore1 ); //assigned to core
  xTaskCreatePinnedToCore( fDo_LEDs, "fDo_ LEDs", TaskStack40K, NULL, Priority4, NULL, TaskCore0 ); //assigned to core
  xEventGroupSetBits( eg, evtDo_AudioReadFreq );
} // setup()
////
void loop() {} // void loop
////
void fDo_LEDs( void *pvParameters )
{
  int iFreqVal[7];
  int j;
  leds.setBrightness( Brightness ); //  1 = min brightness (off), 255 = max brightness.
  for (;;)
  {
    if (xQueueReceive( xQ_LED_Info, &iFreqVal,  portMAX_DELAY) == pdTRUE)
    {
      j = 0;
      //assign different values for different parts of the led strip
      for (j = 0; j < LED_COUNT; j++)
      {
        if ( (0 <= j) && (j < (LED_COUNT / SEG)) )
        {
          set(j, iFreqVal[0]); // set the color of led
        }
        else if ( ((LED_COUNT / SEG) <= j) && (j < (LED_COUNT / SEG * 2)) )
        {
          set(j, iFreqVal[1]); //orginal code
        }
        else if ( ((LED_COUNT / SEG * 2) <= j) && (j < (LED_COUNT / SEG * 3)) )
        {
          set(j, iFreqVal[2]);
        }
        else if ( ((LED_COUNT / SEG * 3) <= j) && (j < (LED_COUNT / SEG * 4)) )
        {
          set(j, iFreqVal[3]);
        }
        else if ( ((LED_COUNT / SEG * 4) <= j) && (j < (LED_COUNT / SEG * 5)) )
        {
          set(j, iFreqVal[4]);
        }
        else
        {
          set(j, iFreqVal[5]);
        }
      }
      leds.show();
    }
    xEventGroupSetBits( eg, evtDo_AudioReadFreq );
  }
  vTaskDelete( NULL );
} // void fDo_ LEDs( void *pvParameters )
////
void fDo_AudioReadFreq( void *pvParameters )
{
  int64_t EndTime = esp_timer_get_time();
  int64_t StartTime = esp_timer_get_time(); //gets time in uSeconds like Arduino Micros
  for (;;)
  {
    xEventGroupWaitBits (eg, evtDo_AudioReadFreq, pdTRUE, pdTRUE, portMAX_DELAY);
    EndTime = esp_timer_get_time() - StartTime;
    // log_i( "TimeSpentOnTasks: %d", EndTime );
    Audio.ReadFreq(FreqVal);
    for (int i = 0; i < 7; i++)
    {
      FreqVal[i] = constrain( FreqVal[i], NOISE, A_D_ConversionBits );
      FreqVal[i] = map( FreqVal[i], NOISE, A_D_ConversionBits, 0, 255 );
      // log_i( "Freq %d Value: %d", i, FreqVal[i]);//used for debugging and Freq choosing
    }
    xQueueSend( xQ_LED_Info, ( void * ) &FreqVal, xTicksToWait0 );
    StartTime = esp_timer_get_time();
  }
  vTaskDelete( NULL );
} // fDo_ AudioReadFreq( void *pvParameters )