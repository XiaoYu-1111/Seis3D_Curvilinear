"""Read a DEM and extract documented, metre-based surface profiles.

The input is always read-only. Generated raster caches and exports belong in out/.
The fixed-record fallback intentionally supports only unrotated metric USGS-style
profiles; it does not invent a geographic datum or an EPSG identifier.
"""
from __future__ import annotations

from datetime import datetime, timezone
import hashlib
import json
import math
from pathlib import Path

import numpy as np
import pandas as pd
import rasterio
from rasterio import Affine
from rasterio.enums import Resampling
from rasterio.errors import RasterioIOError
from rasterio.windows import Window


def project_output() -> Path:
    for parent in (Path.cwd(), *Path.cwd().parents):
        if (parent / 'CMakePresets.json').is_file() and (parent / 'src').is_dir():
            return parent.parent / 'out/dem_profiles'
        if (parent / 'Seis3D_Curvilinear/CMakePresets.json').is_file():
            return parent / 'out/dem_profiles'
    return Path.cwd() / 'out/dem_profiles'


def _number(record: bytes, start: int, width: int, integer=False):
    text = record[start:start + width].decode('ascii').strip().replace('D', 'E')
    if not text:
        raise ValueError(f'Empty fixed-width DEM field at byte {start}.')
    value = int(text) if integer else float(text)
    if not math.isfinite(value):
        raise ValueError('Non-finite DEM header value.')
    return value


def read_metric_usgs_records(path: Path):
    """Validated 1024-byte A/B record reader for the supplied metric DEM variant.

    Raster values are height * z_resolution + local_profile_datum. B profile
    samples run south to north; output raster rows run north to south.
    """
    with path.open('rb') as stream:
        header = stream.read(1024)
        if len(header) != 1024:
            raise ValueError('Not a 1024-byte USGS-style DEM header.')
        pattern = _number(header, 150, 6, True)
        system = _number(header, 156, 6, True)
        h_unit = _number(header, 528, 6, True)
        v_unit = _number(header, 534, 6, True)
        if pattern != 1 or system != 9 or h_unit != 2 or v_unit not in (1, 2):
            raise ValueError('Fallback requires regular projection-code-9 profiles with horizontal metres.')
        if abs(_number(header, 786, 24)) > 1e-12:
            raise ValueError('Rotated fixed-record DEM requires conversion by its exporting GIS.')
        corners = np.array([_number(header, 546 + i * 24, 24) for i in range(8)]).reshape(4, 2)
        xmin, ymin = corners.min(axis=0)
        xmax, ymax = corners.max(axis=0)
        dx, dy, dz = (_number(header, 816 + i * 12, 12) for i in range(3))
        width = _number(header, 858, 6, True)
        n_profile_rows = _number(header, 852, 6, True)
        if n_profile_rows != 1 or min(dx, dy, dz) <= 0 or not 1 <= width <= 100_000:
            raise ValueError('Invalid DEM dimensions or sample spacing.')
        height = int(round((ymax - ymin) / dy)) + 1
        if height < 1 or width * height > 25_000_000:
            raise ValueError('Fixed-record DEM exceeds this notebook reader\'s 25-million-pixel limit.')
        tolerance = max(dx, dy) * 2e-5
        if abs(xmax - xmin - (width - 1) * dx) > tolerance:
            raise ValueError('Header X extent is inconsistent with profile count and spacing.')
        if abs(ymax - ymin - (height - 1) * dy) > tolerance:
            raise ValueError('Header Y extent is inconsistent with sample spacing.')
        values = np.full((height, width), np.nan, dtype=np.float64)
        for column in range(width):
            record = stream.read(1024)
            if len(record) != 1024:
                raise ValueError(f'Truncated B record at column {column + 1}.')
            row_id, col_id, count, ncols = (_number(record, 6 * i, 6, True) for i in range(4))
            if row_id != 1 or col_id != column + 1 or ncols != 1 or not 1 <= count <= height:
                raise ValueError(f'Unexpected profile numbering/dimensions at column {column + 1}.')
            x0, y0, local_datum, zlo, zhi = (_number(record, 24 + 24 * i, 24) for i in range(5))
            if abs(x0 - (xmin + column * dx)) > tolerance:
                raise ValueError(f'Profile X alignment failed at column {column + 1}.')
            chunks = [record[144:144 + min(count, 146) * 6]]
            remaining = max(0, count - 146)
            while remaining:
                continuation = stream.read(1024)
                if len(continuation) != 1024:
                    raise ValueError('Truncated DEM continuation record.')
                take = min(remaining, 170)
                chunks.append(continuation[:take * 6])
                remaining -= take
            try:
                raw = np.frombuffer(b''.join(chunks), dtype='S6').astype(np.int32)
            except ValueError as exc:
                raise ValueError('Invalid fixed-width elevation sample.') from exc
            valid = raw != -32767
            elevations = raw.astype(np.float64) * dz + local_datum
            northings = y0 + np.arange(count) * dy
            positions = (ymax - northings) / dy
            indices = np.rint(positions).astype(np.int64)
            if np.any(np.abs(positions - indices) * dy > tolerance) or np.any((indices < 0) | (indices >= height)):
                raise ValueError('Profile Y alignment or coverage failed.')
            if valid.any() and (elevations[valid].min() < zlo - 2 * dz or elevations[valid].max() > zhi + 2 * dz):
                raise ValueError('Decoded profile heights exceed the B-record declared range.')
            values[indices[valid], column] = elevations[valid]
        global_min, global_max = _number(header, 738, 24), _number(header, 762, 24)
        finite = values[np.isfinite(values)]
        if not finite.size or finite.min() < global_min - 2 * dz or finite.max() > global_max + 2 * dz:
            raise ValueError('Decoded heights do not match the A-record range.')
        metadata = dict(reader='metric_USGS_fixed_records', projection_code=system,
                        projection_parameters=[_number(header, 168 + 24 * i, 24) for i in range(15)],
                        horizontal_unit='m', vertical_unit='m' if v_unit == 2 else 'ft',
                        original_z_resolution=dz, original_height_range=[global_min, global_max],
                        coordinate_datum='not_assigned', header_sha256=hashlib.sha256(header).hexdigest(),
                        pixel_center_extent=[xmin, ymin, xmax, ymax])
        transform = Affine(dx, 0, xmin - dx / 2, 0, -dy, ymax + dy / 2)
    return values, transform, metadata


