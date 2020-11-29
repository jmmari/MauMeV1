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
#ifndef MAUMEHEADER
  #define MAUMEHEADER

  // #define FORWARD_GEN_PKTS                 // Change to forward any packet (risky, makes collisions live forever !)
  #define MM_MAX_BOX_PKTS                64  // Maximum number of PKTS stored in INBOX or OUTBOX before overwriting.
  #define MM_MAX_NODE_PKTS               64  // Maximum number of node packets stored before overwriting. 
  #define MM_MAX_NODE_ACKS               64  // Maximum number of node ACKs stored before overwriting. 
  #define MM_MAX_NODE_REMS               48  // Maximum number of node ACKs REMinders stored before overwriting. 

  #define MM_ADDR_DISP_LONG               6  // Maximum number of node ACKs REMinders stored before overwriting. 
  #define MM_ADDR_DISP_SHRT               3  // Maximum number of node ACKs REMinders stored before overwriting. 

  #define MAUME_CLEAN_RATE                3  // Integer. Cleaning process occurs less often than packet sending.  
  
  // #define MAUME_DEBUG   
  // #define MAUME_CHECK_ADMIN   
  #define MAUME_HOP_ON_TX           // PKT NHP and ACKs HTL are inc/dec-remented on each transmit.
  #ifndef MAUME_HOP_ON_TX 
    #define MAUME_HOP_ON_RECEIVE    // OR PKT NHP and ACKs HTL are inc/dec-remented on receive.
  #endif 
  #define MAUME_ADMIN_ADDR        "ea:b9:dd:dc:5e:1d:41:13:12:c6:0c:8f"   
  #define MAUME_SETUP_WEB_SERVER     // Define if you want to serve messsages with http (requires DNS and soft access point).
  #define MAUME_SETUP_DNS_SERVER  // Define if you want to serve messsages with http (requires soft access point).
  #define MAUME_ACTIVATE_WIFI        // Define if you want to activate MauMe Wifi soft access point.
  
  #include "MauMeHeader.h"
//----------------------------------------------------------------------------------------------------------------------------------------------------------
class MauMeClass {

public:
  
  MauMeClass();
  
  void           runMauMe();
  void           sleepMauMe();
  void           serveHttp();
  void           haltHttp();
  void           setup(long serialSpeed);
  void           oledDisplay(bool doFlip);
  void           oledMauMeMessage(String message1, String message2, String message3);
  void           onDelivery(void(*_onDelivery)(int));        // Use this function to set the callback to notify when messages arrive.
  bool           enlistMessageForTransmit(String recipientsMacStyleAddress, String message);
  int            countReceivedMessages();            
  String         pkt2Str(MM_PKT * packet);  
  uint32_t       getCRC32(String str);
  byte*          getSha256Bytes(String payload);
  byte*          getSha256BytesOfBytes(char* payload, const size_t payloadLength);
  
  PKT_LINK * IRAM_ATTR getReceivedPackets();  // Get a Packet * linked-list of the arrived packets.
  PKT_LINK *           freeAndGetNext(PKT_LINK * list);  // Free list and return next element of linked-list.
  
  int       packetsRoutineProcessInterval     = 15000; // milliseconds
  int       cleaningRoutineProcessInterval     = MAUME_CLEAN_RATE*packetsRoutineProcessInterval; // milliseconds
  SSD1306 * display;
  String    myMacAddress;                 // This will be your MAC once initialized.
  String    MyAddress                         = "";       // This will be your node address once initialized.
  volatile  bool runDNS                       = true;     // You can control the DNS server activity using this.
  volatile  SemaphoreHandle_t spiMutex; // Use this mutex recursively to access SPI bus safely 
                                        // (or you will collide with LoRa!).    
  volatile bool MauMeIsRunning = false;         
  //-----------------------------------------------------------------------------------------------------------------------------                               
private:
  
