use crate::graphics::stdsort;

pub fn std_sort(v: &mut [usize], mut less: impl FnMut(usize, usize) -> bool) {
    stdsort::sort(v, |&a, &b| less(a, b));
}
