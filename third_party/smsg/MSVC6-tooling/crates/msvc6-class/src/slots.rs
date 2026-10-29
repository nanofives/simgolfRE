use crate::decl::{ClassDecl, DTOR, MAX_DEPTH, MAX_LINEAGE, Method, VPTR};
use std::cell::{Cell, RefCell};
use std::collections::{BTreeMap, BTreeSet};

pub const MAX_STEPS: usize = 1 << 16;
pub const MAX_FLAT: usize = 1 << 16;

#[derive(Debug, Clone, Copy, PartialEq, Eq, Default)]
pub enum Rule {
    #[default]
    Vc6,
    Legacy,
}

impl Rule {
    #[must_use]
    pub fn legacy(self) -> bool {
        self == Self::Legacy
    }
}

#[derive(Debug, Clone, PartialEq, Eq, PartialOrd, Ord)]
pub struct Slot {
    pub name: String,
    pub params: String,
    pub pure: bool,
    pub owner: String,
    pub stdcall: bool,
}

impl Slot {
    fn of(m: &Method, owner: &str) -> Self {
        Self {
            name: m.name.clone(),
            params: if m.name == DTOR {
                String::new()
            } else {
                m.params.clone()
            },
            pure: m.pure,
            owner: owner.to_string(),
            stdcall: m.stdcall,
        }
    }

    fn named(name: String) -> Self {
        Self {
            name,
            params: String::new(),
            pure: false,
            owner: String::new(),
            stdcall: false,
        }
    }
}

#[derive(Debug, Clone, PartialEq, Eq)]
pub enum Implied {
    Count { slots: usize, from_base: usize },
    UnknownBase(String),
    AmbiguousBase(String, Vec<usize>),
}

pub const TOO_DEEP: &str = "<cycle or too deep>";

type Key = (usize, usize, bool);

#[derive(Debug, Default)]
struct Memo {
    vc6: BTreeMap<Key, Option<Vec<Slot>>>,
    legacy: BTreeMap<Key, Option<Vec<String>>>,
    implied: BTreeMap<Key, Implied>,
    flat: BTreeMap<Key, Option<Vec<String>>>,
}

fn key_of(name: &str, params: &str) -> (String, String) {
    if name == DTOR {
        (DTOR.to_string(), String::new())
    } else {
        (name.to_string(), params.to_string())
    }
}

#[derive(Debug)]
pub struct Model<'a> {
    pop: &'a [ClassDecl],
    by_name: BTreeMap<&'a str, Vec<&'a ClassDecl>>,
    steps: Cell<usize>,
    spent: Cell<bool>,
    spent_queries: Cell<usize>,
    memo: RefCell<Memo>,
}

impl<'a> Model<'a> {
    #[must_use]
    pub fn new(pop: &'a [ClassDecl]) -> Self {
        let mut by_name: BTreeMap<&str, Vec<&ClassDecl>> = BTreeMap::new();
        for c in pop {
            by_name.entry(c.name.as_str()).or_default().push(c);
        }
        Self {
            pop,
            by_name,
            steps: Cell::new(MAX_STEPS),
            spent: Cell::new(false),
            spent_queries: Cell::new(0),
            memo: RefCell::new(Memo::default()),
        }
    }

