#include <WiFi.h>
#include <WiFiUdp.h>
#include <AsyncUDP.h>

// WiFi network name and password:
const char * networkName = "jumpnrun";
const char * networkPswd = "raspberry";

// IP address to send UDP data to:
// either use the ip address of the server or
// a network broadcast address

const char * udpAddress = "255.255.255.255";
const char * udpBroadcast = "255.255.255.255";
const int udpPort = 2209;

const char * cmdJNRS_ACKNOWLEDGE = "JNRS:DISCOVER:ACK\0";
const char * cmdJNRS_MODE_PUSH = "JNRS:MODE:PUSH\0";
const char * cmdJNRS_MODE_JUMP = "JNRS:MODE:JUMP\0";
const char * cmdJNRS_MODE_SPLT = "JNRS:MODE:SPLT\0";
const char * cmdJNRS_MODE_WALK = "JNRS:MODE:WALK\0";
const char * cmdJNRS_MODE_LEAN = "JNRS:MODE:LEAN\0";

enum ControllerMode { NONE, PUSH, JUMP, SPLT, WALK, LEAN };
ControllerMode cm = NONE;

// Are we currently connected?

boolean connected = false;
boolean connecting = false;
boolean wifiEventHandlerRegistered = false;

int lastSensorValueLEFT = 32767;
int lastSensorValueRIGHT = 32767;
int sensorValueLEFT = 0;
int sensorValueRIGHT = 0;
int heartbeat = 7000;
int cycles = 0;

boolean downUnlocked = false;
int downLimit = 2500;
int downCycles = 0;

bool pressed = false;
bool pressedL = false;
bool pressedR = false;
bool pressedD = false;
bool bindRequested = false;
String stringChipId;

const char * packetPressLeft;
const char * packetReleaseLeft;

const char * packetPressRight;
const char * packetReleaseRight;

const char * packetPressJump;
const char * packetReleaseJump;

const char * packetPressPush;
const char * packetReleasePush;

const char * packetPressDown;
const char * packetReleaseDown;

const char * packetHeartbeat;

// The udp library class

WiFiUDP udp;
AsyncUDP adp;

// the setup routine runs once when you press reset:
void setup() 
{
  // initialize serial communication at 115200 bits per second:
  Serial.begin(115200);
  Serial.println("Rise and shine, Mr. Anderson!");
  stringChipId = getMacAddress();
  packetPressJump = "JNRC:J\0";
  packetReleaseJump = "JNRC:j\0";

  packetPressPush = "JNRC:P\0";
  packetReleasePush = "JNRC:p\0";

  packetPressLeft = "JNRC:L\0";
  packetReleaseLeft = "JNRC:l\0";

  packetPressRight = "JNRC:R\0";
  packetReleaseRight = "JNRC:r\0";

  packetPressDown = "JNRC:D\0";
  packetReleaseDown = "JNRC:d\0";

  packetHeartbeat = ".\0";

  Serial.println("CHIPID: " + stringChipId);
  connecting = true;
  connectToWiFi(networkName, networkPswd);

  // initial reading
  lastSensorValueRIGHT = analogRead(A7);
  lastSensorValueLEFT = analogRead(A6);

  // DEBUG
  ////cm = SPLT;
}

// the loop routine runs over and over again forever:
void loop() 
{
  // run sensorLoop after parameters have been set by server
  if(cm != NONE)
  {
    sensorLoop();
    if (connected) 
    {
      cycles++;
      if (cycles >= heartbeat) 
      {
        cycles = 0;
        udp.beginPacket(udpAddress, udpPort);
        udp.printf("%s", packetHeartbeat);
        udp.endPacket();
      }
    }
    else
    {
      if (connecting == false)
      {
        connecting = true;
        connectToWiFi(networkName, networkPswd);
      }
    }
    delay(2);        // delay in between reads for stability
  }

}

