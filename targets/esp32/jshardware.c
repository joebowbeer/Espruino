/*
 * This file is part of Espruino, a JavaScript interpreter for Microcontrollers
 *
 * Copyright (C) 2015 Gordon Williams <gw@pur3.co.uk>
 *
 * This Source Code Form is subject to the terms of the Mozilla Publici
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 *
 * ----------------------------------------------------------------------------
 * This file is designed to be parsed during the build process
 *
 * Contains ESP32 board specific functions.
 * ----------------------------------------------------------------------------
 */


/**
 * The ESP32 must implement its part of the Espruino contract.  This file
 * provides implementations for interfaces that are expected to be provided
 * by an Espruino board.  The signatures of the exposed functions are part
 * of the Espruino environment and can not be changed without express
 * approval from all the stakeholders.  In addition, the semantics of the
 * functions should follow the expected conventions.
 */
#include <stdio.h>

#include "jshardware.h"
#include "jsutils.h"
#include "jstimer.h"
#include "jsparse.h"
#include "jsinteractive.h"
#include "jspininfo.h"

#include "jswrap_esp32_network.h"

#include "esp_log.h"
#include "esp_wifi.h"
#include "esp_system.h"
#include "rom/uart.h"
#include "driver/gpio.h"

// The logging tag used for log messages issued by this file.
static char *tag = "jshardware";

// Convert an Espruino pin to an ESP32 pin number.
static gpio_num_t pinToESP32Pin(Pin pin);

/**
 * Initialize the JavaScript hardware interface.
 */
void jshInit() {
  ESP_LOGD(tag,">> jshInit");
  uint32_t freeHeapSize = system_get_free_heap_size();
  ESP_LOGD(tag, "Free heap size: %d", freeHeapSize);
  // FIX
  // Setup an ESP32 listening environment.
  ESP_ERROR_CHECK( esp_wifi_set_mode(WIFI_MODE_STA) );
  wifi_config_t sta_config = {
    .sta = {
      .ssid = "sweetie",
      .password = "kolbanpassword",
      .bssid_set = false
    }
  };
  ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA, &sta_config));
  ESP_ERROR_CHECK(esp_wifi_start());
  ESP_ERROR_CHECK(esp_wifi_connect());
  ESP_LOGD(tag,"<< jshInit");
} // End of jshInit


/**
 * Reset the Espruino environment.
 */
void jshReset() {
  ESP_LOGD(tag,">> jshReset");
  ESP_LOGD(tag,"<< jshReset");
}

/**
 * Re-init the ESP32 after a soft-reset
 */
void jshSoftInit() {
  ESP_LOGD(tag,">> jshSoftInit");
  jswrap_ESP32_wifi_soft_init();
  ESP_LOGD(tag,"<< jshSoftInit");
}

/**
 * Handle whatever needs to be done in the idle loop when there's nothing to do.
 *
 * Nothing is needed on the ESP32.
 */
void jshIdle() {
  //ESP_LOGD(tag,">> jshIdle");  // Can't debug log as called too often.
  // Here we poll the serial input looking for a new character which, if we
  // find, we add to the input queue of input events.  This is going to be
  // wrong for a variety of reasons including:
  //
  // * What if we want to use the serial for data input?
  // * Busy polling is never good ... we should eventually use an interrupt
  //   driven mechanism.
  //
  char rxChar;
  STATUS status = uart_rx_one_char((uint8_t *)&rxChar);
  if (status == OK) {
    jshPushIOCharEvents(EV_SERIAL1, &rxChar, 1);
  }
  //ESP_LOGD(tag,"<< jshIdle");  // Can't debug log as called too often.
}

// esp8266 chips don't have a serial number but they do have a MAC address
int jshGetSerialNumber(unsigned char *data, int maxChars) {
  ESP_LOGD(tag,">> jshGetSerialNumber");
  if (maxChars > 0) {
    data[0] = '\0';
  }
  ESP_LOGD(tag,"<< jshGetSerialNumber");
  return 0;
}

//===== Interrupts and sleeping

void jshInterruptOff() {  }
void jshInterruptOn()  { }

