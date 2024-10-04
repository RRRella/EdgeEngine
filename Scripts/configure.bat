@echo off
mkdir "../build"
cmake -S .. -B "../build" -G "Visual Studio 17 2022"
pause