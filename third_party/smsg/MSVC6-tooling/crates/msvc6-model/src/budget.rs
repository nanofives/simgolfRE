use std::cell::Cell;

pub const NESTING: u32 = 256;

#[derive(Debug)]
pub struct Budget {
    left: Cell<u64>,
    spent: Cell<u64>,
    depth: Cell<u32>,
    deepest: Cell<u32>,
    max_depth: u32,
    over: Cell<Option<&'static str>>,
}

impl Budget {
    #[must_use]
    pub fn new(steps: u64, max_depth: u32) -> Self {
        Self {
            left: Cell::new(steps),
            spent: Cell::new(0),
            depth: Cell::new(0),
            deepest: Cell::new(0),
            max_depth,
            over: Cell::new(None),
        }
    }

    #[must_use]
    pub fn per_item(items: usize, per: u64, floor: u64) -> Self {
        let n = u64::try_from(items).unwrap_or(u64::MAX);
        Self::new(n.saturating_mul(per).saturating_add(floor), NESTING)
    }

    pub fn tick(&self) -> bool {
        if self.over.get().is_some() {
            return false;
        }
        let left = self.left.get();
        if left == 0 {
            self.over.set(Some("its step budget"));
            return false;
        }
        self.left.set(left - 1);
        self.spent.set(self.spent.get().saturating_add(1));
        true
    }

    pub fn spend(&self, n: usize) -> bool {
        if self.over.get().is_some() {
            return false;
        }
        let n = u64::try_from(n).unwrap_or(u64::MAX);
        let left = self.left.get();
        if n > left {
            self.left.set(0);
            self.spent.set(self.spent.get().saturating_add(left));
            self.over.set(Some("its step budget"));
            return false;
        }
        self.left.set(left - n);
        self.spent.set(self.spent.get().saturating_add(n));
        true
    }

    pub fn enter(&self) -> bool {
        if self.over.get().is_some() {
            return false;
        }
        let d = self.depth.get().saturating_add(1);
        if d > self.max_depth {
            self.over.set(Some("its nesting limit"));
            return false;
        }
        self.depth.set(d);
        if d > self.deepest.get() {
            self.deepest.set(d);
        }
        true
    }

    pub fn leave(&self) {
        self.depth.set(self.depth.get().saturating_sub(1));
    }

    pub fn it<'a, T>(&'a self, s: &'a [T]) -> impl Iterator<Item = &'a T> + 'a {
        s.iter().take_while(move |_| self.tick())
    }

    pub fn rit<'a, T>(&'a self, s: &'a [T]) -> impl Iterator<Item = &'a T> + 'a {
        s.iter().rev().take_while(move |_| self.tick())
    }

    #[must_use]
    pub fn spent(&self) -> u64 {
        self.spent.get()
    }

    #[must_use]
    pub fn deepest(&self) -> u32 {
        self.deepest.get()
    }

    #[must_use]
    pub fn over(&self) -> Option<&'static str> {
        self.over.get()
    }

    #[must_use]
    pub fn max_depth(&self) -> u32 {
        self.max_depth
    }
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn a_budget_stops_every_walk_once_spent_and_says_which_bound() {
        let b = Budget::new(3, 2);
        let v = [1, 2, 3, 4, 5];
        assert_eq!(b.it(&v).count(), 3);
        assert_eq!(b.it(&v).count(), 0);
        assert_eq!(b.over(), Some("its step budget"));
        let d = Budget::new(10, 2);
        assert!(d.enter() && d.enter());
        assert!(!d.enter());
        assert_eq!(d.over(), Some("its nesting limit"));
        assert_eq!(d.deepest(), 2);
        assert!(!d.tick());
    }
}
