#!/bin/bash
mkdir -p ./build/
g++ src/main.cpp src/sentinel/color.cpp src/sentinel/cmdargs.cpp src/sentinel/log.cpp  -Isrc/ -lncurses -o ./build/Sentinel 