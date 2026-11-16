use super::s8d::{GfxDesc, Group, Host, ImGui, Op, Ptr, Val, id_of};
use super::s8d_sd::{Sd, off};
use super::s8d_text::{SubAction, parse_string};
use std::cell::RefCell;
use std::collections::HashMap;

type Sub = (i32, i32, Vec<u8>);

/// A host that records the engine calls and names gui images and fonts by file.
#[derive(Default)]
struct Fake {
    log: RefCell<Vec<String>>,
    gfx: RefCell<HashMap<Vec<u8>, Ptr>>,
    gfx_names: RefCell<HashMap<Ptr, Vec<u8>>>,
    fonts: RefCell<HashMap<Vec<u8>, Ptr>>,
    font_names: RefCell<HashMap<Ptr, Vec<u8>>>,
    strings: RefCell<HashMap<Ptr, Vec<u8>>>,
    actions: HashMap<Vec<u8>, Vec<Sub>>,
}

impl Fake {
    fn gfx_ptr(&self, name: &[u8]) -> Ptr {
        let mut m = self.gfx.borrow_mut();
        let next = m.len() + 1;
        let p = *m.entry(name.to_vec()).or_insert(0x1000 + next);
        self.gfx_names.borrow_mut().insert(p, name.to_vec());
        p
    }

    fn font_ptr(&self, name: &[u8]) -> Ptr {
        let mut m = self.fonts.borrow_mut();
        let next = m.len() + 1;
        let p = *m.entry(name.to_vec()).or_insert(0x2000 + next);
        self.font_names.borrow_mut().insert(p, name.to_vec());
        p
    }

    fn note(&self, s: String) {
        self.log.borrow_mut().push(s);
    }
}

