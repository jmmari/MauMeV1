/*
            MauMe LoRa Multi-Hops Messaging for Arduino
  
  Copyright (c) 2020 Jean Martial Mari & Alban Gabillon, University of French Polynesia. 
  All rights reserved.
                                                syy/    -yyy   .yyyyyyyyys     /yyyyyyyyy/  
                                                mMMo    /MMM`  :MMMMMMMMMMdo   yMMMMMMMMMy  
                                                mMMo    /MMM`  :MMM-````hMMh   yMMm.....``  
                                                mMMo    /MMM`  :MMM/----dMdo   yMMMMMMMN    
                                                mMMo    /MMM`  :MMMMMMMMhs     yMMNyyyys    
                                                mMMo    /MMM`  :MMMs++++.      yMMd         
                                                mMMmh.`hdMMM`  :MMM.           yMMd         
                                                ...sMmmMh...   :MMM.           yMMd        
 
    This library is free software; you can redistribute it and/or
    modify it under the terms of the GNU Lesser General Public
    License as published by the Free Software Foundation; either
    version 2.1 of the License, or (at your option) any later version.
    This library is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
    Lesser General Public License for more details.  
*/
//--------------------------------------------------------------------------------------------------------------------------------------------------------
  #include <SPI.h>
  #include "LoRa_JMM.h"
  #include <Arduino.h>
  #include <EEPROM.h>
  #include "WiFi.h"
  #include "crc.h"
  #ifdef ESP32
    #include <WiFi.h>
    #include "esp_wifi.h"
    #include <AsyncTCP.h>
  #else
    #include <ESP8266WiFi.h>
    #include <ESPAsyncTCP.h>
  #endif
  #include <ESPAsyncWebServer.h>
  #include <Streaming.h>      // Ref: http://arduiniana.org/libraries/streaming/
  extern "C" {
    #include "crypto/base64.h"
    //#include "base64.h"
    //#include "mbedtls/base64.h"
  }
  #include <FS.h>
  #include <SPIFFS.h>
  #include <sys/dir.h>
  #include "SpifFsFunctions.hpp"
  #include <string.h>
  #include <DNSServer.h> 
  #include <lwip/etharp.h>
  #include "SSD1306.h"
  // #include "md.h"
  #include "mbedtls/md.h"
  #define EEPROM_SIZE          512
  //---------------------------------------------------- OLED SCREEN PINS and Values
  #define OLED_SDA               4
  #define OLED_SCL              15
  #define OLED_RST              16
  #define posX                  21
  #define posY                   0
  //----------------------------------------------------- LORA RADIO PARAMETERS
  #define LoRa_SCK     5 // GPIO 5  -- SX1278's SCK
  #define LoRa_MISO   19 // GPIO 19 -- SX1278's MISO
  #define LoRa_MOSI   27 // GPIO 27 -- SX1278's MOSI
  #define LoRa_CS     18 // GPIO 18 -- SX1278's CS
  #define LoRa_RESET  14 // GPIO 14 -- SX1278's RESET
  #define LoRa_IRQ    26 // GPIO 26 -- SX1278's IRQ(Interrupt Request)
  
  #define BAND               867.875E6
  #define PABOOST            false
  #define TXPOWER               20
  #define SPREADING_FACTOR      12
  #define BANDWIDTH         125000 //500000 // 
  #define CODING_RATE            5 // ? 
  #define PREAMBLE_LENGTH        7 // 9
  #define SYNC_WORD           0x4E
  #define BLUE_LED              25
  
  #define LORA_READ_DELAY      100
  //------------------------------------------------------ MAUME -------------------------------------------
  // #define PORT_HTTP        80
  #define MAX_CLIENTS        64  
  #define DNS_TASK_DELAY    128
  #define MAX_PACKETS_COUNT  99  
  
  #define MM_TYPE_UKN          1   // Undefined MauMe format.
  #define MM_TYPE_REM          2   // 
  #define MM_TYPE_SHA          3   // 
  #define MM_TYPE_REC          4   // 
  #define MM_TYPE_SHRC         5   // 
  #define MM_TYPE_PKT          6   // 
     
  #define MM_PKT_TYPE_UKN      1  // Undefined format
  #define MM_PKT_TYPE_GEN      2  // Generic LoRa packet of unknown format. Possibly corrupted.
  #define MM_PKT_TYPE_MM       3  // MauMe style packet.

  #define MM_PKT_MAXIMUM     256  //
  #define MM_ACK_THRESHOLD    -1  //
    
  #define MAX_LoRa_PKT_SIZE            255 // LoRa Max packet size.
  #define MAX_MM_LoRa_SIZE           MAX_LoRa_PKT_SIZE-sizeof(MM_LoRa_PKT_HDR) // (MM_ADDRESS_SIZE + 6) // Max MauMe packet load size
  
  //---------------------------------------------------- DATA STRUCTURES
  #define PACKED __ATTR_PACKED__
  #define __ATTR_PACKED__ __attribute((__packed__))
  //---------------------------------------
  #define MM_ADDRESS_SIZE       12
  typedef struct PACKED {
    char dat[MM_ADDRESS_SIZE];
  }ADDRESS;
  //----------------------------------------
  typedef struct MM_LoRa_PKT_HDR MM_LoRa_PKT_HDR;
  struct PACKED MM_LoRa_PKT_HDR {    
    uint32_t             CRC32;   // 4       
    ADDRESS         SenderNode;   // MM_ADDRESS_SIZE
    unsigned char     PKTSIZE;   // 1
  };
  //----------------------------------------
  typedef struct MM_LoRa_PKT MM_LoRa_PKT;
  struct PACKED MM_LoRa_PKT {    
    MM_LoRa_PKT_HDR          HDR;   // MM_ADDRESS_SIZE + 6
    char    DAT[MAX_MM_LoRa_SIZE];   // MAX_MM_LoRa_SIZE
  };
  //----------------------------------------
  typedef struct Data_PKT Data_PKT;
  struct PACKED Data_PKT {
    MM_LoRa_PKT     LoRaPKT;
    unsigned char   PKTTYP;   // 1
    unsigned char   PKTSIZ;   // 1
    unsigned char   TX_POW;   // 1
    unsigned char    NB_TX;   // 1
    unsigned char   NB_MSG;   // 1
    unsigned char   NB_ACK;   // 1
    int               RSSI;   // 4
    float              SNR;   // ?
    long              FERR;   // ?
    ADDRESS     SenderNode;   // MM_ADDRESS_SIZE
    Data_PKT *        next;
  };
  //---------------------------------------
  typedef struct PACKED {
    ADDRESS ** list;
    int          nb;
  }ADDRESS_LIST;
  //---------------------------------------
  typedef struct PACKED {
    unsigned char      NHP;     // NHP MUST be the FIRST char in all formats ( ID CRC32 computed on other bytes).
    unsigned char     TYPE;     // Format MUST be the SECOND CHAR in all FORMATS.
    ADDRESS           SEND;     // char  SEND[MM_ADDRESS_SIZE];
    ADDRESS           DEST;     // char  DEST[MM_ADDRESS_SIZE];
    unsigned char LOADSIZE;
  }MM_PKT_HDR;
  //---------------------------------------
  typedef struct PACKED {
    unsigned char  HTL;     // HTL MUST be the FIRST char in all formats ( ID CRC32 computed on other bytes).
    unsigned char TYPE;     // SHACK ou RECACK ou SHRCACK
    ADDRESS       SEND;
    ADDRESS       DEST;
    uint32_t      HASH;     // TARGET : CRC32 of PKT or RECACK
  }MM_ACK;
  //---------------------------------------
  #define MAX_MM_PKT_SIZE (MAX_MM_LoRa_SIZE-sizeof(MM_PKT_HDR)-sizeof(MM_ACK)) // All space is availabme to user.
  //---------------------------------------
  typedef struct PACKED {
    MM_PKT_HDR                HDR;
    char       LOAD[MAX_MM_PKT_SIZE];
  }MM_PKT;
  //---------------------------------------
  #define MM_PROCESS_MIN_DELAY                1000  // milliseconds
  #define MM_PROCESS_MIN_RANDOM_DELAY            0  // milliseconds
  //----------------------------------------
  typedef struct PKT_LINK PKT_LINK;
  struct PACKED PKT_LINK {
    MM_PKT   *   PKT;
    PKT_LINK *  next;
  };
  //---------------------------------------
  #define DUMMY_PROCESS_MIN_STATIC_DELAY         1000  // milliseconds
  #define DUMMY_PROCESS_MIN_RANDOM_DELAY            0  // milliseconds
  #define DUMMY_PROCESS_MIN_ADRESS_SIZE            12  // milliseconds
  
