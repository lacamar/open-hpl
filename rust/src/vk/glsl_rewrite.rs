//! `GlslToVulkan`: GLSL 1.20/1.30 compatibility pair to GLSL 4.50 for Vulkan. Every `std::regex`
//! of the C++ is a hand-written matcher over bytes with the same ECMAScript semantics.

use std::collections::{BTreeSet, HashMap};

pub const FIRST_SAMPLER_BINDING: i32 = 1;
const TEXCOORD_LOC: i32 = 0;
const FRONT_COLOR_LOC: i32 = 8;
const FRONT_SECONDARY_LOC: i32 = 9;
const FIRST_USER_VARYING_LOC: i32 = 10;

type Bytes = Vec<u8>;

#[derive(Default, Debug)]
pub struct Sampler {
    pub name: Bytes,
    pub ty: Bytes,
    pub binding: i32,
}

#[derive(Default, Debug)]
pub struct UniformInit {
    pub name: Bytes,
    pub values: Vec<f32>,
}

#[derive(Default, Debug)]
pub struct Program {
    pub vertex: Bytes,
    pub fragment: Bytes,
    pub samplers: Vec<Sampler>,
    pub inits: Vec<UniformInit>,
    pub attrib_mask: u32,
    pub frag_outputs: i32,
}

struct Decl {
    qualifier: &'static [u8],
    ty: Bytes,
    name: Bytes,
    array: Bytes,
    init: Bytes,
}

fn is_word(b: u8) -> bool {
    b.is_ascii_alphanumeric() || b == b'_'
}

fn is_space(b: u8) -> bool {
    matches!(b, b' ' | b'\t' | b'\n' | 0x0b | 0x0c | b'\r')
}

fn at(s: &[u8], i: usize) -> Option<u8> {
    s.get(i).copied()
}

fn skip_ws(s: &[u8], mut i: usize) -> usize {
    while at(s, i).is_some_and(is_space) {
        i += 1;
    }
    i
}

fn skip_word(s: &[u8], mut i: usize) -> usize {
    while at(s, i).is_some_and(is_word) {
        i += 1;
    }
    i
}

fn lit(s: &[u8], i: usize, l: &[u8]) -> bool {
    s.get(i..).is_some_and(|t| t.starts_with(l))
}

fn left_boundary(s: &[u8], i: usize) -> bool {
    i == 0 || !i.checked_sub(1).and_then(|j| at(s, j)).is_some_and(is_word)
}

fn trim(s: &[u8]) -> &[u8] {
    let t = |b: &u8| !matches!(b, b' ' | b'\t' | b'\r' | b'\n');
    let Some(a) = s.iter().position(t) else {
        return &[];
    };
    let b = s.iter().rposition(t).map_or(a, |b| b + 1);
    s.get(a..b).unwrap_or(&[])
}

fn strip_comments(s: &[u8]) -> Bytes {
    let mut out = Vec::with_capacity(s.len());
    let mut i = 0;
    while i < s.len() {
        let c = at(s, i);
        let n = at(s, i + 1);
        if c == Some(b'/') && n == Some(b'/') {
            while i < s.len() && at(s, i) != Some(b'\n') {
                i += 1;
            }
            out.push(b'\n');
        } else if c == Some(b'/') && n == Some(b'*') {
            i += 2;
            while i + 1 < s.len() && !(at(s, i) == Some(b'*') && at(s, i + 1) == Some(b'/')) {
                if at(s, i) == Some(b'\n') {
                    out.push(b'\n');
                }
                i += 1;
            }
            i += 1;
            out.push(b' ');
        } else if let Some(c) = c {
            out.push(c);
        }
        i += 1;
    }
    out
}

fn split_top_level(s: &[u8], sep: u8) -> Vec<&[u8]> {
    let mut v = Vec::new();
    let mut depth = 0i32;
    let mut start = 0;
    for (i, &c) in s.iter().enumerate() {
        if c == b'(' || c == b'[' {
            depth += 1;
        } else if c == b')' || c == b']' {
            depth -= 1;
        }
        if c == sep && depth == 0 {
            v.push(s.get(start..i).unwrap_or(&[]));
            start = i + 1;
        }
    }
    v.push(s.get(start..).unwrap_or(&[]));
    v
}

