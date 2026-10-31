#!/bin/sh
# Installs what make_hd_pack.sh needs into a "venv" folder next to this file.
# Optional argument: cpu, cu126 (default, GTX 900 to RTX 40) or cu128 (RTX 50).
# On macOS the default PyTorch (Apple GPU through MPS) is used.
set -e
cd "$(dirname "$0")"
CUDA="${1:-cu126}"
python3 -m venv venv
venv/bin/python -m pip install --upgrade pip
if [ "$(uname)" = Darwin ]; then
    venv/bin/python -m pip install torch torchvision
else
    venv/bin/python -m pip install torch torchvision --index-url "https://download.pytorch.org/whl/$CUDA"
fi
venv/bin/python -m pip install -r requirements.txt
echo
echo 'Done. Now run: ./make_hd_pack.sh "path/to/Digimon World (USA).cue"'
