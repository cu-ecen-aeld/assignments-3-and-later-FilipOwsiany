#!/bin/bash
(
    cd .. || exit 1
    echo "Bulding..."
    make
    echo "Adding driver..."
    sudo insmod aesdchar.ko
    sudo dmesg | tail -n 10
    echo "Creating device..."
    sudo mknod /dev/aesdchar c 511 0
    sudo chmod 666 /dev/aesdchar
    echo "Runing test..."
    ./../assignment-autotest/test/assignment9/drivertest.sh
    echo "Deleting device..."
    sudo rmmod aesdchar
    sudo rm /dev/aesdchar
)