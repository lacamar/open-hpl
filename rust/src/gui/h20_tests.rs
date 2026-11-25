mod list_direct {
    use super::super::gw;
    use super::super::gw_h20::{self, KIND_LIST, P};
    use super::super::gw_items as gi;
    use super::super::h20_list as l;

    #[test]
    fn multi_select_toggle_and_clear() {
        let mut shell = Box::new([0usize; 2]);
        let w = shell.as_ptr() as usize;
        shell[1] = gw::new_gw(w, 0, std::ptr::null_mut(), std::ptr::null_mut()) as usize;
        gw_h20::set_kind(w, KIND_LIST, 0, 0);
        for _ in 0..6 {
            gi::add(w, &[]);
        }
        let Some(P::List(p)) = gw_h20::ptr(w) else {
            panic!("list state missing");
        };
        unsafe {
            (*p).list.allow_multi = true;
            l::set_selected_item(p, 2, false, false, 1, true);
            assert_eq!(l::selected_item(p), 2);
            l::set_selected_item(p, 4, false, false, 1, false);
            assert_eq!(l::multi_selection_num(p), 2);
            assert_eq!(l::multi_selection(p, 1), 4);
            l::set_selected_item(p, 2, false, false, 1, false);
            assert_eq!(l::multi_selection_num(p), 1);
            l::clear_selection(p);
            assert_eq!(l::multi_selection_num(p), 0);
            assert_eq!(l::selected_item(p), -1);
        }
        for i in 0..6 {
            assert!(!gi::ohpl_gwi_selected(gi::get(w, i)));
        }
    }
}
