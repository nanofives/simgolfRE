use md5::Digest as _;

fn hex(bytes: &[u8]) -> String {
    bytes.iter().map(|b| format!("{b:02x}")).collect()
}

#[must_use]
pub fn md5_hex(data: &[u8]) -> String {
    hex(&md5::Md5::digest(data))
}

#[must_use]
pub fn sha256_hex(data: &[u8]) -> String {
    hex(&sha2::Sha256::digest(data))
}

#[must_use]
pub fn differ(a: &[u8], b: &[u8]) -> usize {
    let common = a.iter().zip(b).filter(|(x, y)| x != y).count();
    common + a.len().abs_diff(b.len())
}

#[must_use]
pub fn first_difference(a: &[u8], b: &[u8]) -> Option<usize> {
    a.iter()
        .zip(b)
        .position(|(x, y)| x != y)
        .or_else(|| (a.len() != b.len()).then(|| a.len().min(b.len())))
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn the_digests_match_their_published_vectors() {
        assert_eq!(md5_hex(b""), "d41d8cd98f00b204e9800998ecf8427e");
        assert_eq!(md5_hex(b"abc"), "900150983cd24fb0d6963f7d28e17f72");
        assert_eq!(
            sha256_hex(b"abc"),
            "ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad"
        );
    }

    #[test]
    fn the_lint_crates_sha256_agrees_at_every_block_edge() {
        let data: Vec<u8> = (0u32..300)
            .map(|i| u8::try_from((i * 7 + 3) % 251).unwrap())
            .collect();
        for n in 0..data.len() {
            assert_eq!(
                msvc6_lint::sha256::sha256_hex(&data[..n]),
                sha256_hex(&data[..n]),
                "length {n}"
            );
        }
    }

    #[test]
    fn differ_counts_changed_bytes_and_the_length_gap() {
        assert_eq!(differ(b"abcd", b"abcd"), 0);
        assert_eq!(differ(b"abcd", b"abXd"), 1);
        assert_eq!(differ(b"abcd", b"ab"), 2);
        assert_eq!(differ(b"", b"xyz"), 3);
    }

    #[test]
    fn the_first_difference_is_where_the_bytes_part() {
        assert_eq!(first_difference(b"abc", b"abc"), None);
        assert_eq!(first_difference(b"abc", b"aXc"), Some(1));
        assert_eq!(first_difference(b"abc", b"abcd"), Some(3));
        assert_eq!(first_difference(b"", b"a"), Some(0));
    }
}
