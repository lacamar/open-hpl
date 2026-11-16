//! Script natives of `cImGui`, `cLuxScriptImGui`, the gui-handler camera textures, the input
//! handler's glyph layouts and the HUD geometry, registered through the recovered declarations.

use super::imgui::{Layout, Prev};
use super::s8d::{
    Group, Host, ImGui, Mods, Timer, Val, current, id_of, preload_gfx, prev_input_focus,
    script_input_focus, set_current, set_input_focus,
};
use super::s8d_hud::{ScriptImGui, create_script_imgui, geom, hud};
use super::s8d_sd::{Sd, off};
use super::s8d_text::{
    add_keyboard_layout, clear_keyboard_layout, draw_screen_text, parse_screen_text,
    set_keyboard_defaults, set_mouse_layout,
};
use crate::somascript::s2_api::new_owned_script_struct;
use crate::somascript::s2_as::{BEHAVE_FACTORY, Engine, Frame, Soma, str_bytes, wstr_units};
use std::ffi::c_void;

const T: &str = "cImGui";

fn im<'a>(f: &Frame) -> &'a mut ImGui {
    // SAFETY: `this` of a `cImGui` method is a UI made by `ImGui::new_boxed`, never freed.
    unsafe { &mut *f.this.cast::<ImGui>() }
}

fn ui<'a>(f: &Frame, i: usize) -> &'a mut ImGui {
    // SAFETY: a `cImGui@` argument is such a UI.
    unsafe { &mut *f.ptr_arg(i).cast::<ImGui>() }
}

fn bs<'a>(f: &Frame, i: usize) -> &'a [u8] {
    // SAFETY: the declaration passes a `const tString &in` in slot `i`.
    unsafe { str_bytes(f.ptr_arg(i)) }
}

fn ws<'a>(f: &Frame, i: usize) -> &'a [u32] {
    // SAFETY: the declaration passes a `const tWString &in` in slot `i`.
    unsafe { wstr_units(f.ptr_arg(i)) }
}

fn sd(f: &Frame, i: usize) -> Sd {
    Sd(f.ptr_arg(i).cast_const().cast())
}

fn me(f: &Frame) -> Sd {
    Sd(f.this.cast_const().cast())
}

fn v3(f: &Frame, i: usize) -> [f32; 3] {
    *f.rf::<[f32; 3]>(i)
}

fn v2(f: &Frame, i: usize) -> [f32; 2] {
    *f.rf::<[f32; 2]>(i)
}

fn col(f: &Frame, i: usize) -> [f32; 4] {
    *f.rf::<[f32; 4]>(i)
}

fn val(f: &Frame, i: usize, k: u8) -> Val {
    match k {
        0 => Val::Int(f.arg(i)),
        1 => Val::Float(f.arg(i)),
        2 => Val::Vec(v3(f, i)),
        _ => Val::Col(col(f, i)),
    }
}

fn ret_val(f: &Frame, v: Val) {
    match v {
        Val::Int(x) => f.ret(x),
        Val::Float(x) => f.ret(x),
        Val::Vec(x) => f.ret(x),
        Val::Col(x) => f.ret(x),
    }
}

fn str_ref(h: &dyn Host, s: Sd, o: usize) -> *mut c_void {
    let p = s.ptr(o);
    (if p == 0 { h.intern(b"") } else { p }) as *mut c_void
}

fn add3(a: [f32; 3], b: [f32; 3]) -> [f32; 3] {
    [a[0] + b[0], a[1] + b[1], a[2] + b[2]]
}

fn osc(im: &ImGui, t: f32, ease: i32) -> f32 {
    let ph = if t > 0.0 {
        im.time_count % (2.0 * t) / t
    } else {
        0.0
    };
    crate::somalux::scriptable::easing(ease, if ph <= 1.0 { ph } else { 2.0 - ph })
}