/// Enter simple sleep mode (can be woken up by interrupts). Returns true on success
bool jshSleep(JsSysTime timeUntilWake) {
  //ESP_LOGD(tag,">> jshSleep");  // Can't debug log as called too often.
  //ESP_LOGD(tag,"<< jshSleep");  // Can't debug log as called too often.
   return true;
} // End of jshSleep

/**
 * Delay (blocking) for the supplied number of microseconds.
 * Note that for the ESP8266 we must NOT CPU block for more than
 * 10 milliseconds or else we may starve the WiFi subsystem.
 */
void jshDelayMicroseconds(int microsec) {
  ESP_LOGD(tag,">> jshDelayMicroseconds: microsec=%d", microsec);
  ESP_LOGD(tag,"<< jshDelayMicroseconds");
} // End of jshDelayMicroseconds



/**
 * Set the state of the specific pin.
 *
 * The possible states are:
 *
 * JSHPINSTATE_UNDEFINED
 * JSHPINSTATE_GPIO_OUT
 * JSHPINSTATE_GPIO_OUT_OPENDRAIN
 * JSHPINSTATE_GPIO_IN
 * JSHPINSTATE_GPIO_IN_PULLUP
 * JSHPINSTATE_GPIO_IN_PULLDOWN
 * JSHPINSTATE_ADC_IN
 * JSHPINSTATE_AF_OUT
 * JSHPINSTATE_AF_OUT_OPENDRAIN
 * JSHPINSTATE_USART_IN
 * JSHPINSTATE_USART_OUT
 * JSHPINSTATE_DAC_OUT
 * JSHPINSTATE_I2C
 *
 * This function is exposed indirectly through the exposed global function called
 * `pinMode()`.  For example, `pinMode(pin, "input")` will set the given pin to input.
 */
void jshPinSetState(
  Pin pin,                 //!< The pin to have its state changed.
    JshPinState state        //!< The new desired state of the pin.
  ) {
  ESP_LOGD(tag,">> jshPinSetState: pin=%d, state=0x%x", pin, state);
  gpio_mode_t mode;
  switch(state) {
  case JSHPINSTATE_GPIO_OUT:
    mode = GPIO_MODE_OUTPUT;
    break;
  case JSHPINSTATE_GPIO_IN:
    mode = GPIO_MODE_INPUT;
    break;
  case JSHPINSTATE_GPIO_OUT_OPENDRAIN:
    mode = GPIO_MODE_OUTPUT_OD;
    break;
  default:
    ESP_LOGE(tag, "jshPinSetState: Unexpected state: %d", state);
  return;
  }
  gpio_num_t gpioNum = pinToESP32Pin(pin);
  gpio_set_direction(gpioNum, mode);
  ESP_LOGD(tag,"<< jshPinSetState");
}


/**
 * Return the current state of the selected pin.
 * \return The current state of the selected pin.
 */
JshPinState jshPinGetState(Pin pin) {
  ESP_LOGD(tag,">> jshPinGetState: pin=%d", pin);
  ESP_LOGD(tag,"<< jshPinGetState");
  return 0;
}

//===== GPIO and PIN stuff =====

/**
 * Set the value of the corresponding pin.
 */
void jshPinSetValue(
    Pin pin,   //!< The pin to have its value changed.
    bool value //!< The new value of the pin.
  ) {
  ESP_LOGD(tag,">> jshPinSetValue: pin=%d, value=%d", pin, value);
  gpio_num_t gpioNum = pinToESP32Pin(pin);
  gpio_set_level(gpioNum, (uint32_t)value);
  ESP_LOGD(tag,"<< jshPinSetValue");
}


/**
 * Get the value of the corresponding pin.
 * \return The current value of the pin.
 */
bool CALLED_FROM_INTERRUPT jshPinGetValue( // can be called at interrupt time
    Pin pin //!< The pin to have its value read.
  ) {
  ESP_LOGD(tag,">> jshPinGetValue: pin=%d", pin);
  gpio_num_t gpioNum = pinToESP32Pin(pin);
  bool level = gpio_get_level(gpioNum);
  ESP_LOGD(tag,"<< jshPinGetValue: level=%d", level);
  return level;
}


