use super::{Arena, Handle};

pub type Link<T> = Handle<Node<T>>;

pub struct Node<T> {
    value: T,
    prev: Option<Link<T>>,
    next: Option<Link<T>>,
}

/// `std::list` with stable positions: a cursor stays valid when other elements are inserted or
/// erased; a cursor on an erased element resolves to nothing instead of dangling.
pub struct List<T> {
    nodes: Arena<Node<T>>,
    head: Option<Link<T>>,
    tail: Option<Link<T>>,
}

impl<T> Default for List<T> {
    fn default() -> Self {
        Self {
            nodes: Arena::default(),
            head: None,
            tail: None,
        }
    }
}

impl<T: Copy> List<T> {
    pub fn len(&self) -> usize {
        self.nodes.len()
    }

    pub fn is_empty(&self) -> bool {
        self.nodes.is_empty()
    }

    pub fn first(&self) -> Option<Link<T>> {
        self.head
    }

    pub fn next(&self, at: Link<T>) -> Option<Link<T>> {
        self.nodes.get(at)?.next
    }

    pub fn get(&self, at: Link<T>) -> Option<T> {
        self.nodes.get(at).map(|n| n.value)
    }

    pub fn push_back(&mut self, value: T) -> Option<Link<T>> {
        let link = self.nodes.insert(Node {
            value,
            prev: self.tail,
            next: None,
        })?;
        match self.tail.and_then(|t| self.nodes.get_mut(t)) {
            Some(t) => t.next = Some(link),
            None => self.head = Some(link),
        }
        self.tail = Some(link);
        Some(link)
    }

    /// Unlinks `at`; returns the element after it (what `std::list::erase` returns).
    pub fn erase(&mut self, at: Link<T>) -> Option<Link<T>> {
        let node = self.nodes.remove(at)?;
        match node.prev.and_then(|p| self.nodes.get_mut(p)) {
            Some(p) => p.next = node.next,
            None => self.head = node.next,
        }
        match node.next.and_then(|n| self.nodes.get_mut(n)) {
            Some(n) => n.prev = node.prev,
            None => self.tail = node.prev,
        }
        node.next
    }

    pub fn iter(&self) -> impl Iterator<Item = T> + '_ {
        let mut at = self.head;
        std::iter::from_fn(move || {
            let cur = at?;
            at = self.next(cur);
            self.get(cur)
        })
    }

    pub fn clear(&mut self) {
        self.nodes = Arena::default();
        self.head = None;
        self.tail = None;
    }
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn push_erase_order_and_stable_cursor() {
        let mut l = List::default();
        let hs: Vec<_> = (0..4).map(|i| l.push_back(i).unwrap()).collect();
        let after = l.erase(hs[1]);
        assert_eq!(after, Some(hs[2]));
        assert_eq!(l.iter().collect::<Vec<_>>(), [0, 2, 3]);
        assert_eq!(l.get(hs[1]), None);
        assert_eq!(l.erase(hs[3]), None);
        assert_eq!(l.push_back(9).and_then(|h| l.get(h)), Some(9));
        assert_eq!(l.iter().collect::<Vec<_>>(), [0, 2, 9]);
        assert_eq!(l.erase(hs[0]), Some(hs[2]));
        assert_eq!(l.first(), Some(hs[2]));
    }

    #[test]
    fn clear_resets() {
        let mut l = List::default();
        l.push_back(1);
        l.clear();
        assert!(l.is_empty() && l.first().is_none());
    }
}
