use std::collections::BTreeSet;

#[derive(Debug, Clone, PartialEq, Eq, Hash, PartialOrd, Ord)]
pub enum Callee {
    Fn(usize),
    Api(String),
    Indirect,
}

#[derive(Debug, Clone, Default)]
pub struct Feat {
    pub calls: Vec<Callee>,
    pub strs: Vec<String>,
    pub consts: BTreeSet<String>,
    pub disps: BTreeSet<u32>,
    pub globals: Vec<u64>,
    pub size: usize,
    pub body: u64,
    pub loose: u64,
    pub this_ecx: Option<bool>,
    pub ret_pop: Option<u16>,
}

pub const MIN_CONST: u64 = 0x100;

#[must_use]
pub fn int_key(v: i64) -> Option<String> {
    let a = v.unsigned_abs();
    if a < MIN_CONST || a.is_power_of_two() || a.checked_add(1).is_some_and(u64::is_power_of_two) {
        return None;
    }
    Some(format!("i:{v}"))
}

pub const COMMON_FLOATS: [f64; 9] = [0.0, 1.0, -1.0, 0.5, 2.0, 10.0, 100.0, 255.0, 256.0];

#[must_use]
pub fn float_key(x: f64) -> Option<String> {
    if !x.is_finite() || COMMON_FLOATS.contains(&x) {
        return None;
    }
    Some(format!("f:{x:.5e}"))
}

#[must_use]
pub fn api_key(name: &str) -> String {
    let n = name.rsplit('!').next().unwrap_or(name);
    match n {
        "??2@YAPAXI@Z" | "_Znwm" | "_Znwj" | "__Znwm" | "__Znwj" | "Znwm" | "Znwj"
        | "__nw__FUl" | "__nw__FUi" => return "op_new".to_string(),
        "??_U@YAPAXI@Z" | "_Znam" | "_Znaj" | "__Znam" | "__Znaj" | "Znam" | "Znaj"
        | "__nwa__FUl" => return "op_new[]".to_string(),
        "??3@YAXPAX@Z" | "_ZdlPv" | "__ZdlPv" | "ZdlPv" | "__dl__FPv" => {
            return "op_delete".to_string();
        }
        "??_V@YAXPAX@Z" | "_ZdaPv" | "__ZdaPv" | "ZdaPv" | "__dla__FPv" => {
            return "op_delete[]".to_string();
        }
        _ => {}
    }
    let lower = n.trim_start_matches(['_', '.']).to_ascii_lowercase();
    match lower.strip_suffix("_win32") {
        Some(s) => s.to_string(),
        None => lower,
    }
}

pub const NOISE: &[&str] = &[
    "memcpy",
    "memset",
    "memmove",
    "memcmp",
    "strlen",
    "strcpy",
    "strcat",
    "strcmp",
    "abs",
    "labs",
    "fabs",
    "chkstk",
    "alloca_probe",
    "ftol",
    "ftol2",
    "allmul",
    "alldiv",
    "allrem",
    "aulldiv",
    "aullrem",
    "allshl",
    "allshr",
    "aullshr",
    "eh_prolog",
    "unwind_resume",
    "dyld_stub_binding_helper",
    "stack_chk_fail",
    "divdi3",
    "udivdi3",
    "moddi3",
    "umoddi3",
    "floatdidf",
    "fixdfdi",
    "fixunsdfdi",
    "ptr_glue",
    "savef14",
    "restf14",
];

#[must_use]
pub fn is_noise(key: &str) -> bool {
    NOISE.contains(&key)
        || ["cxa_", "gxx_", "savegpr", "restgpr", "savefpr", "restfpr"]
            .iter()
            .any(|p| key.starts_with(p))
}

pub fn normalize_calls(f: &mut Feat, universe: &BTreeSet<String>) {
    f.calls
        .retain(|c| !matches!(c, Callee::Api(n) if is_noise(n)));
    for c in &mut f.calls {
        if let Callee::Api(n) = c
            && !universe.contains(n.as_str())
        {
            *c = Callee::Indirect;
        }
    }
}

#[must_use]
pub fn jaccard<T: Ord>(a: &BTreeSet<T>, b: &BTreeSet<T>) -> f32 {
    if a.is_empty() || b.is_empty() {
        return 0.0;
    }
    let inter = a.intersection(b).count();
    let union = a.len().saturating_add(b.len()).saturating_sub(inter);
    inter as f32 / union as f32
}
