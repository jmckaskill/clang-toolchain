# About
This repository contains a ninja & clang based toolchain that allows you to easily manage both native and arm-raw or arm-linux executables.

# Versions
The most recent build uses the current versions:
xz - 5.2.3 (windows only)
ninja - 1.8.2 (windows only)
clang - 5.0.1
protoc - 3.5.1

# Updating
Every so often we may update the clang version, c libraries, etc.
The process to do so is as follows:
1. Update the arm built libraries
2. Update the linux, osx, and windows host packages
3. Upload new tgz to online storage
4. Enable public URL
5. Update installer to use new URLs

For linux and osx updating the host packages includes the following:
1. Download binaries:
- clang from http://releases.llvm.org/download.html
- protoc from https://github.com/google/protobuf/releases
2. Unpack to local directory
3. Rename bin/ld.lld to bin/ld.lld.exe
4. Add build date
date --utc --iso-8601=seconds > build-date.txt
5. Rename unpacked directory to host
6. Repack
tar -cJ --options='compression-level=9' -f host-linux-2018-02-14.txz host

For linux use the most recent x86_64 ubuntu install.

For Windows:
1. Download latest executables:
- protoc from https://github.com/google/protobuf/releases
- xz from https://tukaani.org/xz/
- clang (64 bit) from http://releases.llvm.org/download.html
- ninja from https://github.com/ninja-build/ninja/releases
2. Unpack all to a local directory named host
3. Clean up files into bin & include correctly
- e.g. protoc.exe should be in host\bin\protoc.exe
2. Install clang. It is distributed upstream as a setup exe
3. Copy C:\Program Files\LLVM\* to host\*
4. Add built date
date --utc --iso-8601=seconds > build-date.txt
5. Use 7zip to compress the host folder to host-win64-<date>.txz
- Add to tar archive first and then compress the tar file
