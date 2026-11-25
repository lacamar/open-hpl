use crate::string;
use crate::types::{Color, Vector2f, Vector3f};

fn floats<const N: usize>(s: Option<&[u8]>) -> Option<[f32; N]> {
    let mut v = [0.0f32; N];
    string::floats_exact(s?, &mut v).then_some(v)
}

pub trait AttrRead {
    fn attr(&self, name: &[u8]) -> Option<&[u8]>;

    fn string(&self, name: &[u8], default: &[u8]) -> Vec<u8> {
        self.attr(name).unwrap_or(default).to_vec()
    }

    fn float(&self, name: &[u8], default: f32) -> f32 {
        self.attr(name).map_or(default, string::token_to_float)
    }

    fn int(&self, name: &[u8], default: i32) -> i32 {
        self.attr(name).map_or(default, string::token_to_int)
    }

    fn bool(&self, name: &[u8], default: bool) -> bool {
        self.attr(name).map_or(default, string::to_bool)
    }

    fn vec2(&self, name: &[u8], default: Vector2f) -> Vector2f {
        floats::<2>(self.attr(name)).map_or(default, |[x, y]| Vector2f { x, y })
    }

    fn vec3(&self, name: &[u8], default: Vector3f) -> Vector3f {
        floats::<3>(self.attr(name)).map_or(default, |[x, y, z]| Vector3f { x, y, z })
    }

    fn color(&self, name: &[u8], default: Color) -> Color {
        floats::<4>(self.attr(name)).map_or(default, |[r, g, b, a]| Color { r, g, b, a })
    }
}

#[cfg(test)]
mod tests {
    use super::*;

    struct One(&'static [u8]);

    impl AttrRead for One {
        fn attr(&self, name: &[u8]) -> Option<&[u8]> {
            (name == b"a").then_some(self.0)
        }
    }

    #[test]
    fn typed_readers_fall_back_to_default() {
        let e = One(b"1 2 3");
        assert_eq!(e.vec3(b"a", Vector3f::default()).z, 3.0);
        assert_eq!(
            e.vec3(
                b"b",
                Vector3f {
                    x: 9.0,
                    y: 0.0,
                    z: 0.0
                }
            )
            .x,
            9.0
        );
        assert_eq!(e.vec2(b"a", Vector2f::default()), Vector2f::default());
        assert_eq!(e.string(b"b", b"d"), b"d");
        assert_eq!(One(b"7").int(b"a", 0), 7);
        assert!(One(b"true").bool(b"a", false));
        assert_eq!(One(b"1.5").float(b"a", 0.0), 1.5);
        assert_eq!(One(b"1 2 3 4").color(b"a", Color::default()).a, 4.0);
    }
}
