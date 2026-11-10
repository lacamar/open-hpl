//! libstdc++'s `std::sort` (introsort + heap fallback + insertion sort), step for step, so that
//! elements the comparator ranks equal land where the C++ build puts them.
//!
//! Out-of-range indices read `T::default()` and write nothing; a well-formed comparator never
//! gets there.

struct Sorter<'a, T, F> {
    v: &'a mut [T],
    less: F,
    /// Heap routines work on `v[base..]`.
    base: usize,
}

impl<T: Copy + Default, F: Fn(&T, &T) -> bool> Sorter<'_, T, F> {
    fn at(&self, i: usize) -> T {
        self.v.get(i).copied().unwrap_or_default()
    }

    fn set(&mut self, i: usize, x: T) {
        if let Some(s) = self.v.get_mut(i) {
            *s = x;
        }
    }

    fn lt(&self, a: usize, b: usize) -> bool {
        (self.less)(&self.at(a), &self.at(b))
    }

    fn swap(&mut self, a: usize, b: usize) {
        let (x, y) = (self.at(a), self.at(b));
        self.set(a, y);
        self.set(b, x);
    }

    fn hat(&self, i: usize) -> T {
        self.at(self.base + i)
    }

    fn hset(&mut self, i: usize, x: T) {
        self.set(self.base + i, x);
    }

    fn hlt(&self, a: usize, b: usize) -> bool {
        self.lt(self.base + a, self.base + b)
    }

    fn push_heap(&mut self, mut hole: usize, top: usize, value: T) {
        while hole > top {
            let parent = (hole - 1) >> 1;
            if !(self.less)(&self.hat(parent), &value) {
                break;
            }
            let p = self.hat(parent);
            self.hset(hole, p);
            hole = parent;
        }
        self.hset(hole, value);
    }

    fn adjust_heap(&mut self, mut hole: usize, len: usize, value: T) {
        let top = hole;
        let mut child = hole;
        while child < (len.wrapping_sub(1) >> 1) {
            child = 2 * (child + 1);
            if self.hlt(child, child - 1) {
                child -= 1;
            }
            let c = self.hat(child);
            self.hset(hole, c);
            hole = child;
        }
        if len & 1 == 0 && child == (len.wrapping_sub(2) >> 1) {
            child = 2 * (child + 1);
            let c = self.hat(child - 1);
            self.hset(hole, c);
            hole = child - 1;
        }
        self.push_heap(hole, top, value);
    }

    fn make_heap(&mut self, len: usize) {
        if len < 2 {
            return;
        }
        let mut parent = (len - 2) >> 1;
        loop {
            let value = self.hat(parent);
            self.adjust_heap(parent, len, value);
            if parent == 0 {
                return;
            }
            parent -= 1;
        }
    }

    /// `std::partial_sort(first, last, last)`.
    fn heap_sort(&mut self, first: usize, last: usize) {
        self.base = first;
        let len = last - first;
        self.make_heap(len);
        let mut end = len;
        while end > 1 {
            end -= 1;
            let value = self.hat(end);
            let top = self.hat(0);
            self.hset(end, top);
            self.adjust_heap(0, end, value);
        }
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
        let n = self.v.len();
        loop {
            while first < n && self.lt(first, pivot) {
                first += 1;
            }
            last = last.wrapping_sub(1);
            while last < n && self.lt(pivot, last) {
                last = last.wrapping_sub(1);
            }
            if first >= last {
                return first;
            }
            self.swap(first, last);
            first += 1;
        }
    }

    fn introsort_loop(&mut self, first: usize, mut last: usize, mut depth: u32) {
        while last - first > 16 {
            if depth == 0 {
                self.heap_sort(first, last);
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

    fn linear_insert(&mut self, mut last: usize) {
        let val = self.at(last);
        while last > 0 && (self.less)(&val, &self.at(last - 1)) {
            let p = self.at(last - 1);
            self.set(last, p);
            last -= 1;
        }
        self.set(last, val);
    }

    fn insertion_sort(&mut self, first: usize, last: usize) {
        for i in first + 1..last {
            if self.lt(i, first) {
                let val = self.at(i);
                for k in (first..i).rev() {
                    let p = self.at(k);
                    self.set(k + 1, p);
                }
                self.set(first, val);
            } else {
                self.linear_insert(i);
            }
        }
    }
}

pub fn sort<T: Copy + Default>(v: &mut [T], less: impl Fn(&T, &T) -> bool) {
    let n = v.len();
    if n < 2 {
        return;
    }
    let mut s = Sorter { v, less, base: 0 };
    s.introsort_loop(0, n, n.ilog2() * 2);
    if n > 16 {
        s.insertion_sort(0, 16);
        for i in 16..n {
            s.linear_insert(i);
        }
    } else {
        s.insertion_sort(0, n);
    }
}
