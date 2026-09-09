echo Building Debug, this might take a while!
rd /s /q build\test\
cmake -DCMAKE_BUILD_TYPE=Test -DOPENSSL_ROOT_DIR=C:/msys64/ucrt64 -DCMAKE_EXPORT_COMPILE_COMMANDS:BOOL=TRUE -S %cd% -B %cd%\build\test -G Ninja
cmake --build build\test
build\test\ICPAK.exe %*