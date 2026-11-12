//! libstdc++ `std::sort` (introsort, insertion sort up to 16 elements, heap sort fallback), so
//! equal keys end up in the order the C++ binary produces.

#![expect(
    clippy::indexing_slicing,
    clippy::integer_division_remainder_used,
    reason = "every index is below the slice length by the algorithm's loop bounds"
)]

pub fn sort<T: Copy>(v: &mut [T], less: &impl Fn(T, T) -> bool) {
    let n = v.len();
    if n == 0 {
        return;
    }
    let depth = 2 * (usize::BITS - 1 - n.leading_zeros());
    introsort_loop(v, 0, n, depth, less);
    if n > 16 {
        insertion_sort(v, 0, 16, less);
        for i in 16..n {
            unguarded_linear_insert(v, i, less);
        }
    } else {
        insertion_sort(v, 0, n, less);
    }
}

fn introsort_loop<T: Copy>(
    v: &mut [T],
    first: usize,
    mut last: usize,
    mut depth: u32,
    less: &impl Fn(T, T) -> bool,
) {
    while last - first > 16 {
        if depth == 0 {
            heap_sort(v, first, last, less);
            return;
        }
        depth -= 1;
        let mid = first + (last - first) / 2;
        median_to_first(v, first, first + 1, mid, last - 1, less);
        let cut = partition(v, first + 1, last, first, less);
        introsort_loop(v, cut, last, depth, less);
        last = cut;
    }
}

fn median_to_first<T: Copy>(
    v: &mut [T],
    result: usize,
    a: usize,
    b: usize,
    c: usize,
    less: &impl Fn(T, T) -> bool,
) {
    let pick = if less(v[a], v[b]) {
        if less(v[b], v[c]) {
            b
        } else if less(v[a], v[c]) {
            c
        } else {
            a
        }
    } else if less(v[a], v[c]) {
        a
    } else if less(v[b], v[c]) {
        c
    } else {
        b
    };
    v.swap(result, pick);
}

fn partition<T: Copy>(
    v: &mut [T],
    mut first: usize,
    mut last: usize,
    pivot: usize,
    less: &impl Fn(T, T) -> bool,
) -> usize {
    loop {
        while less(v[first], v[pivot]) {
            first += 1;
        }
        last -= 1;
        while less(v[pivot], v[last]) {
            last -= 1;
        }
        if first >= last {
            return first;
        }
        v.swap(first, last);
        first += 1;
    }
}

fn insertion_sort<T: Copy>(v: &mut [T], first: usize, last: usize, less: &impl Fn(T, T) -> bool) {
    for i in first + 1..last {
        if less(v[i], v[first]) {
            v[first..=i].rotate_right(1);
        } else {
            unguarded_linear_insert(v, i, less);
        }
    }
}

fn unguarded_linear_insert<T: Copy>(v: &mut [T], mut last: usize, less: &impl Fn(T, T) -> bool) {
    let val = v[last];
    while last > 0 && less(val, v[last - 1]) {
        v[last] = v[last - 1];
        last -= 1;
    }
    v[last] = val;
}

fn heap_sort<T: Copy>(v: &mut [T], first: usize, last: usize, less: &impl Fn(T, T) -> bool) {
    let len = last - first;
    if len >= 2 {
        let mut parent = (len - 2) / 2;
        loop {
            let value = v[first + parent];
            adjust_heap(v, first, parent, len, value, less);
            if parent == 0 {
                break;
            }
            parent -= 1;
        }
    }
    let mut end = last;
    while end - first > 1 {
        end -= 1;
        let value = v[end];
        v[end] = v[first];
        adjust_heap(v, first, 0, end - first, value, less);
    }
}

fn adjust_heap<T: Copy>(
    v: &mut [T],
    first: usize,
    mut hole: usize,
    len: usize,
    value: T,
    less: &impl Fn(T, T) -> bool,
) {
    let top = hole;
    let mut child = hole;
    while child < len.saturating_sub(1) / 2 {
        child = 2 * (child + 1);
        if less(v[first + child], v[first + child - 1]) {
            child -= 1;
        }
        v[first + hole] = v[first + child];
        hole = child;
    }
    if len.is_multiple_of(2) && child == (len - 2) / 2 {
        child = 2 * (child + 1);
        v[first + hole] = v[first + child - 1];
        hole = child - 1;
    }
    while hole > top {
        let parent = (hole - 1) / 2;
        if !less(v[first + parent], value) {
            break;
        }
        v[first + hole] = v[first + parent];
        hole = parent;
    }
    v[first + hole] = value;
}