def unit_to_metres(unit):
    name = str(unit or '').strip().lower().replace(' ', '_')
    if name in ('m', 'metre', 'meter', 'metres', 'meters'):
        return 1.0
    if name in ('ft', 'foot', 'feet', 'international_foot'):
        return .3048
    if name in ('us_survey_foot', 'us_survey_feet', 'ftus', 'ft_us'):
        return 1200 / 3937
    raise ValueError(f'Unknown unit {unit!r}; set an explicit unit override after checking the source.')


class DEM:
    def __init__(self, path, output_dir=None, crs_override=None, xy_unit_override=None, z_unit_override=None):
        self.path = Path(path).expanduser().resolve()
        self.output_dir = Path(output_dir or project_output()).resolve()
        self.extra = {}
        self.fallback = False
        self.source = None
        if not self.path.is_file():
            raise FileNotFoundError(self.path)
        try:
            self.source = rasterio.open(self.path, 'r')
        except RasterioIOError as original_error:
            try:
                data, transform, self.extra = read_metric_usgs_records(self.path)
            except Exception as exc:
                raise ValueError(f'GDAL could not open the raster; fixed-record fallback also failed: {exc}') from original_error
            self.fallback = True
            cache = self.output_dir / 'cache'
            cache.mkdir(parents=True, exist_ok=True)
            # Timestamp/size distinguish changed inputs, not only their shared A header.
            identity = f'fixed-record-v2-float64|{self.path}|{self.path.stat().st_size}|{self.path.stat().st_mtime_ns}'
            key = hashlib.sha256(identity.encode()).hexdigest()[:12]
            cache_path = cache / f'{self.path.stem}-{key}.tif'
            if not cache_path.exists():
                with rasterio.open(cache_path, 'w', driver='GTiff', height=data.shape[0], width=data.shape[1],
                                   count=1, dtype='float64', transform=transform, nodata=np.nan,
                                   compress='deflate') as dst:
                    dst.write(data, 1)
                    dst.set_band_unit(1, self.extra['vertical_unit'])
                    dst.update_tags(SOURCE_FORMAT='USGS-style projection code 9; geographic datum unresolved')
            self.source = rasterio.open(cache_path, 'r')
            if self.source.shape != data.shape or self.source.transform != transform or self.source.dtypes[0] != 'float64':
                self.source.close()
                raise ValueError('Existing DEM cache is inconsistent; use a new output directory.')
        try:
            from pyproj import CRS
            self.crs = CRS.from_user_input(crs_override or self.source.crs) if (crs_override or self.source.crs) else None
            if self.crs is not None and self.crs.is_compound:
                self.crs = next(c for c in self.crs.sub_crs_list if c.is_projected or c.is_geographic)
            if xy_unit_override:
                self.xy_factor = unit_to_metres(xy_unit_override)
            elif self.extra.get('horizontal_unit') == 'm':
                self.xy_factor = 1.0
            elif self.crs is not None and self.crs.is_projected:
                factors = [a.unit_conversion_factor for a in self.crs.axis_info[:2]]
                if len(factors) != 2 or not math.isclose(*factors, rel_tol=1e-10):
                    raise ValueError('Projected axes need compatible length units.')
                self.xy_factor = factors[0]
            else:
                self.xy_factor = None
            self.z_unit = z_unit_override or self.source.units[0]
            self.z_factor = unit_to_metres(self.z_unit)
            self.scale = self.source.scales[0]
            self.offset = self.source.offsets[0]
            if not np.isfinite([self.scale, self.offset]).all() or self.scale == 0:
                raise ValueError('Invalid raster height scale/offset.')
        except Exception:
            self.close()
            raise

    def close(self):
        if self.source is not None:
            self.source.close()

    @property
    def bounds(self):
        return self.source.bounds

    def info(self):
        s = self.source
        return dict(input=str(self.path), driver=s.driver, fallback_reader=self.fallback,
                    shape=[s.height, s.width], resolution=list(s.res), bounds=list(s.bounds),
                    transform=list(s.transform), crs=self.crs.to_string() if self.crs else None,
                    horizontal_metres_per_unit=self.xy_factor, height_unit=self.z_unit,
                    height_to_metres=self.z_factor, raster_scale=self.scale, raster_offset=self.offset,
                    nodata=s.nodata if s.nodata is None or np.isfinite(s.nodata) else 'NaN',
                    vertical_datum_conversion='none', original_header=self.extra)

    def preview(self, max_size=850):
        s = self.source
        ratio = min(1.0, max_size / max(s.width, s.height))
        h, w = max(1, int(s.height * ratio)), max(1, int(s.width * ratio))
        z = s.read(1, out_shape=(h, w), masked=True, resampling=Resampling.nearest).astype('float64').filled(np.nan)
        z = (z * self.scale + self.offset) * self.z_factor
        t = s.transform * Affine.scale(s.width / w, s.height / h)
        col, row = np.meshgrid(np.arange(w + 1), np.arange(h + 1))
        x = t.a * col + t.b * row + t.c
        y = t.d * col + t.e * row + t.f
        return x, y, z

    def sample(self, x, y, method='bilinear'):
        x, y = np.asarray(x, dtype=float), np.asarray(y, dtype=float)
        if x.ndim != 1 or x.shape != y.shape or not np.isfinite([x, y]).all():
            raise ValueError('Sampling coordinates must be finite vectors of equal length.')
        s, inv = self.source, ~self.source.transform
        c, r = inv.a*x + inv.b*y + inv.c, inv.d*x + inv.e*y + inv.f
        inside = (c >= 0) & (r >= 0) & (c < s.width) & (r < s.height)
        result = np.full(x.size, np.nan)
        if not inside.any():
            return result
        if method == 'nearest':
            cc, rr = np.floor(c[inside]).astype(int), np.floor(r[inside]).astype(int)
            columns, rows = [cc], [rr]
            weights = np.ones((1, cc.size))
        elif method == 'bilinear':
            u, v = np.clip(c[inside]-.5, 0, s.width-1), np.clip(r[inside]-.5, 0, s.height-1)
            c0, r0 = np.floor(u).astype(int), np.floor(v).astype(int)
            c1, r1 = np.minimum(c0+1, s.width-1), np.minimum(r0+1, s.height-1)
            a, b = u-c0, v-r0
            columns, rows = [c0, c1, c0, c1], [r0, r0, r1, r1]
            weights = np.array([(1-a)*(1-b), a*(1-b), (1-a)*b, a*b])
        else:
            raise ValueError('Choose nearest or bilinear interpolation.')
        cmin, cmax = min(k.min() for k in columns), max(k.max() for k in columns)
        rmin, rmax = min(k.min() for k in rows), max(k.max() for k in rows)
        if (cmax-cmin+1)*(rmax-rmin+1) > 25_000_000:
            raise ValueError('Selected sampling window exceeds 25 million pixels; shorten the profile.')
        data = s.read(1, window=Window(int(cmin), int(rmin), int(cmax-cmin+1), int(rmax-rmin+1)),
                      masked=True).astype('float64').filled(np.nan)
        samples = np.array([data[rr-rmin, cc-cmin] for cc, rr in zip(columns, rows)])
        bad = ((~np.isfinite(samples)) & (weights > 1e-12)).any(axis=0)
        z = np.sum(np.where(np.isfinite(samples), samples, 0)*weights, axis=0)
        z = (z*self.scale + self.offset)*self.z_factor
        z[bad] = np.nan
        result[inside] = z
        return result


