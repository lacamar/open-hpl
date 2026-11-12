//! Replays the H6 golden scripts (`rust/tests/golden/H6/{skel,anim,mesh}_*.txt`, recorded from the
//! C++ cSkeleton/cAnimationTrack/cMesh by `harness/h6_script.cpp`) against the Rust state.

use super::anim::KeyFrame;
use super::animdata::{AnimInfo, MeshInfo, Track};
use super::skel::Skeleton;
use crate::types::{Matrixf, Quaternion, Vector3f};
use std::fs;
use std::path::PathBuf;

fn f(s: &str) -> f32 {
    f32::from_bits(u32::from_str_radix(s, 16).unwrap())
}

fn hx(v: f32) -> String {
    format!("{:08x}", v.to_bits())
}

fn mat(s: &str) -> Matrixf {
    let mut m = Matrixf::default();
    for (i, t) in s.split(',').enumerate() {
        m.m[i / 4][i % 4] = f(t);
    }
    m
}

fn mtx(m: Option<&Matrixf>) -> String {
    let m = m.unwrap();
    m.m.iter()
        .flatten()
        .map(|&v| hx(v))
        .collect::<Vec<_>>()
        .join(",")
}

fn kf_str(k: &KeyFrame) -> String {
    format!(
        "{} {},{},{} {},{},{},{}",
        hx(k.time),
        hx(k.trans.x),
        hx(k.trans.y),
        hx(k.trans.z),
        hx(k.rotation.v.x),
        hx(k.rotation.v.y),
        hx(k.rotation.v.z),
        hx(k.rotation.w)
    )
}

fn kf_parse(time: &str, tr: &str, rot: &str) -> KeyFrame {
    let t: Vec<f32> = tr.split(',').map(f).collect();
    let r: Vec<f32> = rot.split(',').map(f).collect();
    KeyFrame {
        time: f(time),
        trans: Vector3f {
            x: t[0],
            y: t[1],
            z: t[2],
        },
        rotation: Quaternion {
            v: Vector3f {
                x: r[0],
                y: r[1],
                z: r[2],
            },
            w: r[3],
        },
    }
}

fn sid(s: &str) -> &[u8] {
    if s == "-" { b"" } else { s.as_bytes() }
}

fn skel_op(sk: &mut Skeleton, w: &[&str]) -> String {
    let id = |s: &str| s.parse::<usize>().unwrap();
    let bone = |b: Option<usize>| b.map_or(-1, |b| b as i32);
    match w[0] {
        "child" => format!(
            "id {}",
            sk.create_child(id(w[1]), w[2].as_bytes(), sid(w[3]))
                .unwrap()
        ),
        "tf" => {
            sk.set_transform(id(w[1]), &mat(w[2]));
            ".".to_string()
        }
        "utf" => {
            sk.set_unscaled(id(w[1]), &mat(w[2]));
            ".".to_string()
        }
        "ibind" => {
            sk.set_inv_bind(id(w[1]), &mat(w[2]));
            ".".to_string()
        }
        "local" => mtx(sk.local(id(w[1]))),
        "world" => mtx(sk.world(id(w[1]))),
        "inv" => mtx(sk.inv_world(id(w[1]))),
        "ibindq" => mtx(sk.inv_bind(id(w[1]))),
        "uworld" => mtx(sk.world_unscaled(id(w[1]))),
        "idx" => {
            let i = sk.index_by_name(w[1].as_bytes());
            format!("{i} {}", bone(sk.by_index(i)))
        }
        "sidx" => {
            let i = sk.index_by_sid(sid(w[1]));
            format!("{i} {}", bone(sk.by_index(i)))
        }
        "byidx" => {
            let i: i32 = w[1].parse().unwrap();
            format!("{} of {}", bone(sk.by_index(i)), sk.num())
        }
        "num" => sk.num().to_string(),
        "detach" => {
            sk.detach(id(w[1]));
            format!("parent {}", bone(sk.parent(id(w[1]))))
        }
        o => panic!("skel op {o}"),
    }
}

fn index_of(t: &Track, p: *mut KeyFrame) -> i32 {
    if p.is_null() {
        return -1;
    }
    (0..t.len())
        .find(|&i| t.frame(i) == p)
        .map_or(-2, |i| i as i32)
}

struct AnimModel {
    length: f32,
    info: AnimInfo,
    tracks: Vec<Track>,
}

