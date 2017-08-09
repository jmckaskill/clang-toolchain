#!/bin/sh

ROOT=`realpath "$1"`
WANT="clang version 4.0.1 (tags/RELEASE_401/final)"

echo "installing clang"

install() {
	URL="$1"
	TAR_DIR="$2"
	VERSION=`$ROOT/host/bin/clang -v 2>&1 | head -n 1`
	if [ "$VERSION" != "$WANT" ]; then
		FILE="$ROOT/clang.txz"
		rm -f "$FILE"
		echo "downloading clang"
		curl "$URL" -o "$FILE" || exit 1
		echo "unpacking clang"
		cd "$ROOT" || exit 1
		rm -rf "$TAR_DIR" host || exit 1
		tar -xJf "$FILE" || exit 1
		rm -f "$FILE"
		mv "$ROOT/$TAR_DIR" "$ROOT/host" || exit 1

		VERSION=`$ROOT/host/bin/clang -v 2>&1 | head -n 1`
		if [ "$VERSION" != "$WANT" ]; then
			echo "clang failed to install"
			exit 1
		fi
		(cd "$ROOT/host/bin" && ln -s ld.lld ld.lld.exe) || exit 1
	fi
}

case `uname -s` in
Linux)
	install "http://releases.llvm.org/4.0.1/clang+llvm-4.0.1-x86_64-linux-gnu-debian8.tar.xz" clang+llvm-4.0.1-x86_64-linux-gnu-debian8
	;;
Darwin)
	install "http://releases.llvm.org/4.0.1/clang+llvm-4.0.1-x86_64-apple-darwin.tar.xz" clang+llvm-4.0.1-x86_64-apple-macosx10.9.0
	;;
MINGW64_NT*)
	install "https://storage.googleapis.com/ctct-clang-toolchain/clang-win64-4.0.1.txz" LLVM
	VERSION=`/c/Program\ Files/LLVM/bin/clang.exe -v 2>&1 | head -n 1`
	if [ "$VERSION" != "$WANT" ]; then
		URL="http://releases.llvm.org/4.0.1/LLVM-4.0.1-win64.exe"
		FILE="$ROOT/LLVM-4.0.1-win64.exe"
		echo "downloading clang"
		rm -f "$FILE"
		curl "$URL" -o "$FILE" || exit 1
		echo "installing clang"
		"$FILE" || exit 1
		rm -f "$FILE"

		VERSION=`/c/Program\ Files/LLVM/bin/clang.exe -v 2>&1 | head -n 1`
		if [ "$VERSION" != "$WANT" ]; then
			echo "clang failed to install"
			exit 1
		fi
	fi
	;;
*)
	echo "unsupported host platform"
	uname -s
	exit 1
	;;
esac

echo "installing libc"

LIBC_HAVE=`cat $ROOT/lib/arm/build-date.txt`
LIBC_WANT="2017-08-08T21:16:39+00:00"

if [ "$LIBC_HAVE" != "$LIBC_WANT" ]; then
	echo "downloading libc"
	rm -f "$ROOT/lib/arm"
	cd "$ROOT" || exit 1
	rm -f libarm.txz
	curl "https://storage.googleapis.com/ctct-clang-toolchain/libarm-2017-08-08.txz" -o libarm.txz || exit 1

	echo "unpacking libc"
	tar -xJf libarm.txz || exit 1

	LIBC_HAVE=`cat $ROOT/lib/arm/build-date.txt`
	if [ "$LIBC_HAVE" != "$LIBC_WANT" ]; then
		echo "libc failed to install"
		exit 1
	fi

	rm -f libarm.txz
fi




