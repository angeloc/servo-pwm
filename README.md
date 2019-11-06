# servo-pwm
Linux kernel module for servo motors.

## Usage example

    # echo 0 > /sys/devices/platform/servo@0/angle
    # echo 90 > /sys/devices/platform/servo@0/angle
    # echo 180 > /sys/devices/platform/servo@0/angle
