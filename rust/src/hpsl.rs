//! HPSL to GLSL 1.20/1.30 source transpiler.
//!
//! Strings are raw bytes: the C++ original is byte-based (`std::string`, `isalnum`,
//! `std::regex<char>`), so no UTF-8 is assumed. Character classes follow the C locale.
#![allow(
    clippy::indexing_slicing,
    clippy::unwrap_used,
    clippy::expect_used,
    clippy::panic,
    clippy::unreachable,
    clippy::integer_division_remainder_used,
    clippy::string_slice,
    reason = "panic audit pending (open-hpl-smj.23)"
)]

use std::collections::HashMap;

#[derive(Clone, Copy, PartialEq, Eq, Debug)]
pub enum ShaderType {
    Vertex,
    Fragment,
    Other,
}

impl ShaderType {
    pub fn from_raw(raw: i32) -> Self {
        match raw {
            0 => Self::Vertex,
            1 => Self::Fragment,
            _ => Self::Other,
        }
    }
}

type Bytes = Vec<u8>;
type Res<T> = Result<T, Bytes>;

macro_rules! cat {
    ($($part:expr),* $(,)?) => {{
        let mut out: Bytes = Vec::new();
        $( out.extend_from_slice(AsRef::<[u8]>::as_ref(&$part)); )*
        out
    }};
}

struct Param {
    qualifier: Bytes,
    ty: Bytes,
    name: Bytes,
    semantic: i32,
}

fn type_name(ident: &[u8]) -> Option<&'static str> {
    Some(match ident {
        b"cVector4f" => "vec4",
        b"cVector3f" => "vec3",
        b"cVector2f" => "vec2",
        b"cVector4i" | b"cVector4l" | b"cVector4u" => "ivec4",
        b"cVector3i" | b"cVector3l" | b"cVector3u" => "ivec3",
        b"cVector2i" | b"cVector2l" | b"cVector2u" => "ivec2",
        b"cVector4b" => "bvec4",
        b"cVector3b" => "bvec3",
        b"cVector2b" => "bvec2",
        b"cMatrixf" | b"cMatrix4f" => "mat4",
        b"cMatrix3f" | b"cMatrix3x3f" => "mat3",
        b"cMatrix2x2f" | b"cMatrix2f" => "mat2",
        b"cMatrix2x4f" => "mat2x4",
        b"cTexture2D" => "sampler2D",
        b"cTextureCube" => "samplerCube",
        b"cTextureRect" => "sampler2DRect",
        b"cTexture3D" => "sampler3D",
        b"cTexture2DCmp" => "sampler2DShadow",
        b"cTexture2DArray" => "sampler2DArray",
        _ => return None,
    })
}

fn vertex_builtin(ident: &[u8]) -> Option<&'static str> {
    Some(match ident {
        b"vtx_vPosition" => "gl_Vertex",
        b"vtx_vColor" => "gl_Color",
        b"vtx_vNormal" => "gl_Normal",
        b"vtx_vTexCoord0" => "gl_MultiTexCoord0",
        b"vtx_vTexCoord1" | b"vtx_vTangent" => "gl_MultiTexCoord1",
        _ => return None,
    })
}

fn is_alnum(c: u8) -> bool {
    c.is_ascii_alphanumeric()
}

fn is_word(c: u8) -> bool {
    is_alnum(c) || c == b'_'
}

fn is_space(c: u8) -> bool {
    matches!(c, b' ' | b'\t' | b'\n' | 0x0b | 0x0c | b'\r')
}

fn is_trim(c: u8) -> bool {
    matches!(c, b' ' | b'\t' | b'\r' | b'\n')
}

fn find(hay: &[u8], needle: &[u8], from: usize) -> Option<usize> {
    if from > hay.len() || hay.len() - from < needle.len() {
        return None;
    }
    (from..=hay.len() - needle.len()).find(|&i| &hay[i..i + needle.len()] == needle)
}

fn find_byte(hay: &[u8], b: u8, from: usize) -> Option<usize> {
    hay.get(from..)?
        .iter()
        .position(|&c| c == b)
        .map(|i| i + from)
}

fn rfind(hay: &[u8], needle: &[u8], max_start: usize) -> Option<usize> {
    if hay.len() < needle.len() {
        return None;
    }
    let top = max_start.min(hay.len() - needle.len());
    (0..=top)
        .rev()
        .find(|&i| &hay[i..i + needle.len()] == needle)
}

// std::string::substr with the count clamped; pos never exceeds len at the call sites
fn substr(s: &[u8], pos: usize, len: usize) -> &[u8] {
    let pos = pos.min(s.len());
    &s[pos..pos + len.min(s.len() - pos)]
}

fn trim(s: &[u8]) -> &[u8] {
    let Some(b) = s.iter().position(|&c| !is_trim(c)) else {
        return &[];
    };
    let e = s.iter().rposition(|&c| !is_trim(c)).unwrap_or(b);
    &s[b..=e]
}

fn replace_all(src: &[u8], from: &[u8], to: &[u8]) -> Bytes {
    let mut out = Vec::with_capacity(src.len());
    let mut i = 0;
    while i < src.len() {
        if src[i..].starts_with(from) {
            out.extend_from_slice(to);
            i += from.len();
        } else {
            out.push(src[i]);
            i += 1;
        }
    }
    out
}

