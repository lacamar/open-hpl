//! `cPhysics::LoadSurfaceData`: the `SurfaceData.xml` material list.

use super::host::Str;
use crate::string::{split, to_lower, token_to_float, token_to_int};
use crate::xml::{DOC, Doc, NodeId, c_str};

const COMB_AVERAGE: i32 = 0;
const COMB_MIN: i32 = 1;
const COMB_MULTIPLY: i32 = 2;
const COMB_MAX: i32 = 3;

#[repr(C)]
#[derive(Clone, Copy)]
pub struct ImpactDef {
    pub min_speed: f32,
    pub sound: Str,
    pub ps: Str,
    pub ps_prio: i32,
}

#[repr(C)]
pub struct SurfaceDef {
    pub name: Str,
    pub user_var: Str,
    pub step_type: Str,
    pub scrape_sound: Str,
    pub roll_sound: Str,
    pub elasticity: f32,
    pub kinetic_friction: f32,
    pub static_friction: f32,
    pub priority: i32,
    pub elasticity_mode: i32,
    pub friction_mode: i32,
    pub min_scrape_speed: f32,
    pub min_scrape_freq: f32,
    pub min_scrape_freq_speed: f32,
    pub middle_scrape_speed: f32,
    pub max_scrape_freq_speed: f32,
    pub max_scrape_freq: f32,
    pub min_scrape_contacts: i32,
    pub min_roll_speed: f32,
    pub min_roll_freq: f32,
    pub min_roll_volume: f32,
    pub min_roll_freq_speed: f32,
    pub middle_roll_speed: f32,
    pub max_roll_freq_speed: f32,
    pub max_roll_freq: f32,
    pub max_roll_volume: f32,
    pub roll_axis_flags: u32,
    pub impacts: *const ImpactDef,
    pub n_impacts: usize,
    pub hits: *const ImpactDef,
    pub n_hits: usize,
}

fn comb_mode(v: Option<&[u8]>) -> i32 {
    let Some(v) = v else {
        return COMB_AVERAGE;
    };
    match to_lower(c_str(v)).as_slice() {
        b"min" => COMB_MIN,
        b"max" => COMB_MAX,
        b"multiply" => COMB_MULTIPLY,
        _ => COMB_AVERAGE,
    }
}

struct Attrs<'a> {
    doc: &'a Doc,
    node: NodeId,
}

impl<'a> Attrs<'a> {
    fn raw(&self, name: &[u8]) -> Option<&'a [u8]> {
        self.doc.attribute(self.node, name).map(c_str)
    }

    fn string(&self, name: &[u8]) -> &'a [u8] {
        self.raw(name).unwrap_or_default()
    }

    fn float(&self, name: &[u8], default: f32) -> f32 {
        self.raw(name).map_or(default, token_to_float)
    }

    fn int(&self, name: &[u8], default: i32) -> i32 {
        self.raw(name).map_or(default, token_to_int)
    }
}

fn impact_list(doc: &Doc, material: NodeId, tag: &[u8]) -> Vec<ImpactDef> {
    let mut out = Vec::new();
    let mut cur = doc.first_child_element(material, Some(tag));
    while let Some(node) = cur {
        let a = Attrs { doc, node };
        out.push(ImpactDef {
            min_speed: a.float(b"MinSpeed", 1.0),
            sound: Str::new(a.string(b"SoundName")),
            ps: Str::new(a.string(b"PSName")),
            ps_prio: a.int(b"PSPrio", 10),
        });
        cur = doc.next_sibling_element(node, Some(tag));
    }
    out
}

fn axis_flags(v: &[u8]) -> u32 {
    let mut flags = 0;
    for token in split(v, None, true) {
        match to_lower(token).as_slice() {
            b"x" => flags |= 1,
            b"y" => flags |= 2,
            b"z" => flags |= 4,
            _ => {}
        }
    }
    flags
}

/// False when the file does not load. An empty root (a document of only comments) yields no
/// materials where C++ dereferences a null root.
pub fn parse(data: &[u8], read_ok: bool, mut sink: impl FnMut(&SurfaceDef)) -> bool {
    let mut doc = Doc::new();
    if !doc.load(data, read_ok) {
        return false;
    }
    let Some(root) = doc.first_child_element(DOC, None) else {
        return true;
    };
    let mut cur = doc.first_child_element(root, Some(b"Material"));
    while let Some(node) = cur {
        let a = Attrs { doc: &doc, node };
        let name = a.string(b"Name");
        if !name.is_empty() {
            let impacts = impact_list(&doc, node, b"Impact");
            let hits = impact_list(&doc, node, b"Hit");
            sink(&SurfaceDef {
                name: Str::new(name),
                user_var: Str::new(a.string(b"UserVar")),
                step_type: Str::new(a.string(b"StepType")),
                scrape_sound: Str::new(a.string(b"ScrapeSoundName")),
                roll_sound: Str::new(a.string(b"RollSoundName")),
                elasticity: a.float(b"Elasticity", 0.5),
                kinetic_friction: a.float(b"KineticFriction", 0.3),
                static_friction: a.float(b"StaticFriction", 0.3),
                priority: a.int(b"Priority", 0),
                elasticity_mode: comb_mode(a.raw(b"ElasticityMode")),
                friction_mode: comb_mode(a.raw(b"FrictionMode")),
                min_scrape_speed: a.float(b"MinScrapeSpeed", 0.7),
                min_scrape_freq: a.float(b"MinScrapeFreq", 0.7),
                min_scrape_freq_speed: a.float(b"MinScrapeFreqSpeed", 0.7),
                middle_scrape_speed: a.float(b"MiddleScrapeSpeed", 1.2),
                max_scrape_freq_speed: a.float(b"MaxScrapeFreqSpeed", 3.0),
                max_scrape_freq: a.float(b"MaxScrapeFreq", 2.0),
                min_scrape_contacts: a.int(b"MinScrapeContacts", 4),
                min_roll_speed: a.float(b"MinRollSpeed", 0.7),
                min_roll_freq: a.float(b"MinRollFreq", 0.7),
                min_roll_volume: a.float(b"MinRollVolume", 0.7),
                min_roll_freq_speed: a.float(b"MinRollFreqSpeed", 0.7),
                middle_roll_speed: a.float(b"MiddleRollSpeed", 1.2),
                max_roll_freq_speed: a.float(b"MaxRollFreqSpeed", 3.0),
                max_roll_freq: a.float(b"MaxRollFreq", 2.0),
                max_roll_volume: a.float(b"MaxRollVolume", 2.0),
                roll_axis_flags: axis_flags(a.string(b"RollAxis")),
                impacts: impacts.as_ptr(),
                n_impacts: impacts.len(),
                hits: hits.as_ptr(),
                n_hits: hits.len(),
            });
        }
        cur = doc.next_sibling_element(node, Some(b"Material"));
    }
    true
}
