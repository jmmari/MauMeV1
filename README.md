# MauMeV1
MauMe LoRa Multi-Hops Messaging for Arduino

This library allows creating LoRa nodes executing your code for your application, using a Multi-Hop collaborative LoRa messaging protocol. 

Please use the example to get started.

Your nodes can use this to exchange messages up to 205 bytes long. Each node acts as a relay forwarding messages from all other MauMe nodes using the same LoRa settings. Do not modify those settings if you want to benefit from the other users'nodes. 

You should also consider leaving your nodes active as long as possible: the less a node sleeps, the more it can relay messages from your other nodes.

Please call "sleepMauMe()" method before going to deep-sleep in non-terminal mode, to ensure saving the last received packets.

______________________________________________________________________________________________
 MauMe LoRa Multi-Hops Messaging for Arduino
  
  Copyright (c) 2020 Jean Martial Mari & Alban Gabillon, University of French Polynesia. 
  All rights reserved.
  
  ![www.upf.pf](http://www.upf.pf/themes/upf/images/logo.png)      
 
    This library is free software; you can redistribute it and/or
    modify it under the terms of the GNU Lesser General Public
    License as published by the Free Software Foundation; either
    version 2.1 of the License, or (at your option) any later version.
    This library is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
    Lesser General Public License for more details.  
