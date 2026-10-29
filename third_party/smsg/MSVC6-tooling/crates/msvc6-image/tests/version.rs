use msvc6_image::pe::Image;
use msvc6_image::res::{FileVersion, file_version, fixed_file_version};

#[test]
fn the_version_resource_names_the_tools_build() {
    let img = Image::parse(msvc6_fixture::sample::versioned(12, 0, 8966)).unwrap();
    let v = file_version(&img).unwrap().unwrap();
    assert_eq!(
        v,
        FileVersion {
            major: 12,
            minor: 0,
            build: 8966,
            revision: 0
        }
    );
    assert_eq!(v.to_string(), "12.00.8966");
    let plain = Image::parse(msvc6_fixture::sample::sample().pe.build()).unwrap();
    assert_eq!(file_version(&plain).unwrap(), None);
}

#[test]
fn a_block_without_the_key_or_the_signature_is_refused() {
    let mut b = msvc6_fixture::tables::version_info(6, 0, 8447, 0);
    assert_eq!(fixed_file_version(&b).unwrap().build, 8447);
    let sig = b.iter().position(|&x| x == 0xBD).unwrap();
    b[sig] = 0;
    assert!(
        fixed_file_version(&b)
            .unwrap_err()
            .to_string()
            .contains("signature")
    );
    b[6] = b'X';
    assert!(
        fixed_file_version(&b)
            .unwrap_err()
            .to_string()
            .contains("VS_VERSION_INFO")
    );
    assert!(fixed_file_version(&[0, 0]).is_err());
}