fn anim_op(m: &mut AnimModel, w: &[&str]) -> String {
    let t = |s: &str| s.parse::<usize>().unwrap();
    match w[0] {
        "length" => {
            m.length = f(w[1]);
            ".".to_string()
        }
        "mk" => {
            m.info.add_track(w[1].as_bytes());
            m.tracks.push(Track::new());
            (m.tracks.len() - 1).to_string()
        }
        "create" => {
            let tr = &mut m.tracks[t(w[1])];
            let p = tr.create(f(w[2]));
            // SAFETY: `p` is the live frame just created.
            format!("{} {}", index_of(tr, p), kf_str(unsafe { &*p }))
        }
        "set" => {
            let tr = &m.tracks[t(w[1])];
            let p = tr.frame(w[2].parse().unwrap());
            let k = kf_parse(w[3], w[4], w[5]);
            // SAFETY: live frame of the track.
            unsafe {
                (*p).trans = k.trans;
                (*p).rotation = k.rotation;
            }
            ".".to_string()
        }
        "interp" => kf_str(&m.tracks[t(w[1])].interpolate(m.length, f(w[2]))),
        "at" => {
            let tr = &m.tracks[t(w[1])];
            let (a, b, wt) = tr.frames_at_time(m.length, f(w[2]));
            if tr.is_empty() {
                format!("{} -1 -1", hx(wt))
            } else {
                format!("{} {a} {b}", hx(wt))
            }
        }
        "clear" => {
            m.tracks[t(w[1])].clear();
            ".".to_string()
        }
        "cnt" => m.tracks[t(w[1])].len().to_string(),
        "byname" => format!(
            "{} of {}",
            m.info.track_by_name(w[1].as_bytes()),
            m.tracks.len()
        ),
        o => panic!("anim op {o}"),
    }
}

fn mesh_op(m: &mut MeshInfo, subs: &mut i32, anims: &mut i32, w: &[&str]) -> String {
    match w[0] {
        "sub" => {
            m.add_sub(w[1].as_bytes());
            *subs += 1;
            (*subs - 1).to_string()
        }
        "subq" => format!(
            "{} {} {subs}",
            m.sub_index(w[1].as_bytes()),
            m.sub_by_name(w[1].as_bytes())
        ),
        "anim" => {
            m.add_animation(w[1].as_bytes());
            *anims += 1;
            anims.to_string()
        }
        "animq" => {
            let i = m.animation_index(w[1].as_bytes());
            format!("{i} {i}")
        }
        "clr" => {
            m.clear_animations();
            *anims = 0;
            "0".into()
        }
        o => panic!("mesh op {o}"),
    }
}

#[test]
fn h6_golden() {
    let dir = PathBuf::from(env!("CARGO_MANIFEST_DIR")).join("tests/golden/H6");
    let mut files: Vec<_> = fs::read_dir(&dir)
        .unwrap()
        .map(|e| e.unwrap().path())
        .filter(|p| p.extension().is_some_and(|e| e == "txt"))
        .filter(|p| {
            let n = p.file_name().unwrap().to_string_lossy().into_owned();
            ["skel_", "anim_", "mesh_"].iter().any(|k| n.starts_with(k))
        })
        .collect();
    files.sort();
    assert!(files.len() >= 30, "golden files missing");
    let mut ops = 0;
    for p in files {
        let name = p.file_name().unwrap().to_string_lossy().into_owned();
        let text = fs::read_to_string(&p).unwrap();
        let mut sk = Skeleton::new();
        let mut am = AnimModel {
            length: 0.0,
            info: AnimInfo::default(),
            tracks: Vec::new(),
        };
        let mut mi = MeshInfo::default();
        let (mut subs, mut anims) = (0, 0);
        for (n, line) in text.lines().enumerate() {
            let (op, want) = line.split_once('\t').unwrap();
            let w: Vec<&str> = op.split(' ').collect();
            let got = if name.starts_with("skel_") {
                skel_op(&mut sk, &w)
            } else if name.starts_with("anim_") {
                anim_op(&mut am, &w)
            } else {
                mesh_op(&mut mi, &mut subs, &mut anims, &w)
            };
            assert_eq!(got, want, "{name}:{} {op}", n + 1);
            ops += 1;
        }
    }
    assert!(ops > 3000);
}
