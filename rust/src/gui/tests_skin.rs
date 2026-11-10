use super::gfx::{
    ANIM_LOOP, ANIM_OSCILLATE, ANIM_RANDOM, ANIM_STOP_AT_END, AnimInput, AnimState, FlipSource,
    Tex, animation_time, flush_changed, restart, set_flip_uv_y, texture_uv, update,
};
use super::guicore::{
    ATTRIBUTES, FONTS, GFXS, SkinKind, gui_mouse_to_mouse, material_blend, material_slot,
    mouse_to_gui_mouse,
};
use super::skin::{SkinError, parse};

#[test]
fn skin_names_round_trip_and_miss() {
    for (kind, names) in [
        (SkinKind::Font, &FONTS[..]),
        (SkinKind::Attribute, &ATTRIBUTES[..]),
        (SkinKind::Gfx, &GFXS[..]),
    ] {
        for (i, n) in names.iter().enumerate() {
            assert_eq!(kind.from_name(n.as_bytes()), i as u32, "{n}");
            assert_eq!(kind.name(i as u32), *n);
        }
        assert_eq!(kind.from_name(b"default"), names.len() as u32);
        assert_eq!(kind.from_name(b""), names.len() as u32);
        assert_eq!(kind.name(names.len() as u32), "Unknown");
        assert_eq!(kind.name(u32::MAX), "Unknown");
    }
    assert_eq!(SkinKind::Attribute.from_name(b"TabFrameButtonSize"), 22);
    assert_eq!(SkinKind::Gfx.from_name(b"TextBoxMarker"), 28);
}

#[test]
fn material_blends() {
    let cases = [
        (0, Some((1, 0))),
        (1, Some((6, 7))),
        (2, Some((6, 7))),
        (3, Some((1, 1))),
        (4, Some((4, 0))),
        (5, Some((1, 7))),
        (6, None),
    ];
    for (ty, want) in cases {
        assert_eq!(material_blend(ty), want, "material {ty}");
    }
    assert_eq!(material_slot(5), 5);
    assert_eq!(material_slot(6), 1);
    assert_eq!(material_slot(u32::MAX), 1);
}

#[test]
fn mouse_buttons_translate() {
    for i in 0..9 {
        assert_eq!(gui_mouse_to_mouse(1 << i), i);
        assert_eq!(mouse_to_gui_mouse(i), 1 << i);
    }
    assert_eq!(gui_mouse_to_mouse(3), 9);
    assert_eq!(gui_mouse_to_mouse(0), 9);
    assert_eq!(mouse_to_gui_mouse(9), 0x200);
    assert_eq!(mouse_to_gui_mouse(-1), 0x200);
}

const SKIN: &str = "<GuiSkin>\r\n<Attributes>\r\n\
<A type=\"WindowButtonSize\" value=\"3 4\"/><!-- c -->\r\n\
<A type=\"Nope\" value=\"1 2 3 4\"/><A value=\"1,2,3\"/></Attributes>\r\n\
<Fonts><F type=\"Default\" file=\"vera.fnt\" size=\"2 3\" color=\"1 0 0\"/></Fonts>\r\n\
<GfxElements>\
<G type=\"PointerText\" file=\"p.tga\" offset=\"-7 -11\" active_size=\"4 5\" color=\"1 0.5 0.25 0.125\" material=\"MODULATIVE\" animated=\"TRUE\" anim_frame_time=\"0.5\" anim_mode=\"Stop_At_End\"/>\
<G type=\"X\" material=\"bogus\" anim_mode=\"weird\"/></GfxElements></GuiSkin>";

