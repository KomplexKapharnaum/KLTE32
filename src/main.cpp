#include <M5Stack.h>
#include <Preferences.h>

#include "LTEModule.h"
#include "TFTTerminal.h"

#include <ArduinoJson.h>

#define K32_SET_NODEID 1      // board unique id
#define K32_SET_CHANNEL 1     // board channel mqtt elp 1

#include "k32_loader.h"


TFT_eSprite Disbuff      = TFT_eSprite(&M5.Lcd);
TFT_eSprite TerminalBuff = TFT_eSprite(&M5.Lcd);
TFTTerminal terminal(&TerminalBuff);

String readstr;
String myNumber;

String URLSMSAPI = "https://relay.kxkm.net/relay/api/multisms";

unsigned long lastRSSI = 0;
bool relayMQTT = false;

void setup() 
{
    // put your setup code here, to run once:
    M5.begin();

    Disbuff.createSprite(320, 20);
    Disbuff.fillRect(0, 0, 320, 20, BLACK);
    Disbuff.drawRect(0, 0, 320, 20, Disbuff.color565(100, 100, 100));
    Disbuff.setTextColor(WHITE);
    Disbuff.setTextSize(1);
    Disbuff.setCursor(7, 7);
    Disbuff.printf("LTE Module");
    Disbuff.pushSprite(0, 0);

    TerminalBuff.createSprite(250, 220);
    TerminalBuff.fillRect(0, 0, 250, 220, BLACK);
    TerminalBuff.drawRect(0, 0, 250, 220, Disbuff.color565(36, 36, 36));
    TerminalBuff.pushSprite(0, 30);
    terminal.setGeometry(0, 30, 320, 210);

    k32_setup();

    // Heap Memory log
    k32->timer->every(1000, []() {
        static int lastheap = 0;
        int heap = ESP.getFreeHeap();
        LOGF2("Free memory: %d / %d\n", heap, heap - lastheap);
        lastheap = heap;
        if (heap < 50000) LOGF2("WARNING: free memory < 50K, new task might not properly start. %d / %d\n", heap, heap - lastheap);
    });

    // START LTE MODULE
    terminal.print("Starting ");
    LTEModule_init(5, 13);
    terminal.println("OK");

    // DISABLE ECHO
    terminal.print("Disable ECHO ");
    terminal.println( LTE_cmd("ATE0") ? "OK" : "ERROR" );

    // PIN
    terminal.print("PIN state ");
    terminal.println( LTE_cmd("AT+CPIN?", &readstr) ? readstr : "ERROR" );

    if ( argAt(readstr, 0, "+CPIN:") == "SIM PIN") 
    {
        terminal.print("Entering PIN ");
        terminal.println( LTE_cmd("AT+CPIN=1234") ? "OK" : "ERROR" );
    }

    // WAIT FOR NETWORK REGISTRATION
    terminal.print("Network Registration");
    while(1) {
        LTE_cmd("AT+CREG?", &readstr);
        if (argAt(readstr, 1) == "1") {
            terminal.println(" OK");
            break;
        }
        terminal.print('.');
    }

    // CHECK OPERATOR
    // terminal.print("Network: ");
    // terminal.println( LTE_cmd("AT+COPS?", &readstr) ? stringAt(readstr, 2) : "ERROR" );

    // CHECK NUMBER
    terminal.print("My Number: ");
    terminal.println( LTE_cmd("AT+CNUM", &readstr) ? argAt(readstr, 1) : "ERROR" );
    myNumber = argAt(readstr, 1);

    // SMS TEXT
    terminal.print("SMS mode text ");
    terminal.println( LTE_cmd("AT+CMGF=1") ? "OK" : "ERROR" );

    // SMS UNICODE
    terminal.print("SMS unicode ");
    terminal.println( LTE_cmd("AT+CSCS=\"UCS2\"") ? "OK" : "ERROR" );       
    //terminal.println( LTE_cmd("AT+CSCS=\"IRA\"") ? "OK" : "ERROR" );

    // SMS MODE
    terminal.print("SMS mode ");
    terminal.println( LTE_cmd("AT+CSMP=17,168,0,8") ? "OK" : "ERROR" );       
    //terminal.println( LTE_cmd("AT+CSMP=17,168,0,0") ? "OK" : "ERROR" ); 

    // SMS NOTIF
    terminal.print("SMS disable push notification ");
    terminal.println( LTE_cmd("AT+CNMI=0,0,0,0,0") ? "OK" : "ERROR" );  // original = 2,1,0,0,0

    // SMS CLEAR
    // terminal.print("SMS clear old messages ");
    // terminal.println( LTE_cmd("AT+CMGD=,3") ? "OK" : "ERROR" ); 

    delay(2000);

    // terminal.print("4G GPRS network ");
    // terminal.println( LTE_cmd("AT+CGACT?") ? "OK" : "ERROR" );

    // Local IP
    terminal.print("4G APN ");
    LTE_cmd("AT+CGDCONT=1,\"IP\",\"free\"");

    // Attach NEtwork
    terminal.print("4G Attach NEtwork ");
    terminal.println( LTE_cmd("AT+CGATT=1") ? "OK" : "ERROR" );

    // GPRS
    terminal.print("4G GPRS network ");
    terminal.println( LTE_cmd("AT+CGACT=1,1") ? "OK" : "ERROR" );

    // Con UP

    terminal.println("READY !");

}


