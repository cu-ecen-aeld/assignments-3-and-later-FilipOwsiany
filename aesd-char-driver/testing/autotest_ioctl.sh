#!/bin/bash

make clean -C ioctl_test
make -C ioctl_test
echo "Building ioctl test..."
if [ $? -ne 0 ]; then
    echo "Build failed."
    exit 1
fi
echo "Running ioctl test..."
./ioctl_test/ioctl_test
if [ $? -ne 0 ]; then
    echo "Ioctl test failed."
    exit 1
fi
echo "Ioctl test passed."