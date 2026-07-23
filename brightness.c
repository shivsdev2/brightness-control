/*
  * Just trying to learn *Hello Bus* of D-Bus   

  * References 
  *           https://www.freedesktop.org/software/systemd/man/latest/sd_bus_call_method.html#
  *           https://github.com/Hummer12007/brightnessctl
  *           
  * This Code has a lot of bugs, i will be fixing them later.
  */

#define _DEFAULT_SOURCE
#include <stdio.h>
#include <systemd/sd-bus.h>
#include <string.h>
#include <stdint.h>
#include <stdlib.h>
#include <dirent.h>
#include <unistd.h>
#include <errno.h>
#include <getopt.h>

#define BACKLIGHT_PATH "/sys/class/backlight/"
#define MAX_BACKLIGHT_DEVICES 16
#define MAX_DEVICE_NAME_LEN 256

/* to hold backlight device info */
typedef struct {
    char name[MAX_DEVICE_NAME_LEN];
    unsigned int max_brightness;
    int is_active;  /* 1 if device appears to be the active one */
} BacklightDevice;

/*
 * Discover backlight devices from /sys/class/backlight/
 * Returns the number of devices found
 * Or if no then -1.
 */
int discover_backlight_devices(BacklightDevice *devices, int max_devices) {
    DIR *dir;
    struct dirent *entry;
    int count = 0;
    
    dir = opendir(BACKLIGHT_PATH);
    if (dir == NULL) {
        fprintf(stderr, "Failed to open %s: %s\n", BACKLIGHT_PATH, strerror(errno));
        return -1;
    }
    
    while ((entry = readdir(dir)) != NULL && count < max_devices) {
        /* Skip . and .. entries */
        if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0) {
            continue;
        }
        
        /* Only process directories (backlight devices are directories) */
        if (entry->d_type == DT_DIR) {
            strncpy(devices[count].name, entry->d_name, MAX_DEVICE_NAME_LEN - 1);
            devices[count].name[MAX_DEVICE_NAME_LEN - 1] = '\0';
            devices[count].max_brightness = 0;
            devices[count].is_active = 0;
            count++;
        }
    }
    
    closedir(dir);
    return count;
}

/*
 * Read max_brightness from a specific backlight device
*/
int read_max_brightness(const char *device_name, unsigned int *max_brightness) {
    char path[MAX_DEVICE_NAME_LEN + 64];
    FILE *f;
    
    snprintf(path, sizeof(path), "%s%s/max_brightness", BACKLIGHT_PATH, device_name);
    
    f = fopen(path, "r");
    if (f == NULL) {
        return -1;
    }
    
    if (fscanf(f, "%u", max_brightness) != 1) {
        fclose(f);
        return -1;
    }
    
    fclose(f);
    return 0;
}

/*
 * Check if a device is the active backlight by reading actual_brightness
 * Returns 1 if active (has actual_brightness), 0 otherwise
*/
int check_device_active(const char *device_name) {
    char path[MAX_DEVICE_NAME_LEN + 64];
    FILE *f;
    unsigned int actual_brightness;
    
    snprintf(path, sizeof(path), "%s%s/actual_brightness", BACKLIGHT_PATH, device_name);
    
    f = fopen(path, "r");
    if (f == NULL) {
        return 0;
    }
    
    /* If we can read actual_brightness, this is likely the active device */
    if (fscanf(f, "%u", &actual_brightness) == 1) {
        fclose(f);
        return 1;
    }
    
    fclose(f);
    return 0;
}

/*
 * Select the best backlight device from the discovered list
 * Priority: active device > intel_backlight > amdgpu_bl0 > acpi_video0 > first available
*/
int select_backlight_device(BacklightDevice *devices, int count) {
    int i;
    
    /* First, try to find an active device */
    for (i = 0; i < count; i++) {
        if (check_device_active(devices[i].name)) {
            devices[i].is_active = 1;
            if (read_max_brightness(devices[i].name, &devices[i].max_brightness) == 0) {
                return i;
            }
        }
    }
    
    /* If no active device found, use priority order */
    const char *priority_names[] = {
        "intel_backlight",
        "amdgpu_bl0",
        "nvidia_wmi_ec",
        "acpi_video0",
        "amdgpu_bl1",
        "nvidia_backlight",
        NULL
    };
    
    for (int p = 0; priority_names[p] != NULL; p++) {
        for (i = 0; i < count; i++) {
            if (strcmp(devices[i].name, priority_names[p]) == 0) {
                if (read_max_brightness(devices[i].name, &devices[i].max_brightness) == 0) {
                    return i;
                }
            }
        }
    }
    


    
    /* Fall back to first available device */
    for (i = 0; i < count; i++) {
        if (read_max_brightness(devices[i].name, &devices[i].max_brightness) == 0) {
            return i;
        }
    }
    
    return -1;
}

