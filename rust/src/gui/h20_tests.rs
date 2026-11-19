//! H20 list-box host mock tests.

mod list_mock {
    use super::super::h19_widget::Base;
    use super::super::h20_host::{self as hh, Args, HostVt, Hx};
    use super::super::h20_list::{self as l, ListBase};
    use std::cell::RefCell;
    use std::ffi::c_void;

    struct Mock {
        selected: Vec<bool>,
        slider: Vec<i32>,
        lb: *mut ListBase,
    }

    thread_local! {
        static M: RefCell<Option<Mock>> = const { RefCell::new(None) };
    }

    extern "C" fn op(_: *mut c_void, code: i32, a: *mut Args) -> usize {
        // SAFETY: the test passes valid argument blocks.
        let a = unsafe { &mut *a };
        let mut ret = 0;
        let mut set_item = None;
        M.with(|m| {
            let mut m = m.borrow_mut();
            let Some(m) = m.as_mut() else { return };
            match code {
                hh::ITEM_NUM => a.ri[0] = m.selected.len() as i32,
                hh::ITEM_GET => {
                    ret = usize::try_from(a.i[0])
                        .ok()
                        .filter(|i| *i < m.selected.len())
                        .map_or(0, |i| i + 1);
                }
                hh::ITEM_IS_SELECTED => {
                    ret = usize::from(m.selected.get(a.p[0] - 1).copied().unwrap_or(false))
                }
                hh::ITEM_IS_SELECTABLE => ret = 1,
                hh::ITEM_INDEX => a.ri[0] = a.p[0] as i32 - 1,
                hh::ITEM_SET_SELECTED => {
                    let (i, v) = (a.p[0] - 1, a.i[0] != 0);
                    if m.selected.get(i).copied() != Some(v) {
                        if let Some(s) = m.selected.get_mut(i) {
                            *s = v;
                        }
                        set_item = Some((a.p[0], v, m.lb));
                    }
                }
                hh::SLIDER_SET_VALUE => m.slider.push(a.i[0]),
                hh::GET_FONT_SIZE => a.rf[..2].copy_from_slice(&[12.0, 12.0]),
                _ => {}
            }
        });
        if let Some((item, v, lb)) = set_item {
            let vt = HostVt {
                ctx: std::ptr::null_mut(),
                op,
            };
            // SAFETY: `lb` outlives the test; the table is valid for the call.
            unsafe {
                let hx = Hx::new(&vt);
                if v {
                    l::add_item_to_selection(hx, lb, item);
                } else {
                    l::remove_item_from_selection(hx, lb, item);
                }
            }
        }
        ret
    }

    #[test]
    fn multi_select_toggle_and_clear() {
        let vt = HostVt {
            ctx: std::ptr::null_mut(),
            op,
        };
        let mut base = std::mem::MaybeUninit::<Base>::uninit();
        // SAFETY: `init` writes the base whole.
        unsafe { (*base.as_mut_ptr()).init() };
        let mut lb = std::mem::MaybeUninit::<ListBase>::uninit();
        // SAFETY: the mock host answers every op the list makes; `lb` lives for the test.
        unsafe {
            let hx = Hx::new(&vt);
            l::init(hx, lb.as_mut_ptr(), 1, 2, base.as_mut_ptr(), l::KIND_LIST);
            M.with(|m| {
                *m.borrow_mut() = Some(Mock {
                    selected: vec![false; 6],
                    slider: Vec::new(),
                    lb: lb.as_mut_ptr(),
                });
            });
            let p = lb.as_mut_ptr();
            (*p).list.allow_multi = true;
            l::set_selected_item(hx, p, 2, false, false, 1, true);
            assert_eq!(l::selected_item(p), 2);
            l::set_selected_item(hx, p, 4, false, false, 1, false);
            assert_eq!(l::multi_selection_num(p), 2);
            assert_eq!(l::multi_selection(p, 1), 4);
            l::set_selected_item(hx, p, 2, false, false, 1, false);
            assert_eq!(l::multi_selection_num(p), 1);
            l::clear_selection(hx, p);
            assert_eq!(l::multi_selection_num(p), 0);
            assert_eq!(l::selected_item(p), -1);
            M.with(|m| {
                assert!(
                    m.borrow()
                        .as_ref()
                        .is_some_and(|m| m.selected.iter().all(|s| !s))
                )
            });
            l::free(p);
        }
    }
}
