import sys,unittest,struct,json
from pathlib import Path
sys.dont_write_bytecode=True
sys.path.insert(0,str(Path(__file__).resolve().parents[1]))
import numpy as np
from seisplot.io import read_segy,read_vts
from seisplot.core import WORKSPACE,gain,setup,grid_data
from seisplot.four_case import layered_reflection
from seisplot.surface_wave import SurfaceWaveStudy

class ReaderTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.out=WORKSPACE/'out/validation/seismic_plotting/tests';cls.out.mkdir(parents=True,exist_ok=True)

    def segy(self,name,fmt=5,dt=1250,trace_dt=None,extended=0):
        p=self.out/name;values=np.arange(5*121,dtype=float).reshape(5,121)/17-4
        bh=bytearray(400);struct.pack_into('>HH',bh,16,dt,dt);struct.pack_into('>H',bh,20,121);struct.pack_into('>H',bh,24,fmt);struct.pack_into('>h',bh,304,extended)
        with p.open('wb') as f:
            f.write(b' '*3200);f.write(bh);f.write(b' '*3200*extended)
            for i,row in enumerate(values):
                h=bytearray(240);struct.pack_into('>HH',h,114,121,dt if trace_dt is None else trace_dt);struct.pack_into('>h',h,70,-10);struct.pack_into('>i',h,80,1000+35*i);struct.pack_into('>H',h,88,1)
                f.write(h);f.write(row.astype('>f4' if fmt==5 else '>f8').tobytes())
        return p,values

    def test_dimensions_dt_and_coordinate_scalar(self):
        p,a=self.segy('arbitrary.sgy');g=read_segy(p,source_x_m=80)
        self.assertEqual(g.values.shape,(5,121));self.assertAlmostEqual(g.dt,.00125)
        np.testing.assert_allclose(g.x,100+3.5*np.arange(5));np.testing.assert_allclose(g.offset,g.x-80)
        np.testing.assert_allclose(g.values,a,rtol=1e-6,atol=1e-6)

    def test_float64_with_extended_text_header(self):
        p,a=self.segy('float64.sgy',fmt=6,extended=1);np.testing.assert_array_equal(read_segy(p).values,a)

    def test_conflicting_dt_and_truncation_rejected(self):
        p,_=self.segy('conflict.sgy',trace_dt=2000)
        with self.assertRaisesRegex(ValueError,'conflicting'):read_segy(p)
        p,_=self.segy('truncated.sgy');p.write_bytes(p.read_bytes()[:-3])
        with self.assertRaisesRegex(ValueError,'truncated'):read_segy(p)

    def test_ibm_samples(self):
        p,_=self.segy('ibm.sgy');b=bytearray(p.read_bytes());struct.pack_into('>H',b,3224,1)
        vals=[0x41100000,0xC1200000,0x40800000,0x00000000]
        for i in range(5):
            start=3600+i*(240+121*4)+240
            for j in range(121):struct.pack_into('>I',b,start+4*j,vals[j%4])
        p.write_bytes(b);np.testing.assert_array_equal(read_segy(p).values[0,:4],[1.,-2.,.5,0.])

    def test_long_line_vts_and_geometry_selection(self):
        nx=70001;nz=2;x,z=np.meshgrid(np.arange(nx)*2.,[0.,3.]);coords=np.stack([x,np.zeros_like(x),z+np.sin(x/100)],axis=-1)
        v=np.arange(nx*nz,dtype=float).reshape(nz,nx)-100
        p=self.out/'long_line.vts'
        p.write_text(f'<VTKFile><StructuredGrid WholeExtent="0 {nx-1} 0 0 0 1"><Piece><PointData><DataArray Name="Vz" format="ascii">'+
          ' '.join(map(str,v.ravel()))+'</DataArray></PointData><Points><DataArray Name="Coordinates" NumberOfComponents="3" format="ascii">'+
          ' '.join(map(str,coords.ravel()))+'</DataArray></Points></Piece></StructuredGrid></VTKFile>',encoding='ascii')
        a=read_vts(p,'Vz',137,1,self.out/'cache');b=read_vts(p,'Coordinates',137,1,self.out/'cache')
        np.testing.assert_array_equal(a['values'],v[:,::137]);np.testing.assert_array_equal(b['values'],coords[:,::137])
        self.assertEqual(float(a['full_max']),v.max())
        # Same-size rewrite with changed timestamp must invalidate the display cache.
        text=p.read_text().replace('-100.0 -99.0','-101.0 -99.0',1);p.write_text(text)
        self.assertEqual(read_vts(p,'Vz',137,1,self.out/'cache')['values'][0,0],-101)

    def test_zero_agc_is_finite_and_preserves_input(self):
        a=np.zeros((3,511));before=a.copy();r=gain(a,.002,'agc',.3,.03,'reflect',True)
        self.assertTrue(np.isfinite(r).all());np.testing.assert_array_equal(a,before)

    def test_output_guard(self):
        with self.assertRaisesRegex(ValueError,'output_dir'):setup({'output_dir':str(Path(__file__).parent)})

    def test_reflection_kinematics_and_zero_offset(self):
        offsets=np.array([0.,100.,-100.]);t=layered_reflection([0,2000],[3000],30,1000,offsets,.1)
        np.testing.assert_allclose(t,np.hypot(offsets,1970)/3000+.1,atol=1e-10)
        with self.assertRaises(ValueError):layered_reflection([0,100],[3000],30,1000,[0],.1)

    def test_independent_known_phase_speed(self):
        folder=self.out/'phase_input';folder.mkdir(exist_ok=True)
        dt=.001;t=np.arange(1001)*dt;offset=20+2*np.arange(61);speed=347.
        u=t[None,:]-.12-offset[:,None]/speed
        a=(1-2*(np.pi*15*u)**2)*np.exp(-(np.pi*15*u)**2)
        bh=bytearray(400);struct.pack_into('>H',bh,16,1000);struct.pack_into('>H',bh,20,1001);struct.pack_into('>H',bh,24,5)
        with (folder/'Vz.sgy').open('wb') as f:
            f.write(b' '*3200);f.write(bh)
            for off,row in zip(offset,a):
                h=bytearray(240);struct.pack_into('>HH',h,114,1001,1000);struct.pack_into('>i',h,80,int(100+off));struct.pack_into('>H',h,88,1)
                f.write(h);f.write(row.astype('>f4').tobytes())
        cfg=dict(output_dir=str(self.out/'phase_output'),datasets=[dict(root=str(folder),cases=[dict(id='phase',record_files={'Vz':'Vz.sgy'},metadata={'source_x_m':100})])],
                 dispersion=dict(component='Vz',frequency_hz=[12,24,2],velocity_m_s=[200,600,2],offset_windows_m=[[20,140]]))
        study=SurfaceWaveStudy(cfg);picks=study.compute_dispersion()
        error=max(abs(r['phase_velocity_m_s']/speed-1) for r in picks)
        self.assertLess(error,.0015)

if __name__=='__main__':unittest.main()