const QUALIFIER_PREFIX: [&[u8]; 8] = [
    b"invariant",
    b"centroid",
    b"flat",
    b"smooth",
    b"noperspective",
    b"highp",
    b"mediump",
    b"lowp",
];
const DECL_QUALIFIERS: [&[u8]; 3] = [b"uniform", b"varying", b"attribute"];
const PRECISION: [&[u8]; 3] = [b"highp", b"mediump", b"lowp"];

/// One of `words` at `i` followed by `\s+`; returns (word, index after the whitespace).
fn word_then_ws(s: &[u8], i: usize, words: &[&'static [u8]]) -> Option<(&'static [u8], usize)> {
    for &w in words {
        if lit(s, i, w) {
            let e = i + w.len();
            if at(s, e).is_some_and(is_space) {
                return Some((w, skip_ws(s, e)));
            }
        }
    }
    None
}

/// `\w+\s+` at `i`: (word end, index after the whitespace).
fn ident_then_ws(s: &[u8], i: usize) -> Option<(usize, usize)> {
    let e = skip_word(s, i);
    (e > i && at(s, e).is_some_and(is_space)).then(|| (e, skip_ws(s, e)))
}

/// `declRe`: qualifier, type, start of the remainder.
fn match_decl(s: &[u8]) -> Option<(&'static [u8], &[u8], usize)> {
    let mut i = skip_ws(s, 0);
    while let Some((_, n)) = word_then_ws(s, i, &QUALIFIER_PREFIX) {
        i = n;
    }
    let (q, j) = word_then_ws(s, i, &DECL_QUALIFIERS)?;
    if let Some((_, k)) = word_then_ws(s, j, &PRECISION)
        && let Some((e, r)) = ident_then_ws(s, k)
    {
        return Some((q, s.get(k..e)?, r));
    }
    let (e, r) = ident_then_ws(s, j)?;
    Some((q, s.get(j..e)?, r))
}

/// `nameRe`: (name, array, init) as raw slices.
fn match_name(s: &[u8]) -> Option<(&[u8], &[u8], &[u8])> {
    let a = skip_ws(s, 0);
    let b = skip_word(s, a);
    if b == a {
        return None;
    }
    let mut k = skip_ws(s, b);
    let mut array: &[u8] = &[];
    if at(s, k) == Some(b'[')
        && let Some(c) = s.get(k + 1..)?.iter().position(|&x| x == b']')
    {
        let end = k + 1 + c + 1;
        array = s.get(k..end)?;
        k = skip_ws(s, end);
    }
    match at(s, k) {
        None => Some((s.get(a..b)?, array, &[])),
        Some(b'=') => Some((s.get(a..b)?, array, s.get(k + 1..)?)),
        _ => None,
    }
}

fn parse_stage(src: &[u8]) -> Result<(Bytes, Vec<Decl>), Bytes> {
    let src = strip_comments(src);
    let mut body: Bytes = Vec::with_capacity(src.len() + 16);
    let mut pieces: Vec<&[u8]> = src.split(|&b| b == b'\n').collect();
    if pieces.last().is_some_and(|l| l.is_empty()) {
        pieces.pop();
    }
    for line in pieces {
        let t = trim(line);
        if t.starts_with(b"#version") || t.starts_with(b"#extension") {
            body.push(b'\n');
            continue;
        }
        body.extend_from_slice(line);
        body.push(b'\n');
    }

    let mut decls = Vec::new();
    let mut depth = 0i32;
    let mut start = 0usize;
    let mut i = 0usize;
    while i < body.len() {
        let c = at(&body, i);
        if c == Some(b'#')
            && depth == 0
            && (i == 0 || i.checked_sub(1).and_then(|j| at(&body, j)) == Some(b'\n'))
        {
            while i < body.len() && at(&body, i) != Some(b'\n') {
                i += 1;
            }
            start = i + 1;
            i += 1;
            continue;
        }
        if c == Some(b'{') {
            depth += 1;
        } else if c == Some(b'}') {
            depth -= 1;
            if depth == 0 {
                start = i + 1;
            }
        } else if c == Some(b';') && depth == 0 {
            let stmt = body.get(start..i).unwrap_or(&[]).to_vec();
            if let Some((q, ty, rest)) = match_decl(&stmt) {
                for part in split_top_level(stmt.get(rest..).unwrap_or(&[]), b',') {
                    let Some((name, array, init)) = match_name(part) else {
                        let mut e = b"can't parse declaration: ".to_vec();
                        e.extend_from_slice(trim(&stmt));
                        return Err(e);
                    };
                    decls.push(Decl {
                        qualifier: q,
                        ty: ty.to_vec(),
                        name: name.to_vec(),
                        array: array.to_vec(),
                        init: trim(init).to_vec(),
                    });
                }
                for b in body.iter_mut().skip(start).take(i + 1 - start) {
                    if *b != b'\n' {
                        *b = b' ';
                    }
                }
            }
            start = i + 1;
        }
        i += 1;
    }
    Ok((body, decls))
}

/// glibc `atoi`: `strtol` clamps to the `long` range, then the `int` conversion wraps.
pub fn atoi(s: &[u8]) -> i32 {
    let mut i = skip_ws(s, 0);
    let neg = at(s, i) == Some(b'-');
    if matches!(at(s, i), Some(b'-' | b'+')) {
        i += 1;
    }
    let mut v: i64 = 0;
    let mut over = false;
    while let Some(d) = at(s, i).filter(u8::is_ascii_digit) {
        match v
            .checked_mul(10)
            .and_then(|v| v.checked_add(i64::from(d - b'0')))
        {
            Some(n) => v = n,
            None => over = true,
        }
        i += 1;
    }
    let v = if over {
        if neg { i64::MIN } else { i64::MAX }
    } else if neg {
        -v
    } else {
        v
    };
    v as i32
}

fn location_count(ty: &[u8], array: &[u8]) -> i32 {
    let mut n = match ty {
        b"mat2" => 2,
        b"mat3" => 3,
        b"mat4" => 4,
        _ => 1,
    };
    if !array.is_empty() {
        // C++ `n *= atoi` overflowed (UB); wraps here
        n = i32::wrapping_mul(n, atoi(array.get(1..).unwrap_or(&[])));
    }
    n
}

fn uses(s: &[u8], word: &[u8]) -> bool {
    find_word(s, word).is_some()
}

/// `\bword\b`.
fn find_word(s: &[u8], word: &[u8]) -> Option<usize> {
    (0..s.len()).find(|&i| {
        lit(s, i, word) && left_boundary(s, i) && !at(s, i + word.len()).is_some_and(is_word)
    })
}

/// `regex_replace` with a matcher returning the end of a match starting at `i`.
fn replace_with(s: &mut Bytes, to: &[u8], m: impl Fn(&[u8], usize) -> Option<usize>) {
    let mut out = Vec::with_capacity(s.len());
    let mut i = 0;
    while i < s.len() {
        match m(s, i) {
            Some(e) if e > i => {
                out.extend_from_slice(to);
                i = e;
            }
            _ => {
                if let Some(&c) = s.get(i) {
                    out.push(c);
                }
                i += 1;
            }
        }
    }
    *s = out;
}

fn replace_word(s: &mut Bytes, from: &[u8], to: &[u8]) {
    replace_with(s, to, |s, i| {
        (lit(s, i, from) && left_boundary(s, i) && !at(s, i + from.len()).is_some_and(is_word))
            .then_some(i + from.len())
    });
}

fn replace_call(s: &mut Bytes, from: &[u8], to: &[u8]) {
    let mut t = to.to_vec();
    t.push(b'(');
    replace_with(s, &t, |s, i| {
        if !(lit(s, i, from) && left_boundary(s, i)) {
            return None;
        }
        let k = skip_ws(s, i + from.len());
        (at(s, k) == Some(b'(')).then_some(k + 1)
    });
}

/// `\bhead\s*\(\s*name\s*tail`
fn match_call_arg(s: &[u8], i: usize, head: &[u8], name: &[u8], tail: u8) -> Option<usize> {
    if !(lit(s, i, head) && left_boundary(s, i)) {
        return None;
    }
    let mut k = skip_ws(s, i + head.len());
    if at(s, k) != Some(b'(') {
        return None;
    }
    k = skip_ws(s, k + 1);
    if !lit(s, k, name) {
        return None;
    }
    k = skip_ws(s, k + name.len());
    (at(s, k) == Some(tail)).then_some(k + 1)
}

fn replace_main(s: &mut Bytes) {
    replace_with(s, b"void hpl_main(", |s, i| {
        if !(lit(s, i, b"void") && left_boundary(s, i)) {
            return None;
        }
        let a = i + 4;
        let k = skip_ws(s, a);
        if k == a || !lit(s, k, b"main") {
            return None;
        }
        let k = skip_ws(s, k + 4);
        (at(s, k) == Some(b'(')).then_some(k + 1)
    });
}

fn replace_ftransform(s: &mut Bytes) {
    replace_with(s, b"(gl_ModelViewProjectionMatrix * gl_Vertex)", |s, i| {
        if !(lit(s, i, b"ftransform") && left_boundary(s, i)) {
            return None;
        }
        let k = skip_ws(s, i + 10);
        if at(s, k) != Some(b'(') {
            return None;
        }
        let k = skip_ws(s, k + 1);
        (at(s, k) == Some(b')')).then_some(k + 1)
    });
}

/// Numbers of `[-+]?(\d+\.?\d*|\.\d+)([eE][-+]?\d+)?` in order.
fn numbers(s: &[u8]) -> Vec<f32> {
    let mut v = Vec::new();
    let mut i = 0;
    let digits = |mut k: usize| {
        while at(s, k).is_some_and(|b| b.is_ascii_digit()) {
            k += 1;
        }
        k
    };
    while i < s.len() {
        let mut k = i;
        if matches!(at(s, k), Some(b'-' | b'+')) {
            k += 1;
        }
        let mut e = None;
        if at(s, k).is_some_and(|b| b.is_ascii_digit()) {
            let mut m = digits(k);
            if at(s, m) == Some(b'.') {
                m += 1;
            }
            e = Some(digits(m));
        } else if at(s, k) == Some(b'.') && at(s, k + 1).is_some_and(|b| b.is_ascii_digit()) {
            e = Some(digits(k + 1));
        }
        let Some(mut e) = e else {
            i += 1;
            continue;
        };
        if matches!(at(s, e), Some(b'e' | b'E')) {
            let mut x = e + 1;
            if matches!(at(s, x), Some(b'-' | b'+')) {
                x += 1;
            }
            let d = digits(x);
            if d > x {
                e = d;
            }
        }
        let txt = s.get(i..e).unwrap_or(&[]);
        v.push(
            std::str::from_utf8(txt)
                .ok()
                .and_then(|t| t.parse::<f32>().ok())
                .unwrap_or(0.0),
        );
        i = e;
    }
    v
}

fn push_str(v: &mut Bytes, s: &str) {
    v.extend_from_slice(s.as_bytes());
}

struct BuiltinUniform(&'static str, &'static str, &'static str);
const BUILTIN_UNIFORMS: [BuiltinUniform; 4] = [
    BuiltinUniform(
        "gl_ModelViewProjectionMatrix",
        "hpl_ModelViewProjectionMatrix",
        "mat4",
    ),
    BuiltinUniform("gl_ModelViewMatrix", "hpl_ModelViewMatrix", "mat4"),
    BuiltinUniform("gl_ProjectionMatrix", "hpl_ProjectionMatrix", "mat4"),
    BuiltinUniform("gl_NormalMatrix", "hpl_NormalMatrix", "mat3"),
];

const BUILTIN_ATTRIBS: [(&str, &str, i32); 12] = [
    ("gl_Vertex", "vec4", 0),
    ("gl_Normal", "vec3", 1),
    ("gl_Color", "vec4", 2),
    ("gl_SecondaryColor", "vec4", 3),
    ("gl_MultiTexCoord0", "vec4", 4),
    ("gl_MultiTexCoord1", "vec4", 5),
    ("gl_MultiTexCoord2", "vec4", 6),
    ("gl_MultiTexCoord3", "vec4", 7),
    ("gl_MultiTexCoord4", "vec4", 8),
    ("gl_MultiTexCoord5", "vec4", 9),
    ("gl_MultiTexCoord6", "vec4", 10),
    ("gl_MultiTexCoord7", "vec4", 11),
];

const HELPERS: [(&str, &str); 5] = [
    (
        "hpl_shadow2DProj",
        "vec4 hpl_shadow2DProj(sampler2DShadow s, vec4 c){ return vec4(textureProj(s, c)); }\n",
    ),
    (
        "hpl_shadow2D",
        "vec4 hpl_shadow2D(sampler2DShadow s, vec3 c){ return vec4(texture(s, c)); }\n",
    ),
    (
        "hpl_textureRectProj",
        "vec4 hpl_textureRectProj(sampler2D s, vec3 c){ return texture(s, c.xy / (c.z * vec2(textureSize(s, 0)))); }\nvec4 hpl_textureRectProj(sampler2D s, vec4 c){ return texture(s, c.xy / (c.w * vec2(textureSize(s, 0)))); }\n",
    ),
    (
        "hpl_textureRect",
        "vec4 hpl_textureRect(sampler2D s, vec2 c){ return texture(s, c / vec2(textureSize(s, 0))); }\n",
    ),
    (
        "hpl_texelFetchRect",
        "vec4 hpl_texelFetchRect(sampler2D s, ivec2 p){ return texelFetch(s, p, 0); }\n",
    ),
];

const RENAMES: [(&str, &str); 15] = [
    ("texture1D", "texture"),
    ("texture2D", "texture"),
    ("texture3D", "texture"),
    ("textureCube", "texture"),
    ("texture2DArray", "texture"),
    ("texture1DLod", "textureLod"),
    ("texture2DLod", "textureLod"),
    ("texture3DLod", "textureLod"),
    ("textureCubeLod", "textureLod"),
    ("texture2DProj", "textureProj"),
    ("texture2DProjLod", "textureProjLod"),
    ("shadow2D", "hpl_shadow2D"),
    ("shadow2DProj", "hpl_shadow2DProj"),
    ("texture2DRect", "hpl_textureRect"),
    ("texture2DRectProj", "hpl_textureRectProj"),
];

const NATIVE: [&[u8]; 9] = [
    b"Position",
    b"FragCoord",
    b"FragDepth",
    b"FrontFacing",
    b"PointCoord",
    b"PointSize",
    b"VertexID",
    b"InstanceID",
    b"ClipDistance",
];

const VERTEX_MAIN: &str = "\nvoid main()\n{\n\thpl_main();\n\tgl_Position.z = (gl_Position.z + gl_Position.w) * 0.5;\n}\n";
const FRAGMENT_MAIN: &str = "\nvoid main()\n{\n\thpl_main();\n\tif(hpl_AlphaTest.x > 0.5)\n\t{\n\t\tfloat a = hpl_FragData[0].a, r = hpl_AlphaTest.z;\n\t\tint f = int(hpl_AlphaTest.y);\n\t\tbool p = f==1 ? a<r : f==2 ? a<=r : f==3 ? a>r : f==4 ? a>=r : f==5 ? a==r : f==6 ? a!=r : f==7;\n\t\tif(!p) discard;\n\t}\n}\n";

/// First unsupported `gl_*` identifier left in `body`.
fn unsupported_builtin(body: &[u8]) -> Option<&[u8]> {
    let mut i = 0;
    while i < body.len() {
        if lit(body, i, b"gl_") && left_boundary(body, i) {
            let e = skip_word(body, i + 3);
            if e > i + 3 {
                let name = body.get(i + 3..e)?;
                if !NATIVE.contains(&name) {
                    return Some(name);
                }
                i = e;
                continue;
            }
        }
        i += 1;
    }
    None
}

fn frag_outputs(body: &[u8], mut outputs: i32) -> i32 {
    const KEY: &[u8] = b"gl_FragData";
    let mut i = 0;
    while i < body.len() {
        if lit(body, i, KEY)
            && left_boundary(body, i)
            && let k = skip_ws(body, i + KEY.len())
            && at(body, k) == Some(b'[')
            && let a = skip_ws(body, k + 1)
            && let b = skip_word(body, a)
            && b > a
            && let c = skip_ws(body, b)
            && at(body, c) == Some(b']')
        {
            let idx = body.get(a..b).unwrap_or(&[]);
            let v = if idx.first().is_some_and(u8::is_ascii_digit) {
                atoi(idx)
            } else {
                7
            };
            // C++ `lIdx+1` overflowed (UB) at INT_MAX; wraps here
            let n = v.wrapping_add(1);
            if n > outputs {
                outputs = n;
            }
            i = c + 1;
            continue;
        }
        i += 1;
    }
    outputs
}

pub fn glsl_to_vulkan(vertex: &[u8], fragment: &[u8]) -> Result<Program, Bytes> {
    let mut out = Program::default();
    let (vb, vd) = parse_stage(vertex)?;
    let (fb, fd) = parse_stage(fragment)?;
    let mut bodies = [vb, fb];
    let decls = [vd, fd];

    let mut uniforms: Vec<&Decl> = Vec::new();
    let mut uniform_idx: HashMap<&[u8], usize> = HashMap::new();
    let mut sampler_names: HashMap<&[u8], i32> = HashMap::new();
    let mut rect: BTreeSet<&[u8]> = BTreeSet::new();
    let mut varyings: Vec<&Decl> = Vec::new();
    let mut varying_loc: HashMap<&[u8], i32> = HashMap::new();
    let mut next_loc = FIRST_USER_VARYING_LOC;

    for d in decls.iter().flatten() {
        if d.qualifier == b"uniform" {
            if d.ty.starts_with(b"sampler") {
                if sampler_names.contains_key(d.name.as_slice()) {
                    continue;
                }
                let ty: &[u8] = match d.ty.as_slice() {
                    b"sampler2DRect" => b"sampler2D",
                    b"sampler2DRectShadow" => b"sampler2DShadow",
                    t => t,
                };
                if d.ty.windows(4).any(|w| w == b"Rect") {
                    rect.insert(&d.name);
                }
                let binding =
                    FIRST_SAMPLER_BINDING + i32::try_from(out.samplers.len()).unwrap_or(0);
                sampler_names.insert(&d.name, binding);
                out.samplers.push(Sampler {
                    name: d.name.clone(),
                    ty: ty.to_vec(),
                    binding,
                });
                continue;
            }
            if let Some(&i) = uniform_idx.get(d.name.as_slice()) {
                if uniforms.get(i).is_some_and(|u| u.ty != d.ty) {
                    let mut e = b"uniform type mismatch: ".to_vec();
                    e.extend_from_slice(&d.name);
                    return Err(e);
                }
                continue;
            }
            uniform_idx.insert(&d.name, uniforms.len());
            uniforms.push(d);
            if !d.init.is_empty() {
                out.inits.push(UniformInit {
                    name: d.name.clone(),
                    values: numbers(&d.init),
                });
            }
        } else if d.qualifier == b"varying" && !varying_loc.contains_key(d.name.as_slice()) {
            varying_loc.insert(&d.name, next_loc);
            next_loc = next_loc.wrapping_add(location_count(&d.ty, &d.array));
            varyings.push(d);
        }
    }

    let mut block: Bytes = Vec::new();
    push_str(
        &mut block,
        "layout(std140, set=0, binding=0) uniform hpl_Uniforms\n{\n\tvec4 hpl_AlphaTest;\n",
    );
    let [first, second] = &bodies;
    for u in &BUILTIN_UNIFORMS {
        if uses(first, u.0.as_bytes())
            || uses(second, u.0.as_bytes())
            || (u.0 == "gl_ModelViewProjectionMatrix" && uses(first, b"ftransform"))
        {
            push_str(&mut block, &format!("\t{} {};\n", u.2, u.1));
        }
    }
    for u in &uniforms {
        block.push(b'\t');
        block.extend_from_slice(&u.ty);
        block.push(b' ');
        block.extend_from_slice(&u.name);
        block.extend_from_slice(&u.array);
        block.extend_from_slice(b";\n");
    }
    push_str(&mut block, "};\n");
    for s in &out.samplers {
        push_str(
            &mut block,
            &format!("layout(set=0, binding={}) uniform ", s.binding),
        );
        block.extend_from_slice(&s.ty);
        block.push(b' ');
        block.extend_from_slice(&s.name);
        block.extend_from_slice(b";\n");
    }

    for (idx, body) in bodies.iter_mut().enumerate() {
        let vtx = idx == 0;
        let mut head: Bytes = b"#version 450\n".to_vec();

        if vtx {
            replace_ftransform(body);
            for &(gl, ty, loc) in &BUILTIN_ATTRIBS {
                if !uses(body, gl.as_bytes()) {
                    continue;
                }
                let name = gl.get(3..).unwrap_or("");
                push_str(
                    &mut head,
                    &format!("layout(location={loc}) in {ty} hpl_{name};\n"),
                );
                replace_word(body, gl.as_bytes(), format!("hpl_{name}").as_bytes());
                out.attrib_mask |= 1u32 << loc;
            }
            for d in decls[0].iter().filter(|d| d.qualifier == b"attribute") {
                push_str(&mut head, "const ");
                head.extend_from_slice(&d.ty);
                head.push(b' ');
                head.extend_from_slice(&d.name);
                push_str(&mut head, " = ");
                head.extend_from_slice(&d.ty);
                push_str(&mut head, "(vec4(0.0, 0.0, 0.0, 1.0));\n");
            }
        }

        let io = if vtx { "out" } else { "in" };
        if uses(body, b"gl_TexCoord") {
            push_str(
                &mut head,
                &format!("layout(location={TEXCOORD_LOC}) {io} vec4 hpl_TexCoord[8];\n"),
            );
            replace_word(body, b"gl_TexCoord", b"hpl_TexCoord");
        }
        let color: &[u8] = if vtx { b"gl_FrontColor" } else { b"gl_Color" };
        let secondary: &[u8] = if vtx {
            b"gl_FrontSecondaryColor"
        } else {
            b"gl_SecondaryColor"
        };
        if uses(body, color) {
            push_str(
                &mut head,
                &format!("layout(location={FRONT_COLOR_LOC}) {io} vec4 hpl_FrontColor;\n"),
            );
            replace_word(body, color, b"hpl_FrontColor");
        }
        if uses(body, secondary) {
            push_str(
                &mut head,
                &format!(
                    "layout(location={FRONT_SECONDARY_LOC}) {io} vec4 hpl_FrontSecondaryColor;\n"
                ),
            );
            replace_word(body, secondary, b"hpl_FrontSecondaryColor");
        }
        for v in &varyings {
            let loc = varying_loc.get(v.name.as_slice()).copied().unwrap_or(0);
            push_str(&mut head, &format!("layout(location={loc}) {io} "));
            head.extend_from_slice(&v.ty);
            head.push(b' ');
            head.extend_from_slice(&v.name);
            head.extend_from_slice(&v.array);
            head.extend_from_slice(b";\n");
        }

        if !vtx {
            let mut outputs = 0;
            if uses(body, b"gl_FragColor") {
                outputs = 1;
                replace_word(body, b"gl_FragColor", b"hpl_FragData[0]");
            }
            outputs = frag_outputs(body, outputs);
            replace_word(body, b"gl_FragData", b"hpl_FragData");
            if outputs == 0 {
                outputs = 1;
            }
            out.frag_outputs = outputs;
            push_str(
                &mut head,
                &format!("layout(location=0) out vec4 hpl_FragData[{outputs}];\n"),
            );
        }

        for u in &BUILTIN_UNIFORMS {
            replace_word(body, u.0.as_bytes(), u.1.as_bytes());
        }

        for name in &rect {
            let mut to = b"hpl_texelFetchRect(".to_vec();
            to.extend_from_slice(name);
            to.push(b',');
            replace_with(body, &to, |s, i| {
                match_call_arg(s, i, b"texelFetch", name, b',')
            });
            let mut to = b"textureSize(".to_vec();
            to.extend_from_slice(name);
            to.extend_from_slice(b", 0)");
            replace_with(body, &to, |s, i| {
                match_call_arg(s, i, b"textureSize", name, b')')
            });
        }
        for (from, to) in RENAMES {
            replace_call(body, from.as_bytes(), to.as_bytes());
        }

        head.extend_from_slice(&block);
        for (key, text) in HELPERS {
            if body.windows(key.len()).any(|w| w == key.as_bytes()) {
                push_str(&mut head, text);
            }
        }

        replace_main(body);
        push_str(body, if vtx { VERTEX_MAIN } else { FRAGMENT_MAIN });

        if let Some(name) = unsupported_builtin(body) {
            let mut e = b"unsupported built-in gl_".to_vec();
            e.extend_from_slice(name);
            return Err(e);
        }
        head.extend_from_slice(body);
        if vtx {
            out.vertex = head;
        } else {
            out.fragment = head;
        }
    }
    Ok(out)
}