JsVarFloat jshPinAnalog(Pin pin) {
  ESP_LOGD(tag,">> jshPinAnalog: pin=%d", pin);
  ESP_LOGD(tag,"<< jshPinAnalog");
  return (JsVarFloat)0;
}


int jshPinAnalogFast(Pin pin) {
  ESP_LOGD(tag,">> jshPinAnalogFast: pin=%d", pin);
  ESP_LOGD(tag,"<< jshPinAnalogFast");
  return 0;
}


/**
 * Set the output PWM value.
 */
JshPinFunction jshPinAnalogOutput(Pin pin, JsVarFloat value, JsVarFloat freq, JshAnalogOutputFlags flags) { // if freq<=0, the default is used
  ESP_LOGD(tag,">> jshPinAnalogOutput: pin=%d", pin);
  ESP_LOGD(tag,"<< jshPinAnalogOutput");
  return 0;
}


/**
 *
 */
void jshSetOutputValue(JshPinFunction func, int value) {
  ESP_LOGD(tag,">> JshPinFunction");
  ESP_LOGD(tag,"<< JshPinFunction");
}


/**
 *
 */
void jshEnableWatchDog(JsVarFloat timeout) {
  ESP_LOGD(tag,">> jshEnableWatchDog");
  ESP_LOGD(tag,"<< jshEnableWatchDog");
}


// Kick the watchdog
void jshKickWatchDog() {
  ESP_LOGD(tag,">> jshKickWatchDog");
  ESP_LOGD(tag,"<< jshKickWatchDog");
}


/**
 * Get the state of the pin associated with the event flag.
 */
bool CALLED_FROM_INTERRUPT jshGetWatchedPinState(IOEventFlags eventFlag) { // can be called at interrupt time
  ESP_LOGD(tag,">> jshGetWatchedPinState");
  ESP_LOGD(tag,"<< jshGetWatchedPinState");
  return 0;
}


/**
 * Set the value of the pin to be the value supplied and then wait for
 * a given period and set the pin value again to be the opposite.
 */
void jshPinPulse(
    Pin pin,              //!< The pin to be pulsed.
    bool pulsePolarity,   //!< The value to be pulsed into the pin.
    JsVarFloat pulseTime  //!< The duration in milliseconds to hold the pin.
) {
  ESP_LOGD(tag,">> jshPinPulse: pin=%d, polarity=%d", pin, pulsePolarity);
  ESP_LOGD(tag,"<< jshPinPulse");
}


/**
 * Determine whether the pin can be watchable.
 * \return Returns true if the pin is wathchable.
 */
bool jshCanWatch(
    Pin pin //!< The pin that we are asking whether or not we can watch it.
  ) {
  ESP_LOGD(tag,">> jshCanWatch: pin=%d", pin);
  ESP_LOGD(tag,"<< jshCanWatch");
  return true;
}


/**
 * Do what ever is necessary to watch a pin.
 * \return The event flag for this pin.
 */
IOEventFlags jshPinWatch(
    Pin pin,         //!< The pin to be watched.
    bool shouldWatch //!< True for watching and false for unwatching.
  ) {
  ESP_LOGD(tag,">> jshPinWatch: pin=%d, shouldWatch=%d", pin, shouldWatch);
  ESP_LOGD(tag,"<< jshPinWatch");
  return 0;
}


/**
 *
 */
JshPinFunction jshGetCurrentPinFunction(Pin pin) {
  ESP_LOGD(tag,">> jshGetCurrentPinFunction: pin=%d", pin);
  ESP_LOGD(tag,"<< jshGetCurrentPinFunction");
  return JSH_NOTHING;
}


/**
 * Determine if a given event is associated with a given pin.
 * \return True if the event is associated with the pin and false otherwise.
 */
bool jshIsEventForPin(
    IOEvent *event, //!< The event that has been detected.
    Pin pin         //!< The identity of a pin.
  ) {
  ESP_LOGD(tag,">> jshIsEventForPin: pin=%d", pin);
  ESP_LOGD(tag,"<< jshIsEventForPin");
return 0;
}

