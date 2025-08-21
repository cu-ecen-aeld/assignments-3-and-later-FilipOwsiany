#!/bin/bash
(
    cd .. || exit 1
    pwd
    echo "Deleting device..."
    sudo rmmod aesdchar
    sudo rm /dev/aesdchar
)