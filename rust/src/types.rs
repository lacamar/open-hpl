//! `#[repr(C)]` mirrors of the HPL2 value types that cross the C ABI.
//! Layout is asserted on both sides: here and in `math/RustTypes.h`.

#[repr(C)]
#[derive(Clone, Copy, Debug, Default, PartialEq)]
pub struct Vector2<T> {
    pub x: T,
    pub y: T,
}

#[repr(C)]
#[derive(Clone, Copy, Debug, Default, PartialEq)]
pub struct Vector3<T> {
    pub x: T,
    pub y: T,
    pub z: T,
}

pub type Vector2f = Vector2<f32>;
pub type Vector2l = Vector2<i32>;
pub type Vector3f = Vector3<f32>;
pub type Vector3l = Vector3<i32>;

#[repr(C)]
#[derive(Clone, Copy, Debug, Default, PartialEq)]
pub struct Color {
    pub r: f32,
    pub g: f32,
    pub b: f32,
    pub a: f32,
}

/// Row-major, `m[row][col]`, as `hpl::cMatrix<float>`.
#[repr(C)]
#[derive(Clone, Copy, Debug, Default, PartialEq)]
pub struct Matrixf {
    pub m: [[f32; 4]; 4],
}

#[repr(C)]
#[derive(Clone, Copy, Debug, Default, PartialEq)]
pub struct Quaternion {
    pub v: Vector3f,
    pub w: f32,
}

const _: () = {
    use std::mem::{align_of, offset_of, size_of};
    assert!(size_of::<Vector2f>() == 8 && size_of::<Vector2l>() == 8);
    assert!(size_of::<Vector3f>() == 12 && size_of::<Vector3l>() == 12);
    assert!(size_of::<Color>() == 16 && offset_of!(Color, a) == 12);
    assert!(size_of::<Matrixf>() == 64 && align_of::<Matrixf>() == 4);
    assert!(size_of::<Quaternion>() == 16 && offset_of!(Quaternion, w) == 12);
};
