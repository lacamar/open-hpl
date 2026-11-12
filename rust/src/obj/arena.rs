use core::marker::PhantomData;

/// Index plus generation; stale handles never resolve after their slot is reused.
pub struct Handle<T> {
    index: u32,
    generation: u32,
    _t: PhantomData<fn() -> T>,
}

impl<T> Clone for Handle<T> {
    fn clone(&self) -> Self {
        *self
    }
}
impl<T> Copy for Handle<T> {}
impl<T> PartialEq for Handle<T> {
    fn eq(&self, o: &Self) -> bool {
        self.index == o.index && self.generation == o.generation
    }
}
impl<T> Eq for Handle<T> {}
impl<T> core::fmt::Debug for Handle<T> {
    fn fmt(&self, f: &mut core::fmt::Formatter<'_>) -> core::fmt::Result {
        write!(f, "Handle({}v{})", self.index, self.generation)
    }
}

impl<T> Handle<T> {
    /// Nonzero id for C callers; 0 is never a live handle.
    pub fn to_bits(self) -> u64 {
        ((u64::from(self.generation) << 32) | u64::from(self.index)) + 1
    }

    pub fn from_bits(bits: u64) -> Option<Self> {
        let v = bits.checked_sub(1)?;
        Some(Self {
            index: v as u32,
            generation: (v >> 32) as u32,
            _t: PhantomData,
        })
    }
}

struct Slot<T> {
    generation: u32,
    value: Option<T>,
}

pub struct Arena<T> {
    slots: Vec<Slot<T>>,
    free: Vec<u32>,
}

impl<T> Default for Arena<T> {
    fn default() -> Self {
        Self {
            slots: Vec::new(),
            free: Vec::new(),
        }
    }
}

impl<T> Arena<T> {
    /// `None` only when the slot count would overflow `u32`.
    pub fn insert(&mut self, value: T) -> Option<Handle<T>> {
        if let Some(index) = self.free.pop()
            && let Some(slot) = self.slots.get_mut(index as usize)
        {
            slot.value = Some(value);
            return Some(Handle {
                index,
                generation: slot.generation,
                _t: PhantomData,
            });
        }
        let index = u32::try_from(self.slots.len()).ok()?;
        self.slots.push(Slot {
            generation: 0,
            value: Some(value),
        });
        Some(Handle {
            index,
            generation: 0,
            _t: PhantomData,
        })
    }

    pub fn get(&self, h: Handle<T>) -> Option<&T> {
        self.slots
            .get(h.index as usize)
            .filter(|s| s.generation == h.generation)
            .and_then(|s| s.value.as_ref())
    }

    pub fn get_mut(&mut self, h: Handle<T>) -> Option<&mut T> {
        self.slots
            .get_mut(h.index as usize)
            .filter(|s| s.generation == h.generation)
            .and_then(|s| s.value.as_mut())
    }

    pub fn remove(&mut self, h: Handle<T>) -> Option<T> {
        let slot = self
            .slots
            .get_mut(h.index as usize)
            .filter(|s| s.generation == h.generation)?;
        let value = slot.value.take()?;
        slot.generation = slot.generation.wrapping_add(1);
        self.free.push(h.index);
        Some(value)
    }

    pub fn len(&self) -> usize {
        self.slots.len() - self.free.len()
    }

    pub fn is_empty(&self) -> bool {
        self.len() == 0
    }
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn insert_get_remove() {
        let mut a = Arena::default();
        let h = a.insert(5).unwrap();
        assert_eq!(a.get(h), Some(&5));
        *a.get_mut(h).unwrap() = 6;
        assert_eq!(a.remove(h), Some(6));
        assert_eq!(a.get(h), None);
        assert!(a.is_empty());
    }

    #[test]
    fn stale_handle_does_not_resolve_after_reuse() {
        let mut a = Arena::default();
        let h1 = a.insert('a').unwrap();
        a.remove(h1);
        let h2 = a.insert('b').unwrap();
        assert_ne!(h1, h2);
        assert_eq!(a.get(h1), None);
        assert_eq!(a.get(h2), Some(&'b'));
        assert_eq!(a.remove(h1), None);
    }

    #[test]
    fn bits_round_trip_and_zero_is_null() {
        let mut a = Arena::default();
        let h = a.insert(1).unwrap();
        assert_ne!(h.to_bits(), 0);
        assert_eq!(Handle::from_bits(h.to_bits()), Some(h));
        assert_eq!(Handle::<i32>::from_bits(0), None);
    }

    #[test]
    fn len_counts_live_values() {
        let mut a = Arena::default();
        let hs: Vec<_> = (0..4).map(|i| a.insert(i).unwrap()).collect();
        a.remove(hs[1]);
        assert_eq!(a.len(), 3);
    }
}
