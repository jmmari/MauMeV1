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
  // #define ACK_PKTS_OF_ACKS                 // Define if you want packets of ACKs acknowledged.
  #define MAX_ACKSENT_PKTS                64  // Maximum number of ACKS stored before overwriting.
  #define MAX_PKTS                        64  // Maximum number of packets stored before overwriting.
  #define MAX_NODE_PKTS                   64  // Maximum number of node packets stored before overwriting.
  #define PCKT_SAVE_DELAY               3000  // milliseconds
  #define PCKT_PROCESS_STATIC_DELAY    50000  // milliseconds
  #define PCKT_PROCESS_RANDOM_DELAY    20000  // milliseconds
  #define DUMMY_PROCESS_STATIC_DELAY       0 // 300000   // milliseconds
  #define DUMMY_PROCESS_RANDOM_DELAY  300000  // milliseconds
  #define TX_POWER_MIN                     1  // THESE ARE FOR HELTEC LoRa V2
  #define TX_POWER_MAX                    20  // THESE ARE FOR HELTEC LoRa V2

  // #define MAUME_DEBUG   
  #define MAUME_SETUP_WEB_SERVER  // Define of you want to serve messsages with http (requires DNS and soft access point).
  #define MAUME_SETUP_DNS_SERVER  // Define of you want to serve messsages with http (requires soft access point).
  #define MAUME_ACTIVATE_WIFI     // Define of you want to activate MauMe Wifi soft access point.
  
  #include "MauMeHeader.h"
//--------------------------------------------------------------------------------------------------------------------------
class MauMeClass {

public:
  
  MauMeClass();
  void setup(long serialSpeed);
  void oledDisplay();
    
  int IRAM_ATTR countReceivedMessages();            
  void onInBox(void(*whenInBox)(int));        // Use this function to set the callback to notify when messages arrive.
  bool enlistMessageForTransmit(String recipientsAddress, String message);
  String IRAM_ATTR pkt2Str(LoRa_PKT * lrPkt);  
  LoRa_PKT * IRAM_ATTR getReceivedPackets();  // Get a Packet * linked-list of the arrived packets.
  LoRa_PKT * IRAM_ATTR freeAndGetNext(LoRa_PKT * list);  // Free list and return next element of linked-list.
  MauMePKT_0 * IRAM_ATTR parseMMPktFromLoRa(LoRa_PKT * lrPkt);
  
    
  int dummySendInterval                 = 30000; // milliseconds
  int packetsRoutineProcessInterval     = 15000; // milliseconds
    
  volatile bool runDNS                  = true;     // You can control the DNS server activity using this.
  volatile SemaphoreHandle_t spiMutex;              // Use this mutex recursively to access SPI bus safely (or you will collide with LoRa!).
  String MyAddress                          = "";       // This will be your MAC once initialized.
    
private:

  int deleteOlderFiles(const char * dir, int overNumber);
  int sendLoRaBuffer(char* outgoing, int len);
  int sendLoRaPKT(LoRa_PKT * lrPkt, bool save);
  int countSavedPackets();
  int countSavedACKS2SENDs();
  int countfiles(String path);
  int saveLoRaACK_0(const char * dirname, MauMeACK_0 * mmAck, bool overWrite);  
  int IRAM_ATTR saveLoRaPkt(const char * dirname, LoRa_PKT * lrPkt, bool overWrite);
  
  ADDRESS zeroAddress(ADDRESS address);
  ADDRESS getZeroAddress();
  ADDRESS IRAM_ATTR macFromString(String str);
  ADDRESS IRAM_ATTR addressFromString(String str);
  IPAddress getIPFromString(const char * strIP, int len);

  String getPKTType(unsigned char type);
  String getACKType(unsigned char type);
  String ACK2STR(MauMeACK_0 * mmAck);
  String IRAM_ATTR getContentType(String filename);
  String IRAM_ATTR Mac2Str(char* buff);
  String IRAM_ATTR addressBuffer2Str(char* buff);
  String IRAM_ATTR pkt2Html(LoRa_PKT * lrPkt);  
  String IRAM_ATTR recPkt2Html(LoRa_PKT * lrPkt);
  String IRAM_ATTR mes2Str(MauMePKT_0 *mmpkt);  
  
