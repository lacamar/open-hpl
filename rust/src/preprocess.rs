//! Shader-source preprocessor (`@define`, `@ifdef`/`@elseif`/`@else`/`@endif`, `@include`, `$var$`).

use std::collections::BTreeMap;

pub type Vars = BTreeMap<Vec<u8>, Vec<u8>>;

#[derive(Clone, Copy, PartialEq, Eq, Debug)]
pub enum Level {
    Warning,
    Error,
}

pub trait Host {
    /// Ok(file bytes), or Err(8-bit path text for the "not found" message).
    fn include(&mut self, name: &[u8]) -> Result<Vec<u8>, Vec<u8>>;
    fn log(&mut self, level: Level, msg: &[u8]);
}

const MAX_INCLUDE_DEPTH: u32 = 128;

#[derive(Clone, Copy, PartialEq, Eq)]
enum Keyword {
    Define,
    Ifdef,
    Else,
    Elseif,
    Endif,
    Include,
}

#[derive(Clone, Copy, PartialEq, Eq)]
enum Op {
    And,
    Or,
}

enum Sym {
    Text(Vec<u8>),
    Keyword(Keyword),
    Operator(Op),
    Variable(Vec<u8>),
}

impl Sym {
    fn type_id(&self) -> i32 {
        match self {
            Sym::Text(_) => 0,
            Sym::Keyword(_) => 1,
            Sym::Operator(_) => 2,
            Sym::Variable(_) => 3,
        }
    }
}

#[derive(Clone, Copy, PartialEq, Eq)]
enum Process {
    PureText,
    Line,
    Variable,
}

struct Shared<'h> {
    env: &'h Vars,
    cur: &'h Vars,
    host: &'h mut dyn Host,
}

struct Parser<'s, 'h> {
    sh: &'s mut Shared<'h>,
    input: &'s [u8],
    pos: usize,
    process: Process,
    cur_row: i32,
    row_count: i32,
    syms: Vec<(Sym, i32)>,
    idx: usize,
    parsing: Vars,
    out: Vec<u8>,
    depth: u32,
}

fn keyword(s: &[u8]) -> Option<Keyword> {
    Some(match s {
        b"define" => Keyword::Define,
        b"ifdef" => Keyword::Ifdef,
        b"else" => Keyword::Else,
        b"elseif" => Keyword::Elseif,
        b"endif" => Keyword::Endif,
        b"include" => Keyword::Include,
        _ => return None,
    })
}

// C++ passed these through `%s`, which stops at NUL.
fn c_str(s: &[u8]) -> &[u8] {
    s.split(|&c| c == 0).next().unwrap_or(&[])
}

fn num(n: i32) -> Vec<u8> {
    n.to_string().into_bytes()
}

impl<'s, 'h> Parser<'s, 'h> {
    fn new(sh: &'s mut Shared<'h>, input: &'s [u8], depth: u32) -> Self {
        Parser {
            sh,
            input,
            pos: 0,
            process: Process::PureText,
            cur_row: 1,
            row_count: 1,
            syms: Vec::new(),
            idx: 0,
            parsing: Vars::new(),
            out: Vec::new(),
            depth,
        }
    }

    fn error(&mut self, parts: &[&[u8]]) {
        self.sh.host.log(Level::Error, &parts.concat());
    }

    fn get_var(&self, name: &[u8]) -> Option<&Vec<u8>> {
        self.sh
            .env
            .get(name)
            .or_else(|| self.sh.cur.get(name))
            .or_else(|| self.parsing.get(name))
    }

    fn run(&mut self) -> bool {
        while self.pos < self.input.len() {
            let process = self.process;
            let s = self.next_string();
            self.push_symbol(process, s);
        }
        if self.syms.is_empty() {
            self.error(&[b"Parser failed: No symbols to parse!\n"]);
        }
        self.idx = 0;
        while !self.end() {
            if !self.parse_symbol() {
                return false;
            }
        }
        true
    }

