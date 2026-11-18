#!/bin/bash
# 英語版: main.cpp を別の obj/output に分けてビルドする (kInkKind="en")
cd "$(dirname "$0")"
f=source/program/main.cpp; o=obj_en/$(echo $f | tr / _).o
mkdir -p obj_en
# main.cpp.o 以外のオブジェクト (crt0 等) を obj/ から引き継ぐ。main.cpp.o だけ en 版で上書きする
cp -n obj/*.o obj_en/ 2>/dev/null; true
SH=$(realpath ../shim); G=/usr/include/c++/13
COMMON="-mno-outline-atomics --target=aarch64-linux-gnu -mcpu=cortex-a57+crc+crypto -fPIC -fvisibility=hidden -O2 -g0 -ffunction-sections -fdata-sections -Wall -Wno-format-zero-length -DINKDRAW_BASELINE=1 -DINKDRAW_DUMP=0 -DINKDRAW_CAMPOS=1 -DINKDRAW_LIVEPOS=1 -DINKDRAW_HIDDRAW=1 -DINKDRAW_USE_PLAYER_POS=0 -DINKDRAW_KIND=\"en\" -D__SWITCH__ -D__RTLD_6XX__ -DEXL_LOAD_KIND=Module -DEXL_LOAD_KIND_ENUM=2 -DEXL_PROGRAM_ID=0x0100c2500fc20000 -I$(pwd)/source -fno-stack-protector -fno-builtin-printf"
INC="-nostdinc -isystem $SH -isystem $G -isystem /usr/include/x86_64-linux-gnu/c++/13 -isystem /usr/lib/llvm-18/lib/clang/18/include -isystem /usr/include -isystem /usr/include/x86_64-linux-gnu"
MODS=$(find source -mindepth 1 -maxdepth 1 -type d | sed 's/^/-I/')
rm -f $o
clang++ $COMMON $INC $MODS -std=gnu++23 -fno-rtti -fno-exceptions -fno-asynchronous-unwind-tables -fno-unwind-tables -c $f -o $o 2>&1 | grep -E "error|warning"
[ -s $o ] || { echo "BUILD FAILED (compile error)"; exit 1; }
ld.lld -T misc/link.ld --shared --export-dynamic --gc-sections --build-id=sha1 -init=exl_module_init -z max-page-size=0x1000 -z norelro -o tmt_en.elf obj_en/*.o 2>&1 | tail -3
../tc/elf2nso tmt_en.elf subsdk9_en | tail -1
ls -la subsdk9_en
llvm-nm -u tmt_en.elf