    #[must_use]
    pub fn population(&self) -> &'a [ClassDecl] {
        self.pop
    }

    #[must_use]
    pub fn named(&self, name: &str) -> &[&'a ClassDecl] {
        self.by_name.get(name).map_or(&[], Vec::as_slice)
    }

    #[must_use]
    pub fn steps_taken(&self) -> usize {
        MAX_STEPS.saturating_sub(self.steps.get())
    }

    #[must_use]
    pub fn spent_queries(&self) -> usize {
        self.spent_queries.get()
    }

    #[must_use]
    pub fn note(&self) -> Option<String> {
        let n = self.spent_queries();
        (n > 0).then(|| {
            format!(
                "NOTE: {n} slot quer(ies) ran out of their {MAX_STEPS} steps and computed no slots"
            )
        })
    }

    fn index(&self, decl: &ClassDecl) -> Option<usize> {
        let size = std::mem::size_of::<ClassDecl>();
        let off = std::ptr::from_ref(decl)
            .addr()
            .checked_sub(self.pop.as_ptr().addr())?;
        (size > 0 && off % size == 0 && off / size < self.pop.len()).then_some(off / size)
    }

    fn cached<T: Clone>(
        &self,
        decl: &ClassDecl,
        key: (usize, bool),
        table: fn(&mut Memo) -> &mut BTreeMap<Key, T>,
        compute: impl FnOnce() -> T,
    ) -> T {
        let k = self.index(decl).map(|i| (i, key.0, key.1));
        if let Some(k) = k
            && let Some(v) = table(&mut self.memo.borrow_mut()).get(&k)
        {
            return v.clone();
        }
        let v = compute();
        if let Some(k) = k
            && !self.spent.get()
        {
            table(&mut self.memo.borrow_mut()).insert(k, v.clone());
        }
        v
    }

    fn candidates(&self, base: &str, decl: &ClassDecl) -> Vec<&'a ClassDecl> {
        let all = self.named(base);
        let same: Vec<&ClassDecl> = all
            .iter()
            .copied()
            .filter(|c| c.file == decl.file)
            .collect();
        if same.is_empty() { all.to_vec() } else { same }
    }

    fn begin(&self) {
        self.steps.set(MAX_STEPS);
        self.spent.set(false);
    }

    fn step(&self, depth: usize) -> Option<()> {
        if depth > MAX_DEPTH {
            return None;
        }
        let s = self.steps.get();
        if s == 0 {
            if !self.spent.get() {
                self.spent_queries
                    .set(self.spent_queries.get().saturating_add(1));
            }
            self.spent.set(true);
            return None;
        }
        self.steps.set(s - 1);
        Some(())
    }

    fn agreed<T: Ord + Clone>(
        &self,
        base: &str,
        decl: &ClassDecl,
        each: &dyn Fn(&ClassDecl) -> Option<T>,
    ) -> Option<T> {
        let mut seen: BTreeSet<T> = BTreeSet::new();
        for c in self.candidates(base, decl) {
            seen.insert(each(c)?);
        }
        if seen.len() == 1 {
            seen.into_iter().next()
        } else {
            None
        }
    }

    fn legacy_names(&self, decl: &ClassDecl, depth: usize) -> Option<Vec<String>> {
        self.cached(
            decl,
            (depth, false),
            |m| &mut m.legacy,
            || self.legacy_names_of(decl, depth),
        )
    }

    fn legacy_names_of(&self, decl: &ClassDecl, depth: usize) -> Option<Vec<String>> {
        self.step(depth)?;
        let mut out: Vec<String> = Vec::new();
        if let Some(base) = decl.bases.iter().find(|b| !decl.virtual_bases.contains(b)) {
            out = self.agreed(base, decl, &|c| self.legacy_names(c, depth + 1))?;
        }
        let mut elsewhere: BTreeSet<String> = BTreeSet::new();
        for vb in &decl.virtual_bases {
            for c in self.named(vb) {
                elsewhere.extend(self.legacy_names(c, depth + 1)?);
            }
        }
        let mut have: BTreeSet<String> = out.iter().cloned().collect();
        for v in &decl.legacy.virtuals {
            if !have.contains(v) && !elsewhere.contains(v) {
                have.insert(v.clone());
                out.push(v.clone());
            }
        }
        Some(out)
    }

    fn primary(&self, decl: &ClassDecl, depth: usize) -> Option<Option<(String, Vec<Slot>)>> {
        let mut first: Option<(String, Vec<Slot>)> = None;
        for b in decl
            .bases
            .iter()
            .filter(|b| !decl.virtual_bases.contains(b))
        {
            let s = self.agreed(b, decl, &|c| self.vc6_slots(c, depth + 1))?;
            if !s.is_empty() {
                return Some(Some((b.clone(), s)));
            }
            first.get_or_insert((b.clone(), s));
        }
        Some(first)
    }

    fn vc6_slots(&self, decl: &ClassDecl, depth: usize) -> Option<Vec<Slot>> {
        self.cached(
            decl,
            (depth, false),
            |m| &mut m.vc6,
            || self.vc6_slots_of(decl, depth),
        )
    }

    fn vc6_slots_of(&self, decl: &ClassDecl, depth: usize) -> Option<Vec<Slot>> {
        self.step(depth)?;
        let prim = self.primary(decl, depth)?;
        let prim_name = prim.as_ref().map(|(n, _)| n.clone());
        let mut table: Vec<Slot> = prim.map(|(_, s)| s).unwrap_or_default();
        let mut elsewhere: BTreeSet<(String, String)> = BTreeSet::new();
        for b in decl.bases.iter().filter(|b| Some(*b) != prim_name.as_ref()) {
            let from = if decl.virtual_bases.contains(b) {
                let mut all = Vec::new();
                for c in self.named(b) {
                    all.extend(self.vc6_slots(c, depth + 1)?);
                }
                all
            } else {
                self.agreed(b, decl, &|c| self.vc6_slots(c, depth + 1))?
            };
            elsewhere.extend(from.iter().map(|s| key_of(&s.name, &s.params)));
        }
        let mut at: BTreeMap<(String, String), usize> = BTreeMap::new();
        for (i, s) in table.iter().enumerate() {
            at.entry(key_of(&s.name, &s.params)).or_insert(i);
        }
        let mut groups: Vec<(&str, Vec<&Method>)> = Vec::new();
        let mut group_of: BTreeMap<&str, usize> = BTreeMap::new();
        for m in &decl.methods {
            let g = *group_of.entry(m.name.as_str()).or_insert_with(|| {
                groups.push((m.name.as_str(), Vec::new()));
                groups.len() - 1
            });
            if let Some((_, v)) = groups.get_mut(g) {
                v.push(m);
            }
        }
        for (_, members) in &groups {
            for m in members.iter().rev() {
                let k = key_of(&m.name, &m.params);
                if let Some(s) = at.get(&k).and_then(|i| table.get_mut(*i)) {
                    s.pure = m.pure;
                    s.owner.clone_from(&decl.name);
                    continue;
                }
                if elsewhere.contains(&k) {
                    continue;
                }
                if m.virt {
                    at.insert(k, table.len());
                    table.push(Slot::of(m, &decl.name));
                }
            }
        }
        if let Some(s) = table.iter_mut().find(|s| s.name == DTOR) {
            s.owner.clone_from(&decl.name);
        }
        Some(table)
    }

    #[must_use]
    pub fn slots(&self, decl: &ClassDecl, rule: Rule) -> Option<Vec<Slot>> {
        self.begin();
        match rule {
            Rule::Vc6 => self.vc6_slots(decl, 0),
            Rule::Legacy => self
                .legacy_names(decl, 0)
                .map(|v| v.into_iter().map(Slot::named).collect()),
        }
    }

    #[must_use]
    pub fn slot_names(&self, decl: &ClassDecl, rule: Rule) -> Option<Vec<String>> {
        self.slots(decl, rule)
            .map(|v| v.into_iter().map(|s| s.name).collect())
    }

    fn legacy_implied(&self, decl: &ClassDecl, depth: usize) -> Implied {
        self.cached(
            decl,
            (depth, false),
            |m| &mut m.implied,
            || self.legacy_implied_of(decl, depth),
        )
    }

    fn legacy_implied_of(&self, decl: &ClassDecl, depth: usize) -> Implied {
        if self.step(depth).is_none() {
            return Implied::UnknownBase(TOO_DEEP.to_string());
        }
        let mut slots = decl.legacy.virtuals.len();
        let mut from_base = 0usize;
        for base in &decl.bases {
            let cands = self.candidates(base, decl);
            if cands.is_empty() {
                return Implied::UnknownBase(base.clone());
            }
            let mut totals = BTreeSet::new();
            for c in &cands {
                match self.legacy_implied(c, depth.saturating_add(1)) {
                    Implied::Count { slots, .. } => {
                        totals.insert(slots);
                    }
                    _ => return Implied::UnknownBase(base.clone()),
                }
            }
            if totals.len() > 1 {
                return Implied::AmbiguousBase(base.clone(), totals.into_iter().collect());
            }
            let bslots = totals.into_iter().next().unwrap_or(0);
            let bnames: BTreeSet<&str> = cands
                .iter()
                .flat_map(|c| c.legacy.virtuals.iter().map(String::as_str))
                .collect();
            let overrides = decl
                .legacy
                .virtuals
                .iter()
                .filter(|n| bnames.contains(n.as_str()))
                .count();
            from_base = from_base.saturating_add(bslots);
            slots = slots.saturating_sub(overrides).saturating_add(bslots);
        }
        Implied::Count { slots, from_base }
    }

    #[must_use]
    pub fn implied(&self, decl: &ClassDecl, rule: Rule) -> Implied {
        self.begin();
        if rule.legacy() {
            return self.legacy_implied(decl, 0);
        }
        for base in &decl.bases {
            let cands = self.candidates(base, decl);
            if cands.is_empty() {
                return Implied::UnknownBase(base.clone());
            }
            let lens: BTreeSet<Option<usize>> = cands
                .iter()
                .map(|c| self.vc6_slots(c, 1).map(|s| s.len()))
                .collect();
            if lens.contains(&None) {
                return Implied::UnknownBase(base.clone());
            }
            if lens.len() > 1 {
                return Implied::AmbiguousBase(base.clone(), lens.into_iter().flatten().collect());
            }
        }
        self.begin();
        let from_base = self.primary(decl, 0).flatten().map_or(0, |(_, s)| s.len());
        self.begin();
        match self.vc6_slots(decl, 0) {
            Some(s) => Implied::Count {
                slots: s.len(),
                from_base,
            },
            None => Implied::UnknownBase(TOO_DEEP.to_string()),
        }
    }

    fn flat(&self, decl: &ClassDecl, rule: Rule, depth: usize) -> Option<Vec<String>> {
        self.cached(
            decl,
            (depth, rule.legacy()),
            |m| &mut m.flat,
            || self.flat_of(decl, rule, depth),
        )
    }

    fn flat_of(&self, decl: &ClassDecl, rule: Rule, depth: usize) -> Option<Vec<String>> {
        self.step(depth)?;
        let mut out: Vec<String> = Vec::new();
        for base in &decl.bases {
            let mut seen: BTreeSet<Vec<String>> = BTreeSet::new();
            for c in self.candidates(base, decl) {
                seen.insert(self.flat(c, rule, depth.saturating_add(1))?);
            }
            if seen.len() != 1 {
                return None;
            }
            out.extend(seen.into_iter().next()?);
            if out.len() > MAX_FLAT {
                return None;
            }
        }
        for d in decl.data(rule.legacy()) {
            if d == VPTR && out.iter().any(|x| x == VPTR) {
                continue;
            }
            out.push(d.clone());
        }
        Some(out)
    }

    #[must_use]
    pub fn flat_layout(&self, decl: &ClassDecl, rule: Rule) -> Option<Vec<String>> {
        self.begin();
        self.flat(decl, rule, 0)
    }

    #[must_use]
    pub fn lineage(&self, class: &str) -> Vec<String> {
        let mut out = vec![class.to_string()];
        let mut i = 0;
        while let Some(c) = out.get(i).cloned() {
            for d in self.named(&c) {
                for b in &d.bases {
                    if !out.contains(b) && out.len() < MAX_LINEAGE {
                        out.push(b.clone());
                    }
                }
            }
            i += 1;
        }
        out
    }

    #[must_use]
    pub fn primary_chain(&self, class: &str) -> Vec<String> {
        let mut out = vec![class.to_string()];
        while let Some(d) = out.last().and_then(|c| self.named(c).first().copied()) {
            match d.bases.iter().find(|b| !d.virtual_bases.contains(b)) {
                Some(b) if !out.contains(b) && out.len() < MAX_LINEAGE => out.push(b.clone()),
                _ => break,
            }
        }
        out
    }
}

