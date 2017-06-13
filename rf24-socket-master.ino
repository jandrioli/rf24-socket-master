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

// Use multicast?
// sets the multicast behavior this unit in hardware.  Connect to GND to use unicast
// Leave open (default) to use multicast.
const int multicast_pin = 6 ;

// sets the role of this unit in hardware.  Connect to GND to be the 'pong' receiver
// Leave open to be the 'ping' transmitter
const int role_pin = 7;
bool multicast = true ;

//
// Topology
//

// Radio pipe addresses for the 2 nodes to communicate.
//const uint64_t pipes[2] = { 0xEEFAFDFDEELL, 0xEEFDFAF50DFLL };
const uint8_t pipes[][6] = {"1Node","2Node"};

void setup(void)
{
  //
  // Multicast
  //
  pinMode(multicast_pin, INPUT);
  digitalWrite(multicast_pin,HIGH);
  delay( 20 ) ;

  // read multicast role, LOW for unicast
  if( digitalRead( multicast_pin ) )
    multicast = true ;
  else
    multicast = false ;

  //
  // Print preamble
  //
  Serial.begin(115200);
  Serial.println(F("RF24 Master - power socket controller"));  
  Serial.print(F("MULTICAST: "));
  Serial.println(multicast ? F("true (unreliable)") : F("false (reliable)"));
  
  //
  // Setup and configure rf radio
  //
  radio.begin();
  radio.setCRCLength( RF24_CRC_16 ) ;
  // optionally, increase the delay between retries & # of retries
  radio.setRetries( 15, 5 ) ;
  radio.setAutoAck( true ) ;
  //radio.setPALevel( RF24_PA_LOW ) ;
  //radio.enableDynamicPayloads(); //dont work with RPi :-/

  //
  // Open pipes to other nodes for communication
  //
  // This simple sketch opens two pipes for these two nodes to communicate
  // back and forth.
  // Open 'our' pipe for writing
  // Open the 'other' pipe for reading, in position #1 (we can have up to 5 pipes open for reading)
  radio.openWritingPipe(pipes[0]);
  radio.openReadingPipe(1,pipes[1]);


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
  radio.write( theQuery, queryLength, multicast );

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
