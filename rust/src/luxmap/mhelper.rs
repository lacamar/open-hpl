//! `cLuxMapHelper`: ray queries, shape damage and light level. Engine objects are reached through [`mhost`].

use super::helper::{self, DamageFilter, SubMesh};
use super::mhost::{self, A, S, body, op, slice_of};
use crate::math::{fma, point_in_aabb, rand_rectf};
use crate::types::Vector3f;

pub const RAY_LOS: i32 = 0;
pub const RAY_ENTITY: i32 = 1;
pub const RAY_CHAR: i32 = 2;
pub const RAY_ATTACK: i32 = 3;

const CALC_DIST: i32 = 1;
const CALC_NORMAL: i32 = 2;
const PREFILTER: i32 = 8;

pub const NO_HIT: f32 = 9999999.0;

/// State of one ray callback, on the Rust stack for the length of the cast.
#[repr(C)]
pub struct Ray {
    pub kind: i32,
    pub check_shadow: bool,
    pub intersected: bool,
    pub skip: usize,
    pub dist: f32,
    pub body: usize,
    pub normal: Vector3f,
}

impl Ray {
    fn new(kind: i32) -> Ray {
        Ray {
            kind,
            check_shadow: false,
            intersected: false,
            skip: 0,
            dist: NO_HIT,
            body: 0,
            normal: Vector3f::default(),
        }
    }
}

struct Cur {
    phys: usize,
    world: usize,
}

fn cur_map() -> Option<Cur> {
    let mut a = A::new();
    mhost::call(op::CUR_MAP, &mut a);
    (a.r != 0).then_some(Cur {
        phys: a.p[0],
        world: a.p[1],
    })
}

struct Body {
    flags: i32,
    mass: f32,
    user: usize,
    char_user: usize,
}

impl Body {
    fn has(&self, f: i32) -> bool {
        self.flags & f != 0
    }
}

fn body_info(b: usize) -> Body {
    let mut a = A::new();
    a.p[0] = b;
    mhost::call(op::BODY_INFO, &mut a);
    Body {
        flags: a.i[0],
        mass: a.f[0],
        user: a.r,
        char_user: a.p[1],
    }
}

fn ent_info(e: usize) -> (bool, bool) {
    let mut a = A::new();
    a.p[0] = e;
    mhost::call(op::ENT_INFO, &mut a);
    (a.i[0] != 0, a.i[1] != 0)
}

fn cast(c: &Cur, ray: &mut Ray, start: Vector3f, end: Vector3f, flags: i32) {
    let mut a = A::new();
    a.p[0] = c.phys;
    a.p[1] = std::ptr::from_mut(ray) as usize;
    a.i[0] = ray.kind;
    a.i[1] = flags;
    a.f[..6].copy_from_slice(&[start.x, start.y, start.z, end.x, end.y, end.z]);
    mhost::call(op::RAY_CAST, &mut a);
}

/// `BeforeIntersect` of the four ray callbacks.
///
/// # Safety
/// `ray` is the live [`Ray`] of the cast in progress.
pub unsafe fn ray_before(ray: *mut Ray, b: usize) -> bool {
    // SAFETY: caller contract; the borrow ends before any host op.
    let (kind, check_shadow, skip) = unsafe { ((*ray).kind, (*ray).check_shadow, (*ray).skip) };
    let info = body_info(b);
    match kind {
        RAY_LOS => {
            let is_char = info.has(body::IS_CHAR);
            let (collide, blocks) = (info.has(body::COLLIDE), info.has(body::BLOCKS_LIGHT));
            if !helper::los_before(is_char, collide, check_shadow, blocks, false, &[]) {
                return false;
            }
            if info.user == 0 {
                return true;
            }
            let mut a = A::new();
            a.p[0] = b;
            mhost::call(op::BODY_SUBMESHES, &mut a);
            if a.i[1] == 0 {
                return true;
            }
            // SAFETY: the host returns `i[0]` entries valid until the next op.
            let subs: &[SubMesh] = slice_of(a.r as *const SubMesh, a.i[0] as usize);
            helper::los_before(is_char, collide, check_shadow, blocks, true, subs)
        }
        RAY_ENTITY => {
            let (active, _) = if info.user != 0 {
                ent_info(info.user)
            } else {
                (false, false)
            };
            let action = helper::closest_entity_before(
                info.user != 0,
                active,
                info.has(body::COLLIDE),
                info.has(body::IS_CHAR),
            );
            if action == helper::CLOSEST_ASK {
                let mut a = A::new();
                a.p[0] = info.user;
                a.p[1] = b;
                mhost::call(op::ENT_CAN_INTERACT, &mut a);
                return a.i[0] != 0;
            }
            action == helper::CLOSEST_ACCEPT
        }
        RAY_CHAR => {
            helper::char_collider_before(info.has(body::COLLIDE_CHAR), info.has(body::IS_CHAR))
        }
        _ => helper::attack_ray_before(info.has(body::IS_CHAR), info.has(body::COLLIDE), skip == b),
    }
}

