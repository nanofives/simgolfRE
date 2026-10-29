use anyhow::{Context as _, Result, anyhow};
use serde_json::Value;
use std::path::Path;

pub const KEY: &str = "msvc6t";
pub const MAX_REPORT: u64 = 256 << 20;

#[derive(Debug, Clone, Default, PartialEq, Eq)]
pub struct Row {
    pub address: Option<u64>,
    pub recomp: Option<u64>,
    pub has_recomp: bool,
    pub name: String,
    pub symbol: Option<String>,
    pub obj: String,
    pub tu: String,
    pub size: Option<u64>,
    pub edit: Option<u64>,
    pub seat: Option<String>,
    pub line: Option<u64>,
    pub library: bool,
}

impl Row {
    #[must_use]
    pub fn rva(&self, base: u32) -> Option<u32> {
        rva_of(self.address, base)
    }

    #[must_use]
    pub fn our_rva(&self, base: u32) -> Option<u32> {
        rva_of(self.recomp, base)
    }

    #[must_use]
    pub fn size32(&self) -> Option<u32> {
        u32::try_from(self.size.unwrap_or(0)).ok()
    }

    #[must_use]
    pub fn symbol_or_name(&self) -> &str {
        self.symbol.as_deref().unwrap_or(&self.name)
    }
}

fn rva_of(va: Option<u64>, base: u32) -> Option<u32> {
    va.and_then(|v| v.checked_sub(u64::from(base)))
        .and_then(|r| u32::try_from(r).ok())
}

#[must_use]
pub fn hex(s: &str) -> Option<u64> {
    let t = s.trim();
    u64::from_str_radix(t.strip_prefix("0x").unwrap_or(t), 16).ok()
}

#[derive(Debug, Clone, Default)]
pub struct Report {
    pub file: Option<String>,
    pub rows: Vec<Row>,
}

impl Report {
    pub fn read(path: &Path, key: &str) -> Result<Self> {
        let bytes = msvc6_base::srcio::read_bounded(path, MAX_REPORT)
            .with_context(|| format!("reading the report {}", path.display()))?;
        Self::parse(&bytes, key, &path.display().to_string())
    }

    pub fn parse(bytes: &[u8], key: &str, what: &str) -> Result<Self> {
        let v: Value =
            serde_json::from_slice(bytes).with_context(|| format!("{what} is not JSON"))?;
        Self::from_value(&v, key, what)
    }

    pub fn from_value(v: &Value, key: &str, what: &str) -> Result<Self> {
        let data = v
            .get("data")
            .and_then(Value::as_array)
            .ok_or_else(|| anyhow!("{what} has no `data` array"))?;
        Ok(Self {
            file: v.get("file").and_then(Value::as_str).map(str::to_string),
            rows: data.iter().map(|r| row(r, key)).collect(),
        })
    }
}

fn row(r: &Value, key: &str) -> Row {
    let top = |k: &str| r.get(k).and_then(Value::as_str);
    let ext = r.get(key);
    let field = |k: &str| ext.and_then(|e| e.get(k));
    let text = |k: &str| field(k).and_then(Value::as_str).map(str::to_string);
    Row {
        address: top("address").and_then(hex),
        recomp: top("recomp").and_then(hex),
        has_recomp: top("recomp").is_some(),
        name: top("name").unwrap_or_default().to_string(),
        symbol: text("symbol"),
        obj: text("obj").unwrap_or_default(),
        tu: text("tu").unwrap_or_default(),
        size: field("size").and_then(Value::as_u64),
        edit: field("edit").and_then(Value::as_u64),
        seat: text("seat"),
        line: field("line").and_then(Value::as_u64),
        library: r.get("library").and_then(Value::as_bool) == Some(true),
    }
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn a_reccmp_row_and_its_extension_are_read() {
        let j = br#"{"file":"a.dll","format":1,"data":[
            {"address":"0x10001000","recomp":"0x10002000","name":"f","library":true,
             "x":{"symbol":"?f@@YAXXZ","obj":"A.obj","tu":"src/a.cpp","size":32,"edit":0,"seat":"seated","line":7}},
            {"address":"0x10003000","name":"g"},
            {"address":"zz","recomp":"0x1","name":"h","x":{"size":-1}}]}"#;
        let r = Report::parse(j, "x", "r.json").unwrap();
        assert_eq!(r.file.as_deref(), Some("a.dll"));
        let a = &r.rows[0];
        assert_eq!(a.rva(0x1000_0000), Some(0x1000));
        assert_eq!(a.our_rva(0x1000_0000), Some(0x2000));
        assert_eq!(a.symbol_or_name(), "?f@@YAXXZ");
        assert_eq!((a.obj.as_str(), a.tu.as_str()), ("A.obj", "src/a.cpp"));
        assert_eq!((a.size32(), a.edit, a.line), (Some(32), Some(0), Some(7)));
        assert!(a.library && a.has_recomp);
        let b = &r.rows[1];
        assert_eq!((b.recomp, b.has_recomp, b.size32()), (None, false, Some(0)));
        assert_eq!(b.symbol_or_name(), "g");
        assert_eq!(r.rows[2].address, None);
        assert_eq!(r.rows[2].size, None);
        assert_eq!(r.rows[0].rva(0x2000_0000), None);
        assert!(Report::parse(b"{}", "x", "r").is_err());
        assert!(Report::parse(b"[", "x", "r").is_err());
    }
}