def line_in_roi(roi, direction='horizontal', position=.5, endpoints=None):
    xmin, xmax, ymin, ymax = map(float, roi)
    if not np.isfinite(roi).all() or not xmin < xmax or not ymin < ymax or not 0 <= position <= 1:
        raise ValueError('ROI needs xmin<xmax, ymin<ymax and position in [0,1].')
    if direction == 'horizontal':
        y = ymin + position*(ymax-ymin)
        return (xmin, y), (xmax, y)
    if direction == 'vertical':
        x = xmin + position*(xmax-xmin)
        return (x, ymin), (x, ymax)
    if direction == 'custom' and endpoints is not None:
        points = np.asarray(endpoints, dtype=float)
        if points.shape != (2, 2) or not np.isfinite(points).all():
            raise ValueError('Custom endpoints need shape (2,2).')
        if np.any((points[:,0] < xmin) | (points[:,0] > xmax) | (points[:,1] < ymin) | (points[:,1] > ymax)):
            raise ValueError('Both custom endpoints must lie inside the ROI.')
        return tuple(points[0]), tuple(points[1])
    raise ValueError('Choose horizontal, vertical or custom direction.')


def extract_profile(dem, p0, p1, spacing_m=5.0, start_m=0.0, end_m=None,
                    method='bilinear', distance_mode='grid'):
    points = np.asarray([p0, p1], dtype=float)
    if points.shape != (2,2) or not np.isfinite(points).all() or np.all(points[0] == points[1]):
        raise ValueError('Need two different finite endpoints in DEM coordinates.')
    if not np.isfinite([spacing_m, start_m]).all() or spacing_m < 1e-4 or start_m < 0:
        raise ValueError('Spacing must be at least 0.0001 m; start distance must be nonnegative.')
    t = np.linspace(0, 1, 4097)
    x, y = points[0,:,None] + (points[1]-points[0])[:,None]*t
    lon = lat = None
    if distance_mode == 'grid':
        if dem.xy_factor is None:
            raise ValueError('Grid distance needs declared projected length units; do not treat degrees as metres.')
        distance = np.r_[0, np.cumsum(np.hypot(np.diff(x), np.diff(y))*dem.xy_factor)]
    elif distance_mode == 'geodesic':
        if dem.crs is None:
            raise ValueError('Geodesic distance requires a confirmed CRS, which this DEM does not declare reliably.')
        from pyproj import Transformer
        lon, lat = Transformer.from_crs(dem.crs, dem.crs.geodetic_crs, always_xy=True).transform(x, y)
        _, _, lengths = dem.crs.get_geod().inv(lon[:-1], lat[:-1], lon[1:], lat[1:])
        distance = np.r_[0, np.cumsum(lengths)]
    else:
        raise ValueError('Choose grid or geodesic distance.')
    if not np.isfinite(distance).all() or distance[-1] <= 0:
        raise ValueError('Invalid profile length; check coordinate units and CRS.')
    stop = float(distance[-1] if end_m is None else end_m)
    if not np.isfinite(stop) or not start_m < stop or stop > distance[-1] + 1e-6:
        raise ValueError(f'Distance range must satisfy 0 <= start < end <= {distance[-1]:.6f} m.')
    stop = min(stop, float(distance[-1]))
    if (stop-start_m)/spacing_m > 1_000_000:
        raise ValueError('More than one million samples requested; increase sampling spacing.')
    sample_distances = np.arange(start_m, stop, spacing_m)
    if stop-sample_distances[-1] < 1e-7:
        sample_distances[-1] = stop
    else:
        sample_distances = np.r_[sample_distances, stop]
    sx, sy = np.interp(sample_distances, distance, x), np.interp(sample_distances, distance, y)
    z = dem.sample(sx, sy, method)
    table = pd.DataFrame(dict(x_m=sample_distances-start_m, elevation_m=z, dem_x=sx, dem_y=sy,
                              source_distance_m=sample_distances, valid=np.isfinite(z)))
    if lon is not None:
        table['longitude_deg'] = np.interp(sample_distances, distance, lon)
        table['latitude_deg'] = np.interp(sample_distances, distance, lat)
    metadata = dict(dem=dem.info(), endpoints_dem=points.tolist(), full_line_length_m=float(distance[-1]),
                    selected_distance_range_m=[float(start_m),stop], distance_mode=distance_mode,
                    distance_definition='horizontal projected grid length' if distance_mode == 'grid' else 'ellipsoidal horizontal length along the selected line',
                    requested_spacing_m=float(spacing_m), last_interval_m=float(np.diff(table.x_m)[-1]),
                    interpolation=method, height_processing='source scale/offset and unit-to-metre conversion only',
                    smoothing='none', datum_shift_m=0, invalid_samples=int((~table.valid).sum()))
    return table, metadata


