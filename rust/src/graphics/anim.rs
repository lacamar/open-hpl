//! `cAnimationTrack` key-frame lookup, interpolation and sorted insertion.

use crate::math::{clamp, fma, quat_normalize, quat_scale, quat_slerp, v3};
use crate::types::{Quaternion, Vector3f};

/// Mirror of `hpl::cKeyFrame`.
#[repr(C)]
#[derive(Clone, Copy, Debug, Default, PartialEq)]
pub struct KeyFrame {
    pub trans: Vector3f,
    pub rotation: Quaternion,
    pub time: f32,
}

const _: () = {
    use std::mem::{offset_of, size_of};
    assert!(size_of::<KeyFrame>() == 32);
    assert!(offset_of!(KeyFrame, rotation) == 12 && offset_of!(KeyFrame, time) == 28);
};

/// The track's `std::vector<cKeyFrame*>`.
#[derive(Clone, Copy)]
pub struct Frames<'a>(&'a [*const KeyFrame]);

impl<'a> Frames<'a> {
    /// # Safety
    /// Every pointer is non-null, aligned and points to a live `KeyFrame` for `'a`.
    pub unsafe fn new(frames: &'a [*const KeyFrame]) -> Self {
        Self(frames)
    }

    fn len(&self) -> usize {
        self.0.len()
    }

    fn get(&self, i: usize) -> KeyFrame {
        self.0
            .get(i)
            // SAFETY: `Frames::new` contract.
            .map(|p| unsafe { **p })
            .unwrap_or_default()
    }

    fn time(&self, i: usize) -> f32 {
        self.get(i).time
    }
}

/// `GetKeyFramesAtTime`: indices of frames A and B and the blend weight.
///
/// The C++ indexes before the vector for a NaN time, past it for an empty track, and does so for
/// a time above every frame once `ClearKeyFrames` left a stale maximum; all three give frame 0.
pub fn frames_at_time(
    frames: Frames,
    anim_length: f32,
    max_frame_time: f32,
    time: f32,
) -> (usize, usize, f32) {
    let n = frames.len();
    if n == 0 {
        return (0, 0, 0.0);
    }
    let time = clamp(time, 0.0, anim_length);
    if time >= max_frame_time {
        return (n - 1, 0, 0.0);
    }
    let Some(b) = (0..n).find(|&i| time <= frames.time(i)) else {
        return (0, 0, 0.0);
    };
    if b == 0 {
        return (0, 0, 0.0);
    }
    let a = b - 1;
    let ta = frames.time(a);
    let delta = frames.time(b) - ta;
    (a, b, (time - ta) / delta)
}

pub fn interpolate(frames: Frames, anim_length: f32, max_frame_time: f32, time: f32) -> KeyFrame {
    let mut result = KeyFrame {
        time,
        ..KeyFrame::default()
    };
    if frames.len() == 0 {
        result.rotation = Quaternion {
            v: v3(0.0, 0.0, 0.0),
            w: 1.0,
        };
        result.trans = v3(0.0, 0.0, 0.0);
        return result;
    }
    let (a, b, t) = frames_at_time(frames, anim_length, max_frame_time, time);
    let fa = frames.get(a);
    if t == 0.0 {
        result.rotation = fa.rotation;
        result.trans = fa.trans;
        return result;
    }
    let fb = frames.get(b);
    let mut rot_a = fa.rotation;
    let mut rot_b = fb.rotation;
    quat_normalize(&mut rot_a);
    quat_normalize(&mut rot_b);
    if rot_a.w < 0.0 {
        rot_a = quat_scale(&rot_a, -1.0);
    }
    if rot_b.w < 0.0 {
        rot_b = quat_scale(&rot_b, -1.0);
    }
    result.rotation = quat_slerp(t, &rot_a, &rot_b, true);
    let inv = 1.0f32 - t;
    result.trans = v3(
        fma(fa.trans.x, inv, fb.trans.x * t),
        fma(fa.trans.y, inv, fb.trans.y * t),
        fma(fa.trans.z, inv, fb.trans.z * t),
    );
    result
}

/// `CreateKeyFrame`: the insert position and whether the track maximum becomes `time`.
pub fn insert_position(frames: Frames, max_frame_time: f32, time: f32) -> (usize, bool) {
    let n = frames.len();
    if time > max_frame_time || n == 0 {
        return (n, true);
    }
    let pos = (0..n).find(|&i| time < frames.time(i)).unwrap_or(n);
    (pos, false)
}
