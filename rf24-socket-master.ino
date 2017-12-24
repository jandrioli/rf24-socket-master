/*
 Copyright (C) 2011 James Coliz, Jr. <maniacbug@ymail.com>

 This program is free software; you can redistribute it and/or
 modify it under the terms of the GNU General Public License
 version 2 as published by the Free Software Foundation.
 */

/**
 * Example using Dynamic Payloads 
 *
 * This is an example of how to use payloads of a varying (dynamic) size. 
 */

#include <SPI.h>
#include "printf.h"
#include "RF24.h"

//
// Hardware configuration
//

// Set up nRF24L01 radio on SPI bus plus pins 8 & 9
RF24 radio(9,10);

// Radio pipe addresses for the 2 nodes to communicate.
// WARNING!! 3Node and 4Node are used by my testing sketches ping/pong
const uint8_t addresses[][5] = {
  "0Node", // master writes broadcasts here
  "1Node", // unor3 writes here
  "2Node", // unor3 reads here
  "3Node", // arrosoir reads here
  "4Node", // arrosoir writes here
  "5Node"};// not yet used by anybody

  
/**
 * exchange data via radio more efficiently with data structures.
 * we can exchange max 32 bytes of data per msg. 
 * schedules are reset every 24h (last for a day) so an INTEGER is
 * large enough to store the maximal value of a 24h-schedule.
 * temperature threshold is rarely used
 */
struct relayctl {
  unsigned long uptime = 0;                      // current running time of the machine (millis())  4 bytes  
  unsigned long sched1 = DEFAULT_ACTIVATION;     // schedule in minutes for the relay output nbr1   4 bytes
  unsigned long sched2 = DEFAULT_ACTIVATION;     // schedule in minutes for the relay output nbr2   4 bytes
  unsigned int  maxdur1 = DEFAULT_DURATION;      // max duration nbr1 is ON                         2 bytes
  unsigned int  maxdur2 = DEFAULT_DURATION;      // max duration nbr2 is ON                         2 bytes
  unsigned int  temp_thres = 999;                // temperature at which the syatem is operational  4 bytes
  float         temp_now   = 20;                 // current temperature read on the sensor          4 bytes
  short         battery    =  0;                 // current temperature read on the sensor          2 bytes
  bool          state1 = false;                  // state of relay output 1                         1 byte
  bool          state2 = false;                  // "" 2                                            1 byte
  bool          waterlow = false;                // indicates whether water is low                  1 byte
  byte          nodeid = THIS_NODE_ID;           // nodeid is the identifier of the slave           1 byte
} myData;

void setup(void)
{

  //
  // Print preamble
  //
  Serial.begin(115200);
  Serial.println(F("RF24 Master - power socket controller"));  
  
  //
  // Setup and configure rf radio
  //
  radio.begin();
  radio.setCRCLength( RF24_CRC_16 ) ;
  // optionally, increase the delay between retries & # of retries
  radio.setRetries( 15, 5 ) ;
  radio.setAutoAck( true ) ;
  radio.setPALevel( RF24_PA_MAX ) ;
  radio.setDataRate( RF24_250KBPS ) ;
  radio.setChannel( 108 ) ;
  radio.enableDynamicPayloads(); //dont work with RPi :-/

  //
  // Open pipes to other nodes for communication
  //
  // This simple sketch opens two pipes for these two nodes to communicate
  // back and forth.
  // Open 'our' pipe for writing
  // Open the 'other' pipe for reading, in position #1 (we can have up to 5 pipes open for reading)
  radio.openWritingPipe(pipes[1]);
  radio.openReadingPipe(1,pipes[0]);


  //
  // Dump the configuration of the rf unit for debugging
  //
  printf_begin();
  radio.printDetails();
  
  //
  // Start listening
  //
  radio.powerUp() ;
  radio.startListening();
}

void loop(void)
{
  bool timeout = false;
  unsigned long started_waiting_at = 0;

  if (Serial.available())
  {
    String s1 = Serial.readString();
    if (s1.indexOf("start")>=0)
    {
      String sched = s1.substring(s1.indexOf(" ")+1);
      long seconds = 1;
      if (sched.indexOf("h")>0)
      {
        seconds = sched.substring(0, sched.indexOf("h")).toInt() * 3600 ;
        sched = sched.substring(sched.indexOf("h")+1);
      }
      if (sched.indexOf("min")>0)
      {
        seconds = seconds + (sched.substring(0, sched.indexOf("min")).toInt() * 60);
        Serial.println(seconds);
        sched = sched.substring(0, sched.indexOf("min"));
        Serial.println(sched);
      }
      if (seconds > 1)
      {
        s1 = "activate1 " + String(seconds);
      }
    }
    else if ((s1.indexOf("stop")==0) && (s1.indexOf("status")==0))
    {
      Serial.println("** Serial interface expects:\n\r"\
        "** 0 - start1: activate relay1 after x time\n\r"\
        "** 0 - start2: activate relay2 after x time\n\r"\
        "** 1 - stop: deactivate everything and reset all counters\n\r"\
        "** 2 - status: returns the current status of relays and counters");
      s1 = "";
    }
    
    if (s1 != "")
    {
      Serial.println("OUT: " + s1);
      char qS[s1.length()+1] ;
      s1.toCharArray(qS, s1.length()+1);
      qS[s1.length()+1] = '\0';

      
      // First, stop listening so we can talk.
      radio.stopListening();
      if (queryString(qS, s1.length()+1, s1))
      {
        Serial.println(s1);
      } 
      // restart listening
      radio.startListening();
    }
  }
}

/*
 * This function sends out a message (theQuery) and expects
 * to receive a response with a number (return value). 
 * It will send the query, wait for 500ms, and return. 
 */
bool queryString(char theQuery[], uint8_t queryLength, String& oValue)
{

  // send query
  radio.write( theQuery, queryLength, false );

  // Now, continue listening
  radio.startListening();

  // Wait here until we get a response, or timeout
  unsigned long started_waiting_at = millis();
  bool timeout = false;
  while ( ! radio.available() && ! timeout )
    if (millis() - started_waiting_at > 500 )
      timeout = true;

  // Describe the results
  if ( timeout )
  {
     Serial.println(F("Failed, response timed out."));
     return false;
  }
  else
  {
    while (radio.available())
    {
      // Fetch the payload, and see if this was the last one.
      uint8_t len = radio.getDynamicPayloadSize();
      char* rx_data = NULL;
      rx_data = (char*)calloc(len, sizeof(char));
      radio.read( rx_data, len );
      
      // Put a zero at the end for easy printing
      rx_data[len] = 0;
      
      // Spew it
      /*Serial.print(F("Got msg size="));
      Serial.print(len);
      Serial.print(F(" value="));
      Serial.println(rx_data);*/
      
      oValue += String(rx_data);
      free(rx_data);
    }
  }
  return true;
}