void connectToWiFi(const char * ssid, const char * pwd)
{
  Serial.println("Connecting to WiFi network: " + String(ssid));

  while(1) 
  {

    Serial.println("Connecting to WiFi ...");

    // delete old config
    WiFi.disconnect(true);

    if (wifiEventHandlerRegistered == false)
    {
      // register event handler
      WiFi.onEvent(WiFiEvent);
      wifiEventHandlerRegistered = true;
    }

    // Initiate connection
    WiFi.begin(ssid, pwd);

    Serial.println("Waiting for WIFI connection...");

    if (WiFi.waitForConnectResult() != WL_CONNECTED) 
    {
          Serial.println("WiFi Failed - retrying in 10 seconds");
              delay(10000);
    }
    else
    {
      if(adp.listen(2210)) 
      {
        Serial.print("UDP Listening on IP: ");
        Serial.println(WiFi.localIP());
        adp.onPacket([](AsyncUDPPacket packet) 
        {
            // Convert the packet data char array into a string to allow
            // for comparing stuff

            char cmd[packet.length() + 1];
            memcpy(cmd, packet.data(), sizeof(cmd));
            cmd[packet.length()] = '\0';

            /* Debugging feature disabled */

            Serial.print("RECEIVED: ");
            Serial.println(cmd);

            if(strcmp((char *) cmd, cmdJNRS_ACKNOWLEDGE) == 0) {
              Serial.print("Discovered JNR Server: ");
              Serial.println(packet.remoteIP());
              Serial.println("Asking server for configuration... please stand by");

              udp.beginPacket(packet.remoteIP(), udpPort);
              udp.printf("JNRC:CONFIGURE\0");
              udp.endPacket();
            }
           else if(strcmp((char *) cmd, cmdJNRS_MODE_PUSH) == 0) {
              Serial.println("Configuration applied: MODE: PUSH");
              cm = PUSH;

              udp.beginPacket(packet.remoteIP(), udpPort);
              udp.printf("JNRC:MODE:ACK\0");
              udp.endPacket();
            }
            else if(strcmp((char *) cmd, cmdJNRS_MODE_JUMP) == 0) {
              Serial.println("Configuration applied: MODE: JUMP");
              cm = JUMP;

              udp.beginPacket(packet.remoteIP(), udpPort);
              udp.printf("JNRC:MODE:ACK\0");
              udp.endPacket();
            }
            else if(strcmp((char *) cmd, cmdJNRS_MODE_SPLT) == 0) {
              Serial.println("Configuration applied: MODE: SPLT");
              cm = SPLT;

              udp.beginPacket(packet.remoteIP(), udpPort);
              udp.printf("JNRC:MODE:ACK\0");
              udp.endPacket();
            }
            else if(strcmp((char *) cmd, cmdJNRS_MODE_WALK) == 0) {
              Serial.println("Configuration applied: MODE: WALK");
              cm = WALK;

              udp.beginPacket(packet.remoteIP(), udpPort);
              udp.printf("JNRC:MODE:ACK\0");
              udp.endPacket();
            }
            else if(strcmp((char *) cmd, cmdJNRS_MODE_LEAN) == 0) {
              Serial.println("Configuration applied: MODE: LEAN");
              cm = LEAN;

              udp.beginPacket(packet.remoteIP(), udpPort);
              udp.printf("JNRC:MODE:ACK\0");
              udp.endPacket();
            }

            memset(cmd, 0, sizeof(cmd));
        });

        if(bindRequested == false)
        {
          Serial.println("Discovering JNR Server...");
          udp.beginPacket(udpBroadcast, udpPort);
          udp.printf("JNRC:DISCOVER\0");
          udp.endPacket();
          bindRequested = true;
        }

      }
      
      // Okay, we are now connected to WiFi. Leave the connection loop.
      break;

    }
  }
}

//wifi event handler
void WiFiEvent(WiFiEvent_t event) 
{
  switch (event) 
  {
        case SYSTEM_EVENT_WIFI_READY:
            Serial.println("WiFi interface ready");
            break;

        case SYSTEM_EVENT_SCAN_DONE:
            Serial.println("Completed scan for access points");
            break;

        case SYSTEM_EVENT_STA_START:
            break;

        case SYSTEM_EVENT_STA_STOP:
            Serial.println("WiFi client stopped");
            connecting = false;
            break;

        case SYSTEM_EVENT_STA_CONNECTED:
            Serial.println("Connected to access point");
            connecting = false;
            break;

        case SYSTEM_EVENT_STA_AUTHMODE_CHANGE:
            Serial.println("Authentication mode of access point has changed");
            connecting = false;
            break;

        case SYSTEM_EVENT_STA_GOT_IP:

          Serial.print("WiFi connected! IP address: ");
          Serial.println(WiFi.localIP());

          //initializes the UDP state
          //This initializes the transfer buffer
          udp.begin(WiFi.localIP(), udpPort);
          connected = true;
          break;

        case SYSTEM_EVENT_STA_DISCONNECTED:
          Serial.println("WiFi lost connection");
          connecting = false;
          connected = false;
          break;
  }
}

