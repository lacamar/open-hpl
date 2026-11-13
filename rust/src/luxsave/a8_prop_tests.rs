use crate::luxprop::a5_small::{A5Button, A5Door, A5Oil, A5Photocell};
use crate::luxsave::a8_props::*;
use crate::system::serialize::{Class, Field, M_VAR, T_BOOL, T_FLOAT32, register};
use std::sync::Arc;

fn reg(name: &str, vars: &[(&str, usize, usize, u32)]) {
    register(Class {
        name: name.as_bytes().to_vec(),
        parent: Vec::new(),
        fields: vars
            .iter()
            .map(|&(n, offset, size, ty)| {
                Arc::new(Field {
                    name: n.as_bytes().to_vec(),
                    class_name: Vec::new(),
                    offset,
                    size,
                    ty,
                    main: M_VAR,
                    array_size: 0,
                })
            })
            .collect(),
        size: 32,
        create: None,
    });
}

#[test]
fn a5_states_round_trip_through_registered_offsets() {
    reg(
        "cLuxProp_Button_SaveData",
        &[
            ("mbSwitchedOn", 9, 1, T_BOOL),
            ("mbCanBeSwitchedOn", 10, 1, T_BOOL),
            ("mbCanBeSwitchedOff", 11, 1, T_BOOL),
        ],
    );
    reg(
        "cLuxProp_OilBarrel_SaveData",
        &[
            ("mbAmountCalculated", 8, 1, T_BOOL),
            ("mfOilAmount", 12, 4, T_FLOAT32),
        ],
    );
    reg(
        "cLuxProp_LevelDoor_SaveData",
        &[("mbLocked", 8, 1, T_BOOL), ("mbShowStats", 9, 1, T_BOOL)],
    );
    reg(
        "cLuxProp_Photocell_SaveData",
        &[
            ("mbLit", 8, 1, T_FLOAT32),
            ("mfLightLevel", 12, 4, T_FLOAT32),
            ("mfLightLevelCheckCount", 16, 4, T_FLOAT32),
            ("mfLightLevelOnLimit", 20, 4, T_FLOAT32),
            ("mfLightLevelOffLimit", 24, 4, T_FLOAT32),
        ],
    );
    let mut buf = [0_u8; 32];
    let p = buf.as_mut_ptr();
    let b = A5Button {
        on: true,
        can_on: false,
        can_off: true,
    };
    let mut b2 = A5Button::default();
    unsafe {
        ohpl_a8_button_save(&b, p);
        ohpl_a8_button_load(&mut b2, p);
    }
    assert_eq!(buf[9..12], [1, 0, 1]);
    assert_eq!(b, b2);

    let o = A5Oil {
        calculated: true,
        amount: 2.5,
    };
    let mut o2 = A5Oil::default();
    unsafe {
        ohpl_a8_oil_save(&o, p);
        ohpl_a8_oil_load(&mut o2, p);
    }
    assert_eq!(o, o2);
    assert_eq!(buf[12..16], 2.5_f32.to_ne_bytes());

    let d = A5Door {
        locked: true,
        show_stats: true,
        locked_count: 0.0,
    };
    let mut d2 = A5Door::default();
    unsafe {
        ohpl_a8_door_save(&d, p);
        ohpl_a8_door_load(&mut d2, p);
    }
    assert_eq!(d, d2);

    let ph = A5Photocell {
        lit: true,
        light_level: 0.5,
        check_count: 1.0,
        on_limit: 0.25,
        off_limit: 0.75,
    };
    let mut ph2 = A5Photocell::default();
    unsafe {
        ohpl_a8_photocell_save(&ph, p);
        ohpl_a8_photocell_load(&mut ph2, p);
    }
    assert_eq!(ph, ph2);
}