#[test]
fn skin_fields_and_defaults() {
    let skin = parse(SKIN.as_bytes(), true).unwrap();
    assert_eq!(skin.attributes.len(), 3);
    assert_eq!(skin.attributes[0].type_name, b"WindowButtonSize");
    assert_eq!(skin.attributes[0].value, [3.0, 4.0, 0.0]);
    assert_eq!(skin.attributes[1].value, [1.0, 2.0, 3.0]);
    assert_eq!(skin.attributes[2].type_name, b"");
    assert_eq!(skin.attributes[2].value, [1.0, 0.0, 0.0]);
    let font = &skin.fonts[0];
    assert_eq!(font.file, b"vera.fnt");
    assert_eq!(font.size, [2.0, 3.0]);
    assert_eq!(font.color, [1.0; 4]);

    let g = &skin.gfx[0];
    assert_eq!(
        (g.type_name.as_slice(), g.file.as_slice()),
        (&b"PointerText"[..], &b"p.tga"[..])
    );
    assert_eq!(g.offset, [-7.0, -11.0]);
    assert!(g.has_active_size());
    assert_eq!(g.color, [1.0, 0.5, 0.25, 0.125]);
    assert_eq!((g.material, g.material_warn.clone()), (4, None));
    assert!(g.animated);
    assert_eq!((g.frame_time, g.anim_mode), (0.5, ANIM_STOP_AT_END));

    let d = &skin.gfx[1];
    assert_eq!(d.file, b"");
    assert_eq!(
        (d.offset, d.active_size, d.color),
        ([0.0; 2], [-1.0; 2], [1.0; 4])
    );
    assert!(!d.has_active_size() && !d.animated);
    assert_eq!(d.frame_time, 1.0);
    assert_eq!(d.material, 1);
    assert_eq!(d.material_warn.as_deref(), Some(&b"bogus"[..]));
    assert_eq!(
        (d.anim_mode, d.anim_warn.as_deref()),
        (ANIM_LOOP, Some(&b"weird"[..]))
    );
}

#[test]
fn skin_errors() {
    assert_eq!(parse(b"", true), Err(SkinError::Xml));
    assert_eq!(parse(b"<a><b></a>", true), Err(SkinError::Xml));
    assert_eq!(parse(SKIN.as_bytes(), false), Err(SkinError::Xml));
    for text in [
        "<GuiSkin/>",
        "<GuiSkin><Fonts/><GfxElements/></GuiSkin>",
        "<GuiSkin><Attributes/><Fonts/></GuiSkin>",
        "<!-- x -->",
    ] {
        assert_eq!(
            parse(text.as_bytes(), true),
            Err(SkinError::MissingSection),
            "{text}"
        );
    }
    assert!(parse(b"<g><Attributes/><Fonts/><GfxElements/></g>", true).is_ok());
}

fn input(ty: i32, len: f32, buffer: usize, dt: f32) -> AnimInput {
    AnimInput {
        animation_count: 1,
        current_animation: 0,
        paused: false,
        ty,
        frame_length: len,
        buffer_len: buffer,
        time_step: dt,
    }
}

fn run(ty: i32, buffer: usize, steps: usize) -> Vec<(i32, bool)> {
    let mut s = AnimState::default();
    (0..steps)
        .map(|_| {
            update(&mut s, &input(ty, 1.0, buffer, 1.0));
            (s.active_image, s.forward)
        })
        .collect()
}

#[test]
fn loop_and_stop_at_end() {
    let looped: Vec<i32> = run(ANIM_LOOP, 3, 7).iter().map(|x| x.0).collect();
    assert_eq!(looped, [1, 2, 0, 1, 2, 0, 1]);
    let stopped: Vec<i32> = run(ANIM_STOP_AT_END, 3, 6).iter().map(|x| x.0).collect();
    assert_eq!(stopped, [1, 2, 2, 2, 2, 2]);
}

#[test]
fn oscillate_restarts_at_one() {
    let osc = run(ANIM_OSCILLATE, 4, 8);
    // wrap pins the frame to 1 and flips direction; backwards it counts from the end
    let want = [
        (1, true),
        (2, true),
        (3, true),
        (2, false),
        (1, false),
        (0, false),
        (1, true),
        (2, true),
    ];
    assert_eq!(osc, want);
}

#[test]
fn update_gates_and_reports_image() {
    let mut s = AnimState::default();
    let mut i = input(ANIM_LOOP, 1.0, 3, 1.0);
    i.paused = true;
    assert_eq!(update(&mut s, &i), None);
    assert_eq!(s, AnimState::default());
    i.paused = false;
    i.current_animation = 1;
    assert_eq!(update(&mut s, &i), None);
    i.current_animation = -1;
    assert_eq!(update(&mut s, &i), None);
    i.current_animation = 0;
    assert_eq!(update(&mut s, &i), Some(1));
    assert_eq!(update(&mut s, &input(ANIM_LOOP, 1.0, 3, 0.25)), None);
    assert_eq!(s.current_frame, 1.25);
    i.animation_count = 0;
    assert_eq!(update(&mut s, &i), None);
}

