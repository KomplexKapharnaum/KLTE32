
#include <K32.h> // https://github.com/KomplexKapharnaum/K32-lib
K32* k32 = nullptr;

#include <hardware/K32_buttons.h>
K32_buttons* buttons = nullptr;

#include <K32_wifi.h>
K32_wifi* wifi = nullptr;

#include <K32_osc.h>
K32_osc* osc = nullptr;

#include <K32_mqtt.h>
K32_mqtt* mqtt = nullptr;



// K32 loader
//
void k32_setup() {

    //////////////////////////////////////// K32_lib ////////////////////////////////////
    k32 = new K32();
    
    //////////////////////////////////////// K32 hardware ////////////////////////////////////
    // BUTTONS (GPIO)
    buttons = new K32_buttons(k32);
    // if (k32->system->hw() >= 3) // ATOM
    //     buttons->add(39, "atom");

    /////////////////////////////////////////////// NETWORK //////////////////////////////////////

    // WIFI
    wifi = new K32_wifi(k32);
    wifi->setHostname(k32->system->name());

    // Define router (also MQTT broker)
    String router = "10.0.10.37";

    // Wifi connect (SSID / password)
    //
    // wifi->connect("kxkm24", NULL); // KXKM 24
    // wifi->connect("phare", NULL); //KXKM phare
    // wifi->connect("kxkm24lulu", NULL);                                                         //KXKM 24 lulu
    // wifi->connect("mgr4g", NULL);                                                              //Maigre dev
    wifi->connect("hmsphr", "hemiproject");                                                 //Maigre dev hmsphr
    // wifi->connect("interweb", "superspeed37");                                                 //Maigre dev home
    // wifi->connect("riri_new", "B2az41opbn6397");                                               //Riri dev home
    // TODO: if wifi->connect ommited = crash on mqtt/artnet/osc

    ////////////////// MQTT
    mqtt = new K32_mqtt(k32, wifi);
    if (mqtt)
    mqtt->start({
        .broker = router.c_str(), // Komplex
        // .broker = "2.0.0.10", // Riri dev home
        // .broker = "192.168.43.132",  // MGR dev home
        .beatInterval = 0, // heartbeat interval milliseconds (0 = disable) 5000
        .statusInterval = 0   // full beacon interval milliseconds (0 = disable) 15000
    });

    ////////////////// OSC
    osc = new K32_osc(k32, wifi);
    if (osc)
    osc->start({
        .port_in = 1818,        // osc port input (0 = disable)  // 1818
        .port_out = 1819,       // osc port output (0 = disable) // 1819
        .beatInterval = 0,   // heartbeat interval milliseconds (0 = disable)
        .statusInterval = 0 // full beacon interval milliseconds (0 = disable)
    });


}
