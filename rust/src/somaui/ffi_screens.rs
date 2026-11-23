//! C ABI of the gamma screen and splash, mirrored in `soma/src/game/RustSomaUi.h`.

use super::screens::{GammaLayout, SplashPlan};

const _: () = {
    use std::mem::size_of;
    assert!(size_of::<GammaLayout>() == 56);
    assert!(size_of::<SplashPlan>() == 64);
};