//===== USART and Serial =====



void jshUSARTSetup(IOEventFlags device, JshUSARTInfo *inf) {
  ESP_LOGD(tag,">> jshUSARTSetup");
  ESP_LOGD(tag,"<< jshUSARTSetup");
}

bool jshIsUSBSERIALConnected() {
  ESP_LOGD(tag,">> jshIsUSBSERIALConnected");
  ESP_LOGD(tag,"<< jshIsUSBSERIALConnected");
  return false; // "On non-USB boards this just returns false"
}

/**
 * Kick a device into action (if required).
 *
 */
void jshUSARTKick(
    IOEventFlags device //!< The device to be kicked.
) {
    //ESP_LOGD(tag,">> jshUSARTKick");
  int c = jshGetCharToTransmit(device);
  while(c >= 0) {
    // Examination ... if there something more performance than using
    // printf()?  For example, some kind of putchar()?
    printf("%c", c);
    c = jshGetCharToTransmit(device);
  }
  //ESP_LOGD(tag,"<< jshUSARTKick");
}

//===== SPI =====

/**
 * Initialize the hardware SPI device.
 * On the ESP8266, hardware SPI is implemented via a set of pins defined
 * as follows:
 *
 * | GPIO   | NodeMCU | Name  | Function |
 * |--------|---------|-------|----------|
 * | GPIO12 | D6      | HMISO | MISO     |
 * | GPIO13 | D7      | HMOSI | MOSI     |
 * | GPIO14 | D5      | HSCLK | CLK      |
 * | GPIO15 | D8      | HCS   | CS       |
 *
 */
void jshSPISetup(
    IOEventFlags device, //!< The identity of the SPI device being initialized.
    JshSPIInfo *inf      //!< Flags for the SPI device.
) {
  ESP_LOGD(tag,">> jshSPISetup");
  ESP_LOGD(tag,"<< jshSPISetup");
}


/** Send data through the given SPI device (if data>=0), and return the result
 * of the previous send (or -1). If data<0, no data is sent and the function
 * waits for data to be returned */
int jshSPISend(
    IOEventFlags device, //!< The identity of the SPI device through which data is being sent.
    int data             //!< The data to be sent or an indication that no data is to be sent.
) {
  ESP_LOGD(tag,">> jshSPISend");
  ESP_LOGD(tag,"<< jshSPISend");
  return 0;
}


/**
 * Send 16 bit data through the given SPI device.
 */
void jshSPISend16(
    IOEventFlags device, //!< Unknown
    int data             //!< Unknown
) {
  ESP_LOGD(tag,">> jshSPISend16");
  ESP_LOGD(tag,"<< jshSPISend16");
}


/**
 * Set whether to send 16 bits or 8 over SPI.
 */
void jshSPISet16(
    IOEventFlags device, //!< Unknown
    bool is16            //!< Unknown
) {
  ESP_LOGD(tag,">> jshSPISet16");
  ESP_LOGD(tag,"<< jshSPISet16");
}


/**
 * Wait until SPI send is finished.
 */
void jshSPIWait(
    IOEventFlags device //!< Unknown
) {
  ESP_LOGD(tag,">> jshSPIWait");
  ESP_LOGD(tag,"<< jshSPIWait");
}


/** Set whether to use the receive interrupt or not */
void jshSPISetReceive(IOEventFlags device, bool isReceive) {
  ESP_LOGD(tag,">> jshSPISetReceive");
  ESP_LOGD(tag,"<< jshSPISetReceive");
}

//===== I2C =====

/** Set-up I2C master for ESP8266, default pins are SCL:12, SDA:13. Only device I2C1 is supported
 *  and only master mode. */
void jshI2CSetup(IOEventFlags device, JshI2CInfo *info) {
 
}


void jshI2CWrite(IOEventFlags device, unsigned char address, int nBytes,
    const unsigned char *data, bool sendStop) {
  ESP_LOGD(tag,">> jshI2CWrite");
  ESP_LOGD(tag,"<< jshI2CWrite");
}

