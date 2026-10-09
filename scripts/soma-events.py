#!/usr/bin/env python3
"""List FMOD event paths from SOMA's .fdp projects: soma-events.py [SUBSTRING ...]"""
import glob, os, sys, xml.etree.ElementTree as ET

root = os.environ.get("OPENHPL_SOMA_ROOT", os.path.expanduser("~/.local/share/Steam/steamapps/common/SOMA"))
events = set()

def walk(group, path):
    for e in group.findall("event") + group.findall("simpleevent/event"):
        events.add(path + "/" + e.findtext("name"))
    for g in group.findall("eventgroup"):
        walk(g, path + "/" + g.findtext("name"))

for f in glob.glob(root + "/sounds/**/*.fdp", recursive=True):
    t = ET.parse(f).getroot()
    walk(t, t.findtext("name"))
pats = [a.lower() for a in sys.argv[1:]]
for e in sorted(events):
    if not pats or any(p in e.lower() for p in pats):
        print(e)
