#!/bin/bash

# create / clean bin directory
if [ ! -d "bin" ]; then
    mkdir bin
else
    rm -f bin/*
fi

# EP
g++ -g -O0 -std=c++17 -I . \
    -o bin/interrupts_EP_101258593 \
    interrupts_EP_101258593.cpp

# RR
g++ -g -O0 -std=c++17 -I . \
    -o bin/interrupts_RR_101258593 \
    interrupts_RR_101258593.cpp

# EP + RR
g++ -g -O0 -std=c++17 -I . \
    -o bin/interrupts_EP_RR_101258593 \
    interrupts_EP_RR_101258593.cpp
