#!/usr/bin/env python3
"""Offline census of a game's maps -> <game>/conformance/expected.json (.hpm sidecar tracks; HPL2 .map: names only).

Independent of the engine loader on purpose: it is the oracle the sweep
compares load_report against.
"""
import argparse
import json
import os
import sys
import xml.etree.ElementTree as ET
from collections import Counter

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from ohpl_game import game  # noqa: E402

SECTION_TRACKS = [
    "Area", "Billboard", "Compound", "Decal", "Entity", "ExposureArea", "FogArea", "LensFlare",
    "Light", "LightMask", "ParticleSystem", "Primitive", "Sound", "StaticComboArea", "StaticObject",
]


def find_maps(root, ext):
    maps = {}
    for dirpath, dirnames, filenames in os.walk(os.path.join(root, "maps"), followlinks=False):
        dirnames[:] = [d for d in dirnames if not os.path.islink(os.path.join(dirpath, d))]
        for f in filenames:
            if f.endswith(ext):
                maps.setdefault(f, os.path.join(dirpath, f))
    return dict(sorted(maps.items()))


def parse(path):
    try:
        return ET.parse(path).getroot()
    except (ET.ParseError, OSError):
        return None


def census_map(path, files):
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
            index = {f.get("Id"): f.get("Path") for fi in section if fi.tag.startswith("FileIndex") for f in fi}
            for child in objects:
                # A few objects reference files the depot doesn't ship.
                ref = child.get("File") or index.get(child.get("FileIndex"))
                if ref and os.path.basename(ref.replace("\\", "/")).lower() not in files:
                    continue
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
    ap = argparse.ArgumentParser(description=__doc__)
    ap.add_argument("--game", help="soma, rebirth, bunker, amnesia, amfp (default $OHPL_GAME or soma)")
    ap.add_argument("--root")
    ap.add_argument("--out")
    args = ap.parse_args()
    g = game(args.game)
    args.soma_root = args.root or g.root
    args.out = args.out or os.path.join(g.conf, "expected.json")
    os.makedirs(os.path.dirname(args.out), exist_ok=True)

    maps = find_maps(args.soma_root, g.map_ext)
    if not maps:
        sys.exit(f"no {g.map_ext} maps under {args.soma_root}/maps")

    files = {f.lower() for _, _, fs in os.walk(args.soma_root) for f in fs}
    out = {"maps": {}}
    for name, path in maps.items():
        entry = census_map(path, files) if g.hpl3 else {"terrain_active": False, "tracks": {}}
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