def longest_valid_segment(table, metadata):
    good = table.valid.to_numpy(dtype=bool)
    starts = np.flatnonzero(np.diff(np.r_[False, good, False].astype(int)) == 1)
    ends = np.flatnonzero(np.diff(np.r_[False, good, False].astype(int)) == -1)
    if not starts.size:
        raise ValueError('No valid terrain samples along the selected line.')
    k = int(np.argmax(ends-starts))
    result = table.iloc[starts[k]:ends[k]].copy().reset_index(drop=True)
    if len(result) < 2:
        raise ValueError('Fewer than two consecutive valid samples.')
    result['x_m'] -= result.x_m.iloc[0]
    updated = dict(metadata, invalid_samples=0, longest_valid_segment_applied=True,
                   kept_source_distance_range_m=[float(result.source_distance_m.iloc[0]),float(result.source_distance_m.iloc[-1])],
                   discarded_samples=len(table)-len(result))
    return result, updated


def plot_map(dem, ax, table=None, roi=None):
    from matplotlib.patches import Rectangle
    x,y,z = dem.preview()
    image = ax.pcolormesh(x,y,z,shading='flat',cmap='terrain',rasterized=True)
    if roi is not None:
        xmin,xmax,ymin,ymax = roi
        ax.add_patch(Rectangle((xmin,ymin),xmax-xmin,ymax-ymin,fill=False,edgecolor='#db6d28',lw=1.5))
    if table is not None:
        ax.plot(table.dem_x,table.dem_y,color='red',lw=1.6,label='Selected profile')
        ax.scatter([table.dem_x.iloc[0]],[table.dem_y.iloc[0]],color='black',s=24,zorder=3)
    ax.set(xlabel='DEM X (source coordinate units)',ylabel='DEM Y (source coordinate units)')
    ax.ticklabel_format(style='plain',useOffset=False)
    return image


