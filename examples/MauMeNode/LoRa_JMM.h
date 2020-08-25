// Copyright (c) Sandeep Mistry. All rights reserved.
// Licensed under the MIT license. See LICENSE file in the project root for full license information.

// Thank you so much Sandeep ! I just modified the name of the library to make modifications while 
// not confusing with the original version! Jm.

#ifndef LORA_JMM_H
#define LORA_JMM_H

#include <Arduino.h>
#include <SPI.h>

#ifdef ARDUINO_SAMD_MKRWAN1300
#define LORA_DEFAULT_SPI           SPI1
#define LORA_DEFAULT_SPI_FREQUENCY 250000
#define LORA_DEFAULT_SS_PIN        LORA_IRQ_DUMB
#define LORA_DEFAULT_RESET_PIN     -1
#define LORA_DEFAULT_DIO0_PIN      -1
#else
#define LORA_DEFAULT_SPI           SPI
#define LORA_DEFAULT_SPI_FREQUENCY 8E6 
#define LORA_DEFAULT_SS_PIN        10
#define LORA_DEFAULT_RESET_PIN     9
#define LORA_DEFAULT_DIO0_PIN      2
#endif

#define REG_SYMB_TIMEOUT_LSB     0x1f
#define PA_OUTPUT_RFO_PIN          0
#define PA_OUTPUT_PA_BOOST_PIN     1

class LoRaJMMClass : public Stream {
public:
  LoRaJMMClass();
  
  static volatile SemaphoreHandle_t spiMutex;   //                       JMM
  
  int begin(long frequency, SemaphoreHandle_t onReceiveMutex);
  void end();

  int IRAM_ATTR beginPacket(int implicitHeader = false);
  int IRAM_ATTR endPacket(bool async = false);

  int   IRAM_ATTR parsePacket(int size = 0);
  int   IRAM_ATTR packetRssi();
  float IRAM_ATTR packetSnr();
  long  IRAM_ATTR packetFrequencyError();

  // from Print
  virtual size_t IRAM_ATTR write(uint8_t byte);
  virtual size_t IRAM_ATTR write(const uint8_t *buffer, size_t size);

  // from Stream
  virtual int IRAM_ATTR available();
  virtual int IRAM_ATTR read();
  virtual int peek();
  virtual void flush();

#ifndef ARDUINO_SAMD_MKRWAN1300
  void IRAM_ATTR onReceive(void(*callback)(int));

  void IRAM_ATTR receive(int size = 0);
#endif
  void IRAM_ATTR idle();
  void sleep();
  void setTxPower(int level, int outputPin = PA_OUTPUT_PA_BOOST_PIN);
  void setFrequency(long frequency);
  void setSpreadingFactor(int sf);
  void setSignalBandwidth(long sbw);
  void setCodingRate4(int denominator);
  void setPreambleLength(long length);
  void setSyncWord(int sw);
  void enableCrc();
  void disableCrc();
  void enableInvertIQ();
  void disableInvertIQ();
  
  void setOCP(uint8_t mA); // Over Current Protection control

  // deprecated
  void crc() { enableCrc(); }
  void noCrc() { disableCrc(); }

  byte random();

  void setPins(int ss = LORA_DEFAULT_SS_PIN, int reset = LORA_DEFAULT_RESET_PIN, int dio0 = LORA_DEFAULT_DIO0_PIN);
  void setSPI(SPIClass& spi);
  void setSPIFrequency(uint32_t frequency);

  void dumpRegisters(Stream& out);
  	
private:
  void IRAM_ATTR explicitHeaderMode();
  void IRAM_ATTR implicitHeaderMode();

  void IRAM_ATTR handleDio0Rise();
  bool isTransmitting();

  int getSpreadingFactor();
  long getSignalBandwidth();

  void setLdoFlag();

  uint8_t IRAM_ATTR readRegister(uint8_t address);
  void IRAM_ATTR writeRegister(uint8_t address, uint8_t value);
  uint8_t IRAM_ATTR singleTransfer(uint8_t address, uint8_t value);

  static void IRAM_ATTR onDio0Rise();

private:
  SPISettings _spiSettings;
  SPIClass* _spi;
  int _ss;
  int _reset;
  int _dio0;
  long _frequency;
  int _packetIndex;
  int _implicitHeaderMode;
  void (*_onReceive)(int);
  
};

extern LoRaJMMClass LoRa_JMM;

#endif
