// The negated compares keep the C++ NaN behaviour.
#![allow(clippy::neg_cmp_op_on_partial_ord)]

//! Player state decisions: the `Normal`, `HandObject`, `UseItem` and `DefaultBase` action tables,
//! focus and crosshair choice, use-item plans, and the shared interact-state action split.

pub const ACT_HOLSTER: i32 = 1;
pub const ACT_ATTACK: i32 = 2;
pub const ACT_INTERACT: i32 = 3;
pub const ACT_LANTERN: i32 = 4;

pub const PS_NONE: i32 = 0;
pub const PS_NORMAL: i32 = 1;
pub const PS_HAND: i32 = 2;
pub const PS_PUSH: i32 = 4;
pub const PS_GRAB: i32 = 5;

/// `ImplementedDoAction` result: `consumed` is the C++ `false` return.
#[repr(C)]
#[derive(Clone, Copy, Debug, Default, PartialEq)]
pub struct Action {
    pub pass: u32,
    pub change: i32,
    pub forward_to_hands: u32,
    pub use_item: u32,
}

const PASS: Action = Action {
    pass: 1,
    change: PS_NONE,
    forward_to_hands: 0,
    use_item: 0,
};

const fn eat(change: i32) -> Action {
    Action {
        pass: 0,
        change,
        forward_to_hands: 0,
        use_item: 0,
    }
}

pub fn normal_action(act: i32, pressed: bool) -> Action {
    if act == ACT_HOLSTER && pressed {
        return eat(PS_HAND);
    }
    PASS
}

pub fn hand_object_action(act: i32, pressed: bool) -> Action {
    if act == ACT_HOLSTER && pressed {
        return eat(PS_NORMAL);
    }
    if act == ACT_ATTACK {
        return Action {
            forward_to_hands: 1,
            ..eat(PS_NONE)
        };
    }
    PASS
}

pub fn use_item_action(act: i32, pressed: bool) -> Action {
    if act == ACT_HOLSTER && pressed {
        return eat(PS_HAND);
    }
    if (act == ACT_INTERACT || act == ACT_ATTACK) && pressed {
        return Action {
            use_item: 1,
            ..eat(PS_NORMAL)
        };
    }
    PASS
}

/// `HandObject` leave: whether the hand object stops being drawn.
pub fn hand_object_hides(new_state: i32) -> bool {
    matches!(new_state, PS_NORMAL | PS_PUSH | PS_GRAB)
}

/// `DefaultBase::OnDoAction` after the implemented action passed: `(pass, interact)`.
pub fn default_base_interact(act: i32, pressed: bool, can_interact: bool) -> (bool, bool) {
    if act == ACT_INTERACT && pressed {
        return (false, can_interact);
    }
    (true, false)
}

/// Shared tail of the interact states: 1 = interact released, 2 = attack pressed, else 0.
pub fn interact_action(act: i32, pressed: bool) -> i32 {
    if act == ACT_INTERACT {
        if !pressed {
            return 1;
        }
    } else if act == ACT_ATTACK && pressed {
        return 2;
    }
    0
}

/// `(release, pass)` for the ladder state's `OnDoAction`.
pub fn ladder_action(act: i32, pressed: bool) -> (bool, bool) {
    (
        pressed && (act == ACT_INTERACT || act == ACT_ATTACK),
        act == ACT_LANTERN,
    )
}

/// `entity && distance < maxFocus`.
pub fn focus_in_range(has_entity: bool, dist: f32, max_focus: f32) -> bool {
    has_entity && dist < max_focus
}

/// `CanInteractWithEntity` once the entity's own checks are known.
pub fn focus_can_interact(in_range: bool, can_interact: bool, disabled: bool) -> bool {
    in_range && can_interact && !disabled
}

pub const CROSSHAIR_DEFAULT_GFX: i32 = -1;
pub const CROSSHAIR_SIMPLE_GFX: i32 = -2;

/// `GetCrosshair` after `GetFocusCrosshair`: the focus gfx index, or one of the negative gfx codes.
pub fn crosshair_pick(
    crosshair: i32,
    default_crosshair: i32,
    last_enum: i32,
    disabled: bool,
    style_default: bool,
) -> i32 {
    let crosshair = if disabled {
        default_crosshair
    } else {
        crosshair
    };
    if crosshair == last_enum || crosshair == default_crosshair {
        CROSSHAIR_DEFAULT_GFX
    } else if style_default {
        crosshair
    } else {
        CROSSHAIR_SIMPLE_GFX
    }
}

/// `AddOutlineObjects` early exit: false when no outline is drawn.
pub fn outline_gate(has_entity: bool, is_prop: bool, dist: f32, max_focus: f32) -> bool {
    has_entity && is_prop && !(dist > max_focus)
}

/// `ShowOutlineOnEntity` for a prop, `item` and `can_interact` already known.
pub fn show_outline(is_item: bool, can_interact: bool, disabled: bool) -> bool {
    is_item && can_interact && !disabled
}

/// `cMath::Max(maxFocus, minUse)`.
pub fn use_item_max_focus(max_focus: f32, min_use: f32) -> f32 {
    if max_focus > min_use {
        max_focus
    } else {
        min_use
    }
}

pub const PLAN_NONE: i32 = 0;
pub const PLAN_CALLBACK: i32 = 1;
pub const PLAN_DOES_NOT_WORK: i32 = 2;
pub const PLAN_NO_OBJECT: i32 = 3;

/// `UseItem` branch: callback lookup runs only when `in_focus` holds.
pub fn use_item_focus(has_item: bool, has_entity: bool, has_map: bool, dist: f32, max: f32) -> i32 {
    if !has_item {
        return PLAN_NONE;
    }
    if has_entity && has_map && dist < max {
        PLAN_CALLBACK
    } else {
        PLAN_NO_OBJECT
    }
}

/// `UseItem` outcome once the callback lookup is known.
pub fn use_item_plan(callback: bool) -> i32 {
    if callback {
        PLAN_CALLBACK
    } else {
        PLAN_DOES_NOT_WORK
    }
}

pub const DRAW_NO: i32 = 0;
pub const DRAW_CHECK_CALLBACK: i32 = 1;
pub const DRAW_YES: i32 = 2;

/// `OnDrawCrossHair` stage one; props other than items skip the callback test.
pub fn use_item_draw(has_entity: bool, dist: f32, max: f32, is_prop: bool, is_item: bool) -> i32 {
    if !has_entity || dist > max {
        return DRAW_NO;
    }
    if is_prop {
        if is_item { DRAW_NO } else { DRAW_YES }
    } else {
        DRAW_CHECK_CALLBACK
    }
}