void loop() 
{

    delay(100);
    M5.update();

    // RSSI
    if (millis() - lastRSSI > 10000) {
        terminal.println("CSQ rssi: " + (LTE_cmd("AT+CSQ", &readstr) ? argAt(readstr, 0) : "0") );
        lastRSSI = millis();
    }

    // DIAL
    if (M5.BtnA.wasPressed()) 
    {
        terminal.print("Calling NICO ");
        terminal.println( LTE_cmd("ATD0662064246;", &readstr) ? readstr : "ERROR" );
        // terminal.print("Send SMS mano ");
        // terminal.println( sendSMS( string2hex("0675471820") , string2hex("yo"), &readstr) ? readstr : "ERROR" );

        // terminal.print("Send SMS0 ");
        // terminal.println( sendSMS( string2hex("+33675471820") , 0, &readstr) ? readstr : "ERROR" );   // D83EDD50

        // terminal.println("send SMS 0 (+SMS0)");
        // if (mqtt && mqtt->isConnected()) 
        //     mqtt->publish("rpi/all/sms", (recallSMS(0)+"§UCS2").c_str());
        // else 
        //     terminal.println("-> MQTT not connected ");
    }

    // SMS
    if (M5.BtnB.wasPressed()) 
    {
        terminal.println("Clear RPI");
        if (mqtt && mqtt->isConnected()) {
            mqtt->publish("rpi/casa/textall", myNumber.c_str());
            mqtt->publish("rpi/mgr-ux/textall", myNumber.c_str());
        }
        else 
            terminal.println("-> MQTT not connected ");
    }

    // Get RSSI
    if (M5.BtnC.wasPressed()) 
    {
        relayMQTT = !relayMQTT;
        terminal.print("MQTT relay: ");
        terminal.println(relayMQTT ? "YES" : "NO");
    }

    // CHECK NEW MESSAGE
    pullSMS();
    

    // PROCESS RECEIVED MESSAGE
    if ( receivedSMS() ) 
    {
        // String data = "{\"from\": \"+33675471820\", \"text\": \"Yeah\"}";

        DynamicJsonDocument data( 200*receivedSMS() );
        data["enc"] = "UCS2";

        struct SMS msg;
        int count = 0;
        while ( recvSMS(&msg) )
        {
                //002B0030   // 002B003000200041

            String head = hex2string( msg.msg.substring(0, 24) );
            head.trim();
            head.toUpperCase();

            // Store Message
            if (head.startsWith("+SMS")) {
                int mem = head.substring(4, 6).toInt();
                String msgstore = msg.msg.substring(24, msg.msg.length());

                Serial.println("Storing "+String(mem)+" : "+hex2string(msgstore));
                storeSMS(msgstore, mem);

                terminal.print("Store ");    
                terminal.println( hex2string(msg.msg) );  
            }
            else 
            {
                terminal.print("New MSG from ");    
                terminal.println( hex2string(msg.from) );    
                terminal.println( hex2string(msg.msg) );

                // Append to Relay payload
                data["sms"][count]["from"] = msg.from;
                data["sms"][count]["text"] = msg.msg;

                count ++;

                // Forward to MQTT
                if (relayMQTT)
                    if (mqtt && mqtt->isConnected()) {
                        mqtt->publish("rpi/casa/textdispatch", (msg.msg+"§UCS2").c_str());
                        mqtt->publish("rpi/mgr-ux/textdispatch", (msg.msg+"§UCS2").c_str());
                    }
                    else terminal.println("-> MQTT not connected ");

                // Kick Back
                // terminal.print("-> answer ");
                // terminal.println( sendSMS(msg.from, string2hex("Loud and clear,\nmy dude"), &readstr) ? readstr : "ERROR" ); 

                
            }
            
        } 
        
        // Send to Relay
        String out;
        serializeJson(data, out);
        terminal.print("-> Relay 4G: ");
        bool relayPost = postJSON(URLSMSAPI, out);   // Send to https://relay.kxkm.net/relay/api/sms
        terminal.println((relayPost)?"OK":"FAILED");
    }
    
}