/*
 * Read current brightness from a specific backlight device via sysfs
 */
int read_current_brightness_sysfs(const char *device_name, unsigned int *current_brightness) {
    char path[MAX_DEVICE_NAME_LEN + 64];
    FILE *f;
    
    snprintf(path, sizeof(path), "%s%s/brightness", BACKLIGHT_PATH, device_name);
    
    f = fopen(path, "r");
    if (f == NULL) {
        return -1;
    }
    
    if (fscanf(f, "%u", current_brightness) != 1) {
        fclose(f);
        return -1;
    }
    
    fclose(f);
    return 0;
}

/*
 * Get current brightness via D-Bus (systemd login1)
 * Returns 0 on success, -1 on failure
 */
int get_brightness_dbus(sd_bus *bus, const char *device_name, unsigned int *current_brightness) {
    sd_bus_error error = SD_BUS_ERROR_NULL;
    sd_bus_message *reply = NULL;
    int r;
    
    r = sd_bus_call_method(
        bus,
        "org.freedesktop.login1",
        "/org/freedesktop/login1/session/auto",
        "org.freedesktop.login1.Session",
        "GetBrightness",
        &error,
        &reply,
        "s",
        "backlight",
        device_name
    );
    
    if (r < 0) {
        fprintf(stderr, "Failed to get brightness via D-Bus: %s\n", error.message);
        sd_bus_error_free(&error);
        return -1;
    }
    
    r = sd_bus_message_read(reply, "u", current_brightness);
    if (r < 0) {
        fprintf(stderr, "Failed to parse D-Bus reply: %s\n", strerror(-r));
        sd_bus_message_unref(reply);
        return -1;
    }
    
    sd_bus_message_unref(reply);
    return 0;
}

/* Print usage information */
void print_usage(const char *program_name) {
    printf("Usage: %s [OPTIONS] BRIGHTNESS\n", program_name);
    printf("Set screen brightness via D-Bus.\n\n");
    printf("Options:\n");
    printf("  -d, --device NAME   Specify backlight device (e.g., intel_backlight, amdgpu_bl0)\n");
    printf("  -l, --list          List available backlight devices and exit\n");
    printf("  -g, --get           Get current brightness and exit\n");
    printf("  -h, --help          Show this help message\n");
    printf("\nBRIGHTNESS can be a percentage (e.g., 50%%) or absolute value.\n");
}

