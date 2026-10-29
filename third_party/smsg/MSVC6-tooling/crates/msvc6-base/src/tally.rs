#[derive(Debug, Clone, PartialEq, Eq)]
pub struct Tally {
    what: String,
    denom: usize,
    parts: Vec<(String, usize)>,
    may_overlap: bool,
}

impl Tally {
    #[must_use]
    pub fn new(what: impl Into<String>, denom: usize) -> Self {
        Self {
            what: what.into(),
            denom,
            parts: Vec::new(),
            may_overlap: false,
        }
    }

    #[must_use]
    pub fn overlapping(mut self) -> Self {
        self.may_overlap = true;
        self
    }

    #[must_use]
    pub fn part(mut self, name: impl Into<String>, n: usize) -> Self {
        self.parts.push((name.into(), n));
        self
    }

    #[must_use]
    pub fn denominator(&self) -> usize {
        self.denom
    }

    #[must_use]
    pub fn parts(&self) -> &[(String, usize)] {
        &self.parts
    }

    fn sum(&self) -> usize {
        self.parts
            .iter()
            .map(|(_, n)| *n)
            .fold(0, usize::saturating_add)
    }

    #[must_use]
    pub fn residual(&self) -> usize {
        self.denom.saturating_sub(self.sum())
    }

    #[must_use]
    pub fn over_counted(&self) -> bool {
        self.sum() > self.denom
    }

    #[must_use]
    pub fn render(&self, indent: &str) -> String {
        let mut out = format!("{indent}{} -- {}\n", self.denom, self.what);
        for (name, n) in &self.parts {
            out.push_str(&format!("{indent}  {n:>8}  {name}\n"));
        }
        if self.over_counted() {
            out.push_str(&format!(
                "{indent}  PARTS SUM TO {}, WHICH EXCEEDS THE {} ABOVE BY {}.\n",
                self.sum(),
                self.denom,
                self.sum().saturating_sub(self.denom)
            ));
            out.push_str(&format!(
                "{indent}  {}\n",
                if self.may_overlap {
                    "EXPECTED: these parts are counted independently over the same population, so one byte \
                     meeting two of them is in both. The residual below is floored at zero: a LOWER bound, not a \
                     measurement."
                } else {
                    "NOT EXPECTED: these parts were declared non-overlapping. One of them counts outside the \
                     denominator, or the denominator measures a smaller population than the parts. Do not read the \
                     residual."
                }
            ));
        }
        out.push_str(&format!(
            "{indent}  {:>8}  RESIDUAL, derived here and nowhere else\n",
            self.residual()
        ));
        out
    }
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn the_residual_is_derived_from_the_parts() {
        let t = Tally::new("differing bytes in gaps", 8603)
            .part("same-shift operand", 1180)
            .part("cross-block operand", 3144);
        assert_eq!(t.residual(), 8603 - 1180 - 3144);
        assert!(!t.over_counted());
        assert_eq!(t.denominator(), 8603);
        assert_eq!(t.parts().len(), 2);
    }

    #[test]
    fn over_counted_parts_are_announced_and_the_residual_floors() {
        let t = Tally::new("differing bytes in gaps", 8603)
            .overlapping()
            .part("a", 5000)
            .part("b", 5000);
        assert!(t.over_counted());
        assert_eq!(t.residual(), 0);
        let s = t.render("  ");
        assert!(s.contains("EXCEEDS"), "{s}");
        assert!(s.contains("LOWER bound"), "{s}");
    }

    #[test]
    fn an_undeclared_overlap_reads_as_a_defect() {
        let s = Tally::new("x", 10).part("a", 7).part("b", 8).render("");
        assert!(s.contains("NOT EXPECTED"), "{s}");
        assert!(s.contains("Do not read the residual"), "{s}");
    }
}
