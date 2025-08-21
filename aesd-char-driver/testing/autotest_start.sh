#!/bin/bash
(
    cd .. || exit 1
    echo "Bulding..."
    make
    if [ $? -ne 0 ]; then
        echo "Build failed."
        exit 1
    fi
    echo "Adding driver..."
    sudo insmod aesdchar.ko
    sudo dmesg | tail -n 10
    echo "Creating device..."
    sudo mknod /dev/aesdchar c 511 0
    sudo chmod 666 /dev/aesdchar
)