int main(int argc, char *argv[]){
    sd_bus *bus = NULL;
    int r; 
    BacklightDevice devices[MAX_BACKLIGHT_DEVICES];
    int device_count;
    int selected_device = -1;
    unsigned int max_brightness;
    sd_bus_error error = SD_BUS_ERROR_NULL;
    char *specified_device = NULL;
    int list_mode = 0;
    int opt;
    
    int get_mode = 0;
    
    static struct option long_options[] = {
        {"device", required_argument, 0, 'd'},
        {"list", no_argument, 0, 'l'},
        {"get", no_argument, 0, 'g'},
        {"help", no_argument, 0, 'h'},
        {0, 0, 0, 0}
    };
    
    while ((opt = getopt_long(argc, argv, "d:lgh", long_options, NULL)) != -1) {
        switch (opt) {
            case 'd':
                specified_device = optarg;
                break;
            case 'l':
                list_mode = 1;
                break;
            case 'g':
                get_mode = 1;
                break;
            case 'h':
                print_usage(argv[0]);
                return 0;
            default:
                print_usage(argv[0]);
                return 1;
        }
    }
    
    /* Discover backlight devices */
    device_count = discover_backlight_devices(devices, MAX_BACKLIGHT_DEVICES);
    if (device_count < 0) {
        fprintf(stderr, "Failed to discover backlight devices.\n");
        return 1;
    }
    
    if (device_count == 0) {
        fprintf(stderr, "No backlight devices found in %s\n", BACKLIGHT_PATH);
        return 1;
    }
    
    /* List mode: just show devices and exit */
    if (list_mode) {
        printf("Available backlight devices:\n");
        for (int i = 0; i < device_count; i++) {
            unsigned int max_b = 0;
            read_max_brightness(devices[i].name, &max_b);
            printf("  %s (max_brightness: %u)\n", devices[i].name, max_b);
        }
        return 0;
    }
    
    /* If a specific device was requested, find it */
    if (specified_device != NULL) {
        for (int i = 0; i < device_count; i++) {
            if (strcmp(devices[i].name, specified_device) == 0) {
                selected_device = i;
                if (read_max_brightness(devices[i].name, &devices[i].max_brightness) != 0) {
                    fprintf(stderr, "Failed to read max_brightness from %s\n", specified_device);
                    return 1;
                }
                break;
            }
        }
        if (selected_device < 0) {
            fprintf(stderr, "Device '%s' not found. Available devices:\n", specified_device);
            for (int i = 0; i < device_count; i++) {
                printf("  %s\n", devices[i].name);
            }
            return 1;
        }
    } else if (device_count > 1) {
        /* Multiple devices found - auto-select the best one */
        printf("Multiple backlight devices found:\n");
        for (int i = 0; i < device_count; i++) {
            printf("  %d: %s\n", i + 1, devices[i].name);
        }
        
        /* Auto-select the best device */
        selected_device = select_backlight_device(devices, device_count);
        if (selected_device >= 0) {
            printf("Auto-selected: %s (max_brightness: %u)\n", 
                   devices[selected_device].name, 
                   devices[selected_device].max_brightness);
        }
    } else {
        /* Single device - use it directly */
        selected_device = 0;
        if (read_max_brightness(devices[0].name, &devices[0].max_brightness) != 0) {
            fprintf(stderr, "Failed to read max_brightness from %s\n", devices[0].name);
            return 1;
        }
    }
    
    if (selected_device < 0) {
        fprintf(stderr, "Failed to select a valid backlight device.\n");
        return 1;
    }
    
    max_brightness = devices[selected_device].max_brightness;
    
    /* Get mode: just show current brightness and exit */
    if (get_mode) {
        unsigned int current_b = 0;
        r = sd_bus_open_system(&bus);
        if (r >= 0) {
            /* Try D-Bus first */
            if (get_brightness_dbus(bus, devices[selected_device].name, &current_b) == 0) {
                printf("Current brightness: %u (max: %u)\n", current_b, max_brightness);
                sd_bus_unref(bus);
                return 0;
            }
            sd_bus_unref(bus);
        }
        /* Fall back to sysfs */
        if (read_current_brightness_sysfs(devices[selected_device].name, &current_b) == 0) {
            printf("Current brightness: %u (max: %u)\n", current_b, max_brightness);
        } else {
            fprintf(stderr, "Failed to read current brightness from %s\n", devices[selected_device].name);
        }
        return 0;
    }
    
    r = sd_bus_open_system(&bus);
    if (r < 0) {
        fprintf(stderr, "Failed to connect to system bus: %s\n", strerror(-r));
        return 1;
    }

    int user_brightness;
    char *brightness_str = NULL;
    
    /* Get brightness from command line argument or prompt */
    if (optind < argc) {
        brightness_str = argv[optind];
    } else {
        printf("Enter the brightness: " );
        char input[64];
        if (scanf("%63s", input) != 1) {
            fprintf(stderr, "Brightness cannot be non integer.");
            sd_bus_unref(bus);
            return 1;
        }
        brightness_str = input;
    }
    
    /* Check for percentage */
    if (brightness_str != NULL && brightness_str[strlen(brightness_str) - 1] == '%') {
        int percent = atoi(brightness_str);
        if (percent < 0 || percent > 100) {
            fprintf(stderr, "Error: Percentage must be between 0 and 100\n");
            sd_bus_unref(bus);
            return 1;
        }
        user_brightness = (percent * max_brightness) / 100;
    } else {
        user_brightness = atoi(brightness_str);
    }

    if (user_brightness < 0) {
        fprintf(stderr, "Error: Brightness cannot be negative (%d).\n", user_brightness);
        sd_bus_unref(bus);
        return 1;
    }

    if (user_brightness > (int)max_brightness) {
        fprintf(stderr, "Error: brightness exceeds maximum brightness %u\n", max_brightness);
        sd_bus_unref(bus);
        return 1;
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
        devices[selected_device].name, 
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