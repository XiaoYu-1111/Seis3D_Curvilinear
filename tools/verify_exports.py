"""Independent standard-library validation of this demo's ASCII VTS and receiver CSV."""
import csv
import math
import sys
import xml.etree.ElementTree as ET
from pathlib import Path


def check_vts(path):
    root = ET.parse(path).getroot()
    assert root.get("type") == "StructuredGrid", "Wrong VTK dataset type"
    piece = root.find("./StructuredGrid/Piece")
    extent = [int(x) for x in piece.get("Extent").split()]
    dimensions = [extent[2 * i + 1] - extent[2 * i] + 1 for i in range(3)]
    count = math.prod(dimensions)
    arrays = piece.findall("./PointData/DataArray")
    required = {"Vp", "Vs", "Density", "Vz", "Velocity", "Sxx", "Syy", "Szz", "Sxy", "Sxz", "Syz"}
    assert required.issubset({a.get("Name") for a in arrays}), "Missing physical fields"
    for node in arrays + piece.findall("./Points/DataArray"):
        values = [float(x) for x in node.text.split()]
        components = int(node.get("NumberOfComponents", "1"))
        assert len(values) == count * components, f"Wrong tuple count: {node.get('Name', 'Points')}"
        assert all(math.isfinite(x) for x in values), "Non-finite output value"
    metadata = {n.get("Name"): float(n.text) for n in root.findall("./StructuredGrid/FieldData/DataArray")}
    assert abs(metadata["TIME_VALUE"] - metadata["SimulationDt"] * metadata["SimulationStep"]) < 1e-8
    coordinates = [float(x) for x in piece.find("./Points/DataArray").text.split()]
    points_per_layer = dimensions[0] * dimensions[1]
    for layer in range(dimensions[2]):
        if layer * metadata["GridSpacing"] >= metadata["TerrainTransitionDepth"]:
            z = coordinates[layer * points_per_layer * 3 + 2:(layer + 1) * points_per_layer * 3:3]
            assert max(z) - min(z) < 1e-6, "Deep model layer is not horizontal"
    bottom = coordinates[(dimensions[2] - 1) * points_per_layer * 3 + 2::3]
    assert max(bottom) - min(bottom) < 1e-6, "Model bottom is not horizontal"
    return dimensions, len(arrays), metadata


def check_csv(path, metadata):
    with path.open(encoding="utf-8") as stream:
        rows = list(csv.reader(line for line in stream if not line.startswith("#")))
    columns = len(rows[0])
    data = [[float(x) for x in row] for row in rows[1:]]
    assert data and data[0][0] == 0.0, "Missing time-zero sample"
    assert all(len(row) == columns and all(math.isfinite(x) for x in row) for row in data)
    assert len(data) == int(metadata["SimulationStep"]) + 1, "Wrong receiver sample count"
    for index, row in enumerate(data):
        assert abs(row[0] - index * metadata["SimulationDt"]) < 1e-8, "Nonuniform receiver interval"
    assert abs(data[-1][0] - metadata["TIME_VALUE"]) < 1e-8, "CSV and snapshot times differ"
    return columns - 1, len(data)


if __name__ == "__main__":
    output = Path(sys.argv[1]) if len(sys.argv) > 1 else Path(__file__).resolve().parents[2] / "out"
    dimensions, arrays, metadata = check_vts(output / "curvilinear_snapshot.vts")
    traces, samples = check_csv(output / "receiver_traces.csv", metadata)
    print(f"PASS: VTS {dimensions}, {arrays} point arrays; CSV {traces} traces x {samples} samples; t={metadata['TIME_VALUE']:.9f} s")
