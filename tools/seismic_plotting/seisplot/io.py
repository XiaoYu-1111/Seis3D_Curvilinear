"""Readers for 2-D exported models, snapshots and arbitrary-sized seismic records."""
from dataclasses import dataclass
from pathlib import Path
import csv, hashlib, json, re, struct, warnings
import numpy as np

@dataclass
class Gather:
    values: np.ndarray                 # [receiver, time], original velocity samples
    dt: float
    x: np.ndarray
    z: np.ndarray
    offset: np.ndarray
    @property
    def t(self): return np.arange(self.values.shape[1])*self.dt

def table(path):
    with Path(path).open(encoding='utf-8-sig', newline='') as f:
        return list(csv.DictReader(f))

def metadata(path):
    return json.loads(Path(path).read_text(encoding='utf-8-sig')) if Path(path).is_file() else {}

def read_segy(path, geometry_csv=None, sample_interval_s=None, source_x_m=0):
    """SEG-Y format 1/5/6. Check headers rather than assuming 250 or 241 traces.

    Explicit sample_interval_s overrides consistent header dt with a warning.
    A geometry CSV (x_m, optional z_m/offset_m) takes precedence over trace headers.
    Without it only Cartesian length coordinates are accepted (header unit 0/1).
    """
    path=Path(path); raw=path.read_bytes()
    if len(raw)<3600: raise ValueError(f'{path}: incomplete SEG-Y header')
    bh=raw[3200:3600]; fmt=struct.unpack_from('>H',bh,24)[0]
    widths={1:4,5:4,6:8}
    if fmt not in widths: raise ValueError(f'{path}: unsupported SEG-Y sample format {fmt}; use 1/5/6')
    extended=struct.unpack_from('>h',bh,304)[0]
    if extended<0: raise ValueError('Unknown-count extended SEG-Y headers are not supported')
    pos=3600+extended*3200; rows=[]; coords=[]; dts=[]; binary_dt=struct.unpack_from('>H',bh,16)[0]
    if pos>len(raw): raise ValueError('Truncated extended text headers')
    while pos<len(raw):
        if pos+240>len(raw): raise ValueError(f'{path}: truncated trace header')
        h=raw[pos:pos+240];ns=struct.unpack_from('>H',h,114)[0] or struct.unpack_from('>H',bh,20)[0]
        trace_dt=struct.unpack_from('>H',h,116)[0]
        if trace_dt: dts.append(trace_dt)
        pos+=240;end=pos+ns*widths[fmt]
        if ns==0 or end>len(raw): raise ValueError(f'{path}: truncated/empty trace')
        if fmt==1:
            word=np.frombuffer(raw[pos:end],dtype='>u4'); sign=np.where(word>>31,-1.,1.)
            a=sign*(word & 0x00ffffff)/2**24 * np.power(16.,((word>>24)&127).astype(int)-64)
        else:a=np.frombuffer(raw[pos:end],dtype='>f4' if fmt==5 else '>f8').astype(float)
        rows.append(a);pos=end
        scalar=struct.unpack_from('>h',h,70)[0];factor=scalar if scalar>0 else 1/abs(scalar) if scalar<0 else 1
        unit=struct.unpack_from('>H',h,88)[0]
        coords.append((struct.unpack_from('>i',h,80)[0]*factor,unit))
    if not rows or len({r.size for r in rows})!=1: raise ValueError('Empty or variable-length SEG-Y traces')
    headers=set(dts+([binary_dt] if binary_dt else []))
    if len(headers)>1: raise ValueError(f'{path}: conflicting trace/binary sample intervals {headers}')
    header_dt=next(iter(headers))*1e-6 if headers else None
    dt=sample_interval_s if sample_interval_s is not None else header_dt
    if dt is None or not np.isfinite(dt) or dt<=0: raise ValueError('Missing dt: specify sample_interval_s')
    if sample_interval_s is not None and header_dt is not None and not np.isclose(dt,header_dt):
        warnings.warn(f'{path.name}: explicit dt={dt} overrides header dt={header_dt}',stacklevel=2)
    a=np.stack(rows)
    if not np.isfinite(a).all(): raise ValueError(f'{path}: nonfinite samples')
    if geometry_csv is not None:
        g=table(geometry_csv)
        if len(g)!=a.shape[0]: raise ValueError('Geometry row count differs from SEG-Y trace count')
        x=np.array([float(r['x_m']) for r in g]);z=np.array([float(r.get('z_m',0)) for r in g])
        offset=np.array([float(r.get('offset_m',x[i]-source_x_m)) for i,r in enumerate(g)])
    else:
        if any(u not in (0,1) for _,u in coords): raise ValueError('Angular coordinates require explicit geometry_csv')
        x=np.array([c[0] for c in coords]);z=np.zeros(len(x));offset=x-source_x_m
        if len(x)>1 and np.unique(x).size!=len(x): raise ValueError('Missing/repeated X coordinates: provide geometry_csv')
    if not all(np.isfinite(v).all() for v in (x,z,offset)): raise ValueError('Nonfinite receiver geometry')
    return Gather(a,float(dt),x,z,offset)