#[test]
fn out_of_buffer_selects_nothing() {
    let mut s = AnimState::default();
    assert_eq!(update(&mut s, &input(ANIM_STOP_AT_END, 1.0, 0, 1.0)), None);
    assert_eq!((s.active_image, s.current_frame), (-1, -1.0));
    let mut s = AnimState::default();
    assert_eq!(update(&mut s, &input(ANIM_OSCILLATE, 1.0, 1, 5.0)), None);
    assert_eq!((s.active_image, s.forward), (-1, false));
    let mut s = AnimState::default();
    assert_eq!(update(&mut s, &input(ANIM_LOOP, 1.0, 3, -5.0)), None);
    assert_eq!(s.active_image, -5);
}

#[test]
fn random_picks_new_image_on_frame_change() {
    let mut s = AnimState::default();
    assert_eq!(update(&mut s, &input(ANIM_RANDOM, 1.0, 1, 5.0)), None);
    assert_eq!(s, AnimState::default());
    assert_eq!(update(&mut s, &input(ANIM_RANDOM, 1.0, 5, 0.5)), None);
    assert_eq!(s.current_frame, 0.5);
    let image = update(&mut s, &input(ANIM_RANDOM, 1.0, 5, 0.75)).unwrap();
    assert!((1..5).contains(&image));
    assert_eq!(s.current_frame, image as f32);
    assert_eq!(s.active_image, image as i32);
}

#[test]
fn frame_time_arithmetic() {
    let mut s = AnimState::default();
    update(&mut s, &input(ANIM_LOOP, 0.5, 100, 0.25));
    assert_eq!(s.current_frame, 0.5);
    restart(&mut s);
    assert_eq!(s, AnimState::default());
    assert_eq!(animation_time(0.7, Some(0.5)), 0.7 / 0.5);
    assert_eq!(animation_time(0.7, None), 0.7);
}

#[test]
fn texture_uv_2d_and_rect() {
    let mut tex: Tex = [[9.0; 2]; 4];
    let size = texture_uv(&mut tex, [64.0, 32.0], false, [0.25, 0.5], [0.75, 1.0]);
    assert_eq!(size, [32.0, 16.0]);
    assert_eq!(tex, [[0.25, 0.5], [0.75, 0.5], [0.75, 1.0], [0.25, 1.0]]);
    let size = texture_uv(&mut tex, [64.0, 32.0], true, [0.25, 0.5], [0.75, 1.0]);
    assert_eq!(size, [32.0, 16.0]);
    assert_eq!(
        tex,
        [[16.0, 16.0], [48.0, 16.0], [48.0, 32.0], [16.0, 32.0]]
    );
}

#[test]
fn flip_uv_variants() {
    let base: Tex = [[0.0, 1.0], [1.0, 2.0], [2.0, 3.0], [3.0, 4.0]];
    let ys = |t: &Tex| t.map(|v| v[1]);

    let mut t = base;
    set_flip_uv_y(&mut t, true, &FlipSource::Texture);
    assert_eq!(ys(&t), [4.0, 3.0, 2.0, 1.0]);
    set_flip_uv_y(&mut t, false, &FlipSource::Texture);
    assert_eq!(ys(&t), [1.0, 2.0, 3.0, 4.0]);
    // flipping off still swaps: a texture that was never flipped gets flipped
    let mut t = base;
    set_flip_uv_y(&mut t, false, &FlipSource::Texture);
    assert_eq!(ys(&t), [4.0, 3.0, 2.0, 1.0]);

    let image: Tex = [[0.1, 10.0], [0.2, 20.0], [0.3, 30.0], [0.4, 40.0]];
    let mut t = base;
    set_flip_uv_y(&mut t, false, &FlipSource::Image(&image));
    assert_eq!(t, image);
    set_flip_uv_y(&mut t, true, &FlipSource::Image(&image));
    assert_eq!(ys(&t), [30.0, 40.0, 30.0, 40.0]);
    assert_eq!(t.map(|v| v[0]), [0.1, 0.2, 0.3, 0.4]);

    let mut t = base;
    set_flip_uv_y(&mut t, true, &FlipSource::Nothing);
    assert_eq!(t, base);
}

#[test]
fn flush_tracks_update_count() {
    let mut last = -1;
    assert!(flush_changed(&mut last, 1));
    assert!(!flush_changed(&mut last, 1));
    assert!(flush_changed(&mut last, 2));
    assert_eq!(last, 2);
}
