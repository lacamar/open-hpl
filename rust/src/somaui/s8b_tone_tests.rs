use super::*;
use std::cell::{Cell, RefCell};

#[derive(Default)]
struct Fake {
    log: RefCell<Vec<String>>,
    next: Cell<usize>,
    world: Cell<usize>,
}

impl Host for Fake {
    fn grading_tex(&self, name: &[u8]) -> Ptr {
        let id = self.next.get() + 1;
        self.next.set(id);
        self.log
            .borrow_mut()
            .push(format!("tex+ {}", String::from_utf8_lossy(name)));
        id
    }
    fn noise_tex(&self) -> Ptr {
        self.log.borrow_mut().push("noise".into());
        99
    }
    fn tex_destroy(&self, t: Ptr) {
        self.log.borrow_mut().push(format!("tex- {t}"));
    }
    fn tex_name(&self, _: Ptr) -> Option<Vec<u8>> {
        None
    }
    fn world_grading_name(&self, _: Ptr) -> Vec<u8> {
        b"w.dds".to_vec()
    }
    fn world_tone(&self, _: Ptr) -> [f32; 3] {
        [0.25, 1.0, 4.0]
    }
    fn world_areas(&self, _: Ptr) -> Vec<ExposureArea> {
        Vec::new()
    }
    fn current_world(&self) -> Ptr {
        self.world.get()
    }
    fn current_viewport(&self) -> Ptr {
        0
    }
    fn tone_object(&self) -> Ptr {
        1
    }
    fn camera_pos(&self) -> Option<[f32; 3]> {
        Some([0.0; 3])
    }
    fn set_tone(&self, key: f32, scale: f32, cut: f32, gamma: f32) {
        self.log
            .borrow_mut()
            .push(format!("tone {key} {scale} {cut} {gamma}"));
    }
    fn set_layers(&self, l: &[(Ptr, f32)]) {
        self.log.borrow_mut().push(format!("layers {l:?}"));
    }
    fn set_grading(&self, tex: Ptr, target: Ptr, blend: f32) {
        self.log
            .borrow_mut()
            .push(format!("grading {tex} {target} {blend}"));
    }
    fn user_bloom(&self) -> bool {
        true
    }
    fn set_bloom(&self, a: bool, b: f32, w: f32, _: [f32; 4]) {
        self.log.borrow_mut().push(format!("bloom {a} {b} {w}"));
    }
    fn set_grain(&self, tex: Ptr, i: f32) {
        self.log.borrow_mut().push(format!("grain {tex} {i}"));
    }
    fn set_srgb(&self, on: bool) {
        self.log.borrow_mut().push(format!("srgb {on}"));
    }
}

fn drain(h: &Fake) -> Vec<String> {
    std::mem::take(&mut *h.log.borrow_mut())
}

#[test]
fn map_load_and_update_order() {
    create();
    let h = Fake::default();
    h.world.set(7);
    map_loaded(&h, 7);
    assert_eq!(
        drain(&h),
        [
            "tex+ w.dds",
            "tone 0.25 2 4 2.2",
            "layers []",
            "grading 1 0 0",
            "bloom true 0.75 128",
            "noise",
            "grain 99 1",
            "srgb false"
        ]
    );
    assert_eq!(world_exposure(), Some((1.0, 4.0)));
}

#[test]
fn grading_fade_promotes_target() {
    create();
    let h = Fake::default();
    fade_grading(&h, b"a.dds", 2.0);
    update(&h, 1.0);
    let l = drain(&h);
    assert!(l.contains(&"tex+ grading_default.dds".to_string()), "{l:?}");
    assert!(l.contains(&"grading 2 1 0.5".to_string()), "{l:?}");
    update(&h, 1.0);
    let l = drain(&h);
    assert!(l.contains(&"tex- 2".to_string()), "{l:?}");
    assert!(l.contains(&"grading 1 0 1".to_string()), "{l:?}");
}

#[test]
fn layers_sorted_and_removed() {
    create();
    let h = Fake::default();
    set_layer(&h, 5, b"b", 0.5);
    set_layer(&h, 1, b"a", 0.25);
    set_layer(&h, 5, b"c", 1.0);
    set_layer(&h, 1, b"a", 0.75);
    update(&h, 0.0);
    let l = drain(&h);
    assert!(
        l.contains(&"layers [(2, 0.75), (1, 0.5), (3, 1.0)]".to_string()),
        "{l:?}"
    );
    remove_layer(&h, b"b");
    assert_eq!(drain(&h), ["tex- 1"]);
    remove_layer(&h, b"zz");
    assert!(drain(&h).is_empty());
}

#[test]
fn exposure_fades_only_in_the_current_world() {
    create();
    let h = Fake::default();
    h.world.set(3);
    map_loaded(&h, 3);
    drain(&h);
    fade_exposure(2.0, 4.0, 1.0);
    update(&h, 0.5);
    let first = drain(&h);
    assert!(first.first().is_some_and(|s| s.starts_with("tone 0.25 ")));
    h.world.set(4);
    update(&h, 0.5);
    assert_eq!(drain(&h).first(), first.first(), "frozen in another world");
}
