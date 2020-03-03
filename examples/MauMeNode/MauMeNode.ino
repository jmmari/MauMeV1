/*
            MauMe LoRa Multi-Hops Messaging Example for Arduino
  
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
#include "MauMeV1.h"

void IRAM_ATTR onPacketDelivery(int nbPacketsInBox){
  // Do something to retrieve messages as fast as possible. Too long operations may trigger an ISR "Guru Meditation" error. 
  // If you want to free yourself from ISR (callbacks) issues, run a separate thread of your own to check for messages using 
  // either 'countReceivedMessages()' or 'getReceivedPackets()'.
  Serial.println("Received "+String(nbPacketsInBox) +" message(s).");
  LoRa_PKT * list = MauMe.getReceivedPackets();
  while(list){                                    // This should not be done here
    Serial.println(MauMe.pkt2Str(list) + "\n");   // This should not be done here
    list = MauMe.freeAndGetNext(list);            // This should not be done here
  }
}

void setup() {
  MauMe.setup(115200); // Setup MauMe with serial speed. Everything eles is automated.
  MauMe.oledDisplay(false); // This is configured for Heltec LoRa V2 board. Use according to requirements.
  MauMe.onDelivery(onPacketDelivery);
  //
  // put your setup code here, to run once:

}

void loop() {
  // put your main code here, to run repeatedly:

}
