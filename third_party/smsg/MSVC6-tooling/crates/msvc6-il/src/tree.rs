use crate::ex::{Arg, Rec};
use crate::grammar::Grammar;
use crate::ops;
use crate::ty;

pub const MAX_TEXT: usize = 4096;
pub const MAX_DEPTH: usize = 4096;
pub const MAX_ITEMS: usize = 1 << 16;
pub const MAX_CALLS: usize = 1 << 16;
pub const EH_LIVE: u8 = 0x5c;

#[derive(Debug, Clone, PartialEq, Eq, Default)]
pub struct Item {
    pub text: String,
    pub sym: Option<u32>,
    pub calls: Vec<usize>,
}

impl Item {
    fn text(s: String) -> Self {
        Self {
            text: clip(s),
            ..Self::default()
        }
    }
}

fn clip(mut s: String) -> String {
    if s.len() > MAX_TEXT {
        let mut cut = MAX_TEXT;
        while !s.is_char_boundary(cut) {
            cut = cut.saturating_sub(1);
        }
        s.truncate(cut);
        s.push_str("...");
    }
    s
}

fn joined(items: &[Item]) -> Vec<usize> {
    let mut v: Vec<usize> = items.iter().flat_map(|i| i.calls.iter().copied()).collect();
    v.truncate(MAX_CALLS);
    v
}

#[derive(Debug, Clone, Copy, PartialEq, Eq)]
enum Kind {
    Top,
    Call,
    Entry,
    Switch,
}

#[derive(Debug, Clone)]
struct Frame {
    kind: Kind,
    head: String,
    items: Vec<Item>,
    calls: Vec<usize>,
    pending: Option<Call>,
}

#[derive(Debug, Clone, Copy, PartialEq, Eq)]
pub struct Call {
    pub index: usize,
    pub at: usize,
    pub callee: Option<u32>,
    pub this: bool,
    pub slot: bool,
    pub intrinsic: bool,
    pub cleanup: bool,
    pub exit: bool,
    pub dead: bool,
}

#[derive(Debug, Clone, PartialEq, Eq, Default)]
pub struct Step {
    pub stmt: Option<String>,
    pub call: Option<Call>,
    pub cleanup: Vec<usize>,
}

#[derive(Debug, Clone)]
pub struct Trees {
    frames: Vec<Frame>,
    pub broken: bool,
    this_mode: u8,
    calls: usize,
    calls_at_end: bool,
}

pub trait Names {
    fn short(&self, id: u32) -> String;
    fn full(&self, id: u32) -> String;
}

fn arg_text(names: &dyn Names, a: &Arg) -> String {
    match a {
        Arg::Type(t, blk) => ty::of(*t).text(*blk),
        Arg::Id(i) => names.full(*i),
        Arg::Byte(b) => format!("b:0x{b:x}"),
        Arg::Short(s) => format!("{s}"),
        Arg::Long(l) => {
            if (-0x100..0x100).contains(l) {
                format!("{l}")
            } else {
                format!("0x{:x}", u32::from_le_bytes(l.to_le_bytes()))
            }
        }
        Arg::Quad(q) => format!("{q}"),
        Arg::Real(r) => format!("{:?}", crate::ex::ext80(r)),
        Arg::LStr(s) => crate::cursor::quote(s),
        Arg::List(v) => {
            let items: Vec<String> = v
                .iter()
                .map(|(n, b)| {
                    let w = if *n < 700 {
                        i32::from(*n) + 700
                    } else {
                        i32::from(*n)
                    };
                    format!("C4{w:03}:0x{b:x}")
                })
                .collect();
            format!("[{}]", items.join(" "))
        }
    }
}

#[must_use]
pub fn rec_text(names: &dyn Names, r: &Rec) -> String {
    let info = ops::info(r.op);
    let mut s = match r.sub {
        Some(sub) => format!("{:02x}.{:02x} {}", r.op, sub, ops::pragma_name(sub)),
        None => format!("{:02x}    {}", r.op, info.name),
    };
    for a in &r.args {
        s.push(' ');
        s.push_str(&arg_text(names, a));
    }
    s
}

fn sym_of(names: &dyn Names, r: &Rec) -> String {
    r.first_id().map(|i| names.short(i)).unwrap_or_default()
}

