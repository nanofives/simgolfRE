use crate::layout::{ROLES, Toolchain};
pub use msvc6_image::res::FileVersion;

pub trait VersionReader {
    fn file_version(&self, bytes: &[u8]) -> Option<FileVersion>;
}

#[derive(Debug, Clone, Copy, Default)]
pub struct ImageReader;

impl VersionReader for ImageReader {
    fn file_version(&self, bytes: &[u8]) -> Option<FileVersion> {
        let img = msvc6_image::pe::Image::parse(bytes.to_vec()).ok()?;
        msvc6_image::res::file_version(&img).ok().flatten()
    }
}

pub const KNOWN_BUILDS: [(u32, u32, &str); 8] = [
    (
        12,
        8168,
        "Visual C++ 6.0, no service pack (also SP1 and SP2)",
    ),
    (12, 8447, "Visual C++ 6.0 SP3"),
    (12, 8799, "Visual C++ 6.0 SP4"),
    (12, 8966, "Visual C++ 6.0 SP5"),
    (12, 9044, "Visual C++ 6.0 SP5 Processor Pack"),
    (12, 9782, "Visual C++ 6.0 SP6"),
    (
        6,
        8168,
        "Visual C++ 6.0 tools, no service pack (also SP1 and SP2)",
    ),
    (6, 8447, "Visual C++ 6.0 tools, SP3 and later"),
];

pub fn build_name(v: &FileVersion) -> Option<&'static str> {
    KNOWN_BUILDS
        .iter()
        .find(|(m, b, _)| *m == v.major && *b == v.build)
        .map(|(_, _, n)| *n)
}

#[derive(Debug, Clone, PartialEq, Eq)]
pub struct ToolVersion {
    pub role: &'static str,
    pub file: &'static str,
    pub path: Option<std::path::PathBuf>,
    pub version: Option<FileVersion>,
    pub md5: Option<String>,
    pub expected: Option<String>,
}

impl ToolVersion {
    pub fn verdict(&self) -> Option<bool> {
        let want = self.expected.as_deref()?;
        Some(self.version.is_some_and(|v| v.to_string() == want))
    }
}

pub fn detect(tc: &Toolchain, reader: &dyn VersionReader) -> Vec<ToolVersion> {
    ROLES
        .iter()
        .map(|(role, file, _)| {
            let path = tc.tool(file).ok();
            let bytes = path.as_ref().and_then(|p| std::fs::read(p).ok());
            ToolVersion {
                role,
                file,
                version: bytes.as_deref().and_then(|b| reader.file_version(b)),
                md5: bytes.as_deref().map(msvc6_base::digest::md5_hex),
                path,
                expected: tc.expect.get(*role).cloned(),
            }
        })
        .collect()
}

pub fn unknown_expectations(tc: &Toolchain) -> Vec<String> {
    tc.expect
        .keys()
        .filter(|k| !ROLES.iter().any(|(r, _, _)| r == k))
        .cloned()
        .collect()
}

pub fn compiler_build(found: &[ToolVersion]) -> Option<FileVersion> {
    found
        .iter()
        .find(|t| t.role == "c2")
        .and_then(|t| t.version)
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn builds_are_named_by_their_number() {
        let v = |major, build| FileVersion {
            major,
            minor: 0,
            build,
            revision: 0,
        };
        assert_eq!(build_name(&v(12, 8966)), Some("Visual C++ 6.0 SP5"));
        assert!(
            build_name(&v(12, 8168))
                .unwrap()
                .contains("no service pack")
        );
        assert!(build_name(&v(6, 8447)).unwrap().contains("SP3"));
        assert_eq!(build_name(&v(12, 1)), None);
        let t = ToolVersion {
            role: "c2",
            file: "C2.DLL",
            path: None,
            version: Some(v(12, 8966)),
            md5: None,
            expected: Some("12.00.8966".into()),
        };
        assert_eq!(t.verdict(), Some(true));
        let t = ToolVersion {
            expected: Some("12.00.8168".into()),
            ..t
        };
        assert_eq!(t.verdict(), Some(false));
    }

    #[test]
    fn the_image_reader_reads_a_tools_version_resource() {
        let tool = msvc6_fixture::sample::versioned(12, 0, 8168);
        let v = ImageReader.file_version(&tool).unwrap();
        assert_eq!(v.to_string(), "12.00.8168");
        assert_eq!(ImageReader.file_version(b"not a PE"), None);
    }
}
