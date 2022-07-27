#include <M5Stack.h>

#include "LTEModule.h"
#include "TFTTerminal.h"

TFT_eSprite Disbuff      = TFT_eSprite(&M5.Lcd);
TFT_eSprite TerminalBuff = TFT_eSprite(&M5.Lcd);
TFTTerminal terminal(&TerminalBuff);

String readstr;

const bool UNICODE_MODE = true;

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
    LTEModule_init(5, 13, UNICODE_MODE);
    terminal.println("OK");

    // DISABLE ECHO
    // terminal.print("Disable ECHO ");
    // terminal.println( LTE_cmd("ATE0") ? "OK" : "ERROR" );

    // PIN
    terminal.print("PIN state ");
    terminal.println( LTE_cmd("AT+CPIN?", &readstr) ? readstr : "ERROR" );

    if (readstr == "+CPIN: SIM PIN") 
    {
        terminal.print("Entering PIN ");
        terminal.println( LTE_cmd("AT+CPIN=1234") ? "OK" : "ERROR" );
    }

    // WAIT FOR NETWORK REGISTRATION
    terminal.print("Network Registration");
    while(1) {
        LTE_cmd("AT+CREG?", &readstr);
        if (intAt(readstr, 1) == 1) {
            terminal.println(" OK");
            break;
        }
        terminal.print('.');
    }

    // SMS TEXT
    // terminal.print("SMS mode text ");
    // terminal.println( LTE_cmd("AT+CMGF=1") ? "OK" : "ERROR" );

    // SMS UNICODE
    // terminal.print("SMS unicode ");
    // if (UNICODE_MODE)   terminal.println( LTE_cmd("AT+CSCS=\"UCS2\"") ? "OK" : "ERROR" );       
    // else                terminal.println( LTE_cmd("AT+CSCS=\"IRA\"") ? "OK" : "ERROR" );

    // SMS MODE
    // terminal.print("SMS mode ");
    // if (UNICODE_MODE)   terminal.println( LTE_cmd("AT+CSMP=17,168,0,8") ? "OK" : "ERROR" );       
    // else                terminal.println( LTE_cmd("AT+CSMP=17,168,0,0") ? "OK" : "ERROR" ); 

    // SMS NOTIF
    // terminal.print("SMS disable push notification ");
    // terminal.println( LTE_cmd("AT+CNMI=0,0,0,0,0") ? "OK" : "ERROR" );  // original = 2,1,0,0,0

    // SMS CLEAR
    // terminal.print("SMS clear old messages ");
    // terminal.println( LTE_cmd("AT+CMGD=,3") ? "OK" : "ERROR" ); 


    // CHECK OPERATOR
    // terminal.print("Network: ");
    // terminal.println( LTE_cmd("AT+COPS?", &readstr) ? stringAt(readstr, 2) : "ERROR" );

    // CHECK NUMBER
    // terminal.print("My Number: ");
    // terminal.println( LTE_cmd("AT+CNUM", &readstr) ? stringAt(readstr, 1) : "ERROR" );

    // Start Task SMS
    // xTaskCreate(LTE_smsTask, "LTESmsTask", 1024 * 2, (void*)0, 3, NULL);
}


void loop() 
{
    

    

    delay(100);
    M5.update();

    // DIAL
    if (M5.BtnA.wasPressed()) 
    {
        terminal.print("Calling MGR ");
        terminal.println( LTE_cmd("ATD0675471820;", &readstr) ? readstr : "ERROR" );
    }

    // SMS
    if (M5.BtnB.wasPressed()) 
    {
        terminal.print("Send SMS ");
        terminal.println( sendSMS("0675471820", "Yo Rasta !", &readstr) ? readstr : "ERROR" );
    }

    // Get RSSI
    if (M5.BtnC.wasPressed()) 
    {
        terminal.printf("CSQ rssi: %d\n", (LTE_cmd("AT+CSQ", &readstr) ? intAt(readstr, 0) : 0) );
    }

    // CHECK NEW MESSAGE
    struct SMS msg;
    while ( recvSMS(&msg) )
    {
        terminal.print("New MSG: ");    
        terminal.println(msg.msg);    
        terminal.print("\tfrom: ");    
        terminal.println(msg.from);    
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