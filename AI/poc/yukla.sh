#!/bin/bash
# 0-bosqich: kerakli dastur va modellarni yuklash (har biri o'z papkasiga)
set -u
cd "$(dirname "$0")"
y() { [ -s "$2" ] && { echo "bor: $2"; return; }; echo "yuklanmoqda: $2"; curl -sSL --fail -o "$2.part" "$1" && mv "$2.part" "$2" || echo "XATO: $1"; }
GH=https://github.com
HF=https://huggingface.co
y $GH/ggml-org/llama.cpp/releases/download/b12290/llama-b12290-bin-win-cpu-x64.zip vendor/llama/llama.zip
y $GH/CrispStrobe/CrispASR/releases/download/v0.8.53/crispasr-windows-x86_64-cpu.zip vendor/crispasr/crispasr.zip
y $GH/handy-computer/transcribe.cpp/releases/download/v0.4.1/transcribe-native-0.4.1-windows-x86_64-cpu-vulkan.tar.gz vendor/transcribe/transcribe.tar.gz
y $GH/rhasspy/piper/releases/download/2023.11.14-2/piper_windows_amd64.zip vendor/piper/piper.zip
y $HF/cstr/stt-uz-fastconformer-hybrid-ctc-large-GGUF/resolve/main/stt-uz-fastconformer-hybrid-ctc-large-q4_k.gguf models/stt-uz-fastconformer-q4_k.gguf
y $HF/nizom333/whisper-small-uzbek-gguf/resolve/main/whisper-small-uzbek-Q8_0.gguf models/whisper-small-uzbek-Q8_0.gguf
y $HF/NikolayKozloff/Qwen3-0.6B-Instruct-Uz-Q8_0-GGUF/resolve/main/qwen3-0.6b-instruct-uz-q8_0.gguf models/qwen3-0.6b-uz-q8_0.gguf
y "$HF/rhasspy/piper-voices/resolve/v1.0.0/kk/kk_KZ/issai/high/kk_KZ-issai-high.onnx" models/kk_KZ-issai-high.onnx
y "$HF/rhasspy/piper-voices/resolve/v1.0.0/kk/kk_KZ/issai/high/kk_KZ-issai-high.onnx.json" models/kk_KZ-issai-high.onnx.json
echo TAYYOR