    fn next_string(&mut self) -> Vec<u8> {
        let mut s = Vec::new();
        self.cur_row = self.row_count;
        loop {
            let c = self.input[self.pos];
            if c == b'\n' || c == b'\r' {
                self.row_count += 1;
            }
            if self.process != Process::PureText {
                if matches!(c, b' ' | b'\n' | b'\r' | b'\t') {
                    if c == b'\n' || c == b'\r' || self.process == Process::Variable {
                        self.process = Process::PureText;
                    }
                    self.pos += 1;
                    return s;
                } else if c == b'$' {
                    self.pos += 1;
                    if self.process == Process::Variable {
                        self.process = Process::PureText;
                        return s;
                    }
                } else if !c.is_ascii_alphanumeric() {
                    if self.process == Process::Variable {
                        self.process = Process::PureText;
                        return s;
                    }
                    self.pos += 1;
                    s.push(c);
                } else {
                    self.pos += 1;
                    s.push(c);
                }
            } else if c == b'@' {
                while matches!(s.last(), Some(b' ' | b'\t')) {
                    s.pop();
                }
                self.pos += 1;
                self.process = Process::Line;
                return s;
            } else if c == b'$' {
                self.pos += 1;
                self.process = Process::Variable;
                return s;
            } else {
                self.pos += 1;
                s.push(c);
            }
            if self.pos >= self.input.len() {
                return s;
            }
        }
    }

    fn push_symbol(&mut self, process: Process, s: Vec<u8>) {
        let row = self.cur_row;
        if process == Process::PureText {
            self.syms.push((Sym::Text(s), row));
            return;
        }
        if s.is_empty() || s[0] == b' ' || s[0] == b'\t' {
            return;
        }
        if process == Process::Line {
            if let Some(k) = keyword(&s) {
                self.syms.push((Sym::Keyword(k), row));
                return;
            }
            let op = match s.as_slice() {
                b"&&" => Some(Op::And),
                b"||" => Some(Op::Or),
                _ => None,
            };
            if let Some(op) = op {
                self.syms.push((Sym::Operator(op), row));
                return;
            }
        }
        self.syms.push((Sym::Variable(s), row));
    }

    fn end(&self) -> bool {
        self.idx >= self.syms.len()
    }

    fn advance(&mut self) {
        if !self.end() {
            self.idx += 1;
        }
    }

    fn cur_kw(&self) -> Option<Keyword> {
        match self.syms.get(self.idx) {
            Some((Sym::Keyword(k), _)) => Some(*k),
            _ => None,
        }
    }

    fn cur_is_block_end(&self) -> bool {
        matches!(
            self.cur_kw(),
            Some(Keyword::Endif | Keyword::Else | Keyword::Elseif)
        )
    }

    fn parse_symbol(&mut self) -> bool {
        let row = self.syms[self.idx].1;
        match &self.syms[self.idx].0 {
            Sym::Text(t) => {
                self.out.extend_from_slice(t);
                self.advance();
                true
            }
            Sym::Variable(name) => {
                let name = name.clone();
                match self.get_var(&name).cloned() {
                    Some(v) => self.out.extend_from_slice(&v),
                    None => {
                        let m = [
                            &b"Parser warning: Variable '"[..],
                            c_str(&name),
                            b"' on row ",
                            &num(row),
                            b" was not found!\n",
                        ]
                        .concat();
                        self.sh.host.log(Level::Warning, &m);
                    }
                }
                self.advance();
                true
            }
            Sym::Operator(_) => {
                self.error(&[
                    b"Parser failed: Unexpected operator on row ",
                    &num(row),
                    b"!\n",
                ]);
                self.advance();
                true
            }
            Sym::Keyword(k) => {
                let k = *k;
                self.parse_keyword(k, row)
            }
        }
    }

