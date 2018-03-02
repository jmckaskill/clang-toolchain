#!/bin/sh

ROOT="$PWD"
BIN="$PWD/host/bin"

MUSL="$PWD/musl-1.1.16"
COMPILER_RT="$PWD/compiler-rt-4.0.1.src"
LIBCXX="$PWD/libcxx-4.0.1.src"
LIBCXXABI="$PWD/libcxxabi-4.0.1.src"
LIBUNWIND="$PWD/libunwind-4.0.1.src"
LIBGCC="$PWD/libgcc-7.3.0"

NINJA_COMMON="$PWD/build-libc-common.ninja"

case $1 in
armv7)
	export CFLAGS="--target=armv7---linux-eabi -Os -Xclang -target-feature -Xclang -neon"
	NINJA_TARGET="--target=armv7---linux-eabi -Xclang -target-feature -Xclang -neon"
	OUT="$PWD/lib/armv7"
	export LIBCC="$OUT/libclang-rt.so"
	;;
armv5)
	export CFLAGS="--target=armv5---linux-eabi -Os"
	NINJA_TARGET="--target=armv5---linux-eabi"
	OUT="$PWD/lib/armv5"
	export LIBCC="$OUT/libgcc.a"
	;;
*)
	echo "unknown target $1"
	exit 1
	;;
esac

export CC="$BIN/clang"
export CXX="$BIN/clang++"
export CROSS_COMPILE="$BIN/llvm-"
export LD="$BIN/ld.lld"

rm -rf "$OUT"
mkdir -p "$OUT"

if [ "$1" == "armv5" ]; then
	cp libgcc-armv5.a $LIBCC || exit 1
fi

cd $MUSL
echo "disabling musl files with overrides in compiler-rt"
mkdir disabled
mv src/string/arm/__aeabi_*.c disabled/

echo "configure musl"
./configure --target=arm --prefix=$OUT --libdir=$OUT --syslibdir=$OUT || exit 1

echo "install musl headers"
make -e clean install-headers || exit 1

cd "$OUT"
F="build-libc.ninja"
echo "TARGET=$NINJA_TARGET" > $F
echo "OUT=$OUT" >> $F
echo "BIN=$BIN" >> $F
echo "include $NINJA_COMMON" >> $F

if [ "$1" == "armv5" ]; then
	echo "create compiler-rt with symbols missing from libgcc.a"
	SA="aeabi_memset.S aeabi_memcpy.S aeabi_memmove.S"
	for s in $SA; do
		echo "build $OUT/obj/$s.o: cc $COMPILER_RT/lib/builtins/arm/$s" >> $F
	done
	echo "build $OUT/libclang-rt.a: lib $" >> $F
	for s in $SA; do
		echo " $OUT/obj/$s.o $" >> $F
	done
	echo >> $F

	echo "building compiler-rt"
	ninja -f "$F" "$OUT/libclang-rt.a" || exit 1
else

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

	echo "disabling libcxxabi overrides"
	mkdir "$LIBCXXABI/disabled"
	mv "$LIBCXXABI/src/cxa_noexception.cpp" "$LIBCXXABI/disabled/"

	mkdir "$LIBCXX/disabled"
	mv "$LIBCXX/src/debug.cpp" "$LIBCXX/disabled/"

	echo "generate ninja for compiler-rt"
	ls $C $CA | sed -e 's#\(.*\)#build $OUT/obj/\1.o: cc \1#g' >> $F
	ls $SA | sed -e 's#\(.*\)#build $OUT/obj/\1.o: as \1#g' >> $F

	echo 'build $OUT/libclang-rt.a: lib $' >> $F
	ls $C $CA $SA | sed -e 's#\(.*\)# $OUT/obj/\1.o $#g' >> $F
	echo >> $F

	echo 'build $OUT/libclang-rt.so: dll $' >> $F
	ls $C $CA $SA | sed -e 's#\(.*\)# $OUT/obj/\1.o $#g' >> $F
	echo >> $F

	echo "generating ninja for libcpp (libcxx+libcxxabi+libunwind)"

	P="$LIBUNWIND/src/libunwind.cpp $LIBUNWIND/src/Unwind-EHABI.cpp $LIBUNWIND/src/*.S $LIBCXX/src/*.cpp $LIBCXXABI/src/*.cpp"
	ls $P | sed -e 's#\(.*\)#build $OUT/obj/\1.o: cxx \1#g' >> $F

	echo 'build $OUT/libcpp.so: dll $' >> $F
	ls $P | sed -e 's#\(.*\)# $OUT/obj/\1.o $#g' >> $F
	echo >> $F

	echo 'build $OUT/libcpp.a: lib $' >> $F
	ls $P | sed -e 's#\(.*\)# $OUT/obj/\1.o $#g' >> $F
	echo >> $F

	mv $COMPILER_RT/lib/builtins/disabled/* "$COMPILER_RT/lib/builtins"
	mv $LIBCXXABI/disabled/* "$LIBCXXABI/src"
	mv $LIBCXX/disabled/* "$LIBCXX/src"

	echo "building compiler-rt"
	ninja -f "$F" "$OUT/libclang-rt.so" "$OUT/libclang-rt.a" || exit 1
fi

echo "build musl"
cd "$MUSL"
make -e install || exit 1
mv disabled/* src/string/arm/

if [ "$1" != "armv5" ]; then
	echo "copying c++ headers"
	rm -rf "$OUT/include/c++"
	mkdir "$OUT/include/c++"
	cp -vr $LIBUNWIND/include/* $LIBCXX/include/* $LIBCXXABI/include/* $OUT/include/c++/

	echo "building c++ library"
	cd "$OUT"
	ninja -f "$F" "$OUT/libcpp.so" "$OUT/libcpp.a" || exit 1
fi

# get rid of the musl clang wrappers which we don't want
rm -rf "$OUT/bin"
rm -f "$OUT/ld-musl-arm.so.1" 
rm -rf "$OUT/build-libc.ninja" "$OUT/obj" "$OUT/.ninja_deps" "$OUT/.ninja_log"

cd "$ROOT"
date --utc --iso-8601=seconds > "$OUT/build-date.txt"
TAR_FILE="libarm-`date --utc --iso-8601=date`.txz"
rm -f $TAR_FILE
tar -cJf $TAR_FILE lib || exit 1

