#!/bin/bash
# Lokal STT (mikrofon bilan gapirish) fayllari: CrispASR (MIT) + NVIDIA FastConformer uz CTC q8_0 (CC-BY-4.0).
# O'yin bilan birga tarqatiladi (BozorUnreal.Build.cs: RuntimeDependencies). Git'ga kirmaydi.
set -eu
cd "$(dirname "$0")"
V=v0.8.53
ZIP_SHA=6dbb713c537248d7   # GitHub reliz digest boshi (0-bosqichda tekshirilgan)
if [ ! -s crispasr.exe ]; then
  curl -sSL --fail -o crispasr.zip "https://github.com/CrispStrobe/CrispASR/releases/download/$V/crispasr-windows-x86_64-cpu.zip"
  sha256sum crispasr.zip | grep -q "^$ZIP_SHA" || { echo "crispasr.zip SHA256 mos emas"; exit 1; }
  mkdir -p _zip && unzip -oq crispasr.zip -d _zip
  cp _zip/crispasr-windows-x86_64-cpu/{crispasr.exe,openblas.dll,LICENSE,THIRD_PARTY_NOTICES.txt} .
  rm -rf _zip crispasr.zip
fi
[ -s stt-uz.gguf ] || curl -sSL --fail -o stt-uz.gguf \
  "https://huggingface.co/cstr/stt-uz-fastconformer-hybrid-ctc-large-GGUF/resolve/main/stt-uz-fastconformer-hybrid-ctc-large-q8_0.gguf"
echo "AI/stt tayyor"
