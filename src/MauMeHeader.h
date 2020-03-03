
//---------------------------------------------------------------------------------------------------------
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
  
  #define BAND               867E6
  #define PABOOST            false
  #define TXPOWER                2
  #define SPREADING_FACTOR       7 // 12
  #define BANDWIDTH         125000
  #define CODING_RATE            5
  #define PREAMBLE_LENGTH        8
  #define SYNC_WORD           0x4D
  #define BLUE_LED              25
  // #define LowPower               4
  // #define HighPower             20
  
  #define LORA_READ_DELAY      250
  //------------------------------------------------------ MAUME -------------------------------------------
  //             DYNAMIC PART |                STATIC CONTENT                |      
  // - Sequence : NHP | CRC32 | FRMT | DEST | SEND | APPTYP | LOADSIZE | LOAD 
  // - Nb bytes :  1  |    4  |   1  |   6  |   6  |    1   |    1     | 1-101 

  //#define PORT_HTTP        80
  #define MAX_CLIENTS        64  
  #define DNS_TASK_DELAY    500
  
  // #define configSUPPORT_STATIC_ALLOCATION   1
  // #define configSUPPORT_DYNAMIC_ALLOCATION  1
  
  #define HDR0_FRMT                 1
  #define HDR1_FRMT                 2
  #define HDR0_FRMT_POS             5
  // #define HDR0_FRMT_RST         15
  
  #define MMM_APPTYP_UKN            1
  #define MMM_APPTYP_NOD            2
  #define MMM_APPTYP_PRV            3
  #define MMM_APPTYP_DUM            4
  #define MMM_APPTYP_CMD            5
  #define MMM_APPTYP_INSTR          6
 
  #define MM_PKT_TYPE_UKN           1
  #define MM_PKT_TYPE_GEN           2
  #define MM_PKT_TYPE_MMM           3
  #define MM_PKT_TYPE_PILE          4
  #define MM_PKT_TYPE_ACK_OF_PILE   5
  #define MM_PKT_TYPE_CMD           6
  
  // FRMT
  #define ACK0_FRMT                 1
  #define ACK1_FRMT                 2
  
  // ACK types:
  #define ACK0_TYPE_SH              1
  #define ACK0_TYPE_RC              2
  #define ACK0_TYPE_PL              3 // packet with only piled-up ACKS are not acknowledged
  
  #define MAX_MSG_SIZE            100
  #define MAX_PKT_SIZE            255
  //---------------------------------------------------- DATA STRUCTURES
  #define PACKED __ATTR_PACKED__
  #define __ATTR_PACKED__ __attribute((__packed__))
  
  typedef struct LoRa_PKT LoRa_PKT;
  struct PACKED LoRa_PKT {
    char DAT[MAX_PKT_SIZE];
    unsigned char   PKTSIZ;
    unsigned char  PCKTTYP;
    uint32_t         CRC32;
    unsigned char   TX_POW;
    unsigned char    NB_TX;
    unsigned char   NB_MSG;
    unsigned char   NB_ACK;
    LoRa_PKT *        next;
  };
  //---------------------------------------
  #define MM_ADDRESS_SIZE       12
  typedef struct PACKED {
    char dat[MM_ADDRESS_SIZE];
  }ADDRESS;
  //---------------------------------------
  typedef struct PACKED {
    ADDRESS ** list;
    int nb;
  }ADDRESS_LIST;
  //---------------------------------------
  typedef struct PACKED {
    unsigned char           NHP;
    uint32_t              CRC32;
    unsigned char          FRMT;
    char  DEST[MM_ADDRESS_SIZE];
    char  SEND[MM_ADDRESS_SIZE];
    unsigned char        APPTYP;
    unsigned char      LOADSIZE;
  }MauMeHDR_0;
  //---------------------------------------
  typedef struct PACKED {
    MauMeHDR_0       HDR;
    char       LOAD[101];
  }MauMePKT_0;
  #define MM_PROCESS_MIN_DELAY               60000  // milliseconds
  #define MM_PROCESS_MIN_RANDOM_DELAY        10000  // milliseconds
  //---------------------------------------
  // HTL : 1byte | ACK CRC32: 4 bytes | Format 1 byte | TYPE : 1 FWRD-ACK, 2 REC-ACK | Mess. Hash : 4 bytes | Total : 14
  typedef struct PACKED {
    unsigned char  HTL;
    uint32_t     CRC32;
    unsigned char FRMT;
    unsigned char TYPE;
    uint32_t      HASH;
  }MauMeACK_0;
  #define DUMMY_PROCESS_MIN_STATIC_DELAY        60000  // milliseconds
  #define DUMMY_PROCESS_MIN_RANDOM_DELAY        10000  // milliseconds
  #define DUMMY_PROCESS_MIN_ADRESS_SIZE            12  // milliseconds
  //---------------------------------------
  typedef struct PACKED {
    byte                HTL;
    uint32_t          CRC32;
    unsigned char      FRMT;
    unsigned char      TYPE;
    uint32_t           HASH;
    char DEST[MM_ADDRESS_SIZE];
    char SEND[MM_ADDRESS_SIZE];
  }MauMeACK_1;
  //---------------------------------------
  typedef struct PACKED {
    unsigned char  HTL;
    uint32_t     CRC32;
    uint32_t        ID;
    unsigned char FRMT;
    unsigned char  CMD;  
    time_t           T;      
  }MauMeCMD_0;
 static const char css[] PROGMEM = R"rawliteral("<style type="text/css">html,body{font-family:Verdana,sans-serif;font-size:15px;line-height:1.5}html{overflow-x:hidden}
      h1{font-size:36px}h2{font-size:30px}h3{font-size:24px}h4{font-size:20px}h5{font-size:18px}h6{font-size:16px}.w3-serif{font-family:serif}
      h1,h2,h3,h4,h5,h6{font-family:"Segoe UI",Arial,sans-serif;font-weight:400;margin:10px 0}.w3-wide{letter-spacing:4px}
      h2{color:orangered;}h3{color:darkred;}h4{color:firebrick;}h5{color:darkslategray;}body{background-color: cornsilk;}
      #submitBtn{background:crimson;padding:5px 7px 6px;font:bold;color:#fff;border:1px solid #eee;border-radius:6px;box-shadow: 5px 5px 5px #eee;text-shadow:none;}
      </style>)rawliteral";
      
