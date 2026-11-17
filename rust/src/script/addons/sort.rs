//! libstdc++'s `std::sort` (introsort, insertion sort below 16, heap sort at the depth limit), step for step:
//! object arrays were sorted with it, so the order of equal elements and the sequence of comparisons
//! are part of the script-visible behaviour. Scans that libstdc++ leaves unguarded stop at the range
//! ends here instead of reading out of bounds when the comparison is inconsistent.

const THRESHOLD: usize = 16;

struct Seq<'a, F> {
    v: &'a mut [usize],
    less: F,
}

impl<F: FnMut(usize, usize) -> bool> Seq<'_, F> {
    fn g(&self, i: usize) -> usize {
        self.v.get(i).copied().unwrap_or(0)
    }

    fn set(&mut self, i: usize, x: usize) {
        if let Some(s) = self.v.get_mut(i) {
            *s = x;
        }
    }

    fn swap(&mut self, a: usize, b: usize) {
        let (x, y) = (self.g(a), self.g(b));
        self.set(a, y);
        self.set(b, x);
    }

    fn lt(&mut self, a: usize, b: usize) -> bool {
        let (x, y) = (self.g(a), self.g(b));
        (self.less)(x, y)
    }

    fn lt_val(&mut self, x: usize, b: usize) -> bool {
        let y = self.g(b);
        (self.less)(x, y)
    }

    fn val_lt(&mut self, a: usize, y: usize) -> bool {
        let x = self.g(a);
        (self.less)(x, y)
    }

    fn move_median_to_first(&mut self, result: usize, a: usize, b: usize, c: usize) {
        if self.lt(a, b) {
            if self.lt(b, c) {
                self.swap(result, b);
            } else if self.lt(a, c) {
                self.swap(result, c);
            } else {
                self.swap(result, a);
            }
        } else if self.lt(a, c) {
            self.swap(result, a);
        } else if self.lt(b, c) {
            self.swap(result, c);
        } else {
            self.swap(result, b);
        }
    }

    fn unguarded_partition(&mut self, mut first: usize, mut last: usize, pivot: usize) -> usize {
        let end = last;
        loop {
            while first < end && self.lt(first, pivot) {
                first += 1;
            }
            last = last.saturating_sub(1);
            while last > pivot && self.lt(pivot, last) {
                last -= 1;
            }
            if first >= last {
                return first;
            }
            self.swap(first, last);
            first += 1;
        }
    }

    fn unguarded_linear_insert(&mut self, mut last: usize, floor: usize) {
        let val = self.g(last);
        while last > floor {
            let next = last - 1;
            if !self.lt_val(val, next) {
                break;
            }
            let n = self.g(next);
            self.set(last, n);
            last = next;
        }
        self.set(last, val);
    }

    fn insertion_sort(&mut self, first: usize, last: usize) {
        let mut i = first + 1;
        while i < last {
            if self.lt(i, first) {
                let val = self.g(i);
                let mut k = i;
                while k > first {
                    let p = self.g(k - 1);
                    self.set(k, p);
                    k -= 1;
                }
                self.set(first, val);
            } else {
                self.unguarded_linear_insert(i, first);
            }
            i += 1;
        }
    }

    fn adjust_heap(&mut self, first: usize, mut hole: usize, len: usize, value: usize) {
        let top = hole;
        let mut second = hole;
        while second < len.saturating_sub(1) >> 1 {
            second = 2 * (second + 1);
            if self.lt(first + second, first + second - 1) {
                second -= 1;
            }
            let c = self.g(first + second);
            self.set(first + hole, c);
            hole = second;
        }
        if len & 1 == 0 && second == len.saturating_sub(2) >> 1 {
            second = 2 * (second + 1);
            let c = self.g(first + second - 1);
            self.set(first + hole, c);
            hole = second - 1;
        }
        while hole > top {
            let parent = (hole - 1) >> 1;
            if !self.val_lt(first + parent, value) {
                break;
            }
            let p = self.g(first + parent);
            self.set(first + hole, p);
            hole = parent;
        }
        self.set(first + hole, value);
    }

    fn make_heap(&mut self, first: usize, last: usize) {
        if last - first < 2 {
            return;
        }
        let len = last - first;
        let mut parent = (len - 2) >> 1;
        loop {
            let value = self.g(first + parent);
            self.adjust_heap(first, parent, len, value);
            if parent == 0 {
                return;
            }
            parent -= 1;
        }
    }

    fn pop_heap(&mut self, first: usize, last: usize, result: usize) {
        let value = self.g(result);
        let f = self.g(first);
        self.set(result, f);
        self.adjust_heap(first, 0, last - first, value);
    }

    fn partial_sort(&mut self, first: usize, last: usize) {
        self.make_heap(first, last);
        let mut hi = last;
        while hi - first > 1 {
            hi -= 1;
            self.pop_heap(first, hi, hi);
        }
    }

    fn introsort_loop(&mut self, first: usize, mut last: usize, mut depth: u32) {
        while last - first > THRESHOLD {
            if depth == 0 {
                self.partial_sort(first, last);
                return;
            }
            depth -= 1;
            let mid = first + ((last - first) >> 1);
            self.move_median_to_first(first, first + 1, mid, last - 1);
            let cut = self.unguarded_partition(first + 1, last, first);
            self.introsort_loop(cut, last, depth);
            last = cut;
        }
    }
}

pub fn std_sort(v: &mut [usize], less: impl FnMut(usize, usize) -> bool) {
    let n = v.len();
    if n == 0 {
        return;
    }
    let mut s = Seq { v, less };
    s.introsort_loop(0, n, (usize::BITS - 1 - n.leading_zeros()) * 2);
    if n > THRESHOLD {
        s.insertion_sort(0, THRESHOLD);
        for i in THRESHOLD..n {
            s.unguarded_linear_insert(i, 0);
        }
    } else {
        s.insertion_sort(0, n);
    }
}

#[cfg(test)]
mod tests {
    use super::std_sort;

    #[test]
    fn sorts_like_a_sort() {
        let mut x = 12345u64;
        for n in [0usize, 1, 2, 5, 16, 17, 33, 100, 1000] {
            let mut v: Vec<usize> = (0..n)
                .map(|_| {
                    x = x
                        .wrapping_mul(6364136223846793005)
                        .wrapping_add(1442695040888963407);
                    usize::try_from((x >> 40) % 50).unwrap_or(0) + 1
                })
                .collect();
            let mut w = v.clone();
            w.sort_unstable();
            std_sort(&mut v, |a, b| a < b);
            assert_eq!(v, w, "n={n}");
        }
    }

    #[test]
    fn inconsistent_comparison_terminates() {
        let mut v: Vec<usize> = (0..200).collect();
        std_sort(&mut v, |_, _| true);
        let mut u: Vec<usize> = (0..200).collect();
        std_sort(&mut u, |_, _| false);
    }
}