def vts_shape(path):
    with Path(path).open('rb') as f: header=f.read(65536).decode('ascii',errors='replace')
    m=re.search(r'(?:WholeExtent|Extent)="([^"]+)"',header)
    if not m: raise ValueError(f'{path}: VTS extent missing')
    x0,x1,y0,y1,z0,z1=map(int,m.group(1).split())
    if y0!=y1: raise ValueError('This plotting toolkit supports 2-D X-Z VTS only')
    return z1-z0+1,x1-x0+1

def _ascii_chunks(path,name):
    """Bounded text buffers, including arrays written as a single huge XML line."""
    target=re.compile(r'<DataArray\b[^>]*\bName=["\']'+re.escape(name)+r'["\'][^>]*>')
    with Path(path).open('r',encoding='ascii',errors='strict') as f:
        text='';found=False
        while not found:
            chunk=f.read(2**20)
            if not chunk: raise ValueError(f'{path}: DataArray {name!r} not found')
            text+=chunk;m=target.search(text)
            if m:
                if not re.search(r'format=["\']ascii["\']',m.group(0)): raise ValueError('Only ASCII VTS is supported')
                text=text[m.end():];found=True
            else:text=text[-8192:]
        while True:
            end=text.find('</DataArray>')
            if end>=0:
                if text[:end].strip():yield np.fromstring(text[:end],sep=' ')
                return
            # Split only at whitespace so scientific notation tokens remain intact.
            cut=max(text.rfind(' '),text.rfind('\n'),text.rfind('\t'),text.rfind('\r'))
            if cut>=0:
                part=text[:cut]
                if part.strip():yield np.fromstring(part,sep=' ')
                text=text[cut+1:]
            chunk=f.read(2**20)
            if not chunk:raise ValueError(f'{path}: unterminated DataArray')
            text+=chunk

def read_vts(path,name,stride_x=1,stride_z=1,cache_dir=None):
    """Return selected native nodes plus full-resolution min/max and extent.

    The cache depends on source absolute path, mtime_ns, size, field and stride.
    Every array is checked against its extent. No regular-grid geometry is invented.
    """
    path=Path(path).resolve();sx=int(stride_x);sz=int(stride_z)
    if sx<1 or sz<1: raise ValueError('VTS strides must be positive integers')
    stat=path.stat();key=hashlib.sha256(f'{path}|{stat.st_mtime_ns}|{stat.st_size}|{name}|{sx}|{sz}'.encode()).hexdigest()[:24]
    cache=Path(cache_dir)/(key+'.npz') if cache_dir else None
    if cache and cache.exists():
        with np.load(cache,allow_pickle=False) as c:return {k:c[k] for k in c.files}
    nz,nx=vts_shape(path);nc=3 if name=='Coordinates' else 1;pieces=[];count=0;lo=np.inf;hi=-np.inf
    for a in _ascii_chunks(path,name):
        if not np.isfinite(a).all():raise ValueError(f'{path}: nonfinite {name}')
        if a.size:lo=min(lo,float(a.min()));hi=max(hi,float(a.max()))
        point=np.arange(count,count+a.size)//nc
        keep=((point%nx)%sx==0)&((point//nx)%sz==0)
        pieces.append(a[keep]);count+=a.size
    if count!=nx*nz*nc:raise ValueError(f'{path}: {name} has {count} values, expected {nx*nz*nc}')
    shape=(len(range(0,nz,sz)),len(range(0,nx,sx)))+((nc,) if nc>1 else ())
    out=dict(values=np.concatenate(pieces).reshape(shape),full_min=np.array(lo),full_max=np.array(hi),nx=np.array(nx),nz=np.array(nz))
    if cache:
        cache.parent.mkdir(parents=True,exist_ok=True);np.savez_compressed(cache,**out)
    return out

def numeric_csv(path):
    return np.genfromtxt(path,delimiter=',',names=True,encoding='utf-8-sig')