def plot_profile(table, ax):
    ax.plot(table.x_m,table.elevation_m,color='#254f66',lw=1.5)
    ax.set(xlabel='Distance from selected start (m)',ylabel='Actual elevation (m)')
    ax.grid(alpha=.2)


def export_profile(dem, table, metadata, output_dir=None, roi=None):
    if len(table) < 2 or not np.isfinite(table[['x_m','elevation_m','dem_x','dem_y']]).all().all():
        raise ValueError('Export refused: selected profile includes NoData/out-of-bounds samples. Shorten the range or explicitly keep the longest valid segment.')
    if not (np.diff(table.x_m) > 0).all():
        raise ValueError('Export distances must increase strictly.')
    folder = Path(output_dir or dem.output_dir)/datetime.now().strftime('profile_%Y%m%d_%H%M%S_%f')
    folder.mkdir(parents=True,exist_ok=False)
    table[['x_m','elevation_m']].to_csv(folder/'surface_profile.csv',index=False,float_format='%.9f')
    table.to_csv(folder/'profile_with_coordinates.csv',index=False,float_format='%.9f')
    metadata = dict(metadata, exported_utc=datetime.now(timezone.utc).isoformat(),samples=len(table),
                    output_length_m=float(table.x_m.iloc[-1]), elevation_range_m=[float(table.elevation_m.min()),float(table.elevation_m.max())],
                    roi_dem_coordinates=list(roi) if roi is not None else None)
    (folder/'selection.json').write_text(json.dumps(metadata,ensure_ascii=False,indent=2,allow_nan=False),encoding='utf-8')
    import matplotlib.pyplot as plt
    figure,ax = plt.subplots(figsize=(10,3.8),layout='constrained')
    plot_profile(table,ax)
    figure.savefig(folder/'profile.png',dpi=160)
    plt.close(figure)
    figure,ax = plt.subplots(figsize=(9,6),layout='constrained')
    image = plot_map(dem,ax,table,roi)
    figure.colorbar(image,ax=ax,label='Elevation (m)')
    figure.savefig(folder/'dem_with_profile.png',dpi=150)
    plt.close(figure)
    return folder
