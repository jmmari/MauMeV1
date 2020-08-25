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
//--------------------------------------------------------------------------------------------------------------------------------------------------------
MauMeClass::MauMeClass(){
  MauMe.DUMMY_PROCESS_STATIC_DELAY = max(DUMMY_PROCESS_MIN_STATIC_DELAY, MauMe.DUMMY_PROCESS_STATIC_DELAY);
}

//--------------------------------------------------------------------------------------------------------------------------------------------------------
bool  MauMeClass::writeMem(int index, int number){
  bool ret = false;
  EEPROM.writeInt(index * sizeof(int), number);
  while(!xSemaphoreTakeRecursive(MauMe.spiMutex, portMAX_DELAY));
    ret = EEPROM.commit();
  xSemaphoreGiveRecursive(MauMe.spiMutex);
  return ret;
}

//--------------------------------------------------------------------------------------------------------------------------------------------------------
int  MauMeClass::readMem(int index){
  int nb = -1;
  nb = (int)EEPROM.readInt(index * sizeof(int));    
  return nb;
}

//--------------------------------------------------------------------------------------------------------------------------------------------------------
void  MauMeClass::writeString2Mem(int index, String str, int len){
  ADDRESS addr = MauMe.addressFromMacStyleString(str);
  while(len){
    EEPROM.writeChar(index* sizeof(int) + len - 1, addr.dat[len-1]);
    len -= 1;
  } 
  while(!xSemaphoreTakeRecursive(MauMe.spiMutex, portMAX_DELAY));
    EEPROM.commit(); 
  xSemaphoreGiveRecursive(MauMe.spiMutex);  
}

//--------------------------------------------------------------------------------------------------------------------------------------------------------
String  MauMeClass::readMem2String(int index, int len){
  char * buf = (char*)calloc(len, sizeof(char));
  int ln = len;
  while(len){
    buf[len-1] = EEPROM.readChar(index* sizeof(int) + len - 1);
    len -= 1;
  }
  String res = MauMe.addressBuffer2MacFormat((char*)buf, ln);
  free(buf);
  return res;
}

//---------------------------------------------------------------------------------------------------------------------------------------------------------
String  MauMeClass::getTimeString(time_t now){
  char dat[10];
  int hours   = (int)(now/3600);
  int minutes = (int)((now-hours*3600)/60);
  int seconds = (int)(now-hours*3600 - minutes*60);
  sprintf(dat, "%02d:%02d:%02d", hours, minutes, seconds);
  return String(dat);
}

//---------------------------------------------------------------------------------------------------------------------------------------------------------
int  MauMeClass::Log(String str){
  #ifdef MAUME_DEBUG // #endif
    Serial.println(" >> Appending : "+str+"<br/>\n");
  #endif
  int ret = 0;
  ret = appendFile(SPIFFS, MauMe.spiMutex, MauMe.logFile, MauMe.getTimeString(time(NULL))+str+"<br/>\n");
  return ret; // appendFile(SPIFFS, MauMe.spiMutex, (const char*)(MauMe.logFile.c_str()), str);
}

