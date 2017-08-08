#!/bin/sh

D=$PWD

case `uname -s` in
Linux)
	BIN=$D/host-linux/bin
	;;
Darwin)
	BIN=$D/host-osx/bin
	;;
*)
	echo "unsupported host platform"
	exit 1
	;;
esac

case $1 in
arm)
	export CFLAGS="--target=armv7---linux-eabi -Os -Xclang -target-feature -Xclang -neon"
	MUSL_TGT="arm"
	CRT_TGT="arm"
	OUT="$D/target-arm"
	NINJA_TARGET="--target=armv7---linux-eabi -Xclang -target-feature -Xclang -neon"
	;;
x64)
	export CFLAGS="--target=x86_64--linux-gnu -Os"
	MUSL_TGT="x86_64"
	CRT_TGT="x86_64"
	OUT="$D/target-x64"
	NINJA_TARGET="--target=x86_64--linux-gnu"
	;;
*)
	echo "usage ./build-libc.sh [arm|x64]"
	exit 1
	;;
esac

rm -rf $OUT/*

echo "configure musl"
cd $D/musl
export CC="$BIN/clang"
export CROSS_COMPILE="$BIN/llvm-"
export LD="$BIN/ld.lld.exe"
export LIBCC="$OUT/libclang-rt.so"
./configure --target=$MUSL_TGT --prefix=$OUT --libdir=$OUT --syslibdir=$OUT || exit 1

echo "install musl headers"
make -e clean install-headers || exit 1

#echo "copying C++ headers"
#mkdir -p $OUT/include/c++
#cp -r $D/libcxx/include/* $OUT/include/c++/ || exit 1
#cp -r $D/libcxxabi/include/* $OUT/include/c++/ || exit 1
#cp -r $D/libunwind/include/* $OUT/include/c++/ || exit 1
#rm $OUT/include/c++/CMakeLists.txt

cd $D
F=$D/build-libc.ninja
echo "TARGET=$NINJA_TARGET" > $F
echo "OUT=$OUT" >> $F
echo "include build-libc-common.ninja" >> $F

C="compiler-rt/lib/builtins/*.c"
CA="compiler-rt/lib/builtins/$CRT_TGT/*.c"
SA="compiler-rt/lib/builtins/$CRT_TGT/*.S"

echo "disabling compiler-rt files with architecture overrides"
mkdir compiler-rt/lib/builtins/disabled
mv compiler-rt/lib/builtins/disabled/* compiler-rt/lib/builtins
for c in $CA; do
	mv "compiler-rt/lib/builtins/`basename -s .c $c`.c" compiler-rt/lib/builtins/disabled/
done
for s in $SA; do
	mv "compiler-rt/lib/builtins/`basename -s .S $s`.c" compiler-rt/lib/builtins/disabled/
done

echo "generate ninja for compiler-rt"
ls $C $CA | sed -e 's#\(.*\)#build $OUT/obj/\1.o: cc \1#g' >> $F
ls $SA | sed -e 's#\(.*\)#build $OUT/obj/\1.o: as \1#g' >> $F

echo 'build $OUT/libclang-rt.a: lib $' >> $F
ls $C $CA $SA | sed -e 's#\(.*\)# $OUT/obj/\1.o $#g' >> $F
echo >> $F

echo 'build $OUT/libclang-rt.so: dll $' >> $F
ls $C $CA $SA | sed -e 's#\(.*\)# $OUT/obj/\1.o $#g' >> $F
echo >> $F

mv compiler-rt/lib/builtins/disabled/* compiler-rt/lib/builtins

echo "generating ninja for libcpp (libcxx+libcxxabi+libunwind)"

#P="libunwind/src/libunwind.cpp libunwind/src/Unwind-EHABI.cpp libunwind/src/*.S libcxx/src/*.cpp libcxxabi/src/*.cpp"
P="libcxx/src/*.cpp libcxxabi/src/*.cpp"
ls $P | sed -e 's#\(.*\)#build $OUT/obj/\1.o: cxx \1#g' >> $F

echo 'build $OUT/libcpp.so: dll $' >> $F
ls $P | sed -e 's#\(.*\)# $OUT/obj/\1.o $#g' >> $F
echo >> $F

echo 'build $OUT/libcpp.a: lib $' >> $F
ls $P | sed -e 's#\(.*\)# $OUT/obj/\1.o $#g' >> $F
echo >> $F

echo "building compiler-rt"
export PATH="$BIN:$PATH"
ninja -f $F $OUT/libclang-rt.so $OUT/libclang-rt.a || exit 1

echo "build musl"
cd $D/musl
make -e install || exit 1

#echo "building c++ library"
#cd $D
#ninja -f $F $OUT/libcpp.so $OUT/libcpp.a || exit 1

# get rid of the musl clang wrappers which we don't want
cd $D
rm -rf $OUT/bin
