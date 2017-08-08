#!/bin/sh

ROOT=`realpath "$1"`
WANT="clang version 4.0.1 (tags/RELEASE_401/final)"

install_posix() {
	URL="$1"
	TAR_DIR="$2"
	VERSION=`$ROOT/host/bin/clang -v 2>&1 | head -n 1`
	if [ "$VERSION" != "$WANT" ]; then
		FILE="$ROOT/clang.tar.xz"
		rm -f "$FILE"
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
	install_posix "http://releases.llvm.org/4.0.1/clang+llvm-4.0.1-x86_64-linux-gnu-debian8.tar.xz"
	;;
Darwin)
	install_posix "http://releases.llvm.org/4.0.1/clang+llvm-4.0.1-x86_64-apple-darwin.tar.xz" "clang+llvm-4.0.1-x86_64-apple-macosx10.9.0"
	;;
MINGW64_NT*)
	VERSION=`/c/Program\ Files/LLVM/bin/clang.exe -v 2>&1 | head -n 1`
	if [ "$VERSION" != "$WANT" ]; then
		URL="http://releases.llvm.org/4.0.1/LLVM-4.0.1-win64.exe"
		FILE="$ROOT/LLVM-4.0.1-win64.exe"
		if [ ! -f "$ROOT/$FILE" ]; then
			rm -f "$FILE"
			curl "$URL" -o "$FILE" || exit 1
		fi
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

