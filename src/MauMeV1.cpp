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

#include <LiquidCrystal.h>
#include "MauMeV1.h"
#include "Logo-UPF-small.h"

#if (ESP8266 || ESP32)
    #define ISR_PREFIX ICACHE_RAM_ATTR
#else
    #define ISR_PREFIX
#endif
//-----------------------------------------------------------------------------------------------------------------------
MauMeClass::MauMeClass(){
  MauMe.DUMMY_PROCESS_STATIC_DELAY = max(DUMMY_PROCESS_MIN_STATIC_DELAY, MauMe.DUMMY_PROCESS_STATIC_DELAY);
}
//-------------------------------------------------------------------------------
bool IRAM_ATTR MauMeClass::writeMem(int index, int number){
  bool ret = false;
  while(!xSemaphoreTakeRecursive(MauMe.spiMutex, portMAX_DELAY));
    // EEPROM.begin(EEPROM_SIZE);
    EEPROM.writeInt(index * sizeof(int), number);
    ret = EEPROM.commit();
  xSemaphoreGiveRecursive(MauMe.spiMutex);
  return ret;
}
//------------------------------------------------------------------------------------------------------------------------
int IRAM_ATTR MauMeClass::readMem(int index){
  int nb = -1;
  while(!xSemaphoreTakeRecursive(MauMe.spiMutex, portMAX_DELAY));
    // EEPROM.begin(EEPROM_SIZE);
    nb = (int)EEPROM.readInt(index * sizeof(int));    
  xSemaphoreGiveRecursive(MauMe.spiMutex);  
  return nb;
}
//------------------------------------------------------------------------------------------------------------------------
void IRAM_ATTR MauMeClass::writeString2Mem(int index, String str, int len){
  ADDRESS addr = MauMe.addressFromMacStyleString(str);
  bool ret = false;
  while(!xSemaphoreTakeRecursive(MauMe.spiMutex, portMAX_DELAY));
    while(len){
      EEPROM.writeChar(index* sizeof(int) + len - 1, addr.dat[len-1]);
      // Serial.printf(" Wrote : %d.\n", addr.dat[len-1]);
      EEPROM.commit();
      len -= 1;
    }  
  xSemaphoreGiveRecursive(MauMe.spiMutex);  
}
//------------------------------------------------------------------------------------------------------------------------
String IRAM_ATTR MauMeClass::readMem2String(int index, int len){
  char * buf = (char*)calloc(len, sizeof(char));
  int ln = len;
  while(!xSemaphoreTakeRecursive(MauMe.spiMutex, portMAX_DELAY));
    // EEPROM.begin(EEPROM_SIZE);
    while(len){
      buf[len-1] = EEPROM.readChar(index* sizeof(int) + len - 1);
      // Serial.printf(" Read : %d.\n", buf[len-1]);
      len -= 1;
    }
  xSemaphoreGiveRecursive(MauMe.spiMutex);  
  String res = MauMe.addressBuffer2MacFormat((char*)buf, ln);
  free(buf);
  return res;
}
//-----------------------------------------------------------------------------------------------------------------
String IRAM_ATTR MauMeClass::getTimeString(time_t now){
  char dat[10];
  int hours   = (int)(now/3600);
  int minutes = (int)((now-hours*3600)/60);
  int seconds = (int)(now-hours*3600 - minutes*60);
  sprintf(dat, "%02d:%02d:%02d", hours, minutes, seconds);
  return String(dat);
}
//-----------------------------------------------------------------------------------------------------------------
int IRAM_ATTR MauMeClass::Log(String str){
  #ifdef MAUME_DEBUG // #endif
    Serial.println(" >> NOT Appending : "+str+"<br/>\n");
  #endif
  int ret = 0;
  //ret = appendFile(SPIFFS, MauMe.spiMutex, MauMe.logFile, MauMe.getTimeString(time(NULL))+str+"<br/>\n");
  return ret; // appendFile(SPIFFS, MauMe.spiMutex, (const char*)(MauMe.logFile.c_str()), str);
}
//----------------------------------------------------------------------------------------------------------------
byte* IRAM_ATTR MauMeClass::getSha256BytesOfBytes(char* payload, const size_t payloadLength){
  //Serial.println("Computing SHA 256 of '"+MauMe.addressBuffer2Str(payload, (int)payloadLength)+"'.");
  byte * shaResult = (byte*)malloc(sizeof(byte)*32);
  mbedtls_md_context_t ctx;
  mbedtls_md_type_t md_type = MBEDTLS_MD_SHA256;
  mbedtls_md_init(&ctx);
  mbedtls_md_setup(&ctx, mbedtls_md_info_from_type(md_type), 0);
  mbedtls_md_starts(&ctx);
  mbedtls_md_update(&ctx, (const unsigned char *) payload, payloadLength);
  mbedtls_md_finish(&ctx, shaResult);
  mbedtls_md_free(&ctx);
  #ifdef MAUME_DEBUG // #endif 
    Serial.print("Hash: ");
    for(int i= 0; i< 32; i++)  {
      char str[3];
      sprintf(str, "%02x", (int)shaResult[i]);
      Serial.print(str);
    }
    Serial.println(".");
  #endif
  return (shaResult);
}
//------------------------------------------------------------------------------------------------------------------------
byte* IRAM_ATTR MauMeClass::getSha256Bytes(String payload){
  // Serial.println("Computing sha 256 of '"+payload+"'.");
  byte * shaResult = (byte*)malloc(sizeof(byte)*32);
  mbedtls_md_context_t ctx;
  mbedtls_md_type_t md_type = MBEDTLS_MD_SHA256;
  const size_t payloadLength = payload.length(); //strlen(payload);
  mbedtls_md_init(&ctx);
  mbedtls_md_setup(&ctx, mbedtls_md_info_from_type(md_type), 0);
  mbedtls_md_starts(&ctx);
  mbedtls_md_update(&ctx, (const unsigned char *) payload.c_str(), payloadLength);
  mbedtls_md_finish(&ctx, shaResult);
  mbedtls_md_free(&ctx);
  #ifdef MAUME_DEBUG // #endif
    Serial.print("Hash: ");
    for(int i= 0; i< 32; i++)  {
      char str[3];
      sprintf(str, "%02x", (int)shaResult[i]);
      Serial.print(str);
    }
  #endif
  return (shaResult);
}
//-------------------------------------------------------------------------------------------------------------------------
int IRAM_ATTR MauMeClass::countReceivedMessages(){
  int cnt = 0;
  File root = fileOpen(SPIFFS, MauMe.spiMutex, "/RECMES");
  if(!root){Serial.println(" ---------------------------------------------------- Failed to open directory");return 0;}
  if(!root.isDirectory()){Serial.println(" -------------------------------------------- Not a directory");return 0;}
  File file = getNextFile(MauMe.spiMutex, root);
  while (file) {
    LoRa_PKT * lrPkt = MauMe.loadLoRaPacket(String(file.name()));
    if(lrPkt == NULL){
      file = getNextFile(MauMe.spiMutex, root);
    }else{ // bool            compareBuffers(const char* buf1,const char* buf2, int len);
      MauMeHDR_0 * strHdr = (MauMeHDR_0 *)&lrPkt->DAT[0];
      if(compareBuffers((const char*)myByteAddress.dat, (const char*)&strHdr->DEST, MM_ADDRESS_SIZE)){
        cnt++;
      }
      file = getNextFile(MauMe.spiMutex, root);
    }
  }
  root.close();
  file.close();
  
  return cnt;
  //return countfiles("/RECMES");
}
//------------------------------------------------------------------------------------------------------------------------
int IRAM_ATTR MauMeClass::countNodeReceivedMessages(){
  return countfiles("/RECMES");
}
//------------------------------------------------------------------------------------------------------------------------
void MauMeClass::onDelivery(void(*callback)(int)){
  MauMe._onDelivery = callback;  
}
//------------------------------------------------------------------------------------------------------------------------
ADDRESS MauMeClass::zeroAddress(ADDRESS address){
  int i = 0;
  for(i=0;i<MM_ADDRESS_SIZE;i++){
    address.dat[i] = 0;
  }
  return address;
}
//------------------------------------------------------------------------------------------------------------------------
ADDRESS MauMeClass::getZeroAddress(){
  ADDRESS address;
  address = zeroAddress(address);
  return address;
}
//------------------------------------------------------------------------------------------------------------------------
String IRAM_ATTR MauMeClass::getContentType(String filename) {
       if (filename.endsWith(".htm"))   return "text/html";
  else if (filename.endsWith(".html"))  return "text/html";
  else if (filename.endsWith(".css"))   return "text/css";
  else if (filename.endsWith(".js"))    return "application/javascript";
  else if (filename.endsWith(".json"))  return "application/json";
  else if (filename.endsWith(".png"))   return "image/png";
  else if (filename.endsWith(".gif"))   return "image/gif";
  else if (filename.endsWith(".jpg"))   return "image/jpeg";
  else if (filename.endsWith(".ico"))   return "image/x-icon";
  else if (filename.endsWith(".xml"))   return "text/xml";
  else if (filename.endsWith(".pdf"))   return "application/x-pdf";
  else if (filename.endsWith(".zip"))   return "application/x-zip";
  else if (filename.endsWith(".gz"))    return "application/x-gzip";
  else if (filename.indexOf("download")>=0) return "application/octet-stream";
  return "text/plain";
}
//------------------------------------------------------------------------------------------------------------------------
bool IRAM_ATTR MauMeClass::compareBuffers(const char* buf1,const char* buf2, int len){
  while(len){
    if(buf1[len-1] != buf2[len-1]){return false;}
    len -= 1;
  }
  return true;
}
//------------------------------------------------------------------------------------------------------------------------
char IRAM_ATTR MauMeClass::hexStr2Num(char str[]){
  return (char) strtol(str, 0, 16);
}
//------------------------------------------------------------------------------------------------------------------------
String IRAM_ATTR MauMeClass::Mac2Str(char* buff){
  char dat[24] = {'\0'};
  sprintf(dat, "%02x:%02x:%02x:%02x:%02x:%02x", buff[0], buff[1], buff[2], buff[3], buff[4], buff[5]);
  return String(dat);
}
//------------------------------------------------------------------------------------------------------------------------
String IRAM_ATTR MauMeClass::addressBuffer2Str(char* buff, int len){
  char dat[(len+1)*2+1] = {'\0'};
  int u = 0;
  for(u=0; u<(len+1)*2+1;u++){dat[u] = 0;}
  int i = 0;
  for(i=0; i<len;i++){sprintf(dat+i*2, "%02x", buff[i]);} // Serial.printf(" Conv : %d.\n", buff[i]);
  dat[i*2] = '\0';
  return String(dat);
}
//------------------------------------------------------------------------------------------------------------------------
String IRAM_ATTR MauMeClass::addressBuffer2MacFormat(char* buff, int len){
  //Serial.println(" GETTING STRING FROM BUFFER : "+String(buff));
  char dat[(len+1)*3+1] = {'\0'};
  int u = 0;
  for(u=0; u<(len+1)*3+1;u++){dat[u] = 0;}
  int i = 0;
  for(i=0; i<len;i++){sprintf(dat+i*3, "%02x:", buff[i]);}
  //sprintf(dat+i*3, "%02x", buff[i]);
  dat[i*3-1] = '\0';
  //sprintf(dat+i*3, "%02x", buff[i]);
  return String(dat);
}
//------------------------------------------------------------------------------------------------------------------------
/*ADDRESS IRAM_ATTR MauMeClass::addressFromHexString(String str){
  Serial.println(" Tokenizing : " + str + ".");
  ADDRESS mac = getZeroAddress();
  String token = str.substring(0, 2);
  Serial.println(" Token : " + token+".");
  int i = 0;
  while( token!=NULL and token.length() > 0 and i<MM_ADDRESS_SIZE) {
    mac.dat[i] = hexStr2Num((char*)token.c_str());
    i++;
    token = str.substring(2*i+1, 2*i+3);    
    Serial.println(" Token : " + token+".");
  }
  return mac;
}*/
//------------------------------------------------------------------------------------------------------------------------
ADDRESS IRAM_ATTR MauMeClass::addressFromBytes(byte * str){
  ADDRESS mac = getZeroAddress();
  memcpy(&mac.dat[0], str, MM_ADDRESS_SIZE);
  return mac;
}
//------------------------xSemaphoreGiveRecursive(MauMe.spiMutex);  ------------------------------------------------------------------------------------------------
ADDRESS IRAM_ATTR MauMeClass::macFromMacStyleString(String str){
  ADDRESS mac = getZeroAddress();
  const char s[2] = ":";
  char * token;
  token = strtok((char*)str.c_str(), (const char*)s);
  int i = 0;
  while( token != NULL and i<MM_ADDRESS_SIZE) {
    mac.dat[i] = hexStr2Num(token);
    token = strtok(NULL, (const char*)s);
    i++;
  }
  return mac;
}
//------------------------------------------------------------------------------------------------------------------------
ADDRESS IRAM_ATTR MauMeClass::addressFromMacStyleString(String str){
  ADDRESS mac = getZeroAddress();
  const char s[2] = ":";
  char * token;
  token = strtok((char*)str.c_str(), (const char*)s);
  int i = 0;
  while( token != NULL and i<MM_ADDRESS_SIZE) {
    mac.dat[i] = hexStr2Num(token);
    token = strtok(NULL, (const char*)s);
    i++;
  }
  return mac;
}
//------------------------------------------------------------------------------------------------------------------------
String IRAM_ATTR MauMeClass::recPkt2Html(LoRa_PKT * lrPkt){
  String outStr = "Void";
  if(lrPkt != NULL){
    outStr = "";// <b>CRC</b>: "+String(mmpkt->HDR.CRC32)+", 
    MauMePKT_0 *mmpkt =  (MauMePKT_0 *)&lrPkt->DAT[0]; // <b> * MauMe - Message </b>:&#160;//To    </b>: "+addressBuffer2MacFormat(mmpkt->HDR.DEST, MM_ADDRESS_SIZE)+"<br/>&#13;<b>
    outStr = "<span>NHP "+String(mmpkt->HDR.NHP)+".<br/>&#13;<b>From</b>: "+addressBuffer2MacFormat(mmpkt->HDR.SEND, MM_ADDRESS_SIZE)+"<br/>&#13;<b>Text: </b>"+String((char*)&mmpkt->LOAD[0])+"</span>";
  }else{Serial.println(" #< ! NULL packet !");}
  return outStr;
}
//------------------------------------------------------------------------------------------------------------------------
String IRAM_ATTR MauMeClass::pkt2Html(LoRa_PKT * lrPkt){
  String outStr = "Void";
  if(lrPkt != NULL){
    outStr = "";// CRC</b>: "+String(mmpkt->HDR.CRC32)+"<br/>&#13;
    MauMePKT_0 *mmpkt = (MauMePKT_0 *)&lrPkt->DAT[0]; //  <b> * MauMe - Message </b>:&#160;<br/>&#13;<b> // <b>From</b>: "+addressBuffer2MacFormat(mmpkt->HDR.SEND, MM_ADDRESS_SIZE)+"<br/>&#13;
    outStr = "<span><b>To  </b>: "+addressBuffer2MacFormat(mmpkt->HDR.DEST, MM_ADDRESS_SIZE)+"<br/>&#13;<b>Text: </b>"+String((char*)&mmpkt->LOAD[0])+"</span>";
  }else{Serial.println(" #< ! NULL packet !");}
  return outStr;
}
//------------------------------------------------------------------------------------------------------------------------
String IRAM_ATTR MauMeClass::mes2Str(MauMePKT_0 *mmpkt){
  if(mmpkt != NULL){
    String outStr = "";
    outStr = "MauMeMessage (NHP "+String(mmpkt->HDR.NHP)+", CRC32:"+String(mmpkt->HDR.CRC32)+", FRMT:"+String(mmpkt->HDR.FRMT)+", LOADSIZE:"+String(mmpkt->HDR.LOADSIZE)+"):\nFrom: "+addressBuffer2MacFormat(mmpkt->HDR.SEND, MM_ADDRESS_SIZE)+"\nTo  : "+addressBuffer2MacFormat(mmpkt->HDR.DEST, MM_ADDRESS_SIZE)+"\nMess: "+String((char*)&mmpkt->LOAD[0])+"";
    return outStr;
  }else{Serial.println(" #< ! NULL packet !");}
  return " Void packet ";
}
//------------------------------------------------------------------------------------------------------------------------
String IRAM_ATTR MauMeClass::pkt2Str(LoRa_PKT * lrPkt){
  if(lrPkt != NULL){
    MauMePKT_0 * mmPkt = MauMe.parseMMPktFromLoRa(lrPkt);
    if(mmPkt != NULL){
      String outStr = "MauMeMessage (NHP "+String(mmPkt->HDR.NHP)+", CRC32:"+String(mmPkt->HDR.CRC32)+", FRMT:"+String(mmPkt->HDR.FRMT)+", LOADSIZE:"+String(mmPkt->HDR.LOADSIZE)+"):\nFrom: "+addressBuffer2MacFormat(mmPkt->HDR.SEND, MM_ADDRESS_SIZE)+"\nTo  : "+addressBuffer2MacFormat(mmPkt->HDR.DEST, MM_ADDRESS_SIZE)+"\nMess: "+String((char*)&mmPkt->LOAD[0])+"";
      return outStr;  
    }else{Serial.println(" #< ! NOT MAUME PACKET !");}
  }else{Serial.println(" #< ! NULL packet !");}
  return " VOID OR WRONG PACKET ";
}
//------------------------------------------------------------------------------------------------------------------------
LoRa_PKT * IRAM_ATTR MauMeClass::loadLoRaPacket(String path){ // MauMeACK_0 * loadLoRaACK(String path)
  LoRa_PKT * lrPkt = NULL;
  File file = fileOpenFor(SPIFFS, MauMe.spiMutex, path.c_str(), FILE_READ); 
  if(file){
    lrPkt = (LoRa_PKT *)malloc(sizeof(LoRa_PKT));
    size_t nbRd = readSomeBytes(MauMe.spiMutex, file, (char*)lrPkt, (size_t)sizeof(LoRa_PKT));
    if(nbRd <= 0){free(lrPkt);lrPkt = NULL;}
    file.close();
  }
  return lrPkt;
}
//-----------------------------------------------------------------------------------------------------------------------
LoRa_PKT * IRAM_ATTR MauMeClass::freeAndGetNext(LoRa_PKT * list){
  LoRa_PKT * next = list->next;
  free(list);
  return next;
}
//-----------------------------------------------------------------------------------------------------------------------
LoRa_PKT * IRAM_ATTR MauMeClass::getReceivedPackets(){ 
  String path = "";
  LoRa_PKT * headPkt = (LoRa_PKT *)malloc(sizeof(LoRa_PKT));
  headPkt->next = NULL;
  LoRa_PKT * lrPkt = headPkt;
  File root = fileOpen(SPIFFS, MauMe.spiMutex, "/RECMES");
  if(!root){Serial.println(" ---------------------------------------------------- Failed to open directory");free(headPkt);return NULL;}
  if(!root.isDirectory()){Serial.println(" -------------------------------------------- Not a directory");free(headPkt);return NULL;}
  File file = getNextFile(MauMe.spiMutex, root);
  while (file) {
    lrPkt->next = MauMe.loadLoRaPacket(String(file.name()));
    if(lrPkt == NULL){
      file = getNextFile(MauMe.spiMutex, root);
    }else{ // bool            compareBuffers(const char* buf1,const char* buf2, int len);
      MauMeHDR_0 * strHdr = (MauMeHDR_0 *)&lrPkt->DAT[0];
      if(compareBuffers((const char*)myByteAddress.dat, (const char*)&strHdr->DEST, MM_ADDRESS_SIZE)){
        lrPkt = lrPkt->next;        
      }
      file = getNextFile(MauMe.spiMutex, root);
    }
  }
  root.close();
  file.close();
  lrPkt = headPkt->next;free(headPkt);
  return lrPkt;
}

