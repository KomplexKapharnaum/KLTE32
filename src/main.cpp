#include <M5Stack.h>
#include <Preferences.h>

#include "LTEModule.h"
#include "TFTTerminal.h"

#include <ArduinoJson.h>

TFT_eSprite Disbuff      = TFT_eSprite(&M5.Lcd);
TFT_eSprite TerminalBuff = TFT_eSprite(&M5.Lcd);
TFTTerminal terminal(&TerminalBuff);

String readstr;

String URLSMSAPI = "https://relay.kxkm.net/relay/api/multisms";

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
    // terminal.print("My Number: ");
    // terminal.println( LTE_cmd("AT+CNUM", &readstr) ? stringAt(readstr, 1) : "ERROR" );

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

    // DIAL
    if (M5.BtnA.wasPressed()) 
    {
        // terminal.print("Calling MGR ");
        // terminal.println( LTE_cmd("ATD0675471820;", &readstr) ? readstr : "ERROR" );
        terminal.print("Send SMS mano ");
        terminal.println( sendSMS( string2hex("0675471820") , string2hex("yo"), &readstr) ? readstr : "ERROR" );
    }

    // SMS
    if (M5.BtnB.wasPressed()) 
    {
        terminal.print("Send SMS0 ");
        terminal.println( sendSMS( string2hex("+33675471820") , 0, &readstr) ? readstr : "ERROR" );   // D83EDD50
    }

    // Get RSSI
    if (M5.BtnC.wasPressed()) 
    {
        terminal.println("CSQ rssi: " + (LTE_cmd("AT+CSQ", &readstr) ? argAt(readstr, 0) : "0") );
    }

    // CHECK NEW MESSAGE
    pullSMS();
    

    // PROCESS RECEIVED MESSAGE
    if ( receivedSMS() ) 
    {
        // String data = "{\"from\": \"+33675471820\", \"text\": \"Yeah\"}";

        DynamicJsonDocument data( 200*receivedSMS() );

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
            }
            else 
            {
                terminal.print("New MSG from ");    
                terminal.println( hex2string(msg.from) );    
                terminal.println( hex2string(msg.msg) );

                // Forward to Relay
                data["sms"][count]["from"] = hex2string(msg.from);
                data["sms"][count]["text"] = hex2string(msg.msg);

                // LTE_cmd("AT+HTTPINIT");
                // LTE_cmd("AT+HTTPPARA=\"URL\",\"https://relay.kxkm.net/relay/api/sms\"");
                // LTE_cmd("AT+HTTPACTION=0");
                // LTE_cmd("AT+HTTPTERM");

                // Kick Back
                // terminal.print("-> answer ");
                // terminal.println( sendSMS(msg.from, string2hex("Loud and clear,\nmy dude"), &readstr) ? readstr : "ERROR" ); 

                count ++;
            }
            
        } 
        
        String out;
        serializeJson(data, out);
        postJSON(URLSMSAPI, out);

    }
    

    // if (M5.BtnA.wasPressed()) {
    //     restate = AddMsg("ATD13800088888;");
    //     while ((readSendState(0) == kSendReady) ||
    //            (readSendState(0) == kSending) ||
    //            (readSendState(0) == kWaitforMsg))
    //         delay(50);
    //     Serial.printf("Read state = %d \n", readSendState(0));
    //     readstr = ReadMsgstr(0).c_str();
    //     Serial.print(readstr);
    //     while (1) {
    //         M5.update();
    //         if (M5.BtnA.wasPressed()) break;
    //         delay(100);
    //     }
    //     EraseFirstMsg();
    //     restate = AddMsg("AT+CHUP", kASSIGN_MO);
    // }

    // put your main code here, to run repeatedly:
}