fn replace_identifiers<'a>(src: &[u8], map: impl Fn(&[u8]) -> Option<&'a str>) -> Bytes {
    let mut out = Vec::with_capacity(src.len());
    let mut i = 0;
    while i < src.len() {
        let mut j = i;
        while j < src.len() && is_word(src[j]) {
            j += 1;
        }
        if j == i {
            out.push(src[i]);
            i += 1;
            continue;
        }
        match map(&src[i..j]) {
            Some(to) => out.extend_from_slice(to.as_bytes()),
            None => out.extend_from_slice(&src[i..j]),
        }
        i = j;
    }
    out
}

fn replace_identifier(src: &[u8], from: &[u8], to: &[u8]) -> Bytes {
    let mut out = Vec::with_capacity(src.len());
    let mut i = 0;
    while i < src.len() {
        let mut j = i;
        while j < src.len() && is_word(src[j]) {
            j += 1;
        }
        if j == i {
            out.push(src[i]);
            i += 1;
            continue;
        }
        out.extend_from_slice(if &src[i..j] == from { to } else { &src[i..j] });
        i = j;
    }
    out
}

fn strip_line_comments(src: &[u8]) -> Bytes {
    let mut out = Vec::with_capacity(src.len());
    let mut in_comment = false;
    let mut i = 0;
    while i < src.len() {
        let c = src[i];
        if in_comment {
            if c == b'\n' {
                in_comment = false;
                out.push(c);
            }
        } else if c == b'/' && src.get(i + 1) == Some(&b'/') {
            in_comment = true;
            i += 1;
        } else {
            out.push(c);
        }
        i += 1;
    }
    out
}

fn split_params(list: &[u8]) -> Vec<&[u8]> {
    let mut out: Vec<&[u8]> = list.split(|&c| c == b',').collect();
    if trim(out.last().copied().unwrap_or(&[])).is_empty() {
        out.pop();
    }
    out
}

// glibc atoi: strtol saturating at 64 bits, then truncated to int; stops at NUL like c_str()
fn atoi(s: &[u8]) -> i32 {
    let s = s.split(|&c| c == 0).next().unwrap_or(&[]);
    let mut i = 0;
    while i < s.len() && is_space(s[i]) {
        i += 1;
    }
    let neg = match s.get(i) {
        Some(b'-') => {
            i += 1;
            true
        }
        Some(b'+') => {
            i += 1;
            false
        }
        _ => false,
    };
    let mut v: i64 = 0;
    let mut over = false;
    while let Some(d) = s.get(i).filter(|c| c.is_ascii_digit()) {
        let d = i64::from(d - b'0');
        match v.checked_mul(10).and_then(|x| x.checked_add(d)) {
            Some(n) if !over => v = n,
            _ => over = true,
        }
        i += 1;
    }
    let r = if over {
        if neg { i64::MIN } else { i64::MAX }
    } else if neg {
        -v
    } else {
        v
    };
    r as i32
}

fn skip_while(s: &[u8], mut p: usize, f: impl Fn(u8) -> bool) -> usize {
    while p < s.len() && f(s[p]) {
        p += 1;
    }
    p
}

fn skip_ws(s: &[u8], p: usize) -> usize {
    skip_while(s, p, is_space)
}

fn skip_ws1(s: &[u8], p: usize) -> Option<usize> {
    let q = skip_ws(s, p);
    (q > p).then_some(q)
}

fn skip_run1(s: &[u8], p: usize, f: impl Fn(u8) -> bool) -> Option<usize> {
    let q = skip_while(s, p, f);
    (q > p).then_some(q)
}

fn expect(s: &[u8], p: usize, lit: &[u8]) -> Option<usize> {
    s.get(p..)?.starts_with(lit).then(|| p + lit.len())
}

// "(in|out)\s+(\S+)\s+(\S+)" matched against the whole string
fn match_decl(s: &[u8]) -> Option<(&[u8], &[u8], &[u8])> {
    for qual in [&b"in"[..], &b"out"[..]] {
        let Some(p) = expect(s, 0, qual) else {
            continue;
        };
        let Some(p) = skip_ws1(s, p) else { continue };
        let Some(t1) = skip_run1(s, p, |c| !is_space(c)) else {
            continue;
        };
        let Some(p2) = skip_ws1(s, t1) else { continue };
        let Some(t2) = skip_run1(s, p2, |c| !is_space(c)) else {
            continue;
        };
        if t2 == s.len() {
            return Some((qual, &s[p..t1], &s[p2..t2]));
        }
    }
    None
}

fn parse_param(raw: &[u8]) -> Res<Param> {
    let mut param = trim(raw);
    let mut semantic = -1;
    if let Some(colon) = param.iter().position(|&c| c == b':') {
        semantic = atoi(trim(&param[colon + 1..]));
        param = trim(&param[..colon]);
    }
    match match_decl(param) {
        Some((q, t, n)) => Ok(Param {
            qualifier: q.to_vec(),
            ty: t.to_vec(),
            name: n.to_vec(),
            semantic,
        }),
        None => Err(cat!(
            b"Couldn't parse parameter '",
            raw,
            b"' (expected '(in|out) TYPE NAME [: N]')"
        )),
    }
}

