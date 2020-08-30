# MauMeV1
MauMe LoRa Multi-Hops Messaging for Arduino
(Develloped on [Heltec LoRa](https://heltec.org/project/wifi-lora-32/) [V2](https://heltec.org/wp-content/uploads/2020/04/SAM_0748_800X800.png))

![https://github.com/jmmari/MauMeV1](https://github.com/jmmari/MauMeV1/blob/master/data/WWW/Logo-MMM-color.png?raw=true)

This library allows creating LoRa nodes executing your code for your application, using a Multi-Hop collaborative LoRa messaging protocol. 

![https://github.com/jmmari/MauMeV1](https://github.com/jmmari/MauMeV1/blob/master/Figures/Figure1.png?raw=true)

Please use the example to get started. Just copy the libray in your Arduino/librairies directory.

Your nodes can use this to exchange messages up to 205 bytes long. Each node acts as a relay forwarding messages from all other MauMe nodes using the same LoRa settings. Do not modify those settings if you want to benefit from the other users'nodes. 

You should also consider leaving your nodes active as long as possible: the less a node sleeps, the more it can relay messages from (your ?) other nodes. Run your well powered nodes in relay mode (comment MAUME_BEHAVE_AS_TERMINAL), with MAUME_HOP_ON_TX commented so that relayed messages will not be deleted until hoped to another node to increase items lifetime. 

You should call the "MauMe.sleepMauMe()" method before going to deep-sleep in non-terminal mode, to ensure saving the current parameters and the last received packets.

You can operate your node as:
- Relay and Terminal node: accepts MauMe packets and forwards them (comment line MAUME_BEHAVE_AS_TERMINAL).
- Terminal node (uncomment MAUME_BEHAVE_AS_TERMINAL): does not accept messages from others, only transmits (you should call the processPackets method yourself if you want to skip the random timing transmission delay after posting a message).

If you run your node in relay mode, you should de-activate TTL decrease on transmit (comment MAUME_HOP_ON_TX), and take advantage of the MauMe SMS WiFi http interface (http://maume or http://1.1.1.1):

![https://github.com/jmmari/MauMeV1](https://github.com/jmmari/MauMeV1/blob/master/Figures/Figure11.png?raw=true) ![https://github.com/jmmari/MauMeV1](https://github.com/jmmari/MauMeV1/blob/master/Figures/Figure15.png?raw=true)

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
    
Thanks and respects to the people who wrote the routines this library is using ! Their work remains theirs' !

![https://github.com/jmmari/MauMeV1](https://github.com/jmmari/MauMeV1/blob/master/Figures/Figure5.png?raw=true)
