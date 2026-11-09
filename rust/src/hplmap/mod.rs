//! Parse side of the `.map` loader (`WorldLoaderHplMap`): walks the MapData element and the
//! `.map_cache` binary, and drives a [`Builder`] that creates the engine objects. The C++ side of the
//! seam is `HPL2/core/include/resources/HplMapBuilder.h`.

pub mod abi;
mod cache;
mod parse;
#[cfg(test)]
mod tests;

pub use cache::{read_cache, write_cache};
pub use parse::parse_map;

use crate::binary_buffer::BinaryBuffer;
use crate::types::{Color, Matrixf, Vector2f, Vector3f};

pub const FLAG_NO_LIGHTS: u32 = 0x01;
pub const FLAG_NO_ENTITIES: u32 = 0x02;
pub const FLAG_NO_GAME_ENTITIES: u32 = 0x04;
pub const FLAG_FAST_STATIC_LOAD: u32 = 0x20;

#[derive(Clone, Copy, Debug, PartialEq, Eq)]
pub enum Phase {
    Static,
    StaticObjects,
    Primitives,
    Decals,
    Combos,
    Entities,
}

#[derive(Clone, Copy, Debug, PartialEq, Eq)]
pub enum Parsed {
    Ok,
    NoContents,
    /// A node the original dereferenced unchecked is missing, or a count would not allocate.
    Abort,
}

#[derive(Clone, Copy, Debug, PartialEq, Eq)]
pub enum NodeKind {
    FogArea,
    ParticleSystem,
    Sound,
    Billboard,
    Light,
}

#[derive(Clone, Copy, Debug, PartialEq)]
pub struct Fog {
    pub active: bool,
    pub color: Color,
    pub falloff_exp: f32,
    pub start: f32,
    pub end: f32,
    pub culling: bool,
}

#[derive(Clone, Copy, Debug, PartialEq)]
pub struct Placement {
    pub pos: Vector3f,
    pub rot: Vector3f,
    pub scale: Vector3f,
}

#[derive(Clone, Copy, Debug, PartialEq)]
pub struct StaticObject<'a> {
    pub name: &'a [u8],
    pub file: &'a [u8],
    pub at: Placement,
    pub collides: bool,
    pub cast_shadows: bool,
    pub id: i32,
}

#[derive(Clone, Copy, Debug, PartialEq)]
pub struct Plane<'a> {
    pub name: &'a [u8],
    pub material: &'a [u8],
    pub cast_shadows: bool,
    pub collides: bool,
    pub id: i32,
    pub at: Placement,
    pub start_corner: Vector3f,
    pub end_corner: Vector3f,
    pub uv: [Vector2f; 4],
}

#[derive(Clone, Copy, Debug, PartialEq)]
pub struct Decal<'a, N> {
    pub name: &'a [u8],
    pub color: Color,
    pub id: i32,
    pub material: &'a [u8],
    pub mesh: Option<N>,
}

#[derive(Clone, Copy, Debug, PartialEq)]
pub struct Entity<'a, N> {
    pub name: &'a [u8],
    pub id: i32,
    pub active: bool,
    pub at: Placement,
    /// The file for an entity, the area type for an area.
    pub file: &'a [u8],
    pub user_vars: Option<N>,
}

#[derive(Clone, Copy, Debug, PartialEq)]
pub struct CacheShape {
    pub kind: i32,
    pub size: Vector3f,
    pub offset: Matrixf,
}

#[derive(Clone, Copy, Debug, PartialEq)]
pub struct ShapeBody<'a> {
    pub material: &'a [u8],
    pub transform: Matrixf,
    pub char_collider: bool,
    pub blocks_light: bool,
    pub colliders: &'a [CacheShape],
}

/// `data` holds `count` elements: 4 bytes each unless the format is byte and `compression` is 0.
#[derive(Clone, Copy, Debug, PartialEq)]
pub struct CacheArray<'a> {
    pub kind: i32,
    pub format: i32,
    pub prog_var: i32,
    pub elem_num: i32,
    pub compression: i32,
    pub count: usize,
    pub data: &'a [u8],
}