// "uniform\s+cTextureBuffer\s+\w+\s*(:\s*\d+\s*)?;" -> end of match
fn match_texture_buffer(s: &[u8], i: usize) -> Option<usize> {
    let p = expect(s, i, b"uniform")?;
    let p = skip_ws1(s, p)?;
    let p = expect(s, p, b"cTextureBuffer")?;
    let p = skip_ws1(s, p)?;
    let p = skip_run1(s, p, is_word)?;
    let p = skip_ws(s, p);
    if s.get(p) == Some(&b':') {
        let q = skip_ws(s, p + 1);
        if let Some(q) = skip_run1(s, q, |c| c.is_ascii_digit()) {
            let q = skip_ws(s, q);
            if s.get(q) == Some(&b';') {
                return Some(q + 1);
            }
        }
    }
    (s.get(p) == Some(&b';')).then_some(p + 1)
}

// "(uniform\s+\S+\s+\w+)\s*:\s*\d+(\s*;)" -> (end of group 1, start of group 2, end)
fn match_binding(s: &[u8], i: usize) -> Option<(usize, usize, usize)> {
    let p = expect(s, i, b"uniform")?;
    let p = skip_ws1(s, p)?;
    let p = skip_run1(s, p, |c| !is_space(c))?;
    let p = skip_ws1(s, p)?;
    let g1 = skip_run1(s, p, is_word)?;
    let p = skip_ws(s, g1);
    let p = expect(s, p, b":")?;
    let p = skip_ws(s, p);
    let g2 = skip_run1(s, p, |c| c.is_ascii_digit())?;
    let end = expect(s, skip_ws(s, g2), b";")?;
    Some((g1, g2, end))
}

fn strip_uniform_binding_indices(src: &[u8]) -> Bytes {
    let mut mid = Vec::with_capacity(src.len());
    let mut i = 0;
    while i < src.len() {
        match match_texture_buffer(src, i) {
            Some(end) => i = end,
            None => {
                mid.push(src[i]);
                i += 1;
            }
        }
    }
    let mut out = Vec::with_capacity(mid.len());
    let mut i = 0;
    while i < mid.len() {
        match match_binding(&mid, i) {
            Some((g1, g2, end)) => {
                out.extend_from_slice(&mid[i..g1]);
                out.extend_from_slice(&mid[g2..end]);
                i = end;
            }
            None => {
                out.push(mid[i]);
                i += 1;
            }
        }
    }
    out
}

fn flatten_buffer_body(body: &[u8]) -> Bytes {
    let mut out = Vec::with_capacity(body.len());
    for (n, line) in body.split(|&c| c == b'\n').enumerate() {
        if n > 0 {
            out.push(b'\n');
        }
        let t = trim(line);
        if t.is_empty() || t.starts_with(b"//") || t[0] == b'#' {
            out.extend_from_slice(line);
        } else {
            let indent = line
                .iter()
                .position(|&c| c != b' ' && c != b'\t')
                .unwrap_or(0);
            out.extend_from_slice(&line[..indent]);
            out.extend_from_slice(b"uniform ");
            out.extend_from_slice(&line[indent..]);
        }
    }
    out
}

fn flatten_constant_buffers(src: &[u8]) -> Res<Bytes> {
    let mut result = Vec::with_capacity(src.len());
    let mut cursor = 0;
    loop {
        let mut found = None;
        let mut from = cursor;
        while let Some(cand) = find(src, b"cBuffer", from) {
            let after = cand + 7;
            let ok = (cand == 0 || !is_word(src[cand - 1]))
                && (after >= src.len() || !is_word(src[after]));
            if ok {
                found = Some(cand);
                break;
            }
            from = after;
        }
        let Some(found) = found else {
            result.extend_from_slice(&src[cursor.min(src.len())..]);
            return Ok(result);
        };
        result.extend_from_slice(&src[cursor..found]);

        let Some(open) = find_byte(src, b'{', found) else {
            return Err(b"Found 'cBuffer' with no following '{'".to_vec());
        };
        let mut depth = 1;
        let mut i = open + 1;
        while i < src.len() && depth > 0 {
            match src[i] {
                b'{' => depth += 1,
                b'}' => depth -= 1,
                _ => {}
            }
            i += 1;
        }
        if depth != 0 {
            return Err(b"Unterminated 'cBuffer { ... }' block (unbalanced braces)".to_vec());
        }
        let close = i - 1;
        let semi = skip_ws(src, close + 1);
        if src.get(semi) != Some(&b';') {
            return Err(
                b"'cBuffer { ... }' block not terminated with ';' after the closing brace".to_vec(),
            );
        }
        result.extend_from_slice(&flatten_buffer_body(&src[open + 1..close]));
        cursor = semi + 1;
    }
}

fn split_top_level_args(list: &[u8]) -> Vec<Bytes> {
    let mut out = Vec::new();
    let mut start = 0;
    let mut depth = 0i32;
    for (i, &c) in list.iter().enumerate() {
        match c {
            b'(' => depth += 1,
            b')' => depth -= 1,
            _ => {}
        }
        if c == b',' && depth == 0 {
            out.push(trim(&list[start..i]).to_vec());
            start = i + 1;
        }
    }
    let last = trim(&list[start..]);
    if !last.is_empty() {
        out.push(last.to_vec());
    }
    out
}

