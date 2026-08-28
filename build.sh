#!/bin/bash
# 交叉编译 Llama Launcher (Linux → Windows)
# 依赖: x86_64-w64-mingw32-g++ / windres (sudo apt install g++-mingw-w64-x86-64)
set -e
cd "$(dirname "$0")"
mkdir -p dist
x86_64-w64-mingw32-windres src/resources.rc -o src/resources.o
x86_64-w64-mingw32-g++ src/main.cpp src/resources.o -o "dist/Llama Launcher.exe" -mwindows -municode -static -O2 -lwininet -lcomctl32
echo "OK → dist/Llama Launcher.exe"