#[must_use]
pub fn slots(decl: &ClassDecl, pop: &[ClassDecl], rule: Rule) -> Option<Vec<Slot>> {
    Model::new(pop).slots(decl, rule)
}

#[must_use]
pub fn slot_names(decl: &ClassDecl, pop: &[ClassDecl], rule: Rule) -> Option<Vec<String>> {
    Model::new(pop).slot_names(decl, rule)
}

#[must_use]
pub fn implied(decl: &ClassDecl, pop: &[ClassDecl], rule: Rule) -> Implied {
    Model::new(pop).implied(decl, rule)
}

#[must_use]
pub fn flat_layout(decl: &ClassDecl, pop: &[ClassDecl], rule: Rule) -> Option<Vec<String>> {
    Model::new(pop).flat_layout(decl, rule)
}

#[must_use]
pub fn param_count(params: &str) -> Option<usize> {
    let inner = params.strip_prefix('(')?;
    let mut depth = 0i32;
    let mut close = None;
    for (i, c) in inner.char_indices() {
        match c {
            '(' | '<' | '[' => depth += 1,
            ')' if depth == 0 => {
                close = Some(i);
                break;
            }
            ')' | '>' | ']' => depth -= 1,
            _ => {}
        }
    }
    let list = inner.get(..close?)?;
    if list.contains("...") {
        return None;
    }
    if list.trim().is_empty() {
        return Some(0);
    }
    let mut n = 1usize;
    let mut d = 0i32;
    for c in list.chars() {
        match c {
            '(' | '<' | '[' => d += 1,
            ')' | '>' | ']' => d -= 1,
            ',' if d == 0 => n += 1,
            _ => {}
        }
    }
    Some(n)
}

