"""Archive raw test frames under the outer out directory; preview by default."""
from pathlib import Path
import argparse
import hashlib
import json
import stat
from datetime import datetime
from zipfile import ZipFile, ZIP_DEFLATED

OUTPUT = Path(__file__).resolve().parents[2] / 'out'


def checked(path):
    path = path.absolute()
    if not path.is_relative_to(OUTPUT) or path == OUTPUT:
        raise RuntimeError(f'Outside output directory: {path}')
    for parent in (path, *path.parents):
        if parent.exists() and getattr(parent.lstat(), 'st_file_attributes', 0) & stat.FILE_ATTRIBUTE_REPARSE_POINT:
            raise RuntimeError(f'Reparse point: {parent}')
        if parent == OUTPUT:
            break
    return path


def digest(path):
    with path.open('rb') as stream:
        return hashlib.file_digest(stream, 'sha256').hexdigest()


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--apply', action='store_true', help='Archive, verify, then remove original raw frames')
    args = parser.parse_args()
    if not OUTPUT.is_dir():
        raise RuntimeError('Expected the project outer out directory')
    files = sorted(checked(p) for folder in ('screenshots', 'validation')
                   for p in checked(OUTPUT / folder).rglob('*.ppm') if p.is_file())
    total = sum(p.stat().st_size for p in files)
    print(f'{len(files)} raw test frames; {total / 1024**2:.2f} MiB; apply={args.apply}')
    if not args.apply or not files:
        return
    stamp = datetime.now().strftime('%Y%m%d-%H%M%S-%f')
    archive = checked(OUTPUT / 'archive' / f'raw-test-frames-{stamp}.zip')
    manifest = checked(OUTPUT / 'maintenance' / f'organize-{stamp}.json')
    archive.parent.mkdir(parents=True, exist_ok=True)
    manifest.parent.mkdir(parents=True, exist_ok=True)
    # Protect user output, appearance preferences and the newly designed icons.
    protected = [p for p in OUTPUT.iterdir() if p.is_file() and p.suffix in ('.vts', '.csv')]
    for folder in ('2d', 'ui', 'assets'):
        base = checked(OUTPUT / folder)
        if base.is_dir():
            protected.extend(p for p in base.rglob('*') if p.is_file())
    before = {str(p.relative_to(OUTPUT)): digest(checked(p)) for p in protected}
    entries = [{'path': p.relative_to(OUTPUT).as_posix(), 'bytes': p.stat().st_size,
                'sha256': digest(p)} for p in files]
    with ZipFile(archive, 'x', compression=ZIP_DEFLATED, compresslevel=6) as bundle:
        for p, entry in zip(files, entries):
            bundle.write(p, entry['path'])
    # Read every archived byte back before removing any original.
    with ZipFile(archive) as bundle:
        for entry in entries:
            with bundle.open(entry['path']) as stream:
                if hashlib.file_digest(stream, 'sha256').hexdigest() != entry['sha256']:
                    raise RuntimeError(f'Archive hash mismatch: {entry["path"]}')
    for entry in entries:
        if digest(checked(OUTPUT / entry['path'])) != entry['sha256']:
            raise RuntimeError(f'Frame changed during archive: {entry["path"]}')
    for p in protected:
        if digest(checked(p)) != before[str(p.relative_to(OUTPUT))]:
            raise RuntimeError(f'Protected output changed: {p}')
    report = {'archive': str(archive.relative_to(OUTPUT)), 'originalBytes': total,
              'archiveBytes': archive.stat().st_size, 'entries': entries,
              'protectedSha256': before, 'removedOriginals': []}
    # Persist the recovery index first, then record each completed file removal.
    manifest.write_text(json.dumps(report, ensure_ascii=False, indent=2), encoding='utf-8')
    for entry in entries:
        source = checked(OUTPUT / entry['path'])
        if digest(source) != entry['sha256']:
            raise RuntimeError(f'Frame changed before removal: {source}')
        source.unlink()
        report['removedOriginals'].append(entry['path'])
        manifest.write_text(json.dumps(report, ensure_ascii=False, indent=2), encoding='utf-8')
    print(f'Verified archive: {archive}')
    print(f'Saved {(total - archive.stat().st_size) / 1024**2:.2f} MiB; protected {len(before)} files')
    print(f'Recovery manifest: {manifest}')


if __name__ == '__main__':
    main()