fn rewrite_call(src: &[u8], name: &str, mut fmt: impl FnMut(&[Bytes]) -> Res<Bytes>) -> Res<Bytes> {
    let mut s = src.to_vec();
    let pat = cat!(name, b"(");
    loop {
        let mut pos = None;
        let mut from = s.len();
        while from != 0 {
            let Some(f) = rfind(&s, &pat, from - 1) else {
                break;
            };
            if f == 0 || !is_word(s[f - 1]) {
                pos = Some(f);
                break;
            }
            from = f;
        }
        let Some(pos) = pos else { return Ok(s) };

        let args_start = pos + pat.len();
        let mut depth = 1;
        let mut end = args_start;
        while end < s.len() && depth > 0 {
            match s[end] {
                b'(' => depth += 1,
                b')' => depth -= 1,
                _ => {}
            }
            end += 1;
        }
        if depth != 0 {
            return Err(cat!(
                b"Unterminated '",
                name,
                b"(' call (unbalanced parens)"
            ));
        }
        end -= 1;

        let args = split_top_level_args(&s[args_start..end]);
        let repl = fmt(&args)?;
        s = cat!(&s[..pos], repl, &s[end + 1..]);
    }
}

type Samplers = HashMap<Bytes, &'static str>;

// "(?:uniform\s+|[(,]\s*)(sampler2D|samplerCube|sampler2DRect|sampler3D)\s+(\w+)(?=\s*[;,)])"
fn match_sampler_decl(s: &[u8], i: usize) -> Option<(&'static str, usize, usize)> {
    let p = if let Some(p) = expect(s, i, b"uniform") {
        skip_ws1(s, p)?
    } else if matches!(s.get(i), Some(b'(' | b',')) {
        skip_ws(s, i + 1)
    } else {
        return None;
    };
    for (ty, func) in [
        ("sampler2D", "texture2D"),
        ("samplerCube", "textureCube"),
        ("sampler2DRect", "texture2DRect"),
        ("sampler3D", "texture3D"),
    ] {
        let Some(q) = expect(s, p, ty.as_bytes()) else {
            continue;
        };
        let Some(n0) = skip_ws1(s, q) else { continue };
        let Some(n1) = skip_run1(s, n0, is_word) else {
            continue;
        };
        if matches!(s.get(skip_ws(s, n1)), Some(b';' | b',' | b')')) {
            return Some((func, n0, n1));
        }
    }
    None
}

fn collect_sampler_types(src: &[u8]) -> Samplers {
    let mut out = Samplers::new();
    let mut i = 0;
    while i < src.len() {
        match match_sampler_decl(src, i) {
            Some((func, n0, n1)) => {
                out.insert(src[n0..n1].to_vec(), func);
                i = n1;
            }
            None => i += 1,
        }
    }
    out
}

// "uniform\s+mat4\s+NAME\s*;\n?" removed everywhere
fn remove_matrix_decls(src: &[u8], name: &str) -> Option<Bytes> {
    let m = |i: usize| -> Option<usize> {
        let p = expect(src, i, b"uniform")?;
        let p = skip_ws1(src, p)?;
        let p = expect(src, p, b"mat4")?;
        let p = skip_ws1(src, p)?;
        let p = expect(src, p, name.as_bytes())?;
        let p = expect(src, skip_ws(src, p), b";")?;
        Some(expect(src, p, b"\n").unwrap_or(p))
    };
    let mut out = Vec::with_capacity(src.len());
    let mut found = false;
    let mut i = 0;
    while i < src.len() {
        match m(i) {
            Some(end) => {
                found = true;
                i = end;
            }
            None => {
                out.push(src[i]);
                i += 1;
            }
        }
    }
    found.then_some(out)
}

fn substitute_fixed_function_matrix_uniforms(src: &[u8]) -> Bytes {
    const BUILTINS: [(&str, &str); 4] = [
        ("a_mtxModelViewProjection", "gl_ModelViewProjectionMatrix"),
        ("a_mtxModelView", "gl_ModelViewMatrix"),
        ("a_mtxProjection", "gl_ProjectionMatrix"),
        ("a_mtxNormal", "gl_NormalMatrix"),
    ];
    let mut out = src.to_vec();
    for (name, builtin) in BUILTINS {
        if let Some(stripped) = remove_matrix_decls(&out, name) {
            out = replace_identifier(&stripped, name.as_bytes(), builtin.as_bytes());
        }
    }
    replace_all(&out, b"ToMatrix3f(gl_NormalMatrix)", b"gl_NormalMatrix")
}

fn rewrite_mul(src: &[u8]) -> Res<Bytes> {
    rewrite_call(src, "mul", |args| {
        if args.len() != 2 {
            return Err(cat!(
                b"mul() with ",
                args.len().to_string(),
                b" argument(s) is not supported (only the 2-argument matrix/vector form is)"
            ));
        }
        Ok(cat!(b"(", args[0], b" * ", args[1], b")"))
    })
}