void sensorLoop() 
{
    sensorValueRIGHT = analogRead(A6);
    sensorValueLEFT = analogRead(A7);

    //if (sensorValueLEFT != lastSensorValueLEFT || sensorValueRIGHT != lastSensorValueRIGHT)
    //{
        //Serial.println("FSR -- L:" + (String)sensorValueLEFT + " R:" + (String)sensorValueRIGHT);
    //}

    switch(cm)
    {
      case NONE:
        Serial.println("Input ignored. Controllermode not set.");
        break;

      case JUMP:
        detectJump();
        break;

      case PUSH:
        detectPush();
        break;

      case SPLT:
        detectSplit();
        break;

      case LEAN:
        detectLean();
        break;
    }

    // memorize
    lastSensorValueLEFT = sensorValueLEFT;
    lastSensorValueRIGHT = sensorValueRIGHT;

}

// DETECTION: SPLIT
//            left and right sensor arrays are treated individually
//
//            TABLE OF TRUTH:
//
//              left sensor down, right sensor up:            SEND LEFT DOWN
//              - left sensor up again, right sensor up:      SEND LEFT UP
//              - left sensor still down, right sensor down:  SEND LEFT UP
//
//              right sensor down, left sensor up:            SEND RIGHT DOWN
//              - right sensor up again, left sensor up:      SEND RIGHT UP
//              - right sensor still down, left sensor down:  SEND RIGHT UP
//
//              both sensors up: -
//
//              both sensors down:                            SEND CURSOR-DOWN DOWN
//