//--------------------------------------------------------------------------------------------------------------------------------------------------------
byte*  MauMeClass::getSha256BytesOfBytes(char* payload, const size_t payloadLength){
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

//--------------------------------------------------------------------------------------------------------------------------------------------------------
byte*  MauMeClass::getSha256Bytes(String payload){
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

//--------------------------------------------------------------------------------------------------------------------------------------------------------
int  MauMeClass::countReceivedMessages(){
  int cnt = 0;
  File root = fileOpen(SPIFFS, MauMe.spiMutex, "/INBOX");
  if(!root){Serial.println(" ---------------------------------------------------- Failed to open directory");return 0;}
  if(!root.isDirectory()){Serial.println(" -------------------------------------------- Not a directory");return 0;}
  File file = getNextFile(MauMe.spiMutex, root);
  while (file) {
    MM_PKT * pkt = MauMe.load_PKT(String(file.name()));
    if(pkt != NULL){ // addressesMatch  // if(MauMe.compareBuffers((const char*)&MauMe.myByteAddress, (const char*)&pkt->HDR.DEST, MM_ADDRESS_SIZE)){
     if(MauMe.addressesMatch(MauMe.myByteAddress, pkt->HDR.DEST)){
      cnt++;
     }
    }
    file = getNextFile(MauMe.spiMutex, root);
  }
  root.close();
  file.close();
  return cnt;
}

//--------------------------------------------------------------------------------------------------------------------------------------------------------
int  MauMeClass::countNodeTotalInboxMessages(){
  return MauMe.countfiles("/INBOX");
}

//--------------------------------------------------------------------------------------------------------------------------------------------------------
void MauMeClass::onDelivery(void(*callback)(int)){
  MauMe._onDelivery = callback;  
}

//--------------------------------------------------------------------------------------------------------------------------------------------------------
ADDRESS MauMeClass::zeroAddress(ADDRESS address){
  int i = 0;
  for(i=0;i<MM_ADDRESS_SIZE;i++){
    address.dat[i] = 0;
  }
  return address;
}

//--------------------------------------------------------------------------------------------------------------------------------------------------------
ADDRESS MauMeClass::getZeroAddress(){
  ADDRESS address = {0};
  address = MauMe.zeroAddress(address);
  return address;
}

//--------------------------------------------------------------------------------------------------------------------------------------------------------
String  MauMeClass::getContentType(String filename) {
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

//--------------------------------------------------------------------------------------------------------------------------------------------------------
uint32_t  MauMeClass::getCRC32FromChars(char * str, int len){
  /*Serial.println("-----------------------------------------\n Calculating CRC32 for : "+String(len)+" bytes:");
  Serial.write((const uint8_t*)str, len);
  Serial.println("\n-----------------------------------------\n");*/
  return crc32_le(MauMe.crcState, (const uint8_t*)str, len);
}

//--------------------------------------------------------------------------------------------------------------------------------------------------------
bool IRAM_ATTR MauMeClass::compareBuffers(const char* buf1,const char* buf2, int len){
  while(len){
    if(buf1[len-1] != buf2[len-1]){return false;}
    len -= 1;
  }
  return true;
}

//--------------------------------------------------------------------------------------------------------------------------------------------------------
char  MauMeClass::hexStr2Num(char str[]){
  return (char) strtol(str, 0, 16);
}

//--------------------------------------------------------------------------------------------------------------------------------------------------------
String  MauMeClass::Mac2Str(char* buff){
  char dat[24] = {'\0'};
  sprintf(dat, "%02x:%02x:%02x:%02x:%02x:%02x", buff[0], buff[1], buff[2], buff[3], buff[4], buff[5]);
  return String(dat);
}

//--------------------------------------------------------------------------------------------------------------------------------------------------------
String  MauMeClass::addressBuffer2Str(char* buff, int len){
  char dat[(len+1)*2+1] = {'\0'};
  int u = 0;
  for(u=0; u<(len+1)*2+1;u++){dat[u] = 0;}
  int i = 0;
  for(i=0; i<len;i++){sprintf(dat+i*2, "%02x", buff[i]);} // Serial.printf(" Conv : %d.\n", buff[i]);
  dat[i*2] = '\0';
  return String(dat);
}

//--------------------------------------------------------------------------------------------------------------------------------------------------------
String  IRAM_ATTR MauMeClass::addressBuffer2MacFormat(char* buff, int len){
  // Serial.println(" GETTING STRING FROM BUFFER : "+String(buff));
  char dat[(len+1)*3+1] = {'\0'};
  int u = 0;
  for(u=0; u<(len+1)*3+1;u++){dat[u] = 0;}
  int i = 0;
  for(i=0; i<len;i++){sprintf(dat+i*3, "%02x:", buff[i]);}
  // sprintf(dat+i*3, "%02x", buff[i]);
  dat[i*3-1] = '\0';
  // sprintf(dat+i*3, "%02x", buff[i]);
  String ret = String(dat);
  ret.trim();
  return ret;
}

//--------------------------------------------------------------------------------------------------------------------------------------------------------
ADDRESS  MauMeClass::addressFromBytes(byte * str){
  ADDRESS mac = getZeroAddress();
  memcpy(&mac.dat[0], str, MM_ADDRESS_SIZE);
  return mac;
}

//------------------------xSemaphoreGiveRecursive(MauMe.spiMutex);  --------------------------------------------------------------------------------------
ADDRESS  MauMeClass::macFromMacStyleString(String str){
  ADDRESS mac = getZeroAddress();
  const char s[2] = ":";
  char * token;
  token = strtok((char*)str.c_str(), (const char*)s);
  int i = 0;
  while( token != NULL and i<MM_ADDRESS_SIZE) {
    mac.dat[i] = MauMe.hexStr2Num(token);
    token = strtok(NULL, (const char*)s);
    i++;
  }
  return mac;
}

//--------------------------------------------------------------------------------------------------------------------------------------------------------
ADDRESS  MauMeClass::addressFromMacStyleString(String str){
  ADDRESS mac = getZeroAddress();
  const char s[2] = ":";
  char * token;
  token = strtok((char*)str.c_str(), (const char*)s);
  int i = 0;
  while( token != NULL and i<MM_ADDRESS_SIZE) {
    mac.dat[i] = MauMe.hexStr2Num(token);
    token = strtok(NULL, (const char*)s);
    i++;
  }
  return mac;
}

//--------------------------------------------------------------------------------------------------------------------------------------------------------
String  MauMeClass::recPkt2Html(MM_PKT * mmPkt){
  String outStr = "Void";
  if(mmPkt != NULL){
    outStr = "";
    uint32_t crc32 = MauMe.get_PKT_CRC32(mmPkt);
    String sender = String(""+MauMe.addressBuffer2MacFormat(mmPkt->HDR.SEND.dat, MM_ADDRESS_SIZE)); // <br/>
    // String sender = MauMe.getShortAddress(mmPkt->HDR.SEND, MM_ADDR_DISP_SHRT);
    sender.toUpperCase(); // <b>PKT "+String(crc32)+", 
    outStr = "<h7>From</b>: "+sender+", <b>NHP</b> "+String(mmPkt->HDR.NHP)+"</h7><h6><b>Text: </b><span>"+String((char*)&mmPkt->LOAD[0])+"</span><br/>__________________________________________</h6>";
  }else{Serial.println(" #< ! NULL packet !");}
  return outStr;
}

//--------------------------------------------------------------------------------------------------------------------------------------------------------
String  MauMeClass::pktParams2Html(MM_PKT * mmPkt){
  String outStr = "Void";
  if(mmPkt != NULL){
    outStr = ""; 
    uint32_t crc32 = MauMe.get_PKT_CRC32(mmPkt);
    String sender = MauMe.getShortAddress(mmPkt->HDR.SEND, MM_ADDR_DISP_SHRT);
    sender.toUpperCase();
    String recipient = String(MauMe.getShortAddress(mmPkt->HDR.DEST, MM_ADDR_DISP_SHRT)); // <br/>
    recipient.toUpperCase();
    outStr =  "<h6><b>PKT "+String(crc32)+"</b>, <b>From</b>..:<span>"+sender+",<b>To</b>.."+recipient+", <b>NHP</b> "+String(mmPkt->HDR.NHP)+".</span></h6>";
  }else{Serial.println(" #< ! NULL packet !");}
  return outStr;
}

//--------------------------------------------------------------------------------------------------------------------------------------------------------
String  MauMeClass::ackParams2Html(MM_ACK * ack){
  String outStr = "Void";
  if(ack != NULL){
    outStr = "";
    uint32_t crc32 = MauMe.get_ACK_CRC32(ack);
    String sender =    String(MauMe.getShortAddress(ack->SEND, MM_ADDR_DISP_SHRT));
    sender.toUpperCase();
    String recipient = String(MauMe.getShortAddress(ack->DEST, MM_ADDR_DISP_SHRT));
    recipient.toUpperCase();
    outStr =  "<h6><span><b> " +MauMe.getTypeString(ack->TYPE)+" "+String(crc32)+" for "+String(ack->HASH)+"</b>, HTL "+String(ack->HTL)+"</span><br/>";
    outStr += "<span><b>From</b>.."+sender+",<b>To..</b>"+recipient+"</span></h6>";
  }else{Serial.println(" #< ! NULL ACK !");}
  return outStr;
}

//--------------------------------------------------------------------------------------------------------------------------------------------------------
String  MauMeClass::pkt2Html(MM_PKT * packet){
  String outStr = "Void";
  if(packet != NULL){
    outStr = "<span><b>To  </b>: "+MauMe.addressBuffer2MacFormat(packet->HDR.DEST.dat, MM_ADDRESS_SIZE)+"<br/>&#13;<b>Text: </b>"+String((char*)&packet->LOAD[0])+"</span>";
  }else{Serial.println(" #< ! NULL packet !");}
  return outStr;
}
//--------------------------------------------------------------------------------------------------------------------------------------------------------
String  MauMeClass::pkt2Str(MM_PKT * packet){
  if(packet != NULL){
    uint32_t newCRC32 = MauMe.get_PKT_CRC32(packet);
    String outStr = "MM PKT (NHP "+String(packet->HDR.NHP)+", CRC32:"+String(newCRC32)+", SIZE:"+String(packet->HDR.LOADSIZE)+"):\nFrom: "+MauMe.addressBuffer2MacFormat(packet->HDR.SEND.dat, MM_ADDRESS_SIZE)+"\nTo  : "+MauMe.addressBuffer2MacFormat(packet->HDR.DEST.dat, MM_ADDRESS_SIZE)+"\nMess: "+String((char*)&packet->LOAD[0])+"";
    return outStr;
  }else{Serial.println(" #< ! NULL packet !");}
  return " VOID OR WRONG PACKET ";
}

//--------------------------------------------------------------------------------------------------------------------------------------------------------
PKT_LINK *  MauMeClass::freeAndGetNext(PKT_LINK * list){
  PKT_LINK * next = list->next;
  free(list);
  return next;
}

//--------------------------------------------------------------------------------------------------------------------------------------------------------
PKT_LINK * IRAM_ATTR MauMeClass::getReceivedPackets(){ 
  String path = "";
  PKT_LINK * headPkt = (PKT_LINK *)calloc(1, sizeof(PKT_LINK));
  headPkt->next = NULL;
  PKT_LINK * sweepLinkPacket = headPkt;
  File root = fileOpen(SPIFFS, MauMe.spiMutex, "/INBOX");
  if(!root){Serial.println(" ---------------------------------------------------- Failed to open directory");free(headPkt);return NULL;}
  if(!root.isDirectory()){Serial.println(" -------------------------------------------- Not a directory");free(headPkt);return NULL;}
  File file = getNextFile(MauMe.spiMutex, root);
  while (file) {
    if(!String(file.name()).endsWith(".cts")){
      MM_PKT * pkt = MauMe.load_PKT(String(file.name()));
      if(pkt != NULL){ // addressesMatch if(MauMe.compareBuffers((const char*)&MauMe.myByteAddress, (const char*)&pkt->HDR.DEST, MM_ADDRESS_SIZE)){
        if(MauMe.addressesMatch(MauMe.myByteAddress, pkt->HDR.DEST)){
          sweepLinkPacket->next = (PKT_LINK *)calloc(1, sizeof(PKT_LINK));
          if(sweepLinkPacket->next){
            sweepLinkPacket->next->PKT = pkt;
            sweepLinkPacket->next->next = NULL;
            sweepLinkPacket = sweepLinkPacket->next;        
          }
        }
      }
    }
    file = getNextFile(MauMe.spiMutex, root);
  }
  root.close();
  file.close();
  sweepLinkPacket = headPkt->next;free(headPkt);headPkt=NULL;
  return sweepLinkPacket;
}

//--------------------------------------------------------------------------------------------------------------------------------------------------------
String  MauMeClass::listReceivedACKs2Html(fs::FS &fs, SemaphoreHandle_t mutex, const char * dirname){
  String str = "";
  File root = fileOpen(fs, mutex, dirname);
  if(!root){Serial.println(" ---------------------------------------------------- Failed to open directory");return "";}
  if(!root.isDirectory()){Serial.println(" -------------------------------------------- Not a directory");return "";}
  File file = getNextFile(mutex, root);
  if(file){
    while (file) {
      if(!String(file.name()).endsWith(".cts")){
        MM_ACK * ack = NULL;
        ack = MauMe.load_ACK(String(file.name()));
        if(ack != NULL){
          str +=  MauMe.ackParams2Html(ack);
          // str += "<br/>";            
          free(ack);ack = NULL;        
        }
      }
      file = getNextFile(mutex, root);
    }
  }else{str += "------------- NONE -------------<br/>\r\n";}
  root.close();
  file.close();
  return str;
}

//--------------------------------------------------------------------------------------------------------------------------------------------------------
String  MauMeClass::listReceivedPackets2Html(fs::FS &fs, SemaphoreHandle_t mutex, const char * dirname){
  String str = "";
  File root = fileOpen(fs, mutex, dirname);
  if(!root){Serial.println(" ---------------------------------------------------- Failed to open directory");return "";}
  if(!root.isDirectory()){Serial.println(" -------------------------------------------- Not a directory");return "";}
  File file = getNextFile(mutex, root);
  if(file){
    while (file) {
      if(!String(file.name()).endsWith(".cts")){
        MM_PKT * packet = NULL;
        packet = MauMe.load_PKT(String(file.name()));
        if(packet != NULL){
          str +=  MauMe.pktParams2Html(packet);
          //str += "<br/>";
          free(packet);packet = NULL;        
        }
      }
      file = getNextFile(mutex, root);
    }
  }else{str += "------------- NONE -------------<br/>\r\n";}
  root.close();
  file.close();
  return str;
}

//--------------------------------------------------------------------------------------------------------------------------------------------------------
String  MauMeClass::listReceivedMessages2Html(fs::FS &fs, SemaphoreHandle_t mutex, const char * dirname, ADDRESS macFrom){
  String str = "<h3>&#160;&#160;Received SMS&#160;:</h3>";
  File root = fileOpen(fs, mutex, dirname);
  if(!root){Serial.println(" ---------------------------------------------------- Failed to open directory");return "";}
  if(!root.isDirectory()){Serial.println(" -------------------------------------------- Not a directory");return "";}
  File file = getNextFile(mutex, root);
  if(file){
    while (file) {
      if(!String(file.name()).endsWith(".cts")){
        MM_PKT * packet = NULL;
        packet = MauMe.load_PKT(String(file.name()));
        if(packet != NULL){
          switch(packet->HDR.TYPE){
            case MM_TYPE_PKT:{ // MauMe.addressesMatch(MauMe.myByteAddress, packet->HDR.SEND) <br/>&#13; <br/>&#13;
              if(MauMe.addressesMatch(MauMe.myByteAddress, packet->HDR.DEST) || MauMe.addressesMatch(macFrom, packet->HDR.DEST)){
                //if(MauMe.compareBuffers((const char*)&MauMe.myByteAddress, (const char*)&packet->HDR.DEST, MM_ADDRESS_SIZE) || MauMe.compareBuffers((const char*)&macFrom, (const char*)&packet->HDR.DEST, MM_ADDRESS_SIZE)){
                str +=  MauMe.recPkt2Html(packet);
                //str += "<h6>__________________________________________</h6>";
              }
              free(packet);packet = NULL;
            }break;
            default:{
              #ifdef MAUME_DEBUG // #endif
                Serial.println(" ! Unknown Packet Format.");
              #endif
            }break;
          }
        }
      }
      file = getNextFile(mutex, root);
    }
  }else{str += "------------- NONE -------------<br/>\r\n";}
  root.close();
  file.close();
  return str;
}

//--------------------------------------------------------------------------------------------------------------------------------------------------------
String  MauMeClass::listSent2Str(fs::FS &fs, SemaphoreHandle_t mutex, const char * dirname, ADDRESS macFrom){
  String str = "<h3>&#160;&#160;Sent SMS&#160;:</h3>";
  File root = fileOpen(fs, mutex, dirname);
  if(!root){Serial.println(" #< Failed to open directory");return "";}
  if(!root.isDirectory()){Serial.println(" #< Not a directory");return "";}
  File file = getNextFile(mutex, root);
  if(file){
    while (file) {
      if(!String(file.name()).endsWith(".cts")){
        MM_PKT * packet = NULL;
        packet = MauMe.load_PKT(String(file.name()));
        if(packet != NULL){
          switch(packet->HDR.TYPE){
            //--------------------------------------------------------------------------------------------------------------------------------------------------------- addressesMatch(mmAck->SEND, list->LoRaPKT.HDR.SenderNode)){ //
            case MM_TYPE_PKT:{            // Check if message is for wifi client 'macFrom' of for this node:
              if(MauMe.addressesMatch(MauMe.myByteAddress, packet->HDR.SEND) || MauMe.addressesMatch(macFrom, packet->HDR.SEND)){
                // if(MauMe.compareBuffers((const char*)&MauMe.myByteAddress, (const char*)&packet->HDR.SEND, MM_ADDRESS_SIZE) || MauMe.compareBuffers((const char*)&macFrom, (const char*)&packet->HDR.SEND, MM_ADDRESS_SIZE)){
                str +=  MauMe.pkt2Html(packet);
                String ackRecName = file.name();
                ackRecName.replace(dirname, "/INRCAS");
                String ackHopName = file.name();
                ackHopName.replace(dirname, "/PKTSOUT");
                bool isHop = !fileExists(SPIFFS, mutex, ackHopName.c_str());
                bool isAck = fileExists(SPIFFS, mutex, ackRecName.c_str());
                str += "<br/>&#13;__________________ ";
                if(isHop or isAck){str += " Sent";}
                if(isAck){str += " & Received.";}else{str += "...";}
                str += "<br/>&#13;";
              }
              free(packet);packet = NULL;
            }break;
            //---------------------------------------------------------------------------------------------------------------------------------------------------------
            default:{
              #ifdef MAUME_DEBUG // #endif
                Serial.println(" ! Unknown Packet Format.");
              #endif
            }break;
          }
        }
      }
      file = getNextFile(mutex, root);
    } 
  }else{str += "------------- NONE -------------<br/>\r\n";}
  root.close();
  file.close();
  return str;
}

//--------------------------------------------------------------------------------------------------------------------------------------------------------
uint32_t MauMeClass::getCRC32(String str){
  return crc32_le(MauMe.crcState, (const uint8_t*)str.c_str(), str.length());
}

//--------------------------------------------------------------------------------------------------------------------------------------------------------
int MauMeClass::save_PKT(const char * dirname, String fileName, MM_PKT * pktIn){
  String path = String(dirname)+fileName;
  #ifdef MAUME_DEBUG // #endif
        Serial.println(" save_PKT : Saving : " + path+".");
  #endif
  int ret = 0;
  if(pktIn != NULL){
    MM_PKT * pktOut = (MM_PKT *)calloc(1, sizeof(MM_PKT));
    if(pktOut){
      memcpy((char*)pktOut, (char*)pktIn, sizeof(MM_PKT_HDR) + pktIn->HDR.LOADSIZE);
      ret = writeBytesToFile(SPIFFS, MauMe.spiMutex, (char*)path.c_str(), (byte *)pktOut, sizeof(MM_PKT));   
      if(!ret){
        Serial.println(" -< Write failed.");
      }
      free(pktOut);pktOut=NULL;
    }
  }else{Serial.println("\n #< Packet NULL !\n");}
  return ret;
}

//--------------------------------------------------------------------------------------------------------------------------------------------------------
MM_PKT * IRAM_ATTR MauMeClass::load_PKT(String path){ // MM_ACK * load_ACK(String path)
  MM_PKT * packet = NULL;
  File file = fileOpenFor(SPIFFS, MauMe.spiMutex, path.c_str(), FILE_READ); 
  if(file){
    packet = (MM_PKT *)calloc(1, sizeof(MM_PKT));
    for(int i=0; i<sizeof(MM_PKT); i++){((char*)packet)[i]=0;}
    size_t nbRd = readSomeBytes(MauMe.spiMutex, file, (char*)packet, (size_t)sizeof(MM_PKT));
    if(nbRd <= 0){free(packet);packet = NULL;}
    file.close();
  }
  return packet;
}

//--------------------------------------------------------------------------------------------------------------------------------------------------------
int MauMeClass::save_ACK_AS(const char * dirname, String fileName, MM_ACK * ack){
  String path = String(dirname)+fileName;
  #ifdef MAUME_DEBUG // #endif
        Serial.println(" -> save_ACK_AS : Saving : " + path+".");
  #endif
  int ret = 0;
  if(ack != NULL){
    ret = writeBytesToFile(SPIFFS, MauMe.spiMutex, (char*)path.c_str(), (byte *)ack, sizeof(MM_ACK));    
  }else{Serial.println(" #< Ack NULL !");}
  return ret;
}

//--------------------------------------------------------------------------------------------------------------------------------------------------------
int MauMeClass::save_ACK(const char * dirname, MM_ACK * mmAck, bool overWrite){ // "/PKTSOUT/"
  String path = String(dirname)+String(mmAck->HASH)+".bin";
  int ret = 0;
  bool already = false;
  if(!overWrite){already = fileExists(SPIFFS, MauMe.spiMutex, path.c_str());}
  if(mmAck != NULL && (overWrite || !already)){
    ret = writeBytesToFile(SPIFFS, MauMe.spiMutex, (char*)path.c_str(), (byte *)mmAck, sizeof(MM_ACK));
    if(ret <= 0){
      Serial.println(" #< ACK Write failed");
    }
  }else{
    #ifdef MAUME_DEBUG // #endif
      Serial.println(" #< ACK NULL or file already exists ! ("+path+")");
    #endif
  }    
  return ret;
}

//--------------------------------------------------------------------------------------------------------------------------------------------------------
int MauMeClass::save_ACK_AS_SELF(const char * dirname, MM_ACK * mmAck, bool overWrite){ // "/PKTSOUT/"
  String fileName = String(MauMe.get_ACK_CRC32(mmAck));
  String path = String(dirname)+fileName+".bin";
  bool already = false;
  int ret = 0;
  if(!overWrite){already = fileExists(SPIFFS, MauMe.spiMutex, path.c_str());}
  if(mmAck != NULL && (overWrite || !already)){
    ret = writeBytesToFile(SPIFFS, MauMe.spiMutex, (char*)path.c_str(), (byte *)mmAck, sizeof(MM_ACK));
    if(ret <= 0){
      Serial.println(" #< ACK Write failed");
    }
  }else{
    #ifdef MAUME_DEBUG // #endif
      Serial.println(" #< ACK NULL or file already exists ! ("+path+")");
    #endif
  }    
  return ret;
}

//--------------------------------------------------------------------------------------------------------------------------------------------------------
int MauMeClass::save_SHRC_ACK(const char * dirname, MM_ACK * mmAck, bool overWrite){ // "/PKTSOUT/"
  mmAck->TYPE = MM_TYPE_REC;
  String fileName = String(MauMe.get_ACK_CRC32(mmAck)); // String(mmAck->HASH); //
  mmAck->TYPE = MM_TYPE_SHRC;
  String path = String(dirname)+fileName+".bin";
  bool already = false;
  int ret = 0;
  if(!overWrite){already = fileExists(SPIFFS, MauMe.spiMutex, path.c_str());}
  if(mmAck != NULL && (overWrite || !already)){
    ret = writeBytesToFile(SPIFFS, MauMe.spiMutex, (char*)path.c_str(), (byte *)mmAck, sizeof(MM_ACK));
    if(ret <= 0){
      Serial.println(" #< ACK Write failed");
    }
  }else{
    #ifdef MAUME_DEBUG // #endif
      Serial.println(" #< ACK NULL or file already exists ! ("+path+")");
    #endif
  }    
  return ret;
}

//--------------------------------------------------------------------------------------------------------------------------------------------------------
MM_ACK * MauMeClass::load_ACK(String path){
  MM_ACK * mmAckt = NULL;
  File file = fileOpenFor(SPIFFS, MauMe.spiMutex, path.c_str(), FILE_READ); 
  if(file){
    mmAckt = (MM_ACK *)calloc(1, sizeof(MM_ACK));
    size_t nbRd = readSomeBytes(MauMe.spiMutex, file, (char *)mmAckt, (size_t)sizeof(MM_ACK));
    if(nbRd <= 0){
      Serial.println(" #<                                                               ERROR !");
      free(mmAckt);mmAckt = NULL;
    }
    file.close();
  }
  return mmAckt;
}

//--------------------------------------------------------------------------------------------------------------------------------------------------------
uint32_t MauMeClass::get_LoRa_PKT_CRC32(MM_LoRa_PKT * loRaPKT){
  //return MauMe.getCRC32FromChars((char*)mlpkt + sizeof(char), sizeof(MM_PKT_HDR) + mmpkt->HDR.LOADSIZE - sizeof(char));
  /*#ifdef MAUME_DEBUG // #endif
    Serial.println(" -> Computing PKT CRC32 on DATA (display includes CRC32 field):"); // getLoRaPacketSize
    Serial.write((const uint8_t*)loRaPKT, (int)(sizeof(MM_LoRa_PKT_HDR) + min((unsigned int)loRaPKT->HDR.PKTSIZE, (unsigned int)MAX_MM_LoRa_SIZE)));
    Serial.println(".\n-----------------------------------------\n");
  #endif */
  return MauMe.getCRC32FromChars(((char*)loRaPKT)+sizeof(uint32_t), sizeof(MM_LoRa_PKT_HDR) + loRaPKT->HDR.PKTSIZE - sizeof(uint32_t));
}

//--------------------------------------------------------------------------------------------------------------------------------------------------------
uint32_t MauMeClass::get_PKT_CRC32(MM_PKT *mmpkt){
  return MauMe.getCRC32FromChars((char*)mmpkt + sizeof(char), sizeof(MM_PKT_HDR) + mmpkt->HDR.LOADSIZE - sizeof(char));
}

//--------------------------------------------------------------------------------------------------------------------------------------------------------
uint32_t MauMeClass::get_ACK_CRC32(MM_ACK * mmAck){
  return MauMe.getCRC32FromChars((char*)mmAck + sizeof(char), sizeof(MM_ACK) - sizeof(char));
}

//--------------------------------------------------------------------------------------------------------------------------------------------------------
MM_PKT *  MauMeClass::createPKT(ADDRESS addressFrom, ADDRESS addressTo, String message){
  MM_PKT * mmm = NULL;
  if(message.length() > 0 && message.length() <= MAX_MM_PKT_SIZE){
    mmm = (MM_PKT *)calloc(1, sizeof(MM_PKT));
    mmm->HDR.NHP = (unsigned char)(0);
    mmm->HDR.TYPE = MM_TYPE_PKT;
    mmm->HDR.SEND = addressFrom;
    mmm->HDR.DEST = addressTo;
    mmm->HDR.LOADSIZE = (unsigned char)(message.length());
    memcpy(&mmm->LOAD[0], message.c_str(), mmm->HDR.LOADSIZE);  
    #ifdef MAUME_DEBUG // #endif
      Serial.println("________________________\nPacket created:\n"+MauMe.pkt2Str(mmm)+"\n________________________");
      /*Serial.println("-----------------------------------------\n CREATED PKT DAT : "+String(MauMe.getPacketSize(mmm))+" bytes:");
      Serial.write((const uint8_t*)mmm, MauMe.getPacketSize(mmm));
      Serial.println("\n-----------------------------------------\n");*/
    #endif
  }
  return mmm;
}

//-------------------------------------------------------------------------------------------------------------------------------------------------------- 
MM_PKT * MauMeClass::createDummyPKT(String text){
  ADDRESS addressTo = MauMe.getZeroAddress();    
  addressTo = MauMe.macFromMacStyleString(MauMe.dummyTargetAddress);  
  return MauMe.createPKT(MauMe.myByteAddress, addressTo, text);
}

//--------------------------------------------------------------------------------------------------------------------------------------------------------
int MauMeClass::sendLoRaBuffer(char* outgoing, int len) {
  while(!xSemaphoreTakeRecursive(MauMe.spiMutex, portMAX_DELAY));
    LoRa_JMM.beginPacket();                           
    LoRa_JMM.write((const uint8_t*)outgoing, len);   
    int ret = LoRa_JMM.endPacket();
  xSemaphoreGiveRecursive(MauMe.spiMutex);
  return ret;
}

//--------------------------------------------------------------------------------------------------------------------------------------------------------
String MauMeClass::getLoRaPktType(unsigned char type){
  switch(type){
    case MM_PKT_TYPE_UKN:
      return "PKT_TYPE_UKN";
    case MM_PKT_TYPE_GEN:
      return "PKT_TYPE_GEN";
    case MM_PKT_TYPE_MM:
      return "PKT_TYPE_MM";
    default:
      return "UNDEFINED PKT TYPE"; // MM_PKT_TYPE_PILEACK
  }
}

//--------------------------------------------------------------------------------------------------------------------------------------------------------
String MauMeClass::getType(unsigned char type){
  switch(type){
    case MM_TYPE_UKN:
      return "MM_TYPE_UKN";
    case MM_TYPE_PKT:
      return "MM_TYPE_PKT";
    case MM_TYPE_REC:
      return "MM_TYPE_REC";
    case MM_TYPE_SHRC:
      return "MM_TYPE_SHRC";
    case MM_TYPE_SHA:
      return "MM_TYPE_SHA";      
    case MM_TYPE_REM:
      return "MM_TYPE_REM";
    default:
      return "UNDEFINED TYPE";
  }
}

//--------------------------------------------------------------------------------------------------------------------------------------------------------
String MauMeClass::getTypeString(unsigned char type){
  switch(type){
    case MM_TYPE_UKN:
      return "UKNOWN";
    case MM_TYPE_PKT:
      return "PACKET";
    case MM_TYPE_REC:
      return "RECACK";
    case MM_TYPE_SHRC:
      return "SHRCACK";
    case MM_TYPE_SHA:
      return "SHACK";      
    case MM_TYPE_REM:
      return "REM-ACK";
    default:
      return "UNDEFINED";
  }
}
//-------------------------------------------------------------------------------------------------------------------------------------------------------- 
MM_ACK * MauMeClass::make_ACK(unsigned char htl, unsigned char type, uint32_t hash, ADDRESS SenderNode, ADDRESS destNode){
  MM_ACK * mmAck = (MM_ACK *)calloc(1, sizeof(MM_ACK));
  mmAck->HTL = htl;
  mmAck->TYPE = type;
  mmAck->SEND = SenderNode;
  mmAck->DEST = destNode;
  mmAck->HASH = hash;
  return mmAck;
}

//-------------------------------------------------------------------------------------------------------------------------------------------------------- 
String MauMeClass::ack2Str(MM_ACK * mmAck){  // "): FROM: "+String(MauMe.addressBuffer2MacFormat(mmAck->ORIG, MM_ADDRESS_SIZE))+
  uint32_t crc32 = MauMe.get_ACK_CRC32(mmAck); // getCRC32FromChars((char*)&mmAck + sizeof(char), sizeof(MM_ACK) - sizeof(char));
  String outStr = "MauMeACK (HTL "+String(mmAck->HTL)+", CRC32:"+String(crc32)+", TYPE:"+String(mmAck->TYPE)+"): HASH: "+String(mmAck->HASH)+"";
  return outStr;
}

//--------------------------------------------------------------------------------------------------------------------------------------------------------
Data_PKT *  IRAM_ATTR MauMeClass::getCurrentLoRaPkt(){
  char dat[256] = {0};
  int pos = 0;
  int rssi = 0;
  float snr = 0;
  while(!xSemaphoreTakeRecursive(MauMe.spiMutex, portMAX_DELAY));      // xSemaphoreGiveRecursive(MauMe.spiMutex);
    int avail = LoRa_JMM.available();
    while(avail){
      size_t rd = LoRa_JMM.readBytes((uint8_t *)&dat[pos], avail);
      pos += rd;
      delay(LORA_READ_DELAY);
      avail = LoRa_JMM.available();
    }
    rssi = LoRa_JMM.packetRssi();
    snr = LoRa_JMM.packetSnr();
  xSemaphoreGiveRecursive(MauMe.spiMutex);
  
  Data_PKT * dataPkt = NULL;
  pos = min(pos, (int)sizeof(MM_LoRa_PKT));
  if(pos > 0){
    #ifdef MAUME_DEBUG // #endif
      Serial.println(" -------------------- ! Received "+String(pos)+" bytes ! -> Packet RSSI : "+String(LoRa_JMM.packetRssi())); // ↑  ↓ ⇧⇩ ⇊  ⇑ ⇓ ⇫ ⇯ ⟰ ⟱
    #endif
    dataPkt = (Data_PKT *)calloc(1, sizeof(Data_PKT));
    if(dataPkt){
      memcpy(&dataPkt->LoRaPKT, &dat[0], pos);
      dataPkt->PKTTYP = MM_PKT_TYPE_UKN;
      dataPkt->PKTSIZ = pos;
      dataPkt->NB_MSG = 0;
      dataPkt->NB_ACK = 0;
      dataPkt->TX_POW = MauMe.MM_TX_POWER_MIN;
      dataPkt->NB_TX  = 0;
      dataPkt->RSSI   = rssi; // packetSnr
      dataPkt->SNR    = snr; // packetSnr
      dataPkt->FERR   = 0; // LoRa_JMM.packetFrequencyError();
      dataPkt->SenderNode = dataPkt->LoRaPKT.HDR.SenderNode;
    }else{
      #ifdef MAUME_DEBUG // #endif
        Serial.println(" ! Allocation ERROR !");
      #endif
    }
  }else{
    #ifdef MAUME_DEBUG // #endif
      Serial.println(" ! Receive ERROR !");
    #endif
  }
  for(int i=0; i<256;i++){dat[i] = 0;}
  return dataPkt;
}

//--------------------------------------------------------------------------------------------------------------------------------------------------------
bool MauMeClass::addPacket(MM_PKT * packet){
  #ifdef MAUME_DEBUG // #endif
    Serial.println(" -> Recording packet to FS...");
  #endif
  uint32_t pktCRC32 = MauMe.get_PKT_CRC32(packet); 
  if(MauMe.save_PKT("/OUTBOX/", String(pktCRC32)+".bin", packet)){            // to remember the packet is from here        
    #ifdef MAUME_DEBUG // #endif
      Serial.println(" -> Packet recorded to OUTBOX.");                  
    #endif
    if(MauMe.save_PKT("/PKTSOUT/", String(pktCRC32)+".bin", packet)){        // to enlist packet for transmit 
      #ifdef MAUME_DEBUG // #endif
        Serial.println(" -> Packet recorded to PKTSOUT.");                  
      #endif
      if(MauMe.save_PKT("/PKTSREM/", String(pktCRC32)+".bin", packet)){        // to reject future receptions of our packet            
        #ifdef MAUME_DEBUG // #endif
          Serial.println(" -> Packet recorded to PKTSREM.");                  
        #endif
      }else{
        #ifdef MAUME_DEBUG // #endif
          Serial.println(" #< Failed Recording Packet to PKTSREM ! Packet will auto-collide !!!");
        #endif
        return false;
      }
    }else{
      #ifdef MAUME_DEBUG // #endif
        Serial.println(" #< Failed Recording Packet to PKTSOUT ! Packet won't go !!!");
      #endif
      return false;
    }
  }else{
    #ifdef MAUME_DEBUG // #endif
      Serial.println(" #< Failed Recording Packet to OUTBOX ! Packet won't go !!!");
    #endif
    return false;
  }
  // MauMe.Log("-INJCT PKT: "+String(dataPkt->LoRaPKT.HDR.CRC32)+":"+MauMe.getLoRaPktType(dataPkt->PKTTYP)+"(TYP"+String(dataPkt->PKTTYP)+",PW"+String(dataPkt->TX_POW)+",TX"+String(dataPkt->NB_TX)+","+String(dataPkt->NB_MSG)+"MSG,"+String(dataPkt->NB_ACK)+"ACK)");
  return true;
}

//-----------------------------------------------------------------------------------------------------------------------------------------------------
bool MauMeClass::enlistMessageForTransmit(String recipientsMacStyleAddress, String message){
  ADDRESS addressTo = MauMe.addressFromMacStyleString(recipientsMacStyleAddress);
  MM_PKT * mmm = MauMe.createPKT(MauMe.myByteAddress, addressTo, message);
  if(mmm != NULL){
    return MauMe.addPacket(mmm);
  }else{
    return false;
  }
}
//-----------------------------------------------------------------------------------------------------------------------------------------------------
int MauMeClass::countSavedPackets(){
  return MauMe.countfiles("/PKTSOUT");
}
//-----------------------------------------------------------------------------------------------------------------------------------------------------
int MauMeClass::countSavedACKS2SENDs(){
  return MauMe.countfiles("/SHAOUT")+MauMe.countfiles("/RCAOUT");
}
//-----------------------------------------------------------------------------------------------------------------------------------------------------
int MauMeClass::countfiles(String path){
  int nb = 0;
  File root = fileOpen(SPIFFS, MauMe.spiMutex, path.c_str());
  if(!root){Serial.println(" #< Failed to open directory");
  }else{
    File file = getNextFile(MauMe.spiMutex, root);
    while(file){
      if(!String(file.name()).endsWith(".cts")){
        nb++;
      }
      file = getNextFile(MauMe.spiMutex, root);
    }
    root.close();
    file.close();
  }
  return nb;
}
//-----------------------------------------------------------------------------------------------------------------------------------------------------
void MauMeClass::runMauMe(){
  int ind = 0;
  int first = MauMe.readMem(ind++);
  if(first > 0){
    MauMe.DUMMY_PROCESS_STATIC_DELAY   = first;  
    MauMe.DUMMY_PROCESS_RANDOM_DELAY   = MauMe.readMem(ind++);
    MauMe.MM_PCKT_PROCESS_STATIC_DELAY = MauMe.readMem(ind++);
    MauMe.MM_PCKT_PROCESS_RANDOM_DELAY = MauMe.readMem(ind++);
    MauMe.CLEANING_PROCESS_STATIC_DELAY        =   MAUME_CLEAN_RATE*MauMe.MM_PCKT_PROCESS_STATIC_DELAY;  // milliseconds
    MauMe.CLEANING_PROCESS_RANDOM_DELAY        =   MAUME_CLEAN_RATE*MauMe.MM_PCKT_PROCESS_RANDOM_DELAY;  // milliseconds
    MauMe.MM_TX_POWER_MIN     = MauMe.readMem(ind++);
    MauMe.MM_TX_POWER_MAX     = MauMe.readMem(ind++);
    MauMe.MM_PCKT_SAVE_DELAY  = MauMe.readMem(ind++);
    MauMe.doAlternate         = (bool)MauMe.readMem(ind++);
    MauMe.percentActivity     = MauMe.readMem(ind++);
    MauMe.alternateInterval   = MauMe.readMem(ind++);
    MauMe.dummySendInterval   = MauMe.readMem(ind++);
    MauMe.nbDummyPkts         = MauMe.readMem(ind++);
    MauMe.sendDummies         = false; // (bool)
    MauMe.readMem(ind++);       
    MauMe.dummyTargetAddress  = MauMe.readMem2String(ind, MM_ADDRESS_SIZE);
  }
  MauMe.doRun = true;
  delay(250);
  LoRa_JMM.receive();  
  MauMe.MauMeIsRunning = true;
}

//-----------------------------------------------------------------------------------------------------------------------------------------------------
void MauMeClass::sleepMauMe(){
  if(MauMe.doRun){
    LoRa_JMM.sleep();
    MauMe.doRun = false;
    delay(250);
    MauMe.saveReceivedPackets();
  }
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
  //MauMe.receivedPacketsLastSaveTime = millis();            // timestamp the message
  MauMe.MauMeIsRunning = false;
}

//-----------------------------------------------------------------------------------------------------------------------------------------------------
void MauMeClass::serveHttp(){
  MauMe.doServe = true;
}

//-----------------------------------------------------------------------------------------------------------------------------------------------------
void MauMeClass::haltHttp(){
  MauMe.doServe = false;
}

//-----------------------------------------------------------------------------------------------------------------------------------------------------
bool MauMeClass::pileUp(MM_LoRa_PKT * loRaPkt, char * dat, int len){
  if(loRaPkt->HDR.PKTSIZE + len < MAX_MM_LoRa_SIZE){
    memcpy((char*)&loRaPkt->DAT[loRaPkt->HDR.PKTSIZE], dat, len);
    loRaPkt->HDR.PKTSIZE += len;
    return true;
  }else{
    return false;
  }
}

//-----------------------------------------------------------------------------------------------------------------------------------------------------
MM_LoRa_PKT * MauMeClass::getEmptyLoRaPKT(ADDRESS sender){
  MM_LoRa_PKT * loRaPKT = NULL;
  loRaPKT = (MM_LoRa_PKT *)calloc(1, sizeof(MM_LoRa_PKT));
  if(loRaPKT){
    loRaPKT->HDR.SenderNode = sender;     
    loRaPKT->HDR.PKTSIZE = 0;     
    loRaPKT->HDR.CRC32   = 0;
  }
  return loRaPKT;
}
//-----------------------------------------------------------------------------------------------------------------------------------------------------
int MauMeClass::processPackets(){
  #ifdef MAUME_DEBUG // #endif
    Serial.println(" 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0\n     PROCESSING PACKETS : ");
  #endif
  int nbPktsPiledUp = 0;
  int nbAcksPiledUp = 0;
  char shAcksList[10][32] = {'\0'}; // FWRDD
  int nbPackets2Send = MauMe.countfiles("/PKTSOUT");
  int nbShAcks2Send = MauMe.countfiles( "/SHAOUT");
  int nbRecAcks2Send = MauMe.countfiles("/RCAOUT");
  int nbItems2Send = nbShAcks2Send + nbPackets2Send + nbRecAcks2Send;
  if(nbItems2Send > 0){
    NamesList * pktsList = NULL;
    NamesList * rcaList = NULL;
    NamesList * shaList = NULL;
    if(nbPackets2Send > 0){
      pktsList = getDirList(SPIFFS, MauMe.spiMutex, "/PKTSOUT", MM_TYPE_PKT);
    }
    if(nbRecAcks2Send > 0){
      rcaList = getDirList(SPIFFS, MauMe.spiMutex, "/RCAOUT", MM_TYPE_REC);
    }
    if(nbShAcks2Send > 0){
      shaList = getDirList(SPIFFS, MauMe.spiMutex, "/SHAOUT", MM_TYPE_SHA);
    }
    NamesList * tempList = fuseLists(pktsList, rcaList);
    NamesList * itemsList = fuseLists(tempList, shaList);
    if(itemsList != NULL){ // just in case of errors...
      sortFileList(itemsList);
      int count = 0;
      bool didPileUpPKT = true;
      bool didPileUpACK = true;
      MM_LoRa_PKT * loRaPKT = MauMe.getEmptyLoRaPKT(MauMe.myByteAddress);
      if(loRaPKT){
        #ifdef MAUME_DEBUG // #endif
          Serial.println(" -> LoRa packet allocated.");
        #endif
        while((didPileUpPKT or didPileUpACK) and count < itemsList->nbFiles){
          //-------------------------------------------------------------------------------------- PKT
          if(itemsList->types[count] == MM_TYPE_PKT){ // it's a packet
            #ifdef MAUME_DEBUG // #endif
              Serial.println(" --> Loading PACKET n°"+String(nbPktsPiledUp+1)+" : "+String(itemsList->fileNames[count])+", "+getTimeString(itemsList->lwTimes[count])+".");
            #endif
            MM_PKT * pkt = MauMe.load_PKT(itemsList->fileNames[count]);
            if(pkt){
              #ifdef MAUME_DEBUG // #endif
                Serial.println(" ---> Item "+String(count)+" is a PKT.");
              #endif
              #ifdef MAUME_HOP_ON_TX    // #endif
                if(pkt->HDR.NHP < 255){
                  pkt->HDR.NHP++;
                  
                }
              #endif
              didPileUpPKT = MauMe.pileUp(loRaPKT, (char*)pkt, MauMe.getPacketSize(pkt));
              if(didPileUpPKT){
                nbPktsPiledUp++;
                #ifdef MAUME_HOP_ON_TX
                  if(pkt->HDR.NHP < 255){ 
                    if(MauMe.save_PKT("", String(itemsList->fileNames[count]), pkt)){         // Update-overwrite PKT to decrease HTL at each TX.
                      #ifdef MAUME_DEBUG // #endif
                        Serial.println(" --> Updated item "+String(count)+".");                  
                      #endif
                    }else{
                      #ifdef MAUME_DEBUG // #endif
                        Serial.println("  #< Failed Updating item "+String(count)+" !");
                      #endif
                    }
                  }else{ 
                    MauMe.Log(" --> PKT EXPIRED ("+String(itemsList->fileNames[count])+").");
                    deleteFile(SPIFFS, MauMe.spiMutex, itemsList->fileNames[count]);  
                    #ifdef MAUME_DEBUG // #endif
                      Serial.println("   !> PACKET NHP has reached maximum: deleting from 2go list.");
                    #endif
                  }
                #endif
                #ifdef MAUME_DEBUG // #endif
                  Serial.println(" ---> Piled-up PKT "+String(nbPktsPiledUp)+" (Item "+String(nbPktsPiledUp+nbAcksPiledUp)+"/"+String(nbItems2Send)+").");
                  Serial.println(" ---> PKT content : \n"+MauMe.pkt2Str(pkt)+"§");
                #endif
              }else{
                #ifdef MAUME_DEBUG // #endif
                  Serial.println("   !> No more room to pile-up.");
                #endif
              }
              free(pkt); pkt = NULL;
            }else{
              #ifdef MAUME_DEBUG // #endif
                Serial.println("   !> Failed loading PACKET "+String(itemsList->fileNames[count])+".");
              #endif
            }      
          }else{  // it's an ACK
            //-------------------------------------------------------------------------------------- ACK
            #ifdef MAUME_DEBUG // #endif
              Serial.println(" --> Loading REC-ACK n°"+String(nbAcksPiledUp+1)+" : "+String(itemsList->fileNames[count])+", "+getTimeString(itemsList->lwTimes[count])+".");
            #endif
            MM_ACK * ack = MauMe.load_ACK(itemsList->fileNames[count]);
            if(ack){
              #ifdef MAUME_DEBUG // #endif
                Serial.println(" ---> Item "+String(count)+" is an ACK.");
              #endif
              #ifdef MAUME_HOP_ON_TX    // #endif                                   // HTL is decreased at each TX to kill dead-ends.                
                if(ack->HTL > 0){
                  ack->HTL--;
              #endif 
              didPileUpACK = MauMe.pileUp(loRaPKT, (char*)ack, sizeof(MM_ACK));
              if(didPileUpACK){
                nbAcksPiledUp++;
                #ifdef MAUME_HOP_ON_TX    // #endif                                   // HTL is decreased at each TX to kill dead-ends.                
                  if(ack->TYPE != MM_TYPE_SHA and ack->HTL > 0){
                    if(MauMe.save_ACK_AS("", itemsList->fileNames[count], ack)){                    // Update-overwrite RECACK to decrease HTL at each TX.
                      #ifdef MAUME_DEBUG // #endif
                        Serial.println(" --> Updated REC-ACK "+String(nbAcksPiledUp)+".");                  
                      #endif
                    }else{
                      #ifdef MAUME_DEBUG // #endif
                        Serial.println("  #< Failed Updtaing REC-ACK "+String(nbAcksPiledUp)+" !");
                      #endif
                    }
                  }else{
                    if(ack->TYPE != MM_TYPE_SHA){
                      MauMe.Log(" --> RECACK EXPIRED ("+String(itemsList->fileNames[count])+").");
                    }
                    deleteFile(SPIFFS, MauMe.spiMutex, itemsList->fileNames[count]);  
                    #ifdef MAUME_DEBUG // #endif
                      Serial.println("   !> ACK HTL has reached zero: deleting from 2go list.");
                    #endif
                  }
                  #endif 
                  #ifdef MAUME_DEBUG // #endif
                    Serial.println(" ---> ACK "+String(nbAcksPiledUp)+" piled-up ("+String(nbPktsPiledUp+nbAcksPiledUp)+"/"+String(nbItems2Send)+").");
                  #endif
                }else{
                  #ifdef MAUME_DEBUG // #endif
                    Serial.println("   !> No more room to pile-up.");
                  #endif
                }
                free(ack); ack = NULL;
              }else{
                #ifdef MAUME_DEBUG // #endif
                  Serial.println("   !> Failed loading ACK "+String(itemsList->fileNames[count])+".");
                #endif
              }
            }
          }
          count++;
        }
        freeNamesList(itemsList);
        if(nbPktsPiledUp+nbAcksPiledUp > 0){
          loRaPKT->HDR.CRC32 = MauMe.get_LoRa_PKT_CRC32(loRaPKT);
          // ------------------------------------------------------------------------------------ SEND LORA PKT.
          MauMe.MM_TX_POWER += 1;
          if(MauMe.MM_TX_POWER > MauMe.MM_TX_POWER_MAX){
            MauMe.MM_TX_POWER = MauMe.MM_TX_POWER_MIN;
          }
          #ifdef MAUME_DEBUG // #endif
            Serial.println("_________________________________________________________\n ->     LoRa PKT TX:"); 
            Serial.println(" -->              LoRa PKT CRC32 : "+String(loRaPKT->HDR.CRC32)); 
            Serial.println(" -->               Setting power : "+String(MauMe.MM_TX_POWER)); 
            Serial.println(" -->    Sending LoRa packet size : "+String(MauMe.getLoRaPacketSize(loRaPKT))); 
            Serial.println(" --> Sending packet with nb PKTs : "+String(nbPktsPiledUp)); 
            Serial.println(" --> Sending packet with nb ACKs : "+String(nbAcksPiledUp));  
            MauMe.Log("-> Sent "+String(nbPktsPiledUp)+" PKTS and "+String(nbAcksPiledUp)+" ACKs.");    
          #endif
          LoRa_JMM.setTxPower(MauMe.MM_TX_POWER);                                               // Set TX POWER
          /*#ifdef MAUME_DEBUG 
            Serial.println(" -> Sending LoRa data :");
            Serial.write((const uint8_t*)loRaPKT, MauMe.getLoRaPacketSize(loRaPKT));
            Serial.println(".\n-----------------------------------------\n");
          #endif*/
          delay(LORA_READ_DELAY);
          bool ret = MauMe.sendLoRaBuffer((char*)loRaPKT, MauMe.getLoRaPacketSize(loRaPKT));       // SEND PACKET 
          delay(LORA_READ_DELAY);
          LoRa_JMM.receive();                                                                   // Go back into receive mode
          if(ret){
            #ifdef MAUME_DEBUG // #endif
              Serial.println(" --> Sent LoRa packet with CRC32 "+String(loRaPKT->HDR.CRC32)+" (Packet Size = "+String(MauMe.getLoRaPacketSize(loRaPKT))+")(Result: "+String(ret)+")");
            #endif  // FWRDD
            // ------------------------------------------------------------------------------------ DELETE SH-ACKs Piled-Up if SEND SUCCESS.
            MauMe.MM_NB_TXT += 1; 
            MauMe.MM_NB_TXT = (MauMe.MM_NB_TXT > MAX_PACKETS_COUNT)?0:MauMe.MM_NB_TXT;
            MauMe.oledMauMeMessage(" UP "+String(MauMe.MM_NB_TXT)+" DWN "+String(MauMe.MM_NB_RXT), "  TX PKT "+String(loRaPKT->HDR.CRC32)+".", " PW "+String(MauMe.MM_TX_POWER)+", P "+String(nbPktsPiledUp)+", A "+String(nbAcksPiledUp));
          }else{
            #ifdef MAUME_DEBUG // #endif getMMType
              Serial.println(" !!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!\n -> !!! ERROR SENDING LORA PACKET !!!\n!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!");
            #endif
            MauMe.oledMauMeMessage(" UP "+String(MauMe.MM_NB_TXT)+" DWN "+String(MauMe.MM_NB_RXT), " ERR TX "+String(loRaPKT->HDR.CRC32)+".", " PW "+String(MauMe.MM_TX_POWER)+", P "+String(nbPktsPiledUp)+", A "+String(nbAcksPiledUp));
          }
        }else{
          #ifdef MAUME_DEBUG // #endif getMMType
            Serial.println(" !!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!\n -> !!! EMPTY PACKET ! NOT SENDING !!!\n!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!");
          #endif
        }
        // -------------------------------------------------------------------------------------- FREE LORA PKT.
        free(loRaPKT);
        // -----------------------------------------------------------------------------------------------------
      }else{
        #ifdef MAUME_DEBUG // #endif getMMType
          Serial.println(" !!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!\n -> !!! COULD NOT ALLOCATE LORA PKT !!!\n!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!");
        #endif
      }
    }else{
      #ifdef MAUME_DEBUG // #endif
        Serial.println(" <! No packets or ACKs recorded to go. !>");
      #endif
    }
    #ifdef MAUME_DEBUG // #endif 
      Serial.println(" -> TX process finished !");
    #endif  
    }
  return nbPktsPiledUp+nbAcksPiledUp;
}
//-----------------------------------------------------------------------------------------------------------------------------------------------------
bool MauMeClass::isAtDestination(char *buf){
  bool arrived = false;
  ADDRESS_LIST macAddressList = MauMe.getClientsList();
  for(int i=0; i<macAddressList.nb; i++){
    if(MauMe.compareBuffers((const char*)buf, (const char*)&(macAddressList.list[i]->dat[0]), MM_ADDRESS_SIZE)){
      arrived = true;
      break;
    }
  }
  if(!arrived && MauMe.compareBuffers((const char*)buf, (const char*)MauMe.myByteAddress.dat, MM_ADDRESS_SIZE)){
    arrived = true;
  }
  MauMe.freeClientsList(macAddressList);
  return arrived;
}
//-----------------------------------------------------------------------------------------------------------------------------------------------------
int MauMeClass::cleanPacketsVsACKs(){
  #ifdef MAUME_DEBUG // #endif
    Serial.println(" ...................................................................... \n CLEANING PACKETS : ");
  #endif
  int nb = 0;
  File root = fileOpen(SPIFFS, MauMe.spiMutex, "/PKTSOUT");
  if(!root){
    #ifdef MAUME_DEBUG // #endif
      Serial.println(" #< Failed to open /PKTSOUT directory\n...................................");
    #endif
  }else{
    #ifdef MAUME_DEBUG // #endif
      Serial.println(" -> Opened /PKTSOUT directory.");
    #endif
    File file = getNextFile(MauMe.spiMutex, root);
    while (file) {
      if(!String(file.name()).endsWith(".cts")){
        String ackOfPktName = file.name();
        ackOfPktName.replace("PKTSOUT", "SHASIN");
        if(fileExists(SPIFFS, MauMe.spiMutex, ackOfPktName.c_str())){
          #ifdef MAUME_DEBUG // #endif
            Serial.println("¤¤¤¤¤¤¤¤¤¤¤¤¤¤¤¤¤¤¤¤¤¤¤¤¤¤¤¤¤¤¤¤¤¤¤¤¤¤¤¤¤¤¤¤¤¤¤¤¤¤¤¤¤¤¤¤¤¤¤¤¤¤¤¤¤¤¤¤¤¤¤¤¤¤¤¤¤¤¤¤¤¤¤¤¤¤¤¤\n  > Removing packet : "+String(file.name()));
          #endif
          deleteFile(SPIFFS, MauMe.spiMutex, file.name());    
          #ifdef MAUME_DEBUG // #endif
            MauMe.Log(" -> SH-CLN:"+String(file.name())+".");  
          #endif
          nb++;
        }
      }
      file = getNextFile(MauMe.spiMutex, root);
    }
    root.close();
    file.close();
  }
  #ifdef MAUME_DEBUG // #endif
    Serial.println(" ...................................................................... \n CLEANING REC-ACKS : ");
  #endif
  root = fileOpen(SPIFFS, MauMe.spiMutex, "/RCAOUT");
  if(!root){
    #ifdef MAUME_DEBUG // #endif
      Serial.println(" #< Failed to open /RCAOUT directory\n...................................");
    #endif
  }else{
    #ifdef MAUME_DEBUG // #endif
      Serial.println(" -> Opened /RCAOUT directory.");
    #endif
    File file = getNextFile(MauMe.spiMutex, root);
    while (file) {
      if(!String(file.name()).endsWith(".cts")){
        MM_ACK * recAck = MauMe.load_ACK(String(file.name()));
        if(recAck!=NULL){
          String pktName = String("/PKTSOUT/"+String(recAck->HASH)+".bin");
          if(fileExists(SPIFFS, MauMe.spiMutex, pktName.c_str())){
            int ret1 = deleteFile(SPIFFS, MauMe.spiMutex, pktName.c_str());    
            int ret2 = deleteFile(SPIFFS, MauMe.spiMutex, String(file.name()).c_str());    
            #ifdef MAUME_DEBUG // #endif
              Serial.println("--> REC-ACK has 2go packet. PKT and RECACK CANCELLING each-other !!!");
              Serial.println("---> Removing packet : "+pktName);
              if(ret1 and ret2){
                MauMe.Log(" -> PKT & RECACK CANCELLING !");
                MauMe.Log(" --> RMV Packet:"+pktName);
                MauMe.Log(" --> RMV RECACK:"+String(file.name()));
              }else{
                MauMe.Log(" ->  !! PKT&RECACK CANCEL ERROR !!");
                MauMe.Log(" --> Packet:"+pktName+" ("+String(ret1)+")");
                MauMe.Log(" --> RECACK:"+String(file.name())+" ("+String(ret2)+")");
              }
            // #else
                // MauMe.Log(" -> PKT "+String(recAck->HASH)+" & RECACK CANCEL!");
            #endif
            free(recAck);
            nb++;
          }else{
            #ifdef MAUME_DEBUG // #endif
              Serial.println(" ---> No REC-ACK for packet "+String(file.name())+".");
            #endif
            //---------------------------------------------------- SHACK for RECACK ?
            String ackOfAckName = file.name();
            ackOfAckName.replace("RCAOUT", "SHASIN");
            if(fileExists(SPIFFS, MauMe.spiMutex, ackOfAckName.c_str())){
              MauMe.Log(" -> SH-CLN:"+ String(file.name()));
              #ifdef MAUME_DEBUG // #endif
                MauMe.Log(" -> SH-CLN:"+ String(file.name()));
                Serial.println("¤¤¤¤¤¤¤¤¤¤¤¤¤¤¤¤¤¤¤¤¤¤¤¤¤¤¤¤¤¤¤¤¤¤¤¤¤¤¤¤¤¤¤¤¤¤¤¤¤¤¤¤¤¤¤¤¤¤¤¤¤¤¤¤¤¤¤¤¤¤¤¤¤¤¤¤¤¤¤¤¤¤¤¤¤¤¤¤\n  > Removing RECACK : "+String(file.name()));
              #endif
              deleteFile(SPIFFS, MauMe.spiMutex, file.name());      
              nb++;
            }else{
              #ifdef MAUME_DEBUG // #endif
                Serial.println("¤¤¤¤¤¤¤¤¤¤¤¤¤¤¤¤¤¤¤¤¤¤¤¤¤¤¤¤¤¤¤¤¤¤¤¤¤¤¤¤¤¤¤¤¤¤¤¤¤¤¤¤¤¤¤¤¤¤¤¤¤¤¤¤¤¤¤¤¤¤¤¤¤¤¤¤¤¤¤¤¤¤¤¤¤¤¤¤\n -> No SH-ACK for this RECACK. Skipping.");
              #endif
            }
          }
        }else{
          MauMe.Log(" ->  !! ERR LOAD RECACK ("+String(file.name())+")!!");
        }
      }
      file = getNextFile(MauMe.spiMutex, root);
    }
  file.close();
  }
  root.close();  
  #ifdef MAUME_DEBUG // #endif
    Serial.println(" ..................................................................................");
  #endif
  return nb;
}
//-----------------------------------------------------------------------------------------------------------------------------------------------------
int MauMeClass::getPacketSize(MM_PKT * pkt){
  return sizeof(MM_PKT_HDR) + pkt->HDR.LOADSIZE;
}
//-----------------------------------------------------------------------------------------------------------------------------------------------------
int MauMeClass::getLoRaPacketSize(MM_LoRa_PKT * loRaPkt){
  return sizeof(MM_LoRa_PKT_HDR) + loRaPkt->HDR.PKTSIZE;
}
//-----------------------------------------------------------------------------------------------------------------------------------------------------
int MauMeClass::saveReceivedPackets(){
  Data_PKT * list = NULL;
  xSemaphoreTakeRecursive(MauMe.pcktListSemaphore, portMAX_DELAY);
    list = (Data_PKT *)MauMe.receivedPackets;
    MauMe.receivedPackets = NULL;
  xSemaphoreGiveRecursive(MauMe.pcktListSemaphore);  
  int nbPKTsArrived = 0; 
  if(list!=NULL){
    #ifdef MAUME_DEBUG // #endif
      Serial.println(" * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *\nSaving Received packets.");
    #endif
    int cnt = 1;
    while(list != NULL){
      Data_PKT * next = list->next;
      uint32_t newCRC32 = MauMe.get_LoRa_PKT_CRC32(&list->LoRaPKT);
      #ifdef MAUME_DEBUG // #endif
        Serial.println(" -> Packet CRC32: "+String(list->LoRaPKT.HDR.CRC32)+"; Computed CRC32: "+String(newCRC32)+". Difference : "+String(abs(list->LoRaPKT.HDR.CRC32-newCRC32)));
      #endif
      //-----------------------------------------------------------------------------------------------------------------------------------------------------
      if(newCRC32 == list->LoRaPKT.HDR.CRC32){                                                                        // If CRC32s match, it's a MauMe packet.
        String sender = String(MauMe.getShortAddress(list->LoRaPKT.HDR.SenderNode, MM_ADDR_DISP_SHRT));
        sender.toUpperCase(); 
        #ifdef MAUME_DEBUG // #endif
          Serial.println(" -> Received MM LoRa PKT ! Un-piling : ");
          MauMe.Log(" _________________________________________");
          MauMe.Log(" > Analysing LoRa PKT (RSSI "+String(list->RSSI)+")");
          MauMe.Log(" > "+String(list->LoRaPKT.HDR.PKTSIZE)+" bytes from "+sender);
        #endif
        MauMe.Log(" > RSSI "+String(list->RSSI)+", "+String(list->LoRaPKT.HDR.PKTSIZE)+" bytes from "+sender);
        MauMe.MM_NB_RXT += 1;
        MauMe.MM_NB_RXT = (MauMe.MM_NB_RXT > MAX_PACKETS_COUNT)?0:MauMe.MM_NB_RXT;
        MauMe.oledMauMeMessage("RX="+String(MauMe.MM_NB_RXT)+",RSSI "+String(list->RSSI), " >..."+sender, "   ID "+String(list->LoRaPKT.HDR.CRC32));
        list->PKTTYP = MM_PKT_TYPE_MM;      // This is a MauMe style packet, still of un-determined format.
        //*************************************************************************************************************************************************************************               
        int currentPos = 0;
        while(currentPos < list->LoRaPKT.HDR.PKTSIZE and currentPos < MAX_MM_LoRa_SIZE){
          #ifdef MAUME_DEBUG // #endif
            Serial.println(" -> Current position in LoRa packet : "+String(currentPos));
            Serial.println(" -> Data type found                 : "+String((unsigned char)list->LoRaPKT.DAT[currentPos+1]));
            Serial.println(" -> Data type name                  : "+MauMe.getType((unsigned char)list->LoRaPKT.DAT[currentPos+1]));
          #endif
          bool isSHRC = false;
          uint32_t shRecAckCRC32 = 0;
          switch((unsigned char)list->LoRaPKT.DAT[currentPos+1]){
            
            // *****************************************************************************************************************************************
            // ************************************************************************* SHRC-ACK *******************************************************
            // *****************************************************************************************************************************************
            case MM_TYPE_SHRC:{
              isSHRC = true;
              MM_ACK * ack = (MM_ACK *)&(list->LoRaPKT.DAT[currentPos]);
              shRecAckCRC32 = MauMe.get_ACK_CRC32(ack); 
              MauMe.save_ACK("/SHASIN/", ack, false);
              #ifdef MAUME_DEBUG // #endif // getShortAddress(ADDRESS address1, int n)from "+MauMe.getShortAddress(list->LoRaPKT.HDR.SenderNode, MM_ADDR_DISP_SHRT)+"
                Serial.println(" -> Received SH-REC-ACK "+String(shRecAckCRC32)+": saving to SHASIN list to stop further TX of that PKT.");
                MauMe.Log(" -> SHRC "+String(shRecAckCRC32)+" for TRGT "+String(ack->HASH)+".");                     
              #endif
              ack->TYPE = MM_TYPE_REC;      // jump to next switch case : the SHRCACK is turned into RECACK       
            }
            // *****************************************************************************************************************************************
            // ************************************************************************* REC-ACK *******************************************************
            // *****************************************************************************************************************************************
            case MM_TYPE_REC:{
              list->NB_ACK += 1;  
              MM_ACK * ack = (MM_ACK *)&(list->LoRaPKT.DAT[currentPos]);
              uint32_t ackCRC32 = MauMe.get_ACK_CRC32(ack); // getCRC32FromChars((char*)ack + sizeof(unsigned char), sizeof(MM_ACK) - sizeof(unsigned char));
              #ifdef MAUME_HOP_ON_RECEIVE    // #endif 
                if(ack->HTL != 0){
                  ack->HTL--;
                }
              #endif 
              #ifdef MAUME_DEBUG // #endif
                Serial.println(" -> Received REC-ACK "+String(ackCRC32)+"."); // getShortAddress(ADDRESS address1, int n)
                // MauMe.Log("-> REC-ACK "+String(ackCRC32)+" from "+MauMe.getShortAddress(list->LoRaPKT.HDR.SenderNode, MM_ADDR_DISP_SHRT)+" TRGT "+String(ack->HASH)+".");                     
              #endif
              // *****************************************************************************************************************************************
              if(MauMe.isAtDestination(ack->DEST.dat)){                                                      // RECACK HAS REACHED DESTINATION :                  
                //=======================================================================================================================================
                #ifdef MAUME_DEBUG // #endif
                  Serial.println(" --> REC-ACK "+String(ackCRC32) + " is arrived @ DEST.");
                #endif
                if(!fileExists(SPIFFS, MauMe.spiMutex, String("/INRCAS/"+String(ack->HASH)+".bin").c_str())){
                  // MauMe.Log("-> RECV ACK: "+String(ackCRC32)+" for TRGT "+String(ack->HASH)+".");                     
                  #ifdef MAUME_DEBUG // #endif
                    MauMe.Log(" --> REC-ACK: @ DEST.ACK.");                     
                  #endif
                  if(MauMe.save_ACK("/INRCAS/", ack, false)){                    //save_ACK
                    #ifdef MAUME_DEBUG // #endif
                      Serial.println(" --> Saved Arrived RECACK "+String(cnt)+".");                  
                    #endif
                    cnt++;
                  }else{
                    #ifdef MAUME_DEBUG // #endif
                      Serial.println("  #< Failed Saving RECACK !");
                    #endif
                  }
                }else{
                  // MauMe.Log("-RCPY MMM: "+String(ackCRC32)+")");   
                  #ifdef MAUME_DEBUG // #endif
                    Serial.println(" --> RECACK ALREADY RECEIVED ! Not saving ("+String(ackCRC32)+")");
                    MauMe.Log(" --> REC-ACK @ DEST DUP.ACK.");
                  #endif
                }
                // SH-Acknowledge RECACK: the RECACK was for us, so we can stop any other transmissions of it
                #ifdef MAUME_DEBUG // #endif
                  Serial.println(" --> RECACK is @ DEST : Adding SH-ACK anyway (if a node sent it again, it needs to be stopped !).");
                #endif
                MM_ACK * mmAck = NULL;
                if(isSHRC){
                  mmAck = MauMe.make_ACK(1, MM_TYPE_SHA, shRecAckCRC32, MauMe.myByteAddress, list->LoRaPKT.HDR.SenderNode);
                }else{
                  mmAck = MauMe.make_ACK(1, MM_TYPE_SHA, ackCRC32, MauMe.myByteAddress, list->LoRaPKT.HDR.SenderNode);
                }
                if(mmAck != NULL){
                  MauMe.save_ACK_AS_SELF("/SHAOUT/", mmAck, false);
                  free(mmAck);mmAck = NULL;
                }else{Serial.println("        <> Error recording ACK to SHAOUT list !");}        
                // *****************************************************************************************************************************************                   
              }else{                                                                                              // RECACK MUST BE FORWARDED :
                //=============================================================================================================================
                bool alreadyReceived = fileExists(SPIFFS, MauMe.spiMutex, String("/RCAREM/"+String(ackCRC32)+".bin").c_str());
                #ifdef MAUME_DEBUG // #endif
                  Serial.println(" -> Saving received REC-ACK : "+String(ackCRC32));
                  Serial.println(" ->        Received REC-ACK file exists in "+String("/RCAREM/"+String(ackCRC32)+".bin")+": "+String(alreadyReceived));
                #endif 
                if(!alreadyReceived){ // If REC-ACK is not in the Forwarded Messages list: forward
                  if(int(ack->HTL) > int(MM_ACK_THRESHOLD)){       //                                                     
                    if(!MauMe.save_ACK_AS("/RCAOUT/", String(ackCRC32)+".bin", ack)){    
                      MauMe.Log("-> RECACK "+String(ackCRC32)+" ("+String(ack->HASH)+") from "+MauMe.getShortAddress(list->LoRaPKT.HDR.SenderNode, MM_ADDR_DISP_SHRT)+".");               
                      #ifdef MAUME_DEBUG // #endif
                        MauMe.Log(" --> REC-ACK "+String(ackCRC32)+" ("+String(ack->HASH)+"): FAILED SAVING.");
                        Serial.println(" #< Failed Saving RECACK to ACKs to go !");
                      #endif
                    }else{
                      #ifdef MAUME_DEBUG // #endif
                        MauMe.Log(" --> RECACK "+String(ackCRC32)+" ("+String(ack->HASH)+") : ACK&FRWD.");
                      #endif
                    }
                  }else{                   
                    #ifdef MAUME_DEBUG // #endif
                      Serial.println(" #< REC-ACK HTL is below threshold : not forwarding !");
                      MauMe.Log(" --> RECACK "+String(ackCRC32)+" ("+String(ack->HASH)+") expired.ACK."); 
                    #endif                      
                  }
                  
                  // Record RECACK reminder: for target and for RECACK, so that it's easy to find both
                  
                  // REM for TARGET : 
                  MM_ACK * mmRemAck = NULL;
                  mmRemAck = MauMe.make_ACK(ack->HTL, MM_TYPE_REM, ackCRC32, list->LoRaPKT.HDR.SenderNode, MauMe.myByteAddress);
                  if(mmRemAck != NULL){
                    MauMe.save_ACK_AS("/RCAREM/", String(ack->HASH)+".bin", mmRemAck); // named as target
                  }else{Serial.println("        <> Error recording RCACK TARGET REM to RCAREM list !");}
                  
                  // REM for RECACK :
                  if(mmRemAck != NULL){
                    mmRemAck->HASH = ack->HASH;
                    MauMe.save_ACK_AS("/RCAREM/", String(ackCRC32)+".bin", mmRemAck); // named as RECACK
                    free(mmRemAck);mmRemAck = NULL;
                  }else{Serial.println("        <> Error recording RECACK ACK REM to RCAREM list !");}
                  
                  // ACK is saved: acknowledge ?
                  // SH-Acknowledge RECACK:  
                  MM_ACK * mmShAck = NULL;
                  if(isSHRC){
                    mmShAck = MauMe.make_ACK(1, MM_TYPE_SHA, shRecAckCRC32, MauMe.myByteAddress, list->LoRaPKT.HDR.SenderNode);
                  }else{
                    mmShAck = MauMe.make_ACK(1, MM_TYPE_SHA, ackCRC32, MauMe.myByteAddress, list->LoRaPKT.HDR.SenderNode);
                  }
                  if(mmShAck != NULL){
                    MauMe.save_ACK_AS_SELF("/SHAOUT/", mmShAck, false);
                    free(mmShAck);mmShAck = NULL;
                  }else{Serial.println("        <> Error recording ACK to SHAOUT list !");}                           
                }else{
                  #ifdef MAUME_DEBUG // #endif
                    Serial.println(" --> RECACK ALREADY RECEIVED for FRWD : RE-FORWARD or acknowledge only in special cases...");
                    //MauMe.Log("--> REC-ACK : ALRDY FRWDD.");
                  #endif
                  MM_ACK * mmAck = NULL;
                  mmAck = MauMe.load_ACK(String("/RCAREM/"+String(ackCRC32)+".bin"));
                  if(mmAck != NULL){
                    //                            RE-FORWARD ?
                    // Gave up that option for now.
                    //                            Re-acknowledge ?
                    if(MauMe.addressesMatch(mmAck->SEND, list->LoRaPKT.HDR.SenderNode)){ // First sender node has repeated RECACK: acknowledge !
                      #ifdef MAUME_DEBUG 
                        Serial.println(" --> First Sender node has repeated known RECACK: acknowledge !");
                      #endif
                      MM_ACK * mmAck = NULL;
                      if(isSHRC){
                        mmAck = MauMe.make_ACK(1, MM_TYPE_SHA, shRecAckCRC32, MauMe.myByteAddress, list->LoRaPKT.HDR.SenderNode);
                      }else{
                        mmAck = MauMe.make_ACK(1, MM_TYPE_SHA, ackCRC32, MauMe.myByteAddress, list->LoRaPKT.HDR.SenderNode);
                      }
                      if(mmAck != NULL){
                        int ret = MauMe.save_ACK_AS_SELF("/SHAOUT/", mmAck, false);
                        #ifdef MAUME_DEBUG 
                          MauMe.Log("--> RECACK "+String(ackCRC32)+" ("+String(ack->HASH)+"): REPEATED : ACK ("+String(ret)+").");
                        #endif
                        free(mmAck);mmAck = NULL;
                      }else{Serial.println("        <> Error recording ACK to SHAOUT list !");}
                    }else{
                      #ifdef MAUME_DEBUG 
                        Serial.println(" --> RECACK ALREADY FORWARDED received FROM DIFFERENT FIRST NODE ! Dropping and not acknowledging... ("+String(ackCRC32)+")\n****************************************");                      
                        MauMe.Log("--> REC-ACK "+String(ackCRC32)+" ("+String(ack->HASH)+"): DUPLICATE : IGN.");
                      #endif
                      MauMe.Log("--> RECACK "+String(ackCRC32)+" ("+String(ack->HASH)+"): DUP : IGN.");
                    }
                  }else{
                    Serial.println(" --> NO REM FOR RECACK . ("+String(ackCRC32)+")\n****************************************");                      
                    MauMe.Log("--> NO REM @ REC-ACK "+String(ackCRC32)+".");
                  }
                }
              }
              currentPos += sizeof(MM_ACK);
            }break;
            // *****************************************************************************************************************************************
            // **************************************************************** SH-ACK *****************************************************************
            // *****************************************************************************************************************************************
            case MM_TYPE_SHA:{
              // Is SHACK for this node ? If so, record :
              list->NB_ACK += 1;  
              MM_ACK * ack = (MM_ACK *)&(list->LoRaPKT.DAT[currentPos]);
              if(MauMe.addressesMatch(ack->DEST, MauMe.myByteAddress)){ // compareBuffers((const char*)&ack->DEST, (const char*)&MauMe.myByteAddress, MM_ADDRESS_SIZE)){ // SH-ACK is for this node: record !
                MauMe.save_ACK("/SHASIN/", ack, false);
                #ifdef MAUME_DEBUG // getShortAddress(ADDRESS address1, int n)
                  MauMe.Log("-> SH-ACK "+String(MauMe.get_ACK_CRC32(ack))+" from "+MauMe.getShortAddress(list->LoRaPKT.HDR.SenderNode, MM_ADDR_DISP_SHRT)+" for TRGT "+String(ack->HASH)+".");                     
                #endif
              }
              currentPos += sizeof(MM_ACK);
            }break;
            // *****************************************************************************************************************************************
            // **************************************************************** UNKNOWN ****************************************************************
            // *****************************************************************************************************************************************
            case MM_TYPE_UKN:{
              // Unkown type: abort ?
              #ifdef MAUME_DEBUG 
                Serial.println("#########################################\n ### UNKNOW SECTION FORMAT ! ###\n#########################################");
                Serial.println(" -------  NEXTING ON LoRa PKT LIST  --------------");
              #endif
              MauMe.Log("-> UNKWN MM FORMAT !");
              currentPos = MAX_LoRa_PKT_SIZE;         
            }break;
            // *****************************************************************************************************************************************
            // *****************************************************************************************************************************************
            // ******************************************************** PACKETS : ALL OTHER FORMATS ARE TREATED AS PACKETS ON V0 NODES    **************
            // ******************************************************** ALL FURTHER FORMATS MUST COMPLY WITH PACKET FORMAT 2 BE FORWARDED **************
            // *****************************************************************************************************************************************
            default:{ // case MM_TYPE_PKT:{
              #ifdef MAUME_DEBUG // #endif
                Serial.println(" -> Assuming MM PKT...");
                // MauMe.Log("-> Assuming MM PKT...");
              #endif
              list->NB_MSG += 1;
              MM_PKT * pkt = (MM_PKT *)&(list->LoRaPKT.DAT[currentPos]);
              /*#ifdef MAUME_DEBUG // #endif
                Serial.println(" -> MM PKT DATA :");
                Serial.write((const uint8_t*)pkt, (int)(sizeof(MM_PKT_HDR) + pkt->HDR.LOADSIZE));
                Serial.println(".\n-----------------------------------------\n");
                Serial.print(" -> PKT data :");
                Serial.write((const uint8_t*)pkt, MauMe.getPacketSize(pkt));
                Serial.println("§");
                Serial.println(" -> PKT string :\n"+MauMe.pkt2Str(pkt)+".\n");
              #endif */
              uint32_t pktCRC32 = MauMe.get_PKT_CRC32(pkt);
              #ifdef MAUME_HOP_ON_RECEIVE    // #endif 
                if(pkt->HDR.NHP < 255){
                  pkt->HDR.NHP++;
                }
              #endif 
              #ifdef MAUME_DEBUG // #endif   getShortAddress(ADDRESS address1, int n)
                MauMe.Log("-> PKT "+String(pktCRC32)+" from "+MauMe.getShortAddress(list->LoRaPKT.HDR.SenderNode, MM_ADDR_DISP_SHRT)+".");                     
              #endif
              if(MauMe.isAtDestination(pkt->HDR.DEST.dat)){                                                      // PACKET HAS REACHED DESTINATION :                  
                //=======================================================================================================================================
                #ifdef MAUME_DEBUG // #endif
                  Serial.println(" --> PKT arrived @ destination : "+String(pktCRC32));
                  MauMe.Log("--> PKT "+String(pktCRC32)+" @ DEST.");                     
                #endif
                if(!fileExists(SPIFFS, MauMe.spiMutex, String("/INBOX/"+String(pktCRC32)+".bin").c_str())){
                  #ifdef MAUME_DEBUG // #endif
                    MauMe.Log("---> PKT: "+String(pktCRC32)+" NEW.");   
                  #endif
                  if(MauMe.save_PKT("/INBOX/", String(pktCRC32)+".bin", pkt)){     
                    if(MauMe.addressesMatch(MauMe.myByteAddress, pkt->HDR.DEST)){ // Packet is for the node and not a wifi client
                      #ifdef MAUME_DEBUG 
                        Serial.println(" --> PKT is for this node address.");
                      #endif
                      nbPKTsArrived++;
                    }  
                    #ifdef MAUME_DEBUG // #endif
                      Serial.println(" ---> Saved Arrived Message n°"+String(cnt)+". Acknowledge:");                  
                    #endif
                    cnt++;
                    #ifdef MAUME_DEBUG // #endif
                      Serial.println(" ---> Adding RECACK to go.");
                    #endif
                    MM_ACK * mmAck = NULL;                                // REC-ACK acts as a SH-ACK 
                    mmAck = MauMe.make_ACK(254, MM_TYPE_SHRC, pktCRC32, pkt->HDR.DEST, pkt->HDR.SEND);
                    if(mmAck != NULL){
                      MauMe.save_ACK_AS_SELF("/RCAOUT/", mmAck, false);       // SH-RC-Acknowledge
                      mmAck->TYPE = MM_TYPE_REM;
                      /*MM_ACK * mpktRemAck = MauMe.make_ACK(pkt->HDR.NHP, MM_TYPE_REM, pktCRC32, list->LoRaPKT.HDR.SenderNode, MauMe.myByteAddress);
                      if(mpktRemAck!=NULL){
                        MauMe.save_ACK("/PKTSREM/", mpktRemAck, false);
                        free(mpktRemAck);mpktRemAck = NULL;
                      }*/
                      uint32_t shRcAckCRC32 = MauMe.get_ACK_CRC32(mmAck);
                      mmAck->TYPE = MM_TYPE_REC;
                      uint32_t recAckCRC32 = MauMe.get_ACK_CRC32(mmAck);
                      free(mmAck);mmAck = NULL;
                      
                      // Record RECACK reminder: the node won't accept it's own RECACK : 
                      // One reminder for the SHRCACK : 
                      mmAck = MauMe.make_ACK(pkt->HDR.NHP, MM_TYPE_REM, shRcAckCRC32, MauMe.myByteAddress, pkt->HDR.SEND);
                      MauMe.save_ACK("/RCAREM/", mmAck, false);
                      
                      // One reminder for the corresponding RECACK : 
                      mmAck->HASH = recAckCRC32;
                      MauMe.save_ACK("/RCAREM/", mmAck, false);
                      
                      free(mmAck);mmAck = NULL;
                      // }else{Serial.println("        <> Error recording RECACK ACK REM to RCAREM list !");}
                    }else{Serial.println("   #< Failed saving ACK of message : "+String(pktCRC32));}     // PKTSREM
                  }else{
                    #ifdef MAUME_DEBUG // #endif
                      Serial.println(" #< Failed Saving Message ! Not acknowledging !");
                      MauMe.Log("---> PKT: "+String(pktCRC32)+" FAILED SAVING.");   
                    #endif
                  }
                }else{
                  //MauMe.Log("-RCPY MMM: "+String(pktCRC32)+")");   
                  #ifdef MAUME_DEBUG // #endif
                    Serial.println(" --> PKT ALREADY RECEIVED ! ("+String(pktCRC32)+")");
                    Serial.println(" --> Packet is @ DEST: SH-Acknowledge, as all TX need to be stopped.");   
                    MauMe.Log("--> PKT "+String(pktCRC32)+" DUPLICATE:ACK.");                                    
                  #endif
                  // Acknowledge packet: the packet was for us, so we can stop any other transmissions of it
                  MM_ACK * mmAck = NULL;
                  mmAck = MauMe.make_ACK(1, MM_TYPE_SHA, pktCRC32, MauMe.myByteAddress, list->LoRaPKT.HDR.SenderNode);
                  if(mmAck != NULL){
                    MauMe.save_ACK_AS_SELF("/SHAOUT/", mmAck, true);
                    free(mmAck);mmAck = NULL;
                  }else{Serial.println("  <> Error recording ACK to SHAOUT list !");}  
                }    
              }else{                                                                                              // PACKET MUST BE FORWARED :
                //=============================================================================================================================
                bool alreadyReceived = fileExists(SPIFFS, MauMe.spiMutex, String("/PKTSREM/"+String(pktCRC32)+".bin").c_str());
                #ifdef MAUME_DEBUG // #endif
                  Serial.println(" -> Saving received MMP : "+String(pktCRC32));
                  Serial.println(" ->        Received MMP file exists in "+String("/PKTSREM/"+String(pktCRC32)+".bin")+": "+String(alreadyReceived));
                  MauMe.Log("--> PKT: "+String(pktCRC32)+" : FWRD.");
                #endif 
                if(!alreadyReceived){ // If Message is not in the Forwarded Messages list: forward
                 if(int(pkt->HDR.NHP) < int(MM_PKT_MAXIMUM)){
                   if(!MauMe.save_PKT("/PKTSOUT/", String(pktCRC32)+".bin", pkt)){
                     #ifdef MAUME_DEBUG // #endif
                       Serial.println(" #< Failed Saving PKT ! Not acknowledging !");
                     #endif
                     MauMe.Log("#< Failed Saving PKT ! Not acknowledging ! >");
                   }else{
                    #ifdef MAUME_DEBUG // #endif
                      MauMe.Log("--> PKT: "+String(pktCRC32)+" : FWRD.");
                    #endif 
                   }
                 }else{
                    #ifdef MAUME_DEBUG 
                      Serial.println(" -> PKT has reached MAXIMUM HOP COUNT and is not arrived at DEST : ignoring and acknowledging !");
                      MauMe.Log("--> PKT "+String(pktCRC32)+" expired.");
                    #endif
                 }                 
                 // Record reminder:
                 MM_ACK * mmAck = NULL;
                 mmAck = MauMe.make_ACK(pkt->HDR.NHP, MM_TYPE_REM, pktCRC32, list->LoRaPKT.HDR.SenderNode, MauMe.myByteAddress);
                 if(mmAck != NULL){
                   MauMe.save_ACK("/PKTSREM/", mmAck, false);
                   free(mmAck);mmAck = NULL;
                   // Acknowledge PKT:  
                   #ifdef MAUME_DEBUG // #endif
                     Serial.println(" --> Reminder saved: send SH-ACK to sender node !");
                   #endif
                 }else{Serial.println("   <> Error recording ACK to PKTSREM list !");}
                 // SH-ACKNOWLEDGE :
                 MM_ACK * mmSHAck = NULL;
                 mmSHAck = MauMe.make_ACK(1, MM_TYPE_SHA, pktCRC32, MauMe.myByteAddress, list->LoRaPKT.HDR.SenderNode);
                 if(mmSHAck != NULL){
                  MauMe.save_ACK_AS_SELF("/SHAOUT/", mmSHAck, false);
                  free(mmSHAck);mmSHAck = NULL;
                 }else{Serial.println("  <> Error recording SHACK to SHAOUT list !");} 
                }else{
                  //---------------------------------------------------------------------------------------------------------------- PKT ALRDY FRWD
                  #ifdef MAUME_DEBUG // #endif
                    Serial.println(" --> PACKET ALREADY RECEIVED : acknowledge only in special cases...");
                  #endif
                  MM_ACK * remAck = NULL;
                  remAck = MauMe.load_ACK(String("/PKTSREM/"+String(pktCRC32)+".bin"));
                  if(remAck != NULL){
                    if(MauMe.addressesMatch(remAck->SEND, list->LoRaPKT.HDR.SenderNode)){ // First sender node has repeated MM PKT: acknowledge !
                      #ifdef MAUME_DEBUG 
                        Serial.println(" --> First Sender node has REPEATED KNOWN Packet: acknowledge !");
                        // MauMe.Log("--> PKT "+String(pktCRC32)+" REPEAT:ACK.");
                      #endif
                      MauMe.Log("--> PKT "+String(pktCRC32)+" REPEAT:ACK.");
                      free(remAck);remAck=NULL;
                      MM_ACK * mmShAck = NULL;
                      mmShAck = MauMe.make_ACK(1, MM_TYPE_SHA, pktCRC32, MauMe.myByteAddress, list->LoRaPKT.HDR.SenderNode);
                      if(mmShAck != NULL){
                        MauMe.save_ACK_AS_SELF("/SHAOUT/", mmShAck, false);
                        free(mmShAck);mmShAck = NULL;
                      }else{Serial.println("        <> Error recording ACK to SHAOUT list !");}
                    }else{
                      #ifdef MAUME_DEBUG  // #endif
                        Serial.println("****************************************\n ALREADY FORWARDED packet received FROM DIFFERENT FIRST NODE (NHP: "+String(pkt->HDR.NHP)+")! Dropping and not acknowledging... ("+String(pktCRC32)+")\n****************************************");                      
                        Serial.println(" -> Check for existing RECACK...");
                      #endif
                      if(fileExists(SPIFFS, MauMe.spiMutex, String("/RCAREM/"+String(pktCRC32)+".bin").c_str())){
                        Serial.println(" -> PKT was previously RECACK : RE-RECACK.");
                        MauMe.Log("--> PKT "+String(pktCRC32)+" DUP&RECACK:RECACK.");
                        MM_ACK * mmShAck = NULL;
                        mmShAck = MauMe.make_ACK(1, MM_TYPE_REC, pktCRC32, pkt->HDR.DEST, pkt->HDR.SEND); // MauMe.myByteAddress, list->LoRaPKT.HDR.SenderNode);
                        if(mmShAck != NULL){
                          MauMe.save_ACK_AS_SELF("/RCAOUT/", mmShAck, false);
                          free(mmShAck);mmShAck = NULL;
                        }else{Serial.println("        <> Error recording ACK to SHAOUT list !");}
                      }else{
                        #ifdef MAUME_DEBUG  // #endif
                          MauMe.Log("--> No-RECACK @ DUP PKT "+String(pktCRC32)+":IGN.");
                        #endif
                      }
                    }
                  }else{
                    Serial.println("****************************************\n ERROR LOADING FIRST REMINDER OF PACKET: TREATING AS NEW... ("+String(pktCRC32)+")\n****************************************");                      
                    // Message is alive and not yet received: we save it !
                    MauMe.Log("--> PKT "+String(pktCRC32)+" ERR LOADING REM !");
                    // Save PLT in out dir:
                    if(MauMe.save_PKT("/PKTSOUT/", String(pktCRC32)+".bin", pkt)){                      
                      // PKT is saved: acknowledge ?
                      // Record reminder:
                      MM_ACK * mmAck = NULL;
                      mmAck = MauMe.make_ACK(pkt->HDR.NHP, MM_TYPE_REM, pktCRC32, list->LoRaPKT.HDR.SenderNode, MauMe.myByteAddress);
                      if(mmAck != NULL){
                        MauMe.save_ACK("/PKTSREM/", mmAck, true);
                        free(mmAck);mmAck=NULL;
                        // Acknowledge PKT:  
                        #ifdef MAUME_DEBUG // #endif
                          Serial.println(" --> Reminder saved: send SH-ACK to sender node !");
                        #endif
                        MM_ACK * mmSHAck = NULL;
                        mmSHAck = MauMe.make_ACK(1, MM_TYPE_SHA, pktCRC32, MauMe.myByteAddress, list->LoRaPKT.HDR.SenderNode);
                        if(mmSHAck != NULL){
                          MauMe.save_ACK_AS_SELF("/SHAOUT/", mmSHAck, true);
                          free(mmSHAck);mmSHAck = NULL;
                        }else{Serial.println("        <> Error recording SHACK to SHAOUT list !");} 
                      }else{Serial.println("        <> Error recording ACK to PKTSREM list !");}
                    }else{
                      #ifdef MAUME_DEBUG // #endif
                        Serial.println(" #< Failed Saving Message ! Not acknowledging !");
                      #endif
                    }
                  }
                }
              }
              currentPos += MauMe.getPacketSize(pkt);
            }break;
          }
        }
      }else{
        //-----------------------------------------------------------------------------------------------------------------------------------------------------
        //-----------------------------------------------------------------------------------------------------------------------------------------------------
        list->PKTTYP = MM_PKT_TYPE_GEN;
        // packet transmission error or GEN packet !
        #ifdef MAUME_DEBUG 
          Serial.println(" ### GEN or Corrupted PACKET: DROPPING ! ###");
          Serial.println("-----------------------------------------\n GENERAL PACKET CONTENT : "+String(list->PKTSIZ)+" bytes:");
          Serial.write((const uint8_t*)&list->LoRaPKT, list->PKTSIZ);
          Serial.println("\n-----------------------------------------\n");
          MauMe.Log("-> PKT: CORRUPTED."); 
        #endif
      }
      #ifdef MAUME_DEBUG // #endif
        Serial.println(" ---------------------------------------------- NEXTING ON LIST ---------------------------");
      #endif
      if(list){
        free(list);list = NULL;
      }
      list = next;
    }
    list = NULL;
    #ifdef MAUME_DEBUG // #endif
      Serial.println(" * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *");
    #endif 
  }else{
    #ifdef MAUME_DEBUG // #endif
      Serial.println(" <> No received packet to process.");
    #endif
  } 
  return nbPKTsArrived;
}
//-----------------------------------------------------------------------------------------------------------------------------------------------------
String MauMeClass::getShortAddress(ADDRESS address1, int n){ // Tot len: MM_ADDRESS_SIZE*3-1    MM_ADDRESS_SIZE*3 - 1 - 17  MM_ADDR_DISP_LONG
  return String(MauMe.addressBuffer2MacFormat((char*)address1.dat, MM_ADDRESS_SIZE).substring(int((MM_ADDRESS_SIZE-n)*3)));
}

//-----------------------------------------------------------------------------------------------------------------------------------------------------
bool MauMeClass::addressesMatch(ADDRESS address1, ADDRESS address2){
  return MauMe.compareBuffers((const char*)&address1, (const char*)&address2, MM_ADDRESS_SIZE);
}

//-----------------------------------------------------------------------------------------------------------------------------------------------------
bool MauMeClass::writeMAC2MEM(String mac){
  //EEPROM.begin(64);
  EEPROM.writeString(0, mac);
  bool ret = false;
  while(!xSemaphoreTakeRecursive(MauMe.spiMutex, portMAX_DELAY));      // MauMe.nbPktsReceived
    ret = EEPROM.commit();
  xSemaphoreGiveRecursive(MauMe.spiMutex);
  return ret;
}

//-----------------------------------------------------------------------------------------------------------------------------------------------------
String MauMeClass::readMEM2MAC(){
  //EEPROM.begin(64);
  String mac = EEPROM.readString(0);
  return mac;
}

//-----------------------------------------------------------------------------------------------------------------------------------------------------
void IRAM_ATTR MauMeClass::onLoRaPacket( int input ){
  Data_PKT * dataPacket = NULL;
  dataPacket = MauMe.getCurrentLoRaPkt(); // MauMe.nbPktsSent
  if(dataPacket != NULL){
    xSemaphoreTakeRecursive(MauMe.pcktListSemaphore, portMAX_DELAY);
      dataPacket->next = (Data_PKT *)MauMe.receivedPackets;
      MauMe.receivedPackets = dataPacket;
    xSemaphoreGiveRecursive(MauMe.pcktListSemaphore);
  }
}

//-----------------------------------------------------------------------------------------------------------------------------------------------------
void IRAM_ATTR MauMeClass::LoRaTask( void * pvParameters ){
  #ifdef MAUME_DEBUG   // #endif
    Serial.print(" -> LoRaTask started on core ");
    Serial.println(xPortGetCoreID());
  #endif
  while(true){
    if(MauMe.doRun){
      while(!xSemaphoreTakeRecursive(MauMe.spiMutex, portMAX_DELAY));      // // xSemaphoreGiveRecursive(MauMe.spiMutex); while(!xSemaphoreTakeRecursive(LoRaJMMClass::spiMutex, portMAX_DELAY)){}
        int packetSize = LoRa_JMM.parsePacket();
      xSemaphoreGiveRecursive(MauMe.spiMutex);
      if (packetSize > 0) {
        Data_PKT * dataPacket = NULL;
        dataPacket = MauMe.getCurrentLoRaPkt(); // MauMe.nbPktsSent
        if(dataPacket != NULL){
          xSemaphoreTakeRecursive(MauMe.pcktListSemaphore, portMAX_DELAY);
            dataPacket->next = (Data_PKT *)MauMe.receivedPackets;
            MauMe.receivedPackets = dataPacket;
          xSemaphoreGiveRecursive(MauMe.pcktListSemaphore);
          #ifdef MAUME_DEBUG 
            Serial.println(" -> Done receiving.");
          #endif
          String sender = String(" >..."+MauMe.getShortAddress(dataPacket->LoRaPKT.HDR.SenderNode, MM_ADDR_DISP_LONG));
          sender.toUpperCase();
          MauMe.oledMauMeMessage("RX>"+String(MauMe.MM_NB_RXT)+",RSSI"+String(dataPacket->RSSI), sender, "  ID "+String(dataPacket->LoRaPKT.HDR.CRC32));
          Serial.println(" -> NB>"+String(MauMe.MM_NB_RXT)+",RSSI"+String(dataPacket->RSSI) +", From "+ sender + ", CRC32 "+String(dataPacket->LoRaPKT.HDR.CRC32));
        }else{
          MauMe.oledMauMeMessage(" REC ERR (NB "+String(MauMe.MM_NB_RXT)+")", " ERR ", " ERR ");       
          delay(LORA_READ_DELAY);
        }
        packetSize = 0;
        LoRa_JMM.receive();
      }else{
        delay(LORA_READ_DELAY);
      }
    }else{
      #ifdef MAUME_DEBUG   // #endif
        Serial.println(" !!! LoRa deactivated !!!");
      #endif
      delay(50*LORA_READ_DELAY);
    }
  }
}

//-----------------------------------------------------------------------------------------------------------------------------------------------------
void MauMeClass::activate_LoRa(){
  #ifdef MAUME_DEBUG   // #endif
    Serial.println(" * LoRa activation:");
  #endif
  SPI.begin(LoRa_SCK, LoRa_MISO, LoRa_MOSI, LoRa_CS);
  delay(250); 
  LoRa_JMM.setPins(LoRa_CS, LoRa_RESET, LoRa_IRQ);
  if (!LoRa_JMM.begin(BAND, MauMe.spiMutex)) {
    while (1){
      Serial.println(" LORA ERROR !!!");
      delay(1000);
    }
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
                    12288,      /* Stack size of task */
                    NULL,       /* parameter of the task */
                    2,          /* priority of the task */
                    &MauMe.loRaTask,  /* Task handle to keep track of created task */
                    0);         /* pin task to core 0 */                  
  delay(LORA_READ_DELAY); 
  LoRa_JMM.receive();
  #ifdef MAUME_DEBUG   // #endif
    Serial.println(" -> LoRa Init OK!\n");
  #endif
}

//-----------------------------------------------------------------------------------------------------------------------------------------------------
void MauMeClass::DnsTask(void * pvParameters){ 
  while(true) {
    delay(DNS_TASK_DELAY);
    if(MauMe.runDNS){
      MauMe.dnsServer->processNextRequest();    
    }
  }
}

//-----------------------------------------------------------------------------------------------------------------------------------------------------
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

//-----------------------------------------------------------------------------------------------------------------------------------------------------
void MauMeClass::activate_Wifi(){
  #ifdef MAUME_DEBUG   // #endif
    Serial.println(" * Setting up Wifi.");
  #endif
  IPAddress local_IP(1, 1, 1, 1);
  IPAddress gateway(1, 1, 1, 1);
  IPAddress subnet(255, 255, 255, 0);
  String address = MauMe.getShortAddress(MauMe.myByteAddress, MM_ADDR_DISP_LONG); // String(MyAddress.substring(int(MM_ADDRESS_SIZE*3 - 1 - 20)));
  address.toUpperCase();
  String strSSID = "MauMe-"+address; // MauMe.myMacAddress; // String(String(ssid)+ MauMe.addressBuffer2Str((char*)MauMe.myByteAddress.dat, MM_ADDRESS_SIZE)); // MyAddress);
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

//-----------------------------------------------------------------------------------------------------------------------------------------------------
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
                    2048,    
                    NULL,     
                    1,        
                    &MauMe.dnsTask/*, 
                    1*/);       
  delay(250); 
  #ifdef MAUME_DEBUG   // #endif
    Serial.println(" -> DNS server set-up !");
  #endif
}

//-----------------------------------------------------------------------------------------------------------------------------------------------------
void IRAM_ATTR MauMeClass::oledMauMeMessage(String message1, String message2, String message3){
  while(!xSemaphoreTakeRecursive(MauMe.spiMutex, portMAX_DELAY));
    //MauMe.display = new SSD1306(0x3c, OLED_SDA, OLED_SCL);
    MauMe.display->init();
    MauMe.display->clear();
    MauMe.display->drawXbm(MauMe.display->width()-4 - UPF_Logo_Width, 0, UPF_Logo_Width, UPF_Logo_Height, (const uint8_t*)&UPF_Logo_Bits); 
    MauMe.display->drawString(3,  0, " Mau-Me Node");
    MauMe.display->drawString(3, 12, message1);
    String address = MauMe.getShortAddress(MauMe.myByteAddress, MM_ADDR_DISP_LONG); // String(MyAddress.substring(int(MM_ADDRESS_SIZE*3 - 1 - 20)));
    address.toUpperCase();
    MauMe.display->drawString(4, 25, " ID "+address);    
    MauMe.display->drawString(3, 37, message2);
    MauMe.display->drawString(3, 50, message3);
    MauMe.display->display();
  xSemaphoreGiveRecursive(MauMe.spiMutex);
}

//-----------------------------------------------------------------------------------------------------------------------------------------------------
void MauMeClass::oledDisplay(bool doFlip){
  while(!xSemaphoreTakeRecursive(MauMe.spiMutex, portMAX_DELAY)); 
    MauMe.display = new SSD1306(0x3c, OLED_SDA, OLED_SCL);
    #ifdef MAUME_DEBUG   // #endif
      Serial.println(" * MauMe OLED Display:");
    #endif
    pinMode(OLED_RST, OUTPUT);
    digitalWrite(OLED_RST, LOW);    // set GPIO16 low to reset OLED
    delay(50); 
    digitalWrite(OLED_RST, HIGH);
    MauMe.display->init();
    MauMe.display->clear();
    if(doFlip)
        MauMe.display->flipScreenVertically();
    MauMe.display->setFont(ArialMT_Plain_10);
    MauMe.display->setTextAlignment(TEXT_ALIGN_LEFT);    
    MauMe.display->drawXbm(display->width()-4 - UPF_Logo_Width, 0, UPF_Logo_Width, UPF_Logo_Height, (const uint8_t*)&UPF_Logo_Bits); 
    MauMe.display->drawString(3,  0, " Mau-Me Node");
    MauMe.display->drawString(3, 12, "   UPF 2020");
    String address = MauMe.getShortAddress(MauMe.myByteAddress, MM_ADDR_DISP_LONG); //String(MyAddress.substring(int(MM_ADDRESS_SIZE*3 - 1 - 20)));
    address.toUpperCase();
    MauMe.display->drawString(4, 25, "ID "+address);    
    MauMe.display->drawString(4, 37, "    The Collaborative");
    MauMe.display->drawString(4, 50, "      LoRa Network");
    MauMe.display->display();
  xSemaphoreGiveRecursive(MauMe.spiMutex);
  #ifdef MAUME_DEBUG   // #endif
    Serial.println(" -> MauMe OLED Display set up.");    
  #endif
}

//-----------------------------------------------------------------------------------------------------------------------------------------------------
void MauMeClass::notFound(AsyncWebServerRequest *request) {
  request->send(404, "text/plain", "<br><h2>MauMe Node - Error : </h2><br><br>Not found<br><br><a href=\"/\">Return to Home Page</a>");
}

//-----------------------------------------------------------------------------------------------------------------------------------------------------
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
    if(macAddressList.list[i] != NULL){
      memcpy(&(macAddressList.list[i]->dat[0]), &(MauMe.addressFromBytes(MauMe.getSha256BytesOfBytes((char*)&station.mac, (const size_t)6)).dat[0]), MM_ADDRESS_SIZE);//&station.mac, 6);
    }    
  }
  return macAddressList;
}