  String IRAM_ATTR listReceived2Str(fs::FS &fs, SemaphoreHandle_t mutex, const char * dirname, unsigned char appType, ADDRESS macFrom);
  String IRAM_ATTR listSent2Str(fs::FS &fs, SemaphoreHandle_t mutex, const char * dirname, unsigned char appType, ADDRESS macFrom);
  
  LoRa_PKT * IRAM_ATTR loadLoRaPacket(String path);
  LoRa_PKT * IRAM_ATTR getCurrentLoRaPkt();
  LoRa_PKT * IRAM_ATTR getLoRaPacketFromMMPKT(MauMePKT_0 * mmPkt);
  LoRa_PKT * extractMsgPckt(LoRa_PKT * pckt);
  LoRa_PKT * createDummyLoRaPacket(String text);
  
  uint32_t getCRC32(String str);
  uint32_t IRAM_ATTR getCRC32FromChars(char * str, int len);
  uint32_t get_MMPKT_0_CRC32(MauMePKT_0 *mmpkt);

  char IRAM_ATTR hexStr2Num(char str[]);
    
  MauMePKT_0 * parseMMPktFromBuffer(char* dat, int len);
  MauMePKT_0 * IRAM_ATTR createPacket(ADDRESS macFrom, ADDRESS addressTo, String message, unsigned char pktType);
  MauMePKT_0 * createDummyPacket(String text);
  
  MauMeACK_0 * loadLoRaACK(String path); 
  MauMeACK_0 * makeMauMeACK_0(unsigned char htl, unsigned char frmt, unsigned char type, uint32_t hash);  
  MauMeACK_0 * parseMMAckFromBuffer(char* dat, int len);
  MauMePKT_0 * parseMMPktFromString(String str);
  
  bool IRAM_ATTR addPacket(LoRa_PKT * lrPkt);
  bool compareBuffers(const char* buf1,const char* buf2, int len);
  bool ackAlreadyInPile(LoRa_PKT * pckt, MauMeACK_0 * anAck);
  bool ackAlreadyPiggyBacked(LoRa_PKT * pckt, MauMeACK_0 * anAck);
  bool isMessageArrived(char *buf);
  
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
  
  ADDRESS_LIST getClientsList();
  int freeClientsList(ADDRESS_LIST macAddressList);
  void printWifiClients();
  
  struct eth_addr * IRAM_ATTR getClientMAC(IPAddress clntAd);
  
  AsyncResponseStream * IRAM_ATTR getWebPage(AsyncWebServerRequest *request, ADDRESS macFrom, byte appType);
  AsyncResponseStream * IRAM_ATTR getAdminPage(AsyncWebServerRequest *request, ADDRESS macFrom, byte appType);
  
  bool writeMAC2MEM(String mac);
  String readMEM2MAC();
  
private:
  const byte DNS_PORT  = 53;
  const char* PARAM_DEST = "DEST";
  const char* PARAM_MESS = "MESS";
  const char* PARAM_TYPE = "TYPE";
  const char* PARAM_UPDT = "UPDT";
  const char* PARAM_SEND = "SEND";
  
  DNSServer * dnsServer = NULL;
  AsyncWebServer * server = NULL;
  //----------------------------------------------- MAUME
  int nbReceivedPkts = 0;
  long dummyLastSendTime                = 0;   
  long packetsRoutineLastProcessTime    = 0;
  long receivedPacketsLastSaveTime      = 0;
  void (*whenInBox)(int) = NULL;
  uint32_t crcState     = 0; 
  const char *ssid      = "MauMe-";
  const char *password  = "";
  
  TaskHandle_t loRaTask;
  TaskHandle_t dnsTask;
  TaskHandle_t mauMeTask;
    
  volatile SemaphoreHandle_t pcktListSemaphore;
  volatile LoRa_PKT * receivedPackets = NULL;
    
  ADDRESS myMacAdd;
  ADDRESS wifiClients[MAX_CLIENTS];
};

extern MauMeClass MauMe;

#endif
