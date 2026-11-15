#!/usr/bin/env bash
# Navoiy TTS (CosyVoice2 + aisha-org/navoiy-tts) ni WSL2 Ubuntu'ga o'rnatadi: ~/navoiy
# PyTorch - cu128 (RTX 50xx Blackwell uchun), Python 3.10 (uv bilan). Takroriy ishga tushirsa bo'ladi.
set -euo pipefail
ILDIZ="$HOME/navoiy"
mkdir -p "$ILDIZ" && cd "$ILDIZ"
export PATH="$HOME/.local/bin:$PATH"

command -v uv >/dev/null || curl -LsSf https://astral.sh/uv/install.sh | sh
[ -d CosyVoice ] || git clone --recursive --depth 1 https://github.com/FunAudioLLM/CosyVoice.git
[ -d .venv ] || uv venv -p 3.10 .venv
source .venv/bin/activate

uv pip install torch==2.7.1 torchaudio==2.7.1 --index-url https://download.pytorch.org/whl/cu128
# CosyVoice talablari: torch o'zimizniki (eski cu121 Blackwell'da ishlamaydi), TensorRT/DeepSpeed kerak emas
# pyworld - faqat o'qitish uchun va C++ kompilyator so'raydi (WSL'da yo'q)
grep -viE '^(torch|torchaudio|deepspeed|tensorrt|onnxruntime-gpu|pyworld|--)' CosyVoice/requirements.txt \
	| sed -E 's/^openai-whisper==.*/openai-whisper==20250625/' > talab.txt   # eski whisper triton<3 so'raydi, torch 2.7 - triton 3.3
# openai-whisper yig'ilishi pkg_resources so'raydi - setuptools 81+ da u yo'q
echo 'setuptools<81' > yigish.txt
uv pip install -r talab.txt onnxruntime --build-constraint yigish.txt torch==2.7.1 torchaudio==2.7.1 --index-strategy unsafe-best-match \
	--extra-index-url https://download.pytorch.org/whl/cu128
uv pip install huggingface_hub==0.36.2 numpy==1.26.4 transformers==4.51.3 \
	torch==2.7.1 torchaudio==2.7.1 --index-strategy unsafe-best-match --extra-index-url https://download.pytorch.org/whl/cu128

bash "$(dirname "$0")/pyworld.sh"

python - <<'EOF'
from huggingface_hub import snapshot_download
snapshot_download("FunAudioLLM/CosyVoice2-0.5B", local_dir="models/CosyVoice2-0.5B")
snapshot_download("aisha-org/navoiy-tts", local_dir="models/navoiy-tts")
EOF
python -c "import torch; print('torch', torch.__version__, 'cuda', torch.cuda.is_available(), torch.cuda.get_device_name(0))"
du -sh models/* .venv
echo TAYYOR
