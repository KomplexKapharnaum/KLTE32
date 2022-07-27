#include <Arduino.h>
#include <stdint.h>

#include <vector>

SemaphoreHandle_t command_list_samap;
SemaphoreHandle_t sync_cmd_lock;
uint8_t restate;

typedef enum {
    kQUERY_MO = 0,
    KTEST_MO,
    kASSIGN_MO,
    kACTION_MO,
    kPARTIAL_MO,
    kQUERY_MT,
    kTEST_MT,
    kASSIGN_MT,
    kACTION_MT,
    kPARTIAL_MT,
    kINFORM
} LTEMsg_t;

typedef enum {
    kErrorSendTimeOUT = 0xe1,
    kErrorReError     = 0xe2,
    kErroeSendError   = 0xe3,
    kSendReady        = 0,
    kSending          = 1,
    kWaitforMsg       = 2,
    kWaitforNext      = 3,
    kWaitforRead      = 4,
    kReOK
} LTEState_t;

struct ATCommand {
    uint8_t command_type;
    String str_command;
    uint16_t send_max_number;
    uint16_t max_time;
    uint8_t state;
    String read_str;
    uint16_t _send_count;
    uint16_t _send_time_count;

} user;

struct SMS {
    String msg;
    String from;
    String date;
    String time;
};

using namespace std;
vector<ATCommand> serial_at;
vector<SMS> new_sms;
String zmmi_str;
bool UNIMODE = false;

void LTEModuleTask(void* arg);
void LTE_smsTask(void* arg);
void AddMsg(String str, uint8_t type, uint16_t sendcount, uint16_t sendtime);


void LTEModule_init(int rx, int tx, bool enable_unicode = false)
{
  UNIMODE = enable_unicode;

  Serial2.begin(115200, SERIAL_8N1, rx, tx);

  // Pwr reset module
  pinMode(2, OUTPUT);
  digitalWrite(2, 0);
  delay(1000);
  digitalWrite(2, 1);

  Serial2.print("\x1A\n"); // ESCAPE

  sync_cmd_lock = xSemaphoreCreateMutex();
  xSemaphoreGive(sync_cmd_lock);

  // Start Task
  command_list_samap = xSemaphoreCreateMutex();
  xSemaphoreGive(command_list_samap);
  xTaskCreate(LTEModuleTask, "LTEModuleTask", 1024 * 2, (void*)0, 4, NULL);

}


void LTEModuleTask(void* arg) 
{
    int Number = 0;
    String restr;
    while (1) {
        xSemaphoreTake(command_list_samap, portMAX_DELAY);

        if (Serial2.available() != 0) {
            String str = Serial2.readString();
            restr += str;

            if (restr.indexOf("\r\n") != -1) {
            }

            if (restr.indexOf("+ZMMI:") != -1) {
                zmmi_str = restr;
            } else if ((restr.indexOf("OK") != -1) ||
                       (restr.indexOf("ERROR") != -1)) 
            {
                Serial.print("RCV: ");
                Serial.print(restr);
                if (restr.indexOf("OK") != -1) {
                    if ((serial_at[0].command_type == kACTION_MO) ||
                        (serial_at[0].command_type == kASSIGN_MO)) {
                        serial_at.erase(serial_at.begin());
                        Serial.printf("erase now %d\n", serial_at.size());
                    } else {
                        restr.replace("\nOK", "");
                        restr.trim();
                        restr = restr.substring(restr.indexOf("+"), restr.length());
                        Serial.print("FINAL: ");
                        Serial.println(restr);
                        // String extra = "";
                        // if (restr.startsWith("+CMGR") || restr.startsWith("+CMGL")) {
                        //     extra = restr.substring(restr.indexOf("\n")+1, restr.length());
                        //     extra = extra.substring(0, extra.indexOf("\n"));
                        //     extra.trim();
                        // }
                        // restr = restr.substring(0, restr.indexOf("\n"));
                        // restr.trim();                        
                        // if (extra != "") restr += ",\""+extra+"\"";
                        serial_at[0].read_str = restr;
                        serial_at[0].state    = kWaitforRead;
                    }
                } else if (restr.indexOf("ERROR") != -1) {
                    serial_at[0].state = kErrorReError;
                } else {
                }
                restr.clear();
            }
        }

        if (serial_at.empty() != true) {
            Number = 0;
            switch (serial_at[0].state) {
                case kSendReady:
                    Serial.print("SEND: ");
                    Serial.printf(serial_at[0].str_command.c_str());
                    Serial2.write(serial_at[0].str_command.c_str());
                    serial_at[0].state = kSending;
                    break;
                case kSending:
                    // Serial.println("SENDING");
                    if (serial_at[0]._send_time_count > 0) {
                        serial_at[0]._send_time_count--;
                    } else {
                      if (serial_at[0].command_type == kPARTIAL_MT) serial_at[0].state = kWaitforNext;
                      else serial_at[0].state = kWaitforMsg;
                    }
                    /* code */
                    break;
                case kWaitforMsg:
                    Serial.println("WAIT FOR ANSWER");
                    if (serial_at[0]._send_count > 0) {
                        serial_at[0]._send_count--;
                        serial_at[0]._send_time_count = serial_at[0].max_time;
                        Serial.print("WAIT: ");
                        Serial.printf(serial_at[0].str_command.c_str());
                        Serial2.write(serial_at[0].str_command.c_str());
                        restr.clear();
                        serial_at[0].state = kSending;
                    } else {
                        serial_at[0].state = kErrorSendTimeOUT;
                        Serial.println("SEND TIMEOUT");  
                    }
                    /* code */
                    break;
                case kWaitforRead:
                    /* code */
                    break;
                case kWaitforNext:
                    /* code */
                    break;
                case kReOK:
                    /* code */
                    break;
                case kErrorSendTimeOUT:
                    break;
                case 0xe2:
                    /* code */
                    break;
                default:
                    break;
            }
        }
        xSemaphoreGive(command_list_samap);
        delay(10);
    }
}