/// `OnIntersect` of the four ray callbacks: the first two sorts keep the nearest hit and go on.
///
/// # Safety
/// `ray` is the live [`Ray`] of the cast in progress.
pub unsafe fn ray_hit(ray: *mut Ray, b: usize, dist: f32, normal: Vector3f) -> bool {
    // SAFETY: caller contract; no host op runs while this borrow lives.
    let r = unsafe { &mut *ray };
    match r.kind {
        RAY_ENTITY | RAY_CHAR => {
            if helper::closer(dist, r.dist) {
                r.dist = dist;
                r.body = b;
                if r.kind == RAY_CHAR {
                    r.normal = normal;
                }
            }
            true
        }
        _ => {
            r.intersected = true;
            false
        }
    }
}

pub fn check_line_of_sight(start: Vector3f, end: Vector3f, check_shadows: bool) -> bool {
    let Some(c) = cur_map() else {
        return false;
    };
    let mut ray = Ray::new(RAY_LOS);
    ray.check_shadow = check_shadows;
    cast(&c, &mut ray, start, end, PREFILTER);
    !ray.intersected
}

fn ray_end(start: Vector3f, dir: Vector3f, len: f32) -> Vector3f {
    Vector3f {
        x: fma(dir.x, len, start.x),
        y: fma(dir.y, len, start.y),
        z: fma(dir.z, len, start.z),
    }
}

pub struct Closest {
    pub dist: f32,
    pub body: usize,
    pub entity: usize,
    pub normal: Vector3f,
}

pub fn closest_entity(start: Vector3f, dir: Vector3f, len: f32) -> Option<Closest> {
    let c = cur_map()?;
    let mut ray = Ray::new(RAY_ENTITY);
    cast(
        &c,
        &mut ray,
        start,
        ray_end(start, dir, len),
        CALC_DIST | PREFILTER,
    );
    let entity = if ray.body != 0 {
        body_info(ray.body).user
    } else {
        0
    };
    Some(Closest {
        dist: ray.dist,
        body: ray.body,
        entity,
        normal: Vector3f::default(),
    })
}

pub fn closest_char_collider(start: Vector3f, dir: Vector3f, len: f32) -> Option<Closest> {
    let c = cur_map()?;
    let mut ray = Ray::new(RAY_CHAR);
    cast(
        &c,
        &mut ray,
        start,
        ray_end(start, dir, len),
        CALC_DIST | CALC_NORMAL | PREFILTER,
    );
    Some(Closest {
        dist: ray.dist,
        body: ray.body,
        entity: 0,
        normal: ray.normal,
    })
}

#[repr(C)]
#[derive(Clone, Copy)]
pub struct DamageArgs {
    pub shape: usize,
    pub transform: [f32; 16],
    pub origin: Vector3f,
    pub min_damage: f32,
    pub max_damage: f32,
    pub force: f32,
    pub max_impulse: f32,
    pub strength: i32,
    pub hit_speed: f32,
    pub damage_type: i32,
    pub weapon_hit_type: i32,
    pub check_enemies: bool,
    pub check_player: bool,
    pub check_props: bool,
    pub lethal_for_player: bool,
}

