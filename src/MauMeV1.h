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
  #define MAUMEHEADER 1

  // #define FORWARD_GEN_PKTS                 // Change to forward any packet (risky, makes collisions live forever !)
  #define ACK_PKTS_OF_ACKS                 // Define if you want packets of ACKs acknowledged.
  #define MM_MAX_ACKSENT_PKTS                128  // Maximum number of ACKS stored before overwriting.
  #define MM_MAX_PKTS                        128  // Maximum number of packets stored before overwriting.
  #define MM_MAX_NODE_PKTS                   128  // Maximum number of node packets stored before overwriting. 
  #define ACK_PILE_MIN_COUNT                 0 // Minimum ACK number in a pile for it to beacknow dealt with 
  // #define MAUME_DEBUG   
  // #define MAUME_CHECK_ADMIN   
  #define MAUME_ADMIN_ADDR        "ea:b9:dd:dc:5e:1d:41:13:12:c6:0c:8f"   
  #define MAUME_SETUP_WEB_SERVER  // Define if you want to serve messsages with http (requires DNS and soft access point).
  // #define MAUME_SETUP_DNS_SERVER  // Define if you want to serve messsages with http (requires soft access point).
  #define MAUME_ACTIVATE_WIFI     // Define if you want to activate MauMe Wifi soft access point.
  
  #include "MauMeHeader.h"
//--------------------------------------------------------------------------------------------------------------------------
class MauMeClass {

public:
  
  MauMeClass();
  
  void                    runMauMe();
  void                    sleepMauMe();
  void                    serveHttp();
  void                    haltHttp();
  void                    setup(long serialSpeed);
  void                    oledDisplay(bool doFlip);
  void                    oledMauMeMessage(String message, bool doFlip);
  void                    onDelivery(void(*_onDelivery)(int));        // Use this function to set the callback to notify when messages arrive.
  bool                    enlistMessageForTransmit(String recipientsMacStyleAddress, String message);
  int           IRAM_ATTR countReceivedMessages();            
  String        IRAM_ATTR pkt2Str(LoRa_PKT * lrPkt);  
  uint32_t      IRAM_ATTR getCRC32(String str);
  byte*         IRAM_ATTR getSha256Bytes(String payload);
  byte*         IRAM_ATTR getSha256BytesOfBytes(char* payload, const size_t payloadLength);
  LoRa_PKT *    IRAM_ATTR getReceivedPackets();  // Get a Packet * linked-list of the arrived packets.
  LoRa_PKT *    IRAM_ATTR freeAndGetNext(LoRa_PKT * list);  // Free list and return next element of linked-list.
  MauMePKT_0 *  IRAM_ATTR parseMMPktFromLoRa(LoRa_PKT * lrPkt);
        
  int       packetsRoutineProcessInterval     = 15000; // milliseconds
  SSD1306 * display;
  String    myMacAddress;                 // This will be your MAC once initialized.
  String    MyAddress                         = "";       // This will be your node address once initialized.
  volatile  bool runDNS                       = true;     // You can control the DNS server activity using this.
  volatile  SemaphoreHandle_t spiMutex; // Use this mutex recursively to access SPI bus safely 
                                        // (or you will collide with LoRa!).
       
private:
  
  int               deleteOlderFiles(const char * dir, int overNumber);
  int               sendLoRaBuffer(char* outgoing, int len);
  int               sendLoRaPKT(LoRa_PKT * lrPkt, bool save);
  int               countSavedPackets();
  int               countSavedACKS2SENDs();
  int               countfiles(String path);
  int               countNodeReceivedMessages();
  int               saveLoRaACK_0(const char * dirname, MauMeACK_0 * mmAck, bool overWrite);  
  int     IRAM_ATTR Log(String str);
  int     IRAM_ATTR saveLoRaPkt(const char * dirname, LoRa_PKT * lrPkt, bool overWrite);
  
  
  IPAddress         getIPFromString(const char * strIP, int len);
  ADDRESS IRAM_ATTR getZeroAddress();
  ADDRESS IRAM_ATTR zeroAddress(ADDRESS address);
  ADDRESS IRAM_ATTR addressFromBytes(byte * str);
  ADDRESS IRAM_ATTR addressFromHexString(String str);
  ADDRESS IRAM_ATTR macFromMacStyleString(String str);
  ADDRESS IRAM_ATTR addressFromMacStyleString(String str);