fn rewrite_sample(src: &[u8], needs_offset: &mut bool) -> Res<Bytes> {
    let samplers = collect_sampler_types(src);
    rewrite_call(src, "sample", |args| {
        if args.len() == 3 {
            *needs_offset = true;
            return Ok(cat!(
                b"textureOffset(",
                args[0],
                b", ",
                args[1],
                b", ",
                args[2],
                b")"
            ));
        }
        if args.len() != 2 {
            return Err(cat!(
                b"sample() with ",
                args.len().to_string(),
                b" argument(s) is not supported (only the 2-argument texture/uv form is)"
            ));
        }
        let Some(func) = samplers.get(&args[0]) else {
            return Err(cat!(
                b"sample() references '",
                args[0],
                b"', which isn't a declared uniform texture (or is a texture type this transpiler doesn't map - see gmapTypeNames)"
            ));
        };
        Ok(cat!(func, b"(", args[0], b", ", args[1], b")"))
    })
}

fn rewrite_sample_cmp(src: &[u8]) -> Res<Bytes> {
    rewrite_call(src, "sampleCmp", |args| {
        if args.len() != 3 {
            return Err(cat!(
                b"sampleCmp() with ",
                args.len().to_string(),
                b" argument(s) is not supported (only the 3-argument texture/uv/refZ form is)"
            ));
        }
        Ok(cat!(
            b"shadow2D(",
            args[0],
            b", vec3(",
            args[1],
            b", ",
            args[2],
            b")).x"
        ))
    })
}

fn rewrite_lod_grad(
    src: &[u8],
    name: &str,
    glsl_name: &str,
    n_args: usize,
    fired: &mut bool,
) -> Res<Bytes> {
    rewrite_call(src, name, |args| {
        let offset = name == "sampleLod" && args.len() == n_args + 1;
        if args.len() != n_args && !offset {
            return Err(cat!(
                name,
                b"() with ",
                args.len().to_string(),
                b" argument(s) is not supported"
            ));
        }
        let mut repl = cat!(glsl_name, if offset { "Offset(" } else { "(" });
        for (i, a) in args.iter().enumerate() {
            if i > 0 {
                repl.extend_from_slice(b", ");
            }
            repl.extend_from_slice(a);
        }
        repl.push(b')');
        *fired = true;
        Ok(repl)
    })
}

fn rewrite_gather(src: &[u8], name: &str, comp: &str, fired: &mut bool) -> Res<Bytes> {
    rewrite_call(src, name, |args| {
        if args.len() != 2 && args.len() != 3 {
            return Err(cat!(
                name,
                b"() with ",
                args.len().to_string(),
                b" argument(s) is not supported"
            ));
        }
        let offset = if args.len() == 3 {
            cat!(
                b" + vec2(",
                args[2],
                b") / vec2(textureSize(",
                args[0],
                b", 0))"
            )
        } else {
            Vec::new()
        };
        *fired = true;
        Ok(cat!(
            b"textureGather(",
            args[0],
            b", ",
            args[1],
            offset,
            comp,
            b")"
        ))
    })
}

fn rewrite_load(src: &[u8], fired: &mut bool) -> Res<Bytes> {
    let samplers = collect_sampler_types(src);
    rewrite_call(src, "load", |args| {
        if args.len() != 2 && args.len() != 3 {
            return Err(cat!(
                b"load() with ",
                args.len().to_string(),
                b" argument(s) is not supported (only texture/coords[/mipLevel] is)"
            ));
        }
        if samplers.get(&args[0]) != Some(&"texture2D") {
            return Err(cat!(
                b"load() references '",
                args[0],
                b"', which isn't a declared uniform sampler2D (load() is only supported on sampler2D - see HpslTranspiler.cpp)"
            ));
        }
        *fired = true;
        let mip: &[u8] = if args.len() == 3 { &args[2] } else { b"0" };
        Ok(cat!(
            b"texelFetch(",
            args[0],
            b", ",
            args[1],
            b", ",
            mip,
            b")"
        ))
    })
}

fn needs_int_ops(body: &[u8]) -> bool {
    (0..body.len()).any(|i| {
        body[i] == b'%'
            || body[i..].starts_with(b">>")
            || body[i..].starts_with(b"<<")
            || (body[i..].starts_with(b"isnan(") && (i == 0 || !is_word(body[i - 1])))
    })
}

fn insert_at(s: &mut Bytes, pos: usize, text: &str) {
    s.splice(pos..pos, text.bytes());
}