fn fx_at(world: usize, name: &[u8], ps: bool, pos: Vector3f) {
    let mut a = A::new();
    a.p[0] = world;
    a.s[0] = S::of(name);
    a.i[0] = i32::from(ps);
    a.f[..3].copy_from_slice(&[pos.x, pos.y, pos.z]);
    mhost::call(op::FX_AT, &mut a);
}

/// `cLuxMapHelper::ShapeDamage`; the second result is "hit the player".
pub fn shape_damage(d: &DamageArgs) -> (bool, bool) {
    let mut hit_player = false;
    let Some(c) = cur_map() else {
        return (false, false);
    };
    let mut a = A::new();
    a.p[0] = c.phys;
    a.p[1] = d.shape;
    a.m = d.transform;
    mhost::call(op::SHAPE_BODIES, &mut a);
    let bodies: Vec<usize> = slice_of(a.r as *const usize, a.i[0] as usize).to_vec();
    if bodies.is_empty() {
        return (false, false);
    }
    let mut hit = false;
    for &b in &bodies {
        let info = body_info(b);
        let is_char = info.has(body::IS_CHAR);
        let filter = DamageFilter {
            active: info.has(body::ACTIVE),
            collide: info.has(body::COLLIDE),
            has_user_data: info.user != 0,
            check_props: d.check_props,
            is_char,
            is_player_body: is_char && info.has(body::PLAYER_BODY),
            check_player: d.check_player,
            check_enemies: d.check_enemies,
        };
        if helper::damage_skips(filter) {
            continue;
        }
        let mut q = A::new();
        q.p[0] = c.phys;
        q.p[1] = d.shape;
        q.p[2] = b;
        q.m = d.transform;
        mhost::call(op::SHAPE_COLLIDE, &mut q);
        if q.i[0] == 0 || q.i[1] == 0 {
            continue;
        }
        let points: Vec<Vector3f> = slice_of(q.r as *const f32, q.i[2] as usize * 3)
            .chunks_exact(3)
            .filter_map(|p| match *p {
                [x, y, z] => Some(Vector3f { x, y, z }),
                _ => None,
            })
            .collect();

        let mut ray = Ray::new(RAY_ATTACK);
        ray.skip = b;
        let target = body_pos(b);
        cast(&c, &mut ray, d.origin, target, PREFILTER);
        if ray.intersected {
            continue;
        }

        hit = true;
        let entity = if is_char { info.char_user } else { info.user };
        let hit_pos = helper::hit_position(&points);
        if helper::takes_push(info.mass, is_char) {
            let dir = helper::hit_direction(hit_pos, d.origin);
            let mut p = A::new();
            p.p[0] = b;
            if is_char {
                let v = helper::char_push(dir, d.force);
                p.f[..3].copy_from_slice(&[v.x, v.y, v.z]);
                mhost::call(op::BODY_PUSH, &mut p);
            } else {
                let v = helper::prop_impulse(dir, d.force, info.mass, d.max_impulse);
                p.f[..6].copy_from_slice(&[v.x, v.y, v.z, hit_pos.x, hit_pos.y, hit_pos.z]);
                mhost::call(op::BODY_IMPULSE, &mut p);
            }
        }

        let is_enemy = entity != 0 && ent_info(entity).1;
        if is_enemy {
            let mut e = A::new();
            e.p[0] = entity;
            e.i[0] = d.weapon_hit_type;
            mhost::call(op::ENEMY_HIT, &mut e);
            let (snd, ps) = (e.ret_bytes(0), e.ret_bytes(1));
            if !snd.is_empty() {
                fx_at(c.world, &snd, false, hit_pos);
            }
            if !ps.is_empty() {
                fx_at(c.world, &ps, true, hit_pos);
            }
        } else {
            let mut e = A::new();
            e.p[0] = b;
            e.f[0] = d.hit_speed;
            mhost::call(op::BODY_HIT, &mut e);
            if e.i[0] != 0 {
                let (snd, ps) = (e.ret_bytes(0), e.ret_bytes(1));
                if !snd.is_empty() {
                    fx_at(c.world, &snd, false, hit_pos);
                }
                if !ps.is_empty() {
                    fx_at(c.world, &ps, true, hit_pos);
                }
            }
        }

        let damage = rand_rectf(d.min_damage, d.max_damage);
        match helper::damage_target(entity != 0, info.has(body::PLAYER_BODY)) {
            helper::TARGET_ENTITY => {
                let mut g = A::new();
                g.p[0] = entity;
                g.f[0] = damage;
                g.i[0] = d.strength;
                mhost::call(op::ENT_DAMAGE, &mut g);
            }
            helper::TARGET_PLAYER => {
                let mut g = A::new();
                g.f[0] = damage;
                g.i[0] = d.strength;
                g.i[1] = d.damage_type;
                g.i[2] = i32::from(d.lethal_for_player);
                mhost::call(op::PLAYER_DAMAGE, &mut g);
                hit_player = true;
            }
            _ => {}
        }
    }
    (hit, hit_player)
}