    fn bool_statement(&mut self, value: &mut bool) -> bool {
        *value = false;
        let mut prev: Option<Op> = None;
        loop {
            let name = match self.syms.get(self.idx) {
                Some((Sym::Variable(n), _)) => n.clone(),
                Some((s, row)) => {
                    let (t, r) = (num(s.type_id()), num(*row));
                    self.error(&[
                        b"Parser failed: Invalid symbol type ",
                        &t,
                        b" at row ",
                        &r,
                        b"!\n",
                    ]);
                    return false;
                }
                None => {
                    // C++ dereferenced a null symbol here
                    self.error(&[b"Parser failed: Unexpected end of symbols!\n"]);
                    return false;
                }
            };
            if self.get_var(&name).is_some() {
                if prev != Some(Op::And) {
                    *value = true;
                }
            } else if prev == Some(Op::And) {
                *value = false;
            }
            self.advance();
            match self.syms.get(self.idx) {
                Some((Sym::Operator(op), _)) => {
                    prev = Some(*op);
                    self.advance();
                }
                _ => break,
            }
        }
        true
    }

    fn parse_keyword(&mut self, kw: Keyword, row: i32) -> bool {
        match kw {
            Keyword::Ifdef => {
                let mut stmt = false;
                let mut found = false;
                let mut current = kw;
                while current != Keyword::Endif && !self.end() {
                    self.advance();
                    if matches!(current, Keyword::Ifdef | Keyword::Elseif)
                        && !self.bool_statement(&mut stmt)
                    {
                        return false;
                    }
                    if !found && (stmt || current == Keyword::Else) {
                        found = true;
                        while !self.end() && !self.cur_is_block_end() {
                            self.parse_symbol();
                        }
                    } else {
                        let mut depth = 0;
                        while !self.end() && (!self.cur_is_block_end() || depth > 0) {
                            match self.cur_kw() {
                                Some(Keyword::Ifdef) => depth += 1,
                                Some(Keyword::Endif) => depth -= 1,
                                _ => {}
                            }
                            self.advance();
                        }
                    }
                    if let Some(k) = self.cur_kw() {
                        current = k;
                    }
                }
                self.advance();
                true
            }
            Keyword::Define => {
                let mut parts: [Vec<u8>; 2] = [Vec::new(), Vec::new()];
                for part in &mut parts {
                    self.advance();
                    match self.syms.get(self.idx) {
                        Some((Sym::Variable(n), _)) => part.clone_from(n),
                        _ => {
                            self.error(&[
                                b"Parser failed: Keyword define at row ",
                                &num(row),
                                b" does not meet syntax: '@define var value'!\n",
                            ]);
                            return false;
                        }
                    }
                }
                let [name, val] = parts;
                self.parsing.insert(name, val);
                self.advance();
                true
            }
            Keyword::Include => {
                self.advance();
                let file = match self.syms.get(self.idx) {
                    Some((Sym::Variable(n), _)) => n.clone(),
                    _ => {
                        self.error(&[
                            b"Parser failed: Keyword include at row ",
                            &num(row),
                            b" does not meet syntax: '@include file.ext'!\n",
                        ]);
                        return false;
                    }
                };
                match self.sh.host.include(&file) {
                    Ok(mut data) => {
                        if data.contains(&b'@') {
                            if self.depth >= MAX_INCLUDE_DEPTH {
                                // C++ recursed until the stack overflowed
                                self.error(&[
                                    b"Parser failed: Include nesting too deep at '",
                                    c_str(&file),
                                    b"'!\n",
                                ]);
                                return false;
                            }
                            let mut sub = Parser::new(&mut *self.sh, &data, self.depth + 1);
                            if sub.run() {
                                data = std::mem::take(&mut sub.out);
                            }
                        }
                        self.out.extend_from_slice(&data);
                    }
                    Err(path) => {
                        self.error(&[
                            b"Parser failed: Could not find include file '",
                            c_str(&file),
                            b"' in path: '",
                            c_str(&path),
                            b"' at line ",
                            &num(row),
                            b"!\n",
                        ]);
                        return false;
                    }
                }
                self.advance();
                true
            }
            Keyword::Else | Keyword::Elseif | Keyword::Endif => {
                self.error(&[
                    b"Parser failed: Keyword ",
                    &num(kw as i32),
                    b" at row ",
                    &num(row),
                    b" is not valid!\n",
                ]);
                false
            }
        }
    }
}