void jshI2CRead(IOEventFlags device, unsigned char address, int nBytes,
    unsigned char *data, bool sendStop) {
  ESP_LOGD(tag,">> jshI2CRead");
  ESP_LOGD(tag,"<< jshI2CRead");
}

//===== System time stuff =====

/* The esp8266 has two notions of system time implemented in the SDK by system_get_time()
 * and system_get_rtc_time(). The former has 1us granularity and comes off the CPU cycle
 * counter, the latter has approx 57us granularity (need to check) and comes off the RTC
 * clock. Both are 32-bit counters and thus need some form of roll-over handling in software
 * to produce a JsSysTime.
 *
 * It seems pretty clear from the API and the calibration concepts that the RTC runs off an
 * internal RC oscillator or something similar and the SDK provides functions to calibrate
 * it WRT the crystal oscillator, i.e., to get the current clock ratio. The only benefit of
 * RTC timer is that it keeps running when in light sleep mode. (It also keeps running in
 * deep sleep mode since it can be used to exit deep sleep but some brilliant engineer at
 * espressif decided to reset the RTC timer when coming out of deep sleep so the time is
 * actually lost!)
 *
 * It seems that the best course of action is to use the system timer for jshGetSystemTime()
 * and related functions and to use the rtc timer only to preserve time during light sleep.
 */

/**
 * Given a time in milliseconds as float, get us the value in microsecond
 */
JsSysTime jshGetTimeFromMilliseconds(JsVarFloat ms) {
  //ESP_LOGD(tag,">> jshGetTimeFromMilliseconds");
  //ESP_LOGD(tag,"<< jshGetTimeFromMilliseconds");
  return (JsSysTime) (0);
}

/**
 * Given a time in microseconds, get us the value in milliseconds (float)
 */
JsVarFloat jshGetMillisecondsFromTime(JsSysTime time) {
  ESP_LOGD(tag,">> jshGetMillisecondsFromTime");
  ESP_LOGD(tag,"<< jshGetMillisecondsFromTime");
  return (JsVarFloat) 0;
}


/**
 * Return the current time in microseconds.
 */
JsSysTime CALLED_FROM_INTERRUPT jshGetSystemTime() { // in us -- can be called at interrupt time
  //ESP_LOGD(tag,">> jshGetSystemTime"); // Can't debug log as called too often.
  //ESP_LOGD(tag,"<< jshGetSystemTime");  // Can't debug log as called too often.
  return 0;
}


/**
 * Set the current time in microseconds.
 */
void jshSetSystemTime(JsSysTime newTime) {
  ESP_LOGD(tag,">> jshSetSystemTime");
  ESP_LOGD(tag,"<< jshSetSystemTime");
}



void jshUtilTimerDisable() {
  ESP_LOGD(tag,">> jshUtilTimerDisable");
  ESP_LOGD(tag,"<< jshUtilTimerDisable");
}

void jshUtilTimerStart(JsSysTime period) {
  ESP_LOGD(tag,">> jshUtilTimerStart");
  ESP_LOGD(tag,"<< jshUtilTimerStart");
}

void jshUtilTimerReschedule(JsSysTime period) {
  ESP_LOGD(tag,">> jshUtilTimerReschedule");
  jshUtilTimerDisable();
  jshUtilTimerStart(period);

  ESP_LOGD(tag,"<< jshUtilTimerReschedule");
}

//===== Miscellaneous =====

bool jshIsDeviceInitialised(IOEventFlags device) {
  ESP_LOGD(tag,">> jshIsDeviceInitialised");
  ESP_LOGD(tag,"<< jshIsDeviceInitialised");
 return 0;
} // End of jshIsDeviceInitialised

// the esp8266 doesn't have any temperature sensor
JsVarFloat jshReadTemperature() {
  ESP_LOGD(tag,">> jshReadTemperature");
  ESP_LOGD(tag,"<< jshReadTemperature");
  return NAN;
}

