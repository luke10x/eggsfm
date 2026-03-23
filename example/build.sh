#!/bin/bash
# Build simple_demo.cpp for macOS

cd "$(dirname "$0")"

YMFM=../../ymfm/src
SDL_LIB=../../bowling/build/macos/usr/lib/libSDL2.a
SDL_INC=../../bowling/3rdparty/SDL/include

echo "Building simple_demo..."

g++ -std=c++17 -O2 \
    -I$SDL_INC -I$YMFM -I.. \
    $YMFM/ymfm_misc.cpp \
    $YMFM/ymfm_adpcm.cpp \
    $YMFM/ymfm_ssg.cpp \
    $YMFM/ymfm_opn.cpp \
    ../xfm_impl.cpp \
    simple_demo.cpp \
    $SDL_LIB \
    -framework Cocoa -framework IOKit -framework CoreVideo \
    -framework CoreAudio -framework AudioToolbox \
    -framework ForceFeedback -framework Carbon \
    -framework Metal -framework GameController -framework CoreHaptics \
    -o simple_demo

if [ -f simple_demo ]; then
    echo "Build successful! Run with: ./simple_demo"
else
    echo "Build failed!"
    exit 1
fi
