/*
  * Just trying to learn *Hello Bus* of D-Bus   

  * References 
  *           https://www.freedesktop.org/software/systemd/man/latest/sd_bus_call_method.html#
  *           
  * This Code has a lot of bugs, i will be fixing them later.
*/

#include <stdio.h>
#include <systemd/sd-bus.h>
#include <string.h>


int main(void){

    sd_bus *bus = NULL;
    int r;

    sd_bus_open_system(&bus);

    if (r < 0) {
        fprintf(stderr, "Failed to connect to system bus: %s\n", strerror(-r));
        return 1;
    }

    /*
    * Screen brightness is managed by systemd-logind on the System Bus
    * Brightness is read-only property so we have to use sd_bus_call_method
    * https://www.freedesktop.org/software/systemd/man/latest/sd_bus_call_method.html#
    */
    sd_bus_call_method(
        bus,
        "org.freedesktop.login1",
        "/org/freedesktop/login1/session/auto",
        "org.freedesktop.login1.Session",   
        "SetBrightness",                     
        NULL,                            
        NULL,                               
        "ssu",                                                     
        "backlight", "intel_backlight", 5000

    );

    return 0;

}