pub struct Parsed {
    pub ok: bool,
    pub output: Vec<u8>,
    pub defines: Vars,
}

pub fn parse(input: &[u8], env: &Vars, cur: &Vars, host: &mut dyn Host) -> Parsed {
    let mut sh = Shared { env, cur, host };
    let mut p = Parser::new(&mut sh, input, 0);
    let ok = p.run();
    Parsed {
        ok,
        output: p.out,
        defines: p.parsing,
    }
}

#[cfg(test)]
mod tests {
    use super::*;

    #[derive(Default)]
    struct Fake {
        files: Vec<(&'static str, &'static str)>,
        log: Vec<(Level, String)>,
    }

    impl Host for Fake {
        fn include(&mut self, name: &[u8]) -> Result<Vec<u8>, Vec<u8>> {
            let n = String::from_utf8_lossy(name);
            match self.files.iter().find(|(k, _)| *k == n) {
                Some((_, v)) => Ok(v.as_bytes().to_vec()),
                None => Err(format!("/d/{n}").into_bytes()),
            }
        }
        fn log(&mut self, level: Level, msg: &[u8]) {
            self.log
                .push((level, String::from_utf8_lossy(msg).into_owned()));
        }
    }

    fn vars(names: &[(&str, &str)]) -> Vars {
        names
            .iter()
            .map(|(k, v)| (k.as_bytes().to_vec(), v.as_bytes().to_vec()))
            .collect()
    }

    fn run(src: &str, cur: &[(&str, &str)], host: &mut Fake) -> (bool, String, Vars) {
        let r = parse(src.as_bytes(), &Vars::new(), &vars(cur), host);
        (
            r.ok,
            String::from_utf8_lossy(&r.output).into_owned(),
            r.defines,
        )
    }

    fn logs(h: &Fake) -> Vec<&str> {
        h.log.iter().map(|(_, m)| m.as_str()).collect()
    }

    #[test]
    fn boolean_chains_have_no_precedence() {
        let src = "@ifdef A && B || C\nx\n@else\ny\n@endif\n";
        let cases: [(&[(&str, &str)], &str); 3] = [
            (&[("A", "")], "y\n"),
            (&[("A", ""), ("B", "")], "x\n"),
            (&[("C", "")], "x\n"),
        ];
        for (cur, want) in cases {
            assert_eq!(run(src, cur, &mut Fake::default()).1, want, "{cur:?}");
        }
        let src = "@ifdef A || B && C\nx\n@else\ny\n@endif\n";
        assert_eq!(
            run(src, &[("B", ""), ("C", "")], &mut Fake::default()).1,
            "x\n"
        );
        assert_eq!(run(src, &[("B", "")], &mut Fake::default()).1, "y\n");
    }

    #[test]
    fn elseif_chain_picks_first_true() {
        let src = "@ifdef NO\n1\n@elseif NO2\n2\n@elseif A\n3\n@elseif B\n4\n@else\n5\n@endif\n";
        assert_eq!(run(src, &[], &mut Fake::default()).1, "5\n");
        assert_eq!(
            run(src, &[("A", ""), ("B", "")], &mut Fake::default()).1,
            "3\n"
        );
    }

    #[test]
    fn define_and_variable_substitution() {
        let (ok, out, defs) = run(
            "@define FOO bar\n$FOO$ @ifdef FOO\nyes\n@endif\n",
            &[],
            &mut Fake::default(),
        );
        assert!(ok);
        assert_eq!(out, "baryes\n");
        assert_eq!(defs, vars(&[("FOO", "bar")]));
    }

