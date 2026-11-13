#!/bin/bash
# Sotuvchi "tili" (UBozorAiMiya) uchun fayllar: llama.cpp server (Vulkan + CPU) va Qwen3-4B-Instruct-2507 (Apache-2.0).
# Natija: AI/miya/llama/llama-server.exe (+ dll), AI/miya/model.gguf. Git'ga kirmaydi, o'yin bilan birga paketlanadi.
set -eu
cd "$(dirname "$0")"
GH=https://github.com/ggml-org/llama.cpp/releases/download/b12290
MODEL=https://huggingface.co/unsloth/Qwen3-4B-Instruct-2507-GGUF/resolve/main/Qwen3-4B-Instruct-2507-Q4_K_M.gguf
MODEL_SHA="3605803b982cb64aead44f6c1b2ae36e3acdb41d8e46c8a94c6533bc4c67e597"

if [ ! -f llama/llama-server.exe ]; then
	mkdir -p vendor llama
	curl -sSL --fail -o vendor/vulkan.zip "$GH/llama-b12290-bin-win-vulkan-x64.zip"
	unzip -qo vendor/vulkan.zip -d vendor/vulkan
	cp vendor/vulkan/llama-server.exe vendor/vulkan/LICENSE* llama/
	# faqat server kerak qiladigan dll'lar
	for f in vendor/vulkan/*.dll; do
		case "$(basename "$f")" in llama-batched-bench-impl.dll|llama-bench-impl.dll|llama-cli-impl.dll|llama-completion-impl.dll) ;; *) cp "$f" llama/ ;; esac
	done
	echo "llama-server: tayyor"
fi
if [ ! -s model.gguf ]; then
	curl -SL --fail -o model.gguf.part "$MODEL" && mv model.gguf.part model.gguf
	echo "model: tayyor"
fi
[ -n "$MODEL_SHA" ] && echo "$MODEL_SHA  model.gguf" | sha256sum -c -
echo TAYYOR