  int      deleteOlderFiles(const char * dir, int overNumber);
  int      sendLoRaBuffer(char* outgoing, int len);
  int      sendDataPKT(Data_PKT * dataPacket);
  int      countSavedPackets();
  int      countSavedACKS2SENDs();
  int      countfiles(String path);
  int      countNodeTotalInboxMessages();
  int      save_ACK(const char * dirname, MM_ACK * mmAck, bool overWrite);  
  int      save_ACK_AS_SELF(const char * dirname, MM_ACK * mmAck, bool overWrite);
  int      Log(String str);
  int      getPacketSize(MM_PKT * pkt);
  int      getLoRaPacketSize(MM_LoRa_PKT * loRaPkt);
  int      save_PKT(const char * dirname, String fileName, MM_PKT * pktIn);
  int      save_ACK_AS(const char * dirname, String fileName, MM_ACK * ack);
  int      save_SHRC_ACK(const char * dirname, MM_ACK * mmAck, bool overWrite);
  bool     pileUp(MM_LoRa_PKT * loRaPkt, char * dat, int len);
  
  IPAddress getIPFromString(const char * strIP, int len);
  ADDRESS   getZeroAddress();
  ADDRESS   zeroAddress(ADDRESS address);
  ADDRESS   addressFromBytes(byte * str);
  ADDRESS   addressFromHexString(String str);
  ADDRESS   macFromMacStyleString(String str);
  ADDRESS   addressFromMacStyleString(String str);

  String getType(unsigned char type);
  String getTypeString(unsigned char type);
  String ack2Str(MM_ACK * mmAck);
  String getShortAddress(ADDRESS address1, int n);
  String getLoRaPktType(unsigned char type);
  String getTimeString(time_t now);
  String Mac2Str(char* buff);
  String getContentType(String filename);
  String addressBuffer2Str(char* buff, int len);
  String addressBuffer2MacFormat(char* buff, int len);
  String pkt2Html(MM_PKT * mmPkt);  
  String recPkt2Html(MM_PKT * mmPkt);
  String pktParams2Html(MM_PKT * mmPkt);
  String ackParams2Html(MM_ACK * ack);
  String listReceivedPackets2Html(fs::FS &fs, SemaphoreHandle_t mutex, const char * dirname);
  String listReceivedACKs2Html(fs::FS &fs, SemaphoreHandle_t mutex, const char * dirname);
  String listReceivedMessages2Html(fs::FS &fs, SemaphoreHandle_t mutex, const char * dirname, ADDRESS macFrom);
  String listSent2Str(fs::FS &fs, SemaphoreHandle_t mutex, const char * dirname, ADDRESS macFrom);

  MM_LoRa_PKT *         getEmptyLoRaPKT(ADDRESS sender);
  Data_PKT *  IRAM_ATTR getCurrentLoRaPkt();
  MM_PKT *    IRAM_ATTR load_PKT(String path);
  MM_PKT *              createDummyPKT(String text);  
  MM_PKT *              createPKT(ADDRESS addressFrom, ADDRESS addressTo, String message);
  MM_ACK *              load_ACK(String path); // unsigned char frmt, 
  MM_ACK *              make_ACK(unsigned char htl, unsigned char type, uint32_t hash, ADDRESS SenderNode, ADDRESS destNode);  
  
  uint32_t get_PKT_CRC32(MM_PKT *mmpkt);
  uint32_t get_ACK_CRC32(MM_ACK * mmAck);
  uint32_t getCRC32FromChars(char * str, int len);
  uint32_t get_LoRa_PKT_CRC32(MM_LoRa_PKT * loRaPKT);

  char hexStr2Num(char str[]);
    
  bool           addPacket(MM_PKT * packet);
  bool IRAM_ATTR compareBuffers(const char* buf1, const char* buf2, int len);
  bool           addressesMatch(ADDRESS address1, ADDRESS address2);
  bool           isAtDestination(char *buf);

  int saveReceivedPackets();
  int cleanPacketsVsACKs();
  int processPackets();
  
  void activate_DNS();
  void activate_LoRa();
  void activate_Wifi();
  void setupWebServer();
  
  static void           onLoRaPacket( int input );
  static void IRAM_ATTR LoRaTask( void * pvParameters );
  static void           DnsTask(void * pvParameters);
  static void           MauMeTask(void * pvParameters);
  
  static void notFound(AsyncWebServerRequest *request);
  