impl Host for Fake {
    fn string(&self, p: Ptr) -> Vec<u8> {
        self.strings.borrow().get(&p).cloned().unwrap_or_default()
    }
    fn intern(&self, s: &[u8]) -> Ptr {
        let mut m = self.strings.borrow_mut();
        let p = 0x3000 + m.len();
        m.insert(p, s.to_vec());
        p
    }
    fn gfx_element(&self, d: &GfxDesc<'_>) -> Ptr {
        self.gfx_ptr(d.file)
    }
    fn gfx_image_size(&self, _gfx: Ptr) -> [f32; 2] {
        [64.0, 32.0]
    }
    fn gfx_name(&self, gfx: Ptr) -> Vec<u8> {
        self.gfx_names
            .borrow()
            .get(&gfx)
            .cloned()
            .unwrap_or_default()
    }
    fn white(&self) -> Ptr {
        self.gfx_ptr(b"")
    }
    fn font(&self, file: &[u8]) -> Ptr {
        self.font_ptr(file)
    }
    fn font_name(&self, font: Ptr) -> Vec<u8> {
        self.font_names
            .borrow()
            .get(&font)
            .cloned()
            .unwrap_or_default()
    }
    fn font_length(&self, _font: Ptr, size: [f32; 2], text: &[u32]) -> f32 {
        size[0] * 0.5 * text.len() as f32
    }
    fn font_wrap(
        &self,
        _f: Ptr,
        width: f32,
        _row_h: f32,
        size: [f32; 2],
        text: &[u32],
    ) -> Vec<Vec<u32>> {
        let per = (width / (size[0] * 0.5)).max(1.0) as usize;
        text.chunks(per).map(<[u32]>::to_vec).collect()
    }
    fn draw_gfx(&self, _set: Ptr, gfx: Ptr, o: &Op) {
        self.note(format!("gfx {gfx:x} {:?}", o.pos));
    }
    fn draw_font(&self, _set: Ptr, font: Ptr, o: &Op) {
        self.note(format!("font {font:x} {:?}", o.text));
    }
    fn clip_current(&self, _set: Ptr) -> Ptr {
        100
    }
    fn clip_child(&self, parent: Ptr, _pos: [f32; 3], _size: [f32; 2]) -> Ptr {
        self.note(format!("child of {parent}"));
        parent + 1
    }
    fn clip_set(&self, _set: Ptr, clip: Ptr) {
        self.note(format!("clip {clip}"));
    }
    fn set_info(&self, _set: Ptr) -> ([f32; 2], [f32; 2], bool) {
        ([1024.0, 768.0], [0.0, 0.0], false)
    }
    fn screen_size(&self) -> ([f32; 2], [i32; 2]) {
        ([1920.0, 1080.0], [1920, 1080])
    }
    fn create_set(&self, _name: &[u8]) -> Ptr {
        7
    }
    fn set_hud_virtual(&self, _set: Ptr, _size: [f32; 2], _offset: [f32; 2]) {}
    fn set_render(&self, _set: Ptr) {}
    fn wstr(&self, _p: Ptr) -> Vec<u32> {
        Vec::new()
    }
    fn array_size(&self, _a: Ptr) -> usize {
        0
    }
    fn array_at(&self, _a: Ptr, _i: usize) -> Ptr {
        0
    }
    fn array_resize(&self, _a: Ptr, _n: usize) {}
    fn array_push_wstring(&self, _a: Ptr, _s: &[u32]) {}
    fn array_push_bool(&self, _a: Ptr, _b: bool) {}
    fn array_push_i32(&self, _a: Ptr, _v: i32) {}
    fn action_subs(&self, name: &[u8]) -> Option<Vec<SubAction>> {
        self.actions.get(name).map(|v| {
            v.iter()
                .map(|(kind, code, n)| SubAction {
                    kind: *kind,
                    code: *code,
                    name: n.clone(),
                })
                .collect()
        })
    }
    fn translate_button(&self, name: &[u8]) -> Vec<u32> {
        if name == b"MouseLeft" {
            "Left Mouse".chars().map(u32::from).collect()
        } else {
            Vec::new()
        }
    }
    fn key_name(&self, _key: i32) -> Vec<u8> {
        b"K".to_vec()
    }
    fn string_duration(&self, text: &[u32]) -> f32 {
        text.len() as f32
    }
    fn camtex_create(&self, _n: &[u8], _s: [i32; 2], _f: u32, _fov: f32, _near: f32, _far: f32) {}
    fn camtex_destroy(&self, _n: &[u8]) {}
    fn camtex_settings(&self, _n: &[u8], _fov: f32, _near: f32, _far: f32) {}
    fn camtex_matrix(&self, _n: &[u8], _m: Ptr) {}
    fn camtex_attach(&self, _n: &[u8], _e: Ptr) {}
}

fn new_im(h: &Fake) -> &'static mut ImGui {
    // SAFETY: a leaked box, as the game's UIs are.
    unsafe { &mut *ImGui::new_boxed(h, b"test", 9) }
}

/// A script struct block with colours white, a font and a gfx named `file`.
fn block() -> Vec<u8> {
    vec![0u8; 4096]
}

fn put<T: Copy>(b: &mut [u8], o: usize, v: T) {
    // SAFETY: the offsets used by the tests lie within the 4096-byte block.
    unsafe { b.as_mut_ptr().add(o).cast::<T>().write_unaligned(v) };
}

fn sd(b: &[u8]) -> Sd {
    Sd(b.as_ptr())
}

fn label_data(h: &Fake) -> Vec<u8> {
    let mut b = block();
    put(&mut b, off::W_COLOR_BASE, [1.0f32; 4]);
    put(&mut b, off::W_COLOR_TEXT, [1.0f32; 4]);
    put(&mut b, off::W_DEFAULT_SIZE, [-1.0f32, -1.0]);
    put(&mut b, off::W_FONT + off::FONT_SIZE, [16.0f32, 16.0]);
    put(&mut b, off::W_FONT + off::FONT_COLOR, [1.0f32; 4]);
    put(&mut b, off::W_FONT + off::FONT_FILE, h.intern(b"font.fnt"));
    b
}

fn wide(s: &str) -> Vec<u32> {
    s.chars().map(u32::from).collect()
}

