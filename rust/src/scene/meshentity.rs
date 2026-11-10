//! Per-entity animation step of `cMeshEntity::UpdateLogic`.

use super::animstate::AnimCore;

/// Advances every active state by `dt`, in order.
pub fn update_active<'a>(states: impl Iterator<Item = &'a mut AnimCore>, dt: f32) {
    for s in states.filter(|s| s.active) {
        s.update(dt);
    }
}
