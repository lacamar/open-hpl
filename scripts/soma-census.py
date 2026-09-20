#!/usr/bin/env python3
"""Offline census of SOMA's .hpm sidecar tracks -> soma/conformance/expected.json.

Independent of the engine loader on purpose: it is the oracle the sweep
compares load_report against.
"""
import argparse
import json
import os
import sys
import xml.etree.ElementTree as ET
from collections import Counter

SECTION_TRACKS = [
    "Area", "Billboard", "Compound", "Decal", "Entity", "ExposureArea", "FogArea", "LensFlare",
    "Light", "LightMask", "ParticleSystem", "Primitive", "Sound", "StaticComboArea", "StaticObject",
]


def find_maps(root):
    maps = {}
    for dirpath, dirnames, filenames in os.walk(os.path.join(root, "maps"), followlinks=False):
        dirnames[:] = [d for d in dirnames if not os.path.islink(os.path.join(dirpath, d))]
        for f in filenames:
            if f.endswith(".hpm"):
                maps.setdefault(f, os.path.join(dirpath, f))
    return dict(sorted(maps.items()))


def parse(path):
    try:
        return ET.parse(path).getroot()
    except (ET.ParseError, OSError):
        return None


def census_map(path):
    tracks = {}
    for track in SECTION_TRACKS:
        root = parse(path + "_" + track)
        if root is None:
            tracks[track] = {"file_missing": True, "xml": 0, "tags": {}}
            continue
        tags = Counter()
        for section in root.findall("Section"):
            objects = section.find("Objects")
            if objects is None:
                continue
            for child in objects:
                # 24 decals in the depot are authored with an empty DecalMesh: nothing to create.
                if track == "Decal":
                    mesh = child.find("DecalMesh")
                    if mesh is None or int(mesh.get("NumVerts", "0")) <= 0 or int(mesh.get("NumInds", "0")) <= 0:
                        continue
                tags[child.tag] += 1
        tracks[track] = {"file_missing": False, "xml": sum(tags.values()), "tags": dict(tags)}

    root = parse(path + "_StaticObjectBatches")
    batches = root.find("StaticObjectBatches") if root is not None else None
    tracks["StaticObjectBatches"] = {"file_missing": root is None, "xml": len(batches) if batches is not None else 0, "tags": {}}

    root = parse(path + "_DetailMeshes")
    count = 0
    if root is not None:
        for mesh in root.iter("DetailMesh"):
            count += int(mesh.get("NumOfInstances", "0"))
    tracks["DetailMeshes"] = {"file_missing": root is None, "xml": count, "tags": {}}

    root = parse(path + "_Terrain")
    terrain = root.find("Terrain") if root is not None else None
    terrain_active = terrain is not None and terrain.get("Active", "false").lower() == "true"

    return {"path": path, "terrain_active": terrain_active, "tracks": tracks}


def main():
    repo = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
    ap = argparse.ArgumentParser(description=__doc__)
    ap.add_argument("--soma-root", default=os.environ.get("OPENHPL_SOMA_ROOT", os.path.expanduser("~/.local/share/Steam/steamapps/common/SOMA")))
    ap.add_argument("--out", default=os.path.join(repo, "soma/conformance/expected.json"))
    args = ap.parse_args()

    maps = find_maps(args.soma_root)
    if not maps:
        sys.exit(f"no .hpm maps under {args.soma_root}/maps")

    out = {"maps": {}}
    for name, path in maps.items():
        entry = census_map(path)
        entry["path"] = os.path.relpath(path, args.soma_root)
        out["maps"][name] = entry
        total = sum(t["xml"] for t in entry["tracks"].values())
        print(f"{name:45s} objects={total:6d} terrain={'Y' if entry['terrain_active'] else '-'}")

    with open(args.out, "w") as f:
        json.dump(out, f, indent=1, sort_keys=True)
        f.write("\n")
    print(f"wrote {args.out} ({len(maps)} maps)")


if __name__ == "__main__":
    main()