/// World position of a body, for the attack ray target.
fn body_pos(b: usize) -> Vector3f {
    let mut a = A::new();
    a.p[0] = b;
    mhost::call(op::BODY_POS, &mut a);
    Vector3f {
        x: a.f[0],
        y: a.f[1],
        z: a.f[2],
    }
}

fn lights_at_node(node: usize, pos: Vector3f, out: &mut Vec<usize>) {
    let mut a = A::new();
    a.p[0] = node;
    mhost::call(op::NODE_INFO, &mut a);
    let (min, max) = (
        Vector3f {
            x: a.f[0],
            y: a.f[1],
            z: a.f[2],
        },
        Vector3f {
            x: a.f[3],
            y: a.f[4],
            z: a.f[5],
        },
    );
    let items: Vec<usize> = slice_of(a.r as *const usize, (a.i[0] + a.i[1]) as usize).to_vec();
    let (children, objects) = items.split_at(a.i[0] as usize);
    for &child in children {
        if point_in_aabb(pos, min, max) {
            lights_at_node(child, pos, out);
        }
    }
    for &o in objects {
        let mut t = A::new();
        t.p[0] = o;
        t.f[..3].copy_from_slice(&[pos.x, pos.y, pos.z]);
        mhost::call(op::LIGHT_TEST, &mut t);
        if t.i[0] != 0 && t.i[1] != 0 {
            out.push(o);
        }
    }
}

fn dist3(a: Vector3f, b: Vector3f) -> f32 {
    let (dx, dy, dz) = (b.x - a.x, b.y - a.y, b.z - a.z);
    fma(dz, dz, fma(dx, dx, dy * dy)).sqrt()
}

pub fn light_level_at_pos(pos: Vector3f, skip: Option<&[usize]>, radius_add: f32) -> f32 {
    let Some(c) = cur_map() else {
        return 0.0;
    };
    let mut amb = A::new();
    mhost::call(op::PLAYER_AMB_LIGHT, &mut amb);
    let player_ambient = amb.r;

    let mut lights = Vec::new();
    for kind in 0..2 {
        let mut r = A::new();
        r.p[0] = c.world;
        r.i[0] = kind;
        mhost::call(op::LIGHT_ROOT, &mut r);
        lights_at_node(r.r, pos, &mut lights);
    }

    let mut level = 0.0f32;
    for light in lights {
        if light == player_ambient || skip.is_some_and(|s| s.contains(&light)) {
            continue;
        }
        let mut p = A::new();
        p.p[0] = light;
        mhost::call(op::LIGHT_PROPS, &mut p);
        let color_max = helper::max_rgb(p.f[0], p.f[1], p.f[2]);
        let kind = helper::light_kind(p.i[0] != 0, p.i[1] != 0, p.i[1] != 0 && p.i[2] != 0);
        let light_pos = Vector3f {
            x: p.f[3],
            y: p.f[4],
            z: p.f[5],
        };
        if kind == helper::LIGHT_BOX {
            level += color_max;
        } else {
            if kind == helper::LIGHT_NEEDS_LOS && !check_line_of_sight(light_pos, pos, true) {
                continue;
            }
            let dist = dist3(light_pos, pos);
            level = helper::light_falloff_add(level, color_max, dist, p.f[6], radius_add);
        }
    }
    level
}
