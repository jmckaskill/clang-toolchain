#!/bin/sh

ROOT="$PWD"
BIN="$PWD/host/bin"
OUT="$PWD/lib/arm"

MUSL="$PWD/musl-1.1.16"
COMPILER_RT="$PWD/compiler-rt-4.0.1.src"
NINJA_COMMON="$PWD/build-libc-common.ninja"

export CFLAGS="--target=armv7---linux-eabi -Os -Xclang -target-feature -Xclang -neon"
NINJA_TARGET="--target=armv7---linux-eabi -Xclang -target-feature -Xclang -neon"

rm -rf "$OUT"
mkdir -p "$OUT"

echo "configure musl"
cd $MUSL
export CC="$BIN/clang"
export CROSS_COMPILE="$BIN/llvm-"
export LD="$BIN/ld.lld.exe"
export LIBCC="$OUT/libclang-rt.so"
./configure --target=arm --prefix=$OUT --libdir=$OUT --syslibdir=$OUT || exit 1

echo "install musl headers"
make -e clean install-headers || exit 1

#echo "copying C++ headers"
#mkdir -p $OUT/include/c++
#cp -r $D/libcxx/include/* $OUT/include/c++/ || exit 1
#cp -r $D/libcxxabi/include/* $OUT/include/c++/ || exit 1
#cp -r $D/libunwind/include/* $OUT/include/c++/ || exit 1
#rm $OUT/include/c++/CMakeLists.txt

cd "$OUT"
F="build-libc.ninja"
echo "TARGET=$NINJA_TARGET" > $F
echo "OUT=$OUT" >> $F
echo "BIN=$BIN" >> $F
echo "include $NINJA_COMMON" >> $F

C="$COMPILER_RT/lib/builtins/*.c"
CA="$COMPILER_RT/lib/builtins/arm/*.c"
SA="$COMPILER_RT/lib/builtins/arm/*.S"

echo "disabling compiler-rt files with architecture overrides"
mkdir "$COMPILER_RT/lib/builtins/disabled"
mv "$COMPILER_RT/lib/builtins/disabled/*" "$COMPILER_RT/lib/builtins"
for c in $CA; do
	mv "$COMPILER_RT/lib/builtins/`basename -s .c $c`.c" "$COMPILER_RT/lib/builtins/disabled/"
done
for s in $SA; do
	mv "$COMPILER_RT/lib/builtins/`basename -s .S $s`.c" "$COMPILER_RT/lib/builtins/disabled/"
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

mv "$COMPILER_RT/lib/builtins/disabled/*" "$COMPILER_RT/lib/builtins"

echo "generating ninja for libcpp (libcxx+libcxxabi+libunwind)"

#P="libunwind/src/libunwind.cpp libunwind/src/Unwind-EHABI.cpp libunwind/src/*.S libcxx/src/*.cpp libcxxabi/src/*.cpp"
#P="libcxx/src/*.cpp libcxxabi/src/*.cpp"
#ls $P | sed -e 's#\(.*\)#build $OUT/obj/\1.o: cxx \1#g' >> $F

#echo 'build $OUT/libcpp.so: dll $' >> $F
#ls $P | sed -e 's#\(.*\)# $OUT/obj/\1.o $#g' >> $F
#echo >> $F

#echo 'build $OUT/libcpp.a: lib $' >> $F
#ls $P | sed -e 's#\(.*\)# $OUT/obj/\1.o $#g' >> $F
#echo >> $F

echo "building compiler-rt"
ninja -f "$F" "$OUT/libclang-rt.so" "$OUT/libclang-rt.a" || exit 1

echo "build musl"
cd "$MUSL"
make -e install || exit 1
cd "$ROOT"

#echo "building c++ library"
#cd $D
#ninja -f $F $OUT/libcpp.so $OUT/libcpp.a || exit 1

# get rid of the musl clang wrappers which we don't want
rm -rf "$OUT/bin"
rm -f "$OUT/ld-musl-arm.so.1" 
rm -rf "$OUT/build-libc.ninja" "$OUT/obj" "$OUT/.ninja_deps" "$OUT/.ninja_log"

date --utc --iso-8601=seconds > "$OUT/build-date.txt"
rm -f libarm.txz
tar -cJf libarm.txz lib/arm || exit 1