void AddMsg(String str, uint8_t type, uint16_t retry, uint16_t sendwait) {
    struct ATCommand newcommand;
    newcommand.str_command      = str;
    newcommand.command_type     = type;
    newcommand.max_time         = sendwait;
    newcommand.send_max_number  = retry;
    newcommand.state            = 0;
    newcommand._send_count      = retry;
    newcommand._send_time_count = sendwait;
    xSemaphoreTake(command_list_samap, portMAX_DELAY);
    serial_at.push_back(newcommand);
    xSemaphoreGive(command_list_samap);
}

uint8_t readSendState(uint32_t number) {
    xSemaphoreTake(command_list_samap, portMAX_DELAY);
    uint8_t restate = serial_at[number].state;
    xSemaphoreGive(command_list_samap);
    return restate;
}

uint32_t getATMsgSize() {
    xSemaphoreTake(command_list_samap, portMAX_DELAY);
    uint32_t restate = serial_at.size();
    xSemaphoreGive(command_list_samap);
    return restate;
}
String ReadMsgstr(uint32_t number) {
    xSemaphoreTake(command_list_samap, portMAX_DELAY);
    String restate = serial_at[number].read_str;
    xSemaphoreGive(command_list_samap);
    return restate;
}

bool EraseFirstMsg() {
    xSemaphoreTake(command_list_samap, portMAX_DELAY);
    serial_at.erase(serial_at.begin());
    xSemaphoreGive(command_list_samap);
    return true;
}

bool LTE_cmd(String str, String *value = NULL, uint8_t type = kQUERY_MT, uint16_t retry = 5, uint16_t sendwait = 100)
{ 
    bool ret = false;

    xSemaphoreTake(sync_cmd_lock, portMAX_DELAY);

    AddMsg(str+"\r\n", type, retry, sendwait);
    while ((readSendState(0) == kSendReady) || (readSendState(0) == kSending) || (readSendState(0) == kWaitforMsg)) 
      delay(50);

    if (readSendState(0) == kWaitforNext) {
      if (value) *value = "NEXT";
      ret = true;
    }
    else if (readSendState(0) == kWaitforRead) {
      if (value) *value = ReadMsgstr(0);
      ret = true;
    }
    else {
      if (value) *value = "ERROR";
      ret = false;
    }

    EraseFirstMsg();
    xSemaphoreGive(sync_cmd_lock);

    return ret;
}


String stringAt(String Str, int index) 
{   
    Str = Str.substring(Str.indexOf(": ")+2, Str.length());

    int count = 0;
    while( count < index && Str.length() > 0) 
    {
        Str = Str.substring(Str.indexOf(",")+1, Str.length());
        count++;
    }

    int end = Str.indexOf(",");
    if (end < 0) end = Str.length();
    Str = Str.substring(0, end);

    if (Str.charAt(0) == '"')
        Str = Str.substring(1, Str.length());
    
    if (Str.charAt(Str.length()-1) == '"')
        Str = Str.substring(0, Str.length()-1);

    return Str;
}

int intAt(String Str, int index) 
{   
    String sStr = stringAt(Str, index);
    if (Str.length() > 0) return sStr.toInt();
    return -1;
}

String string2hex(String str)
{
  char out[str.length()*4];
  for (int i = 0; i < str.length(); i++)  {
    sprintf(&out[i*4], "%04x", str.charAt(i));
  };
  String ret = String(out);
  ret.toUpperCase(); 
  return ret;
}