pub fn transpile(hpsl: &[u8], ty: ShaderType) -> Res<Bytes> {
    let mut src = replace_all(
        hpsl,
        b"return pow(avsRGB.rgb, cVector3f(2.2));",
        b"return SRGBToLinear(avsRGB.rgb);",
    );
    src = replace_all(
        &src,
        b"return pow(avsRGB.rgba, cVector4f(2.2));",
        b"return SRGBToLinear(avsRGB);",
    );
    if find(&src, b"GammaToLinearCorrection(", 0).is_some() {
        let a = find(&src, b"cVector3f GammaToLinearCorrection(", 0);
        let b = find(&src, b"void main", 0);
        let first = match (a, b) {
            (Some(x), Some(y)) => Some(x.min(y)),
            (x, None) => x,
            (None, y) => y,
        };
        if let Some(first) = first {
            insert_at(
                &mut src,
                first,
                "cVector3f SRGBToLinear(in cVector3f v) { return mix(v / 12.92, pow((v + 0.055) / 1.055, cVector3f(2.4)), step(cVector3f(0.04045), v)); }\n\
                 cVector4f SRGBToLinear(in cVector4f v) { return mix(v / 12.92, pow((v + 0.055) / 1.055, cVector4f(2.4)), step(cVector4f(0.04045), v)); }\n",
            );
        }
    }
    if find(&src, b"GammaToLinearCorrection(", 0).is_some()
        && find(&src, b"GammaToLinearCorrection(in ", 0).is_none()
        && let Some(main) = find(&src, b"void main", 0)
    {
        insert_at(
            &mut src,
            main,
            "cVector3f GammaToLinearCorrection(in cVector3f v) { return SRGBToLinear(v); }\n\
             cVector4f GammaToLinearCorrection(in cVector4f v) { return SRGBToLinear(v); }\n",
        );
    }
    src = replace_identifiers(&src, type_name);
    src = flatten_constant_buffers(&src)?;
    src = substitute_fixed_function_matrix_uniforms(&src);
    src = strip_uniform_binding_indices(&src);

    src = rewrite_mul(&src)?;
    src = rewrite_sample_cmp(&src)?;
    let mut needs_texel_fetch = false;
    src = rewrite_sample(&src, &mut needs_texel_fetch)?;
    src = rewrite_load(&src, &mut needs_texel_fetch)?;
    let mut needs_grad = false;
    src = rewrite_lod_grad(&src, "sampleGrad", "textureGrad", 4, &mut needs_grad)?;
    src = rewrite_lod_grad(&src, "sampleLod", "textureLod", 3, &mut needs_grad)?;
    let mut needs_gather = false;
    src = rewrite_gather(&src, "sampleGatherAlpha", ", 3", &mut needs_gather)?;
    src = rewrite_gather(&src, "sampleGatherGreen", ", 1", &mut needs_gather)?;
    src = rewrite_gather(&src, "sampleGather", "", &mut needs_gather)?;
    needs_texel_fetch |= needs_grad || needs_gather;

    let Some(main_pos) = find(&src, b"main", 0) else {
        return Err(b"No 'main' function found".to_vec());
    };
    let Some(params_start) = find_byte(&src, b'(', main_pos) else {
        return Err(b"No '(' found after 'main'".to_vec());
    };

    let mut depth = 1;
    let mut params_end = params_start + 1;
    while params_end < src.len() && depth > 0 {
        match src[params_end] {
            b'(' => depth += 1,
            b')' => depth -= 1,
            _ => {}
        }
        params_end += 1;
    }
    params_end -= 1;

    // unbalanced input makes this length wrap, as size_t does in the original; substr clamps it
    let param_len = params_end.wrapping_sub(params_start).wrapping_sub(1);
    let param_list = strip_line_comments(substr(&src, params_start + 1, param_len));

    let Some(body_start) = find_byte(&src, b'{', params_end) else {
        return Err(b"No '{' found after main()'s parameter list".to_vec());
    };
    depth = 1;
    let mut body_end = body_start + 1;
    while body_end < src.len() && depth > 0 {
        match src[body_end] {
            b'{' => depth += 1,
            b'}' => depth -= 1,
            _ => {}
        }
        body_end += 1;
    }
    body_end -= 1;
    let body_len = body_end.wrapping_sub(body_start).wrapping_sub(1);
    let mut body = substr(&src, body_start + 1, body_len).to_vec();

    let mut header = src[..main_pos].to_vec();
    if let Some(t) = header.iter().rposition(|&c| !is_trim(c)) {
        let trimmed = &header[..=t];
        let n = trimmed.len();
        if n >= 4 && trimmed.ends_with(b"void") && (n == 4 || !is_alnum(trimmed[n - 5])) {
            header = trimmed[..n - 4].to_vec();
        }
    }

    let mut globals: Bytes = if ty == ShaderType::Vertex {
        b"invariant gl_Position;\n".to_vec()
    } else {
        Vec::new()
    };
    let mut body_subs: Vec<(Bytes, Bytes)> = Vec::new();
    let mut needs_frag_data = false;
    let mut frag_data_pad: Bytes = Vec::new();

    for raw in split_params(&param_list) {
        let p = parse_param(raw)?;
        let out = p.qualifier == b"out";
        let is_in = p.qualifier == b"in";
        if p.name == b"px_vPosition" && p.semantic == -1 && ty == ShaderType::Vertex && out {
            body_subs.push((p.name, b"gl_Position".to_vec()));
        } else if p.name == b"px_vPosition" && ty == ShaderType::Fragment && is_in {
            body_subs.push((p.name, b"gl_FragCoord".to_vec()));
        } else if ty == ShaderType::Vertex && is_in {
            match vertex_builtin(&p.name) {
                Some(b) => body_subs.push((p.name, b.as_bytes().to_vec())),
                None => globals.extend(cat!(b"attribute ", p.ty, b" ", p.name, b";\n")),
            }
        } else if ty == ShaderType::Fragment && p.semantic >= 0 && out {
            let frag_data = cat!(b"gl_FragData[", p.semantic.to_string(), b"]");
            let ty_name = type_name(&p.ty).map_or(p.ty.clone(), |t| t.as_bytes().to_vec());
            let pad = match &ty_name[..] {
                b"float" => Some(", 0.0, 0.0, 0.0"),
                b"vec2" => Some(", 0.0, 0.0"),
                b"vec3" => Some(", 0.0"),
                _ => None,
            };
            match pad {
                Some(pad) => {
                    globals.extend(cat!(ty_name, b" ", p.name, b";\n"));
                    frag_data_pad.extend(cat!(b"\t", frag_data, b" = vec4(", p.name, pad, b");\n"));
                }
                None => body_subs.push((p.name, frag_data)),
            }
            needs_frag_data = true;
        } else {
            globals.extend(cat!(b"varying ", p.ty, b" ", p.name, b";\n"));
        }
    }

    // stable on purpose: the original's std::sort is insertion sort up to 16 elements
    body_subs.sort_by_key(|(from, _)| std::cmp::Reverse(from.len()));
    for (from, to) in &body_subs {
        body = replace_identifier(&body, from, to);
    }

    let mut version = if needs_texel_fetch || needs_int_ops(&body) {
        b"#version 130\n".to_vec()
    } else {
        b"#version 120\n".to_vec()
    };
    if needs_gather {
        version.extend_from_slice(
            b"#extension GL_ARB_texture_gather : enable\n#extension GL_ARB_gpu_shader5 : enable\n",
        );
    }
    if ty == ShaderType::Fragment && needs_frag_data {
        version.extend_from_slice(b"#extension GL_ARB_draw_buffers : enable\n");
    }

    Ok(if frag_data_pad.is_empty() {
        cat!(version, header, globals, b"void main()\n{", body, b"}\n")
    } else {
        cat!(
            version,
            header,
            globals,
            b"void ohpl_main()\n{",
            body,
            b"}\n",
            b"void main()\n{\n\tohpl_main();\n",
            frag_data_pad,
            b"}\n"
        )
    })
}

