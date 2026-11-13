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
# asosiy STT: rubaiSTT v2 medium Q8 (Apache-2.0) + transcribe.cpp v0.4.1 (MIT, Vulkan+CPU) - o'yin ichida DLL (BozorRubai)
mkdir -p rubai
if [ ! -s rubai/transcribe.dll ]; then
  curl -sSL --fail -o rubai.tar.gz "https://github.com/handy-computer/transcribe.cpp/releases/download/v0.4.1/transcribe-native-0.4.1-windows-x86_64-cpu-vulkan.tar.gz"
  tar -xzf rubai.tar.gz -C rubai --strip-components=1 && rm rubai.tar.gz
  echo "c00cbe50a845003f986d3ed0ee149c08f576d0f3cc54c5346033608185a000d5  rubai/transcribe.dll" | sha256sum -c -
fi
if [ ! -s rubai/rubaistt.gguf ]; then
  curl -SL --fail -o rubai/rubaistt.gguf.part "https://huggingface.co/nizom333/rubaistt-v2-medium-gguf/resolve/main/rubaistt-v2-medium-Q8_0.gguf"
  mv rubai/rubaistt.gguf.part rubai/rubaistt.gguf
fi
echo "2b7a482a4272699a7f54f8c85b48ea1adaefe6a93561a39f923c01a77104d333  rubai/rubaistt.gguf" | sha256sum -c -
echo "AI/stt tayyor"