#[derive(Debug, Clone, PartialEq, Eq)]
pub enum CountVerdict {
    BelowThreshold,
    Exact(usize),
    Admissible { admitting: usize },
    Exceeds(usize),
}

#[must_use]
pub fn classify_count(slots: usize, min_slots: usize, lengths: &[usize]) -> CountVerdict {
    if slots < min_slots {
        return CountVerdict::BelowThreshold;
    }
    let exact = lengths.iter().filter(|l| **l == slots).count();
    if exact > 0 {
        return CountVerdict::Exact(exact);
    }
    match lengths.iter().copied().max() {
        Some(max) if slots > max => CountVerdict::Exceeds(max),
        _ => CountVerdict::Admissible {
            admitting: lengths.iter().filter(|l| **l >= slots).count(),
        },
    }
}

#[cfg(test)]
mod tests {
    use super::*;
    use crate::decl::{class_decls, uncomment};

    fn pop(src: &str) -> Vec<ClassDecl> {
        class_decls(&uncomment(src.as_bytes()), "t.h")
    }

    fn names(src: &str, class: &str, rule: Rule) -> Vec<String> {
        let p = pop(src);
        let d = p.iter().find(|c| c.name == class).unwrap();
        slot_names(d, &p, rule).unwrap()
    }

    #[test]
    fn an_overload_group_sits_at_its_first_declaration_in_reverse() {
        let src = "class W {\npublic:\n    virtual void load();\n    virtual void play(int *);\n    virtual void load(char *, unsigned long);\n    virtual void stop();\n    virtual void play(int);\n    virtual void load(const char *);\n    virtual void play();\n};\n";
        assert_eq!(
            names(src, "W", Rule::Vc6),
            vec!["load", "load", "load", "play", "play", "play", "stop"]
        );
        let p = pop(src);
        let s = slots(&p[0], &p, Rule::Vc6).unwrap();
        let params: Vec<&str> = s.iter().map(|s| s.params.as_str()).collect();
        assert_eq!(
            params,
            vec![
                "(const char*)",
                "(char*,unsigned long)",
                "()",
                "()",
                "(int)",
                "(int*)",
                "()"
            ]
        );
        assert_eq!(names(src, "W", Rule::Legacy), vec!["load", "play", "stop"]);
    }