  String            ack2Str(MauMeACK_0 * mmAck);
  String            getPKTType(unsigned char type);
  String            getACKType(unsigned char type);
  String IRAM_ATTR  getTimeString(time_t now);
  String IRAM_ATTR  Mac2Str(char* buff);
  String IRAM_ATTR  getContentType(String filename);
  String IRAM_ATTR  addressBuffer2Str(char* buff, int len);
  String IRAM_ATTR  addressBuffer2MacFormat(char* buff, int len);
  String IRAM_ATTR  mes2Str(MauMePKT_0 *mmpkt);  
  String IRAM_ATTR  pkt2Html(LoRa_PKT * lrPkt);  
  String IRAM_ATTR  recPkt2Html(LoRa_PKT * lrPkt);
    
  String IRAM_ATTR  listReceived2Str(fs::FS &fs, SemaphoreHandle_t mutex, const char * dirname, unsigned char appType, ADDRESS macFrom);
  String IRAM_ATTR  listSent2Str(fs::FS &fs, SemaphoreHandle_t mutex, const char * dirname, unsigned char appType, ADDRESS macFrom);
  
  LoRa_PKT *           extractMsgPckt(LoRa_PKT * pckt);
  LoRa_PKT *           createDummyLoRaPacket(String text);
  LoRa_PKT *           updateCommandPacketTime(LoRa_PKT * lrPkt);
  LoRa_PKT *           getPacketFromCommand(MauMeCMD_0 * mmCmd);
  LoRa_PKT * IRAM_ATTR getCurrentLoRaPkt();
  LoRa_PKT * IRAM_ATTR loadLoRaPacket(String path);
  LoRa_PKT * IRAM_ATTR getLoRaPacketFromMMPKT(MauMePKT_0 * mmPkt);
      
  uint32_t            get_MMPKT_0_CRC32(MauMePKT_0 *mmpkt);
  uint32_t IRAM_ATTR  getCRC32FromChars(char * str, int len);

  char IRAM_ATTR      hexStr2Num(char str[]);
    
  MauMePKT_0 * parseMMPktFromBuffer(char* dat, int len);
  MauMePKT_0 * IRAM_ATTR createPacket(ADDRESS macFrom, ADDRESS addressTo, String message, unsigned char pktType);
  MauMePKT_0 * createDummyPacket(String text);
  MauMePKT_0 * parseMMPktFromString(String str);
  
  MauMeACK_0 * loadLoRaACK(String path); 
  MauMeACK_0 * makeMauMeACK_0(unsigned char htl, unsigned char frmt, unsigned char type, uint32_t hash);  
  MauMeACK_0 * parseMMAckFromBuffer(char* dat, int len);
  
  MauMeCMD_0 * IRAM_ATTR  makeCommand(unsigned char  HTL, unsigned char FRMT, unsigned char  CMD, time_t T);
  MauMeCMD_0 *            parseCommand(char* dat, bool check);
  MauMeCMD_0 *            updateCommandTime(MauMeCMD_0 * mmCmd);
  MauMeCMD_0 *            getCommand(char * dat);
  MauMeCMD_0 *            getComandFromPacket(LoRa_PKT * lrPkt);
  bool IRAM_ATTR  addPacket(LoRa_PKT * lrPkt);
  bool IRAM_ATTR  compareBuffers(const char* buf1,const char* buf2, int len);
  bool            isMessageArrived(char *buf);
  bool            ackAlreadyInPile(LoRa_PKT * pckt, MauMeACK_0 * anAck);
  bool            ackAlreadyPiggyBacked(LoRa_PKT * pckt, MauMeACK_0 * anAck);
    
