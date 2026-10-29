use std::collections::{BTreeMap, BTreeSet, HashMap};

use crate::linkmap::Map;
use crate::objkey::Keys;
use crate::verbose::Group;

#[derive(Debug, Default, Clone)]
pub struct RefOwners {
    pub refof: HashMap<String, u32>,
    refobj: BTreeMap<u32, BTreeSet<String>>,
}

#[derive(Clone, Debug, PartialEq, Eq)]
pub enum Owner {
    Known(BTreeSet<String>),
    Between(String, String),
    Unknown,
}

pub type Resolver<'a> = dyn Fn(&str) -> Option<u32> + 'a;

#[must_use]
pub fn objects_of(map: &Map, groups: &[Group], keys: &Keys) -> BTreeMap<String, String> {
    let mut objof = map.objects(keys);
    for g in groups {
        for m in g.members() {
            objof.insert(m.name.clone(), keys.log_owner(&m.obj).to_string());
        }
    }
    objof
}

impl RefOwners {
    #[must_use]
    pub fn new(
        ours: &BTreeMap<String, u32>,
        objof: &BTreeMap<String, String>,
        resolve: &Resolver<'_>,
        keys: &Keys,
    ) -> Self {
        let mut me = Self::default();
        for n in ours.keys() {
            if let Some(r) = resolve(n) {
                me.refof.insert(n.clone(), r);
                if let Some(o) = objof.get(n) {
                    me.refobj
                        .entry(r)
                        .or_default()
                        .insert(keys.object_key(o).to_string());
                }
            }
        }
        me
    }

    #[must_use]
    pub fn owner(&self, r: u32) -> Owner {
        let below = self.refobj.range(..r).next_back().map(|(_, o)| o);
        let above = r
            .checked_add(1)
            .and_then(|n| self.refobj.range(n..).next())
            .map(|(_, o)| o);
        match (below, above) {
            (Some(b), Some(a)) => {
                let both: BTreeSet<String> = b.intersection(a).cloned().collect();
                if both.is_empty() {
                    Owner::Between(
                        b.iter().next().cloned().unwrap_or_default(),
                        a.iter().next().cloned().unwrap_or_default(),
                    )
                } else {
                    Owner::Known(both)
                }
            }
            _ => Owner::Unknown,
        }
    }

    #[must_use]
    pub fn owner_at(&self, r: u32) -> String {
        match self.owner(r) {
            Owner::Known(s) => s.into_iter().collect::<Vec<_>>().join("|"),
            Owner::Between(b, a) => format!("between {b} and {a}"),
            Owner::Unknown => "?".into(),
        }
    }
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn an_address_is_owned_by_the_objects_of_its_neighbours() {
        let ours: BTreeMap<String, u32> = [("a", 0), ("b", 0x10), ("c", 0x20), ("d", 0x30)]
            .iter()
            .map(|(n, r)| ((*n).to_string(), *r))
            .collect();
        let objof: BTreeMap<String, String> = [
            ("a", "A.obj"),
            ("b", "A.obj"),
            ("c", "engine:B.obj"),
            ("d", "engine.lib(B.obj)"),
        ]
        .iter()
        .map(|(n, o)| ((*n).to_string(), (*o).to_string()))
        .collect();
        let keys = Keys::new(["engine"]);
        let at = |n: &str| ours.get(n).map(|r| r + 0x100);
        let o = RefOwners::new(&ours, &objof, &at, &keys);
        assert_eq!(o.owner_at(0x108), "A.obj");
        assert_eq!(o.owner_at(0x118), "between A.obj and B.obj");
        assert_eq!(o.owner_at(0x128), "B.obj");
        assert_eq!(o.owner_at(0x138), "?");
        assert_eq!(o.owner_at(u32::MAX), "?");
    }
}