pub fn register(e: Engine, h: &'static dyn Host) {
    let s = Soma(e);
    s.method("cLuxGuiHandler", r#"void CreateCameraTexture(const tString&in asName, const cVector2l&in avSize, uint alFrameRate, float afFOV, float afNearPlane, float afFarPlane)"#, move |f| { h.camtex_create(bs(f, 0), *f.rf::<[i32; 2]>(1), f.arg(2), f.arg(3), f.arg(4), f.arg(5)); });
    s.method(
        "cLuxGuiHandler",
        r#"void DestroyCameraTexture(const tString&in asName)"#,
        move |f| {
            h.camtex_destroy(bs(f, 0));
        },
    );
    s.method("cLuxGuiHandler", r#"void SetCameraTextureSettings(const tString&in asName, float afFOV, float afNearPlane, float afFarPlane)"#, move |f| { h.camtex_settings(bs(f, 0), f.arg(1), f.arg(2), f.arg(3)); });
    s.method(
        "cLuxGuiHandler",
        r#"void SetCameraTextureMatrix(const tString&in asName, const cMatrixf&in a_mtxCamera)"#,
        move |f| {
            h.camtex_matrix(bs(f, 0), f.ptr_arg(1) as usize);
        },
    );
    s.method(
        "cLuxGuiHandler",
        r#"void AttachCameraTextureToEntity(const tString&in asName, iLuxEntity@ apEnt)"#,
        move |f| {
            h.camtex_attach(bs(f, 0), f.ptr_arg(1) as usize);
        },
    );
    for (decl, material, ty) in [
        ("cImGuiGfx@ f()", false, false),
        ("cImGuiGfx@ f(const tString &in asFile)", false, false),
        (
            "cImGuiGfx@ f(const tString &in asFile, eGuiMaterial aMat)",
            true,
            false,
        ),
        (
            "cImGuiGfx@ f(const tString &in asFile, eGuiMaterial aMat, eImGuiGfx aType)",
            true,
            true,
        ),
        (
            "cImGuiGfx@ f(const tString &in asFile, eImGuiGfx aType)",
            false,
            true,
        ),
        ("cImGuiGfx@ f(const cImGuiGfx &in aGfx)", false, false),
    ] {
        let args = decl.matches(',').count() + usize::from(decl.contains("(const"));
        let copy = decl.contains("const cImGuiGfx &in");
        e.reg_behaviour(
            BEHAVE_FACTORY,
            b"cImGuiGfx",
            decl.as_bytes(),
            Box::new(move |f| {
                let p = new_owned_script_struct(b"cImGuiGfx");
                let g = Sd(p.cast_const().cast());
                if copy {
                    sd(f, 0).copy_to(g, 16, off::GFX_SIZE - 16);
                } else if args > 0 {
                    g.set_ptr(off::GFX_FILE, h.intern(bs(f, 0)));
                    if material {
                        g.set_i32(off::GFX_MATERIAL, f.arg(1));
                    }
                    if ty {
                        g.set_i32(off::GFX_TYPE, f.arg(args - 1));
                    }
                }
                f.ret::<*mut c_void>(p);
            }),
        );
    }
    e.reg_behaviour(
        BEHAVE_FACTORY,
        b"cImGuiFont",
        b"cImGuiFont@ f()",
        Box::new(|f| {
            f.ret::<*mut c_void>(new_owned_script_struct(b"cImGuiFont"));
        }),
    );
    e.reg_behaviour(
        BEHAVE_FACTORY,
        b"cImGuiFont",
        b"cImGuiFont@ f(const tString&in asFile, const cVector2f &in avSize)",
        Box::new(move |f| {
            let p = new_owned_script_struct(b"cImGuiFont");
            let g = Sd(p.cast_const().cast());
            g.set_ptr(off::FONT_FILE, h.intern(bs(f, 0)));
            g.set_v2(off::FONT_SIZE, v2(f, 1));
            f.ret::<*mut c_void>(p);
        }),
    );
    s.method(
        "cImGuiGfx",
        r#"void SetFile(const tString&in asFile)"#,
        move |f| {
            me(f).set_ptr(off::GFX_FILE, h.intern(bs(f, 0)));
        },
    );
    s.method("cImGuiGfx", r#"const tString& GetFile()const"#, move |f| {
        f.ret_ref(str_ref(h, me(f), off::GFX_FILE));
    });
    s.method(
        "cImGuiGfx",
        r#"void CopyFrom(const cImGuiGfx &in aGfx)"#,
        move |f| {
            sd(f, 0).copy_to(me(f), 16, off::GFX_SIZE - 16);
        },
    );
    s.method("cImGuiGfx", r#"uint64 GetId()"#, move |f| {
        f.ret(id_of(&me(f).string(h, off::GFX_FILE)));
    });
    s.method(
        "cImGuiWindowData",
        r#"void SetLabelPadding(float afTop, float afRight, float afBottom, float afLeft)"#,
        move |f| {
            let g = me(f);
            for i in 0..4 {
                g.set_f32(off::WINDOW_LABEL_PAD_TOP + 4 * i, f.arg(i));
            }
        },
    );
    s.method(
        "cImGuiWindowData",
        r#"void SetPadding(float afTop, float afRight, float afBottom, float afLeft)"#,
        move |f| {
            let g = me(f);
            for i in 0..4 {
                g.set_f32(off::WINDOW_PAD_TOP + 4 * i, f.arg(i));
            }
        },
    );
    s.method(
        "cImGuiFont",
        r#"void SetFile(const tString&in asFile)"#,
        move |f| {
            me(f).set_ptr(off::FONT_FILE, h.intern(bs(f, 0)));
        },
    );
    s.method("cImGuiFont", r#"const tString& GetFile()const"#, move |f| {
        f.ret_ref(str_ref(h, me(f), off::FONT_FILE));
    });
    s.method("cImGuiFont", r#"uint64 GetId()"#, move |f| {
        f.ret(id_of(&me(f).string(h, off::FONT_FILE)));
    });
    s.method(
        "cImGuiFrameGfx",
        r#"void CopyFrom(const cImGuiFrameGfx &in aFrame)"#,
        move |f| {
            sd(f, 0).copy_to(me(f), 16, 1592 - 16);
        },
    );
    s.func(r#"cImGui@ cLux_GetCurrentImGui()"#, move |f| {
        let p = current();
        f.ret_ref(if p.is_null() { hud(h) } else { p });
    });
    s.func(r#"cImGui@ cLux_GetGameHudImGui()"#, move |f| {
        f.ret_ref(hud(h));
    });
    s.func(r#"cGuiSet@ cLux_GetGameHudSet()"#, move |f| {
        // SAFETY: the HUD UI is a leaked box.
        f.ret_ref(unsafe { (*hud(h)).set } as *mut c_void);
    });
    s.func(
        r#"const cVector2f& cLux_GetHudVirtualCenterSize()"#,
        move |f| {
            let g = geom(h, false); // SAFETY: leaked geometry block.
            f.ret_ref(unsafe { std::ptr::addr_of_mut!((*g).center) });
        },
    );
    s.func(r#"const cVector2f& cLux_GetHudVirtualSize()"#, move |f| {
        let g = geom(h, true); // SAFETY: leaked geometry block.
        f.ret_ref(unsafe { std::ptr::addr_of_mut!((*g).size) });
    });
    s.func(r#"const cVector2f& cLux_GetHudVirtualOffset()"#, move |f| {
        let g = geom(h, true); // SAFETY: leaked geometry block.
        f.ret_ref(unsafe { std::ptr::addr_of_mut!((*g).offset) });
    });
    s.func(
        r#"const cVector3f& cLux_GetHudVirtualStartPos()"#,
        move |f| {
            let g = geom(h, true); // SAFETY: leaked geometry block.
            f.ret_ref(unsafe { std::ptr::addr_of_mut!((*g).start) });
        },
    );
    s.func(
        r#"const cVector2f& cLux_GetHudVirtualCenterScreenSize()"#,
        move |f| {
            let g = geom(h, true); // SAFETY: leaked geometry block.
            f.ret_ref(unsafe { std::ptr::addr_of_mut!((*g).size) });
        },
    );
    s.func(
        r#"const cVector3f& cLux_GetHudVirtualCenterScreenStartPos()"#,
        move |f| {
            let g = geom(h, true); // SAFETY: leaked geometry block.
            f.ret_ref(unsafe { std::ptr::addr_of_mut!((*g).start) });
        },
    );
    s.func(
        r#"void cLux_SetImGuiInputFocus(cImGui@ apImGui, bool abShowMouse)"#,
        move |f| {
            // SAFETY: a `cImGui@` handle is a UI of this port.
            unsafe { set_input_focus(h, f.ptr_arg(0).cast(), f.arg(1)) };
        },
    );
    s.func(r#"void cLux_ParseStringIntoScreenText(const tWString &in asInput, cImGui @apImGui, const cLuxScreenTextFormatParameters &aFormatParams, array<tWString> &out aOutLines, array<cLuxScreenTextIcon@> &out aIconArray, array<array<int>> &out aOutIconsPerLine, float &out afMaxLineHeight, float &out afDisplayTime, bool abTriggeredByGamepad)"#, move |f| { parse_screen_text(h, ui(f, 1), ws(f, 0), sd(f, 2), f.ptr_arg(3) as usize, f.ptr_arg(4) as usize, f.ptr_arg(5) as usize, f.rm::<f32>(6), f.rm::<f32>(7)); });
    s.func(r#"void cLux_DrawScreenText(cImGui @apImGui, float afTimeStep, const cImGuiLabelData &in aLabel, const cVector3f &in avPosition, float afLineWidth, float afLineSpacing, const array<tWString> &in avTextLines, const array<cLuxScreenTextIcon@> &in avIcons, const array<array<int>> &in avIconsPerLine)"#, move |f| { draw_screen_text(h, ui(f, 0), f.arg(1), sd(f, 2), v3(f, 3), f.arg(4), f.arg(5), f.ptr_arg(6) as usize, f.ptr_arg(7) as usize, f.ptr_arg(8) as usize, false); });
    s.func(r#"void cLux_DrawHint(cImGui @apImGui, float afTimeStep, const cImGuiLabelData &in aLabel, const cVector3f &in avPosition, float afLineWidth, float afLineSpacing, const array<tWString> &in avTextLines, const array<cLuxScreenTextIcon@> &in avIcons, const array<array<int>> &in avIconsPerLine)"#, move |f| { draw_screen_text(h, ui(f, 0), f.arg(1), sd(f, 2), v3(f, 3), f.arg(4), f.arg(5), f.ptr_arg(6) as usize, f.ptr_arg(7) as usize, f.ptr_arg(8) as usize, true); });
    s.method(
        "cLuxInputHandler",
        r#"void ClearKeyboardLayout()"#,
        move |_f| {
            clear_keyboard_layout();
        },
    );
    s.method("cLuxInputHandler", r#"void AddKeyboardLayoutRange(eKey aFirstKey, eKey aLastKey, eLuxKeyboardLayoutType aType, const cImGuiGfx &in aGfxKey, const cImGuiLabelData &in aLabelKey)"#, move |f| { add_keyboard_layout(f.arg(0), f.arg(1), f.arg(2), sd(f, 3), sd(f, 4)); });
    s.method("cLuxInputHandler", r#"void AddKeyboardLayoutKey(eKey aKey, eLuxKeyboardLayoutType aType, const cImGuiGfx &in aGfxKey, const cImGuiLabelData &in aLabelKey)"#, move |f| { let k = f.arg(0); add_keyboard_layout(k, k, f.arg(1), sd(f, 2), sd(f, 3)); });
    s.method("cLuxInputHandler", r#"void SetKeyboardLayoutDefaults(const cImGuiGfx &in aGfxKey, const cImGuiLabelData &in aLabelKey)"#, move |f| { set_keyboard_defaults(sd(f, 0), sd(f, 1)); });
    s.method(
        "cLuxInputHandler",
        r#"void SetMouseLayout(array<tString> &in avButtons)"#,
        move |f| {
            let a = f.ptr_arg(0) as usize;
            set_mouse_layout(
                (0..h.array_size(a))
                    .map(|i| h.string(h.array_at(a, i)))
                    .collect(),
            );
        },
    );
    s.func(r#"cImGui@ cLux_GetInputFocusImGui()"#, move |f| {
        f.ret_ref(script_input_focus());
    });
    s.func(r#"cImGui@ cLux_GetPrevInputFocusImGui()"#, move |f| {
        f.ret_ref(prev_input_focus());
    });
    s.func(
        r#"void cLux_PreloadGuiGfx(const tString &in asFile, eImGuiGfx aType)"#,
        move |f| {
            preload_gfx(h, bs(f, 0), f.arg(1));
        },
    );
    s.func(r#"cLuxScriptImGui@ cLux_CreateScriptImGui(const tString &in asName, bool abRegisterForDrawing, bool abSkipResetOnRegistration=true)"#, move |f| { f.ret_ref(create_script_imgui(h, bs(f, 0), f.arg(1))); });
    s.method("cLuxScriptImGui", r#"cImGui@ GetImGui()"#, move |f| {
        f.ret_ref(f.me::<ScriptImGui>().im);
    });
    s.method("cLuxScriptImGui", r#"cGuiSet@ GetSet()"#, move |f| {
        // SAFETY: the script object holds a leaked UI.
        f.ret_ref(unsafe { (*f.me::<ScriptImGui>().im).set } as *mut c_void);
    });
    s.method("cLuxScriptImGui", r#"void DrawAll()"#, move |f| {
        // SAFETY: as above.
        unsafe { (*f.me::<ScriptImGui>().im).draw_all(h) };
    });
    s.method(T, r#"void Begin(float afTimeStep)"#, move |f| {
        im(f).begin(f.arg(0));
        set_current(f.this.cast());
    });
    s.method(T, r#"void End()"#, move |f| {
        im(f).end(h);
    });
    s.method(T, r#"void DrawAll()"#, move |f| {
        im(f).draw_all(h);
    });
    s.method(T, r#"void ClearStates()"#, move |f| {
        im(f).clear_states();
    });
    s.method(T, r#"void DestroyAssets()"#, move |_f| {});
    s.method(T, r#"cGuiSet@ GetSet()"#, move |f| {
        f.ret_ref(im(f).set as *mut c_void);
    });
    s.method(T, r#"const tString& GetName()"#, move |f| {
        f.ret_ref(im(f).name_ptr as *mut c_void);
    });
    s.method(T, r#"bool IsFirstRun()"#, move |f| {
        f.ret(im(f).first_run);
    });
    s.method(T, r#"float GetTimeStep()"#, move |f| {
        f.ret(im(f).time_step);
    });
    s.method(T, r#"float GetTimeCount()"#, move |f| {
        f.ret(im(f).time_count);
    });
    s.method(T, r#"void SetShowMouse(bool abX)"#, move |f| {
        im(f).show_mouse = f.arg(0);
    });
    s.method(T, r#"bool GetShowMouse()"#, move |f| {
        f.ret(im(f).show_mouse);
    });
    s.method(T, r#"void SetShowMouseAutomatically(bool abX)"#, move |f| {
        im(f).show_mouse_auto = f.arg(0);
    });
    s.method(T, r#"bool GetShowMouseAutomatically()"#, move |f| {
        f.ret(im(f).show_mouse_auto);
    });
    s.method(
        T,
        r#"void SendMousePosition(const cVector2l&in avPos, const cVector2l&in avRel)"#,
        move |f| {
            im(f).send_mouse(h, *f.rf::<[i32; 2]>(0), *f.rf::<[i32; 2]>(1));
        },
    );
    s.method(
        T,
        r#"void SendMouseVirtualPosition(const cVector2f&in avPos, const cVector2f&in avRel)"#,
        move |f| {
            im(f).send_mouse_virtual(v2(f, 0), v2(f, 1));
        },
    );
    s.method(
        T,
        r#"void SendAction(eImGuiAction aAction, bool abDown, bool abTriggered)"#,
        move |f| {
            im(f).send_action(f.arg(0), f.arg(1), f.arg(2));
        },
    );
    s.method(
        T,
        r#"bool ActionTriggered(eImGuiAction aAction, bool abCheckIfUsed=false)"#,
        move |f| {
            f.ret(im(f).action_triggered(f.arg(0), f.arg(1)));
        },
    );
    s.method(
        T,
        r#"bool ActionIsDown(eImGuiAction aAction, bool abCheckIfUsed=false)"#,
        move |f| {
            f.ret(im(f).action_is_down(f.arg(0), f.arg(1)));
        },
    );
    s.method(T, r#"const cVector2f& GetMouseRel()"#, move |f| {
        f.ret_ref(im(f).mouse_rel.as_mut_ptr());
    });
    s.method(T, r#"const cVector2f& GetMousePosition()"#, move |f| {
        f.ret_ref(im(f).mouse_pos.as_mut_ptr());
    });
    s.method(T, r#"cVector3f GetMousePosition3D()"#, move |f| {
        let m = im(f).mouse_pos;
        f.ret([m[0], m[1], 0.0f32]);
    });
    s.method(T, r#"cVector3f GetMouseRel3D()"#, move |f| {
        let m = im(f).mouse_rel;
        f.ret([m[0], m[1], 0.0f32]);
    });
    s.method(T, r#"bool CheckMouseHasMoved()"#, move |f| {
        let m = im(f).mouse_rel;
        f.ret(m[0] != 0.0 || m[1] != 0.0);
    });
    s.method(T, r#"void SetAlignment(eImGuiAlign aAlign)"#, move |f| {
        im(f).align = f.arg(0);
    });
    s.method(
        T,
        r#"void SetFocus(const tString&in asWidgetName)"#,
        move |f| {
            im(f).set_focus(bs(f, 0));
        },
    );
    s.method(
        T,
        r#"void SetUIMoveGroupFlags(int alGroupFlags)"#,
        move |f| {
            im(f).focus.group_flags = f.arg(0);
        },
    );
    s.method(T, r#"void SetUIMoveWrapMode(eImGuiWrap aWrap)"#, move |f| {
        im(f).focus.wrap_mode = f.arg(0);
    });
    s.method(T, r#"void LockMouseFocus()"#, move |f| {
        im(f).focus.mouse_lock = 2;
    });
    s.method(T, r#"bool MouseFocusIsLocked()"#, move |f| {
        f.ret(im(f).focus.mouse_lock > 0);
    });
    s.method(T, r#"void SetDrawUIDebugBoxes(bool abX)"#, move |_f| {});
    s.method(
        T,
        r#"bool CheckMouseOver(const cVector3f&in avPos, const cVector2f &in avSize)"#,
        move |f| {
            let g = im(f);
            f.ret(g.mouse_over(h, add3(v3(f, 0), g.group_pos()), v2(f, 1)));
        },
    );
    s.method(
        T,
        r#"int GetStateInt(uint64 alId, int aDefault=0)"#,
        move |f| {
            let v = val(f, 1, 0);
            ret_val(f, im(f).get_or_set(f.arg(0), v));
        },
    );
    s.method(
        T,
        r#"int GetStateInt(const tString&in asVarName, int aDefault=0)"#,
        move |f| {
            let v = val(f, 1, 0);
            ret_val(f, im(f).get_or_set(id_of(bs(f, 0)), v));
        },
    );
    s.method(T, r#"void SetStateInt(uint64 alId, int aVal)"#, move |f| {
        let v = val(f, 1, 0);
        im(f).set_val(f.arg(0), v);
    });
    s.method(
        T,
        r#"void SetStateInt(const tString&in asVarName, int aVal)"#,
        move |f| {
            let v = val(f, 1, 0);
            im(f).set_val(id_of(bs(f, 0)), v);
        },
    );
    s.method(T, r#"void IncStateInt(uint64 alId, int aVal)"#, move |f| {
        let v = val(f, 1, 0);
        im(f).inc_val(f.arg(0), v);
    });
    s.method(
        T,
        r#"void IncStateInt(const tString&in asVarName, int aVal)"#,
        move |f| {
            let v = val(f, 1, 0);
            im(f).inc_val(id_of(bs(f, 0)), v);
        },
    );
    s.method(
        T,
        r#"float GetStateFloat(uint64 alId, float aDefault=0.0f)"#,
        move |f| {
            let v = val(f, 1, 1);
            ret_val(f, im(f).get_or_set(f.arg(0), v));
        },
    );
    s.method(
        T,
        r#"float GetStateFloat(const tString&in asVarName, float aDefault=0.0f)"#,
        move |f| {
            let v = val(f, 1, 1);
            ret_val(f, im(f).get_or_set(id_of(bs(f, 0)), v));
        },
    );
    s.method(
        T,
        r#"void SetStateFloat(uint64 alId, float aVal)"#,
        move |f| {
            let v = val(f, 1, 1);
            im(f).set_val(f.arg(0), v);
        },
    );
    s.method(
        T,
        r#"void SetStateFloat(const tString&in asVarName, float aVal)"#,
        move |f| {
            let v = val(f, 1, 1);
            im(f).set_val(id_of(bs(f, 0)), v);
        },
    );
    s.method(
        T,
        r#"void IncStateFloat(uint64 alId, float aVal)"#,
        move |f| {
            let v = val(f, 1, 1);
            im(f).inc_val(f.arg(0), v);
        },
    );
    s.method(
        T,
        r#"void IncStateFloat(const tString&in asVarName, float aVal)"#,
        move |f| {
            let v = val(f, 1, 1);
            im(f).inc_val(id_of(bs(f, 0)), v);
        },
    );
    s.method(
        T,
        r#"cVector3f GetStateVector3f(uint64 alId, const cVector3f&in aDefault=0.0f)"#,
        move |f| {
            let v = val(f, 1, 2);
            ret_val(f, im(f).get_or_set(f.arg(0), v));
        },
    );
    s.method(T, r#"cVector3f GetStateVector3f(const tString&in asVarName, const cVector3f&in aDefault=cVector3f(0.0f))"#, move |f| { let v = val(f, 1, 2); ret_val(f, im(f).get_or_set(id_of(bs(f, 0)), v)); });
    s.method(
        T,
        r#"void SetStateVector3f(uint64 alId, const cVector3f&in aVal)"#,
        move |f| {
            let v = val(f, 1, 2);
            im(f).set_val(f.arg(0), v);
        },
    );
    s.method(
        T,
        r#"void SetStateVector3f(const tString&in asVarName, const cVector3f&in aVal)"#,
        move |f| {
            let v = val(f, 1, 2);
            im(f).set_val(id_of(bs(f, 0)), v);
        },
    );
    s.method(
        T,
        r#"void IncStateVector3f(uint64 alId, const cVector3f&in aVal)"#,
        move |f| {
            let v = val(f, 1, 2);
            im(f).inc_val(f.arg(0), v);
        },
    );
    s.method(
        T,
        r#"void IncStateVector3f(const tString&in asVarName, const cVector3f&in aVal)"#,
        move |f| {
            let v = val(f, 1, 2);
            im(f).inc_val(id_of(bs(f, 0)), v);
        },
    );
    s.method(
        T,
        r#"cColor GetStateColor(uint64 alId, const cColor&in aDefault=cColor(1,1))"#,
        move |f| {
            let v = val(f, 1, 3);
            ret_val(f, im(f).get_or_set(f.arg(0), v));
        },
    );
    s.method(
        T,
        r#"cColor GetStateColor(const tString&in asVarName, const cColor&in aDefault=cColor(1,1))"#,
        move |f| {
            let v = val(f, 1, 3);
            ret_val(f, im(f).get_or_set(id_of(bs(f, 0)), v));
        },
    );
    s.method(
        T,
        r#"void SetStateColor(uint64 alId, const cColor&in aVal)"#,
        move |f| {
            let v = val(f, 1, 3);
            im(f).set_val(f.arg(0), v);
        },
    );
    s.method(
        T,
        r#"void SetStateColor(const tString&in asVarName, const cColor&in aVal)"#,
        move |f| {
            let v = val(f, 1, 3);
            im(f).set_val(id_of(bs(f, 0)), v);
        },
    );
    s.method(
        T,
        r#"void IncStateColor(uint64 alId, const cColor&in aVal)"#,
        move |f| {
            let v = val(f, 1, 3);
            im(f).inc_val(f.arg(0), v);
        },
    );
    s.method(
        T,
        r#"void IncStateColor(const tString&in asVarName, const cColor&in aVal)"#,
        move |f| {
            let v = val(f, 1, 3);
            im(f).inc_val(id_of(bs(f, 0)), v);
        },
    );
    s.method(T, r#"void FadeStateFloat(const tString&in asVarName, float afGoalVal,float afTime, eEasing aType=eEasing_QuadInOut, bool abReplaceIfExist=true)"#, move |f| { im(f).fade(id_of(bs(f, 0)), 0, [f.arg::<f32>(1), 0.0, 0.0, 0.0], f.arg(2), f.arg(3), f.arg(4)); });
    s.method(T, r#"void FadeStateVector3f(const tString&in asVarName, const cVector3f&in avGoalVal,float afTime, eEasing aType=eEasing_QuadInOut, bool abReplaceIfExist=true)"#, move |f| { let g = v3(f, 1); im(f).fade(id_of(bs(f, 0)), 1, [g[0], g[1], g[2], 0.0], f.arg(2), f.arg(3), f.arg(4)); });
    s.method(T, r#"void FadeStateColor(const tString&in asVarName, const cColor&in aGoalVal,float afTime, eEasing aType=eEasing_QuadInOut, bool abReplaceIfExist=true)"#, move |f| { im(f).fade(id_of(bs(f, 0)), 2, col(f, 1), f.arg(2), f.arg(3), f.arg(4)); });
    s.method(
        T,
        r#"void StopFade(const tString&in asVarName)"#,
        move |f| {
            im(f).fades.remove(&id_of(bs(f, 0)));
        },
    );
    s.method(
        T,
        r#"bool FadeOver(const tString&in asVarName)"#,
        move |f| {
            f.ret(
                im(f)
                    .fades
                    .get(&id_of(bs(f, 0)))
                    .is_some_and(|x| x.count >= x.time),
            );
        },
    );
    s.method(
        T,
        r#"bool IsFading(const tString&in asVarName)"#,
        move |f| {
            f.ret(
                im(f)
                    .fades
                    .get(&id_of(bs(f, 0)))
                    .is_some_and(|x| x.count < x.time),
            );
        },
    );
    s.method(T, r#"float FadeOscillateFloat(const tString&in asVarName, float afStart, float afGoal, float afTime, eEasing aType=eEasing_QuadInOut)"#, move |f| { let (a, b) = (f.arg::<f32>(1), f.arg::<f32>(2)); let x = osc(im(f), f.arg(3), f.arg(4)); f.ret((b - a).mul_add(x, a)); });
    s.method(T, r#"cColor FadeOscillateColor(const tString&in asVarName, const cColor&in aStart, const cColor&in aGoal, float afTime, eEasing aType=eEasing_QuadInOut)"#, move |f| { let (a, b) = (col(f, 1), col(f, 2)); let x = osc(im(f), f.arg(3), f.arg(4)); let mut r = [0.0f32; 4]; for ((r, a), b) in r.iter_mut().zip(a).zip(b) { *r = (b - a).mul_add(x, a); } f.ret(r); });
    s.method(T, r#"cVector3f FadeOscillateVector3f(const tString&in asVarName, const cVector3f&in avStart, const cVector3f&in avGoal, float afTime, eEasing aType=eEasing_QuadInOut)"#, move |f| { let (a, b) = (v3(f, 1), v3(f, 2)); let x = osc(im(f), f.arg(3), f.arg(4)); let mut r = [0.0f32; 3]; for ((r, a), b) in r.iter_mut().zip(a).zip(b) { *r = (b - a).mul_add(x, a); } f.ret(r); });
    s.method(
        T,
        r#"void AddTimer(const tString&in asName, float afTime)"#,
        move |f| {
            im(f).timers.push(Timer {
                id: id_of(bs(f, 0)),
                time: f.arg(1),
                repeat: false,
                touched: true,
            });
        },
    );
    s.method(
        T,
        r#"bool RepeatTimer(const tString&in asName, float afTime)"#,
        move |f| {
            let g = im(f);
            let id = id_of(bs(f, 0));
            let found = g.timer(id);
            if let Some(t) = found
                .and_then(|i| g.timers.get_mut(i))
                .filter(|t| t.time > 0.0)
            {
                t.touched = true;
                f.ret(false);
            } else {
                g.timers.push(Timer {
                    id,
                    time: f.arg(1),
                    repeat: true,
                    touched: true,
                });
                f.ret(found.is_some());
            }
        },
    );
    s.method(T, r#"void StopTimer(const tString&in asName)"#, move |f| {
        let id = id_of(bs(f, 0));
        im(f).timers.retain(|t| t.id != id);
    });
    s.method(T, r#"bool TimerOver(const tString&in asName)"#, move |f| {
        let g = im(f);
        f.ret(
            g.timer(id_of(bs(f, 0)))
                .and_then(|i| g.timers.get(i))
                .is_some_and(|t| t.time <= 0.0),
        );
    });
    s.method(
        T,
        r#"bool TimerExists(const tString&in asName)"#,
        move |f| {
            let g = im(f);
            f.ret(g.timer(id_of(bs(f, 0))).is_some());
        },
    );
    s.method(
        T,
        r#"void SetModColorMul(const cColor&in aCol)"#,
        move |f| {
            im(f).mods.color_mul = col(f, 0);
        },
    );
    s.method(
        T,
        r#"void SetModTextColorMul(const cColor&in aCol)"#,
        move |f| {
            im(f).mods.text_color_mul = col(f, 0);
        },
    );
    s.method(T, r#"void SetModUseUIPos(bool abX)"#, move |f| {
        im(f).mods.use_ui_pos = f.arg(0);
    });
    s.method(T, r#"void SetModUseInput(bool abX)"#, move |f| {
        im(f).mods.use_input = f.arg(0);
    });
    s.method(
        T,
        r#"void SetModUISizeHoriExpansion(float afNeg, float afPos)"#,
        move |f| {
            im(f).mods.exp_hori = [f.arg(0), f.arg(1)];
        },
    );
    s.method(
        T,
        r#"void SetModUISizeVertExpansion(float afNeg, float afPos)"#,
        move |f| {
            im(f).mods.exp_vert = [f.arg(0), f.arg(1)];
        },
    );
    s.method(T, r#"void SetModRotateAngle(float afX)"#, move |f| {
        im(f).mods.rotate_angle = f.arg(0);
    });
    s.method(T, r#"void SetModRotateCustomPivot(bool abX)"#, move |f| {
        im(f).mods.rotate_custom_pivot = f.arg(0);
    });
    s.method(
        T,
        r#"void SetModRotatePivot(const cVector2f&in avPivot)"#,
        move |f| {
            im(f).mods.rotate_pivot = v2(f, 0);
        },
    );
    s.method(T, r#"void ResetModifiers()"#, move |f| {
        im(f).mods = Mods::default();
    });
    s.method(T, r#"void PushModifiers()"#, move |f| {
        let g = im(f);
        g.mod_stack.push(g.mods);
    });
    s.method(T, r#"void PopModifiers()"#, move |f| {
        let g = im(f);
        if let Some(m) = g.mod_stack.pop() {
            g.mods = m;
        }
    });
    s.method(T, r#"bool PrevPressed()"#, move |f| {
        f.ret(im(f).prev.pressed);
    });
    s.method(T, r#"bool PrevBecamePressed()"#, move |f| {
        f.ret(im(f).prev.became_pressed);
    });
    s.method(T, r#"bool PrevInFocus()"#, move |f| {
        f.ret(im(f).prev.in_focus);
    });
    s.method(T, r#"bool PrevBecameInFocus()"#, move |f| {
        f.ret(im(f).prev.became_in_focus);
    });
    s.method(T, r#"bool PrevWasInFocus()"#, move |f| {
        f.ret(im(f).prev.was_in_focus);
    });
    s.method(T, r#"bool PrevMouseOver()"#, move |f| {
        f.ret(im(f).prev.mouse_over);
    });
    s.method(T, r#"bool PrevUpdated()"#, move |f| {
        f.ret(im(f).prev.updated);
    });
    s.method(T, r#"const cVector3f& PrevPosition()"#, move |f| {
        f.ret_ref(im(f).prev.pos.as_mut_ptr());
    });
    s.method(T, r#"const cVector2f& PrevSize()"#, move |f| {
        f.ret_ref(im(f).prev.size.as_mut_ptr());
    });
    s.method(T, r#"void ClearPrevData()"#, move |f| {
        im(f).prev = Prev::default();
    });
    s.method(T, r#"void GroupBegin(const cVector3f&in avPos, const cVector2f&in avSize=0, bool abClip=false)"#, move |f| { let g = im(f);
        let size = v2(f, 1);
        let size = if size[0] > 0.0 || size[1] > 0.0 { size } else { g.group_size(h) };
        let pos = add3(g.group_pos(), v3(f, 0));
        g.push_group(Group { pos, size, clip: f.arg(2) }); });
    s.method(T, r#"void GroupEnd()"#, move |f| {
        im(f).pop_group();
    });
    s.method(T, r#"const cVector3f &GetCurrentGroupPos()"#, move |f| {
        let g = im(f);
        g.tmp_v3 = g.group_pos();
        f.ret_ref(g.tmp_v3.as_mut_ptr());
    });
    s.method(T, r#"const cVector2f &GetCurrentGroupSize()"#, move |f| {
        let g = im(f);
        g.tmp_v2 = g.group_size(h);
        f.ret_ref(g.tmp_v2.as_mut_ptr());
    });
    s.method(
        T,
        r#"void ClipAreaBegin(const cVector3f&in avPos, const cVector2f&in avSize)"#,
        move |f| {
            im(f).clip_begin(v3(f, 0), v2(f, 1));
        },
    );
    s.method(T, r#"void ClipAreaEnd()"#, move |f| {
        im(f).clip_end();
    });
    s.method(T, r#"void LayoutBegin(eImGuiLayout aType, const cVector3f&in avPos=0, const cVector2f&in avSize=-1, const cVector2f&in avSpacing=0)"#, move |f| { let g = im(f);
        let start = add3(g.group_pos(), v3(f, 1));
        g.layouts.push(Layout { ty: f.arg(0), start, cursor: start, size: v2(f, 2), spacing: v2(f, 3), line_max: 0.0 }); });
    s.method(T, r#"void LayoutEnd()"#, move |f| {
        im(f).layouts.pop();
    });
    s.method(
        T,
        r#"void AddLayoutHorizontalSpace(float afWidth, float afHeight=0)"#,
        move |f| {
            if let Some(l) = im(f).layouts.last_mut() {
                l.cursor[0] += f.arg::<f32>(0);
            }
        },
    );
    s.method(
        T,
        r#"void AddLayoutVerticalSpace(float afHeight)"#,
        move |f| {
            if let Some(l) = im(f).layouts.last_mut() {
                l.cursor[1] += f.arg::<f32>(0);
            }
        },
    );
    s.method(
        T,
        r#"void AddItemString(const tWString&in asStr)"#,
        move |f| {
            im(f).items.push(ws(f, 0).to_vec());
        },
    );
    s.method(
        T,
        r#"void AddItemStringList(const tWString&in asStrList)"#,
        move |f| {
            let g = im(f);
            g.items.extend(
                crate::string::split(ws(f, 0), Some(&[u32::from(b';')]), true).map(<[u32]>::to_vec),
            );
        },
    );
    s.method(T, r#"void ClearItems()"#, move |f| {
        im(f).items.clear();
    });
    s.method(
        T,
        r#"void SetDefaultButton(const cImGuiButtonData &in aData)"#,
        move |f| {
            im(f).set_default(b"cImGuiButtonData", sd(f, 0));
        },
    );
    s.method(
        T,
        r#"const cImGuiButtonData& GetDefaultButton()"#,
        move |f| {
            f.ret_ref(im(f).get_default(b"cImGuiButtonData").0.cast_mut());
        },
    );
    s.method(
        T,
        r#"void SetDefaultSliderHorizontal(const cImGuiSliderData &in aData)"#,
        move |f| {
            im(f).set_default(b"cImGuiSliderData", sd(f, 0));
        },
    );
    s.method(
        T,
        r#"const cImGuiSliderData& GetDefaultSliderHorizontal()"#,
        move |f| {
            f.ret_ref(im(f).get_default(b"cImGuiSliderData").0.cast_mut());
        },
    );
    s.method(
        T,
        r#"void SetDefaultLabel(const cImGuiLabelData &in aData)"#,
        move |f| {
            im(f).set_default(b"cImGuiLabelData", sd(f, 0));
        },
    );
    s.method(T, r#"const cImGuiLabelData& GetDefaultLabel()"#, move |f| {
        f.ret_ref(im(f).get_default(b"cImGuiLabelData").0.cast_mut());
    });
    s.method(
        T,
        r#"void SetDefaultCheckBox(const cImGuiCheckBoxData &in aData)"#,
        move |f| {
            im(f).set_default(b"cImGuiCheckBoxData", sd(f, 0));
        },
    );
    s.method(
        T,
        r#"const cImGuiCheckBoxData& GetDefaultCheckBox()"#,
        move |f| {
            f.ret_ref(im(f).get_default(b"cImGuiCheckBoxData").0.cast_mut());
        },
    );
    s.method(
        T,
        r#"void SetDefaultTextFrame(const cImGuiTextFrameData &in aData)"#,
        move |f| {
            im(f).set_default(b"cImGuiTextFrameData", sd(f, 0));
        },
    );
    s.method(
        T,
        r#"const cImGuiTextFrameData& GetDefaultTextFrame()"#,
        move |f| {
            f.ret_ref(im(f).get_default(b"cImGuiTextFrameData").0.cast_mut());
        },
    );
    s.method(
        T,
        r#"void SetDefaultMultiSelect(const cImGuiMultiSelectData &in aData)"#,
        move |f| {
            im(f).set_default(b"cImGuiMultiSelectData", sd(f, 0));
        },
    );
    s.method(
        T,
        r#"const cImGuiMultiSelectData& GetDefaultMultiSelect()"#,
        move |f| {
            f.ret_ref(im(f).get_default(b"cImGuiMultiSelectData").0.cast_mut());
        },
    );
    s.method(
        T,
        r#"void SetDefaultFrame(const cImGuiFrameData &in aData)"#,
        move |f| {
            im(f).set_default(b"cImGuiFrameData", sd(f, 0));
        },
    );
    s.method(T, r#"const cImGuiFrameData& GetDefaultFrame()"#, move |f| {
        f.ret_ref(im(f).get_default(b"cImGuiFrameData").0.cast_mut());
    });
    s.method(
        T,
        r#"void SetDefaultWindow(const cImGuiWindowData &in aData)"#,
        move |f| {
            im(f).set_default(b"cImGuiWindowData", sd(f, 0));
        },
    );
    s.method(
        T,
        r#"const cImGuiWindowData& GetDefaultWindow()"#,
        move |f| {
            f.ret_ref(im(f).get_default(b"cImGuiWindowData").0.cast_mut());
        },
    );
    s.method(
        T,
        r#"void SetDefaultGauge(const cImGuiGaugeData &in aData)"#,
        move |f| {
            im(f).set_default(b"cImGuiGaugeData", sd(f, 0));
        },
    );
    s.method(T, r#"const cImGuiGaugeData& GetDefaultGauge()"#, move |f| {
        f.ret_ref(im(f).get_default(b"cImGuiGaugeData").0.cast_mut());
    });
    s.method(
        T,
        r#"void SetDefaultSliderVertical(const cImGuiSliderData &in aData)"#,
        move |f| {
            im(f).set_default(b"cImGuiSliderDataV", sd(f, 0));
        },
    );
    s.method(
        T,
        r#"const cImGuiSliderData& GetDefaultSliderVertical()"#,
        move |f| {
            f.ret_ref(im(f).get_default(b"cImGuiSliderDataV").0.cast_mut());
        },
    );
    s.method(
        T,
        r#"void SetDefaultMouse(const cImGuiGfx&in aGfx)"#,
        move |f| {
            let d = im(f).get_default(b"__mouse");
            sd(f, 0).copy_to(d, 0, off::GFX_SIZE);
        },
    );
    s.method(
        T,
        r#"void SetDefaultFont(const cImGuiFont&in aFont)"#,
        move |f| {
            let g = im(f);
            let font = sd(f, 0);
            let d = g.get_default(b"__font");
            font.copy_to(d, 0, 64);
            for ty in [
                &b"cImGuiButtonData"[..],
                b"cImGuiSliderData",
                b"cImGuiSliderDataV",
                b"cImGuiLabelData",
                b"cImGuiCheckBoxData",
                b"cImGuiTextFrameData",
                b"cImGuiMultiSelectData",
                b"cImGuiFrameData",
                b"cImGuiWindowData",
                b"cImGuiGaugeData",
            ] {
                let t = g.get_default(ty);
                if t.at(off::W_FONT).string(h, off::FONT_FILE).is_empty() {
                    font.copy_to(t.at(off::W_FONT), 16, 48);
                }
            }
        },
    );
    s.method(T, r#"bool DoButton(const tString&in asName,const tWString&in asText, const cImGuiButtonData &in aData, const cVector3f&in avPos=0, const cVector2f&in avSize=-1)"#, move |f| { let g = im(f); f.ret(g.do_button(h, bs(f, 0), ws(f, 1), sd(f, 2), v3(f, 3), v2(f, 4), 0)); });
    s.method(T, r#"bool DoButton(const tString&in asName,const tWString&in asText, const cVector3f&in avPos=0, const cVector2f&in avSize=-1)"#, move |f| { let g = im(f); let d = g.get_default(b"cImGuiButtonData"); f.ret(g.do_button(h, bs(f, 0), ws(f, 1), d, v3(f, 2), v2(f, 3), 0)); });
    s.method(T, r#"bool DoRepeatButton(const tString&in asName,const tWString&in asText, const cImGuiButtonData &in aData, const cVector3f&in avPos=0, const cVector2f&in avSize=-1)"#, move |f| { let g = im(f); f.ret(g.do_button(h, bs(f, 0), ws(f, 1), sd(f, 2), v3(f, 3), v2(f, 4), 2)); });
    s.method(T, r#"bool DoRepeatButton(const tString&in asName,const tWString&in asText, const cVector3f&in avPos=0, const cVector2f&in avSize=-1)"#, move |f| { let g = im(f); let d = g.get_default(b"cImGuiButtonData"); f.ret(g.do_button(h, bs(f, 0), ws(f, 1), d, v3(f, 2), v2(f, 3), 2)); });
    s.method(T, r#"bool DoToggleButton(const tString&in asName,const tWString&in asText, bool abDefaultChecked, const cImGuiButtonData &in aData, const cVector3f&in avPos=0, const cVector2f&in avSize=-1)"#, move |f| { let g = im(f); super::imgui::default_or_current_int(g.state(id_of(bs(f, 0))), i32::from(f.arg::<bool>(2))); f.ret(g.do_button(h, bs(f, 0), ws(f, 1), sd(f, 3), v3(f, 4), v2(f, 5), 1)); });
    s.method(T, r#"bool DoToggleButton(const tString&in asName,const tWString&in asText, bool abDefaultChecked, const cVector3f&in avPos=0, const cVector2f&in avSize=-1)"#, move |f| { let g = im(f); super::imgui::default_or_current_int(g.state(id_of(bs(f, 0))), i32::from(f.arg::<bool>(2))); let d = g.get_default(b"cImGuiButtonData"); f.ret(g.do_button(h, bs(f, 0), ws(f, 1), d, v3(f, 3), v2(f, 4), 1)); });
    s.method(T, r#"float DoSliderHorizontal(const tString&in asName, float afDefaultValue, float afMin, float afMax, float afStepSize, const cImGuiSliderData &in aData, const cVector3f&in avPos=0, const cVector2f&in avSize=-1)"#, move |f| { let g = im(f); f.ret(g.do_slider(h, bs(f, 0), f.arg(1), f.arg(2), f.arg(3), f.arg(4), sd(f, 5), v3(f, 6), v2(f, 7), false)); });
    s.method(T, r#"float DoSliderHorizontal(const tString&in asName, float afDefaultValue, float afMin, float afMax, float afStepSize=-1, const cVector3f&in avPos=0, const cVector2f&in avSize=-1)"#, move |f| { let g = im(f); let d = g.get_default(b"cImGuiSliderData"); f.ret(g.do_slider(h, bs(f, 0), f.arg(1), f.arg(2), f.arg(3), f.arg(4), d, v3(f, 5), v2(f, 6), false)); });
    s.method(T, r#"float DoSliderVertical(const tString&in asName, float afDefaultValue, float afMin, float afMax, float afStepSize, const cImGuiSliderData &in aData, const cVector3f&in avPos=0, const cVector2f&in avSize=-1)"#, move |f| { let g = im(f); f.ret(g.do_slider(h, bs(f, 0), f.arg(1), f.arg(2), f.arg(3), f.arg(4), sd(f, 5), v3(f, 6), v2(f, 7), true)); });
    s.method(T, r#"float DoSliderVertical(const tString&in asName, float afDefaultValue, float afMin, float afMax, float afStepSize=-1, const cVector3f&in avPos=0, const cVector2f&in avSize=-1)"#, move |f| { let g = im(f); let d = g.get_default(b"cImGuiSliderDataV"); f.ret(g.do_slider(h, bs(f, 0), f.arg(1), f.arg(2), f.arg(3), f.arg(4), d, v3(f, 5), v2(f, 6), true)); });
    s.method(T, r#"void DoLabel(const tWString&in asText, const cImGuiLabelData &in aData, const cVector3f&in avPos=0, const cVector2f&in avSize=-1, float afFontSizeMul=1)"#, move |f| { im(f).do_label(h, ws(f, 0), sd(f, 1), v3(f, 2), v2(f, 3), f.arg(4)); });
    s.method(T, r#"void DoLabel(const tWString&in asText, const cVector3f&in avPos=0, const cVector2f&in avSize=-1, float afFontSizeMul=1)"#, move |f| { let g = im(f); let d = g.get_default(b"cImGuiLabelData"); g.do_label(h, ws(f, 0), d, v3(f, 1), v2(f, 2), f.arg(3)); });
    s.method(T, r#"void DoImage(const cImGuiGfx&in aGfxImage, const cVector3f&in avPos=0, const cVector2f&in avSize=-1)"#, move |f| { im(f).do_image(h, sd(f, 0), v3(f, 1), v2(f, 2)); });
    s.method(T, r#"bool DoCheckBox(const tString&in asName, const tWString&in asText, bool abDefaultChecked, const cImGuiCheckBoxData &in aData, const cVector3f&in avPos=0, const cVector2f&in avSize=-1)"#, move |f| { let g = im(f); f.ret(g.do_checkbox(h, bs(f, 0), ws(f, 1), f.arg(2), sd(f, 3), v3(f, 4), v2(f, 5))); });
    s.method(T, r#"bool DoCheckBox(const tString&in asName, const tWString&in asText, bool abDefaultChecked, const cVector3f&in avPos=0, const cVector2f&in avSize=-1)"#, move |f| { let g = im(f); let d = g.get_default(b"cImGuiCheckBoxData"); f.ret(g.do_checkbox(h, bs(f, 0), ws(f, 1), f.arg(2), d, v3(f, 3), v2(f, 4))); });
    s.method(T, r#"float DoTextFrame(const tWString&in asText,const cVector2f&in avEdgeSpacing, float afRowSpace, float afStartRowOffset, const cImGuiTextFrameData &in aData, const cVector3f&in avPos, const cVector2f&in avSize)"#, move |f| { let g = im(f); f.ret(g.do_text_frame(h, ws(f, 0), v2(f, 1), f.arg(2), f.arg(3), sd(f, 4), v3(f, 5), v2(f, 6))); });
    s.method(T, r#"float DoTextFrame(const tWString&in asText,const cVector2f&in avEdgeSpacing, float afRowSpace, float afStartRowOffset, const cVector3f&in avPos, const cVector2f&in avSize)"#, move |f| { let g = im(f); let d = g.get_default(b"cImGuiTextFrameData"); f.ret(g.do_text_frame(h, ws(f, 0), v2(f, 1), f.arg(2), f.arg(3), d, v3(f, 4), v2(f, 5))); });
    s.method(T, r#"int DoMultiSelect(const tString&in asName, int alDefaultSelectedItem, const cImGuiMultiSelectData &in aData, const cVector3f&in avPos=0, const cVector2f&in avSize=-1)"#, move |f| { let g = im(f); f.ret(g.do_multi_select(h, bs(f, 0), f.arg(1), sd(f, 2), v3(f, 3), v2(f, 4))); });
    s.method(T, r#"int DoMultiSelect(const tString&in asName, int alDefaultSelectedItem, const cVector3f&in avPos=0, const cVector2f&in avSize=-1)"#, move |f| { let g = im(f); let d = g.get_default(b"cImGuiMultiSelectData"); f.ret(g.do_multi_select(h, bs(f, 0), f.arg(1), d, v3(f, 2), v2(f, 3))); });
    s.method(T, r#"int DoMultiToggle(const tString&in asName, int alDefaultSelectedItem, uint alColumnNum, const cVector2f&in avSpacing, const cImGuiButtonData &in aData, const cVector3f&in avPos, const cVector2f&in avSize)"#, move |f| { let g = im(f); f.ret(g.do_multi_toggle(h, bs(f, 0), f.arg(1), f.arg(2), v2(f, 3), sd(f, 4), v3(f, 5), v2(f, 6))); });
    s.method(T, r#"int DoMultiToggle(const tString&in asName, int alDefaultSelectedItem, uint alColumnNum, const cVector2f&in avSpacing, const cVector3f&in avPos, const cVector2f&in avSize)"#, move |f| { let g = im(f); let d = g.get_default(b"cImGuiButtonData"); f.ret(g.do_multi_toggle(h, bs(f, 0), f.arg(1), f.arg(2), v2(f, 3), d, v3(f, 4), v2(f, 5))); });
    s.method(T, r#"void DoFrame(const cImGuiFrameData &in aData, const cVector3f &in avPos=0, const cVector2f &in avSize=-1)"#, move |f| { im(f).do_frame(h, sd(f, 0), v3(f, 1), v2(f, 2)); });
    s.method(
        T,
        r#"void DoFrame(const cVector3f &in avPos=0, const cVector2f &in avSize=-1)"#,
        move |f| {
            let g = im(f);
            let d = g.get_default(b"cImGuiFrameData");
            g.do_frame(h, d, v3(f, 0), v2(f, 1));
        },
    );
    s.method(T, r#"void DoWindowStart(const tWString &in asCaption, const cImGuiWindowData &in aData, const cVector3f &in avPos=0, const cVector2f &in avSize=-1, bool abClip=true)"#, move |f| { im(f).do_window_start(h, ws(f, 0), sd(f, 1), v3(f, 2), v2(f, 3), f.arg(4)); });
    s.method(T, r#"void DoWindowStart(const tWString &in asCaption, const cVector3f &in avPos=0, const cVector2f &in avSize=-1, bool abClip=true)"#, move |f| { let g = im(f); let d = g.get_default(b"cImGuiWindowData"); g.do_window_start(h, ws(f, 0), d, v3(f, 1), v2(f, 2), f.arg(3)); });
    s.method(T, r#"void DoWindowEnd()"#, move |f| {
        im(f).pop_group();
    });
    s.method(T, r#"void DoGauge(const cImGuiGaugeData &in aData, float afFillAmount, const cVector3f &in avPos=0, const cVector2f &in avSize=-1)"#, move |f| { im(f).do_gauge(h, sd(f, 0), f.arg(1), v3(f, 2), v2(f, 3)); });
    s.method(T, r#"void DoGauge(float afFillAmount, const cVector3f &in avPos=0, const cVector2f &in avSize=-1)"#, move |f| { let g = im(f); let d = g.get_default(b"cImGuiGaugeData"); g.do_gauge(h, d, f.arg(0), v3(f, 1), v2(f, 2)); });
    s.method(T, r#"void DoMouse(const cImGuiGfx &in aGfx, const cVector3f&in avOffset=0, const cVector2f&in avSize=-1)"#, move |f| { im(f).do_mouse(h, sd(f, 0), v3(f, 1), v2(f, 2)); });
    s.method(T, r#"void DrawGfx(const cImGuiGfx &in aGfx, const cVector3f&in avPos, const cVector2f&in avSize=-1, const cColor&in aCol=cColor(1,1), const cColor&in aColTopLeft=cColor(1,1), const cColor&in aColTopRight=cColor(1,1), const cColor&in aColBotRight=cColor(1,1), const cColor&in aColBotLeft=cColor(1,1))"#, move |f| { im(f).draw_gfx(h, sd(f, 0), v3(f, 1), v2(f, 2), col(f, 3)); });
    s.method(T, r#"void DrawLine(const cVector2f&in avStart, const cVector2f&in avEnd,float afZ, float afThickness=1.0f, const cColor&in aCol=cColor(1,1), const cImGuiGfx &in aGfx=cImGuiGfx())"#, move |f| { im(f).draw_line(h, v2(f, 0), v2(f, 1), f.arg(2), f.arg(3), col(f, 4), sd(f, 5)); });
    s.method(
        T,
        r#"void AddLineStripVertex(const cVector2f &in avVertex)"#,
        move |f| {
            im(f).line_strip.push(v2(f, 0));
        },
    );
    s.method(T, r#"void DrawAndClearLineStrip(float afZ, float afThickness, const cColor &in aCol=cColor_White, const cImGuiGfx &in aGfx=cImGuiGfx())"#, move |f| { let g = im(f);
        let strip = std::mem::take(&mut g.line_strip);
        for w in strip.windows(2) {
            if let [a, b] = w {
                g.draw_line(h, *a, *b, f.arg(0), f.arg(1), col(f, 2), sd(f, 3));
            }
        } });
    s.method(T, r#"void DrawAlignedGfx(const cImGuiGfx &in aGfx, const cVector3f &in avPos, eImGuiAlign aAlignment, const cVector2f&in avSize=-1, const cColor &in aCol=cColor(1,1), const cColor&in aColTopLeft=cColor(1,1), const cColor&in aColTopRight=cColor(1,1), const cColor&in aColBotRight=cColor(1,1), const cColor&in aColBotLeft=cColor(1,1))"#, move |f| { let g = im(f);
        let gfx = sd(f, 0);
        let size = v2(f, 3);
        let size = if size[0] < 0.0 || size[1] < 0.0 { g.gfx_size(h, gfx) } else { size };
        let pos = g.align_pos(v3(f, 1), size, f.arg(2));
        g.draw_gfx(h, gfx, pos, size, col(f, 4)); });
    s.method(T, r#"void DrawFont(const tWString&in asText, const cImGuiFont &in aFont, const cVector3f&in avPos, eFontAlign aAlign, const cVector2f&in avSizeMul=1, const cColor&in aColMul=cColor(1,1))"#, move |f| { im(f).draw_font(h, ws(f, 0), sd(f, 1), v3(f, 2), f.arg(3), v2(f, 4), col(f, 5)); });
    s.method(T, r#"void DrawFrame(const cImGuiFrameGfx &in aGfx, const cVector3f&in avPos, const cVector2f&in avSize=-1, const cColor&in aCol=cColor(1,1))"#, move |f| { im(f).draw_frame(h, sd(f, 0), v3(f, 1), v2(f, 2), col(f, 3)); });
    s.method(
        T,
        r#"cVector2f GetGfxSize(const cImGuiGfx&in aGfx)"#,
        move |f| {
            let g = im(f);
            f.ret(g.gfx_size(h, sd(f, 0)));
        },
    );
    s.method(
        T,
        r#"cVector2f GetUsedGfxSize(const cImGuiGfx&in aGfx, const cVector2f&in avCustomSize)"#,
        move |f| {
            let g = im(f);
            let s = v2(f, 1);
            f.ret(if s[0] < 0.0 || s[1] < 0.0 {
                g.gfx_size(h, sd(f, 0))
            } else {
                s
            });
        },
    );
    s.method(T, r#"float GetFontLength(const cImGuiFont&in aFont, float afSizeMul, const tWString&in asText)"#, move |f| { let g = im(f); f.ret(g.font_length(h, sd(f, 0), f.arg(1), ws(f, 2))); });
    s.method(
        T,
        r#"const cVector2f& GetUsedFontSize(const cImGuiFont&in aFont)"#,
        move |f| {
            let g = im(f);
            g.tmp_font = g.font_size(sd(f, 0), 1.0);
            f.ret_ref(g.tmp_font.as_mut_ptr());
        },
    );
    s.method(T, r#"cVector2f CalcWidgetSize(const cVector2f&in avArgSize, const cVector2f&in avDefaultSize)"#, move |f| { let ([a0, a1], [d0, d1]) = (v2(f, 0), v2(f, 1));
        f.ret([if a0 < 0.0 { d0 } else { a0 }, if a1 < 0.0 { d1 } else { a1 }]); });
    s.method(T, r#"void GetFontWordWrapRows(const cImGuiFont&in aFont, float afSizeMul, const tWString&in asText, float afLineWidth, array<tWString> &out avLines, array<bool> &out avRowEndedWithNewLine=void)"#, move |f| { let g = im(f);
        let font = sd(f, 0);
        let fd = g.font(h, font);
        let size = g.font_size(font, f.arg(1));
        let rows = if fd != 0 { h.font_wrap(fd, f.arg(3), size[1], size, ws(f, 2)) } else { Vec::new() };
        let (lines, ends) = (f.ptr_arg(4) as usize, f.ptr_arg(5) as usize);
        for r in &rows {
            h.array_push_wstring(lines, r);
            if ends != 0 {
                h.array_push_bool(ends, false);
            }
        } });
}