  int saveReceivedPackets();
  int cleanPacketsVsACKs();
  int processPackets();
  
  void activate_DNS();
  void activate_LoRa();
  void activate_Wifi();
  void setupWebServer();
  
  static void LoRaTask( void * pvParameters );
  static void DnsTask(void * pvParameters);
  static void MauMeTask(void * pvParameters);
  
  static void notFound(AsyncWebServerRequest *request);
  
  ADDRESS_LIST  getClientsList();
  int           freeClientsList(ADDRESS_LIST macAddressList);
  void          printWifiClients();
  
  struct eth_addr * IRAM_ATTR getClientMAC(IPAddress clntAd);
  
  AsyncResponseStream * IRAM_ATTR getWebPage(AsyncWebServerRequest *request, ADDRESS macFrom, byte appType);
  AsyncResponseStream * IRAM_ATTR getAdminPage(AsyncWebServerRequest *request, ADDRESS macFrom, byte appType);
  AsyncResponseStream * IRAM_ATTR getSimusPage(AsyncWebServerRequest *request, ADDRESS macFrom, byte appType);
  bool writeMAC2MEM(String mac);
  String readMEM2MAC();
  bool    IRAM_ATTR writeMem(int index, int number);
  int     IRAM_ATTR readMem(int index);
  void    IRAM_ATTR writeString2Mem(int index, String str, int len);
  String  IRAM_ATTR readMem2String(int index, int len);
  
private:
  const byte  DNS_PORT   =     53;
  const char* PARAM_DEST = "DEST";
  const char* PARAM_MESS = "MESS";
  const char* PARAM_TYPE = "TYPE";
  const char* PARAM_UPDT = "UPDT";
  const char* PARAM_SEND = "SEND";
  
  DNSServer * dnsServer     = NULL;
  AsyncWebServer * server   = NULL;
  //----------------------------------------------- MAUME
  int nbReceivedPkts                          = 0;
  long dummyLastSendTime                      = 0;   
  long alternateLastActivationTime            = 0;   
  long packetsRoutineLastProcessTime          = 0;
  long receivedPacketsLastSaveTime            = 0;
  void (*_onDelivery)(int)                    = NULL;
  uint32_t crcState                           = 0; 
  const char *ssid                            = "MauMe-";
  const char *password                        = "";
  
  TaskHandle_t loRaTask;
  TaskHandle_t dnsTask;
  TaskHandle_t mauMeTask;
  
  volatile bool           doRun = true;
  volatile bool           doServe = true;
  
  String logFile = "";       // This is the path to the MauMe log file.
  int       DUMMY_PROCESS_STATIC_DELAY        =   30000;  // milliseconds
  int       DUMMY_PROCESS_RANDOM_DELAY        =   30000;  // milliseconds
  int       MM_PCKT_PROCESS_STATIC_DELAY      =   30000;  // milliseconds
  int       MM_PCKT_PROCESS_RANDOM_DELAY      =   30000;  // milliseconds
  int       MM_TX_POWER_MIN                   =       2;  // THESE ARE FOR HELTEC LoRa V2
  int       MM_TX_POWER_MAX                   =       2;  // THESE ARE FOR HELTEC LoRa V2
  int       MM_PCKT_SAVE_DELAY                =    5000;  // milliseconds
  bool      doAlternate                       =   false;
  int       percentActivity                   =      75;
  int       alternateInterval                 =   12*60*1000;//3600000;  // milliseconds
  int       dummySendInterval                 =   10000;  // milliseconds
  int       nbDummyPkts                       =      25;
  String    dummyTargetAddress                =      "df:15:33:72:73:7e:fc:30:5c:0b:06:72";//"ff:09:aa:ae:d7:89:73:de:69:74:1e:b9";
  volatile bool sendDummies                   =   false;
  
  volatile SemaphoreHandle_t          pcktListSemaphore;
  volatile LoRa_PKT * receivedPackets         =    NULL;
    
  ADDRESS myByteAddress;
  ADDRESS wifiClients[MAX_CLIENTS];
};

extern MauMeClass MauMe;

#endif
