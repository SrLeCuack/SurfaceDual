#!/bin/bash
# Compila Agente.java a agente.dex (mismas herramientas que F3/audio/compilar.sh).
cd "$(dirname "$0")"; H=../../herramientas; JDK=$(ls -d $H/jdk-21*); BT=$(ls -d $H/sdk/build-tools/*); AJ=$H/sdk/platforms/android-37.0/android.jar
set -e
# Sin NDK ni libc: ELF Linux estático, generado por el LLVM ARM64 instalado.
"/c/Program Files/LLVM/bin/clang.exe" --target=aarch64-linux-android34 -fuse-ld=lld -nostdlib -static -ffreestanding -fno-builtin -fno-stack-protector -O2 -Wall -Wextra -Werror -Wl,-e,_start -o surfacedual-pen pluma.c
export PATH="$(realpath "$JDK")/bin:$PATH"
test -d clases || mkdir clases
temporal=$(mktemp -d clases/compilar-temporal-XXXXXX)
case "$temporal" in clases/compilar-temporal-*) ;; *) exit 1 ;; esac
temporal="clases/${temporal##*/}"
trap 'rm -rf "$temporal"' EXIT
mkdir -p "$temporal/clases" "$temporal/dex" "$temporal/compilador"
jarandroid=$(cygpath -aw "$AJ")
javac -d "$temporal/compilador" Compilar.java
java -cp "$temporal/compilador" Compilar "$jarandroid" "$(cygpath -aw "$temporal/clases")" Agente.java
cmd //c "$(cygpath -w $BT/d8.bat)" --release --min-api 34 --lib "$jarandroid" --output "$(cygpath -w "$temporal/dex")" $(for f in "$temporal"/clases/*.class; do cygpath -w "$f"; done)
mv "$temporal/dex/classes.dex" agente.dex
ls -l agente.dex surfacedual-pen
