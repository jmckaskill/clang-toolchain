@echo off
set INSTALL=%~dp0\msvc
set SRC=%~dp0
set BRANCH=release_39

del /Q %INSTALL%
mkdir %INSTALL%

move %SRC%\clang %SRC%\llvm\tools\
move %SRC%\lld %SRC%\llvm\tools\

mkdir %SRC%\llvm-build
cd %SRC%\llvm-build
"c:\Program Files\CMake\bin\cmake.exe" -G Ninja -DCMAKE_BUILD_TYPE=Release -DCMAKE_INSTALL_PREFIX=%INSTALL% %SRC%/llvm
ninja install-llvm-ar install-clang install-clang-headers tools/lld/tools/lld/install/local install-llvm-as install-llvm-ranlib install-llc install-llvm-config install-llvm-objdump install-llvm-readobj
cd %~dp0
