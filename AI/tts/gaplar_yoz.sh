#!/usr/bin/env bash
# gaplar_yoz.py ni Navoiy muhitida ishga tushiradi (argumentlar o'tadi). Log: gaplar_yoz.log
cd "$(dirname "$0")"
"$HOME/navoiy/.venv/bin/python" gaplar_yoz.py "$@" 2>&1 | grep --line-buffered -E "^[0-9]+/|bo'laklar|Error|error|Traceback|File " | tee gaplar_yoz.log