#[test]
fn golden_frames_replay() {
    for game in ["soma", "rebirth", "bunker"] {
        let path = format!(
            "{}/tests/golden/S8d/frames-{game}.txt",
            env!("CARGO_MANIFEST_DIR")
        );
        let text = std::fs::read_to_string(path).unwrap();
        let mut frames: Vec<String> = Vec::new();
        for l in text.split_inclusive('\n') {
            if l.starts_with("im ") {
                frames.push(String::new());
            }
            frames.last_mut().unwrap().push_str(l);
        }
        assert!(frames.len() > 30, "{game}");
        let (mut clips, mut texts) = (0, 0);
        for f in &frames {
            let h = Fake::default();
            let im = new_im(&h);
            let ops = parse_frame(&h, f, im);
            im.set_drawn(ops);
            assert_eq!(&im.dump_text(&h), f, "{game}");
            h.log.borrow_mut().clear();
            im.draw_all(&h);
            let log = h.log.borrow();
            let begins = f.lines().filter(|l| l.starts_with("c+ ")).count();
            clips += begins;
            texts += f.lines().filter(|l| l.starts_with("t ")).count();
            assert_eq!(
                log.iter().filter(|l| l.starts_with("child")).count(),
                begins
            );
            assert_eq!(
                log.iter()
                    .filter(|l| l.starts_with("gfx") || l.starts_with("font"))
                    .count(),
                f.lines().filter(|l| l.starts_with(['g', 't'])).count()
            );
            // the clip region ends where it began
            assert_eq!(log.last().map(String::as_str), Some("clip 100"));
        }
        assert!(texts > 0 || game == "bunker");
        let _ = clips;
    }
}

fn bits(s: &str) -> Vec<f32> {
    (0..s.len() / 8)
        .map(|i| f32::from_bits(u32::from_str_radix(&s[i * 8..i * 8 + 8], 16).unwrap()))
        .collect()
}

fn parse_frame(h: &Fake, frame: &str, im: &mut ImGui) -> Vec<Op> {
    let mut ops = Vec::new();
    let mut lines = frame.lines();
    let head = lines.next().unwrap();
    let clear = bits(head.rsplit("clear=").next().unwrap());
    im.screen_clear = [clear[0], clear[1], clear[2], clear[3]];
    im.name = head.split(' ').nth(1).unwrap().as_bytes().to_vec();
    for l in lines {
        let t: Vec<&str> = l.split(' ').collect();
        let k = match t[0] {
            "g" => 2,
            "t" => 3,
            _ => 1,
        };
        let (p, sz, c) = (bits(t[k]), bits(t[k + 1]), bits(t[k + 2]));
        let mut op = Op {
            pos: [p[0], p[1], p[2]],
            size: [sz[0], sz[1]],
            color: [c[0], c[1], c[2], c[3]],
            material: t[k + 3][1..].parse().unwrap(),
            align: t[k + 4][1..].parse().unwrap(),
            angle: bits(&t[k + 5][1..])[0],
            ..Op::default()
        };
        match t[0] {
            "g" => op.gfx = h.gfx_ptr(t[1].as_bytes()),
            "t" => {
                op.font = h.font_ptr(t[2].trim_start_matches('|').as_bytes());
                op.text = t[1]
                    .split(',')
                    .filter(|x| !x.is_empty())
                    .map(|x| u32::from_str_radix(x, 16).unwrap())
                    .collect();
            }
            _ => {}
        }
        ops.push(op);
    }
    ops
}