  ADDRESS_LIST  getClientsList();
  int           freeClientsList(ADDRESS_LIST macAddressList);
  void          printWifiClients();
  
  struct eth_addr * getClientMAC(IPAddress clntAd);
  
  AsyncResponseStream * getWebPage(AsyncWebServerRequest *request, ADDRESS macFrom);
  AsyncResponseStream * getAdminPage(AsyncWebServerRequest *request, ADDRESS macFrom);
  AsyncResponseStream * getSimusPage(AsyncWebServerRequest *request, ADDRESS macFrom);
    
  int      readMem(int index);
  void     writeString2Mem(int index, String str, int len);
  bool     writeMAC2MEM(String mac);
  bool     writeMem(int index, int number);  
  String   readMEM2MAC();
  String   readMem2String(int index, int len);
  
private:
  const byte  DNS_PORT   =     53;
  const char* PARAM_DEST = "DEST";
  const char* PARAM_MESS = "MESS";
  const char* PARAM_TYPE = "TYPE";
  const char* PARAM_UPDT = "UPDT";
  const char* PARAM_SEND = "SEND";
  
  DNSServer *   dnsServer   = NULL;
  AsyncWebServer * server   = NULL;
  //----------------------------------------------- MAUME
  long dummyLastSendTime                      = 0;   
  long alternateLastActivationTime            = 0;   
  long packetsRoutineLastProcessTime          = 0;
  long cleaningRoutineLastProcessTime         = 0;
  long receivedPacketsLastSaveTime            = 0;
  void (*_onDelivery)(int)                    = NULL;
  int nbUserPackets                           = 0;
  uint32_t crcState                           = 0; 
  const char *ssid                            = "MauMe-";
  const char *password                        = "";
  
  TaskHandle_t loRaTask;
  TaskHandle_t dnsTask;
  TaskHandle_t mauMeTask;
  
  volatile bool           doRun = true;
  volatile bool           doServe = true;
  
  String    logFile = "";       // This is the path to the MauMe log file.
  int       DUMMY_PROCESS_STATIC_DELAY        =   300000;  // milliseconds
  int       DUMMY_PROCESS_RANDOM_DELAY        =   300000;  // milliseconds
  int       MM_PCKT_PROCESS_STATIC_DELAY      =   300000;  // milliseconds
  int       MM_PCKT_PROCESS_RANDOM_DELAY      =   300000;  // milliseconds
  int       CLEANING_PROCESS_STATIC_DELAY        =   MAUME_CLEAN_RATE*MM_PCKT_PROCESS_STATIC_DELAY;  // milliseconds
  int       CLEANING_PROCESS_RANDOM_DELAY        =   MAUME_CLEAN_RATE*MM_PCKT_PROCESS_RANDOM_DELAY;  // milliseconds
  int       MM_TX_POWER_MIN                   =       2;  // THESE ARE FOR HELTEC LoRa V2
  int       MM_TX_POWER                       =       MM_TX_POWER_MIN;  // THESE ARE FOR HELTEC LoRa V2
  int       MM_TX_POWER_MAX                   =      20;  // THESE ARE FOR HELTEC LoRa V2
  int       MM_NB_TXT                         =       0;  // 
  int       MM_NB_RXT                         =       0;  // 
  int       MM_NB_RX                          =       0;  // 
  int       MM_PCKT_SAVE_DELAY                =    5000;  // milliseconds
  bool      doAlternate                       =   false;
  int       percentActivity                   =      75;
  int       alternateInterval                 =   12*60*1000;//3600000;  // milliseconds
  int       dummySendInterval                 =   10000;  // milliseconds
  int       nbDummyPkts                       =      25;
  String    dummyTargetAddress                =      "df:15:33:72:73:7e:fc:30:5c:0b:06:72";
  volatile bool sendDummies                   =   false;
  
  volatile SemaphoreHandle_t          pcktListSemaphore;
  volatile Data_PKT * receivedPackets         =    NULL;
    
  ADDRESS myByteAddress;
  ADDRESS wifiClients[MAX_CLIENTS];
};

extern MauMeClass MauMe;

#endif