    #[test]
    fn an_override_keeps_its_base_slot_and_a_new_overload_takes_one_at_the_group() {
        let src = "class B {\n    virtual ~B();\n    virtual int x(int);\n    virtual int y();\n};\nclass D : public B {\n    virtual void z();\n    int x(int);\n    virtual int x(char);\n    ~D();\n};\n";
        assert_eq!(names(src, "D", Rule::Vc6), vec!["~", "x", "y", "z", "x"]);
        let p = pop(src);
        let d = p.iter().find(|c| c.name == "D").unwrap();
        let s = slots(d, &p, Rule::Vc6).unwrap();
        assert_eq!(s.get(4).map(|s| s.params.as_str()), Some("(char)"));
        assert_eq!(
            implied(d, &p, Rule::Vc6),
            Implied::Count {
                slots: 5,
                from_base: 3
            }
        );
        assert_eq!(names(src, "D", Rule::Legacy), vec!["~", "x", "y", "z"]);
    }

    #[test]
    fn an_override_of_a_second_or_virtual_base_takes_no_slot_here() {
        let src = "class A {\n    virtual void a();\n};\nclass S {\n    virtual void s();\n};\nclass V {\n    virtual void v();\n};\nclass D : public A, public S, virtual public V {\n    virtual void s();\n    virtual void v();\n    virtual void d();\n};\n";
        assert_eq!(names(src, "D", Rule::Vc6), vec!["a", "d"]);
        assert_eq!(names(src, "D", Rule::Legacy), vec!["a", "s", "d"]);
    }