String hex2string(String str)
{
  String ret = "";
  char hex[str.length()+1];
  str.toCharArray(hex, str.length()+1);

  char temp[5] = {0};
  int index = 0;

    Serial.println(str.length());

  for (int i = 0; i < str.length(); i += 4) {
    strncpy(temp, &hex[i], 4);
    ret += (char) strtol(temp,NULL,16);
    // Serial.print(temp);
    // Serial.print(" ");
    // Serial.print(&hex[i]);
    // Serial.print(" ");
    // Serial.println(strtol(temp,NULL,16));
  }

  return ret;
}


//////////////////// ACTIONS

bool sendSMS(String to, String msg, String *value = NULL)
{
  bool res = false;
  
  if (UNIMODE) {
    to = string2hex(to);
    msg = string2hex(msg);
  }

  LTE_cmd("AT+CMGS=\""+to+"\"", NULL, kPARTIAL_MT, 0, 10);
  res = LTE_cmd(msg+"\x1A", value, kQUERY_MT, 0, 100);

  if (!res || !value->startsWith("+CMGS")) return false;
  return true;
}

void LTE_smsTask(void* arg) 
{   
    String newMsg;
    bool docleanup;
    
    while (1) 
    {
        LTE_cmd("AT+CMGL=\"REC UNREAD\"", &newMsg);

        docleanup = false; 
        while (newMsg != "") 
        { 
            // Serial.println("-NEW SMS-");
            // Serial.println(newMsg);

            struct SMS message;
            message.msg  = stringAt(newMsg, 6);
            message.from = stringAt(newMsg, 2);
            message.date = stringAt(newMsg, 4);
            message.time = stringAt(newMsg, 5);

            if (UNIMODE) {
                message.msg = hex2string(message.msg);
                message.from = hex2string(message.from); 
            }

            // Serial.println(message.msg);
            new_sms.push_back(message);

            LTE_cmd("AT+CMGD="+stringAt(newMsg, 0));
            LTE_cmd("AT+CMGL=\"REC READ\"", &newMsg);

            docleanup = true;
        }

        if (docleanup) LTE_cmd("AT+CMGD=,3");

        delay(100);
    }
}

bool recvSMS( struct SMS* message)
{
    if (!new_sms.empty() && message != NULL) {
        message->msg  = String(new_sms[0].msg);
        message->from = String(new_sms[0].from);
        message->date = String(new_sms[0].date);
        message->time = String(new_sms[0].time);
        new_sms.erase(new_sms.begin());
        return true;
    }
    
    return false;    
}


/////////////////// UNUSED

uint16_t GetstrNumber(String Str, uint32_t* ptrbuff) {
    uint16_t count = 0;
    String Numberstr;
    int indexpos = 0;
    while (Str.length() > 0) {
        indexpos = Str.indexOf(",");
        if (indexpos != -1) {
            Numberstr      = Str.substring(0, Str.indexOf(","));
            Str            = Str.substring(Str.indexOf(",") + 1, Str.length());
            ptrbuff[count] = Numberstr.toInt();
            count++;
        } else {
            ptrbuff[count] = Str.toInt();
            count++;
            break;
        }
    }
    return count;
}
vector<String> restr_v;
uint16_t GetstrNumber(String StartStr, String EndStr, String Str) {
    uint16_t count = 0;
    String Numberstr;
    int indexpos = 0;

    Str = Str.substring(Str.indexOf(StartStr) + StartStr.length(),
                        Str.indexOf(EndStr));
    Str.trim();
    restr_v.clear();

    while (Str.length() > 0) {
        indexpos = Str.indexOf(",");
        if (indexpos != -1) {
            Numberstr = Str.substring(0, Str.indexOf(","));
            Str       = Str.substring(Str.indexOf(",") + 1, Str.length());
            restr_v.push_back(Numberstr);
            count++;
        } else {
            restr_v.push_back(Numberstr);
            ;
            count++;
            break;
        }
    }
    return count;
}

String getReString(uint16_t Number) {
    if (restr_v.empty()) {
        return String("");
    }
    return restr_v.at(Number);
}

uint16_t GetstrNumber(String StartStr, String EndStr, String Str,
                      uint32_t* ptrbuff) {
    uint16_t count = 0;
    String Numberstr;
    int indexpos = 0;

    Str = Str.substring(Str.indexOf(StartStr) + StartStr.length(),
                        Str.indexOf(EndStr));
    Str.trim();

    while (Str.length() > 0) {
        indexpos = Str.indexOf(",");
        if (indexpos != -1) {
            Numberstr      = Str.substring(0, Str.indexOf(","));
            Str            = Str.substring(Str.indexOf(",") + 1, Str.length());
            ptrbuff[count] = Numberstr.toInt();
            count++;
        } else {
            ptrbuff[count] = Str.toInt();
            count++;
            break;
        }
    }
    return count;
}
uint32_t numberbuff[128];

////////////////////////// END UNUSED