//-----------------------------------------------------------------------------------------------------------------------------------------------------
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

//-----------------------------------------------------------------------------------------------------------------------------------------------------
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

//-----------------------------------------------------------------------------------------------------------------------------------------------------
struct eth_addr *  MauMeClass::getClientMAC(IPAddress clntAd){
  #ifdef MAUME_DEBUG 
    Serial.println(" -> Fetching "+String(clntAd));
  #endif
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

//-----------------------------------------------------------------------------------------------------------------------------------------------------
AsyncResponseStream *  MauMeClass::getWebPage(AsyncWebServerRequest *request, ADDRESS macFrom){  //  MMM_APPTYP_NOD
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
      h1{font-size:36px}h2{font-size:30px}h3{font-size:24px}h4{font-size:20px}h5{font-size:18px}h6{font-size:15px}h7{font-size:13px}.w3-serif{font-family:serif}
      h1,h2,h3,h4,h5,h6,h7{font-family:"Segoe UI",Arial,sans-serif;font-weight:400;margin:10px 0}.w3-wide{letter-spacing:4px}
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
  response->print(MauMe.listReceivedMessages2Html(SPIFFS, MauMe.spiMutex, "/INBOX", macFrom) + "</h5></div><br/>");
  response->print("<div style=\"background-color: seashell;padding:6px 10px 8px;border-width:1px;border-radius:10px;border-style:solid;border-color:darkseagreen;\"><h5>"+ MauMe.listSent2Str(SPIFFS, MauMe.spiMutex, "/OUTBOX", macFrom));
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
AsyncResponseStream *  MauMeClass::getAdminPage(AsyncWebServerRequest *request, ADDRESS macFrom){  //  MMM_APPTYP_NOD
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
      h1{font-size:36px}h2{font-size:30px}h3{font-size:24px}h4{font-size:20px}h5{font-size:18px}h6{font-size:15px}h7{font-size:13px}.w3-serif{font-family:serif}
      h1,h2,h3,h4,h5,h6,h7{font-family:"Segoe UI",Arial,sans-serif;font-weight:400;margin:10px 0}.w3-wide{letter-spacing:4px}
      h2{color:dodgerblue;}h3{color:darkslategray;}h4{color:darkcyan;}h5{color:midnightblue;}
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
    <h4 align="Left">Go to Simulations form :</h4><h4 align="center"><form action="/simus">
      <input type="submit" id="submitBtn" value="Go to Simulations" name="simulations"/><br/>
    </form></h4>)rawliteral");
  response->print(R"rawliteral(
    <h4 align="Left">MauMe File System :</h4><h4 align="center"><form action="/delete">
      <input type="submit" id="submitBtn" value="   Delete All   " name="DelAll"/><br/>
      <input type="submit" id="submitBtn" value=" Delete INBOX "   name="DelInbox"  />&#160;&#160;<input type="submit" id="submitBtn"  value=" Delete SENT  "   name="DelSent"/><br/>
      <input type="submit" id="submitBtn" value="Delete PKTSOUT "  name="DelPktsOut"/>&#160;&#160;<input type="submit" id="submitBtn"  value="Delete SHAOUT"  name="DelAcksOut"/><br/>
      <input type="submit" id="submitBtn" value="Delete SHASIN "  name="DelAcksRec"/>&#160;&#160;<input type="submit" id="submitBtn"  value="Delete RECPKTS"  name="DelRecPkts"/><br/>
      <input type="submit" id="submitBtn" value="Delete ACKFWDD"  name="DelNodePkts"/>&#160;&#160;<input type="submit" id="submitBtn" value="Delete OUTBOX "   name="DelOutbox"/><br/>
      <input type="submit" id="submitBtn" value=" Delete INRCAS "  name="DelInAcks"/>&#160;&#160;<input type="submit" id="submitBtn"   value=" Delete RECVD "  name="DelFrwd"/><br/>
    </form></h4>)rawliteral"); // DelNodePkts
  //-------------------------------------------------- MMM_APPTYP_PRV MMM_APPTYP_NOD
  //listReceivedMessages2Html(SPIFFS, MauMe.spiMutex, "/INBOX", appType, macFrom) + listSent2Str(SPIFFS, MauMe.spiMutex, "/OUTBOX", appType, macFrom); ACKFWDD RECVD
  response->print("<h3><b>Directory listing :</b></h3>");
  // response->print("<h4><b>RECPKTS files  : </b></h4><h6>"+listDir2Str(SPIFFS, MauMe.spiMutex, "/RECPKTS", "<br/>")+"</h6>");
  // response->print("<h4><b>ACKFWDD files : </b></h4><h6>"+listDir2Str(SPIFFS, MauMe.spiMutex, "/ACKFWDD","<br/>")+"</h6>");
  response->print("<h4><b>INBOX files    : </b></h4><h6>"+listReceivedPackets2Html(SPIFFS, MauMe.spiMutex, "/INBOX")+"</h6>"); // ,   "<br/>",  "<br/>")
  response->print("<h4><b>OUTBOX files   : </b></h4><h6>"+listReceivedPackets2Html(SPIFFS, MauMe.spiMutex, "/OUTBOX")+"</h6>");
  response->print("<h4><b>INRCAS files   : </b></h4><h6>"+listReceivedACKs2Html(SPIFFS,    MauMe.spiMutex, "/INRCAS")+"</h6>");
  //response->print("<h4><b>SENT files     : </b></h4><h6>"+listDir2Str(SPIFFS, MauMe.spiMutex, "/SENT",    "<br/>")+"</h6>");
  response->print("<h4><b>PKTSOUT files  : </b></h4><h6>"+listReceivedPackets2Html(SPIFFS, MauMe.spiMutex, "/PKTSOUT")+"</h6>"); // , "<br/>"
  response->print("<h4><b>RCAOUT files  : </b></h4><h6>"+listReceivedACKs2Html(SPIFFS, MauMe.spiMutex, "/RCAOUT")+"</h6>"); // , "<br/>"
  response->print("<h4><b>SHAOUT files  : </b></h4><h6>"+listReceivedACKs2Html(SPIFFS, MauMe.spiMutex, "/SHAOUT")+"</h6>"); // , "<br/>"
  //response->print("<h4><b>SHASIN files  : </b></h4><h6>"+listReceivedACKs2Html(SPIFFS, MauMe.spiMutex, "/SHASIN")+"</h6>");
  //response->print("<h4><b>WEB DATA files : </b></h4><h6>"+listDir2Str(SPIFFS, MauMe.spiMutex, "/WWW",     "<br/>")+"</h6>");
  //--------------------------------------------------
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
AsyncResponseStream *  MauMeClass::getSimusPage(AsyncWebServerRequest *request, ADDRESS macFrom){  //  MMM_APPTYP_NOD
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
      h1{font-size:36px}h2{font-size:30px}h3{font-size:24px}h4{font-size:20px}h5{font-size:18px}h6{font-size:15px}h7{font-size:13px}.w3-serif{font-family:serif}
      h1,h2,h3,h4,h5,h6,h7{font-family:"Segoe UI",Arial,sans-serif;font-weight:400;margin:10px 0}.w3-wide{letter-spacing:4px}
      h2{color:dodgerblue;}h3{color:darkslategray;}h4{color:darkcyan;}h5{color:midnightblue;}
      body{background-color: cornsilk;}
      #submitBtn{background:seagreen;padding:5px 7px 6px;font:bold;color:#fff;border:1px solid #eee;border-radius:6px;box-shadow: 5px 5px 5px #eee;text-shadow:none;}
      </style>
      </head><body><div><h2 align="center"><b>Mau-Me Simus</b><form action="/admin"><input type="image" src="/WWW/Logo-MMM-color.png" alt="Admin Form" width="100" align="right"/></form></h2></div>
    )rawliteral");
  //----------------------------------------------
  //response->print("<h3><b>&#160;LoRa SMS - UPF 2020</b><br/><h3><b>&#160;&#160;&#160;Node: "+MauMe.addressBuffer2MacFormat(myByteAddress.dat, MM_ADDRESS_SIZE)+"</b></h3>");
  response->print("<h4><b>&#160;LoRa SMS - UPF 2020</b></h4><br/><h5><b>&#160;&#160;&#160;Node "+MauMe.myMacAddress+"<br/>Node address : <button onclick=\"copyNodeAddress()\" align=\"right\">Copy</button>");
  response->print(MauMe.addressBuffer2MacFormat(MauMe.myByteAddress.dat, MM_ADDRESS_SIZE)+"</b></h5>");  
  response->print("<b><h5>&#160;&#160;&#160;&#160;&#160;&#160;My phone address:</h5><h5 align=\"center\">"+MauMe.addressBuffer2MacFormat(macFrom.dat, MM_ADDRESS_SIZE));
  response->print(R"rawliteral(&#160;&#160;<button onClick="window.location.reload();" align="right">Refresh Page</button></h5></b>)rawliteral");
  //----------------------------------------------
  //File file = fileOpenFor(SPIFFS, MauMe.spiMutex, MauMe.logFile.c_str(), FILE_READ); 
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
    String logString = readFile(SPIFFS, MauMe.spiMutex, MauMe.logFile);
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

//-----------------------------------------------------------------------------------------------------------------------------------------------------
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
        response = MauMe.getWebPage(request, macFrom);
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
        #ifdef MAUME_CHECK_ADMIN  // #endif  addressesMatch  if(MauMe.compareBuffers((const char*)&macFrom, (const char*)&adminMac, MM_ADDRESS_SIZE)){
          ADDRESS adminMac = MauMe.macFromMacStyleString(MAUME_ADMIN_ADDR);//"B0:A2:E7:0E:9E:AF");
          if(MauMe.addressesMatch(macFrom, adminMac){
            response = MauMe.getAdminPage(request, macFrom, (byte)MMM_APPTYP_NOD);
          }else{
            response = MauMe.getWebPage(request, macFrom, (byte)MMM_APPTYP_NOD);
          }
        #else
          response = MauMe.getAdminPage(request, macFrom);
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
    //-----------------------------------------------------------------------------------------------------------------------------------------------------
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
        MM_PKT * mmm = NULL;
        mmm = MauMe.createPKT(macFrom, addressTo, mess);
        if(mmm != NULL){          
          /* #ifdef MAUME_DEBUG 
            Serial.println("-----------------------------------------\n PKT DAT : ");
            Serial.write((const uint8_t*)mmm, mmm->HDR.LOADSIZE);
            Serial.println("\n-----------------------------------------\n");
          #endif */
          bool ret = MauMe.addPacket(mmm);
          if(ret){
            request->send(200, "text/html", "<br><h2 style=\"font-size:30px;font-family:\"Segoe UI\",Arial,sans-serif;font-weight:400;margin:10px 0;color:orangered;\"> MauMe Node "+MauMe.MyAddress+" - Message enlisted : </h2><br><br>" + MauMe.pkt2Html(mmm) +"<br><br><a href=\"/\">Return to Mau-Me Page</a>");
          }else{
            request->send(200, "text/html", "<br><h2 style=\"font-size:30px;font-family:\"Segoe UI\",Arial,sans-serif;font-weight:400;margin:10px 0;color:orangered;\"> MauMe Node "+MauMe.MyAddress+" - Message enlisting failed</h2><br><br><a href=\"/\">Return to Mau-Me Page</a>");
          }
        }else{
          request->send(200, "text/html", "<br><h2 style=\"font-size:30px;font-family:\"Segoe UI\",Arial,sans-serif;font-weight:400;margin:10px 0;color:orangered;\"> MauMe Node "+MauMe.MyAddress+" - Packet creation failed</h2><br><br><a href=\"/\">Return to Mau-Me Page</a>");
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
        ADDRESS adminMac = MauMe.macFromMacStyleString(MAUME_ADMIN_ADDR);//"B0:A2:E7:0E:9E:AF"); addressesMatch  if(MauMe.compareBuffers((const char*)&macFrom.dat, (const char*)&adminMac.dat, MM_ADDRESS_SIZE)){
        ADDRESS macFrom = MauMe.addressFromBytes(MauMe.getSha256BytesOfBytes((char*)clntMacAddr->addr, (const size_t)6));      
        if(MauMe.addressesMatch(macFrom, adminMac)){
      #endif
          bool DelAll = false, DelInAcks = false, DelInbox = false, DelAcksRec = false, DelSent = false, DelPktsOut = false, DelAcksOut = false, DelRecPkts = false, DelOutbox = false, DelNodePkts = false, DelFrwd = false;
          if (request->hasParam("DelAll")) {
            #ifdef MAUME_DEBUG 
              Serial.println(" ------------------------------------------- DelAll Button - ");
            #endif
            DelAll = true;
          }else if (request->hasParam("DelInAcks")) {
            #ifdef MAUME_DEBUG 
              Serial.println(" ------------------------------------------- DelInAcks Button - ");
            #endif
            DelInAcks = true;
          }else if (request->hasParam("DelInbox")) {
            #ifdef MAUME_DEBUG 
              Serial.println(" ------------------------------------------- DelInbox Button - ");
            #endif
            DelInbox = true;
          }else if (request->hasParam("DelSent")) {
            #ifdef MAUME_DEBUG 
              Serial.println(" ------------------------------------------- DelSent Button - ");
            #endif
            DelSent = true;
          }else if (request->hasParam("DelPktsOut")) {
            #ifdef MAUME_DEBUG 
              Serial.println(" ------------------------------------------- DelPktsOut Button - ");
            #endif
            DelPktsOut = true;
          }else if (request->hasParam("DelAcksOut")) {
            #ifdef MAUME_DEBUG 
              Serial.println(" ------------------------------------------- DelAcksOut Button - ");
            #endif
            DelAcksOut = true;
          }else if (request->hasParam("DelRecPkts")) {
            #ifdef MAUME_DEBUG 
              Serial.println(" ------------------------------------------- DelRecPkts Button - ");
            #endif
            DelRecPkts = true;
          }else if (request->hasParam("DelAcksRec")) {
            #ifdef MAUME_DEBUG 
              Serial.println(" ------------------------------------------- DelAcksRec Button - ");
            #endif
            DelAcksRec = true;
          } else if (request->hasParam("DelOutbox")) {
            #ifdef MAUME_DEBUG 
              Serial.println(" ------------------------------------------- DelOutbox Button - ");
            #endif
            DelOutbox = true;
          } else if (request->hasParam("DelNodePkts")) {
            #ifdef MAUME_DEBUG 
              Serial.println(" ------------------------------------------- DelNodePkts Button - ");
            #endif
            DelNodePkts = true;
          } else if (request->hasParam("DelFrwd")) {
            #ifdef MAUME_DEBUG 
              Serial.println(" ------------------------------------------- DelFrwd Button - ");
            #endif
            DelFrwd = true;
          }  
          int dels = 0; // ACKFWDD   DelFrwd
          if(DelAll || DelRecPkts) {dels += deleteDirectoryFiles(SPIFFS, MauMe.spiMutex,  "/RECPKTS");}
          if(DelAll || DelOutbox)  {dels += deleteDirectoryFiles(SPIFFS, MauMe.spiMutex,  "/OUTBOX");}
          if(DelAll || DelInAcks)  {dels += deleteDirectoryFiles(SPIFFS, MauMe.spiMutex,  "/INRCAS");}
          if(DelAll || DelInbox)   {dels += deleteDirectoryFiles(SPIFFS, MauMe.spiMutex,  "/INBOX");}
          if(DelAll || DelSent)    {dels += deleteDirectoryFiles(SPIFFS, MauMe.spiMutex,  "/SENT");}
          if(DelAll || DelPktsOut) {dels += deleteDirectoryFiles(SPIFFS, MauMe.spiMutex,  "/PKTSOUT");}
          if(DelAll || DelAcksOut) {dels += deleteDirectoryFiles(SPIFFS, MauMe.spiMutex,  "/SHAOUT");}
          if(DelAll || DelAcksOut) {dels += deleteDirectoryFiles(SPIFFS, MauMe.spiMutex,  "/RCAOUT");}
          if(DelAll || DelAcksRec) {dels += deleteDirectoryFiles(SPIFFS, MauMe.spiMutex,  "/SHASIN");}
          if(DelAll || DelNodePkts){dels += deleteDirectoryFiles(SPIFFS, MauMe.spiMutex,  "/ACKFWDD");}
          if(DelAll || DelFrwd)    {dels += deleteDirectoryFiles(SPIFFS, MauMe.spiMutex,  "/PKTSREM");}
          if(DelAll || DelFrwd)    {dels += deleteDirectoryFiles(SPIFFS, MauMe.spiMutex,  "/RCAREM");}
          
          #ifdef MAUME_DEBUG 
            Serial.println(" Deleted files ok : "+String(dels)+".");
          #endif
          if(dels>0){ // "/PKTSOUT/"
            request->send(200, "text/html", "<br><h2 style=\"font-size:30px;font-family:\"Segoe UI\",Arial,sans-serif;font-weight:400;margin:10px 0;color:orangered;\"> MauMe Node "+MauMe.MyAddress+" - Deleted "+String(dels) +" files. </h2><br><br><br><br><a href=\"/admin\">Return to Mau-Me Admin Page</a>");
          }else{
            request->send(200, "text/html", "<br><h2 style=\"font-size:30px;font-family:\"Segoe UI\",Arial,sans-serif;font-weight:400;margin:10px 0;color:orangered;\"> MauMe Node "+MauMe.MyAddress+" - No files deleted. </h2><br><br><br><br><a href=\"/admin\">Return to Mau-Me Admin Page</a>");
          }
      #ifdef MAUME_CHECK_ADMIN  // #endif
        }else{
          request->send(MauMe.getWebPage(request, macFrom, (byte)MMM_APPTYP_NOD));
        }
      #endif
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
          ADDRESS adminMac = MauMe.macFromMacStyleString(MAUME_ADMIN_ADDR);//"B0:A2:E7:0E:9E:AF"); addressesMatch  // if(MauMe.compareBuffers((const char*)&macFrom.dat, (const char*)&adminMac.dat, MM_ADDRESS_SIZE)){
          if(MauMe.addressesMatch(macFrom, adminMac)){
            response = MauMe.getSimusPage(request, macFrom);
          }else{
            response = MauMe.getWebPage(request, macFrom);
          }
        #else
          response = MauMe.getSimusPage(request, macFrom);
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
        #endif
        
        #ifdef MAUME_CHECK_ADMIN  // #endif
          ADDRESS adminMac = MauMe.macFromMacStyleString(MAUME_ADMIN_ADDR);//"B0:A2:E7:0E:9E:AF");   addressesMatch   //  if(MauMe.compareBuffers((const char*)&macFrom.dat, (const char*)&adminMac.dat, MM_ADDRESS_SIZE)){
          if(MauMe.addressesMatch(macFrom, adminMac)){              
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
              MauMe.CLEANING_PROCESS_STATIC_DELAY        =   MAUME_CLEAN_RATE*MauMe.MM_PCKT_PROCESS_STATIC_DELAY;  // milliseconds
              MauMe.CLEANING_PROCESS_RANDOM_DELAY        =   MAUME_CLEAN_RATE*MauMe.MM_PCKT_PROCESS_RANDOM_DELAY;  // milliseconds
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
              MauMe.CLEANING_PROCESS_STATIC_DELAY        =   MAUME_CLEAN_RATE*MauMe.MM_PCKT_PROCESS_STATIC_DELAY;  // milliseconds
              MauMe.CLEANING_PROCESS_RANDOM_DELAY        =   MAUME_CLEAN_RATE*MauMe.MM_PCKT_PROCESS_RANDOM_DELAY;  // milliseconds
            }else{
              #ifdef MAUME_DEBUG  // #endif
                Serial.println(" ------------------------------------------- NO PRCSS_MAX_DELAY IN FORM -----");
              #endif
            }
            MauMe.Log("-> SET TX PARAMS: TXPWMN-"+String(MauMe.MM_TX_POWER_MIN)+";TXPWMX-"+String(MauMe.MM_TX_POWER_MAX)+";TXDLMN-"+String(MauMe.MM_PCKT_PROCESS_STATIC_DELAY)+"ms;TXDLMX-"+String(MauMe.MM_PCKT_PROCESS_STATIC_DELAY+MauMe.MM_PCKT_PROCESS_RANDOM_DELAY)+"ms.");
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
            MauMe.Log("-> SET SAVE DELAY-"+String(MauMe.MM_PCKT_SAVE_DELAY)+"ms.");
          }  
        #ifdef MAUME_CHECK_ADMIN  // #endif
        }
        #endif
        #ifdef MAUME_CHECK_ADMIN  // #endif
          ADDRESS adminMac = MauMe.macFromMacStyleString(MAUME_ADMIN_ADDR);//"B0:A2:E7:0E:9E:AF"); // addressesMatch if(MauMe.compareBuffers((const char*)&macFrom.dat, (const char*)&adminMac.dat, MM_ADDRESS_SIZE)){
          if(MauMe.addressesMatch(macFrom, adminMac)){
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
        #endif
      #ifdef MAUME_CHECK_ADMIN  // #endif
        ADDRESS adminMac = MauMe.macFromMacStyleString(MAUME_ADMIN_ADDR);//"B0:A2:E7:0E:9E:AF");    // addressesMatch  if(MauMe.compareBuffers((const char*)&macFrom.dat, (const char*)&adminMac.dat, MM_ADDRESS_SIZE)){
        if(MauMe.addressesMatch(macFrom, adminMac)){              
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
          MauMe.Log("-> STRT ALTERNATING: "+String(MauMe.percentActivity)+" % of "+String(MauMe.alternateInterval)+" millis.");
        }else if(request->hasParam("deActAltAct")) {
          MauMe.doAlternate = false;
          MauMe.Log("-> STOP ALTERNATING: "+String(MauMe.percentActivity)+" % of "+String(MauMe.alternateInterval)+" millis.");
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
          MauMe.Log("-> STRT DUM: "+String(MauMe.nbDummyPkts)+" @ "+String(MauMe.dummyTargetAddress)+".");
          MauMe.dummyLastSendTime = millis();
        }else if(request->hasParam("DeActSimTx")) {
           MauMe.sendDummies = false;
           MauMe.Log("-> STOP DUM: "+String(MauMe.nbDummyPkts)+" @ "+String(MauMe.dummyTargetAddress)+".");
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
//-----------------------------------------------------------------------------------------------------------------------------------------------------
//-----------------------------------------------------------------------------------------------------------------------------------------------------
int MauMeClass::deleteOlderFiles(const char * dir, int overNumber){
  #ifdef MAUME_DEBUG 
    Serial.println(" * Entering old files cleaning");
  #endif
  NamesList * namesList = NULL;
  namesList = getDirList(SPIFFS, MauMe.spiMutex, dir, MM_TYPE_UKN);
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
        // char * truc = namesList->fileNames[i];
        free(namesList->fileNames[i]);
        namesList->fileNames[i] = NULL;
      }
      free(namesList->fileNames);
      namesList->fileNames = NULL;
      free(namesList->lwTimes);
      namesList->lwTimes = NULL;
      free(namesList->types);
      namesList->types = NULL;
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
    if(namesList->types){
      free(namesList->types);
      namesList->types = NULL;
    }    
    namesList->nbFiles = 0;
    if(namesList){
      free(namesList);
      namesList = NULL;
    }
  }else{Serial.println(" #< Could not get file list.");}
  return 0;
}

//-----------------------------------------------------------------------------------------------------------------------------------------------------
void MauMeClass::MauMeTask(void * pvParameters) {
  #ifdef MAUME_DEBUG   // #endif
    Serial.print(" -> Maume task started on core ");
    Serial.println(xPortGetCoreID());
  #endif  
  MauMe.dummySendInterval = 5000;// abs(random(MauMe.DUMMY_PROCESS_STATIC_DELAY, MauMe.DUMMY_PROCESS_STATIC_DELAY+MauMe.DUMMY_PROCESS_RANDOM_DELAY));     // 2-3 secondscleanPackets()          
  MauMe.packetsRoutineProcessInterval = abs(random(MauMe.MM_PCKT_PROCESS_STATIC_DELAY, MauMe.MM_PCKT_PROCESS_STATIC_DELAY+MauMe.MM_PCKT_PROCESS_RANDOM_DELAY));     // 2-3 secondscleanPackets()   RECVD        
  MauMe.cleaningRoutineProcessInterval = random(MauMe.CLEANING_PROCESS_STATIC_DELAY, MauMe.CLEANING_PROCESS_STATIC_DELAY+MauMe.CLEANING_PROCESS_RANDOM_DELAY);     // 2-3 secondscleanPackets()   RECVD
  while(true){
    if(MauMe.doRun){
      if (MauMe.sendDummies && (MauMe.nbDummyPkts > 0) && (MauMe.DUMMY_PROCESS_STATIC_DELAY >= DUMMY_PROCESS_MIN_STATIC_DELAY) && ((millis() - MauMe.dummyLastSendTime) > MauMe.dummySendInterval)) {
        if(MauMe.nbDummyPkts == 1){
          MauMe.sendDummies = false;
        }
        Serial.println("~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~\n    INJECTING DUMMY PACKET :");
        MM_PKT * dummyLoRaPkt = MauMe.createDummyPKT("Dummy message number "+String(MauMe.nbDummyPkts)+" : "+String(random(1, 10000))+".");
        if(MauMe.addPacket(dummyLoRaPkt)){
          Serial.println("\n~~~~~~~~~~ SUCCESS ~~~~~~~~~~~~~~~~");
        }else{
          Serial.println("\n~~~~~~~~~~ FAILURE ~~~~~~~~~~~~~~~~");
        }
        Serial.println("\n~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~");      // +",PW"+String(dummyLoRaPkt->TX_POW)+",TX"+String(dummyLoRaPkt->NB_TX)
        // MauMe.Log("-INJCT DUMY PKT: "+String(dummyLoRaPkt->LoRaPKT.HDR.CRC32)+"(Typ"+String(dummyLoRaPkt->LoRaPKT.HDR.MMTYPE)+","+String(dummyLoRaPkt->NB_MSG)+"MSG,"+String(dummyLoRaPkt->NB_ACK)+"ACK)");
        MauMe.nbDummyPkts -=1;        
        MauMe.dummySendInterval = random(MauMe.DUMMY_PROCESS_STATIC_DELAY, MauMe.DUMMY_PROCESS_STATIC_DELAY+MauMe.DUMMY_PROCESS_RANDOM_DELAY);     // 2-3 secondscleanPackets()  
        MauMe.dummyLastSendTime = millis();            // timestamp the message
      }else{
        /*#ifdef MAUME_DEBUG 
          if(MauMe.sendDummies && millis()%10000 == 0){
            Serial.println("  Next injection at : "+String((int((millis() - MauMe.dummyLastSendTime)*10000) / MauMe.dummySendInterval)/100)+"%");
          }
        #endif */ 
      }
      //==========================================================================================================
      if (millis() - MauMe.receivedPacketsLastSaveTime > abs(MauMe.MM_PCKT_SAVE_DELAY)) {
        #ifdef MAUME_DEBUG 
          Serial.println("_ _ _ _ _ _ _ _ _ _ _ _ _ _ _ _ _ _ _ _ _ _ SAVE DATA LOOP _ _ _ _ _ _ _ _ _ _ _ _ _ _ _ _ _ _ _ _ _ _ _ _ _ _ _ _ ");
          Serial.println("                                     Time : "+MauMe.getTimeString(time(NULL)));
        #endif // MauMe.nbReceivedPkts += 
        MauMe.nbUserPackets = MauMe.saveReceivedPackets();
        #ifdef MAUME_DEBUG 
          if(MauMe.MM_NB_RX > 0){Serial.println(" * Received data saved.");}
          Serial.println("_ _ _ _ _ _ _ _ _ _ _ _ _ _ _ _ _ _ _ _ _ _ _ _ _ _ _ _ _ _ _ _ _ _ _ _ _ _ _ _ _ _ _ _ _ _ _ _ _ _ _ _ _ _ _ _ _ ");
        #endif      
        //-----------------------------------------------------------------------------------------------------------------------------------------------------
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
        MauMe.receivedPacketsLastSaveTime = millis();            // timestamp the message
      }
      
      //==========================================================================================================
      if (millis() - MauMe.packetsRoutineLastProcessTime > MauMe.packetsRoutineProcessInterval) {
        #ifdef MAUME_DEBUG 
          Serial.println("_ _ _ _ _ _ _ _ _ _ _ _ _ _ _ _ _ _ _ _ _ _ PROCESS LOOP _ _ _ _ _ _ _ _ _ _ _ _ _ _ _ _ _ _ _ _ _ _ _ _ _ _ _ _ ");
          if(0){
            listDir(SPIFFS, MauMe.spiMutex, "/INBOX", 0);
            listDir(SPIFFS, MauMe.spiMutex, "/OUTBOX", 0);
            listDir(SPIFFS, MauMe.spiMutex, "/INRCAS", 0);
            listDir(SPIFFS, MauMe.spiMutex, "/PKTSOUT", 0);
            listDir(SPIFFS, MauMe.spiMutex, "/SHAOUT", 0);
            // listDir(SPIFFS, MauMe.spiMutex, "/ACKSOUT", 0);
            listDir(SPIFFS, MauMe.spiMutex, "/RCAOUT", 0);
            listDir(SPIFFS, MauMe.spiMutex, "/SHASIN", 0);
            listDir(SPIFFS, MauMe.spiMutex, "/PKTSREM", 0);
            listDir(SPIFFS, MauMe.spiMutex, "/RCAREM", 0);
          }else{
            //listDir(SPIFFS, MauMe.spiMutex, "/", 0);
          }
        #endif
        #ifdef MAUME_DEBUG 
          int nbPacktsSent = MauMe.processPackets();      
          if(nbPacktsSent > 0){Serial.println(" -> Sent "+String(nbPacktsSent)+" PKTs and ACKs.");}
        #else
          MauMe.processPackets();
        #endif
               
        // ---------------------------------------- CALLING USER CALBBACK IF ANY ----------------------------
        if(MauMe.nbUserPackets > 0){
          if(MauMe._onDelivery){
            MauMe._onDelivery(MauMe.nbUserPackets);
          }
          MauMe.nbUserPackets = 0;
        }
        // --------------------------------------------------------------------------------------------------
        int delNb = MauMe.deleteOlderFiles("/INBOX", MM_MAX_BOX_PKTS);
        #ifdef MAUME_DEBUG 
          Serial.println(" -> Deleted "+String(delNb)+" overnumbered INBOX files.");
        #endif
        delNb = MauMe.deleteOlderFiles("/OUTBOX", MM_MAX_BOX_PKTS);
        #ifdef MAUME_DEBUG 
          Serial.println(" -> Deleted "+String(delNb)+" overnumbered OUTBOX files.");
        #endif
        delNb = MauMe.deleteOlderFiles("/INRCAS", MM_MAX_BOX_PKTS);
        #ifdef MAUME_DEBUG 
          Serial.println(" -> Deleted "+String(delNb)+" overnumbered INRCAS files.");
        #endif
        delNb = MauMe.deleteOlderFiles("/PKTSOUT", MM_MAX_NODE_PKTS);
        #ifdef MAUME_DEBUG 
          Serial.println(" -> Deleted "+String(delNb)+" overnumbered PKTSOUT files.");
        #endif
        delNb = MauMe.deleteOlderFiles("/SHAOUT", MM_MAX_NODE_ACKS);
        #ifdef MAUME_DEBUG 
          Serial.println(" -> Deleted "+String(delNb)+" overnumbered SHAOUT files.");
        #endif
        delNb = MauMe.deleteOlderFiles("/RCAOUT", MM_MAX_NODE_ACKS);
        #ifdef MAUME_DEBUG 
          Serial.println(" -> Deleted "+String(delNb)+" overnumbered RCAOUT files.");
        #endif
        delNb = MauMe.deleteOlderFiles("/PKTSREM", MM_MAX_NODE_REMS);
        #ifdef MAUME_DEBUG 
          Serial.println(" -> Deleted "+String(delNb)+" overnumbered PKTSREM files.");
        #endif
        delNb = MauMe.deleteOlderFiles("/RCAREM", 2*MM_MAX_NODE_REMS);  // x2 as there is tow REM for each RECACK
        #ifdef MAUME_DEBUG 
          Serial.println(" -> Deleted "+String(delNb)+" overnumbered RCAREM files.");
        #endif
        delNb = MauMe.deleteOlderFiles("/SHASIN", MM_MAX_NODE_ACKS);
        #ifdef MAUME_DEBUG 
          Serial.println(" -> Deleted "+String(delNb)+" overnumbered SHASIN files.");
          Serial.println("_ _ _ _ _ _ _ _ _ _ _ _ _ _ _ _ _ _ _ _ _ _ _ _ _ _ _ _ _ _ _ _ _ _ _ _ _ _ _ _ _ _ _ _ _ _ _ _ _ _ _ _ _ _ _ _ _ ");
          MauMe.printWifiClients();
        #endif
        MauMe.packetsRoutineProcessInterval  = random(MauMe.MM_PCKT_PROCESS_STATIC_DELAY, MauMe.MM_PCKT_PROCESS_STATIC_DELAY+MauMe.MM_PCKT_PROCESS_RANDOM_DELAY);     // 2-3 secondscleanPackets()   RECVD
        MauMe.packetsRoutineLastProcessTime = millis();
      }
      //==========================================================================================================
      if (millis() - MauMe.cleaningRoutineLastProcessTime > MauMe.cleaningRoutineProcessInterval) {
        #ifdef MAUME_DEBUG 
          Serial.println("_ _ _ _ _ _ _ _ _ _ _ _ _ _ _ _ _ _ _ _ _ _ CLEANING LOOP _ _ _ _ _ _ _ _ _ _ _ _ _ _ _ _ _ _ _ _ _ _ _ _ _ _ _ _ ");
          int nbCleanPkts = MauMe.cleanPacketsVsACKs();
          if(nbCleanPkts > 0){Serial.println(" * Cleaned "+String(nbCleanPkts)+" packets files.");}
            Serial.println(" * Delayed cleaning by : "+String((millis() - MauMe.cleaningRoutineLastProcessTime)/1000)+" seconds.");
        #else
          MauMe.cleanPacketsVsACKs();
        #endif        
        MauMe.cleaningRoutineProcessInterval = random(MauMe.CLEANING_PROCESS_STATIC_DELAY, MauMe.CLEANING_PROCESS_STATIC_DELAY+MauMe.CLEANING_PROCESS_RANDOM_DELAY);     // 2-3 secondscleanPackets()   RECVD
        MauMe.cleaningRoutineLastProcessTime = millis();
      }
      //==========================================================================================================
      // alternateLastActivationTime  doAlternate   percentActivity  alternateInterval
      if(MauMe.doAlternate){
        time_t t = millis();
        MauMe.alternateInterval = abs(MauMe.alternateInterval);
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
    }  
  }
}
//-----------------------------------------------------------------------------------------------------------------------------------------------------
void MauMeClass::setup(long serialSpeed) {
  Serial.begin(serialSpeed);
  EEPROM.begin(EEPROM_SIZE);    
  while (!Serial); //if just the the basic function, must connect to a computer
  delay(250);
  Serial.println("\n MauMe Node setup in progress...");
  randomSeed(analogRead(0));
  #ifdef MAUME_DEBUG    
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
  //------------------------------------------------------------------------------------------------------------------------------------------------------  
  MauMe.pcktListSemaphore = xSemaphoreCreateRecursiveMutex();
  MauMe.spiMutex = xSemaphoreCreateRecursiveMutex();
  #ifdef MAUME_DEBUG 
    if(MauMe.pcktListSemaphore != NULL){
      Serial.println(" -> Created semaphore");        
    }else{Serial.println(" #< Failed to create Semaphore !");}
  #endif
  //----------------------------------------------  
  if(!SPIFFS.begin(true)){
    Serial.println(" #< SPIFFS Start Failed !");
    while(true);
    return;
  }
  delay(250); 
  //----------------------------------------------
  MauMe.myMacAddress = String(WiFi.macAddress());
  MauMe.myByteAddress = MauMe.macFromMacStyleString(MauMe.myMacAddress);
  byte* truc = MauMe.getSha256BytesOfBytes((char*)MauMe.myByteAddress.dat, (const size_t)6);
  MauMe.MyAddress = MauMe.addressBuffer2MacFormat((char*)truc, MM_ADDRESS_SIZE);
  free(truc);
  MauMe.myByteAddress =  MauMe.addressFromMacStyleString(MauMe.MyAddress);
  MauMe.MyAddress.toUpperCase();
  MauMe.logFile = "/"+MauMe.myMacAddress+".log";
  #ifdef MAUME_DEBUG 
    Serial.println(" -> LOCAL ADDRESS : "+MauMe.MyAddress);  
  #endif
  #ifdef MAUME_ACTIVATE_WIFI
    MauMe.activate_Wifi();
    delay(250);
  #endif
  //---------------------------------------------
  // MauMe.nbPktsReceived = 0;
  // MauMe.nbPktsSent = 0;
  MauMe.activate_LoRa();
  delay(250); 
  //---------------------------------------------
  #ifdef MAUME_SETUP_DNS_SERVER   // #endif
    MauMe.activate_DNS();
    delay(250);
  #endif
  //----------------------------------------------
  #ifdef MAUME_SETUP_WEB_SERVER
    MauMe.setupWebServer();
    delay(250);
  #endif
  //-----------------------------------------------------------------------------------------------------------------------------------------------------
  #ifdef MAUME_DEBUG  // #endif RECVD
    listDir(SPIFFS, MauMe.spiMutex, "/", 0);
  #endif
  if(1){   //  Use this if you want to programmatically erase all files at startup.
    int dels = deleteDirectoryFiles(SPIFFS, MauMe.spiMutex, "/OUTBOX");   // Messages (packets) from this node (named from CRC32 of Message)
    dels += deleteDirectoryFiles(SPIFFS, MauMe.spiMutex,    "/INRCAS");   // RECACK (named from CRC32 of acknowledging Message)
    dels += deleteDirectoryFiles(SPIFFS, MauMe.spiMutex,    "/INBOX");    // Messages received (packets) (named from CRC32 of Message)
    dels += deleteDirectoryFiles(SPIFFS, MauMe.spiMutex,    "/PKTSOUT");  // Packets to send (named from first CRC32 of Packet)
    dels += deleteDirectoryFiles(SPIFFS, MauMe.spiMutex,    "/SHAOUT");  // SHACKs to send (SH-ACKS or RECACK to forward) (named from CRC32 of acknowledging packet or Message)
    dels += deleteDirectoryFiles(SPIFFS, MauMe.spiMutex,    "/RCAOUT");  // RECACKs to send (named as self) 
    dels += deleteDirectoryFiles(SPIFFS, MauMe.spiMutex,    "/SHASIN");  // Received SH-ACKS or RECACK to forward (named from CRC32 of acknowledging packet)
    dels += deleteDirectoryFiles(SPIFFS, MauMe.spiMutex,    "/PKTSREM");    // ACK reminders of Received Messages (named from CRC32 of acknowledging Message)
    dels += deleteDirectoryFiles(SPIFFS, MauMe.spiMutex,    "/RCAREM");    // ACK reminders of Received RECACKS // (named from CRC32 of acknowledging Message)
    
    dels += deleteDirectoryFiles(SPIFFS, MauMe.spiMutex,    "/RECVD");  // ACK reminders of received Packets (named from CRC32 of packet)
    dels += deleteDirectoryFiles(SPIFFS, MauMe.spiMutex,    "/ACKSREC");  // ACK reminders of Packets from this node (named from CRC32 of packet)
    dels += deleteDirectoryFiles(SPIFFS, MauMe.spiMutex,    "/FWRDD");    // ACK reminders of forwarded Messages (named from CRC32 of acknowledging Message)
    dels += deleteDirectoryFiles(SPIFFS, MauMe.spiMutex,    "/RECMES");   // 
    dels += deleteDirectoryFiles(SPIFFS, MauMe.spiMutex,    "/ACKSOUT");   // 
    dels += deleteDirectoryFiles(SPIFFS, MauMe.spiMutex,    "/SENT");     // ACK reminder of sent Packets (named from CRC32 of Packet)
    Serial.println(" -> Deleted files ok : "+String(dels)+".");    
  }
  Serial.println(" -> Deleting log file...");
  deleteFile(SPIFFS, MauMe.spiMutex, MauMe.logFile.c_str());
  #ifdef MAUME_DEBUG  // #endif PKTSREM
    listDir(SPIFFS, MauMe.spiMutex, "/", 0);
  #endif
  int ind = 0;
  int first = MauMe.readMem(ind++);
  if(first > 0){
    MauMe.DUMMY_PROCESS_STATIC_DELAY   = first;  
    MauMe.DUMMY_PROCESS_RANDOM_DELAY   = MauMe.readMem(ind++);
    MauMe.MM_PCKT_PROCESS_STATIC_DELAY = MauMe.readMem(ind++);
    MauMe.MM_PCKT_PROCESS_RANDOM_DELAY = MauMe.readMem(ind++);
    MauMe.CLEANING_PROCESS_STATIC_DELAY        =   MAUME_CLEAN_RATE*MauMe.MM_PCKT_PROCESS_STATIC_DELAY;  // milliseconds
    MauMe.CLEANING_PROCESS_RANDOM_DELAY        =   MAUME_CLEAN_RATE*MauMe.MM_PCKT_PROCESS_RANDOM_DELAY;  // milliseconds
    MauMe.MM_TX_POWER_MIN     = MauMe.readMem(ind++);
    MauMe.MM_TX_POWER_MAX     = MauMe.readMem(ind++);
    MauMe.MM_PCKT_SAVE_DELAY  = MauMe.readMem(ind++);
    MauMe.doAlternate         = (bool)MauMe.readMem(ind++);
    MauMe.percentActivity     = MauMe.readMem(ind++);
    MauMe.alternateInterval   = MauMe.readMem(ind++);
    MauMe.dummySendInterval   = MauMe.readMem(ind++);
    MauMe.nbDummyPkts         = MauMe.readMem(ind++);
    MauMe.sendDummies         = false; // (bool)
    MauMe.readMem(ind++);       
    MauMe.dummyTargetAddress  = MauMe.readMem2String(ind, MM_ADDRESS_SIZE);
  }
  //-----------------------------------------------------------------------------------------------------------------------------------------------------  
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
                    12288,
                    NULL,
                    1,
                    &MauMe.mauMeTask,
                    1);
  delay(250); 
  #ifdef MAUME_DEBUG   // #endif
    Serial.println(" -> MauMe Task set up!\n");    
  #endif
  MauMe.MauMeIsRunning = true;
  Serial.println("\n MauMe Node setup done @ "+MauMe.MyAddress+".");
  MauMe.Log(" * MauMe Node setup done @ "+MauMe.MyAddress+" ("+MauMe.myMacAddress+").");   
}

//--------------------------------------------------------------------------------------------------------------------------------------------------------
MauMeClass MauMe;
//--------------------------------------------------------------------------------------------------------------------------------------------------------
