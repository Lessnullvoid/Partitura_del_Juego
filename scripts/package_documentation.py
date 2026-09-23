#!/usr/bin/env python3
"""Copy the museum/installation documentation; reject a stale offline reader."""
import hashlib
import json
from pathlib import Path
import shutil
import sys


def documentation_files(root):
    catalog = json.loads((root / 'docs/DOCUMENTOS_ENTREGA.json').read_text())
    return [entry['file'] for entry in catalog]


def reader_digest(root):
    names = ['DOCUMENTOS_ENTREGA.json', *documentation_files(root)]
    h = hashlib.sha256()
    for name in names:
        h.update(name.encode() + b'\0')
        h.update((root / 'docs' / name).read_bytes())
    return h.hexdigest()


def copy_documentation(root, package):
    reader = root / 'docs/LECTURA.html'
    expected = f'<meta name="pdj-documentation-sha256" content="{reader_digest(root)}">'
    if expected not in reader.read_text():
        raise SystemExit('Offline reader is stale: run scripts/render_documentation.mjs with Node and marked.')
    target = package / 'Documentacion'
    target.mkdir(parents=True, exist_ok=True)
    for name in [*documentation_files(root), 'LECTURA.html', 'DOCUMENTOS_ENTREGA.json']:
        shutil.copy2(root / 'docs' / name, target / name)
    readme = (root / 'distribution/README-macOS-test.md').read_text()
    (package / 'README.md').write_text(readme.replace('](../docs/', '](Documentacion/'))
    print(f'Copied {len(documentation_files(root))} guides and the offline reader.')


if __name__ == '__main__':
    copy_documentation(Path(sys.argv[1]), Path(sys.argv[2]))
