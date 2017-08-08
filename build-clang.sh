#!/bin/sh

ROOT=$PWD

case `uname -s` in
Darwin)
	INSTALL=$ROOT/host-osx
	;;
Linux)
	INSTALL=$ROOT/host-linux
	;;
*)
	echo "unsupported platform"
	exit 1
	;;
esac

echo "building $INSTALL"

rm -rf $INSTALL && mkdir -p $INSTALL
rm $ROOT/llvm/tools/clang $ROOT/llvm/tools/lld
ln -s $ROOT/clang $ROOT/llvm/tools/clang
ln -s $ROOT/lld $ROOT/llvm/tools/lld

mkdir -p $ROOT/llvm-build && cd $ROOT/llvm-build
cmake -G Ninja -DCMAKE_BUILD_TYPE=Release -DCMAKE_INSTALL_PREFIX=$INSTALL $ROOT/llvm || exit 1
ninja install-llvm-ar install-clang install-clang-headers install-llvm-as install-llvm-ranlib install-llc install-llvm-config install-llvm-objdump install-llvm-readobj bin/lld || exit 1
cp $ROOT/llvm-build/bin/ld $INSTALL/bin/ld
cd $INSTALL/bin/ld
ln -s ld ld.lld.exe
cd $ROOT
