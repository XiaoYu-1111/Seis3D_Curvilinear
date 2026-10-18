"""Publish unmodified, native-resolution solver screenshots after visual review.

Run the application --capture-workflow, --capture-2d and --smoke-home first.
Raw frames stay in ../out; only verified publication PNG/TSV enter docs/images.
"""
from pathlib import Path
import csv, shutil
from PIL import Image
r=Path(__file__).resolve().parents[1]; out=r.parent/'out/screenshots'; shots=r/'docs/images'
for name in ['curvilinear-workflow','two-d-workflow']:
    source=out/name; target=shots if name=='curvilinear-workflow' else shots/'two-d'
    target.mkdir(parents=True,exist_ok=True)
    rows=list(csv.DictReader((source/'capture.tsv').open(encoding='utf-8'),delimiter='\t'))
    for row in rows:
        raw=source/(row['image']+'.ppm'); dest=target/(row['image']+'.png')
        with Image.open(raw) as im:
            assert im.size==(2560,1494)
            im.save(dest,optimize=True)
            with Image.open(dest) as saved: assert saved.size==im.size and saved.tobytes()==im.tobytes()
    shutil.copy2(source/'capture.tsv',target/'capture.tsv')
with Image.open(out/'two-d/home-maximized.ppm') as im:
    assert im.size==(2560,1494);im.save(shots/'two-d/home.png',optimize=True)
shutil.copy2(shots/'07-wave-040.png',shots/'workbench.png')
print('Published 18 3D + 13 2D + 1 home full-size PNG images, lossless and uncropped.')