    #[test]
    fn the_primary_base_is_the_first_one_with_a_table() {
        let src = "class P {\n    int m;\n};\nclass Q {\n    virtual void q();\n};\nclass D : public P, public Q {\n    virtual void d();\n};\n";
        assert_eq!(names(src, "D", Rule::Vc6), vec!["q", "d"]);
    }

    #[test]
    fn a_base_resolves_in_its_own_file_first_and_disagreeing_copies_state_nothing() {
        let mut p = class_decls(
            "class B {\nvirtual int a();\n};\nclass D : public B {\nvirtual int z();\n};",
            "one.h",
        );
        p.extend(class_decls(
            "class B {\nvirtual int a();\nvirtual int b();\n};",
            "two.h",
        ));
        let d = p.iter().find(|c| c.name == "D").unwrap().clone();
        assert_eq!(slot_names(&d, &p, Rule::Vc6).unwrap(), vec!["a", "z"]);
        let orphan = ClassDecl {
            file: "three.h".to_string(),
            ..d
        };
        assert_eq!(slot_names(&orphan, &p, Rule::Vc6), None);
        assert_eq!(
            implied(&orphan, &p, Rule::Vc6),
            Implied::AmbiguousBase("B".into(), vec![1, 2])
        );
        assert_eq!(
            implied(&orphan, &p, Rule::Legacy),
            Implied::AmbiguousBase("B".into(), vec![1, 2])
        );
        let lone = class_decls("class E : public Missing {\nvirtual int a();\n};", "t.h");
        assert_eq!(
            implied(&lone[0], &lone, Rule::Vc6),
            Implied::UnknownBase("Missing".into())
        );
    }

    #[test]
    fn a_cycle_of_bases_states_nothing() {
        let p = pop(
            "class A : public B {\nvirtual void a();\n};\nclass B : public A {\nvirtual void b();\n};\n",
        );
        assert_eq!(slot_names(&p[0], &p, Rule::Vc6), None);
        assert_eq!(slot_names(&p[0], &p, Rule::Legacy), None);
    }

    #[test]
    fn only_a_count_exceeding_every_run_is_refused() {
        assert_eq!(classify_count(88, 2, &[53, 86]), CountVerdict::Exceeds(86));
        assert_eq!(classify_count(86, 2, &[53, 86]), CountVerdict::Exact(1));
        assert_eq!(
            classify_count(89, 2, &[96, 103]),
            CountVerdict::Admissible { admitting: 2 }
        );
        assert_eq!(classify_count(1, 2, &[86]), CountVerdict::BelowThreshold);
        assert_eq!(
            classify_count(89, 2, &[]),
            CountVerdict::Admissible { admitting: 0 }
        );
    }

    #[test]
    fn a_based_layout_equals_the_flattened_one_and_has_one_vptr() {
        let mut p = class_decls(
            "class Resource {\nvirtual ~Resource();\nint m_reserved04;\n};\nclass Based : public Resource {\nvirtual int f();\nint m_field08;\n};",
            "a.h",
        );
        p.extend(class_decls(
            "class Flat {\nvirtual ~Flat();\nint m_field04;\nint m_field08;\n};",
            "b.h",
        ));
        for rule in [Rule::Vc6, Rule::Legacy] {
            let b = flat_layout(p.iter().find(|c| c.name == "Based").unwrap(), &p, rule).unwrap();
            let f = flat_layout(p.iter().find(|c| c.name == "Flat").unwrap(), &p, rule).unwrap();
            assert_eq!(b.len(), 3, "{b:?}");
            assert_eq!(b.iter().filter(|x| *x == VPTR).count(), 1);
            assert_eq!(f.len(), 3, "{f:?}");
        }
    }
}