#[cfg(test)]
mod tests {
    use super::*;

    fn run(src: &str, ty: ShaderType) -> Result<String, String> {
        transpile(src.as_bytes(), ty)
            .map(|v| String::from_utf8(v).unwrap())
            .map_err(|v| String::from_utf8(v).unwrap())
    }

    // expected values captured from the C++ HpslTranspiler.cpp this module replaced
    #[test]
    fn golden_vertex_mul_and_builtin_substitution() {
        let src = "uniform cMatrixf a_mtxModelViewProjection;\n\nvoid main(in cVector4f vtx_vPosition,\n\t\t  in cVector4f vtx_vColor, \n\t\t  out cVector4f px_vColor,\n\t\t  out cVector4f px_vPosition)\n{\t\n\tpx_vPosition = mul(a_mtxModelViewProjection, vtx_vPosition);\n\tpx_vColor = vtx_vColor;\n}";
        assert_eq!(
            run(src, ShaderType::Vertex).unwrap(),
            "#version 120\n\ninvariant gl_Position;\nvarying vec4 px_vColor;\nvoid main()\n{\t\n\tgl_Position = (gl_ModelViewProjectionMatrix * gl_Vertex);\n\tpx_vColor = gl_Color;\n}\n"
        );
    }

    #[test]
    fn golden_fragment_intrinsics_and_padded_output() {
        let src = "uniform cTexture2D aMap : 3;\nuniform cTextureBuffer buf : 15;\nvoid main(in cVector4f px_vPosition,\n\t\t  out cVector4f out_vColor : 0,\n\t\t  out cVector2f out_vSpec : 2)\n{\n\tout_vColor = sampleGather(aMap, px_vPosition.xy, cVector2l(1, 0)) + sampleGatherAlpha(aMap, px_vPosition.xy) + load(aMap, cVector2l(1, 0)) + sample(aMap, uv);\n\tint a = 5 % 2;\n}";
        assert_eq!(
            run(src, ShaderType::Fragment).unwrap(),
            "#version 130\n#extension GL_ARB_texture_gather : enable\n#extension GL_ARB_gpu_shader5 : enable\n#extension GL_ARB_draw_buffers : enable\nuniform sampler2D aMap;\n\nvec2 out_vSpec;\nvoid ohpl_main()\n{\n\tgl_FragData[0] = textureGather(aMap, gl_FragCoord.xy + vec2(ivec2(1, 0)) / vec2(textureSize(aMap, 0))) + textureGather(aMap, gl_FragCoord.xy, 3) + texelFetch(aMap, ivec2(1, 0), 0) + texture2D(aMap, uv);\n\tint a = 5 % 2;\n}\nvoid main()\n{\n\tohpl_main();\n\tgl_FragData[2] = vec4(out_vSpec, 0.0, 0.0);\n}\n"
        );
    }

