use super::s3::{IdHost, Ids, OhplS3Id, OhplS3LiquidBodyIn, liquid_body_plan};

const KINDS: [&str; 8] = [
    "cMeshEntity",
    "cLightPoint",
    "cSoundEntity",
    "iPhysicsBody",
    "iPhysicsJointHinge",
    "iPhysicsJoint",
    "cParticleSystem",
    "iCharacterBody",
];
const QUERY: [&str; 13] = [
    "cMeshEntity",
    "cLightPoint",
    "cSoundEntity",
    "iPhysicsBody",
    "iPhysicsJointHinge",
    "iPhysicsJoint",
    "cParticleSystem",
    "iCharacterBody",
    "iEntity3D",
    "iLight",
    "cLightBox",
    "iPhysicsJointSlider",
    "iPhysicsJointBall",
];
const NO: usize = 8;

#[derive(Default)]
struct Mock {
    kind: [usize; NO],
    dead: [bool; NO],
    joint: [usize; NO],
}

fn idx(o: usize) -> usize {
    (o - 0x1000) / 0x10
}

fn addr(i: usize) -> usize {
    0x1000 + 0x10 * i
}

impl IdHost for Mock {
    fn alive(&mut self, obj: usize, ty: &[u8], _creation: i32) -> bool {
        if ty == b"cSoundEntity" || ty == b"cParticleSystem" {
            return !self.dead[idx(obj)];
        }
        true
    }
    fn creation_id(&mut self, obj: usize) -> i32 {
        idx(obj) as i32 + 100
    }
    fn cast_entity(&mut self, obj: usize, ty: &[u8]) -> usize {
        let k = KINDS[self.kind[idx(obj)]].as_bytes();
        let ok = ty == k
            || (!(k == b"iCharacterBody" || k.starts_with(b"iPhysicsJoint"))
                && (ty == b"iEntity3D" || (ty == b"iLight" && k == b"cLightPoint")));
        if ok { obj } else { 0 }
    }
    fn cast_joint(&mut self, obj: usize, ty: &[u8]) -> usize {
        let want = [
            "iPhysicsJointHinge",
            "iPhysicsJointSlider",
            "iPhysicsJointBall",
        ];
        if want[self.joint[idx(obj)]].as_bytes() == ty {
            obj
        } else {
            0
        }
    }
}

fn nums(s: &str) -> Vec<i64> {
    s.split_whitespace()
        .filter_map(|t| t.parse().ok())
        .collect()
}

#[test]
fn id_registry_matches_cpp_trace() {
    let data = include_str!("../../tests/golden/S3/s3.txt");
    let mut m = Mock::default();
    let mut ids = Ids::new(0);
    let (mut checked, mut seeds) = (0, 0);
    for line in data.lines() {
        let (head, rest) = line.split_once(' ').unwrap_or((line, ""));
        let (args, res) = rest.split_once(" = ").unwrap_or((rest, ""));
        let a = nums(args);
        let r = nums(res);
        match head {
            "S" => {
                seeds += 1;
                ids = Ids::new(a[1] as i32);
                for i in 0..NO {
                    m.kind[i] = a[2 + 2 * i] as usize;
                    m.joint[i] = a[3 + 2 * i] as usize;
                    m.dead[i] = false;
                }
            }
            "ID" => {
                let obj = if a[0] < 0 { 0 } else { addr(a[0] as usize) };
                let ty = QUERY[a[1] as usize].as_bytes();
                let id = ids.object_id(&mut m, obj, ty);
                let want = OhplS3Id {
                    a: r[0] as u8,
                    b: r[1] as i32,
                    c: r[2] as i32,
                };
                assert_eq!(id, want, "{line}");
                checked += 1;
            }
            "FROM" => {
                let id = OhplS3Id {
                    a: a[0] as u8,
                    b: a[1] as i32,
                    c: a[2] as i32,
                };
                let got = ids.object_from_id(&mut m, id, QUERY[a[3] as usize].as_bytes());
                let got = if got == 0 { -1 } else { idx(got) as i64 };
                assert_eq!(got, r[0], "{line}");
                checked += 1;
            }
            "REG" => {
                let v: Vec<usize> = a[1..].iter().map(|&i| addr(i as usize)).collect();
                ids.register_bodies(&v);
            }
            "CLR" => ids.clear(),
            "RETYPE" => {
                m.kind[a[0] as usize] = a[1] as usize;
                m.joint[a[0] as usize] = a[2] as usize;
            }
            "KILL" => m.dead[a[0] as usize] = true,
            "REV" => m.dead[a[0] as usize] = false,
            _ => {}
        }
    }
    assert!(seeds == 12 && checked > 200, "{seeds} {checked}");
}

#[test]
fn liquid_body_plan_matches_cpp_table() {
    let data = include_str!("../../tests/golden/S3/s3.txt");
    let mut n = 0;
    for line in data.lines().filter(|l| l.starts_with("LQ ")) {
        let (args, res) = line[3..].split_once(" = ").unwrap();
        let mut t = args.split_whitespace();
        let m: u32 = t.next().unwrap().parse().unwrap();
        let max_y = f32::from_bits(u32::from_str_radix(t.next().unwrap(), 16).unwrap());
        let bit = |b: u32| m & (1 << b) != 0;
        let p = liquid_body_plan(&OhplS3LiquidBodyIn {
            collide: bit(0),
            active: bit(1),
            has_mass: bit(2),
            is_character: bit(3),
            inside: bit(4),
            no_gravity_underwater: bit(5),
            buoyancy_active: bit(6),
            owner_interacted: bit(7),
            waves_near: bit(8),
            surface_moved: bit(9),
            body_max_y: max_y,
            center_y: 1.0,
            ext_y: 1.0,
        });
        let r: Vec<&str> = res.split_whitespace().collect();
        let got = format!(
            "{} {} {} {} {} {} {} {} {:08x}",
            u8::from(p.skip),
            u8::from(p.exit_callback),
            u8::from(p.buoyancy_off),
            u8::from(p.init_buoyancy),
            u8::from(p.wave_surface),
            u8::from(p.reset_surface),
            p.underwater,
            p.gravity,
            p.max_speed.to_bits()
        );
        assert_eq!(got, r.join(" "), "{line}");
        n += 1;
    }
    assert_eq!(n, 3072);
}