fn ty_of(r: &Rec) -> String {
    r.args
        .iter()
        .find_map(|a| match a {
            Arg::Type(t, b) => Some(ty::of(*t).text(*b).replace(' ', "_")),
            _ => None,
        })
        .unwrap_or_default()
}

impl Default for Trees {
    fn default() -> Self {
        Self::new()
    }
}

impl Trees {
    #[must_use]
    pub fn new() -> Self {
        Self {
            frames: vec![Frame {
                kind: Kind::Top,
                head: String::new(),
                items: Vec::new(),
                calls: Vec::new(),
                pending: None,
            }],
            broken: false,
            this_mode: 0,
            calls: 0,
            calls_at_end: false,
        }
    }

    #[must_use]
    pub fn calls_at_end(mut self) -> Self {
        self.calls_at_end = true;
        self
    }

    fn top(&mut self) -> Option<&mut Frame> {
        self.frames.last_mut()
    }

    fn push(&mut self, it: Item) {
        let over = self.top().is_some_and(|f| f.items.len() >= MAX_ITEMS);
        if over {
            self.broken = true;
            return;
        }
        if let Some(f) = self.top() {
            f.items.push(it);
        }
    }

    fn pop(&mut self) -> Item {
        match self.top().and_then(|f| f.items.pop()) {
            Some(s) => s,
            None => {
                self.broken = true;
                Item::text("?".to_string())
            }
        }
    }

    fn open(&mut self, kind: Kind, head: String, calls: Vec<usize>, pending: Option<Call>) {
        if self.frames.len() >= MAX_DEPTH {
            self.broken = true;
            return;
        }
        self.frames.push(Frame {
            kind,
            head: clip(head),
            items: Vec::new(),
            calls,
            pending,
        });
    }

    fn named(names: &dyn Names, r: &Rec) -> Item {
        Item {
            text: sym_of(names, r),
            sym: r.first_id(),
            calls: Vec::new(),
        }
    }