    #[test]
    fn golden_cbuffer_flatten_and_saturating_semantic() {
        // an overflowing ": N" parses to -1 (glibc atoi), so the output is a plain varying
        let src = "cBuffer cArgs : 2\n{\n\t#define Q 1\n\tfloat f;\n\t// c\n\n\tcMatrixf a_mtxNormal;\n};\nvoid main(out cVector4f px_vColor : 99999999999999999999)\n{\n\tpx_vColor = ToMatrix3f(a_mtxNormal);\n}";
        assert_eq!(
            run(src, ShaderType::Fragment).unwrap(),
            "#version 120\n\n\t#define Q 1\n\tuniform float f;\n\t// c\n\n\t\nvarying vec4 px_vColor;\nvoid main()\n{\n\tpx_vColor = gl_NormalMatrix;\n}\n"
        );
    }

    #[test]
    fn golden_gamma_helpers_inserted() {
        let src = "float x = GammaToLinearCorrection(y);\nvoid main(out cVector4f c : 0)\n{\n}";
        assert_eq!(
            run(src, ShaderType::Fragment).unwrap(),
            "#version 120\n#extension GL_ARB_draw_buffers : enable\nfloat x = GammaToLinearCorrection(y);\nvec3 SRGBToLinear(in vec3 v) { return mix(v / 12.92, pow((v + 0.055) / 1.055, vec3(2.4)), step(vec3(0.04045), v)); }\nvec4 SRGBToLinear(in vec4 v) { return mix(v / 12.92, pow((v + 0.055) / 1.055, vec4(2.4)), step(vec4(0.04045), v)); }\nvec3 GammaToLinearCorrection(in vec3 v) { return SRGBToLinear(v); }\nvec4 GammaToLinearCorrection(in vec4 v) { return SRGBToLinear(v); }\nvoid main()\n{\n}\n"
        );
    }

    #[test]
    fn golden_errors() {
        let frag = ShaderType::Fragment;
        assert_eq!(
            run("void main(in cVector4f px_vPosition)\n{\n\tx = mul(a, b, c);\n}", frag),
            Err("mul() with 3 argument(s) is not supported (only the 2-argument matrix/vector form is)".into())
        );
        assert_eq!(
            run("cBuffer x { float f; ", frag),
            Err("Unterminated 'cBuffer { ... }' block (unbalanced braces)".into())
        );
        assert_eq!(
            run(
                "void main(in cVector4f px_vPosition, bad param)\n{}",
                ShaderType::Vertex
            ),
            Err(
                "Couldn't parse parameter ' bad param' (expected '(in|out) TYPE NAME [: N]')"
                    .into()
            )
        );
        assert_eq!(run("", frag), Err("No 'main' function found".into()));
        assert_eq!(run("main", frag), Err("No '(' found after 'main'".into()));
        assert_eq!(
            run("main()", frag),
            Err("No '{' found after main()'s parameter list".into())
        );
        assert_eq!(
            run("cBuffer", frag),
            Err("Found 'cBuffer' with no following '{'".into())
        );
        assert_eq!(
            run("cBuffer x { } y", frag),
            Err("'cBuffer { ... }' block not terminated with ';' after the closing brace".into())
        );
    }

    #[test]
    fn sample_requires_declared_sampler() {
        let err = run(
            "void main(out cVector4f px_vColor : 0)\n{\n\tpx_vColor = sample(aNotDeclared, uv);\n}",
            ShaderType::Fragment,
        )
        .unwrap_err();
        assert!(err.contains("aNotDeclared"), "{err}");
    }

    #[test]
    fn load_rejects_non_sampler2d() {
        let err = run(
            "uniform cTextureCube aEnvMap : 0;\nvoid main(out cVector4f px_vColor : 0)\n{\n\tpx_vColor = load(aEnvMap, cVector2l(0, 0), 0);\n}",
            ShaderType::Fragment,
        )
        .unwrap_err();
        assert!(err.contains("aEnvMap"), "{err}");
    }

    #[test]
    fn non_ascii_bytes_pass_through() {
        let out = transpile(b"void main()\n{ \xff\xc3\xa9 }", ShaderType::Fragment).unwrap();
        assert!(out.ends_with(b"{ \xff\xc3\xa9 }\n"));
    }

    #[test]
    fn atoi_matches_glibc() {
        let cases: [(&[u8], i32); 8] = [
            (b"", 0),
            (b"  12abc", 12),
            (b"-3", -3),
            (b"+7", 7),
            (b"x", 0),
            (b"4294967295", -1),
            (b"99999999999999999999", -1),
            (b"-99999999999999999999", 0),
        ];
        for (s, want) in cases {
            assert_eq!(atoi(s), want, "input: {s:?}");
        }
    }

    #[test]
    fn identifier_replacement_is_whole_word() {
        assert_eq!(
            replace_identifier(b"a_b a_bc xa_b a_b", b"a_b", b"Z"),
            b"Z a_bc xa_b Z"
        );
    }

    #[test]
    fn matcher_edge_cases() {
        assert_eq!(
            strip_uniform_binding_indices(b"uniform float x : 3;uniform cTextureBuffer b : 1 ;u"),
            b"uniform float x;u"
        );
        assert!(needs_int_ops(b"isnan(x)"));
        assert!(!needs_int_ops(b"xisnan(x) _isnan(y)"));
        assert_eq!(
            collect_sampler_types(b"(sampler2D a, sampler2DRect b)").len(),
            2
        );
    }
}
