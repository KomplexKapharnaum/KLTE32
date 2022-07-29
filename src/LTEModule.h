#include <Arduino.h>
#include <stdint.h>

#include <vector>

SemaphoreHandle_t command_list_samap;
SemaphoreHandle_t sync_cmd_lock;
SemaphoreHandle_t partial_lock;
uint8_t restate;

Preferences storage;

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
};

using namespace std;
vector<ATCommand> serial_at;
vector<SMS> new_sms;
String zmmi_str;

void LTEModuleTask(void* arg);
void LTE_smsTask(void* arg);
void AddMsg(String str, uint8_t type, uint16_t sendcount, uint16_t sendtime);


void LTEModule_init(int rx, int tx)
{

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

    // Load NVS
    storage.begin("KLTE");
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
                if (restr.indexOf("OK") != -1) {
                    if ((serial_at[0].command_type == kACTION_MO) ||
                        (serial_at[0].command_type == kASSIGN_MO)) {
                        serial_at.erase(serial_at.begin());
                        Serial.printf("erase now %d\n", serial_at.size());
                    } else {
                        restr.replace("\r", "");
                        restr.replace("\n\nOK", "\n+END");
                        restr.replace("\nOK", "\n+END");
                        restr = restr.substring(restr.indexOf("+"), restr.length());
                        restr.trim();

                        // Serial.print("PRE: ");
                        // Serial.println(restr);    

                        // Pack extra new line to command line with "
                        int pos = 0;
                        bool lastPosPlus = true;
                        while(1) {
                            pos = restr.indexOf("\n", pos);
                            if (pos < 0) break;
                            if (restr.charAt(pos+1) == '+') 
                            {
                                if (!lastPosPlus) restr = restr.substring(0,pos)+"\""+restr.substring(pos,restr.length());
                                lastPosPlus = true;
                            }
                            else if (lastPosPlus)
                            {
                                restr = restr.substring(0,pos)+",\""+restr.substring(pos+1,restr.length());
                                lastPosPlus = false;
                            }
                            pos++;
                        }

                        restr.replace("+END", "");
                        restr.trim();

                        // Serial.println("PARSED: ");
                        // Serial.println(restr);

                        serial_at[0].read_str = restr;
                        serial_at[0].state    = kWaitforRead;
                    }
                } else if (restr.indexOf("ERROR") != -1) {
                    serial_at[0].state = kErrorReError;
                } else {
                }
                Serial.println("RCV: ");
                Serial.println(restr);
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

int lineCount(String Str) 
{
    if (Str.length() > 0) Str += "\n";
    
    int lineCount = 0;
    bool insideQuotes = false;

    // look for \n but outside double quotes
    for (int k=0; k<Str.length(); k++) {
        if (Str[k] == '"') insideQuotes = !insideQuotes;
        if (Str[k] == '\n' && !insideQuotes) lineCount++;
    }
    return lineCount;
}


String lineAt(String Str, int index)
{
    Str += "\n";

    int start = 0;
    int end = -1;
    bool insideQuotes = false;
    int scannedIndex = -1;

    // look for \n but outside double quotes
    for (int k=0; k<Str.length(); k++) {
        if (Str[k] == '"') insideQuotes = !insideQuotes;
        if (Str[k] == '\n' && !insideQuotes) {
            start = end+1;
            end = k;
            scannedIndex++;
        }
        if (scannedIndex == index) {
            return Str.substring(start, end);
        }
    }
    return "";
}


String lineStartingWith(String Str, String start)
{
    for(int k=0; k<lineCount(Str); k++) {
        String line = lineAt(Str, k);
        if (line.startsWith(start)) return line; 
    }
    return "";
}


String argAt(String Str, int index, int line = 0) 
{   
    Str = lineAt(Str, line);
    Str = Str.substring(Str.indexOf(": ")+2, Str.length())+",";

    int start = 0;
    int end = -1;
    bool insideQuotes = false;
    int scannedIndex = -1;

    // look for , but outside double quotes
    for (int k=0; k<Str.length(); k++) {
        if (Str[k] == '"') insideQuotes = !insideQuotes;
        if (Str[k] == ',' && !insideQuotes) {
            start = end+1;
            end = k;
            scannedIndex++;
        }
        if (scannedIndex == index) {
            String s = Str.substring(start, end);
            if (s.charAt(0) == '"') s = s.substring(1, s.length());
            if (s.charAt(s.length()-1) == '"') s = s.substring(0, s.length()-1);
            return s;
        }
    }
    return "";
}

String argAt(String Str, int index, String lineStart) 
{   
    String line = lineStartingWith(Str, lineStart);
    return argAt(line, index);
}

String string2hex(String str)
{
  char out[str.length()*4];
  for (int i = 0; i < str.length(); i++)  {
    sprintf(&out[i*4], "%04X", str.charAt(i));
  };
  String ret = String(out);
  return ret;
}

String hex2string(String str)
{
  String ret = "";
  char hex[str.length()+1];
  str.toCharArray(hex, str.length()+1);

  char temp[5] = {0};
  int index = 0;

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

  LTE_cmd("AT+CMGS=\""+to+"\"", NULL, kPARTIAL_MT, 0, 10);
  res = LTE_cmd(msg+"\x1A", value, kQUERY_MT);

  if (!res || !value->startsWith("+CMGS")) return false;
  return true;
}

bool sendSMS(String to, int mem, String *value = NULL)
{
  String msg = storage.getString( ("SMS"+String(mem)).c_str() , "?");
  return sendSMS(to, msg, value);
}


void storeSMS(String msg, int mem)   
{
  storage.putString( ("SMS"+String(mem)).c_str() , msg);
}

bool pullSMS() 
{   
    String newMsg;
    bool docleanup;
    int count;
    
    LTE_cmd("AT+CMGL=\"REC UNREAD\"", &newMsg);

    count = lineCount(newMsg);
    for (int k=0; k<count; k++) {
        String line = lineAt(newMsg, k);
        
        // Serial.printf("-NEW SMS %d: ", k);
        // Serial.println(line);

        struct SMS message;
        message.msg  = argAt(line, 5);
        message.from = argAt(line, 2);
        message.date = argAt(line, 4);

        // Serial.println(message.msg);
        new_sms.push_back(message);
    } 

    if (count > 0) LTE_cmd("AT+CMGD=,3");

    return (count > 0);
}

bool recvSMS( struct SMS* message)
{
    if (!new_sms.empty() && message != NULL) {
        message->msg  = String(new_sms[0].msg);
        message->from = String(new_sms[0].from);
        message->date = String(new_sms[0].date);
        new_sms.erase(new_sms.begin());
        return true;
    }
    
    return false;    
}

int receivedSMS() 
{
    return new_sms.size();
}


/////////////////// HTTP

bool postJSON(String url, String data)
{
    String newMsg;

    LTE_cmd("AT+HTTPINIT");
    LTE_cmd("AT+HTTPPARA=\"URL\",\""+url+"\"");
    LTE_cmd("AT+HTTPPARA=\"CONTENT\",\"application/json\""); 
    LTE_cmd("AT+HTTPDATA="+String(data.length())+",10", NULL, kPARTIAL_MT, 0, 10);
    LTE_cmd(data);
    LTE_cmd("AT+HTTPACTION=1", &newMsg);
    LTE_cmd("AT+HTTPTERM");

    Serial.println(argAt(newMsg, 1, "+HTTPACTION:"));
    return (argAt(newMsg, 1, "+HTTPACTION:") == "200");
}

////////////////////////// END UNUSED