    pub fn step(&mut self, g: &Grammar, names: &dyn Names, r: &Rec) -> Step {
        let info = ops::info(r.op);
        let mut step = Step::default();
        let out = match r.op {
            ops::SYM | ops::PARAM => {
                self.push(Self::named(names, r));
                None
            }
            ops::CONST => {
                let v = r
                    .args
                    .get(1)
                    .map(|a| arg_text(names, a))
                    .unwrap_or_default();
                self.push(Item::text(v));
                None
            }
            ops::CTEMP => {
                let s = format!("ctemp.{}({})", ty_of(r), sym_of(names, r));
                self.push(Item::text(s));
                None
            }
            ops::THISARG | ops::THISARG2 => {
                self.this_mode = r.op.saturating_sub(0x98);
                None
            }
            ops::CALL | ops::ICALL => {
                let mode = self.this_mode;
                let this = (mode != 0).then(|| self.pop());
                self.this_mode = 0;
                let callee = self.pop();
                let index = self.calls;
                self.calls = self.calls.saturating_add(1);
                step.call = Some(Call {
                    index,
                    at: r.off,
                    callee: callee.sym,
                    this: this.is_some(),
                    slot: mode == 2,
                    intrinsic: r.op == ops::ICALL,
                    cleanup: false,
                    exit: false,
                    dead: false,
                });
                let mut calls = callee.calls.clone();
                if let Some(t) = &this {
                    calls.extend(t.calls.iter().copied());
                }
                calls.push(index);
                let head = match &this {
                    Some(t) if mode == 2 => format!(
                        "{}.{}(vslot {}, this={}",
                        info.name,
                        ty_of(r),
                        callee.text,
                        t.text
                    ),
                    Some(t) => format!(
                        "{}.{}({}, this={}",
                        info.name,
                        ty_of(r),
                        callee.text,
                        t.text
                    ),
                    None => format!("{}.{}({}", info.name, ty_of(r), callee.text),
                };
                let pending = if self.calls_at_end {
                    step.call.take()
                } else {
                    None
                };
                self.open(Kind::Call, head, calls, pending);
                None
            }
            ops::ENTRY => {
                let this = (self.this_mode != 0).then(|| self.pop());
                self.this_mode = 0;
                let f = self.pop();
                let head = match this {
                    Some(t) => format!("entry {}[this={}](", f.text, t.text),
                    None => format!("entry {}(", f.text),
                };
                self.open(Kind::Entry, head, Vec::new(), None);
                None
            }
            ops::BITFIELD => {
                let w = self.pop();
                let o = self.pop();
                let a = self.pop();
                let calls = joined(&[a.clone(), o.clone(), w.clone()]);
                self.push(Item {
                    text: clip(format!("bits({}, at {}, width {})", a.text, o.text, w.text)),
                    sym: None,
                    calls,
                });
                None
            }
            ops::SWITCH => {
                let sel = self.pop();
                self.open(
                    Kind::Switch,
                    format!("switch({})", sel.text),
                    Vec::new(),
                    None,
                );
                Some(format!("switch({}) {{  (body to endlist)", sel.text))
            }
            ops::ENDLIST => {
                if self.frames.len() <= 1 {
                    step.stmt = Some("endlist".into());
                    return step;
                }
                let Some(f) = self.frames.pop() else {
                    step.stmt = Some("endlist".into());
                    return step;
                };
                match f.kind {
                    Kind::Call => {
                        step.call = f.pending;
                        let mut s = f.head;
                        for a in &f.items {
                            s.push_str(", ");
                            s.push_str(&a.text);
                        }
                        s.push(')');
                        let mut calls = f.calls;
                        calls.extend(joined(&f.items));
                        calls.truncate(MAX_CALLS);
                        self.push(Item {
                            text: clip(s),
                            sym: None,
                            calls,
                        });
                        None
                    }
                    Kind::Entry => Some(format!(
                        "{}{})",
                        f.head,
                        f.items
                            .iter()
                            .map(|i| i.text.as_str())
                            .collect::<Vec<_>>()
                            .join(", ")
                    )),
                    _ => Some(format!(
                        "}} end {}{}",
                        f.head,
                        if f.items.is_empty() {
                            ""
                        } else {
                            " (items left)"
                        }
                    )),
                }
            }
            _ => match info.arity {
                Some((p, q)) => {
                    let mut args = Vec::new();
                    for _ in 0..p {
                        args.push(self.pop());
                    }
                    if r.op == EH_LIVE
                        && let Some(cleanup) = args.first()
                    {
                        step.cleanup.clone_from(&cleanup.calls);
                    }
                    args.reverse();
                    let t = ty_of(r);
                    let mut name = info.name.to_string();
                    if !t.is_empty() && r.op != ops::PRAGMA {
                        name = format!("{name}.{t}");
                    }
                    let lab = if matches!(
                        r.op,
                        ops::BRFALSE
                            | ops::BRTRUE
                            | ops::GOTO
                            | ops::LABEL
                            | ops::CASELIST
                            | ops::CASE
                    ) {
                        sym_of(names, r)
                    } else {
                        String::new()
                    };
                    let texts: Vec<&str> = args.iter().map(|a| a.text.as_str()).collect();
                    let text = match (args.is_empty(), lab.is_empty()) {
                        (true, true) => name,
                        (true, false) => format!("{name} {lab}"),
                        (false, true) => format!("{name}({})", texts.join(", ")),
                        (false, false) => format!("{name} {lab} ({})", texts.join(", ")),
                    };
                    if q == 1 {
                        let calls = if r.op == EH_LIVE {
                            args.first().map(|o| o.calls.clone()).unwrap_or_default()
                        } else {
                            joined(&args)
                        };
                        self.push(Item {
                            text: clip(text),
                            sym: None,
                            calls,
                        });
                        None
                    } else if p > 0 || matches!(r.op, ops::LABEL | ops::GOTO | ops::EXIT) {
                        Some(clip(text))
                    } else {
                        None
                    }
                }
                None => {
                    self.broken = true;
                    Some(format!("({} breaks the tree model)", info.name))
                }
            },
        };
        let ends_tree = r.op == ops::ENDLIST || r.ends_body();
        let at_top = self.frames.len() == 1;
        if g.ends_statement(r.op)
            && !ends_tree
            && at_top
            && let Some(f) = self.top()
            && !f.items.is_empty()
        {
            let left: Vec<String> = std::mem::take(&mut f.items)
                .into_iter()
                .map(|i| i.text)
                .collect();
            let s = format!("flushed: {}", left.join(" ; "));
            step.stmt = Some(match out {
                Some(o) => format!("{o}  {s}"),
                None => s,
            });
            return step;
        }
        step.stmt = out;
        step
    }
}
