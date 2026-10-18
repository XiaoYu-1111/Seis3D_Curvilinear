"""Independent struct-based SEG-Y fixtures and standard-field checks; outputs outside Git."""
from pathlib import Path
import math
import struct
import subprocess
import sys
ROOT=Path(__file__).resolve().parents[1]
OUT=ROOT.parent/'out'
EXE=OUT/'bin'/('Debug' if '--debug' in sys.argv else 'Release')/'seis_record_io_validation.exe'
subprocess.run([EXE],check=True)
folder=OUT/'segy-validation'
for precision,code in [('ieee32',5),('ieee64',6)]:
    for component,factor in [('Vz',1),('Vx',-2)]:
        raw=(folder/f'{precision}_{component}.sgy').read_bytes()
        binary=raw[3200:3600]
        assert binary[300:302]==bytes([2,0])
        assert struct.unpack_from('>H',binary,24)[0]==code
        assert struct.unpack_from('>I',binary,96)[0]==0x01020304
        assert struct.unpack_from('>d',binary,72)[0]*1e-6==.000423370123456789
        width,pattern=(4,'>4f') if code==5 else (8,'>4d')
        assert len(raw)==3600+2*(240+4*width)
        for r in range(2):
            offset=3600+r*(240+4*width)
            assert struct.unpack_from('>h',raw,offset+28)[0]==(12 if component=='Vz' else 14)
            assert struct.unpack_from('>h',raw,offset+70)[0]==-1000
            assert struct.unpack_from('>i',raw,offset+80)[0]==-1234+r*2000
            assert struct.unpack_from('>4'+('f' if code==5 else 'd'),raw,offset+240)==tuple(v*factor for v in [.125,-.5,1.25,0])

for profile,code,encoding in [(2,1,'cp037'),(3,5,'ascii')]:
    for gain in [1,1000000]:
        raw=(folder/f'legacy-{profile}-{gain}.sgy').read_bytes()
        binary=raw[3200:3600]
        assert binary[300:302]==bytes([1,0]) and not any(binary[60:100]) and not any(binary[312:400])
        assert struct.unpack_from('>H',binary,24)[0]==code
        assert struct.unpack_from('>H',binary,16)[0]==500
        textual=raw[:3200].decode(encoding)
        assert 'COMPONENT=Vz' in textual and f'EXPORT_MULTIPLIER={gain};' in textual
        assert ('UNITS=m/s' if gain==1 else 'UNITS=scaled(m/s)') in textual
        assert len(raw)==3600+2*(240+4*4)
        for r in range(2):
            offset=3600+r*(240+4*4)
            assert struct.unpack_from('>H',raw,offset+28)[0]==1
            assert struct.unpack_from('>i',raw,offset+12)[0]==r+1
            assert struct.unpack_from('>i',raw,offset+20)[0]==r+1
            assert struct.unpack_from('>H',raw,offset+116)[0]==500
            assert struct.unpack_from('>h',raw,offset+202)[0]==(6 if gain==1 else 0)
            if code==5:
                values=struct.unpack_from('>4f',raw,offset+240)
            else:
                words=struct.unpack_from('>4I',raw,offset+240)
                if gain==1: assert words==(0x40200000,0xc0800000,0x41140000,0)
                values=tuple((-1 if w&0x80000000 else 1)*math.ldexp(float(w&0xffffff),4*(((w>>24)&127)-64)-24) for w in words)
            assert values==tuple(v*gain for v in [.125,-.5,1.25,0])

def fixture(name,fmt,pattern,values,order='>',extra=0,ns=3,revision=1,weight=0):
    text=b'Independent standard-field fixture'.ljust(3200,b' ')
    bh=bytearray(400);struct.pack_into(order+'H',bh,16,1000);struct.pack_into(order+'H',bh,20,ns);struct.pack_into(order+'H',bh,24,fmt);bh[300]=revision
    struct.pack_into(order+'h',bh,304,extra)
    th=bytearray(240);struct.pack_into(order+'H',th,28,12);struct.pack_into(order+'h',th,70,-10);struct.pack_into(order+'i',th,80,15);struct.pack_into(order+'H',th,88,1);struct.pack_into(order+'h',th,108,100);struct.pack_into(order+'H',th,114,ns);struct.pack_into(order+'H',th,116,1000)
    struct.pack_into(order+'h',th,168,weight)
    path=folder/name;path.write_bytes(text+bh+b' '*3200*extra+th+struct.pack(order+pattern,*values))
    result=subprocess.run([EXE,path],capture_output=True,text=True)
    assert result.returncode==0,(name,result.stdout,result.stderr)
    assert f'traces=1 dt=0.001 first={math.ldexp(1.,-weight):g}' in result.stdout,(name,result.stdout)

fixture('ibm.sgy',1,'3I',[0x41100000,0xC1200000,0])
fixture('int32.sgy',2,'3i',[1,-2,0])
fixture('int16.sgy',3,'3h',[1,-2,0])
fixture('int8.sgy',8,'3b',[1,-2,0])
fixture('little.sgy',5,'3f',[1,-2,0],order='<')
fixture('extended_text.sgy',5,'3f',[1,-2,0],extra=1)
fixture('weighted.sgy',3,'3h',[1,-2,0],weight=1)
bad=folder/'truncated.sgy';bad.write_bytes((folder/'little.sgy').read_bytes()[:-2])
assert subprocess.run([EXE,bad],capture_output=True).returncode!=0
print('PASS independent SEG-Y fields; legacy rev1 IBM/IEEE/EBCDIC, live IDs, CDP/channel, scaled units; IBM, integers, little endian, extended text, truncated input fixtures')