void detectSplit() 
{
  //Serial.println("VELOSTAT -- L:" + (String)sensorValueLEFT + " R:" + (String)sensorValueRIGHT);

  // ====================================================================================
  // ONLY LEFT SENSOR IS BEING PRESSED
  // ====================================================================================

  if (sensorValueLEFT >= 2000 && sensorValueRIGHT <= 2000 )
  {
    if (connected)
    {
      if (pressedR == true)
      {
        udp.beginPacket(udpAddress, udpPort);
        udp.printf("%s", packetReleaseRight);
        udp.endPacket();
        pressedR = false;
        Serial.println("LEFT PUSH DETECTED / RELEASING RIGHT");
      }
      if (pressedL == false)
      {
        udp.beginPacket(udpAddress, udpPort);
        udp.printf("%s", packetPressLeft);
        udp.endPacket();
        pressedL = true;
        Serial.println("LEFT PUSH DETECTED");
      }
      if (pressedD == true)
      {
        udp.beginPacket(udpAddress, udpPort);
        udp.printf("%s", packetReleaseDown);
        udp.endPacket();
        pressedD = false;
        downCycles = 0;
        Serial.println("RIGHT PUSH DETECTED / RELEASING DOWN");
      }
    }
    else
    {
      Serial.println("Sending: PUSH LEFT! -- not connected");
    }
  }
  else if (sensorValueLEFT <= 2000)
  {
    if (connected)
    {
      if (pressedL == true)
      {
        udp.beginPacket(udpAddress, udpPort);
        udp.printf("%s", packetReleaseLeft);
        udp.endPacket();
        pressedL = false;
        Serial.println("RELEASING LEFT");
      }
    }
    else
    {
      Serial.println("Sending: RELEASE LEFT! -- not connected");
    }
  }

  // ====================================================================================
  // ONLY RIGHT SENSOR IS BEING PRESSED
  // ====================================================================================

  if (sensorValueRIGHT >= 2000 && sensorValueLEFT <= 2000 )
  {
    if (connected)
    {
      if (pressedL == true)
      {
        udp.beginPacket(udpAddress, udpPort);
        udp.printf("%s", packetReleaseLeft);
        udp.endPacket();
        pressedL = false;
        Serial.println("RIGHT PUSH DETECTED / RELEASING LEFT");
      }
      if (pressedR == false)
      {
        udp.beginPacket(udpAddress, udpPort);
        udp.printf("%s", packetPressRight);
        udp.endPacket();
        pressedR = true;
        Serial.println("RIGHT PUSH DETECTED");
      }
      if (pressedD == true)
      {
        udp.beginPacket(udpAddress, udpPort);
        udp.printf("%s", packetReleaseDown);
        udp.endPacket();
        pressedD = false;
        downCycles = 0;
        Serial.println("RIGHT PUSH DETECTED / RELEASING DOWN");
      }
    }
    else
    {
      Serial.println("Releasing: RIGHT PUSH! -- not connected");
    }
  }
  else if (sensorValueRIGHT <= 2000)
  {
    if (connected)
    {
      if (pressedR == true)
      {
        udp.beginPacket(udpAddress, udpPort);
        udp.printf("%s", packetReleaseRight);
        udp.endPacket();
        pressedR = false;
        Serial.println("RELEASING RIGHT");
      }
    }
    else
    {
      Serial.println("Sending: RELEASE RIGHT! -- not connected");
    }
  }

  // ====================================================================================
  // BOTH SENSORS ARE BEING PRESSED
  // ====================================================================================

  if (sensorValueLEFT >= 2000 && sensorValueRIGHT >= 2000)
  {
      if (pressedL == true)
      {
        udp.beginPacket(udpAddress, udpPort);
        udp.printf("%s", packetReleaseLeft);
        udp.endPacket();
        pressedL = false;
        Serial.println("STALE SITUATION / RELEASING LEFT");
      }
      if (pressedR == true)
      {
        udp.beginPacket(udpAddress, udpPort);
        udp.printf("%s", packetReleaseRight);
        udp.endPacket();
        pressedR = false;
        Serial.println("STALE SITUATION / RELEASING RIGHT");
      }
      if (pressedD == true)
      {
        udp.beginPacket(udpAddress, udpPort);
        udp.printf("%s", packetReleaseDown);
        udp.endPacket();
        pressedD = false;
        downCycles = 0;
        Serial.println("RIGHT PUSH DETECTED / RELEASING DOWN");
      }
      if (downUnlocked == false)
      {
        // We have a stable stance on the pad now.
        // Unlock the "cursor down" feature.
        downUnlocked = true;
        Serial.println("STABLE STANCE DETECTED. UNLOCKING DOWN.");
      }
  }

  // ====================================================================================
  // NO SENSOR IS BEING PRESSED
  // ====================================================================================

  if (sensorValueLEFT <= 2000 && sensorValueRIGHT <= 2000)
  {
      if (pressedL == true)
      {
        udp.beginPacket(udpAddress, udpPort);
        udp.printf("%s", packetReleaseLeft);
        udp.endPacket();
        pressedL = false;
        Serial.println("PAD ABANDONED / RELEASING LEFT");
      }
      if (pressedR == true)
      {
        udp.beginPacket(udpAddress, udpPort);
        udp.printf("%s", packetReleaseRight);
        udp.endPacket();
        pressedR = false;
        Serial.println("PAD ABANDONED / RELEASING RIGHT");
      }
      if (pressedD == false)
      {
        if (downUnlocked == true)
        {
          downCycles = 0;
          udp.beginPacket(udpAddress, udpPort);
          udp.printf("%s", packetPressDown);
          udp.endPacket();
          pressedD = true;
          Serial.println("PAD ABANDONED / PRESSING DOWN");
        }
      }
      else
      {
        if (downCycles < downLimit)
        {
          downCycles++;
        }
        else
        {
          udp.beginPacket(udpAddress, udpPort);
          udp.printf("%s", packetReleaseDown);
          udp.endPacket();
          pressedD = false;
          downUnlocked = false;
          downCycles = 0;
          Serial.println("DOWN TTL REACHED / RELEASING (and locking) DOWN");
        }
      }
  }
}


// DETECTION: LEAN
//            left and right sensor arrays are treated individually
//
//            TABLE OF TRUTH:
//
//              left sensor force > right sensor:             SEND LEFT DOWN
//              right sensor force > left sensor:             SEND RIGHT DOWN
//              both sensors do not exceed the minimum delta: RELEASE LEFT/RIGHT IF PRESSED
//