#[test]
fn label_is_one_text_op_at_the_widget_origin() {
    let h = Fake::default();
    let im = new_im(&h);
    let b = label_data(&h);
    im.begin(0.016);
    im.do_label(
        &h,
        &wide("Hi"),
        sd(&b),
        [10.0, 20.0, 1.0],
        [100.0, 40.0],
        1.0,
    );
    im.end(&h);
    let o = &im.dump_text(&h);
    assert!(o.contains("t 48,69, |font.fnt"), "{o}");
    assert_eq!(im.drawn_count(), 1);
    // centred vertically: (40 - 16) / 2 on top of y, z + 0.2
    let line = o.lines().nth(1).unwrap();
    assert!(
        line.contains(&format!("{:08x}", 32.0f32.to_bits())),
        "{line}"
    );
    assert!(
        line.contains(&format!("{:08x}", 1.2f32.to_bits())),
        "{line}"
    );
}

#[test]
fn window_group_clips_between_start_and_end() {
    let h = Fake::default();
    let im = new_im(&h);
    let mut b = label_data(&h);
    put(&mut b, off::WINDOW_PAD_TOP, 2.0f32);
    im.begin(0.016);
    im.do_window_start(&h, &wide("cap"), sd(&b), [0.0; 3], [200.0, 100.0], true);
    assert_eq!(im.groups.len(), 1);
    im.pop_group();
    im.end(&h);
    let d = im.dump_text(&h);
    let (begin, end) = (d.find("\nc+ ").unwrap(), d.find("\nc- ").unwrap());
    assert!(begin < end);
}

#[test]
fn states_defaults_fades_and_timers() {
    let h = Fake::default();
    let im = new_im(&h);
    let id = id_of(b"x");
    assert!(matches!(im.get_or_set(id, Val::Int(5)), Val::Int(5)));
    // the default stuck: a second default is ignored
    assert!(matches!(im.get_or_set(id, Val::Int(9)), Val::Int(5)));
    im.inc_val(id, Val::Int(2));
    assert!(matches!(im.get_or_set(id, Val::Int(0)), Val::Int(7)));
    let fid = id_of(b"f");
    im.set_val(fid, Val::Float(0.0));
    im.fade(fid, 0, [10.0, 0.0, 0.0, 0.0], 1.0, 0, true);
    im.begin(0.5);
    assert!((im.state(fid).float - 5.0).abs() < 1e-3 || im.state(fid).float > 0.0);
    im.begin(1.0);
    assert!((im.state(fid).float - 10.0).abs() < 1e-5);
    im.timers.push(super::s8d::Timer {
        id: 1,
        time: 0.4,
        repeat: false,
        touched: true,
    });
    im.begin(0.5);
    im.end(&h);
    assert!(im.timers.is_empty());
}

#[test]
fn input_actions_expand_to_names_or_icons() {
    let mut h = Fake::default();
    h.actions.insert(
        b"Fire".to_vec(),
        vec![(2, 0, b"MouseLeft".to_vec()), (0, 0, b"Pad_A".to_vec())],
    );
    let params = block();
    let out = parse_string(
        &h,
        &wide("Press $Input{Fire} now $Input{Nope}"),
        None,
        sd(&params),
    );
    let s: String = out.iter().filter_map(|&c| char::from_u32(c)).collect();
    assert_eq!(s, "Press Left Mouse / Pad_A now BADACTION");
}

#[test]
fn hud_geometry_widens_the_centre_area() {
    let h = Fake::default();
    // SAFETY: the geometry block is leaked and lives for the run.
    let g = unsafe { &*super::s8d_hud::geom(&h, true) };
    assert_eq!(g.size[1], 768.0);
    assert!((g.size[0] - (3072.0 * 1920.0 / 1080.0 - 1024.0) / 3.0).abs() < 1e-3);
    assert_eq!(g.start[0], -g.offset[0]);
}

#[test]
fn group_origin_offsets_layout() {
    let h = Fake::default();
    let im = new_im(&h);
    im.begin(0.0);
    im.push_group(Group {
        pos: [5.0, 6.0, 0.0],
        size: [50.0, 50.0],
        clip: false,
    });
    let (mut p, mut s) = ([1.0, 1.0, 0.0], [-1.0, -1.0]);
    im.layout(&mut p, &mut s, [10.0, 12.0], false);
    assert_eq!((p, s), ([6.0, 7.0, 0.0], [10.0, 12.0]));
}