//----------------------------------------------------------------------------------------------------------------------------
String IRAM_ATTR MauMeClass::listReceived2Str(fs::FS &fs, SemaphoreHandle_t mutex, const char * dirname, unsigned char appType, ADDRESS macFrom){
  String str = "<h3>&#160;&#160;Received SMS&#160;:</h3>";
  File root = fileOpen(fs, mutex, dirname);
  if(!root){Serial.println(" ---------------------------------------------------- Failed to open directory");return "";}
  if(!root.isDirectory()){Serial.println(" -------------------------------------------- Not a directory");return "";}
  File file = getNextFile(mutex, root);
  if(file){
    while (file) {
      LoRa_PKT * lrPkt = NULL;
      lrPkt = loadLoRaPacket(String(file.name()));
      if(lrPkt != NULL){
        MauMeHDR_0 * strHdr = (MauMeHDR_0 *)&lrPkt->DAT[0];
        if(strHdr->APPTYP == appType){
          if(compareBuffers((const char*)myByteAddress.dat, (const char*)&strHdr->DEST, MM_ADDRESS_SIZE) || compareBuffers((const char*)macFrom.dat, (const char*)&strHdr->DEST, MM_ADDRESS_SIZE)){
            str +=  recPkt2Html(lrPkt);
            str += "<br/>&#13;__________________________________________<br/>&#13;";
          }
        }
        free(lrPkt);lrPkt = NULL;
      }
      file = getNextFile(mutex, root);
    }
  }else{str += "------------- NONE -------------<br/>\r\n";}
  root.close();
  file.close();
  return str;
}
//----------------------------------------------------------------------------------------------------------------------------
String IRAM_ATTR MauMeClass::listSent2Str(fs::FS &fs, SemaphoreHandle_t mutex, const char * dirname, unsigned char appType, ADDRESS macFrom){
  String str = "<h3>&#160;&#160;Sent SMS&#160;:</h3>";
  File root = fileOpen(fs, mutex, dirname);
  if(!root){Serial.println(" #< Failed to open directory");return "";}
  if(!root.isDirectory()){Serial.println(" #< Not a directory");return "";}
  File file = getNextFile(mutex, root);
  if(file){
    while (file) {
      LoRa_PKT * lrPkt = NULL;
      lrPkt = loadLoRaPacket(String(file.name()));
      if(lrPkt != NULL){
        MauMeHDR_0 * strHdr = (MauMeHDR_0 *)&lrPkt->DAT[0];
        if(strHdr->APPTYP == appType){
          if(compareBuffers((const char*)myByteAddress.dat, (const char*)&strHdr->SEND, MM_ADDRESS_SIZE) || compareBuffers((const char*)macFrom.dat, (const char*)&strHdr->SEND, MM_ADDRESS_SIZE)){
            str +=  pkt2Html(lrPkt);
            bool isSent = fileExists(SPIFFS, mutex, String("/ACKSSENT/"+String(lrPkt->CRC32)+".bin").c_str());
            bool isAck = fileExists(SPIFFS, mutex, String("/NODEACKS/"+String(lrPkt->CRC32)+".bin").c_str());
            str += "<br/>&#13;__________________ ";
            if(isSent or isAck){str += " Sent";}
            //if(isAck){str += " & ";}
            if(isAck){str += " & Received.";}
            /*else{
              str += "<br/>&#13;__________________________________________";
            }*/
            str += "<br/>&#13;";
          }
        }
        free(lrPkt);lrPkt = NULL;
      }
      file = getNextFile(mutex, root);
    } // &#13;__________________________________________<br/>&#13;
  }else{str += "------------- NONE -------------<br/>\r\n";}
  root.close();
  file.close();
  return str;
}
//------------------------------------------------------------------------------------------------------------------------
uint32_t MauMeClass::getCRC32(String str){
  return crc32_le(MauMe.crcState, (const uint8_t*)str.c_str(), str.length());
}
//------------------------------------------------------------------------------------------------------------------------
uint32_t IRAM_ATTR MauMeClass::getCRC32FromChars(char * str, int len){
  /*Serial.println("-----------------------------------------\n Calculating CRC32 for : "+String(len)+" bytes:");
  Serial.write((const uint8_t*)str, len);
  Serial.println("\n-----------------------------------------\n");*/
  return crc32_le(MauMe.crcState, (const uint8_t*)str, len);
}
//------------------------------------------------------------------------------------------------------------------------
/*
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
*/
int IRAM_ATTR MauMeClass::saveLoRaPkt(const char * dirname, LoRa_PKT * lrPkt, bool overWrite){
  String path = String(dirname)+String(lrPkt->CRC32)+".bin";
  int ret = 0;
  if(lrPkt != NULL){
    bool already = fileExists(SPIFFS, MauMe.spiMutex, path.c_str());
    if(overWrite || !already){
      LoRa_PKT * next = lrPkt->next;
      lrPkt->next = NULL;    
      char dat[512] = {'\0'};
      memcpy(&dat, (char*)lrPkt, sizeof(LoRa_PKT));
      ret = writeBytesToFile(SPIFFS, MauMe.spiMutex, (char*)path.c_str(), (byte *)&dat[0], sizeof(LoRa_PKT));
      lrPkt->next = next;
      if(ret <= 0){Serial.println(" #< Write failed !");}else{
        if(overWrite && already){
          // MauMe.Log("-UPDT PKT:"+String(dirname)+String(lrPkt->CRC32)+":"+getPKTType(lrPkt->PCKTTYP)+"(TYP"+String(lrPkt->PCKTTYP)+",PW"+String(lrPkt->TX_POW)+",TX"+String(lrPkt->NB_TX)+","+String(lrPkt->NB_MSG)+"MSG,"+String(lrPkt->NB_ACK)+"ACK)");
        }else if(!already){
          // MauMe.Log("-SAVE PKT:"+String(dirname)+String(lrPkt->CRC32)+":"+getPKTType(lrPkt->PCKTTYP)+"(TYP"+String(lrPkt->PCKTTYP)+",PW"+String(lrPkt->TX_POW)+",TX"+String(lrPkt->NB_TX)+","+String(lrPkt->NB_MSG)+"MSG,"+String(lrPkt->NB_ACK)+"ACK)");
        }/*else if(!overWrite && !already){
          MauMe.Log("-SAVE PKT:"+dirname+":"+String(lrPkt->CRC32)+"("+String(lrPkt->PCKTTYP)+","+String(lrPkt->TX_POW)+","+String(lrPkt->NB_TX)+","+String(lrPkt->NB_MSG)+","+String(lrPkt->NB_ACK)+")");
        }*/
      }
    }else{
      #ifdef MAUME_DEBUG // #endif
        Serial.println(" <>  File already exists !");
      #endif
      }
  }else{Serial.println(" #< Packet NULL !");}
  return ret;
}
//------------------------------------------------------------------------------------------------------------------------
/*
  typedef struct PACKED {
    unsigned char  HTL;
    uint32_t     CRC32;
    unsigned char FRMT;
    unsigned char TYPE;
    uint32_t      HASH;
  } MauMeACK_0;
*/
int MauMeClass::saveLoRaACK_0(const char * dirname, MauMeACK_0 * mmAck, bool overWrite){ // "/PKTS/"
  String path = String(dirname)+String(mmAck->HASH)+".bin";
  int ret = 0;
  bool already = false;
  if(!overWrite){already = fileExists(SPIFFS, MauMe.spiMutex, path.c_str());}
  if(mmAck != NULL && (overWrite || !already)){
    ret = writeBytesToFile(SPIFFS, MauMe.spiMutex, (char*)path.c_str(), (byte *)mmAck, sizeof(MauMeACK_0));
    if(ret <= 0){
      Serial.println(" #< ACK Write failed");
    }else{
      if(overWrite && already){
          // MauMe.Log("-UPDT ACK:"+String(dirname)+String(mmAck->CRC32)+":"+MauMe.getACKType(mmAck->TYPE)+"(HTL"+String(mmAck->HTL)+",HSH"+String(mmAck->HASH)+",FRMT"+String(mmAck->FRMT)+",TYP"+String(mmAck->TYPE)+")");
        }else if(!already){
          // MauMe.Log("-SAVE ACK:"+String(dirname)+String(mmAck->CRC32)+":"+MauMe.getACKType(mmAck->TYPE)+"(HTL"+String(mmAck->HTL)+",HSH"+String(mmAck->HASH)+",FRMT"+String(mmAck->FRMT)+",TYP"+String(mmAck->TYPE)+")");
        }
    }
  }else{
    #ifdef MAUME_DEBUG // #endif
      Serial.println(" #< ACK NULL or file already exists ! ("+path+")");
    #endif
    }    
  return ret;
}
//------------------------------------------------------------------------------------------------------------------------
MauMeACK_0 * MauMeClass::loadLoRaACK(String path){
  MauMeACK_0 * mmAckt = NULL;
  File file = fileOpenFor(SPIFFS, MauMe.spiMutex, path.c_str(), FILE_READ); 
  if(file){
    mmAckt = (MauMeACK_0 *)malloc(sizeof(MauMeACK_0));
    size_t nbRd = readSomeBytes(MauMe.spiMutex, file, (char *)mmAckt, (size_t)sizeof(MauMeACK_0));
    if(nbRd <= 0){
      Serial.println(" #<                                                               ERROR !");
      free(mmAckt);mmAckt = NULL;
    }
    file.close();
  }
  return mmAckt;
}
//------------------------------------------------------------------------------------------------------------------------
MauMePKT_0 * MauMeClass::parseMMPktFromBuffer(char* dat, int len){
  if(len >= sizeof(MauMeHDR_0)){
    MauMeHDR_0 * strHdr = (MauMeHDR_0 *)dat;
    switch(strHdr->FRMT){
      case HDR0_FRMT:
        if(strHdr->LOADSIZE<=MAX_MSG_SIZE){
          int pcktLen = sizeof(MauMeHDR_0) + strHdr->LOADSIZE;
          if(len >= pcktLen){
            uint32_t newCRC32 = MauMe.getCRC32FromChars((char*)&strHdr->FRMT, pcktLen - HDR0_FRMT_POS);
            if(newCRC32 == strHdr->CRC32){
              MauMePKT_0 * mmPkt = (MauMePKT_0 *)malloc(sizeof(MauMePKT_0));
              memcpy(mmPkt, dat, pcktLen);
              return mmPkt;       
            }else{Serial.println(" #< Invalid packet CRC32.");}
          }else{Serial.println(" #< Invalid packet length ("+String(len)+", "+String(pcktLen)+").");}
        }else{Serial.println(" #< Invalid payload size ("+String(strHdr->LOADSIZE)+").");}
        break;
      default:
        Serial.println(" #< UNKNOWN PKT FORMAT : "+String(strHdr->FRMT));
    }
  }else{Serial.println(" #< Not MauMe Packet");}
  //}else{Serial.println("Invalid Header Start Key.");}
  return NULL;
}
//----------------------------------------------------------------------------------------------------------------
MauMePKT_0 * MauMeClass::parseMMPktFromLoRa(LoRa_PKT * lrPkt){
  MauMePKT_0 * mmPkt = NULL;
  mmPkt = MauMe.parseMMPktFromBuffer(&lrPkt->DAT[0], lrPkt->PKTSIZ);
  if(mmPkt == NULL){lrPkt->PCKTTYP = MM_PKT_TYPE_GEN;}else{lrPkt->PCKTTYP = MM_PKT_TYPE_MMM;}
  return mmPkt;
}
//------------------------------------------------------------------------------------------------------------------------
LoRa_PKT * IRAM_ATTR MauMeClass::getLoRaPacketFromMMPKT(MauMePKT_0 * mmPkt){
  LoRa_PKT * lrPkt = (LoRa_PKT *)malloc(sizeof(LoRa_PKT));
  byte * pnt = (byte*)lrPkt;
  for(int i=0; i<sizeof(LoRa_PKT); i++){pnt[i] = 0;}
  lrPkt->PKTSIZ = sizeof(MauMeHDR_0) + mmPkt->HDR.LOADSIZE;
  memcpy(&lrPkt->DAT[0], mmPkt, lrPkt->PKTSIZ);  
  lrPkt->CRC32 = mmPkt->HDR.CRC32;
  lrPkt->PCKTTYP = MM_PKT_TYPE_MMM;
  lrPkt->TX_POW = MM_TX_POWER_MIN;
  lrPkt->NB_TX = 0;
  lrPkt->NB_MSG = 1;
  lrPkt->NB_ACK = 0;
  lrPkt->next = NULL;
  return lrPkt;
}
//------------------------------------------------------------------------------------------------------------------------
uint32_t MauMeClass::get_MMPKT_0_CRC32(MauMePKT_0 *mmpkt){
  return MauMe.getCRC32FromChars((char*)&mmpkt->HDR.FRMT, sizeof(MauMeHDR_0) + mmpkt->HDR.LOADSIZE-HDR0_FRMT_POS);
}
//------------------------------------------------------------------------------------------------------------------------  
//    MMM | NHP | FRMT | DEST | SEND | APPTYP | LOADSIZE | LOAD | CRC32
MauMePKT_0 * IRAM_ATTR MauMeClass::createPacket(ADDRESS macFrom, ADDRESS addressTo, String message, unsigned char pktType){
  MauMePKT_0 * mmPkt = NULL;
  if(message.length() > 0 && message.length() < 101){
    mmPkt = (MauMePKT_0 *)malloc(sizeof(MauMePKT_0));
    byte * pnt = (byte*)mmPkt;
    for(int i=0; i<sizeof(MauMePKT_0); i++){pnt[i] = 0;}
    mmPkt->HDR.NHP = (unsigned char)(0);
    mmPkt->HDR.FRMT = (unsigned char)(HDR0_FRMT);
    mmPkt->HDR.APPTYP = pktType;
    memcpy(&mmPkt->HDR.SEND, &macFrom.dat, MM_ADDRESS_SIZE);
    memcpy(&mmPkt->HDR.DEST, &addressTo.dat, MM_ADDRESS_SIZE);
    mmPkt->HDR.LOADSIZE = (unsigned char)(message.length()+1);
    memcpy(&mmPkt->LOAD[0], message.c_str(), mmPkt->HDR.LOADSIZE);  
    mmPkt->LOAD[mmPkt->HDR.LOADSIZE-1] = 0;  
    #ifdef MAUME_DEBUG // #endif
      Serial.println("-----------------------------------------\n Computing CRC on :");
      Serial.write((const uint8_t*)&mmPkt->HDR.FRMT, sizeof(MauMeHDR_0) + mmPkt->HDR.LOADSIZE - HDR0_FRMT_POS);
      Serial.println("\n-----------------------------------------\n");
    #endif
    mmPkt->HDR.CRC32 = MauMe.getCRC32FromChars((char*)&mmPkt->HDR.FRMT, sizeof(MauMeHDR_0) + mmPkt->HDR.LOADSIZE - HDR0_FRMT_POS);
    #ifdef MAUME_DEBUG // #endif
      Serial.println("________________________\nPacket created:\n"+mes2Str(mmPkt)+"\n________________________");
      Serial.println("-----------------------------------------\n CREATED PKT DAT : "+String(sizeof(MauMeHDR_0) + mmPkt->HDR.LOADSIZE)+" bytes:");
      Serial.write((const uint8_t*)&mmPkt->LOAD[0], sizeof(MauMeHDR_0) + mmPkt->HDR.LOADSIZE);
      Serial.println("\n-----------------------------------------\n");
    #endif
  }
  return mmPkt;
}
//------------------------------------------------------------------------------------------------------------------------ b0:a2:e7:0e:9e:af
MauMePKT_0 * MauMeClass::createDummyPacket(String text){
  MauMePKT_0 * mmPkt = (MauMePKT_0 *)malloc(sizeof(MauMePKT_0));
  byte * pnt = (byte*)mmPkt;
  for(int i=0; i<sizeof(MauMePKT_0); i++){
    pnt[i] = 0;
  }
  mmPkt->HDR.LOADSIZE = (unsigned char)(text.length()+1);
  memcpy(&(mmPkt->LOAD[0]), text.c_str(), mmPkt->HDR.LOADSIZE-1);  
  mmPkt->LOAD[mmPkt->HDR.LOADSIZE-1] = '\0';  
  mmPkt->HDR.NHP = (unsigned char)(0);
  mmPkt->HDR.FRMT = (unsigned char)(HDR0_FRMT);
  mmPkt->HDR.APPTYP = (unsigned char)MMM_APPTYP_NOD;
  ADDRESS addressTo = MauMe.getZeroAddress();    
  if(MauMe.dummyTargetAddress.length() > DUMMY_PROCESS_MIN_ADRESS_SIZE){
    addressTo = macFromMacStyleString(MauMe.dummyTargetAddress);
  }else{
    switch(rand()%5){
      case 0:
        addressTo = macFromMacStyleString("26:6e:1c:b2:fc:dc:81:a7:4d:5f:4b:1f");        break;
      case 1:
        addressTo = macFromMacStyleString("c8:12:c7:e6:89:01:b1:3b:5f:fd:db:b5");        break;
      //case 2:
      //  addressTo = macFromMacStyleString("24:6F:28:2B:12:EC");        break;
      case 3:
        addressTo = macFromMacStyleString("7f:61:b2:dd:0d:c2:ca:bf:2d:aa:3e:54");        break;
      case 4:
        addressTo = macFromMacStyleString("d3:3f:84:fd:f0:49:f0:26:8b:bb:e7:de");        break;
      case 5:
        addressTo = macFromMacStyleString("ff:09:aa:ae:d7:89:73:de:69:74:1e:b9");//7C:D6:61:33:64:D2");        break;
      default:
        addressTo = macFromMacStyleString("ea:b9:dd:dc:5e:1d:41:13:12:c6:0c:8f");        break;
    }
  }
  memcpy(&mmPkt->HDR.DEST, &addressTo.dat[0],     MM_ADDRESS_SIZE);
  memcpy(&mmPkt->HDR.SEND, &myByteAddress.dat[0], MM_ADDRESS_SIZE);
  mmPkt->HDR.CRC32 = MauMe.getCRC32FromChars((char*)mmPkt + HDR0_FRMT_POS, sizeof(MauMeHDR_0) + mmPkt->HDR.LOADSIZE - HDR0_FRMT_POS);
  Serial.println("________________________\nPacket created:\n"+mes2Str(mmPkt)+"\n________________________");
  return mmPkt;
}
//------------------------------------------------------------------------------------------------------------------------
LoRa_PKT * MauMeClass::createDummyLoRaPacket(String text){
  return MauMe.getLoRaPacketFromMMPKT(createDummyPacket(text));
}
//------------------------------------------------------------------------------- IRAM_ATTR
int MauMeClass::sendLoRaBuffer(char* outgoing, int len) {
  LoRa_JMM.beginPacket();                           
  LoRa_JMM.write((const uint8_t*)outgoing, len);   
  return LoRa_JMM.endPacket();
}
//--------------------------------------------------------------------------------
String MauMeClass::getPKTType(unsigned char type){
  switch(type){
    case MM_PKT_TYPE_UKN:
      return "PKT_TYPE_UKN";
    case MM_PKT_TYPE_GEN:
      return "PKT_TYPE_GEN";
    case MM_PKT_TYPE_MMM:
      return "PKT_TYPE_MMM";
    case MM_PKT_TYPE_PILE:
      return "PKT_TYPE_PILE";
    case MM_PKT_TYPE_ACK_OF_PILE:
      return "PKT_TYPE_ACK_OF_PILE";
    case MM_PKT_TYPE_CMD:
      return "PKT_TYPE_CMD";
    default:
      return "UNDEFINED PKT TYPE"; // MM_PKT_TYPE_PILEACK
  }
}
//--------------------------------------------------------------------------------
String MauMeClass::getACKType(unsigned char type){
  switch(type){
    case ACK0_TYPE_SH:
      return "ACK0_TYPE_SH";
    case ACK0_TYPE_RC:
      return "ACK0_TYPE_RC";
    case ACK0_TYPE_PL:
      return "ACK0_TYPE_PL";
    default:
      return "UNDEFINED ACK TYPE"; // MM_PKT_TYPE_PILEACK
  }
}
//------------------------------------------------------------------------------------------------------------------------
int MauMeClass::sendLoRaPKT(LoRa_PKT * lrPkt, bool save){
  if(lrPkt->TX_POW > MauMe.MM_TX_POWER_MAX){
    lrPkt->TX_POW = MauMe.MM_TX_POWER_MIN;
  }
  #ifdef MAUME_DEBUG // #endif
    Serial.println(" ->               Setting power : "+String(lrPkt->TX_POW)); 
    Serial.println(" ->         Sending packet type : "+MauMe.getPKTType(lrPkt->PCKTTYP)); 
    Serial.println(" ->         Sending packet size : "+String(lrPkt->PKTSIZ)); 
    Serial.println(" -> Sending packet with nb MSG  : "+String(lrPkt->NB_MSG)); 
    Serial.println(" -> Sending packet with nb ACKs : "+String(lrPkt->NB_ACK)); 
  #endif
  LoRa_JMM.setTxPower(lrPkt->TX_POW);
  bool ret = MauMe.sendLoRaBuffer((char*)&lrPkt->DAT[0], (int)lrPkt->PKTSIZ);
  if(ret){
    lrPkt->NB_TX += 1;      //    
    MauMe.Log("-SEND PKT: "+String(lrPkt->CRC32)+":"+MauMe.getPKTType(lrPkt->PCKTTYP)+"(TYP"+String(lrPkt->PCKTTYP)+",PW"+String(lrPkt->TX_POW)+",TX"+String(lrPkt->NB_TX)+","+String(lrPkt->NB_MSG)+"MSG,"+String(lrPkt->NB_ACK)+"ACK)");
  }
  if(save){
    #ifdef MAUME_DEBUG // #endif
      Serial.println(" -> Needs saving updated packet."); 
    #endif
    if(MauMe.saveLoRaPkt("/PKTS/", lrPkt, true)){
      #ifdef MAUME_DEBUG // #endif
        Serial.println(" -> Saved updated packet.");                  
      #endif
    }else{Serial.println(" #< Failed saving updated packet !");}
  }
  return ret;
}
//------------------------------------------------------------------------------------------------------------------------
MauMeACK_0 * MauMeClass::makeMauMeACK_0(unsigned char htl, unsigned char frmt, unsigned char type, uint32_t hash){
  MauMeACK_0 * mmAck = (MauMeACK_0 *)malloc(sizeof(MauMeACK_0));
  mmAck->HTL = htl;
  mmAck->FRMT = frmt;
  mmAck->TYPE = type;
  mmAck->HASH = hash;
  mmAck->CRC32 = MauMe.getCRC32FromChars((char*)&mmAck->FRMT, 6);
  return mmAck;
}
//------------------------------------------------------------------------------------------------------------------------ 
String MauMeClass::ack2Str(MauMeACK_0 * mmAck){  // "): FROM: "+String(addressBuffer2MacFormat(mmAck->ORIG, MM_ADDRESS_SIZE))+
  String outStr = "MauMeACK (HTL "+String(mmAck->HTL)+", CRC32:"+String(mmAck->CRC32)+", FRMT:"+String(mmAck->FRMT)+", TYPE:"+String(mmAck->TYPE)+"): HASH: "+String(mmAck->HASH)+"";
  return outStr;
}
//------------------------------------------------------------------------------------------------------------------------
//  #define ACK0_FRMT 1//  #define ACK1_FRMT 2//  #define ACK0_TYPE_SH 1//  #define ACK0_TYPE_RC  2
MauMeACK_0 * MauMeClass::parseMMAckFromBuffer(char* dat, int len){
  if(len >= sizeof(MauMeACK_0)){
    MauMeACK_0 * strAck = (MauMeACK_0 *)dat;
    switch(strAck->FRMT){
      case ACK0_FRMT:{
        uint32_t newCRC32 = MauMe.getCRC32FromChars((char*)&strAck->FRMT, 6);
        if(newCRC32 == strAck->CRC32){
          //Serial.println(" ! Valid ACK 0 format !");
          MauMeACK_0 * mmAck = (MauMeACK_0 *)malloc(sizeof(MauMeACK_0));
          memcpy(mmAck, dat, sizeof(MauMeACK_0));
          return mmAck;       
        }else{Serial.println(" #< Invalid ACK CRC32.");}          
      }break;
      
      default:{
        #ifdef MAUME_DEBUG // #endif
          Serial.println(" #< UNKNOWN ACK FORMAT : "+String(strAck->FRMT));
        #endif
      }break;
    }    
  }else{
    #ifdef MAUME_DEBUG // #endif
      Serial.println(" #< Invalid Header Size.");
    #endif
  }
  return NULL;
}
//------------------------------------------------------------------------------------------------------------------------
MauMePKT_0 * MauMeClass::parseMMPktFromString(String str){
  return MauMe.parseMMPktFromBuffer((char*)str.c_str(), str.length());
}
//---------------------------------------------------------------------------------------------
/* typedef struct PACKED {
    unsigned char  HTL;
    uint32_t     CRC32;
    uint32_t        ID;
    unsigned char FRMT;
    unsigned char  CMD;   
    time_t           T;     
   }MauMeCMD_0;    
*/
//---------------------------------------------------------------------------------------------
MauMeCMD_0 * IRAM_ATTR MauMeClass::makeCommand(unsigned char  HTL, unsigned char FRMT, unsigned char  CMD, time_t T){
  MauMeCMD_0 * mmCmd = (MauMeCMD_0 *)malloc(sizeof(MauMeCMD_0));
  mmCmd->HTL = HTL; // uint32_t     CRC32
  mmCmd->FRMT = FRMT;
  mmCmd->CMD = CMD;
  mmCmd->T = T;
  mmCmd->ID = MauMe.getCRC32FromChars((char*)&mmCmd->FRMT, 2 + sizeof(time_t));
  mmCmd->CRC32 = MauMe.getCRC32FromChars((char*)&mmCmd->FRMT, 2 + sizeof(time_t) + sizeof(uint32_t));
  return mmCmd;
}
//----------------------------------------------------------------------------------------------
MauMeCMD_0 * MauMeClass::parseCommand(char* dat, bool check){
  MauMeCMD_0 * mmCmd = (MauMeCMD_0 *)malloc(sizeof(MauMeCMD_0));
  memcpy(&mmCmd->CMD, dat, sizeof(MauMeCMD_0));
  uint32_t CRC32 = MauMe.getCRC32FromChars((char*)&mmCmd->FRMT, 2 + sizeof(time_t) + sizeof(uint32_t));
  if(CRC32 == mmCmd->CRC32){
    return mmCmd;  
  }else{
    free(mmCmd);  
    return NULL;  
  }
}
//----------------------------------------------------------------------------------------------
MauMeCMD_0 * MauMeClass::updateCommandTime(MauMeCMD_0 * mmCmd){
  if(mmCmd != NULL){
    Serial.println(" ->  Time was : "+getTimeString(time(NULL))+".");
    mmCmd->T = time(NULL);
    mmCmd->CRC32 = MauMe.getCRC32FromChars((char*)&mmCmd->FRMT, 2 + sizeof(time_t)+ sizeof(uint32_t));
    Serial.println(" ->  Time is  : "+getTimeString(mmCmd->T)+".");
    return mmCmd;  
  }else{
    return NULL;
  }
}
//----------------------------------------------------------------------------------------------
LoRa_PKT * MauMeClass::updateCommandPacketTime(LoRa_PKT * lrPkt){
  if(lrPkt != NULL){
    MauMeCMD_0 * mmCmd = (MauMeCMD_0 *)&lrPkt->DAT[0];
    MauMe.updateCommandTime(mmCmd);    
    return lrPkt;
  }else{
    return NULL;
  }
}
//----------------------------------------------------------------------------------------------
MauMeCMD_0 * MauMeClass::getCommand(char * dat){
  String str = String(dat);
  int pos = str.indexOf("MauMeCommand");
  Serial.println(" dat[&CMD] = "+String(dat[pos + strlen("MauMeCommand")])+".");
  dat += pos + strlen("MauMeCommand");
  return MauMe.parseCommand(dat, true);
}
//----------------------------------------------------------------------------------------------
/*  MMM_APPTYP_CMD  MM_PKT_TYPE_CMD  */
LoRa_PKT * MauMeClass::getPacketFromCommand(MauMeCMD_0 * mmCmd){
  LoRa_PKT * lrPkt = (LoRa_PKT *)malloc(sizeof(LoRa_PKT));
  byte * pnt = (byte*)lrPkt;
  for(int i=0; i<sizeof(MauMeCMD_0); i++){pnt[i] = 0;}
  lrPkt->PKTSIZ = sizeof(MauMeCMD_0);
  memcpy(&lrPkt->DAT[0], mmCmd, lrPkt->PKTSIZ);  
  lrPkt->CRC32 = mmCmd->ID;
  lrPkt->PCKTTYP = MM_PKT_TYPE_CMD;
  lrPkt->TX_POW = MM_TX_POWER_MIN;
  lrPkt->NB_TX = 0;
  lrPkt->NB_MSG = 0;
  lrPkt->NB_ACK = 0;
  lrPkt->next = NULL;
  return lrPkt;
}
//----------------------------------------------------------------------------------------------
/*  MMM_APPTYP_CMD  MM_PKT_TYPE_CMD  */
MauMeCMD_0 * MauMeClass::getComandFromPacket(LoRa_PKT * lrPkt){
  MauMeCMD_0 * mmCmd = (MauMeCMD_0 *)malloc(sizeof(MauMeCMD_0));
  memcpy(mmCmd, &lrPkt->DAT[0], sizeof(MauMeCMD_0));
  return mmCmd;
}
//---------------------------------------------------------------------------------------------
LoRa_PKT * IRAM_ATTR MauMeClass::getCurrentLoRaPkt(){
  char dat[256] = {0};
  for(int i=0; i<256;i++){dat[i] = 0;}
  delay(LORA_READ_DELAY);
  int avail = LoRa_JMM.available();
  int pos = 0;
  while(avail){
     size_t rd = LoRa_JMM.readBytes((uint8_t *)&dat[pos], avail);
     pos += rd;
     delay(LORA_READ_DELAY);
     avail = LoRa_JMM.available();
  }
  if(pos>0){
    // #ifdef MAUME_DEBUG // #endif
      Serial.println(" ! Received "+String(pos)+" bytes ! -> Packet RSSI : "+String(LoRa_JMM.packetRssi()));
    // #endif
  }
  pos = (pos>MAX_PKT_SIZE)?MAX_PKT_SIZE:pos;
  LoRa_PKT * lrPkt = (LoRa_PKT *)malloc(sizeof(LoRa_PKT));
  memcpy(&lrPkt->DAT[0], &dat[0], pos);
  lrPkt->PKTSIZ = pos;//((dat.length() > 255)?255:dat.length());
  lrPkt->PCKTTYP = MM_PKT_TYPE_UKN;
  lrPkt->NB_MSG = 0;
  lrPkt->NB_ACK = 0;
  lrPkt->CRC32 = MauMe.getCRC32FromChars((char*)&lrPkt->DAT[0], lrPkt->PKTSIZ);
  return lrPkt;
}
//------------------------------------------------------------------------------------------------------------------------
bool IRAM_ATTR MauMeClass::addPacket(LoRa_PKT * lrPkt){
  #ifdef MAUME_DEBUG // #endif
    Serial.println(" -> Adding packet to list...");
  #endif
  xSemaphoreTakeRecursive(MauMe.pcktListSemaphore, portMAX_DELAY);
    lrPkt->next = (LoRa_PKT *)MauMe.receivedPackets;
    MauMe.receivedPackets = lrPkt;
  bool ret = xSemaphoreGiveRecursive(MauMe.pcktListSemaphore);
  #ifdef MAUME_DEBUG // #endif
    Serial.println(" -> Packet added.");
  #endif
  MauMe.Log("-INJCT PKT: "+String(lrPkt->CRC32)+":"+MauMe.getPKTType(lrPkt->PCKTTYP)+"(TYP"+String(lrPkt->PCKTTYP)+",PW"+String(lrPkt->TX_POW)+",TX"+String(lrPkt->NB_TX)+","+String(lrPkt->NB_MSG)+"MSG,"+String(lrPkt->NB_ACK)+"ACK)");
  return ret;
}
//---------------------------------------------------------------------------------------------------------------------
bool MauMeClass::enlistMessageForTransmit(String recipientsMacStyleAddress, String message){
  ADDRESS addressTo = MauMe.addressFromMacStyleString(recipientsMacStyleAddress);
  MauMePKT_0 * mmPkt = MauMe.createPacket(MauMe.myByteAddress, addressTo, message, MMM_APPTYP_PRV);
  if(mmPkt != NULL){
    LoRa_PKT * lrPkt = MauMe.getLoRaPacketFromMMPKT(mmPkt);
    free(mmPkt);
    return MauMe.addPacket(lrPkt);
  }else{
    return false;
  }
}
//---------------------------------------------------------------------------------------------------------------------
int MauMeClass::countSavedPackets(){
  return MauMe.countfiles("/PKTS");
}
//---------------------------------------------------------------------------------------------------------------------
int MauMeClass::countSavedACKS2SENDs(){
  return MauMe.countfiles("/ACKS2SEND");
}
//---------------------------------------------------------------------------------------------------------------------
int MauMeClass::countfiles(String path){
  int nb = 0;
  File root = fileOpen(SPIFFS, MauMe.spiMutex, path.c_str());
  if(!root){Serial.println(" #< Failed to open directory");
  }else{
    File file = getNextFile(MauMe.spiMutex, root);
    while(file){
      nb++;
      file = getNextFile(MauMe.spiMutex, root);
    }
    root.close();
    file.close();
  }
  return nb;
}
//------------------------------------------------------------------------------------------------------------------
LoRa_PKT * MauMeClass::extractMsgPckt(LoRa_PKT * pckt){
  return MauMe.getLoRaPacketFromMMPKT(parseMMPktFromLoRa(pckt));
}
//------------------------------------------------------------------------------------------------------------------
bool MauMeClass::ackAlreadyInPile(LoRa_PKT * pckt, MauMeACK_0 * anAck){
  for(MauMeACK_0 * mmAck = (MauMeACK_0 *)&pckt->DAT[0]; mmAck<=(MauMeACK_0 *)(&pckt->DAT[0]+pckt->PKTSIZ-sizeof(MauMeACK_0)); mmAck++){
    if(mmAck->HASH == anAck->HASH){
      Serial.println("  <> ACK ALREADY IN PILE PACKET !");
      return true;
    }
  }
  return false;
}
//------------------------------------------------------------------------------------------------------------------
bool MauMeClass::ackAlreadyPiggyBacked(LoRa_PKT * pckt, MauMeACK_0 * anAck){
  MauMePKT_0 * mmPkt = parseMMPktFromLoRa(pckt);
  MauMeACK_0 * pilePointer = (MauMeACK_0 *)&pckt->DAT[0];
  if(mmPkt != NULL){
    pilePointer = (MauMeACK_0 *)(&pckt->DAT[0] + sizeof(MauMeHDR_0) + mmPkt->HDR.LOADSIZE);
  }
  for(MauMeACK_0 * mmAck = pilePointer; mmAck<=(MauMeACK_0 *)(&pckt->DAT[0]+pckt->PKTSIZ-sizeof(MauMeACK_0)); mmAck++){
    if(mmAck->HASH == anAck->HASH){
      Serial.println(" <> ACK ALREADY IN MESSAGE PACKET !");
      return true;
    }
  }
  return false;
}
//------------------------------------------------------------------------------------------------------------------
void MauMeClass::runMauMe(){
  MauMe.doRun = true;
  delay(250);
  LoRa_JMM.receive();  
}
//------------------------------------------------------------------------------------------------------------------
void MauMeClass::sleepMauMe(){
  LoRa_JMM.sleep();
  MauMe.doRun = false;
  delay(250);
  MauMe.saveReceivedPackets();
}
//------------------------------------------------------------------------------------------------------------------
void MauMeClass::serveHttp(){
  MauMe.doServe = true;
}
//------------------------------------------------------------------------------------------------------------------
void MauMeClass::haltHttp(){
  MauMe.doServe = false;
}
//---------------------------------------------------------------------------------------------------------------------
int MauMeClass::processPackets(){
  #ifdef MAUME_DEBUG // #endif
    Serial.println(" 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0\n     PROCESSING PACKETS : ");
  #endif
  int nb = 0;
  int fileCount = 0;
  int cnt = 0;
  char acksList[20][32] = {'\0'};
  File root = fileOpen(SPIFFS, MauMe.spiMutex, "/PKTS");
  if(!root){
     Serial.println(" #< Failed to open PKTs directory. Empty ?");
     return 0;
  }
  File file = getNextFile(MauMe.spiMutex, root);
  while(file){
    LoRa_PKT * lrPkt = NULL;
    lrPkt = MauMe.loadLoRaPacket(String(file.name()));
    if(lrPkt == NULL){
      Serial.println(" #< Error loading packet ! Skiping...");
      file = getNextFile(MauMe.spiMutex, root);
      continue;
    }
    cnt = 0;
    #ifdef MAUME_DEBUG // #endif
      Serial.println(" -> Loaded packet "+String(lrPkt->CRC32)+" : "+MauMe.getPKTType(lrPkt->PCKTTYP)+".");
    #endif
    if(lrPkt->PCKTTYP == MM_PKT_TYPE_MMM || lrPkt->PCKTTYP == MM_PKT_TYPE_PILE){
      int leftSpace = MAX_PKT_SIZE - lrPkt->PKTSIZ;
      #ifdef MAUME_DEBUG // #endif
        Serial.println(" -> MM packet: Piggy-backing ? ("+String(leftSpace)+" bytes left)");
      #endif
      int pos = lrPkt->PKTSIZ;
      if( leftSpace >= sizeof(MauMeACK_0) ){
        File ackRoot = fileOpen(SPIFFS, MauMe.spiMutex, "/ACKS2SEND");
        if(!ackRoot){
          #ifdef MAUME_DEBUG // #endif
            Serial.println(" -> Failed to open ACKs directory. Sending packet only.");
          #endif
        }else{
          #ifdef MAUME_DEBUG // #endif
            Serial.println(" -> ACKs directory opened.");
          #endif
          /*lrPkt->NB_MSG = -1;
  lrPkt->NB_ACK = -1;*/
          if(lrPkt->NB_ACK == 255){
            lrPkt->NB_ACK = 0;
          }
          cnt = 0;
          File ackFile = getNextFile(MauMe.spiMutex, ackRoot);
          while(ackFile && leftSpace >= sizeof(MauMeACK_0)){
            MauMeACK_0 * mmAck = NULL;
            mmAck = MauMe.loadLoRaACK(String(ackFile.name()));
            if(mmAck != NULL && !MauMe.ackAlreadyPiggyBacked(lrPkt, mmAck)){
              #ifdef MAUME_DEBUG // #endif
                Serial.println(" -> Piggy-backing ACK "+String(mmAck->CRC32)+" (HASH: "+String(mmAck->HASH)+").");
              #endif
              memcpy((byte*)&(lrPkt->DAT[pos]), mmAck, sizeof(MauMeACK_0));
              free(mmAck);mmAck = NULL;
              leftSpace -= sizeof(MauMeACK_0);
              pos += sizeof(MauMeACK_0);
              lrPkt->PKTSIZ += sizeof(MauMeACK_0);
              lrPkt->NB_ACK += 1;
              acksList[cnt][0] = '\0';
              strcpy(acksList[cnt], ackFile.name());            
              cnt++;
            }
            ackFile = getNextFile(MauMe.spiMutex, ackRoot);
          }
          #ifdef MAUME_DEBUG // #endif
            Serial.println(" -> Piggy-backed "+String(cnt)+" ACKs.");
          #endif
          ackFile.close();
        }
        ackRoot.close();
      }
    }
    //-----------------------------------------
    lrPkt->TX_POW += 1;
    #ifdef MAUME_DEBUG // #endif
      Serial.println(" -> Needs saving updated packet."); 
    #endif
    if(saveLoRaPkt("/PKTS/", lrPkt, true)){
      #ifdef MAUME_DEBUG // #endif
        Serial.println(" -> Saved updated packet.");                  
      #endif
    }else{Serial.println(" #< Failed saving updated packet !");}
    int ret = MauMe.sendLoRaPKT(lrPkt, true);
    LoRa_JMM.receive();                     // go back into receive mode
    if(ret){
      fileCount++;
      #ifdef MAUME_DEBUG // #endif
        Serial.println(" -> Sent LoRa packet ("+String(ret)+")");
        Serial.println(" -> Recording ACK to SENT list.");
      #endif
      MauMeACK_0 * mmAck = NULL;
      mmAck = MauMe.makeMauMeACK_0(1, ACK0_FRMT, ACK0_TYPE_SH, lrPkt->CRC32);
      if(mmAck != NULL){
        MauMe.saveLoRaACK_0("/ACKSSENT/", mmAck, false);
        free(mmAck);mmAck = NULL;
      }else{Serial.println("        <> Error recording ACK to SENT list !");}
      nb++; 
      if(cnt > 0){
        #ifdef MAUME_DEBUG // #endif
          Serial.println(" -> Removing piggy-backed ACKs files : "+String(cnt));
        #endif
        //------------------------------------------------------------
        while(cnt>0){        
          #ifdef MAUME_DEBUG // #endif
            Serial.println("   -> Removing ack : "+String(acksList[cnt-1]));
          #endif
          deleteFile(SPIFFS, MauMe.spiMutex, acksList[cnt-1]);    
          acksList[cnt-1][0] = '\0';
          cnt--;
        }
      }else{
        #ifdef MAUME_DEBUG // #endif
          Serial.println(" -> No piggy-backed ACKs files to remove ("+String(cnt)+")");
        #endif
      }
    }else{Serial.println(" #< Failed sending LoRa packet ("+String(ret)+")");}
    if(lrPkt){
      free(lrPkt);lrPkt = NULL;
    }
    break;
  }
  file.close();
  root.close();
  if(fileCount == 0){
    #ifdef MAUME_DEBUG // #endif
      Serial.println(" >  *** No packet to send. Sending ACK-pile ? ***");
    #endif
    File ackRoot = fileOpen(SPIFFS, MauMe.spiMutex, "/ACKS2SEND");
    if(!ackRoot){
      #ifdef MAUME_DEBUG // #endif
        Serial.println(" #< Failed to open ACKs directory. No ACK to send ?");
      #endif
    }else{
      #ifdef MAUME_DEBUG // #endif
        Serial.println(" -> Sending ACK-pile :");
      #endif
      LoRa_PKT * lrPkt = NULL;
      lrPkt = (LoRa_PKT *)malloc(sizeof(LoRa_PKT));
      if(lrPkt){
        lrPkt->PKTSIZ = 0;
        lrPkt->next = NULL;
        lrPkt->PCKTTYP = MM_PKT_TYPE_PILE;
        lrPkt->NB_MSG = 0;
        lrPkt->NB_ACK = 0;
        int leftSpace = MAX_PKT_SIZE;
        int pos = lrPkt->PKTSIZ;
        #ifdef ACK_PKTS_OF_ACKS  // #endif
          bool onlyACKsOfPile = true; 
        #endif
        cnt = 0;
        File ackFile = getNextFile(MauMe.spiMutex, ackRoot);
        while(ackFile && leftSpace >= sizeof(MauMeACK_0)){
          MauMeACK_0 * mmAck = NULL;
          mmAck = MauMe.loadLoRaACK(String(ackFile.name()));
          if(mmAck != NULL){            
            if(!MauMe.ackAlreadyInPile(lrPkt, mmAck)){
              #ifdef MAUME_DEBUG // #endif
                Serial.println(" -> Piling-up ACK "+String(mmAck->CRC32)+" (HASH: "+String(mmAck->HASH)+").");
                Serial.print(" -> ACK data :");
                Serial.write((const uint8_t*)mmAck, sizeof(MauMeACK_0));
                Serial.println("");
              #endif
              memcpy((byte*)&(lrPkt->DAT[pos]), mmAck, sizeof(MauMeACK_0));
              #ifdef ACK_PKTS_OF_ACKS  // #endif
                if(mmAck->TYPE != ACK0_TYPE_PL){
                  onlyACKsOfPile = false;
                }
              #endif
              free(mmAck);mmAck = NULL;
              leftSpace -= sizeof(MauMeACK_0);
              pos += sizeof(MauMeACK_0);
              lrPkt->PKTSIZ += sizeof(MauMeACK_0);
              acksList[cnt][0] = '\0';
              lrPkt->NB_ACK += 1;
              strcpy(acksList[cnt], ackFile.name());            
              cnt++;
            }else{Serial.println(" #< !!! ACK already in Pile !!!");}
          }else{Serial.println(" #< Failed loading ACK !");}
          ackFile = getNextFile(MauMe.spiMutex, ackRoot);
        }
        #ifdef MAUME_DEBUG // #endif
          Serial.println("  -> Piled-up "+String(cnt)+" ACKs.");
        #endif
        ackRoot.close();
        ackFile.close();
        if(cnt >0){
          #ifdef MAUME_DEBUG // #endif
            Serial.print(" -> Sending ACK-pile data :");
            Serial.write((const uint8_t*)&lrPkt->DAT[0], lrPkt->PKTSIZ);
            Serial.println("");
          #endif
          lrPkt->TX_POW += 1; // getCRC32FromChars((char*)&mmAck->FRMT, 6)
          MauMeACK_0 * headMmAck = (MauMeACK_0 *)&lrPkt->DAT[0];
          lrPkt->CRC32 = headMmAck->CRC32;//getCRC32FromChars((char*)&headMmAck->FRMT, 6);
          //-------------------------------------------------------------------
          #ifdef ACK_PKTS_OF_ACKS
            if(!onlyACKsOfPile && cnt> ACK_PILE_MIN_COUNT){
              #ifdef MAUME_DEBUG // #endif
                Serial.println(" -> Needs saving ACK pile packet."); 
              #endif
              if(MauMe.saveLoRaPkt("/PKTS/", lrPkt, true)){
                #ifdef MAUME_DEBUG // #endif
                  Serial.println(" -> Saved ACK pile packet.");                  
                #endif
              }else{Serial.println(" #< Failed saving ACK pile packet !");}
            }
            int ret = MauMe.sendLoRaPKT(lrPkt, !onlyACKsOfPile && cnt>ACK_PILE_MIN_COUNT);
          #else
            int ret = MauMe.sendLoRaPKT(lrPkt, false);
          #endif
          //-------------------------------------------------------------------
          LoRa_JMM.receive();                     // go back into receive mode
          if(ret){
            nb++; 
            #ifdef MAUME_DEBUG // #endif
              Serial.println(" -> Sent ACKs-pile LoRa packet ("+String(ret)+")");
              Serial.println(" -> Removing piled-ACKs packet ACKs :"+String(cnt));
            #endif
            //------------------------------------------------------------
            while(cnt>0){       
              #ifdef MAUME_DEBUG // #endif 
                Serial.println("   -> Removing ack : "+String(acksList[cnt-1]));
              #endif
              deleteFile(SPIFFS, MauMe.spiMutex, acksList[cnt-1]);    
              acksList[cnt-1][0] = '\0';
              cnt--;
            }
          }else{Serial.println(" #< Failed sending ACKs LoRa packet ("+String(ret)+")");}
        }
        free(lrPkt);lrPkt = NULL;
      }else{Serial.println(" #< Failed allocating LoRa packet !");}
    }
  }
  return nb;
}
//---------------------------------------------------------------------------------------------------------------------
bool MauMeClass::isMessageArrived(char *buf){
  bool arrived = false;
  ADDRESS_LIST macAddressList = getClientsList();
  for(int i=0; i<macAddressList.nb; i++){
    if(MauMe.compareBuffers((const char*)buf, (const char*)&(macAddressList.list[i]->dat[0]), MM_ADDRESS_SIZE)){
      arrived = true;
      break;
    }
  }
  if(!arrived && MauMe.compareBuffers((const char*)buf, (const char*)&myByteAddress.dat, MM_ADDRESS_SIZE)){
    arrived = true;
  }
  MauMe.freeClientsList(macAddressList);
  return arrived;
}
//---------------------------------------------------------------------------------------------------------------------
int MauMeClass::cleanPacketsVsACKs(){
  #ifdef MAUME_DEBUG // #endif
    Serial.println(" ................................... \n CLEANING SENT+ACK PACKETS : ");
  #endif
  int nb = 0;
  File root = fileOpen(SPIFFS, MauMe.spiMutex, "/PKTS");
  if(!root){
    #ifdef MAUME_DEBUG // #endif
      Serial.println(" #< Failed to open /PKTS directory\n...................................");
    #endif
  }else{
    #ifdef MAUME_DEBUG // #endif
      Serial.println(" -> Opened /PKTS directory.");
    #endif
    File file = getNextFile(MauMe.spiMutex, root);
    while (file) {
      String ackNodeName = file.name();
      ackNodeName.replace("PKTS", "NODEACKS");
      String ackRecName = file.name();
      ackRecName.replace("PKTS", "RECACKS");
      String ackSentName = file.name();
      ackSentName.replace("PKTS", "ACKSSENT");
      bool alreadyNode = fileExists(SPIFFS, MauMe.spiMutex, ackNodeName.c_str());
      bool alreadyRec = alreadyNode || fileExists(SPIFFS, MauMe.spiMutex, ackRecName.c_str());
      bool alreadySent = fileExists(SPIFFS, MauMe.spiMutex, ackSentName.c_str());
      LoRa_PKT * lrPkt = NULL;
      lrPkt = MauMe.loadLoRaPacket(String(file.name()));
      if(lrPkt != NULL && (lrPkt->PCKTTYP == MM_PKT_TYPE_GEN || alreadyRec) && alreadySent){ // Packet is ACK and was sent
        #ifdef MAUME_DEBUG // #endif
          Serial.println("  > Removing packet : "+String(file.name()));
        #endif
        deleteFile(SPIFFS, MauMe.spiMutex, file.name());      
        free(lrPkt);lrPkt = NULL;
        nb++;
      }    
      file = getNextFile(MauMe.spiMutex, root);
    }
    root.close();
    file.close();
  }
  #ifdef MAUME_DEBUG // #endif
    Serial.println(" -> Removing left received ACKS for PKTS not sent yet or unknwon...");
  #endif
  File rackRoot = fileOpen(SPIFFS, MauMe.spiMutex, "/RECACKS");
  if(!rackRoot){
    #ifdef MAUME_DEBUG // #endif
      Serial.println(" #< Failed to open /RECACKS directory\n...................................");
    #endif
  }else{
    File rackRootFile = getNextFile(MauMe.spiMutex, rackRoot);
    while(rackRootFile){
      #ifdef MAUME_DEBUG // #endif
        Serial.println("   -> Removing packet : "+String(rackRootFile.name()));
      #endif
      deleteFile(SPIFFS, MauMe.spiMutex, rackRootFile.name());      
      rackRootFile = getNextFile(MauMe.spiMutex, rackRoot);
    }
    rackRoot.close();
    rackRootFile.close();
  }
  #ifdef MAUME_DEBUG // #endif
    Serial.println(" ...................................");
  #endif
  return nb;
}
//---------------------------------------------------------------------------------------------------------------------
int MauMeClass::saveReceivedPackets(){
  LoRa_PKT * list = NULL;
  xSemaphoreTakeRecursive(pcktListSemaphore, portMAX_DELAY);
    list = (LoRa_PKT *)MauMe.receivedPackets;
    MauMe.receivedPackets = NULL;
  xSemaphoreGiveRecursive(pcktListSemaphore);  
  int nbMessagesArrived = 0; 
  if(list!=NULL){
    #ifdef MAUME_DEBUG // #endif
      Serial.println(" * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *\nSaving Received packets.");
    #endif
    int cnt = 1;
    while(list != NULL){
      LoRa_PKT * next = list->next;
      MauMeHDR_0 * strHdr = (MauMeHDR_0 *)&(list->DAT[0]);
      uint32_t newCRC32 = MauMe.getCRC32FromChars((char*)strHdr + HDR0_FRMT_POS, sizeof(MauMeHDR_0) + strHdr->LOADSIZE - HDR0_FRMT_POS);
      #ifdef MAUME_DEBUG // #endif
        Serial.println("-----------------------------------------\n REC PKT DAT : "+String(list->PKTSIZ)+" bytes:");
        Serial.write((const uint8_t*)&list->DAT[0], list->PKTSIZ);
        Serial.println("\n-----------------------------------------\n");      
        Serial.printf(" -> Computing CRC with %p : %d bytes.\n", (char*)&(strHdr->FRMT), sizeof(MauMeHDR_0) + strHdr->LOADSIZE - HDR0_FRMT_POS );
        Serial.println("-----------------------------------------\n Computing CRC on : (HDR0_FRMT_POS = "+String(HDR0_FRMT_POS)+"):");
        Serial.write((const uint8_t*)((char*)strHdr + (char)HDR0_FRMT_POS), sizeof(MauMeHDR_0) + strHdr->LOADSIZE - HDR0_FRMT_POS);
        Serial.println("\n-----------------------------------------\n");
        Serial.println(" PKT CRC32 : "+String(strHdr->CRC32));
        Serial.println(" New CRC32 : "+String(newCRC32));
      #endif
      if(newCRC32 != 0 && strHdr->CRC32 != 0){
        if(newCRC32 == strHdr->CRC32){
          list->PCKTTYP = MM_PKT_TYPE_MMM;
          list->NB_MSG = 1;
          list->NB_ACK = 0;
          list->CRC32 = strHdr->CRC32; 
          if(MauMe.isMessageArrived(strHdr->DEST)){                                  // MESSAGE HAS REACHED DESTINATION :
            #ifdef MAUME_DEBUG // #endif
              Serial.println(" # NODE RECEIVE : "+String(list->CRC32));
            #endif
            if(!fileExists(SPIFFS, MauMe.spiMutex, String("/RECMES/"+String(list->CRC32)+".bin").c_str())){ // ",PW"+String(list->TX_POW)+",TX"+String(list->NB_TX)+
              MauMe.Log("-RECV MMM: "+String(list->CRC32)+":"+MauMe.getPKTType(list->PCKTTYP)+"(TYP"+String(list->PCKTTYP)+","+String(list->NB_MSG)+","+String(list->NB_ACK)+")");   
              strHdr->NHP += 1;
              if(MauMe.saveLoRaPkt("/RECMES/", list, false)){
                nbMessagesArrived++;
                #ifdef MAUME_DEBUG // #endif
                  Serial.println(" -> Saved Arrived Message n°"+String(cnt)+".");                  
                #endif
                cnt++;
              }else{
                #ifdef MAUME_DEBUG // #endif
                  Serial.println(" #< Failed Saving Message !");
                #endif
              }
            }else{
              MauMe.Log("-RCPY MMM: "+String(list->CRC32)+":"+MauMe.getPKTType(list->PCKTTYP)+"(TYP"+String(list->PCKTTYP)+",PW"+String(list->TX_POW)+",TX"+String(list->NB_TX)+","+String(list->NB_MSG)+"MSG,"+String(list->NB_ACK)+"ACK)");   
              #ifdef MAUME_DEBUG // #endif
                Serial.println("****************************************\n MESSAGE ALREADY RECEIVED ! ("+String(list->CRC32)+")\n****************************************");
              #endif
            }
            #ifdef MAUME_DEBUG // #endif
              Serial.println(" -> Adding REC ACK anyway.");
            #endif
            MauMeACK_0 * mmAck = NULL;
            mmAck = MauMe.makeMauMeACK_0(strHdr->NHP+1, ACK0_FRMT, ACK0_TYPE_RC, strHdr->CRC32);
            if(mmAck != NULL){
              MauMe.saveLoRaACK_0("/ACKS2SEND/", mmAck, false);
              free(mmAck);mmAck = NULL;
            }else{Serial.println(" #< Failed saving ACK of PKT : "+String(strHdr->CRC32));}
          }else{                                                               // FORWARD :
            #ifdef MAUME_DEBUG // #endif
              Serial.println(" -> Saving MMM packet: "+String(list->CRC32));
            #endif  // MMM_APPTYP_DUM
            if(strHdr->NHP<254 && !fileExists(SPIFFS, MauMe.spiMutex, String("/ACKSSENT/"+String(list->CRC32)+".bin").c_str())){
              if(MauMe.isMessageArrived(strHdr->SEND)){          // this is to check if need save packet as sent by this node !!!
                //MauMe.Log("-SVNG MMM:/NODEPKTS/"+String(list->CRC32)+":"+MauMe.getPKTType(list->PCKTTYP)+"(TYP"+String(list->PCKTTYP)+",PW"+String(list->TX_POW)+",TX"+String(list->NB_TX)+","+String(list->NB_MSG)+"MSG,"+String(list->NB_ACK)+"ACK)");   
                MauMe.saveLoRaPkt("/NODEPKTS/", list, false);    // leave it there !
              }else{
                strHdr->NHP += 1;
              }
              #ifdef MAUME_DEBUG // #endif
                Serial.println(" -> Needs saving."); 
              #endif
              LoRa_PKT * newPckt = NULL;
              newPckt = MauMe.extractMsgPckt(list);
              if(newPckt && MauMe.saveLoRaPkt("/PKTS/", newPckt, false)){
                #ifdef MAUME_DEBUG // #endif
                  Serial.println(" -> Saved "+String(cnt)+".");                  
                #endif
                cnt++;
              }else{Serial.println(" #< Failed saving packet!");}
              if(newPckt){
                free(newPckt);
              }
            }else{Serial.println("****************************************\n Packet ALREADY FORWARDED or NHP outbounded (NHP: "+String(strHdr->NHP)+")! Dropping... ("+String(list->CRC32)+")\n****************************************");}
            
            if(!fileExists(SPIFFS, MauMe.spiMutex, String("/NODEPKTS/"+String(list->CRC32)+".bin").c_str())){
              #ifdef MAUME_DEBUG // #endif
                Serial.println(" Adding anyway ACK for received message to list for ack or re-ack.");
              #endif
              MauMeACK_0 * mmAck = NULL;
              mmAck = MauMe.makeMauMeACK_0(strHdr->NHP+1, ACK0_FRMT, ACK0_TYPE_SH, strHdr->CRC32);
              if(mmAck != NULL){
                MauMe.Log("-INJCT ACK for PKT:/ACKS2SEND/"+String(mmAck->CRC32)+" for PKT "+String(list->CRC32)+":"+MauMe.getACKType(mmAck->TYPE)+"(TYP"+String(list->PCKTTYP)+",PW"+String(list->TX_POW)+",TX"+String(list->NB_TX)+","+String(list->NB_MSG)+"MSG,"+String(list->NB_ACK)+"ACK)");   
                MauMe.saveLoRaACK_0("/ACKS2SEND/", mmAck, false);
                free(mmAck);mmAck = NULL;
              }else{Serial.println(" #< Failed saving ACK of PKT : "+String(strHdr->CRC32));}
            }else{
              #ifdef MAUME_DEBUG // #endif
                Serial.println(" <> Not sending ACK for message from this node ("+String(strHdr->CRC32)+")");
              #endif
            }
          }
          // -------------------------------------------------- PARSING PIGGY-BACKING :
          #ifdef MAUME_DEBUG // #endif
            Serial.println(" = = = = = = = = = = = \nPARSING PIGGY-BACKING: (Packet size: "+String(list->PKTSIZ)+", Payload headed size: "+String(strHdr->LOADSIZE + sizeof(MauMeHDR_0))+")");
          #endif
          int leftBytes = list->PKTSIZ - strHdr->LOADSIZE - sizeof(MauMeHDR_0);
          byte* pos = (byte*)(&list->DAT[0] + sizeof(MauMeHDR_0) + strHdr->LOADSIZE);
          // FRMT   #define ACK0_FRMT 1    //  #define ACK1_FRMT 2   //  #define ACK0_TYPE_SH 1    //  #define ACK0_TYPE_RC  2
          int count = 0;
          while(leftBytes >= sizeof(MauMeACK_0)){
            // Serial.println(" > leftBytes : "+String(leftBytes)+".");
            MauMeACK_0 * recMmAck = NULL;
            recMmAck = MauMe.parseMMAckFromBuffer((char*)pos, sizeof(MauMeACK_0));
            if(recMmAck != NULL){
              list->NB_ACK += 1;
              count++;
              if(recMmAck->TYPE == ACK0_TYPE_RC && fileExists(SPIFFS, MauMe.spiMutex, String("/NODEPKTS/"+String(recMmAck->HASH)+".bin").c_str())){
                #ifdef MAUME_DEBUG // #endif
                  Serial.println(" > ACK-REC HAS REACHED ORIGINAL MESSAGE OUTBOX.");
                #endif
                //onlyACKsOfPile = false;
                // ACK-REC HAS REACHED DESTINATION :
                MauMe.saveLoRaACK_0("/NODEACKS/", recMmAck, false);
              }else{
                if(recMmAck->HTL > 0){
                  recMmAck->HTL --;
                  if(recMmAck->TYPE == ACK0_TYPE_SH){
                    #ifdef MAUME_DEBUG // #endif
                      Serial.println(" -> SINGLE-HOP ACK (HTL now = "+String(recMmAck->HTL)+"). Enlisting for receive.");
                    #endif
                    //onlyACKsOfPile = false;
                    MauMe.saveLoRaACK_0("/RECACKS/", recMmAck, false);
                  }else if(recMmAck->TYPE == ACK0_TYPE_RC){
                    #ifdef MAUME_DEBUG // #endif
                      Serial.println(" -> RECEIVE-ACK (HTL now = "+String(recMmAck->HTL)+"). Enlisting for forward.");
                    #endif
                    //onlyACKsOfPile = false;
                    MauMe.saveLoRaACK_0("/ACKS2SEND/", recMmAck, false);        // this maybe an error, doubling the RECACKS dir.. !!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!
                    MauMe.saveLoRaACK_0("/ACKSSENT/", recMmAck, false);         //  Receive ACKs cancel possible transmit by enlisting packet as already treated.
                    MauMe.saveLoRaACK_0("/RECACKS/", recMmAck, false);
                  }else if(recMmAck->TYPE == ACK0_TYPE_PL){
                    #ifdef MAUME_DEBUG // #endif
                      Serial.println(" -> ACK for PILE (HTL now = "+String(recMmAck->HTL)+"). Enlisting for receive.");
                    #endif
                    MauMe.saveLoRaACK_0("/RECACKS/", recMmAck, false);
                  }
                }else{
                  #ifdef MAUME_DEBUG 
                    Serial.println(" <> ACK expired. Nexting...");
                  #endif
                }
              }
              free(recMmAck);recMmAck = NULL;
            }
            leftBytes -= sizeof(MauMeACK_0);
            pos += sizeof(MauMeACK_0);            
          }
          if(count > 0){
            // MauMe.Log("-UNPL ACKS: from PKT "+String(list->CRC32)+" UNPLD "+String(count)+".");   
          }
        }else{                                                             //      ACK PILE OR GEN PACKET ?
          list->PCKTTYP = MM_PKT_TYPE_PILE;                                 // Assume it is...
          // -------------------------------------------------- PARSING PILED ACKs :
          #ifdef MAUME_DEBUG // #endif
            Serial.println(" = = = = = ! WRONG CRC FOR MMM ! = = = = = = = = = \nPARSING Piled ACKS ?");
          #endif
          int leftBytes = list->PKTSIZ;
          byte* pos = (byte*)(&list->DAT[0]);
          int byteCount = 0;
          // FRMT  #define ACK0_FRMT 1         //  #define ACK1_FRMT 2          //  #define ACK0_TYPE_SH 1          //  #define ACK0_TYPE_RC  2
          bool ackPile = false;
          #ifdef ACK_PKTS_OF_ACKS   //  #endif
            bool onlyACKsOfPile = true; 
          #endif
          int count = 0;
          while(leftBytes >= sizeof(MauMeACK_0)){
            MauMeACK_0 * recMmAck = NULL;
            recMmAck = MauMe.parseMMAckFromBuffer((char*)pos, sizeof(MauMeACK_0));
            if(recMmAck != NULL){
              list->NB_ACK += 1;
              count++;
              #ifdef MAUME_DEBUG // #endif
                Serial.println(" -> ("+String(byteCount)+") De-piling ACK "+String(recMmAck->CRC32)+" (HASH: "+String(recMmAck->HASH)+").");
              #endif
              ackPile = true;
              if(recMmAck->TYPE == ACK0_TYPE_RC && fileExists(SPIFFS, MauMe.spiMutex, String("/NODEPKTS/"+String(recMmAck->HASH)+".bin").c_str())){
                #ifdef MAUME_DEBUG // #endif
                  Serial.println(" -> ACK-REC HAS REACHED ORIGINAL MESSAGE OUTBOX.");
                #endif
                #ifdef ACK_PKTS_OF_ACKS   //  #endif
                  onlyACKsOfPile = false;
                #endif
                // ACK-REC HAS REACHED DESTINATION :
                MauMe.saveLoRaACK_0("/NODEACKS/", recMmAck, false);
              }else{
                if(recMmAck->HTL > 0){
                  recMmAck->HTL --;
                  if(recMmAck->TYPE == ACK0_TYPE_SH){
                    #ifdef MAUME_DEBUG // #endif
                      Serial.println(" -> SINGLE-HOP ACK (HTL now = "+String(recMmAck->HTL)+"). Enlisting for receive.");
                    #endif
                    #ifdef ACK_PKTS_OF_ACKS   //  #endif
                      onlyACKsOfPile = false;
                    #endif
                    MauMe.saveLoRaACK_0("/RECACKS/", recMmAck, false);
                  }else if(recMmAck->TYPE == ACK0_TYPE_RC){
                    #ifdef MAUME_DEBUG // #endif
                      Serial.println(" -> RECEIVE-ACK (HTL now = "+String(recMmAck->HTL)+"). Enlisting for forward.");
                    #endif
                    #ifdef ACK_PKTS_OF_ACKS   //  #endif
                      onlyACKsOfPile = false;
                    #endif
                    MauMe.saveLoRaACK_0("/ACKS2SEND/", recMmAck, false);  
                    MauMe.saveLoRaACK_0("/ACKSSENT/", recMmAck, false);         //  Receive ACKs cancel possible transmit by enlisting packet as already treated.
                    MauMe.saveLoRaACK_0("/RECACKS/", recMmAck, false);
                  }else if(recMmAck->TYPE == ACK0_TYPE_PL){
                    #ifdef MAUME_DEBUG // #endif
                      Serial.println(" -> ACK for PILE (HTL now = "+String(recMmAck->HTL)+"). Enlisting for receive.");
                    #endif
                    MauMe.saveLoRaACK_0("/RECACKS/", recMmAck, false);
                  }
                }else{
                  #ifdef MAUME_DEBUG 
                    Serial.println(" <> ACK expired. Nexting..."); 
                  #endif 
                }
              }
              free(recMmAck);recMmAck = NULL;
            }else{
              #ifdef MAUME_DEBUG // #endif
                Serial.println(" #< No ACK pile: aborting.");
              #endif
              break;
            }            
            leftBytes -= sizeof(MauMeACK_0);
            pos += sizeof(MauMeACK_0);
            byteCount += sizeof(MauMeACK_0);
          }
          if(count > 0){
            // MauMe.Log("-UNPL ACKS: from PKT "+String(list->CRC32)+" UNPLD "+String(count)+".");   
          }
          if(ackPile){
            #ifdef ACK_PKTS_OF_ACKS   //  #endif
              if(!onlyACKsOfPile){
                #ifdef MAUME_DEBUG // #endif
                  Serial.println("************************************************************\n Adding ACK for received pile to list for ack or re-ack.\n************************************************************");
                #endif
                MauMeACK_0 * mmAck = NULL;
                //list->CRC32 = getCRC32FromChars((char*)&list->DAT[0], sizeof(MauMeACK_0));
                MauMeACK_0 * headAck = (MauMeACK_0 *)&list->DAT[0];
                mmAck = MauMe.makeMauMeACK_0(1, ACK0_FRMT, ACK0_TYPE_PL, headAck->CRC32);
                // MauMe.Log("-ACK OF PILE: "+String(mmAck->CRC32)+" for PILE "+String(list->CRC32)+":"+MauMe.getACKType(mmAck->TYPE)+".");                   
                if(mmAck != NULL){
                  MauMe.saveLoRaACK_0("/ACKS2SEND/", mmAck, true);
                  free(mmAck);mmAck = NULL;
                }else{Serial.println(" #< Failed saving ACK of pile : "+String(list->CRC32));}
              }else{
                #ifdef MAUME_DEBUG // #endif
                  Serial.println("************************************************************\n Not acknowledging ACK(s) for ACK-pile : "+String(list->CRC32)+"\n************************************************************");
                #endif
              }
            #endif
          }else{
            list->PCKTTYP = MM_PKT_TYPE_UKN;  
          }
          //-----------------------------------------------------------------------------------------
          #ifdef FORWARD_GEN_PKTS
            if(!ackPile){                                                      //          GEN PACKET :
              if(list->PKTSIZ <= 255){
                list->CRC32 = MauMe.getCRC32FromChars((char*)&list->DAT[0], list->PKTSIZ);
                list->PCKTTYP = MM_PKT_TYPE_GEN;
                MauMe.Log("-FWRD GEN PKT: "+String(list->CRC32)+":"+MauMe.getACKType(mmAck->TYPE)+".");   
                Serial.println(" -> Saving GEN packet: "+String(list->CRC32));
                if(!fileExists(SPIFFS, MauMe.spiMutex, String("/ACKSSENT/"+String(list->CRC32)+".bin").c_str())){
                  if(MauMe.saveLoRaPkt("/PKTS/", list, false)){ // "/PKTS/"
                    Serial.println(" -> Saved "+String(cnt)+".");                  
                    cnt++;
                  }else{Serial.println(" #< Failed saving packet!");}
                }else{Serial.println("****************************************\n Packet ALREADY FORWARDED ! ("+String(list->CRC32)+")\n****************************************");}
              }else{Serial.println(" #< ! Malformed packet ! Nexting...");}
            }
          #else
            #ifdef MAUME_DEBUG 
              if(!ackPile){
                Serial.println(" ### GEN or Corrupted PACKET: DROPPING ! ###");
              }
            #endif
          #endif
        }
      }else{
        #ifdef MAUME_DEBUG 
          Serial.println(" <> NULL CRC ! Packet ignored..."); 
        #endif
      }
      if(list != NULL){
        free(list);list = NULL;
      }
      #ifdef MAUME_DEBUG // #endif
        Serial.println(" ---------------------------------------------- NEXTING ON LIST ---------------------------");
      #endif
      list = next;
    }
    #ifdef MAUME_DEBUG // #endif
      Serial.println(" * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *");
    #endif 
    return nbMessagesArrived;
  }
  #ifdef MAUME_DEBUG // #endif
    else{Serial.println(" <> No saved packet to process.");}
  #endif 
  return nbMessagesArrived;
}
//-------------------------------------------------------------------------------
bool MauMeClass::writeMAC2MEM(String mac){
  EEPROM.begin(64);
  EEPROM.writeString(0, mac);
  return EEPROM.commit();
}
//-------------------------------------------------------------------------------
String MauMeClass::readMEM2MAC(){
  EEPROM.begin(64);
  String mac = EEPROM.readString(0);
  return mac;
}
//-------------------------------------------------------------------------------
// + + + + + + + + + + + + + + + + + + + + + + + + + + + + + + + + + + + + 
void MauMeClass::LoRaTask( void * pvParameters ){
  #ifdef MAUME_DEBUG   // #endif
    Serial.print(" -> LoRaTask started on core ");
    Serial.println(xPortGetCoreID());
  #endif
  while(true){
    if(MauMe.doRun){
      int packetSize = LoRa_JMM.parsePacket();
      if (packetSize) {
        LoRa_PKT * lrPkt = NULL;
        xSemaphoreTakeRecursive(MauMe.spiMutex, portMAX_DELAY);      
          lrPkt = MauMe.getCurrentLoRaPkt();
        xSemaphoreGiveRecursive(MauMe.spiMutex); // +",PW"+String(lrPkt->TX_POW)+",TX"+String(lrPkt->NB_TX)
        MauMe.Log("-RCV LORA PKT: "+String(lrPkt->CRC32)+":"+MauMe.getPKTType(lrPkt->PCKTTYP)+"(TYP"+String(lrPkt->PCKTTYP)+","+String(lrPkt->NB_MSG)+"MSG,"+String(lrPkt->NB_ACK)+"ACK)");   
        xSemaphoreTakeRecursive(MauMe.pcktListSemaphore, portMAX_DELAY);
          lrPkt->next = (LoRa_PKT *)MauMe.receivedPackets;
          MauMe.receivedPackets = lrPkt;
        xSemaphoreGiveRecursive(MauMe.pcktListSemaphore);
        #ifdef MAUME_DEBUG 
          Serial.println(" -> Done receiving.");
        #endif
      }
      delay(LORA_READ_DELAY);
    }else{
      delay(10*LORA_READ_DELAY);
    }
  }
}
// + + + + + + + + + + + + + + + + + + + + + + + + + + + + + + + + + + + + + +
void MauMeClass::activate_LoRa(){
  #ifdef MAUME_DEBUG   // #endif
    Serial.println(" * LoRa activation:");
  #endif
  SPI.begin(LoRa_SCK, LoRa_MISO, LoRa_MOSI, LoRa_CS);
  delay(250); 
  LoRa_JMM.setPins(LoRa_CS, LoRa_RESET, LoRa_IRQ);
  if (!LoRa_JMM.begin(BAND, MauMe.spiMutex)) {
    //oledDisplay->drawString(5, 25, " #< Starting LoRa failed!");
    while (1);
  }
  delay(250);  
  #ifdef MAUME_DEBUG   // #endif
    Serial.println(" -> Setting parameters.");
  #endif
  LoRa_JMM.setTxPower(TXPOWER);
  delay(LORA_READ_DELAY); 
  LoRa_JMM.setSpreadingFactor(SPREADING_FACTOR);
  LoRa_JMM.setSignalBandwidth(BANDWIDTH);
  LoRa_JMM.setCodingRate4(CODING_RATE);
  LoRa_JMM.setPreambleLength(PREAMBLE_LENGTH);
  LoRa_JMM.setSyncWord(SYNC_WORD);
  #ifdef MAUME_DEBUG   // #endif
    Serial.println(" -> Creating receive task.");
  #endif
  xTaskCreatePinnedToCore(
                    MauMe.LoRaTask,   /* Task function. */
                    "LoRaTask", /* name of task. */
                    10000,      /* Stack size of task */
                    NULL,       /* parameter of the task */
                    1,          /* priority of the task */
                    &MauMe.loRaTask,  /* Task handle to keep track of created task */
                    0);         /* pin task to core 0 */                  
  delay(500); 
  #ifdef MAUME_DEBUG   // #endif
    Serial.println(" -> LoRa Init OK!\n");
  #endif
  //oledDisplay->drawString(5, 25, "LoRa Initializing OK!");  
}

