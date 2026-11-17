use super::own::{Data, Handler};

fn handler() -> Handler {
    // SAFETY: the settings tables never touch the host table, the lowlevel or the resources.
    unsafe { Handler::new(std::ptr::null(), std::ptr::null_mut(), std::ptr::null_mut()) }
}

#[test]
fn global_settings_pick_lowest_free_slot_and_fade() {
    let mut h = handler();
    assert_eq!(h.set_global(0, 0.5, 1, -1), 0);
    assert_eq!(h.set_global(0, 0.25, 2, -1), 1);
    assert_eq!(h.set_global(1, 2.0, 1, -1), 0);
    assert_eq!(h.setting_val(0, 0), 0.5);
    assert_eq!(h.setting_val(1, 0), 2.0);
    assert_eq!(h.setting_val(0, 7), 1.0);
    assert!(h.setting_exists(0, 1));
    h.setting_destroy(0, 0);
    assert_eq!(h.set_global(0, 0.75, 4, -1), 0);
    assert_eq!(h.fade_global(0, 0.0, 1.0, 1, 0, true), 0);
    h.update(0.5);
    assert_eq!(h.setting_val(0, 0), 0.25);
    h.update(1.0);
    assert!(!h.setting_exists(0, 0));
}

#[test]
fn global_volume_is_the_minimum_over_the_entries_of_a_type() {
    let mut h = handler();
    h.set_global(0, 0.5, 1, -1);
    h.set_global(0, 0.25, 1, -1);
    h.set_global(0, 0.1, 2, -1);
    h.update(0.0);
    assert_eq!(h.global_volume(1), 0.25);
    assert_eq!(h.global_volume(2), 0.1);
    assert_eq!(h.global_volume(4), 1.0);
    assert_eq!(h.global_volume(0), 1.0);
}

#[test]
fn data_names_and_defaults() {
    let mut d = Data::default();
    assert!(d.use_3d && d.volume == 1.0 && d.random == 1.0);
    assert!(d.random_name(0, true).is_empty());
    d.add_name(0, b"a.ogg");
    assert_eq!(d.random_name(0, true), b"a.ogg");
    d.add_name(0, b"b.ogg");
    d.set_name(0, 1, b"c.ogg");
    assert_eq!(d.name(0, 1), b"c.ogg");
    assert!(d.name(0, 2).is_empty() && d.name(0, -1).is_empty());
    assert!(d.has(0) && !d.has(1));
    d.setup(2.0, 9.0, true, false);
    assert!(d.looping && !d.use_3d && !d.stream && d.max_distance == 9.0);
}