    #[test]
    fn missing_variable_warns_with_row() {
        let mut h = Fake::default();
        let (ok, out, _) = run("\n\n\n\n$M1$\r\n\r\n$M2$\n@frob\n", &[], &mut h);
        assert!(ok);
        assert_eq!(out, "\n\n\n\n\r\n\r\n\n");
        assert_eq!(
            logs(&h),
            [
                "Parser warning: Variable 'M1' on row 5 was not found!\n",
                "Parser warning: Variable 'M2' on row 9 was not found!\n",
                "Parser warning: Variable 'frob' on row 10 was not found!\n",
            ]
        );
    }

    #[test]
    fn stray_keyword_fails_after_partial_output() {
        let mut h = Fake::default();
        let (ok, out, _) = run("a\n@endif\nb\n", &[], &mut h);
        assert!(!ok);
        assert_eq!(out, "a\n");
        assert_eq!(
            logs(&h),
            ["Parser failed: Keyword 4 at row 2 is not valid!\n"]
        );
    }

    #[test]
    fn unterminated_ifdef_is_silent() {
        let mut h = Fake::default();
        assert_eq!(
            run("@ifdef A\nfoo\n", &[("A", "")], &mut h),
            (true, "foo\n".into(), Vars::new())
        );
        assert!(h.log.is_empty());
    }

    #[test]
    fn empty_input_logs_and_succeeds() {
        let mut h = Fake::default();
        assert!(run("", &[], &mut h).0);
        assert_eq!(logs(&h), ["Parser failed: No symbols to parse!\n"]);
    }

    #[test]
    fn missing_include_fails_with_path() {
        let mut h = Fake::default();
        let (ok, out, _) = run("a\n@include nothere.txt\nb\n", &[], &mut h);
        assert!(!ok);
        assert_eq!(out, "a\n");
        assert_eq!(
            logs(&h),
            [
                "Parser failed: Could not find include file 'nothere.txt' in path: '/d/nothere.txt' at line 2!\n"
            ]
        );
    }

    #[test]
    fn include_with_at_is_parsed_with_caller_vars_and_leaks_no_defines() {
        let mut h = Fake {
            files: vec![
                ("i.txt", "@ifdef A\nIA\n@else\nIN\n@endif\n$INCV$\n"),
                ("d.txt", "@define LEAK 1\n"),
            ],
            ..Fake::default()
        };
        let (ok, out, defs) = run(
            "a\n@include i.txt\n@include d.txt\nb $LEAK$\n",
            &[("A", "")],
            &mut h,
        );
        assert!(ok);
        assert_eq!(out, "a\nIA\n\nb \n");
        assert!(defs.is_empty());
        assert_eq!(logs(&h).len(), 2);
    }

    #[test]
    fn dollar_edge_cases() {
        let (_, out, _) = run(
            "$A$ $A $A\n$$ $ $$$ $A$B$ $A.B$ $A-B$ $\n",
            &[],
            &mut Fake::default(),
        );
        assert_eq!(out, "  B.B-B");
    }

    #[test]
    fn spaces_before_at_are_trimmed() {
        assert_eq!(
            run(
                "foo   \t  @ifdef A\nbar\n   @endif\n",
                &[("A", "")],
                &mut Fake::default()
            )
            .1,
            "foobar\n"
        );
    }

    #[test]
    fn truncated_directives_fail_instead_of_crashing() {
        for src in ["x\n@define A", "@ifdef", "@ifdef A &&", "a\n@include"] {
            assert!(!run(src, &[], &mut Fake::default()).0, "{src:?}");
        }
    }

    #[test]
    fn recursive_include_hits_depth_limit() {
        let mut h = Fake {
            files: vec![("r.txt", "@include r.txt\n")],
            ..Fake::default()
        };
        let (ok, _, _) = run("@include r.txt\n", &[], &mut h);
        assert!(ok);
        assert_eq!(logs(&h).len(), 1);
        assert!(logs(&h)[0].starts_with("Parser failed: Include nesting too deep"));
    }
}

mod ffi {
    use super::{Host, Level, Vars, parse};
    use std::ffi::c_void;

