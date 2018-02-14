# About
This repository contains a ninja & clang based toolchain that allows you to easily manage both native and arm-raw or arm-linux executables.

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