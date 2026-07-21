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
#include <stdint.h>

int main(void){

    sd_bus *bus = NULL;
    int r;
    unsigned int max_brightness; 
    sd_bus_error error = SD_BUS_ERROR_NULL;
    r = sd_bus_open_system(&bus);
    if (r < 0) {
        fprintf(stderr, "Failed to connect to system bus: %s\n", strerror(-r));
        return 1;
    }

    int user_brightness;
    printf("Enter the brightness: " );

    if (scanf("%d", &user_brightness) != 1) {
        fprintf(stderr, "Brightness cannot be non integer.");
        sd_bus_unref(bus);
        return 1;
    }

    if (user_brightness < 0) {
        fprintf(stderr, "Error: Brightness cannot be negative (%d).\n", user_brightness);
        sd_bus_unref(bus);
        return 1;
    }

    FILE *f = fopen("/sys/class/backlight/intel_backlight/max_brightness", "r");
    if (f != NULL) {
        if (fscanf(f, "%u", &max_brightness) == 1) {
            if (user_brightness > max_brightness) {
                fprintf(stderr, "Error: brightness exceeds maximum brightness %u\n", max_brightness);
                fclose(f);
                sd_bus_unref(bus);
                return 1;
            }
        }
        fclose(f);
    }

    /*
    * Screen brightness is managed by systemd-logind on the System Bus
    * Brightness is read-only property so we have to use sd_bus_call_method
    * https://www.freedesktop.org/software/systemd/man/latest/sd_bus_call_method.html#
    */

    r = sd_bus_call_method(
        bus,
        "org.freedesktop.login1",
        "/org/freedesktop/login1/session/auto",
        "org.freedesktop.login1.Session",   
        "SetBrightness",                     
        &error,                            
        NULL,                               
        "ssu",                                                     
        "backlight", 
        "intel_backlight", 
        (uint32_t)user_brightness


    );

    if (r < 0) {
        fprintf(stderr, "Failed to issue method call: %s\n", error.message);
        sd_bus_error_free(&error);
        sd_bus_unref(bus);
        return 1;
    }


    sd_bus_error_free(&error);
    sd_bus_unref(bus);
    return 0;

}
