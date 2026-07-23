# Brightness Control

A tool in C for controlling screen brightness via D-Bus. It automatically discovers backlight devices from `/sys/class/backlight/` and supports multiple backlight drivers.

## Features

- **Automatic backlight discovery**: Scans `/sys/class/backlight/` to find available devices
- **Multi-driver support**: Works with Intel, AMD, NVIDIA, and ACPI backlights
- **device selection**: When multiple devices are found, automatically selects the most appropriate one

## Supported Backlight Drivers

The tool supports various backlight drivers including:
- `intel_backlight` (Intel graphics)
- `amdgpu_bl0`, `amdgpu_bl1` (AMD graphics)
- `nvidia_wmi_ec`, `nvidia_backlight` (NVIDIA graphics)
- `acpi_video0` (ACPI generic)

## Usage

```
./brightness-control [OPTIONS] BRIGHTNESS
```

### Options

- `-d, --device NAME`   Specify backlight device (e.g., intel_backlight, amdgpu_bl0)
- `-l, --list`          List available backlight devices and exit
- `-g, --get`           Get current brightness via D-Bus (falls back to sysfs) and exit
- `-h, --help`          Show help message

### Examples

```bash
# Set brightness to 50%
./brightness-control 50%

# Set brightness to absolute value 500
./brightness-control 500

# Use a specific device
./brightness-control -d amdgpu_bl0 75%

# List available devices
./brightness-control -l

# Get current brightness
./brightness-control -g
```


## References

- [brightnessctl](https://github.com/Hummer12007/brightnessctl) - A more feature-rich tool with similar purpose
- [systemd D-Bus API](https://www.freedesktop.org/software/systemd/man/latest/sd_bus_call_method.html)
- [dirent.h man page](https://man7.org/linux/man-pages/man0/dirent.h.0p.html)