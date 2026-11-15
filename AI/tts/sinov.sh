#!/usr/bin/env bash
# sinov.py ni Navoiy muhitida ishga tushiradi. 1-argument - namuna ovoz (bo'lmasa Navoiy demosi)
cd "$(dirname "$0")"
REF="${1:-$HOME/navoiy/models/navoiy-tts/demo/warm_agent.wav}"
"$HOME/navoiy/.venv/bin/python" sinov.py --ref "$REF" --chiqish natija 2>&1 | tee natija.log | grep -vE 'it/s\]|%\|' | tail -40