void detectLean()
{
  int minimumDelta = 600;
  Serial.println("FSR -- L:" + (String)sensorValueLEFT + " R:" + (String)sensorValueRIGHT);

  if (sensorValueLEFT >= sensorValueRIGHT + minimumDelta)
  {
      if (pressedR == true)
      {
        udp.beginPacket(udpAddress, udpPort);
        udp.printf("%s", packetReleaseRight);
        udp.endPacket();
        pressedR = false;
        Serial.println("LEFT LEAN DETECTED / RELEASING RIGHT");
      }

      if (pressedL == false)
      {
        udp.beginPacket(udpAddress, udpPort);
        udp.printf("%s", packetPressLeft);
        udp.endPacket();
        pressedL = true;
        Serial.println("LEFT LEAN DETECTED / PRESSING LEFT");
      }
  }
  else if (sensorValueRIGHT >= sensorValueLEFT + minimumDelta)
  {
    if (pressedL == true)
    {
      udp.beginPacket(udpAddress, udpPort);
      udp.printf("%s", packetReleaseLeft);
      udp.endPacket();
      pressedL = false;
      Serial.println("RIGHT LEAN DETECTED / RELEASING LEFT");
    }

    if (pressedR == false)
    {
      udp.beginPacket(udpAddress, udpPort);
      udp.printf("%s", packetPressRight);
      udp.endPacket();
      pressedR = true;
      Serial.println("RIGHT LEAN DETECTED / PRESSING RIGHT");
    }
  }
  else
  {
    if (pressedL == true)
    {
      udp.beginPacket(udpAddress, udpPort);
      udp.printf("%s", packetReleaseLeft);
      udp.endPacket();
      pressedL = false;
      Serial.println("LEFT LEAN ENDED / RELEASING LEFT");
    }

    if (pressedR == true)
    {
      udp.beginPacket(udpAddress, udpPort);
      udp.printf("%s", packetReleaseRight);
      udp.endPacket();
      pressedR = false;
      Serial.println("RIGHT LEAN ENDED / RELEASING RIGHT");
    }
  }
}

// DETECTION: JUMP
//            both sensors are treated as a single input
//
//            TABLE OF TRUTH:
//
//              both sensors down:                            JUMP MODE ARMED
//              - both sensors up again:                      SEND SINGLE DOWN
//              - any sensor down again:                      SEND SINGLE UP
//
//              any other input:                              -
//

void detectJump()
{
  if (sensorValueLEFT <= 3000 && sensorValueRIGHT <= 3000 )
  {
    if (connected)
    {
      if (pressed == false)
      {
        udp.beginPacket(udpAddress, udpPort);
        udp.printf("%s", packetPressJump);
        udp.endPacket();
        pressed = true;
        Serial.println("JUMP DETECTED");
      }
    }
    else
    {
      Serial.println("Sending: JUMP! -- not connected");
    }
  }
  else if (sensorValueLEFT >= 3000 || sensorValueRIGHT >= 3000)
  {
    if (connected)
    {
      if (pressed == true)
      {
        // send
        if (connected) 
        {
          udp.beginPacket(udpAddress, udpPort);
          udp.printf("%s", packetReleaseJump);
          udp.endPacket();
          pressed = false;
          Serial.println("RELEASE"); 
        }
      }
    }
    else
    {
      Serial.println("Releasing: JUMP! -- not connected");
    }
  }
}

// DETECTION: PUSH
//            both sensors are treated as a single input
//
//            TABLE OF TRUTH:
//
//              any sensor down:                              SEND SINGLE DOWN
//              - any sensor up again:                        SEND SINGLE UP
//
//              any other input:                              -
//

void detectPush()
{
  if (lastSensorValueLEFT <= 2000 && sensorValueLEFT >= 2000 || lastSensorValueRIGHT <= 2000 && sensorValueRIGHT >= 2000 )
  {
    if (connected)
    {
      if (pressed == false)
      {      
        udp.beginPacket(udpAddress, udpPort);
        udp.printf("%s", packetPressPush);
        udp.endPacket();
        pressed = true;
        Serial.println("PUSH DETECTED"); 
      }
    }
    else
    {
      Serial.println("Sending: PUSH! -- not connected");
    }
  }
  else if (sensorValueLEFT <= 2000 && sensorValueRIGHT <= 2000)
  {
    if (connected)
    {
      if (pressed == true)
      {
        // send
        if (connected) {
          udp.beginPacket(udpAddress, udpPort);
          udp.printf("%s", packetReleasePush);
          udp.endPacket();
          pressed = false;
          Serial.println("RELEASE");
        }
      }
    }
    else
    {
      Serial.println("Releasing: PUSH! -- not connected");
    }
  }
}

String getMacAddress() 
{
    uint8_t baseMac[6];
    // Get MAC address for WiFi station
    esp_read_mac(baseMac, ESP_MAC_WIFI_STA);
    char baseMacChr[18] = {0};
    sprintf(baseMacChr, "%02X%02X%02X%02X%02X%02X", baseMac[0], baseMac[1], baseMac[2], baseMac[3], baseMac[4], baseMac[5]);
    return String(baseMacChr);
}
