#!/bin/sh

if [ -n "$1" ]; then
	cd "$1"
fi

WANT="clang version 4.0.1 (tags/RELEASE_401/final)"

install_txz() {
	URL="$1"
	TAR_DIR="$2"
	VERSION=`host/bin/clang -v 2>&1 | head -n 1`
	if [ "$VERSION" != "$WANT" ]; then
		FILE="clang.txz"
		rm -f "$FILE"
		echo "downloading toolchain"
		curl -f "$URL" -o "$FILE" || exit 1
		echo "unpacking toolchain"
		rm -rf "$TAR_DIR" host || exit 1
		tar -xJf "$FILE" || exit 1
		rm -f "$FILE"
		mv "$TAR_DIR" host || exit 1

		VERSION=`host/bin/clang -v 2>&1 | head -n 1`
		if [ "$VERSION" != "$WANT" ]; then
			echo "toolchain failed to install"
			exit 1
		fi
		(cd "host/bin" && ln -s ld.lld ld.lld.exe) || exit 1
	fi
}

install_7z() {
	URL="$1"
	VERSION=`host/bin/clang -v 2>&1 | head -n 1`
	if [ "$VERSION" != "$WANT" ]; then
		FILE="clang.7z"
		rm -f "$FILE"
		echo "downloading toolchain"
		curl -f "$URL" -o "$FILE" || exit 1
		echo "unpacking toolchain"
		rm -rf host || exit 1
		/c/Program\ Files/7-Zip/7z.exe x clang.7z || exit 1

		VERSION=`host/bin/clang -v 2>&1 | head -n 1`
		if [ "$VERSION" != "$WANT" ]; then
			echo "toolchain failed to install"
			exit 1
		fi
	fi
}

echo "checking toolchain"

case `uname -s` in
Linux)
	install_txz "http://releases.llvm.org/4.0.1/clang+llvm-4.0.1-x86_64-linux-gnu-debian8.tar.xz" clang+llvm-4.0.1-x86_64-linux-gnu-debian8
	;;
Darwin)
	install_txz "http://releases.llvm.org/4.0.1/clang+llvm-4.0.1-x86_64-apple-darwin.tar.xz" clang+llvm-4.0.1-x86_64-apple-macosx10.9.0
	;;
MSYS*|MINGW*)
	install_7z "https://storage.googleapis.com/ctct-clang-toolchain/host-win64-2017-08-09.7z"
	# add host to path for xz so we can unpack libc below
	export PATH=$PATH:$PWD/host/bin
	;;
*)
	echo "unsupported host platform"
	uname -s
	exit 1
	;;
esac

echo "checking libc"

LIBC_HAVE=`cat lib/arm/build-date.txt`
LIBC_WANT="2017-08-11T15:16:01+00:00"
LIBC_URL="https://storage.googleapis.com/ctct-clang-toolchain/libarm-2017-08-11.txz"

if [ "$LIBC_HAVE" != "$LIBC_WANT" ]; then
	echo "downloading libc"
	rm -f lib/arm
	rm -f libarm.txz
	curl -f "$LIBC_URL" -o libarm.txz || exit 1

	echo "unpacking libc"
	tar -xJf libarm.txz || exit 1

	LIBC_HAVE=`cat lib/arm/build-date.txt`
	if [ "$LIBC_HAVE" != "$LIBC_WANT" ]; then
		echo "libc failed to install"
		exit 1
	fi

	rm -f libarm.txz
fi

echo "toolchain successfully installed"