//-------------------------------------------------------------------------------
void MauMeClass::DnsTask(void * pvParameters){ 
  while(true) {
    delay(DNS_TASK_DELAY);
    if(MauMe.runDNS){
      MauMe.dnsServer->processNextRequest();    
    }
  }
}
//-------------------------------------------------------------------------------
IPAddress MauMeClass::getIPFromString(const char * strIP, int len){
  int Parts[4] = {0,0,0,0};
  int Part = 0;
  for(int i=0; i<len; i++){
    char c = strIP[i];
    if(c == '.'){Part++;continue;}
    Parts[Part] *= 10;
    Parts[Part] += c - '0';
  }
  IPAddress ip(Parts[0], Parts[1], Parts[2], Parts[3]);
  return ip;
}
//-------------------------------------------------------------------------------
void MauMeClass::activate_Wifi(){
  #ifdef MAUME_DEBUG   // #endif
    Serial.println(" * Setting up Wifi.");
  #endif
  IPAddress local_IP(1, 1, 1, 1);
  IPAddress gateway(1, 1, 1, 1);
  IPAddress subnet(255, 255, 255, 0);
  // String IRAM_ATTR MauMeClass::MauMe.addressBuffer2Str(char* buff, MM_ADDRESS_SIZE)
  String strSSID = "MauMe-"+MauMe.myMacAddress; // String(String(ssid)+ MauMe.addressBuffer2Str((char*)MauMe.myByteAddress.dat, MM_ADDRESS_SIZE)); // MyAddress);
  WiFi.softAP((const char*)strSSID.c_str());
  delay(1000); 
  if (!WiFi.softAPConfig(local_IP, gateway, subnet)) { // , primaryDNS, secondaryDNS
    Serial.println("    ! STA Failed to configure !");
  }
  delay(500); 
  #ifdef MAUME_DEBUG   // #endif
    Serial.println();
    Serial.println(" -> AP name: "+strSSID+".");
    Serial.print(" -> AP IP address: ");
    Serial.println(WiFi.softAPIP());
    Serial.print(" -> STA IP Address: ");
    Serial.println(WiFi.localIP());
    Serial.println(" -> WIFI set-up !\n");
  #endif
}
//-----------------------------------------------------------------------------------
void MauMeClass::activate_DNS(){
  #ifdef MAUME_DEBUG   // #endif
    Serial.println(" * Starting DNS.");
  #endif
  MauMe.dnsServer = new DNSServer();
  MauMe.dnsServer->start(53, "*", WiFi.softAPIP());
  #ifdef MAUME_DEBUG   // #endif
    Serial.println(" -> Seting up DNS update task.");
  #endif
  xTaskCreate(      MauMe.DnsTask,
                    "DNSTask",
                    10000,    
                    NULL,     
                    0,        
                    &MauMe.dnsTask/*, 
                    1*/);       
  delay(250); 
  #ifdef MAUME_DEBUG   // #endif
    Serial.println(" -> DNS server set-up !");
  #endif
}
//-----------------------------------------------------------------------------------
void MauMeClass::oledMauMeMessage(String message, bool doFlip){
  xSemaphoreTakeRecursive(MauMe.spiMutex, portMAX_DELAY);  
    //SSD1306 display(0x3c, OLED_SDA, OLED_SCL);
    #ifdef MAUME_DEBUG   // #endif
      Serial.println(" * MauMe OLED Display:");
    #endif
    /*
    pinMode(OLED_RST, OUTPUT);
    digitalWrite(OLED_RST, LOW);    // set GPIO16 low to reset OLED
    delay(50); 
    digitalWrite(OLED_RST, HIGH);
    display->init();
    */
    if(doFlip){
      MauMe.display->flipScreenVertically();
    }
    MauMe.display->setFont(ArialMT_Plain_10);
    MauMe.display->setTextAlignment(TEXT_ALIGN_LEFT);
    MauMe.display->clear();
    MauMe.display->drawXbm(MauMe.display->width() -4 -UPF_Logo_Width, 3, UPF_Logo_Width, UPF_Logo_Height, (const uint8_t*)&UPF_Logo_Bits); 
    MauMe.display->setFont(ArialMT_Plain_10);
    MauMe.display->drawString(3,  4, " Mau-Me Node");
    MauMe.display->drawString(3, 20, "   UPF 2020");
    MauMe.display->drawString(4,36, message);
    MauMe.display->display();
  xSemaphoreGiveRecursive(MauMe.spiMutex);
  #ifdef MAUME_DEBUG   // #endif
    Serial.println(" -> Done!");    
  #else
    Serial.println(" -> MauMe OLED Display set up.");    
  #endif
}
//-----------------------------------------------------------------------------------------------
void MauMeClass::oledDisplay(bool doFlip){
  xSemaphoreTakeRecursive(MauMe.spiMutex, portMAX_DELAY);  
    MauMe.display = new SSD1306(0x3c, OLED_SDA, OLED_SCL);
    #ifdef MAUME_DEBUG   // #endif
      Serial.println(" * MauMe OLED Display:");
    #endif
    pinMode(OLED_RST, OUTPUT);
    digitalWrite(OLED_RST, LOW);    // set GPIO16 low to reset OLED
    delay(50); 
    digitalWrite(OLED_RST, HIGH);
    MauMe.display->init();
    if(doFlip)
        MauMe.display->flipScreenVertically();
    MauMe.display->setFont(ArialMT_Plain_10);
    MauMe.display->setTextAlignment(TEXT_ALIGN_LEFT);
    MauMe.display->clear();
    MauMe.display->drawXbm(display->width()-4 - UPF_Logo_Width, 3, UPF_Logo_Width, UPF_Logo_Height, (const uint8_t*)&UPF_Logo_Bits); 
    MauMe.display->setFont(ArialMT_Plain_10);
    MauMe.display->drawString(3,  4, " Mau-Me Node");
    MauMe.display->drawString(3, 20, "   UPF 2020");
    MauMe.display->drawString(4, 36, "MAC: "+String(WiFi.macAddress()));
    MauMe.display->display();
    //display->end();
  xSemaphoreGiveRecursive(MauMe.spiMutex);
  #ifdef MAUME_DEBUG   // #endif
    Serial.println(" -> MauMe OLED Display set up.");    
  #endif
}
//--------------------------------------------------------------------------------------
void MauMeClass::notFound(AsyncWebServerRequest *request) {
  request->send(404, "text/plain", "<br><h2>MauMe Node - Error : </h2><br><br>Not found<br><br><a href=\"/\">Return to Home Page</a>");
}
//--------------------------------------------------------------------------------------
ADDRESS_LIST MauMeClass::getClientsList(){
  ADDRESS_LIST macAddressList;
  wifi_sta_list_t stationList;
  esp_wifi_ap_get_sta_list(&stationList);  
  #ifdef MAUME_DEBUG 
    Serial.print(" -> Nb. of connected stations: ");
    Serial.println(stationList.num);
  #endif
  macAddressList.nb = stationList.num;
  macAddressList.list = (ADDRESS**)malloc(sizeof(ADDRESS*)*macAddressList.nb);
  for(int i = 0; i < stationList.num; i++) {
    wifi_sta_info_t station = stationList.sta[i];
    macAddressList.list[i] = NULL;
    macAddressList.list[i] = (ADDRESS*)malloc(sizeof(ADDRESS));
    // ADDRESS macFrom = MauMe.macFromMacStyleString(String((char*)MauMe.getSha256BytesOfBytes((char*)clntMacAddr->addr, (const size_t)6)));
    if(macAddressList.list[i] != NULL){
      memcpy(&(macAddressList.list[i]->dat[0]), &(MauMe.addressFromBytes(MauMe.getSha256BytesOfBytes((char*)&station.mac, (const size_t)6)).dat[0]), MM_ADDRESS_SIZE);//&station.mac, 6);
    }    
  }
  return macAddressList;
}
//-----------------------------------------------------------------------------------
int MauMeClass::freeClientsList(ADDRESS_LIST macAddressList){
  int i = 0;
  for(i=0;i < macAddressList.nb; i++){
    free(macAddressList.list[i]);
    macAddressList.list[i] = NULL;
  }
  free(macAddressList.list);
  macAddressList.list = NULL;
  return ++i;
}
//-----------------------------------------------------------------------------------
void MauMeClass::printWifiClients(){
  wifi_sta_list_t stationList;
  esp_wifi_ap_get_sta_list(&stationList);  
  Serial.print(" -> N of connected stations: ");
  Serial.println(stationList.num);
  for(int i = 0; i < stationList.num; i++){
    wifi_sta_info_t station = stationList.sta[i];
    for(int j = 0; j< 6; j++){
      char str[3];
      sprintf(str, "%02x", (int)station.mac[j]);
      Serial.print(str);
      if(j<5){
        Serial.print(":");
      }
    }
    Serial.println();
  }
  Serial.println("-----------------");
}
//--------------------------------------------------------------------------------------
struct eth_addr * IRAM_ATTR MauMeClass::getClientMAC(IPAddress clntAd){
  #ifdef MAUME_DEBUG 
    Serial.println(" -> Fetching "+String(clntAd));
  #endif
  //-------------------------------------------------------------------------------------------------------------------------
  ip4_addr_t toCheck;
  IP4_ADDR(&toCheck, clntAd[0], clntAd[1], clntAd[2], clntAd[3]);
  struct eth_addr *ret_eth_addr = NULL;
  struct ip4_addr const *ret_ip_addr = NULL;
  int arp_find = etharp_find_addr(netif_default, &toCheck, &ret_eth_addr, &ret_ip_addr);
  #ifdef MAUME_DEBUG 
    Serial.printf(" -> Lookup: %s | %d\n", clntAd.toString().c_str(), arp_find );
  #endif
  String payload = "";
  if(arp_find != -1 && ret_eth_addr != NULL ) {//  && ( Ping.ping(clntAd, 1) || Ping.ping(clntAd, 1))
   char mac[32];
   sprintf(mac, "{ \"mac\": \"%02x:%02x:%02x:%02x:%02x:%02x\"", ret_eth_addr->addr[0], ret_eth_addr->addr[1], ret_eth_addr->addr[2], ret_eth_addr->addr[3], ret_eth_addr->addr[4], ret_eth_addr->addr[5]);
   payload += mac;
   payload += ", \"ip\": \"" + clntAd.toString() + "\"}, ";
 }
 #ifdef MAUME_DEBUG 
  Serial.println(" -> Found: "+payload+"");
 #endif
 return ret_eth_addr;
}
//-------------------------------------------------------------------------------------------------------------------------
/*%NODE_MAC%)rawliteral";*//*<option value="PUB" %TYPE_PUB%>Public</option><option value="PRV" %TYPE_PRV%>Private</option>*/
/*
 * <option value="PUB" %TYPE_PUB%>Public</option>
        <option value="PRV" %TYPE_PRV%>Private</option><img src="/WWW/Logo-MMM-color.png" alt="UPF Logo" align="right"/>
*/
AsyncResponseStream * IRAM_ATTR MauMeClass::getWebPage(AsyncWebServerRequest *request, ADDRESS macFrom, byte appType){  //  MMM_APPTYP_NOD
  AsyncResponseStream *response = request->beginResponseStream("text/html");
  response->addHeader("Server","Mau-Me Web Server");
  response->print(R"rawliteral(
      <!DOCTYPE HTML><html><head>
      <title>Mau-Me LoRa Messenger</title>
      <meta charset="ISO-8859-1">
      <meta name="viewport" content="width=device-width, initial-scale=1">
      <link rel="shortcut icon" type="image/x-icon" href="/WWW/favicon.ico" /><link rel="apple-touch-icon" sizes="180x180" href="/WWW/apple-touch-icon.png">
      <link rel="icon" type="image/png" sizes="32x32" href="/WWW/favicon-32x32.png"><link rel="icon" type="image/png" sizes="16x16" href="/WWW/favicon-16x16.png">
      <link rel="manifest" href="/WWW/site.webmanifest"><link rel="mask-icon" href="/WWW/safari-pinned-tab.svg" color="#5bbad5"><meta name="apple-mobile-web-app-title" content="Mau-Me">
      <meta name="application-name" content="Mau-Me"><meta name="msapplication-TileColor" content="#ffffff"><meta name="theme-color" content="#ffffff">
      <style type="text/css">
      html,body{font-family:Verdana,sans-serif;font-size:15px;line-height:1.5}html{overflow-x:hidden}
      h1{font-size:36px}h2{font-size:30px}h3{font-size:24px}h4{font-size:20px}h5{font-size:18px}h6{font-size:15px}.w3-serif{font-family:serif}
      h1,h2,h3,h4,h5,h6{font-family:"Segoe UI",Arial,sans-serif;font-weight:400;margin:10px 0}.w3-wide{letter-spacing:4px}
      h2{color:dodgerblue;}h3{color:darkslategray;}h4{color:darkcyan;}h5{color:midnightblue;}
      body{background-color: cornsilk;}
      #submitBtn{background:#016ABC;padding:5px 7px 6px;font:bold;color:#fff;border:1px solid #eee;border-radius:6px;box-shadow: 5px 5px 5px #eee;text-shadow:none;}
      </style>
      </head><body><div><h2 align="center"><b>Mau-Me<form action="/admin"><input type="image" src="/WWW/Logo-MMM-color.png" alt="Admin Form" width="100" align="right"/></form></b></h2></div>
    )rawliteral");
  //----------------------------------------------
  response->print("<b><h4>&#160;LoRa SMS - UPF 2020</h4><h5>&#160;&#160;&#160;&#160;&#160;&#160;My phone address: <button onclick=\"copyClientAddress()\" align=\"right\">Copy</button></h5><h5 align=\"center\">"+MauMe.addressBuffer2MacFormat(macFrom.dat, MM_ADDRESS_SIZE)+"</h5></b>");
  //--------------  PARAM_TYPE  ------------------------------------ 
  response->print(R"rawliteral(<h6>&#160;&#160;&#160;&#160;&#160;&#160; SMS 64 symbols max !&#160;&#160;&#160;&#160;<button onClick="window.location.reload();" align="right">Refresh Page</button></h6>
    <div style="background-color: seashell;padding:6px 10px 8px;border-width:1px;border-radius:10px;border-style:solid;border-color:lightsteelblue;"><h3><form action="/get">To&#160;&#160;&#160;&#160;:&#160; <input type="text" name="DEST"><br/>&#13;Text: &#160;<br/>&#13;&#160;&#160;&#160;&#160;&#160;<textarea name="MESS" rows="2" cols="32" maxlength="64"></textarea><br/>&#13;
    &#160;&#160;&#160;&#160;&#160;<input type="submit" id="submitBtn" value="SEND" align="right"></form></h3></div><br/><div style="background-color: seashell;padding:6px 10px 8px;border-width:1px;border-radius:10px;border-style:solid;border-color:darkseagreen;"><h5>)rawliteral");
  
  // response->print(R"rawliteral(<h4 align="center"><script language = "javascript" type = "text/javascript">document.write(Date.getTime());</script></h4>)rawliteral");
  //-------------------------------------------------- MMM_APPTYP_PRV MMM_APPTYP_NOD
  response->print(MauMe.listReceived2Str(SPIFFS, MauMe.spiMutex, "/RECMES", appType, macFrom) + "</h5></div><br/>");
  response->print("<div style=\"background-color: seashell;padding:6px 10px 8px;border-width:1px;border-radius:10px;border-style:solid;border-color:darkseagreen;\"><h5>"+ MauMe.listSent2Str(SPIFFS, MauMe.spiMutex, "/NODEPKTS", appType, macFrom));
  //--------------------------------------------------
  response->print(R"rawliteral(</h5></div><div align="right"><button onClick="window.location.reload();" align="right">Refresh Page</button></div>
    <div><h2 align="center"><form action="/"><input type="image" src="/WWW/Logo-UPF-color.png" alt="Home Page" width="100" align="center"/></form></h2></div>
    )rawliteral");
  response->print(R"rawliteral(<script>
      function copyClientAddress(){var input = document.createElement('input');input.setAttribute('value', ")rawliteral");
  response->print(MauMe.addressBuffer2MacFormat(macFrom.dat, MM_ADDRESS_SIZE));  
  response->print(R"rawliteral(");document.body.appendChild(input);input.select();var result = document.execCommand('copy');
        document.body.removeChild(input);window.alert("Copied !");}</script></body></html>)rawliteral");
  //--------------------------------------------------
  return response;
}
//-----------------------------------------------------------------------------------------------------------------------------------------------------
AsyncResponseStream * IRAM_ATTR MauMeClass::getAdminPage(AsyncWebServerRequest *request, ADDRESS macFrom, byte appType){  //  MMM_APPTYP_NOD
  AsyncResponseStream *response = request->beginResponseStream("text/html");
  response->addHeader("Server","Mau-Me Web Server");
  response->print(R"rawliteral(<!DOCTYPE HTML><html><head>
    <title>Mau-Me LoRa Messenger</title>
    <meta charset="ISO-8859-1">
    <meta name="viewport" content="width=device-width, initial-scale=1">
    <link rel="shortcut icon" type="image/x-icon" href="/WWW/favicon.ico" /><link rel="apple-touch-icon" sizes="180x180" href="/WWW/apple-touch-icon.png">
    <link rel="icon" type="image/png" sizes="32x32" href="/WWW/favicon-32x32.png"><link rel="icon" type="image/png" sizes="16x16" href="/WWW/favicon-16x16.png">
    <link rel="manifest" href="/WWW/site.webmanifest"><link rel="mask-icon" href="/WWW/safari-pinned-tab.svg" color="#5bbad5"><meta name="apple-mobile-web-app-title" content="Mau-Me">
    <meta name="application-name" content="Mau-Me"><meta name="msapplication-TileColor" content="#ffffff"><meta name="theme-color" content="#ffffff">
    <style type="text/css">
      html,body{font-family:Verdana,sans-serif;font-size:15px;line-height:1.5}html{overflow-x:hidden}
      h1{font-size:36px}h2{font-size:30px}h3{font-size:24px}h4{font-size:20px}h5{font-size:18px}h6{font-size:15px}.w3-serif{font-family:serif}
      h1,h2,h3,h4,h5,h6{font-family:"Segoe UI",Arial,sans-serif;font-weight:400;margin:10px 0}.w3-wide{letter-spacing:4px}
      h2{color:orangered;}h3{color:darkred;}h4{color:firebrick;}h5{color:darkslategray;}
      body{background-color: cornsilk;}
      #submitBtn{background:crimson;padding:5px 7px 6px;font:bold;color:#fff;border:1px solid #eee;border-radius:6px;box-shadow: 5px 5px 5px #eee;text-shadow:none;}
      </style>
      </head><body><div><h2 align="center"><b>Mau-Me Admin</b><form action="/"><input type="image" src="/WWW/Logo-MMM-color.png" alt="Client Form" width="100" align="right"/></form></h2></div>
    )rawliteral");
  //----------------------------------------------
  response->print("<h4><b>&#160;LoRa SMS - UPF 2020</b></h4><br/><h5><b>&#160;&#160;&#160;Node "+MauMe.myMacAddress+"<br/>");
  response->print("Node address: <button onclick=\"copyNodeAddress()\" align=\"right\">Copy</button></h5><h5 align=\"center\">"+MauMe.addressBuffer2MacFormat(MauMe.myByteAddress.dat, MM_ADDRESS_SIZE)+"</b></h5>");
  response->print("<b><h5>&#160;&#160;&#160;&#160;&#160;&#160;My phone address: <button onclick=\"copyClientAddress()\" align=\"right\">Copy</button></h5><h5 align=\"center\">"+MauMe.addressBuffer2MacFormat(macFrom.dat, MM_ADDRESS_SIZE)+"</h5></b>");
  response->print(R"rawliteral(<h5>&#160;&#160;<button onClick="window.location.reload();" align="right">Refresh Page</button></h5>)rawliteral");
  //---------------------------------------------- 
  response->print(R"rawliteral(<h4 align="Left">MauMe Process Parameters:</h4><h5 align="center"><form action="/setValues">
      Power Min (2)&#160;&#160;&#160;&#160;&#160;&#160;:<input type="text" name="PRCSS_TX_POW_MIN" maxlength="6" value=")rawliteral");
      response->print(int(MauMe.MM_TX_POWER_MIN));
      response->print(R"rawliteral("></br>Power Max (20)&#160;&#160;&#160;:<input type="text" name="PRCSS_TX_POW_MAX" maxlength="6" value=")rawliteral");
      response->print(int(MauMe.MM_TX_POWER_MAX));
      response->print(R"rawliteral("></br>
      Delay Min (s):<input type="text" name="PRCSS_MIN_DELAY" maxlength="10" value=")rawliteral");
      response->print(int(MauMe.MM_PCKT_PROCESS_STATIC_DELAY/1000));
      response->print(R"rawliteral("></br>Delay Max (s):<input type="text" name="PRCSS_MAX_DELAY" maxlength="10" value=")rawliteral");
      response->print(int(float(MauMe.MM_PCKT_PROCESS_STATIC_DELAY+MauMe.MM_PCKT_PROCESS_RANDOM_DELAY)/1000));
      response->print(R"rawliteral("></br><input type="submit" id="submitBtn" value="Set values" name="setTXValues"/><br/>
      </form></h5>)rawliteral");   
  response->print(R"rawliteral(<h4 align="Left">MauMe Autosave Frequency:</h4><h5 align="center"><form action="/setValues">
      Save delay (s):<input type="text" name="SAVE_DELAY" maxlength="6" value=")rawliteral");
      response->print(int(MauMe.MM_PCKT_SAVE_DELAY/1000));
      response->print(R"rawliteral("></br><input type="submit" id="submitBtn" value="Set value" name="setSAVEValues"/><br/>
      </form></h5>)rawliteral");
  //----------------------------------------------
  response->print(R"rawliteral(
    <h4 align="Left">Got to Simulations form :</h4><h4 align="center"><form action="/simus">
      <input type="submit" id="submitBtn" value="Go to Simulations" name="simulations"/><br/>
    </form></h4>)rawliteral");
  response->print(R"rawliteral(
    <h4 align="Left">MauMe File System :</h4><h4 align="center"><form action="/delete">
      <input type="submit" id="submitBtn" value="Delete NODEACKS"   name="DelNODEACKS"/>&#160;&#160;<input type="submit" id="submitBtn" value="Deletes All Files" name="DelAll"/><br/>
      <input type="submit" id="submitBtn" value="Delete RECMES"     name="DelRECMES"  />&#160;&#160;<input type="submit" id="submitBtn" value=" Delete ACKSSENT" name="DelACKSSENT"/><br/>
      <input type="submit" id="submitBtn" value=" Delete PKTS "     name="DelPKTS"    />&#160;&#160;<input type="submit" id="submitBtn" value="Delete ACKS2SEND" name="DelACKS2SEND"/><br/>
      <input type="submit" id="submitBtn" value="Delete RECACKS"    name="DelREC"     />&#160;&#160;<input type="submit" id="submitBtn" value=" Delete NODEPKTS" name="DelNODEPKTS"/><br/>
    </form></h4>)rawliteral");
  //-------------------------------------------------- MMM_APPTYP_PRV MMM_APPTYP_NOD
  //listReceived2Str(SPIFFS, MauMe.spiMutex, "/RECMES", appType, macFrom) + listSent2Str(SPIFFS, MauMe.spiMutex, "/NODEPKTS", appType, macFrom);
  response->print("<h3><b>Directory listing :</b></h3>");
  response->print("<h4><b>RECMES files    : </b></h4><h6>"+listDir2Str(SPIFFS, MauMe.spiMutex, "/RECMES",    "<br/>")+"</h6>");
  response->print("<h4><b>NODEPKTS files  : </b></h4><h6>"+listDir2Str(SPIFFS, MauMe.spiMutex, "/NODEPKTS",  "<br/>")+"</h6>");
  response->print("<h4><b>NODEACKS files  : </b></h4><h6>"+listDir2Str(SPIFFS, MauMe.spiMutex, "/NODEACKS",  "<br/>")+"</h6>");
  response->print("<h4><b>ACKSSENT files  : </b></h4><h6>"+listDir2Str(SPIFFS, MauMe.spiMutex, "/ACKSSENT",  "<br/>")+"</h6>");
  response->print("<h4><b>PKTS files      : </b></h4><h6>"+listDir2Str(SPIFFS, MauMe.spiMutex, "/PKTS",      "<br/>")+"</h6>");
  response->print("<h4><b>ACKS2SEND files : </b></h4><h6>"+listDir2Str(SPIFFS, MauMe.spiMutex, "/ACKS2SEND", "<br/>")+"</h6>");
  response->print("<h4><b>RECACKS files   : </b></h4><h6>"+listDir2Str(SPIFFS, MauMe.spiMutex, "/RECACKS",   "<br/>")+"</h6>");
  response->print("<h4><b>WEB DATA files  : </b></h4><h6>"+listDir2Str(SPIFFS, MauMe.spiMutex, "/WWW",       "<br/>")+"</h6>");
  //--------------------------------------------------
  /*
  )rawliteral");
  response->print(MauMe.addressBuffer2MacFormat(MauMe.myByteAddress.dat, MM_ADDRESS_SIZE)); // window.alert("Hello World 0");        
  response->print(R"rawliteral(
  window.alert("Hello World 1");
  response->print(MauMe.addressBuffer2MacFormat(macFrom.dat, MM_ADDRESS_SIZE));      // window.alert("Hello World 2"); 
  */
  response->print(R"rawliteral(<div align="right"><button onClick="window.location.reload();" align="right">Refresh Page</button></div>
    <div><h2 align="center"><form action="/"><input type="image" src="/WWW/Logo-UPF-color.png" alt="Home Page" width="100" align="center"/></form></h2></div>)rawliteral");
  response->print(R"rawliteral(<script>
      function copyNodeAddress(){var input = document.createElement('input');input.setAttribute('value', ")rawliteral");
  response->print(MauMe.addressBuffer2MacFormat(MauMe.myByteAddress.dat, MM_ADDRESS_SIZE)); // window.alert("Hello World 0");        
  response->print(R"rawliteral(");document.body.appendChild(input);input.select();var result = document.execCommand('copy');
        document.body.removeChild(input);window.alert("Copied !"); }function copyClientAddress(){var input = document.createElement('input');input.setAttribute('value', ")rawliteral");
  response->print(MauMe.addressBuffer2MacFormat(macFrom.dat, MM_ADDRESS_SIZE));  
  response->print(R"rawliteral(");document.body.appendChild(input);input.select();var result = document.execCommand('copy');
        document.body.removeChild(input);window.alert("Copied !");}</script></body></html>)rawliteral");
  //--------------------------------------------------
  return response;
}
//-----------------------------------------------------------------------------------------------------------------------------------------------------
AsyncResponseStream * IRAM_ATTR MauMeClass::getSimusPage(AsyncWebServerRequest *request, ADDRESS macFrom, byte appType){  //  MMM_APPTYP_NOD
  AsyncResponseStream *response = request->beginResponseStream("text/html");
  response->addHeader("Server","Mau-Me Web Server");
  response->print(R"rawliteral(<!DOCTYPE HTML><html><head>
    <title>Mau-Me LoRa Messenger</title>
    <meta charset="ISO-8859-1">
    <meta name="viewport" content="width=device-width, initial-scale=1">
    <link rel="shortcut icon" type="image/x-icon" href="/WWW/favicon.ico" /><link rel="apple-touch-icon" sizes="180x180" href="/WWW/apple-touch-icon.png">
    <link rel="icon" type="image/png" sizes="32x32" href="/WWW/favicon-32x32.png"><link rel="icon" type="image/png" sizes="16x16" href="/WWW/favicon-16x16.png">
    <link rel="manifest" href="/WWW/site.webmanifest"><link rel="mask-icon" href="/WWW/safari-pinned-tab.svg" color="#5bbad5"><meta name="apple-mobile-web-app-title" content="Mau-Me">
    <meta name="application-name" content="Mau-Me"><meta name="msapplication-TileColor" content="#ffffff"><meta name="theme-color" content="#ffffff">
    <style type="text/css"> // seashell
      html,body{font-family:Verdana,sans-serif;font-size:15px;line-height:1.5}html{overflow-x:hidden}
      h1{font-size:36px}h2{font-size:30px}h3{font-size:24px}h4{font-size:20px}h5{font-size:18px}h6{font-size:15px}.w3-serif{font-family:serif}
      h1,h2,h3,h4,h5,h6{font-family:"Segoe UI",Arial,sans-serif;font-weight:400;margin:10px 0}.w3-wide{letter-spacing:4px}
      h2{color:limegreen;}h3{color:darkgreen;}h4{color:darkolivegreen;}h5{color:saddlebrown;}
      body{background-color: cornsilk;}
      #submitBtn{background:seagreen;padding:5px 7px 6px;font:bold;color:#fff;border:1px solid #eee;border-radius:6px;box-shadow: 5px 5px 5px #eee;text-shadow:none;}
      </style>
      </head><body><div><h2 align="center"><b>Mau-Me Simus</b><form action="/admin"><input type="image" src="/WWW/Logo-MMM-color.png" alt="Admin Form" width="100" align="right"/></form></h2></div>
    )rawliteral");
  //----------------------------------------------
  //response->print("<h3><b>&#160;LoRa SMS - UPF 2020</b><br/><h3><b>&#160;&#160;&#160;Node: "+addressBuffer2MacFormat(myByteAddress.dat, MM_ADDRESS_SIZE)+"</b></h3>");
  response->print("<h4><b>&#160;LoRa SMS - UPF 2020</b></h4><br/><h5><b>&#160;&#160;&#160;Node "+MauMe.myMacAddress+"<br/>Node address : <button onclick=\"copyNodeAddress()\" align=\"right\">Copy</button>");
  response->print(MauMe.addressBuffer2MacFormat(MauMe.myByteAddress.dat, MM_ADDRESS_SIZE)+"</b></h5>");  
  response->print("<b><h5>&#160;&#160;&#160;&#160;&#160;&#160;My phone address:</h5><h5 align=\"center\">"+MauMe.addressBuffer2MacFormat(macFrom.dat, MM_ADDRESS_SIZE));
  response->print(R"rawliteral(&#160;&#160;<button onClick="window.location.reload();" align="right">Refresh Page</button></h5></b>)rawliteral");
  //----------------------------------------------
  //File file = fileOpenFor(SPIFFS, MauMe.spiMutex, MauMe.logFile.c_str(), FILE_READ); 
  // doAlternate   percentActivity  alternateInterval PRCT_ACTIVITY INTER_ACTIVITY  actAltAct deActAltAct
  if(!MauMe.doAlternate){
    response->print(R"rawliteral(<h4 align="center"><form action="/activateSim">
      % activity :<input type="text" name="PRCT_ACTIVITY" maxlength="12" value=")rawliteral");
      response->print(MauMe.percentActivity);
      response->print(R"rawliteral("></br>Interval (s) :<input type="text" name="INTER_ACTIVITY" maxlength="12" value=")rawliteral");
      response->print(int(MauMe.alternateInterval/1000));
      response->print(R"rawliteral("></br><input type="submit" id="submitBtn" value="Activate Alternating" name="actAltAct"/><br/>
      </form></h4>)rawliteral");
  }else{ // actAltAct   deActAltAct
    response->print(R"rawliteral(<h4 align="center"><form action="/activateSim">
      % activity :<input type="text" name="PRCT_ACTIVITY" maxlength="12" value=")rawliteral");
      response->print(MauMe.percentActivity);
      response->print(R"rawliteral(" readonly> %.</br>Interval (s) :<input type="text" name="INTER_ACTIVITY" maxlength="12" value=")rawliteral");
      response->print(int(MauMe.alternateInterval/1000));
      response->print(R"rawliteral(" readonly></br><input type="submit" id="submitBtn" value="De-Activate Alternating" name="deActAltAct"/><br/>
      </form></h4>)rawliteral");
  }
  if(MauMe.sendDummies){
    response->print(R"rawliteral(
      <h4 align="center"><form action="/activateSim">        
      <input type="submit" id="submitBtn" value="De-Activate Sim Transmit" name="DeActSimTx"/><br/></form>)rawliteral");
    response->print(R"rawliteral(</br><h5 align="left">Sending )rawliteral");
    response->print(MauMe.nbDummyPkts);
    response->print(R"rawliteral( to :&#160;)rawliteral");
    response->print(MauMe.dummyTargetAddress);
    response->print(R"rawliteral(</h5>)rawliteral"); // <h5 align="center"></h5>
  }else{
    response->print(R"rawliteral(
      <h4 align="center"><form action="/activateSim">
      TX static delay (s) :<input type="text" name="TX_STATIC_DELAY" maxlength="12" value=")rawliteral");
      response->print(int(MauMe.DUMMY_PROCESS_STATIC_DELAY/1000));
      response->print(R"rawliteral("></br>TX random delay (s) :<input type="text" name="TX_RANDOM_DELAY" maxlength="12" value=")rawliteral");
      response->print(int(MauMe.DUMMY_PROCESS_RANDOM_DELAY/1000));
      response->print(R"rawliteral("></br>TRGT ADDR :<input type="text" name="TRGT_ADDR" maxlength="40"value=")rawliteral");
      response->print(MauMe.dummyTargetAddress);
      response->print(R"rawliteral("></br>Nb. of TX :<input type="text" name="NB_DUM" maxlength="6" value=")rawliteral");
      response->print(String(MauMe.nbDummyPkts));
      response->print(R"rawliteral("></br><input type="submit" id="submitBtn" value="Activate Sim Transmit" name="actSimTx"/><br/>
      </form></h4>)rawliteral");        
  }
  if(fileExists(SPIFFS, MauMe.spiMutex, MauMe.logFile.c_str())){
    //file.close();
    String logString = readFile(SPIFFS, MauMe.spiMutex, MauMe.logFile);
    #ifdef MAUME_DEBUG // #endif
      Serial.println(" * Log file:\n"+logString);
    #endif
    response->print(R"rawliteral(
        <h4 align="center"><form action=")rawliteral");
    response->print(MauMe.logFile);
    response->print(R"rawliteral("><input type="submit" id="submitBtn" value="Download Log" name="Download" download=")rawliteral");
    response->print(MauMe.logFile);
    response->print(R"rawliteral("/><br/></form></h4>)rawliteral");
    response->print(R"rawliteral(<h4 align="center"><form action="/activateSim">
        <input type="submit" id="submitBtn" value="Reset Log File" name="Reset"/><br/>
        </form></h4>)rawliteral");
        //-------------------------------------------------- MMM_APPTYP_PRV MMM_APPTYP_NOD
    response->print("<h3><b> Log file :</b></h3>");
    response->print("<h6>"+logString+"</h6>");
  }else{
    response->print("<h4><b> No Log file.</b></h4>");
  }
  response->print(R"rawliteral(<div align="right"><button onClick="window.location.reload();" align="right">Refresh Page</button></div>
    <div><h2 align="center"><form action="/"><input type="image" src="/WWW/Logo-UPF-color.png" alt="Home Page" width="100" align="center"/></form></h2></div>
    </form></h2></div>)rawliteral");
  response->print(R"rawliteral(<script>
      function copyNodeAddress(){var input = document.createElement('input');input.setAttribute('value', ")rawliteral");
  response->print(MauMe.addressBuffer2MacFormat(MauMe.myByteAddress.dat, MM_ADDRESS_SIZE)); // window.alert("Hello World 0");        
  response->print(R"rawliteral(");document.body.appendChild(input);input.select();var result = document.execCommand('copy');
        document.body.removeChild(input);window.alert("Copied !"); }</script></body></html>)rawliteral");
  //--------------------------------------------------
  return response;
}
//------------------------------------------------------------------------------------------------------------------------------
void MauMeClass::setupWebServer(){
  #ifdef MAUME_DEBUG  // #endif
    Serial.println(" * Setting-up web server");
  #endif
  MauMe.server = new AsyncWebServer(80);//&webServer;
  //------------------------------------------------------------- SERVER
  //======================================================================================================
  server->on("/", HTTP_GET, [](AsyncWebServerRequest *request){
    #ifdef MAUME_DEBUG 
      Serial.println(" ------------------------------------------- Serving WEB -----");
    #endif
    if(MauMe.doServe && request != NULL){
      struct eth_addr * clntMacAddr = NULL;
      #ifdef MAUME_DEBUG 
        Serial.println(" -> Fetching MAC");
      #endif
      clntMacAddr = MauMe.getClientMAC(request->client()->remoteIP());
      if(clntMacAddr != NULL){
        #ifdef MAUME_DEBUG 
          Serial.println(" -> Found");
        #endif
        ADDRESS macFrom = MauMe.addressFromBytes(MauMe.getSha256BytesOfBytes((char*)clntMacAddr->addr, (const size_t)6));
        #ifdef MAUME_DEBUG /// #endif
          Serial.println(" -> macFrom.dat : " + MauMe.addressBuffer2Str((char*)macFrom.dat, MM_ADDRESS_SIZE)+".");
        #endif  
        AsyncResponseStream * response = NULL;
        #ifdef MAUME_DEBUG   // #endif
          Serial.println(" -> Getting page");
        #endif
        response = MauMe.getWebPage(request, macFrom, (byte)MMM_APPTYP_NOD);
        if(response != NULL){
          #ifdef MAUME_DEBUG   // #endif
            Serial.println(" -> Got it.");
          #endif
          request->send(response);
        }else{
          Serial.println(" -> Error serving http. RESP NULL.");
          request->send(200, "text/html", "<br><h2 style=\"font-size:30px;font-family:\"Segoe UI\",Arial,sans-serif;font-weight:400;margin:10px 0;color:orangered;\"> MauMe Node "+MauMe.MyAddress+" - Could not assemble data !</h2><br><br><a href=\"/\">Return to Mau-Me Page</a>");
        }
      }else{
        Serial.println(" -> Error serving http. ADDR NULL.");
        request->send(200, "text/html", "<br><h2 style=\"font-size:30px;font-family:\"Segoe UI\",Arial,sans-serif;font-weight:400;margin:10px 0;color:orangered;\"> MauMe Node "+MauMe.MyAddress+" - Cannot identify client. Reconnect !</h2><br><br><a href=\"/\">Return to Mau-Me Page</a>");
      }
    }else{
        Serial.println(" -> Error serving http. NOT SERVING.");
        request->send(200, "text/html", "<br><h2 style=\"font-size:30px;font-family:\"Segoe UI\",Arial,sans-serif;font-weight:400;margin:10px 0;color:orangered;\"> MauMe Node "+MauMe.MyAddress+" - Out of order.</h2><br><br><a href=\"/\">Return to Mau-Me Page</a>");
      }
  });
  //======================================================================================================
  server->on("/admin", HTTP_GET, [](AsyncWebServerRequest *request){
    #ifdef MAUME_DEBUG 
        Serial.println(" ------------------------------------------- Serving admin -----");
    #endif
    if(MauMe.doServe && request != NULL){
      struct eth_addr * clntMacAddr = NULL;
      #ifdef MAUME_DEBUG 
        Serial.println(" -> Fetching MAC");
      #endif
      clntMacAddr = MauMe.getClientMAC(request->client()->remoteIP());
      if(clntMacAddr != NULL){
        #ifdef MAUME_DEBUG 
          Serial.println(" -> Found");
        #endif
        ADDRESS macFrom = MauMe.addressFromBytes(MauMe.getSha256BytesOfBytes((char*)clntMacAddr->addr, (const size_t)6));
        AsyncResponseStream * response = NULL;
        #ifdef MAUME_DEBUG 
          Serial.println(" -> Getting page");
        #endif
        #ifdef MAUME_CHECK_ADMIN  // #endif
          ADDRESS adminMac = MauMe.macFromMacStyleString(MAUME_ADMIN_ADDR);//"B0:A2:E7:0E:9E:AF");
          if(MauMe.compareBuffers((const char*)&macFrom.dat, (const char*)&adminMac.dat, MM_ADDRESS_SIZE)){
            response = MauMe.getAdminPage(request, macFrom, (byte)MMM_APPTYP_NOD);
          }else{
            response = MauMe.getWebPage(request, macFrom, (byte)MMM_APPTYP_NOD);
          }
        #else
          response = MauMe.getAdminPage(request, macFrom, (byte)MMM_APPTYP_NOD);
        #endif
        if(response != NULL){
          request->send(response);
        }else{
          Serial.println(" -> Error serving http. NULL.");
          request->send(200, "text/html", "<br><h2 style=\"font-size:30px;font-family:\"Segoe UI\",Arial,sans-serif;font-weight:400;margin:10px 0;color:orangered;\"> MauMe Node "+MauMe.MyAddress+" - Could not assemble data !</h2><br><br><a href=\"/\">Return to Mau-Me Page</a>");
        }
      }else{
        Serial.println(" -> Error serving http. ADDR NULL.");
        request->send(200, "text/html", "<br><h2 style=\"font-size:30px;font-family:\"Segoe UI\",Arial,sans-serif;font-weight:400;margin:10px 0;color:orangered;\"> MauMe Node "+MauMe.MyAddress+" - Cannot identify client. Reconnect !</h2><br><br><a href=\"/\">Return to Mau-Me Page</a>");
      }
    }else{
        Serial.println(" -> Error serving http. NOT SERVING.");
        request->send(200, "text/html", "<br><h2 style=\"font-size:30px;font-family:\"Segoe UI\",Arial,sans-serif;font-weight:400;margin:10px 0;color:orangered;\"> MauMe Node "+MauMe.MyAddress+" - Out of order.</h2><br><br><a href=\"/\">Return to Mau-Me Page</a>");
      }
  });
  
  //======================================================================================================
  server->on("/get", HTTP_GET, [] (AsyncWebServerRequest *request) {
    #ifdef MAUME_DEBUG 
      Serial.println(" ------------------------------------------- Serving Operation -----");
    #endif
    String dest = "", mess = "", type = "";
    //-----------------------------------------------------------------------------------
    struct eth_addr * clntMacAddr = NULL;
    clntMacAddr = MauMe.getClientMAC(request->client()->remoteIP());
    if(!MauMe.doServe or clntMacAddr == NULL){
      Serial.println(" ------------------------------------------- CAN'T GET MAC ADDRESS OF CLIENT -----");
      request->send(200, "text/html", "<br><h2> MauMe Node "+MauMe.MyAddress+" - Unidenfified client.</h2><br><br><a href=\"/\">Return to Home Page</a>");
    }else{
      ADDRESS macFrom = MauMe.addressFromBytes(MauMe.getSha256BytesOfBytes((char*)clntMacAddr->addr, (const size_t)6));
      dest = "";
      mess = "";
      if(request->hasParam(MauMe.PARAM_DEST)){
        dest = request->getParam(MauMe.PARAM_DEST)->value();
      }else{
        Serial.println(" ------------------------------------------- NO DEST IN FORM -----");
      }
      if(request->hasParam(MauMe.PARAM_MESS)){
        mess = request->getParam(MauMe.PARAM_MESS)->value();
      }else{
        Serial.println(" ------------------------------------------- NO MESS IN FORM -----");
      }
      ADDRESS addressTo = MauMe.macFromMacStyleString(dest);
      if(mess.length()==0 || mess.length()>64){
        Serial.println(" ------------------------------------------- MESS SHAPE WRONG -----");
        request->send(200, "text/html", "<br><h2> MauMe Node "+MauMe.MyAddress+" - Malformed request ! Text too short or too long.</h2><br><br><a href=\"/\">Return to Home Page</a>");
      }else{
        MauMePKT_0 * mmPckt = NULL;
        mmPckt = MauMe.createPacket(macFrom, addressTo, mess, MMM_APPTYP_NOD); // macFrom
        if(mmPckt != NULL){          
          String strMMM = MauMe.mes2Str(mmPckt);
          LoRa_PKT * lrPkt = NULL;
          lrPkt = MauMe.getLoRaPacketFromMMPKT(mmPckt);
          free(mmPckt);mmPckt = NULL;
          if(lrPkt != NULL){
            #ifdef MAUME_DEBUG 
              Serial.println("-----------------------------------------\n PKT DAT : ");
              Serial.write((const uint8_t*)lrPkt, lrPkt->PKTSIZ);
              Serial.println("\n-----------------------------------------\n");
            #endif
            bool ret = MauMe.addPacket(lrPkt);
            if(ret){
              if(MauMe.saveLoRaPkt("/NODEPKTS/", lrPkt, false)){ // "/PKTS/"
                request->send(200, "text/html", "<br><h2 style=\"font-size:30px;font-family:\"Segoe UI\",Arial,sans-serif;font-weight:400;margin:10px 0;color:orangered;\"> MauMe Node "+MauMe.MyAddress+" - Message enlisted and saved: </h2><br><br>" + strMMM +"<br><br><a href=\"/\">Return to Mau-Me Page</a>");
              }else{
                request->send(200, "text/html", "<br><h2 style=\"font-size:30px;font-family:\"Segoe UI\",Arial,sans-serif;font-weight:400;margin:10px 0;color:orangered;\"> MauMe Node "+MauMe.MyAddress+" - Message enlisted but not saved: </h2><br><br>" + strMMM +"<br><br><a href=\"/\">Return to Mau-Me Page</a>");
              }
            }else{
              request->send(200, "text/html", "<br><h2 style=\"font-size:30px;font-family:\"Segoe UI\",Arial,sans-serif;font-weight:400;margin:10px 0;color:orangered;\"> MauMe Node "+MauMe.MyAddress+" - Message enlisting failed</h2><br><br><a href=\"/\">Return to Mau-Me Page</a>");
            }
            // free(lrPkt);lrPkt = NULL; DO NOT FREE PACKET IN LIST !!!!!!!!!!!!!!!
          }else{
            request->send(200, "text/html", "<br><h2 style=\"font-size:30px;font-family:\"Segoe UI\",Arial,sans-serif;font-weight:400;margin:10px 0;color:orangered;\"> MauMe Node "+MauMe.MyAddress+" - Packet creation failed</h2><br><br><a href=\"/\">Return to Mau-Me Page</a>");
          }
        }else{
          Serial.println(" ------------------------------------------- PACKET VOID -----");
          request->send(200, "text/html", "<br><h2 style=\"font-size:30px;font-family:\"Segoe UI\",Arial,sans-serif;font-weight:400;margin:10px 0;color:orangered;\"> MauMe Node "+MauMe.MyAddress+" - Malformed request !</h2><br><br><a href=\"/\">Return to Mau-Me Page</a>");
        }
      }  
    }    
  });
  //======================================================================================================
  server->on("/delete", HTTP_GET, [](AsyncWebServerRequest *request){
    #ifdef MAUME_DEBUG 
      Serial.println(" ------------------------------------------- Serving DELETE Page- ");
    #endif
    struct eth_addr * clntMacAddr = NULL;
    clntMacAddr = MauMe.getClientMAC(request->client()->remoteIP());
    if(clntMacAddr != NULL && MauMe.doServe){
      #ifdef MAUME_CHECK_ADMIN  // #endif    
        ADDRESS adminMac = MauMe.macFromMacStyleString(MAUME_ADMIN_ADDR);//"B0:A2:E7:0E:9E:AF");
        ADDRESS macFrom = MauMe.addressFromBytes(MauMe.getSha256BytesOfBytes((char*)clntMacAddr->addr, (const size_t)6));      
        if(MauMe.compareBuffers((const char*)&macFrom.dat, (const char*)&adminMac.dat, MM_ADDRESS_SIZE)){
      #endif
          bool DelAll = false, DelNODEACKS = false, DelRECMES = false, DelRECACKS = false, DelACKSSENT = false, DelPKTS = false, DelACKS2SEND = false, DelNODEPKTS = false;
          if (request->hasParam("DelAll")) {
            #ifdef MAUME_DEBUG 
              Serial.println(" ------------------------------------------- DelAll Button - ");
            #endif
            DelAll = true;
          }else if (request->hasParam("DelNODEACKS")) {
            #ifdef MAUME_DEBUG 
              Serial.println(" ------------------------------------------- DelNODEACKS Button - ");
            #endif
            DelNODEACKS = true;
          }else if (request->hasParam("DelRECMES")) {
            #ifdef MAUME_DEBUG 
              Serial.println(" ------------------------------------------- DelRECMES Button - ");
            #endif
            DelRECMES = true;
          }else if (request->hasParam("DelACKSSENT")) {
            #ifdef MAUME_DEBUG 
              Serial.println(" ------------------------------------------- DelACKSSENT Button - ");
            #endif
            DelACKSSENT = true;
          }else if (request->hasParam("DelPKTS")) {
            #ifdef MAUME_DEBUG 
              Serial.println(" ------------------------------------------- DelPKTS Button - ");
            #endif
            DelPKTS = true;
          }else if (request->hasParam("DelACKS2SEND")) {
            #ifdef MAUME_DEBUG 
              Serial.println(" ------------------------------------------- DelACKS2SEND Button - ");
            #endif
            DelACKS2SEND = true;
          }else if (request->hasParam("DelNODEPKTS")) {
            #ifdef MAUME_DEBUG 
              Serial.println(" ------------------------------------------- DelNODEPKTS Button - ");
            #endif
            DelNODEPKTS = true;
          }else if (request->hasParam("DelRECACKS")) {
            #ifdef MAUME_DEBUG 
              Serial.println(" ------------------------------------------- DelRECACKS Button - ");
            #endif
            DelRECACKS = true;
          } 
          int dels = 0;
          if(DelAll || DelNODEPKTS){dels += deleteDirectoryFiles(SPIFFS, MauMe.spiMutex,  "/NODEPKTS");}
          if(DelAll || DelNODEACKS){dels += deleteDirectoryFiles(SPIFFS, MauMe.spiMutex,  "/NODEACKS");}
          if(DelAll || DelRECMES){dels += deleteDirectoryFiles(SPIFFS, MauMe.spiMutex,    "/RECMES");}
          if(DelAll || DelACKSSENT){dels += deleteDirectoryFiles(SPIFFS, MauMe.spiMutex,  "/ACKSSENT");}
          if(DelAll || DelPKTS){dels += deleteDirectoryFiles(SPIFFS, MauMe.spiMutex,      "/PKTS");}
          if(DelAll || DelACKS2SEND){dels += deleteDirectoryFiles(SPIFFS, MauMe.spiMutex, "/ACKS2SEND");}
          if(DelAll || DelRECACKS){dels += deleteDirectoryFiles(SPIFFS, MauMe.spiMutex,   "/RECACKS");}
          #ifdef MAUME_DEBUG 
            Serial.println(" Deleted files ok : "+String(dels)+".");
          #endif
          if(dels>0){ // "/PKTS/"
            request->send(200, "text/html", "<br><h2 style=\"font-size:30px;font-family:\"Segoe UI\",Arial,sans-serif;font-weight:400;margin:10px 0;color:orangered;\"> MauMe Node "+MauMe.MyAddress+" - Deleted "+String(dels) +" files. </h2><br><br><br><br><a href=\"/admin\">Return to Mau-Me Admin Page</a>");
          }else{
            request->send(200, "text/html", "<br><h2 style=\"font-size:30px;font-family:\"Segoe UI\",Arial,sans-serif;font-weight:400;margin:10px 0;color:orangered;\"> MauMe Node "+MauMe.MyAddress+" - No files deleted. </h2><br><br><br><br><a href=\"/admin\">Return to Mau-Me Admin Page</a>");
          }
      #ifdef MAUME_CHECK_ADMIN  // #endif
        }else{
          request->send(MauMe.getWebPage(request, macFrom, (byte)MMM_APPTYP_NOD));
        }
      #endif
      /*}else{
        request->send(200, "text/html", "<br><h2> MauMe Node "+MauMe.MyAddress+" - You do not have credentials to do that. </h2><br><br><br><br><a href=\"/admin\">Return to Mau-Me Admin Page</a>");
      } */
    }else{
      request->send(200, "text/html", "<br><h2 style=\"font-size:30px;font-family:\"Segoe UI\",Arial,sans-serif;font-weight:400;margin:10px 0;color:orangered;\"> MauMe Node "+MauMe.MyAddress+" - You do not have credentials to do that. </h2><br><br><br><br><a href=\"/admin\">Return to Mau-Me Admin Page</a>");
    }
  });

  //======================================================================================================
  server->on("/simus", HTTP_GET, [](AsyncWebServerRequest *request){
    #ifdef MAUME_DEBUG 
        Serial.println(" ------------------------------------------- Serving Simulation Page -----");
    #endif
    if(request != NULL && MauMe.doServe){
      struct eth_addr * clntMacAddr = NULL;
      #ifdef MAUME_DEBUG 
        Serial.println(" -> Fetching MAC");
      #endif
      clntMacAddr = MauMe.getClientMAC(request->client()->remoteIP());
      if(clntMacAddr != NULL){
        #ifdef MAUME_DEBUG 
          Serial.println(" -> Found client address.");
        #endif
        ADDRESS macFrom = MauMe.addressFromBytes(MauMe.getSha256BytesOfBytes((char*)clntMacAddr->addr, (const size_t)6));
        AsyncResponseStream * response = NULL;
        #ifdef MAUME_DEBUG 
          Serial.println(" -> Getting simulation page");
        #endif
        #ifdef MAUME_CHECK_ADMIN  // #endif
          ADDRESS adminMac = MauMe.macFromMacStyleString(MAUME_ADMIN_ADDR);//"B0:A2:E7:0E:9E:AF");
          if(MauMe.compareBuffers((const char*)&macFrom.dat, (const char*)&adminMac.dat, MM_ADDRESS_SIZE)){
            response = MauMe.getSimusPage(request, macFrom, (byte)MMM_APPTYP_NOD);
          }else{
            response = MauMe.getWebPage(request, macFrom, (byte)MMM_APPTYP_NOD);
          }
        #else
          response = MauMe.getSimusPage(request, macFrom, (byte)MMM_APPTYP_NOD);
        #endif
        if(response != NULL){
          request->send(response);          
        }else{
          Serial.println(" -> Error serving http. SIMUS RESP NULL.");
          request->send(200, "text/html", "<br><h2style=\"font-size:30px;font-family:\"Segoe UI\",Arial,sans-serif;font-weight:400;margin:10px 0;color:orangered;\"> MauMe Node "+MauMe.MyAddress+" - Could not assemble data !</h2><br><br><a href=\"/simus\">Return to Mau-Me Page</a>");
        }
      }else{
        Serial.println(" -> Error serving http. SIMUS ADDR NULL.");
        request->send(200, "text/html", "<br><h2style=\"font-size:30px;font-family:\"Segoe UI\",Arial,sans-serif;font-weight:400;margin:10px 0;color:orangered;\"> MauMe Node "+MauMe.MyAddress+" - Cannot identify client. Reconnect !</h2><br><br><a href=\"/simus\">Return to Mau-Me Page</a>");
      }
    }else{
        Serial.println(" -> Error serving http. SIMUS NOT SERVING.");
        request->send(200, "text/html", "<br><h2 style=\"font-size:30px;font-family:\"Segoe UI\",Arial,sans-serif;font-weight:400;margin:10px 0;color:orangered;\"> MauMe Node "+MauMe.MyAddress+" - Out of order.</h2><br><br><a href=\"/\">Return to Mau-Me Page</a>");
      }
  });
  
  // PRCSS_TX_POW_MIN  PRCSS_TX_POW_MAX  PRCSS_MIN_DELAY PRCSS_MAX_DELAY setValues
  //======================================================================================================
  server->on("/setValues", HTTP_GET, [](AsyncWebServerRequest *request){
    #ifdef MAUME_DEBUG  // #endif
        Serial.println(" ------------------------------------------- Serving setValues  -----");
    #endif
    if(request != NULL && MauMe.doServe){
      struct eth_addr * clntMacAddr = NULL;
      #ifdef MAUME_DEBUG 
        Serial.println(" -> Fetching MAC");
      #endif
      clntMacAddr = MauMe.getClientMAC(request->client()->remoteIP());
      if(clntMacAddr != NULL){
        #ifdef MAUME_DEBUG 
          Serial.println(" -> Found client address.");
        #endif
        #ifdef MAUME_CHECK_ADMIN  // #endif
          ADDRESS macFrom = MauMe.addressFromBytes(MauMe.getSha256BytesOfBytes((char*)clntMacAddr->addr, (const size_t)6));
        #endif
        #ifdef MAUME_DEBUG 
          Serial.println(" -> Getting simulation page");
        #endif // doAlternate   percentActivity  alternateInterval PRCT_ACTIVITY INTER_ACTIVITY  actAltAct deActAltAct
        
        #ifdef MAUME_CHECK_ADMIN  // #endif
          ADDRESS adminMac = MauMe.macFromMacStyleString(MAUME_ADMIN_ADDR);//"B0:A2:E7:0E:9E:AF");            
          if(MauMe.compareBuffers((const char*)&macFrom.dat, (const char*)&adminMac.dat, MM_ADDRESS_SIZE)){              
        #endif
          if(request->hasParam("setTXValues")) { // MauMe.MM_PCKT_SAVE_DELAY  setTXValues setSAVEValues SAVE_DELAY
            if(request->hasParam("PRCSS_TX_POW_MIN")){
              #ifdef MAUME_DEBUG  // #endif
                Serial.println(" ----------------------- PRCSS_TX_POW_MIN "+request->getParam("PRCSS_TX_POW_MIN")->value()+" -----");
              #endif
              MauMe.MM_TX_POWER_MIN = request->getParam("PRCSS_TX_POW_MIN")->value().toInt();
            }else{
              #ifdef MAUME_DEBUG  // #endif
                Serial.println(" ------------------------------------------- NO PRCSS_TX_POW_MIN IN FORM -----");
              #endif
            }
            if(request->hasParam("PRCSS_TX_POW_MAX")){
              #ifdef MAUME_DEBUG  // #endif
                Serial.println(" ----------------------- PRCSS_TX_POW_MAX "+request->getParam("PRCSS_TX_POW_MAX")->value()+" -----");
              #endif
              MauMe.MM_TX_POWER_MAX = request->getParam("PRCSS_TX_POW_MAX")->value().toInt();
            }else{
              #ifdef MAUME_DEBUG  // #endif
                Serial.println(" ------------------------------------------- NO PRCSS_TX_POW_MAX IN FORM -----");
              #endif
            }
            if(request->hasParam("PRCSS_MIN_DELAY")){
              #ifdef MAUME_DEBUG  // #endif
                Serial.println(" ----------------------- PRCSS_MIN_DELAY "+request->getParam("PRCSS_MIN_DELAY")->value()+" -----");
              #endif
              // MM_PROCESS_MIN_DELAY  MM_PROCESS_MIN_RANDOM_DELAY
              MauMe.MM_PCKT_PROCESS_STATIC_DELAY = request->getParam("PRCSS_MIN_DELAY")->value().toInt()*1000;
              MauMe.MM_PCKT_PROCESS_STATIC_DELAY = max(MM_PROCESS_MIN_DELAY, MauMe.MM_PCKT_PROCESS_STATIC_DELAY);
            }else{
              #ifdef MAUME_DEBUG  // #endif
                Serial.println(" ------------------------------------------- NO PRCSS_MIN_DELAY IN FORM -----");
              #endif
            }
            if(request->hasParam("PRCSS_MAX_DELAY")){
              #ifdef MAUME_DEBUG  // #endif
                Serial.println(" ----------------------- PRCSS_MAX_DELAY "+request->getParam("PRCSS_MAX_DELAY")->value()+" -----");
              #endif
              MauMe.MM_PCKT_PROCESS_RANDOM_DELAY = request->getParam("PRCSS_MAX_DELAY")->value().toInt()*1000 - MauMe.MM_PCKT_PROCESS_STATIC_DELAY;
              MauMe.MM_PCKT_PROCESS_RANDOM_DELAY = max(MM_PROCESS_MIN_RANDOM_DELAY, MauMe.MM_PCKT_PROCESS_RANDOM_DELAY);
            }else{
              #ifdef MAUME_DEBUG  // #endif
                Serial.println(" ------------------------------------------- NO PRCSS_MAX_DELAY IN FORM -----");
              #endif
            }
            MauMe.Log("-SET TX PARAMS: TXPWMN-"+String(MauMe.MM_TX_POWER_MIN)+";TXPWMX-"+String(MauMe.MM_TX_POWER_MAX)+";TXDLMN-"+String(MauMe.MM_PCKT_PROCESS_STATIC_DELAY)+"ms;TXDLMX-"+String(MauMe.MM_PCKT_PROCESS_STATIC_DELAY+MauMe.MM_PCKT_PROCESS_RANDOM_DELAY)+"ms.");
          }else if(request->hasParam("setSAVEValues")) { // MauMe.MM_PCKT_SAVE_DELAY  setTXValues setSAVEValues SAVE_DELAY
            if(request->hasParam("SAVE_DELAY")){
              #ifdef MAUME_DEBUG  // #endif
                Serial.println(" ----------------------- SAVE_DELAY "+request->getParam("SAVE_DELAY")->value()+" -----");
              #endif
              MauMe.MM_PCKT_SAVE_DELAY = request->getParam("SAVE_DELAY")->value().toInt()*1000;
            }else{
              #ifdef MAUME_DEBUG  // #endif
                Serial.println(" ------------------------------------------- NO SAVE_DELAY IN FORM -----");
              #endif
            }
            MauMe.Log("-SET SAVE DELAY-"+String(MauMe.MM_PCKT_SAVE_DELAY)+"ms.");
          }  
        #ifdef MAUME_CHECK_ADMIN  // #endif
        }
        #endif
        #ifdef MAUME_CHECK_ADMIN  // #endif
          ADDRESS adminMac = MauMe.macFromMacStyleString(MAUME_ADMIN_ADDR);//"B0:A2:E7:0E:9E:AF");
          if(MauMe.compareBuffers((const char*)&macFrom.dat, (const char*)&adminMac.dat, MM_ADDRESS_SIZE)){
            request->send(200, "text/html", "<br><h2 style=\"font-size:30px;font-family:\"Segoe UI\",Arial,sans-serif;font-weight:400;margin:10px 0;color:orangered;\"> MauMe Node "+MauMe.MyAddress+" - Request executed ! </h2><br><br><a href=\"/admin\">Return to Home Page</a>");
          }else{
            request->send(MauMe.getWebPage(request, macFrom, (byte)MMM_APPTYP_NOD));
          }
        #else
          request->send(200, "text/html", "<br><h2 style=\"font-size:30px;font-family:\"Segoe UI\",Arial,sans-serif;font-weight:400;margin:10px 0;color:orangered;\"> MauMe Node "+MauMe.MyAddress+" - Request executed ! </h2><br><br><a href=\"/admin\">Return to Home Page</a>");
        #endif        
      }else{
        Serial.println(" -> Error serving http. SET ADDR NULL.");
        request->send(200, "text/html", "<br><h2 style=\"font-size:30px;font-family:\"Segoe UI\",Arial,sans-serif;font-weight:400;margin:10px 0;color:orangered;\"> MauMe Node "+MauMe.MyAddress+" - Cannot identify client. Reconnect !</h2><br><br><a href=\"/\">Return to Mau-Me Page</a>");
      }
    }else{
      Serial.println(" -> Error serving http. SET NOT SERVING.");
      request->send(200, "text/html", "<br><h2 style=\"font-size:30px;font-family:\"Segoe UI\",Arial,sans-serif;font-weight:400;margin:10px 0;color:orangered;\"> MauMe Node "+MauMe.MyAddress+" - Out of order.</h2><br><br><a href=\"/\">Return to Mau-Me Page</a>");
    }
  });
  //======================================================================================================
  server->on("/activateSim", HTTP_GET, [](AsyncWebServerRequest *request){
    #ifdef MAUME_DEBUG  // #endif
        Serial.println(" ------------------------------------------- Serving Simulations Activation -----");
    #endif
    if(request != NULL && MauMe.doServe){
      struct eth_addr * clntMacAddr = NULL;
      #ifdef MAUME_DEBUG 
        Serial.println(" -> Fetching MAC");
      #endif
      clntMacAddr = MauMe.getClientMAC(request->client()->remoteIP());
      if(clntMacAddr != NULL){
        #ifdef MAUME_DEBUG 
          Serial.println(" -> Found client address.");
        #endif
        #ifdef MAUME_CHECK_ADMIN  // #endif
          ADDRESS macFrom = MauMe.addressFromBytes(MauMe.getSha256BytesOfBytes((char*)clntMacAddr->addr, (const size_t)6));
        #endif
        #ifdef MAUME_DEBUG 
          Serial.println(" -> Getting simulation page");
        #endif // doAlternate   percentActivity  alternateInterval PRCT_ACTIVITY INTER_ACTIVITY  actAltAct deActAltAct
      #ifdef MAUME_CHECK_ADMIN  // #endif
        ADDRESS adminMac = MauMe.macFromMacStyleString(MAUME_ADMIN_ADDR);//"B0:A2:E7:0E:9E:AF");            
        if(MauMe.compareBuffers((const char*)&macFrom.dat, (const char*)&adminMac.dat, MM_ADDRESS_SIZE)){              
      #endif
        if(request->hasParam("actAltAct")) {
          if(request->hasParam("PRCT_ACTIVITY")){
            #ifdef MAUME_DEBUG  // #endif
              Serial.println(" ----------------------- PRCT_ACTIVITY "+request->getParam("PRCT_ACTIVITY")->value()+" -----");
            #endif
            MauMe.percentActivity = request->getParam("PRCT_ACTIVITY")->value().toInt();
          }else{
            #ifdef MAUME_DEBUG  // #endif
              Serial.println(" ------------------------------------------- NO PRCT_ACTIVITY IN FORM -----");
            #endif
          }
          if(request->hasParam("INTER_ACTIVITY")){
            #ifdef MAUME_DEBUG  // #endif
              Serial.println(" ----------------------- INTER_ACTIVITY "+request->getParam("INTER_ACTIVITY")->value()+" -----");
            #endif
            MauMe.alternateInterval = request->getParam("INTER_ACTIVITY")->value().toInt()*1000;
          }else{
            #ifdef MAUME_DEBUG  // #endif
              Serial.println(" ------------------------------------------- NO INTER_ACTIVITY IN FORM -----");
            #endif
          }
          MauMe.alternateLastActivationTime = millis();
          MauMe.doAlternate = true;
          MauMe.Log("-STRT ALTERNATING: "+String(MauMe.percentActivity)+" % of "+String(MauMe.alternateInterval)+" millis.");
        }else if(request->hasParam("deActAltAct")) {
          MauMe.doAlternate = false;
          MauMe.Log("-STOP ALTERNATING: "+String(MauMe.percentActivity)+" % of "+String(MauMe.alternateInterval)+" millis.");
        }else if(request->hasParam("Reset")) {
          deleteFile(SPIFFS, MauMe.spiMutex, MauMe.logFile.c_str());  
          struct timeval now = { .tv_sec = time(0), .tv_usec = 0  };
          settimeofday(&now, NULL);            
        }else if(request->hasParam("actSimTx")) {
          if(request->hasParam("TRGT_ADDR")){
            #ifdef MAUME_DEBUG  // #endif
              Serial.println(" ----------------------- TRGT_ADDR "+request->getParam("TRGT_ADDR")->value()+" -----");
            #endif
            MauMe.dummyTargetAddress = request->getParam("TRGT_ADDR")->value();
          }else{
            #ifdef MAUME_DEBUG  // #endif
              Serial.println(" ------------------------------------------- NO TRGT_ADDR IN FORM -----");
            #endif
          }
          if(request->hasParam("TX_STATIC_DELAY")){
            #ifdef MAUME_DEBUG  // #endif
              Serial.println(" ----------------------- TX_STATIC_DELAY "+request->getParam("TX_STATIC_DELAY")->value()+" -----");
            #endif
            MauMe.DUMMY_PROCESS_STATIC_DELAY = request->getParam("TX_STATIC_DELAY")->value().toInt()*1000;    
            MauMe.DUMMY_PROCESS_STATIC_DELAY = max(DUMMY_PROCESS_MIN_STATIC_DELAY, MauMe.DUMMY_PROCESS_STATIC_DELAY);        
          }else{
            #ifdef MAUME_DEBUG  // #endif
              Serial.println(" ------------------------------------------- NO TX_STATIC_DELAY IN FORM -----");
            #endif
          }
          if(request->hasParam("TX_RANDOM_DELAY")){
            #ifdef MAUME_DEBUG  // #endif
              Serial.println(" ----------------------- TX_RANDOM_DELAY "+request->getParam("TX_RANDOM_DELAY")->value()+" -----");
            #endif
            MauMe.DUMMY_PROCESS_RANDOM_DELAY = request->getParam("TX_RANDOM_DELAY")->value().toInt()*1000;      
            MauMe.DUMMY_PROCESS_RANDOM_DELAY = max(DUMMY_PROCESS_MIN_RANDOM_DELAY, MauMe.DUMMY_PROCESS_RANDOM_DELAY);      
          }else{
            #ifdef MAUME_DEBUG  // #endif
              Serial.println(" ------------------------------------------- NO TX_RANDOM_DELAY IN FORM -----");
            #endif
          }
          if(request->hasParam("NB_DUM")){
            #ifdef MAUME_DEBUG  // #endif
              Serial.println(" ----------------------- NB_DUM "+request->getParam("NB_DUM")->value()+" -----");
            #endif
            MauMe.nbDummyPkts = request->getParam("NB_DUM")->value().toInt();            
          }else{
            #ifdef MAUME_DEBUG  // #endif
              Serial.println(" ------------------------------------------- NO NB_DUM IN FORM -----");
            #endif
          }
          MauMe.sendDummies = true;
          MauMe.Log("-STRT DUM: "+String(MauMe.nbDummyPkts)+" @ "+String(MauMe.dummyTargetAddress)+".");
          MauMe.dummyLastSendTime = millis();
        }else if(request->hasParam("DeActSimTx")) {
           MauMe.sendDummies = false;
           MauMe.Log("-STOP DUM: "+String(MauMe.nbDummyPkts)+" @ "+String(MauMe.dummyTargetAddress)+".");
        }
        request->send(200, "text/html", "<br><h2> MauMe Node "+MauMe.MyAddress+" - Request executed ! </h2><br><br><a href=\"/simus\">Return to Home Page</a>");
      #ifdef MAUME_CHECK_ADMIN  // #endif
      }else{     
        request->send(MauMe.getWebPage(request, macFrom, (byte)MMM_APPTYP_NOD));
      }
      #endif
      }else{
        Serial.println(" -> Error serving http. ACT SIM ADDR NULL.");
        request->send(200, "text/html", "<br><h2> MauMe Node "+MauMe.MyAddress+" - Cannot identify client. Reconnect !</h2><br><br><a href=\"/\">Return to Mau-Me Page</a>");
      }
    }else{
      Serial.println(" -> Error serving http. ACT SIM NOT SERVING.");
      request->send(200, "text/html", "<br><h2> MauMe Node "+MauMe.MyAddress+" - Out of order.</h2><br><br><a href=\"/\">Return to Mau-Me Page</a>");
    }
  });
  //======================================================================================================
  server->on("/*", HTTP_GET, [](AsyncWebServerRequest *request){
    if (MauMe.doServe && fileExists(SPIFFS, MauMe.spiMutex, request->url().c_str())) {
      String url = String(request->url());
      String contentType = MauMe.getContentType(url);
      request->send(SPIFFS, url, contentType);
    }else{
        Serial.println(" -> Error serving http. NOT SERVING RESSOURCE :" + String(request->url()) +".");
        request->send(200, "text/html", "<br><h2> MauMe Node "+MauMe.MyAddress+" - Out of order.</h2><br><br><a href=\"/\">Return to Mau-Me Page</a>");
      }
  });
  //======================================================================================================
  server->onNotFound(MauMe.notFound);
  //======================================================================================================
  #ifdef MAUME_DEBUG 
    Serial.println(" -> Starting server");
  #endif
  server->begin();
  #ifdef MAUME_DEBUG 
    Serial.println(" -> Web server set up.");
  #endif
}
//--------------------------------------------------------------------------------------------------------------
int MauMeClass::deleteOlderFiles(const char * dir, int overNumber){
  #ifdef MAUME_DEBUG 
    Serial.println(" * Entering files cleaning");
  #endif
  NamesList * namesList = NULL;
  namesList = getDirList(SPIFFS, MauMe.spiMutex, dir);
  if(namesList){
    int extra = namesList->nbFiles - overNumber;
    if(extra > 0){
      #ifdef MAUME_DEBUG 
        Serial.println(" -> "+String(extra)+" extra file(s). Sorting by date...");
      #endif
      sortFileList(namesList);
      for (int i = 0; i < namesList->nbFiles; i++){
        if(i<extra){
          deleteFile(SPIFFS, MauMe.spiMutex, namesList->fileNames[i]);    
        }
        char * truc = namesList->fileNames[i];
        free(truc);
        namesList->fileNames[i] = NULL;
      }
      free(namesList->fileNames);
      namesList->fileNames = NULL;
      free(namesList->lwTimes);
      namesList->lwTimes = NULL;
      namesList->nbFiles = 0;
      free(namesList);
      namesList = NULL;
      #ifdef MAUME_DEBUG 
        Serial.println(" -> Done.");
      #endif
      return extra;
    }else{
      #ifdef MAUME_DEBUG 
        Serial.println(" <> No extra file(s).");
      #endif
    }
    for (int i = 0; i < namesList->nbFiles; i++){
      free(namesList->fileNames[i]);
      namesList->fileNames[i] = NULL;
    }
    if(namesList->fileNames){
      free(namesList->fileNames);
      namesList->fileNames = NULL;
    }
    if(namesList->lwTimes){
      free(namesList->lwTimes);
      namesList->lwTimes = NULL;
    }    
    namesList->nbFiles = 0;
    if(namesList){
      free(namesList);
      namesList = NULL;
    }
  }else{Serial.println(" #< Could not get file list.");}
  return 0;
}
//------------------------------------------------------------------------------------------------------------DUMMY_PROCESS_MIN_ADRESS_SIZE  && dummyTargetAddress.length() == 0) ||
void MauMeClass::MauMeTask(void * pvParameters) {
  while(true){
    if(MauMe.doRun){
      if (MauMe.sendDummies && (MauMe.nbDummyPkts > 0) && (MauMe.DUMMY_PROCESS_STATIC_DELAY >= DUMMY_PROCESS_MIN_STATIC_DELAY) && ((millis() - MauMe.dummyLastSendTime) > MauMe.dummySendInterval)) {
        if(MauMe.nbDummyPkts == 1){
          MauMe.sendDummies = false;
        }
        Serial.println("~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~\n    INJECTING DUMMY PACKET :");
        LoRa_PKT * dummyLoRaPkt = MauMe.createDummyLoRaPacket("This is dummy message number "+String(MauMe.nbDummyPkts)+".");
        xSemaphoreTakeRecursive(MauMe.pcktListSemaphore, portMAX_DELAY);
          dummyLoRaPkt->next = (LoRa_PKT *)MauMe.receivedPackets;
          MauMe.receivedPackets = dummyLoRaPkt;
        xSemaphoreGiveRecursive(MauMe.pcktListSemaphore);
        Serial.println("\n~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~");      // +",PW"+String(dummyLoRaPkt->TX_POW)+",TX"+String(dummyLoRaPkt->NB_TX)
        MauMe.Log("-INJCT DUMY PKT: "+String(dummyLoRaPkt->CRC32)+"(Typ"+String(dummyLoRaPkt->PCKTTYP)+","+String(dummyLoRaPkt->NB_MSG)+"MSG,"+String(dummyLoRaPkt->NB_ACK)+"ACK)");
        MauMe.nbDummyPkts -=1;        
        MauMe.dummySendInterval = random(MauMe.DUMMY_PROCESS_STATIC_DELAY, MauMe.DUMMY_PROCESS_STATIC_DELAY+MauMe.DUMMY_PROCESS_RANDOM_DELAY);     // 2-3 secondscleanPackets()  
        MauMe.dummyLastSendTime = millis();            // timestamp the message
      }
      //==========================================================================================================
      if (millis() - MauMe.receivedPacketsLastSaveTime > MauMe.MM_PCKT_SAVE_DELAY) {
        #ifdef MAUME_DEBUG 
          Serial.println("_ _ _ _ _ _ _ _ _ _ _ _ _ _ _ _ _ _ _ _ _ _ SAVE DATA LOOP _ _ _ _ _ _ _ _ _ _ _ _ _ _ _ _ _ _ _ _ _ _ _ _ _ _ _ _ ");
          Serial.println("                                     Time : "+MauMe.getTimeString(time(NULL)));
        #endif
        MauMe.nbReceivedPkts += MauMe.saveReceivedPackets();
        #ifdef MAUME_DEBUG 
          if(MauMe.nbReceivedPkts > 0){Serial.println(" * Received data saved.");}
          Serial.println("_ _ _ _ _ _ _ _ _ _ _ _ _ _ _ _ _ _ _ _ _ _ _ _ _ _ _ _ _ _ _ _ _ _ _ _ _ _ _ _ _ _ _ _ _ _ _ _ _ _ _ _ _ _ _ _ _ ");
        #endif      
        //---------------------------------------------------------------------------------------------------
        int ind = 0;
        MauMe.writeMem(ind++, MauMe.DUMMY_PROCESS_STATIC_DELAY);  
        MauMe.writeMem(ind++, MauMe.DUMMY_PROCESS_RANDOM_DELAY);
        MauMe.writeMem(ind++, MauMe.MM_PCKT_PROCESS_STATIC_DELAY);
        MauMe.writeMem(ind++, MauMe.MM_PCKT_PROCESS_RANDOM_DELAY);
        MauMe.writeMem(ind++, MauMe.MM_TX_POWER_MIN);
        MauMe.writeMem(ind++, MauMe.MM_TX_POWER_MAX);
        MauMe.writeMem(ind++, MauMe.MM_PCKT_SAVE_DELAY);
        MauMe.writeMem(ind++, (int)MauMe.doAlternate);
        MauMe.writeMem(ind++, MauMe.percentActivity);
        MauMe.writeMem(ind++, MauMe.alternateInterval);
        MauMe.writeMem(ind++, MauMe.dummySendInterval);
        MauMe.writeMem(ind++, MauMe.nbDummyPkts);
        MauMe.writeMem(ind++, (int)MauMe.sendDummies); 
        MauMe.writeString2Mem(ind, MauMe.dummyTargetAddress, MM_ADDRESS_SIZE);
        // Serial.println(" Wrote str : " + MauMe.dummyTargetAddress);
        MauMe.receivedPacketsLastSaveTime = millis();            // timestamp the message
      }
      //==========================================================================================================
      if (millis() - MauMe.packetsRoutineLastProcessTime > MauMe.packetsRoutineProcessInterval) {
        #ifdef MAUME_DEBUG 
          Serial.println("_ _ _ _ _ _ _ _ _ _ _ _ _ _ _ _ _ _ _ _ _ _ PROCESS LOOP _ _ _ _ _ _ _ _ _ _ _ _ _ _ _ _ _ _ _ _ _ _ _ _ _ _ _ _ ");
          if(0){
            listDir(SPIFFS, MauMe.spiMutex, "/PKTS",      0);
            listDir(SPIFFS, MauMe.spiMutex, "/ACKS2SEND", 0);
            listDir(SPIFFS, MauMe.spiMutex, "/RECACKS",   0);
            if(0){
              listDir(SPIFFS, MauMe.spiMutex, "/ACKSSENT", 0);
              listDir(SPIFFS, MauMe.spiMutex, "/NODEPKTS", 0);
              listDir(SPIFFS, MauMe.spiMutex, "/NODEACKS", 0);
              listDir(SPIFFS, MauMe.spiMutex, "/RECMES",   0);
            }
          }else{
            //listDir(SPIFFS, MauMe.spiMutex, "/", 0);
          }
        #endif
        #ifdef MAUME_DEBUG 
          int nbCleanPkts = MauMe.cleanPacketsVsACKs();
          if(nbCleanPkts > 0){Serial.println(" * Cleaned "+String(nbCleanPkts)+" packets files.");}
            Serial.println(" * Delayed packet transmit by : "+String((millis() - MauMe.packetsRoutineLastProcessTime)/1000)+" seconds.");
        #else
          MauMe.cleanPacketsVsACKs();
        #endif
        #ifdef MAUME_DEBUG 
          int nbPacktsSent = MauMe.processPackets();      
          if(nbPacktsSent > 0){Serial.println(" -> Sent "+String(nbPacktsSent)+" packets.");}
        #else
          MauMe.processPackets();
        #endif
               
        // ---------------------------------------- CALLING USER CALBBACK IF ANY ----------------------------
        if(MauMe.nbReceivedPkts > 0){
          if(MauMe._onDelivery){
            MauMe._onDelivery(MauMe.nbReceivedPkts);
          }
          MauMe.nbReceivedPkts = 0;
        }
        // --------------------------------------------------------------------------------------------------
        int delNb = MauMe.deleteOlderFiles("/ACKSSENT", MM_MAX_ACKSENT_PKTS);
        #ifdef MAUME_DEBUG 
          Serial.println(" -> Deleted "+String(delNb)+" overnumbered ACKSSENT files.");
        #endif
        delNb = MauMe.deleteOlderFiles("/RECMES", MM_MAX_ACKSENT_PKTS);
        #ifdef MAUME_DEBUG 
          Serial.println(" -> Deleted "+String(delNb)+" overnumbered RECMES files.");
        #endif
        delNb = MauMe.deleteOlderFiles("/PKTS", MM_MAX_PKTS);
        #ifdef MAUME_DEBUG 
          Serial.println(" -> Deleted "+String(delNb)+" overnumbered PKTS files.");
        #endif
        delNb = MauMe.deleteOlderFiles("/NODEPKTS", MM_MAX_NODE_PKTS);
        #ifdef MAUME_DEBUG 
          Serial.println(" -> Deleted "+String(delNb)+" overnumbered NODEPKTS files.");
        #endif
        delNb = MauMe.deleteOlderFiles("/NODEACKS", MM_MAX_NODE_PKTS);
        #ifdef MAUME_DEBUG 
          Serial.println(" -> Deleted "+String(delNb)+" overnumbered NODEACKS files.");
          Serial.println("_ _ _ _ _ _ _ _ _ _ _ _ _ _ _ _ _ _ _ _ _ _ _ _ _ _ _ _ _ _ _ _ _ _ _ _ _ _ _ _ _ _ _ _ _ _ _ _ _ _ _ _ _ _ _ _ _ ");
          MauMe.printWifiClients();
        #endif
        MauMe.packetsRoutineProcessInterval = random(MauMe.MM_PCKT_PROCESS_STATIC_DELAY, MauMe.MM_PCKT_PROCESS_STATIC_DELAY+MauMe.MM_PCKT_PROCESS_RANDOM_DELAY);     // 2-3 secondscleanPackets()  
        MauMe.packetsRoutineLastProcessTime = millis();
        // LoRa_JMM.dumpRegisters(Serial);
      }
    }else{
      delay(250);
    }
    // alternateLastActivationTime  doAlternate   percentActivity  alternateInterval
    if(MauMe.doAlternate){
      time_t t = millis();
      time_t progress = t%MauMe.alternateInterval;
      if(progress <= MauMe.percentActivity*MauMe.alternateInterval/100){
        MauMe.alternateLastActivationTime = t;
        if(MauMe.doRun == false){
          Serial.println("\n\n_ _ _ _ _ _ _ _ _ _ _ _ _ _ _ _ _ _ _ Progress = "+String(progress)+".\n\n");
          MauMe.Log(" --- ACTIVATING ---"); 
          Serial.println("_ _ _ _ _ _ _ _ _ _ _ _ _ _ _ _ _ _ _ MAUME ACTIVATED _ _ _ _ _ _ _ _ _ _ _ _ _ _ _ _ _ _ _ _ _ _ _ _ _ ");
        }
        MauMe.doRun = true;
      }else{
        if(MauMe.doRun == true){
          Serial.println("\n\n_ _ _ _ _ _ _ _ _ _ _ _ _ _ _ _ _ _ _ Progress = "+String(progress)+".\n\n");
          MauMe.Log(" --- SUSPENDING ---"); 
          Serial.println("_ _ _ _ _ _ _ _ _ _ _ _ _ _ _ _ _ _ _ MAUME SUSPENDED _ _ _ _ _ _ _ _ _ _ _ _ _ _ _ _ _ _ _ _ _ _ _ _ _ ");
        }
        MauMe.doRun = false;                
      }
    }else{
      MauMe.doRun = true;
    }
    /*if(MauMe.runDNS){
      MauMe.dnsServer->processNextRequest();    
    }*/
  }  
}
//----------------------------------------------------------------- FUNCTIONS -----------------------------------------------
void MauMeClass::setup(long serialSpeed) {
  Serial.begin(serialSpeed);
  EEPROM.begin(EEPROM_SIZE);    
  while (!Serial); //if just the the basic function, must connect to a computer
  delay(100);
  Serial.println("\n MauMe Node setup in progress...");
  #ifdef MAUME_DEBUG
    randomSeed(analogRead(0));
    float flashChipSize = (float)ESP.getFlashChipSize() / 1024.0 / 1024.0;
    float flashFreq = (float)ESP.getFlashChipSpeed() / 1000.0 / 1000.0;
    FlashMode_t ideMode = ESP.getFlashChipMode();
    Serial.println("\n\n\n\n\n\n######################################################################################################"); 
    Serial.println("######################################################################################################"); 
    Serial.println("\nMauMe Node Setup - UPF 2020\n"); 
    Serial.println("==========================================================");
    Serial.println(" Firmware: ");
    Serial.print(  "  SDK version: "); Serial.println(ESP.getSdkVersion());
    Serial.printf( "__________________________\n");
    Serial.println(" Flash chip information: ");
    Serial.printf( "  Sketch thinks Flash RAM is size: "); Serial.print(flashChipSize); Serial.println(" MB");
    Serial.print(  "  Flash frequency                : "); Serial.print(flashFreq); Serial.println(" MHz");
    Serial.printf( "  Flash write mode               : %s\n", (ideMode == FM_QIO ? "QIO" : ideMode == FM_QOUT ? "QOUT" : ideMode == FM_DIO ? "DIO" : ideMode == FM_DOUT ? "DOUT" : "UNKNOWN"));
    Serial.printf( "  CPU frequency                  : %u MHz\n", ESP.getCpuFreqMHz());
    Serial.printf( "__________________________\n\n");
  #endif
  //-----------------------------------------------------------------------  
  MauMe.pcktListSemaphore = xSemaphoreCreateRecursiveMutex();
  MauMe.spiMutex = xSemaphoreCreateRecursiveMutex();
  #ifdef MAUME_DEBUG 
    if(MauMe.pcktListSemaphore != NULL){
      Serial.println(" -> Created semaphore");        
    }else{Serial.println(" #< Failed to create Semaphore !");}
  #endif
  //----------------------------------------------
  MauMe.myMacAddress = String(WiFi.macAddress());
  MauMe.myByteAddress = MauMe.macFromMacStyleString(MauMe.myMacAddress);
  byte* truc = MauMe.getSha256BytesOfBytes((char*)MauMe.myByteAddress.dat, (const size_t)6);
  MauMe.MyAddress = MauMe.addressBuffer2MacFormat((char*)truc, MM_ADDRESS_SIZE);
  free(truc);
  MauMe.myByteAddress =  MauMe.addressFromMacStyleString(MauMe.MyAddress);
  MauMe.logFile = "/"+MauMe.myMacAddress+".log";
  #ifdef MAUME_DEBUG 
    Serial.println(" -> LOCAL ADDRESS : "+MauMe.MyAddress);  
  #endif
  #ifdef MAUME_ACTIVATE_WIFI
    MauMe.activate_Wifi();
    delay(1000);
  #endif
  #ifdef MAUME_SETUP_DNS_SERVER   // #endif
    MauMe.activate_DNS();
    delay(1000);
  #endif
  //----------------------------------------------
  #ifdef MAUME_SETUP_WEB_SERVER
    MauMe.setupWebServer();
    delay(1000);
  #endif
  //---------------------------------------------
  MauMe.activate_LoRa();
  delay(1000);
  //----------------------------------------------  
  if(!SPIFFS.begin(true)){
    Serial.println(" #< SPIFFS Start Failed !");
    while(true);
    return;
  }
  delay(1000);  
  //------------------------------------------------------------------------------------------------------
  if(0){   //  Use this if you want to programmatically erase all files at startup.
    int dels = deleteDirectoryFiles(SPIFFS, MauMe.spiMutex, "/NODEPKTS");
    dels += deleteDirectoryFiles(SPIFFS, MauMe.spiMutex,    "/NODEACKS");
    dels += deleteDirectoryFiles(SPIFFS, MauMe.spiMutex,    "/RECMES");
    dels += deleteDirectoryFiles(SPIFFS, MauMe.spiMutex,    "/ACKSSENT");
    dels += deleteDirectoryFiles(SPIFFS, MauMe.spiMutex,    "/PKTS");
    dels += deleteDirectoryFiles(SPIFFS, MauMe.spiMutex,    "/ACKS2SEND");
    dels += deleteDirectoryFiles(SPIFFS, MauMe.spiMutex,    "/RECACKS");
    #ifdef MAUME_DEBUG  // #endif
      Serial.println(" -> Deleted files ok : "+String(dels)+".");
    #endif
    #ifdef MAUME_DEBUG 
      listDir(SPIFFS, MauMe.spiMutex, "/", 0);
      Serial.println(" -> Deleting log file...");
    #endif
  }
  deleteFile(SPIFFS, MauMe.spiMutex, MauMe.logFile.c_str());
  int ind = 0;
  int first = MauMe.readMem(ind++);
  if(first>0){
    MauMe.DUMMY_PROCESS_STATIC_DELAY = first;  
    MauMe.DUMMY_PROCESS_RANDOM_DELAY = MauMe.readMem(ind++);
    MauMe.MM_PCKT_PROCESS_STATIC_DELAY = MauMe.readMem(ind++);
    MauMe.MM_PCKT_PROCESS_RANDOM_DELAY = MauMe.readMem(ind++);
    MauMe.MM_TX_POWER_MIN = MauMe.readMem(ind++);
    MauMe.MM_TX_POWER_MAX = MauMe.readMem(ind++);
    MauMe.MM_PCKT_SAVE_DELAY = MauMe.readMem(ind++);
    MauMe.doAlternate = (bool)MauMe.readMem(ind++);
    MauMe.percentActivity = MauMe.readMem(ind++);
    MauMe.alternateInterval = MauMe.readMem(ind++);
    MauMe.dummySendInterval = MauMe.readMem(ind++);
    MauMe.nbDummyPkts = MauMe.readMem(ind++);
    MauMe.sendDummies = (bool)MauMe.readMem(ind++);       
    MauMe.dummyTargetAddress = MauMe.readMem2String(ind, MM_ADDRESS_SIZE);
    //Serial.println(" Read str : " + str);
  }
  //---------------------------------------------------------------
  LoRa_JMM.receive();  
  // alternateLastActivationTime  doAlternate   percentActivity  alternateInterval
  MauMe.alternateLastActivationTime = millis();
  MauMe.dummyLastSendTime = millis(); 
  MauMe.packetsRoutineLastProcessTime = millis(); 
  #ifdef MAUME_DEBUG 
    Serial.println(" -> Creating MauMe task.");
  #endif
  xTaskCreatePinnedToCore(
                    MauMe.MauMeTask,
                    "MauMeTask",
                    10000,
                    NULL,
                    1,
                    &MauMe.mauMeTask,
                    1);
  delay(250); 
  #ifdef MAUME_DEBUG   // #endif
    Serial.println(" -> MauMe Task set up!\n");    
  #endif
  Serial.println("\n MauMe Node setup done @ "+MauMe.MyAddress+".");
  MauMe.Log(" * MauMe Node setup done @ "+MauMe.MyAddress+" ("+MauMe.myMacAddress+").");   

  
  /*MauMe.writeMem(0, 6358975);  MauMe.writeMem(1, 2);  MauMe.writeMem(2, 3);  MauMe.writeMem(3, 4);

  Serial.println(" Memory : "+String(MauMe.readMem(0))+","+String(MauMe.readMem(1))+","+String(MauMe.readMem(2))+","+String(MauMe.readMem(3))+".");
  */
}
//-------------------------------------------------------------------------
MauMeClass MauMe;
//-------------------------------------------------------------------------