#[derive(Clone, Copy, Debug, PartialEq)]
pub struct CacheMesh<'a> {
    pub name: &'a [u8],
    pub material: &'a [u8],
    pub cast_shadows: bool,
    pub vertex_num: i32,
    pub arrays: &'a [CacheArray<'a>],
    pub indices: &'a [u32],
}

#[derive(Clone, Copy, Debug, PartialEq)]
pub struct CacheBody<'a> {
    pub name: &'a [u8],
    pub material: &'a [u8],
    pub blocks_light: bool,
    pub char_collider: bool,
}

#[derive(Clone, Copy, Debug, PartialEq)]
pub struct CacheSummary {
    pub mesh_bodies: i32,
    pub shape_bodies: i32,
    pub mesh_entities: i32,
}

/// Read-only element tree; nodes are cheap handles into it.
pub trait Xml {
    type Node: Copy;
    fn first_child(&self, node: Self::Node, name: &[u8]) -> Option<Self::Node>;
    fn children(&self, node: Self::Node) -> Vec<Self::Node>;
    fn attr(&self, node: Self::Node, name: &[u8]) -> Option<&[u8]>;
    fn value(&self, node: Self::Node) -> &[u8];
}

/// Creates the engine objects a map describes. Messages are the loader's own log lines.
pub trait Builder<N: Copy> {
    fn log(&mut self, msg: &[u8]);
    fn warning(&mut self, msg: &[u8]);
    fn error(&mut self, msg: &[u8]);
    fn phase(&mut self, phase: Phase, begin: bool);

    fn set_fog(&mut self, fog: Fog);
    fn set_skybox(&mut self, active: bool, color: Color, texture: &[u8]);

    fn static_begin(&mut self);
    fn static_object(&mut self, object: &StaticObject);
    fn static_plane(&mut self, plane: &Plane);
    fn static_decal(&mut self, decal: &Decal<N>);
    fn static_loading_done(&mut self);
    fn static_combo(&mut self, group_id: i32, ids: &[i32]);
    fn static_compile(&mut self);

    fn entities_begin(&mut self);
    fn entities_end(&mut self);
    fn entity(&mut self, entity: &Entity<N>);
    fn area(&mut self, area: &Entity<N>);
    fn node(&mut self, kind: NodeKind, node: N);

    fn cache_header(&mut self, summary: CacheSummary);
    /// The mesh shape follows in `buffer`; the builder consumes it.
    fn cache_mesh_body(&mut self, body: &CacheBody, buffer: *mut BinaryBuffer);
    fn cache_shape_body(&mut self, body: &ShapeBody);
    fn cache_mesh(&mut self, mesh: &CacheMesh);
}

#[derive(Clone, Copy, Debug)]
pub struct Options<'a> {
    pub flags: u32,
    pub log_cache: bool,
    pub file: &'a [u8],
}

#[derive(Clone, Copy, Debug)]
pub struct BodyOut<'a> {
    pub name: &'a [u8],
    pub material: &'a [u8],
    pub blocks_light: bool,
    pub char_collider: bool,
}

#[derive(Clone, Copy, Debug)]
pub struct MeshOut<'a> {
    pub name: &'a [u8],
    pub material: &'a [u8],
    pub cast_shadows: bool,
    pub vertex_num: i32,
    pub arrays: &'a [CacheArray<'a>],
    pub indices: &'a [u32],
}

/// Static data of a world, in the order the cache stores it. `data` of a `CacheArray` is `None`
/// (empty here) when the array format is invalid; Normal, Color0 and Texture1Tangent arrays
/// are always float data.
pub trait CacheSource {
    fn error(&mut self, msg: &[u8]);
    fn counts(&mut self) -> CacheSummary;
    fn next_body(&mut self) -> BodyOut<'_>;
    /// Appends the mesh shape of the body just returned to the cache buffer.
    fn write_body_shape(&mut self);
    fn next_shape_body(&mut self) -> ShapeBody<'_>;
    fn next_mesh(&mut self) -> MeshOut<'_>;
}
