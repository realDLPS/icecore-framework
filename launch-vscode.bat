call emsdk/emsdk activate latest
call emsdk/emsdk_env.bat
emcc --clear-cache
embuilder build sysroot
code .
pause