    type Sink = unsafe extern "C" fn(*mut c_void, *const u8, usize);
    type IncludeFn = unsafe extern "C" fn(*mut c_void, *const u8, usize, Sink, *mut c_void) -> bool;
    type LogFn = unsafe extern "C" fn(*mut c_void, bool, *const u8, usize);
    type DefineSink = unsafe extern "C" fn(*mut c_void, *const u8, usize, *const u8, usize);

    #[repr(C)]
    pub struct Kv {
        k: *const u8,
        kn: usize,
        v: *const u8,
        vn: usize,
    }

    struct CHost {
        include: IncludeFn,
        log: LogFn,
        ctx: *mut c_void,
    }

    unsafe extern "C" fn collect(ctx: *mut c_void, p: *const u8, n: usize) {
        // SAFETY: ctx is the `Vec<u8>` handed to the callback below; p/n is a readable slice.
        let v = unsafe { &mut *(ctx as *mut Vec<u8>) };
        v.extend_from_slice(unsafe { units(p, n) });
    }

    impl Host for CHost {
        fn include(&mut self, name: &[u8]) -> Result<Vec<u8>, Vec<u8>> {
            let mut buf: Vec<u8> = Vec::new();
            // SAFETY: callback contract of `ohpl_pp_parse`; `buf` outlives the call.
            let found = unsafe {
                (self.include)(
                    self.ctx,
                    name.as_ptr(),
                    name.len(),
                    collect,
                    &mut buf as *mut Vec<u8> as *mut c_void,
                )
            };
            if found { Ok(buf) } else { Err(buf) }
        }

        fn log(&mut self, level: Level, msg: &[u8]) {
            // SAFETY: callback contract of `ohpl_pp_parse`.
            unsafe { (self.log)(self.ctx, level == Level::Error, msg.as_ptr(), msg.len()) }
        }
    }

    /// # Safety
    /// `p` points to `n` readable bytes, or `n` is 0.
    unsafe fn units<'a>(p: *const u8, n: usize) -> &'a [u8] {
        if n == 0 {
            &[]
        } else {
            // SAFETY: caller contract.
            unsafe { std::slice::from_raw_parts(p, n) }
        }
    }

    /// # Safety
    /// `kv` points to `n` entries whose pointers are valid for their lengths.
    unsafe fn vars(kv: *const Kv, n: usize) -> Vars {
        if n == 0 {
            return Vars::new();
        }
        // SAFETY: caller contract.
        unsafe { std::slice::from_raw_parts(kv, n) }
            .iter()
            .map(|e| {
                // SAFETY: caller contract.
                unsafe { (units(e.k, e.kn).to_vec(), units(e.v, e.vn).to_vec()) }
            })
            .collect()
    }

    /// Parses `input`, appending to the output through `out`; every define reaches `defines`.
    /// `include` fills its sink with the file data and returns true, or with the 8-bit path and
    /// returns false. Callbacks must not unwind.
    ///
    /// # Safety
    /// All pointers valid for their lengths; `env`/`cur` hold `env_n`/`cur_n` entries; the
    /// callbacks accept `ctx`.
    #[unsafe(no_mangle)]
    pub unsafe extern "C" fn ohpl_pp_parse(
        input: *const u8,
        input_n: usize,
        env: *const Kv,
        env_n: usize,
        cur: *const Kv,
        cur_n: usize,
        include: IncludeFn,
        log: LogFn,
        ctx: *mut c_void,
        out: Sink,
        out_ctx: *mut c_void,
        defines: DefineSink,
        defines_ctx: *mut c_void,
    ) -> bool {
        // SAFETY: caller contract.
        let (input, env, cur) =
            unsafe { (units(input, input_n), vars(env, env_n), vars(cur, cur_n)) };
        let mut host = CHost { include, log, ctx };
        let r = parse(input, &env, &cur, &mut host);
        // SAFETY: caller contract.
        unsafe {
            out(out_ctx, r.output.as_ptr(), r.output.len());
            for (k, v) in &r.defines {
                defines(defines_ctx, k.as_ptr(), k.len(), v.as_ptr(), v.len());
            }
        }
        r.ok
    }
}
