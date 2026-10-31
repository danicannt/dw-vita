#!/bin/sh
# ./make_hd_pack.sh "path/to/Digimon World (USA).cue" [options]   (--help for all)
D="$(cd "$(dirname "$0")" && pwd)"
[ -x "$D/venv/bin/python" ] || { echo "Run ./setup_linux.sh first." >&2; exit 1; }
exec "$D/venv/bin/python" "$D/make_hd_pack.py" "$@"