// the esp8266 can read the VRef but then there's no analog input, so we don't support this
JsVarFloat jshReadVRef() {
  ESP_LOGD(tag,">> jshReadVRef");
  ESP_LOGD(tag,"<< jshReadVRef");
  return NAN;
}

unsigned int jshGetRandomNumber() {
  ESP_LOGD(tag,">> jshGetRandomNumber");
  ESP_LOGD(tag,"<< jshGetRandomNumber");
  return rand();
}

//===== Read-write flash =====

/**
 * Determine available flash depending on EEprom size
 *
 */
uint32_t jshFlashMax() {
  ESP_LOGD(tag,">> jshFlashMax");
  ESP_LOGD(tag,"<< jshFlashMax");
  return 0;
}

/**
 * Read data from flash memory into the buffer.
 *
 * This reads from flash using memory-mapped reads. Only works for the first 1MB and
 * requires 4-byte aligned reads.
 *
 */
void jshFlashRead(
    void *buf,     //!< buffer to read into
    uint32_t addr, //!< Flash address to read from
    uint32_t len   //!< Length of data to read
  ) {
  ESP_LOGD(tag,">> jshFlashRead");
  ESP_LOGD(tag,"<< jshFlashRead");
}


/**
 * Write data to flash memory from the buffer.
 *
 * This is called from jswrap_flash_write and ... which guarantee that addr is 4-byte aligned
 * and len is a multiple of 4.
 */
void jshFlashWrite(
    void *buf,     //!< Buffer to write from
    uint32_t addr, //!< Flash address to write into
    uint32_t len   //!< Length of data to write
  ) {
  ESP_LOGD(tag,">> jshFlashWrite");
  ESP_LOGD(tag,"<< jshFlashWrite");
}


/**
 * Return start address and size of the flash page the given address resides in.
 * Returns false if no page.
 */
bool jshFlashGetPage(
    uint32_t addr,       //!<
    uint32_t *startAddr, //!<
    uint32_t *pageSize   //!<
  ) {
  ESP_LOGD(tag,">> jshFlashGetPage: addr=0x%x", addr);
  ESP_LOGD(tag,"<< jshFlashGetPage");
  return false;
}



JsVar *jshFlashGetFree() {
  ESP_LOGD(tag,">> jshFlashGetFree");
  ESP_LOGD(tag,"<< jshFlashGetFree");
  return 0;
}


/**
 * Erase the flash page containing the address.
 */
void jshFlashErasePage(
    uint32_t addr //!<
  ) {
  ESP_LOGD(tag,">> jshFlashErasePage: addr=0x%x", addr);
  ESP_LOGD(tag,"<< jshFlashErasePage");
}

unsigned int jshSetSystemClock(JsVar *options) {
  ESP_LOGD(tag,">> jshSetSystemClock");
  ESP_LOGD(tag,"<< jshSetSystemClock");
  return 0;
}

/**
 * Convert an Espruino pin id to a native ESP32 pin id.
 * Notes: It is likely that this can be optimized by taking advantage of the
 * underlying implementation of the ESP32 data types but at this time, let us
 * leave as this explicit algorithm until the dust settles.
 */
static gpio_num_t pinToESP32Pin(Pin pin) {
  switch(pin) {
  case 0:
    return GPIO_NUM_0;
  case 1:
    return GPIO_NUM_1;
  case 2:
    return GPIO_NUM_2;
  case 3:
    return GPIO_NUM_3;
  case 4:
    return GPIO_NUM_4;
  case 5:
    return GPIO_NUM_5;
  case 6:
    return GPIO_NUM_6;
  case 7:
    return GPIO_NUM_7;
  case 8:
    return GPIO_NUM_8;
  case 9:
    return GPIO_NUM_9;
  case 10:
    return GPIO_NUM_10;
  case 11:
    return GPIO_NUM_11;
  case 12:
    return GPIO_NUM_12;
  case 13:
    return GPIO_NUM_13;
  case 14:
    return GPIO_NUM_14;
  case 15:
    return GPIO_NUM_15;
  }
  ESP_LOGE(tag, "pinToESP32Pin: Unknown pin: %d", pin);
  return -1;
}