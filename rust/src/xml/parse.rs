//! Literal port of tinyXML 2.5's parser, quirks included. Positions are byte offsets into the
//! NUL-terminated input; `None` is tinyXML's null pointer and reads past the end see NUL.

use super::dom::*;

const UTF_LEAD_0: u8 = 0xef;
const UTF_LEAD_1: u8 = 0xbb;
const UTF_LEAD_2: u8 = 0xbf;
const TAB_SIZE: i32 = 4;
// ponytail: tinyXML recurses until the C stack dies; this errors out at a depth no game file reaches.
const MAX_DEPTH: u32 = 10000;

const ENTITIES: [(&[u8], u8); 5] = [
    (b"&amp;", b'&'),
    (b"&lt;", b'<'),
    (b"&gt;", b'>'),
    (b"&quot;", b'"'),
    (b"&apos;", b'\''),
];

fn utf8_len(b: u8) -> usize {
    match b {
        0xc2..=0xdf => 2,
        0xe0..=0xef => 3,
        0xf0..=0xf4 => 4,
        _ => 1,
    }
}

fn is_white(c: u8) -> bool {
    c == b' ' || (9..=13).contains(&c)
}

fn is_alpha(c: u8) -> bool {
    c >= 127 || c.is_ascii_alphabetic()
}

fn is_alnum(c: u8) -> bool {
    c >= 127 || c.is_ascii_alphanumeric()
}

fn lower(c: u8) -> u8 {
    c.to_ascii_lowercase()
}

fn utf32_to_utf8(mut input: u64) -> Vec<u8> {
    const FIRST: [u8; 5] = [0, 0, 0xC0, 0xE0, 0xF0];
    let len = match input {
        0..0x80 => 1,
        0x80..0x800 => 2,
        0x800..0x10000 => 3,
        0x10000..0x200000 => 4,
        _ => return Vec::new(),
    };
    let mut out = vec![0u8; len];
    for i in (1..len).rev() {
        out[i] = ((input | 0x80) & 0xBF) as u8;
        input >>= 6;
    }
    out[0] = (input | u64::from(FIRST[len])) as u8;
    out
}

struct Parser<'a> {
    s: &'a [u8],
    doc: &'a mut Doc,
    stamp: usize,
    row: i32,
    col: i32,
    depth: u32,
}

type Pos = Option<usize>;

impl Parser<'_> {
    fn at(&self, p: usize) -> u8 {
        self.s.get(p).copied().unwrap_or(0)
    }

    /// `p != null && *p`
    fn live(&self, p: Pos) -> Option<usize> {
        p.filter(|&p| self.at(p) != 0)
    }

    fn stamp(&mut self, now: usize, enc: Enc) {
        let (mut row, mut col, mut p) = (self.row, self.col, self.stamp);
        while p < now {
            match self.at(p) {
                0 => return,
                b'\r' => {
                    row += 1;
                    col = 0;
                    p += 1;
                    if self.at(p) == b'\n' {
                        p += 1;
                    }
                }
                b'\n' => {
                    row += 1;
                    col = 0;
                    p += 1;
                    if self.at(p) == b'\r' {
                        p += 1;
                    }
                }
                b'\t' => {
                    p += 1;
                    col = (col / TAB_SIZE + 1) * TAB_SIZE;
                }
                UTF_LEAD_0 if enc == Enc::Utf8 => {
                    if self.at(p + 1) != 0 && self.at(p + 2) != 0 {
                        let (b1, b2) = (self.at(p + 1), self.at(p + 2));
                        p += 3;
                        if !((b1 == UTF_LEAD_1 && b2 == UTF_LEAD_2)
                            || (b1 == 0xbf && (b2 == 0xbe || b2 == 0xbf)))
                        {
                            col += 1;
                        }
                    } else {
                        p += 1;
                    }
                }
                UTF_LEAD_0 => {
                    p += 1;
                    col += 1;
                }
                b => {
                    p += if enc == Enc::Utf8 { utf8_len(b) } else { 1 };
                    col += 1;
                }
            }
        }
        self.row = row;
        self.col = col;
        self.stamp = p;
    }

    fn fail(&mut self, id: i32, at: Pos, enc: Enc) {
        if self.doc.error {
            return;
        }
        let loc = at.map(|p| {
            self.stamp(p, enc);
            (self.row, self.col)
        });
        self.doc.set_error(id, loc);
    }

    fn skip_white(&self, p: Pos, enc: Enc) -> Pos {
        let mut p = self.live(p)?;
        loop {
            let c = self.at(p);
            if c == 0 {
                break;
            }
            if enc == Enc::Utf8
                && c == UTF_LEAD_0
                && matches!(
                    (self.at(p + 1), self.at(p + 2)),
                    (UTF_LEAD_1, UTF_LEAD_2) | (0xbf, 0xbe | 0xbf)
                )
            {
                p += 3;
            } else if is_white(c) {
                p += 1;
            } else {
                break;
            }
        }
        Some(p)
    }

    fn string_equal(&self, p: Pos, tag: &[u8], ignore_case: bool) -> bool {
        let Some(p) = self.live(p) else { return false };
        tag.iter().enumerate().all(|(i, &t)| {
            let c = self.at(p + i);
            c != 0
                && if ignore_case {
                    lower(c) == lower(t)
                } else {
                    c == t
                }
        })
    }

    fn read_name(&self, p: Pos) -> (Pos, Vec<u8>) {
        match self.live(p) {
            Some(start) if is_alpha(self.at(start)) || self.at(start) == b'_' => {
                let mut q = start;
                while self.at(q) != 0
                    && (is_alnum(self.at(q)) || matches!(self.at(q), b'_' | b'-' | b'.' | b':'))
                {
                    q += 1;
                }
                (Some(q), self.s[start..q].to_vec())
            }
            _ => (None, Vec::new()),
        }
    }

    /// Returns the bytes the entity or character stands for and the next position.
    fn get_entity(&self, p: usize, enc: Enc) -> (Pos, Vec<u8>) {
        if self.at(p + 1) == b'#' && self.at(p + 2) != 0 {
            let hex = self.at(p + 2) == b'x';
            let (start, base, stop) = if hex {
                (p + 3, 16u32, b'x')
            } else {
                (p + 2, 10, b'#')
            };
            if self.at(start) == 0 {
                return (None, Vec::new());
            }
            let Some(semi) = (start..self.s.len()).find(|&i| self.s[i] == b';') else {
                return (None, Vec::new());
            };
            let (mut ucs, mut mult, mut q) = (0u64, 1u32, semi - 1);
            while self.at(q) != stop {
                let c = self.at(q);
                let digit = match c {
                    b'0'..=b'9' => u32::from(c - b'0'),
                    b'a'..=b'f' if hex => u32::from(c - b'a') + 10,
                    b'A'..=b'F' if hex => u32::from(c - b'A') + 10,
                    _ => return (None, Vec::new()),
                };
                ucs = ucs.wrapping_add(u64::from(mult.wrapping_mul(digit)));
                mult = mult.wrapping_mul(base);
                q -= 1;
            }
            let bytes = if enc == Enc::Utf8 {
                utf32_to_utf8(ucs)
            } else {
                vec![ucs as u8]
            };
            return (Some(semi + 1), bytes);
        }
        for (s, c) in ENTITIES {
            if self.s[p..].starts_with(s) {
                return (Some(p + s.len()), vec![c]);
            }
        }
        (Some(p + 1), Vec::new())
    }

    /// `GetChar` + append: the (possibly NUL-padded) bytes of one character.
    fn get_char(&self, p: usize, enc: Enc) -> (Pos, Vec<u8>) {
        let len = if enc == Enc::Utf8 {
            utf8_len(self.at(p))
        } else {
            1
        };
        if len == 1 {
            if self.at(p) == b'&' {
                return self.get_entity(p, enc);
            }
            return (Some(p + 1), vec![self.at(p)]);
        }
        let out = (0..len)
            .map(|i| {
                if (0..i).all(|j| self.at(p + j) != 0) {
                    self.at(p + i)
                } else {
                    0
                }
            })
            .collect();
        (Some(p + len), out)
    }

    fn read_text(&self, p: Pos, trim: bool, end: &[u8], enc: Enc) -> (Pos, Vec<u8>) {
        let mut text = Vec::new();
        let mut p = p;
        if !trim {
            while let Some(q) = self.live(p) {
                if self.string_equal(Some(q), end, false) {
                    break;
                }
                let (np, bytes) = self.get_char(q, enc);
                text.extend(bytes);
                p = np;
            }
        } else {
            let mut white = false;
            p = self.skip_white(p, enc);
            while let Some(q) = self.live(p) {
                if self.string_equal(Some(q), end, false) {
                    break;
                }
                if is_white(self.at(q)) {
                    white = true;
                    p = Some(q + 1);
                } else {
                    if white {
                        text.push(b' ');
                        white = false;
                    }
                    let (np, bytes) = self.get_char(q, enc);
                    text.extend(bytes);
                    p = np;
                }
            }
        }
        if let Some(q) = self.live(p) {
            p = Some(q + end.len());
        }
        (p, text)
    }

    /// `TiXmlAttribute::Parse`; errors are recorded only when the attribute belongs to a document.
    fn parse_attribute(&mut self, p: Pos, enc: Enc, owned: bool) -> (Pos, Attr) {
        let mut attr = Attr::default();
        let Some(p) = self.skip_white(p, enc).filter(|&p| self.at(p) != 0) else {
            return (None, attr);
        };
        self.stamp(p, enc);
        let (np, name) = self.read_name(Some(p));
        attr.name = name;
        let Some(p) = self.live(np) else {
            if owned {
                self.fail(ERR_READING_ATTRIBUTES, Some(p), enc);
            }
            return (None, attr);
        };
        let p = self.skip_white(Some(p), enc);
        let Some(p) = p.filter(|&p| self.at(p) == b'=') else {
            if owned {
                self.fail(ERR_READING_ATTRIBUTES, p, enc);
            }
            return (None, attr);
        };
        let p = self.skip_white(Some(p + 1), enc);
        let Some(p) = self.live(p) else {
            if owned {
                self.fail(ERR_READING_ATTRIBUTES, p, enc);
            }
            return (None, attr);
        };
        let c = self.at(p);
        if c == b'\'' || c == b'"' {
            let (np, value) = self.read_text(Some(p + 1), false, &[c], enc);
            attr.value = value;
            return (np, attr);
        }
        let mut q = p;
        while self.at(q) != 0 && !is_white(self.at(q)) && self.at(q) != b'/' && self.at(q) != b'>' {
            if self.at(q) == b'\'' || self.at(q) == b'"' {
                if owned {
                    self.fail(ERR_READING_ATTRIBUTES, Some(q), enc);
                }
                return (None, attr);
            }
            attr.value.push(self.at(q));
            q += 1;
        }
        (Some(q), attr)
    }

    fn parse_text(&mut self, id: NodeId, p: usize, enc: Enc) -> Pos {
        self.stamp(p, enc);
        const START: &[u8] = b"<![CDATA[";
        if self.doc.nodes[id as usize].kind == (Kind::Text { cdata: true }) {
            if !self.string_equal(Some(p), START, false) {
                self.fail(ERR_PARSING_CDATA, Some(p), enc);
                return None;
            }
            let mut q = p + START.len();
            let mut value = Vec::new();
            while self.at(q) != 0 && !self.string_equal(Some(q), b"]]>", false) {
                value.push(self.at(q));
                q += 1;
            }
            self.doc.nodes[id as usize].value = value;
            return self.read_text(Some(q), false, b"]]>", enc).0;
        }
        let (np, value) = self.read_text(Some(p), true, b"<", enc);
        self.doc.nodes[id as usize].value = value;
        np.map(|q| q.wrapping_sub(1))
    }

    fn parse_comment(&mut self, id: NodeId, p: Pos, enc: Enc) -> Pos {
        let p = self.skip_white(p, enc);
        if let Some(p) = p {
            self.stamp(p, enc);
        }
        if !self.string_equal(p, b"<!--", false) {
            self.fail(ERR_PARSING_COMMENT, p, enc);
            return None;
        }
        let mut q = p? + 4;
        let mut value = Vec::new();
        while self.at(q) != 0 && !self.string_equal(Some(q), b"-->", false) {
            value.push(self.at(q));
            q += 1;
        }
        self.doc.nodes[id as usize].value = value;
        Some(if self.at(q) != 0 { q + 3 } else { q })
    }

    fn parse_unknown(&mut self, id: NodeId, p: Pos, enc: Enc) -> Pos {
        let p = self.skip_white(p, enc);
        if let Some(p) = p {
            self.stamp(p, enc);
        }
        let Some(mut q) = p.filter(|&p| self.at(p) == b'<') else {
            self.fail(ERR_PARSING_UNKNOWN, p, enc);
            return None;
        };
        q += 1;
        let mut value = Vec::new();
        while self.at(q) != 0 && self.at(q) != b'>' {
            value.push(self.at(q));
            q += 1;
        }
        self.doc.nodes[id as usize].value = value;
        Some(if self.at(q) == b'>' { q + 1 } else { q })
    }

    fn parse_declaration(&mut self, id: NodeId, p: Pos, enc: Enc) -> Pos {
        let p = self.skip_white(p, enc);
        let Some(mut q) = p.filter(|&p| self.string_equal(Some(p), b"<?xml", true)) else {
            self.fail(ERR_PARSING_DECLARATION, None, enc);
            return None;
        };
        self.stamp(q, enc);
        q += 5;
        let (mut version, mut encoding, mut standalone) = (Vec::new(), Vec::new(), Vec::new());
        let mut p = Some(q);
        while let Some(q) = self.live(p) {
            if self.at(q) == b'>' {
                p = Some(q + 1);
                self.doc.nodes[id as usize].kind = Kind::Declaration {
                    version,
                    encoding,
                    standalone,
                };
                return p;
            }
            p = self.skip_white(Some(q), enc);
            let field = [(&b"version"[..], 0), (b"encoding", 1), (b"standalone", 2)]
                .into_iter()
                .find(|(n, _)| self.string_equal(p, n, true));
            if let Some((_, which)) = field {
                let (np, attr) = self.parse_attribute(p, enc, false);
                p = np;
                let v = c_str(&attr.value).to_vec();
                match which {
                    0 => version = v,
                    1 => encoding = v,
                    _ => standalone = v,
                }
            } else {
                while let Some(q) = self.live(p) {
                    if self.at(q) == b'>' || is_white(self.at(q)) {
                        break;
                    }
                    p = Some(q + 1);
                }
            }
        }
        self.doc.nodes[id as usize].kind = Kind::Declaration {
            version,
            encoding,
            standalone,
        };
        None
    }

    fn parse_element(&mut self, id: NodeId, p: Pos, enc: Enc) -> Pos {
        let p = self.skip_white(p, enc);
        let Some(p) = self.live(p) else {
            self.fail(ERR_PARSING_ELEMENT, None, enc);
            return None;
        };
        self.stamp(p, enc);
        if self.at(p) != b'<' {
            self.fail(ERR_PARSING_ELEMENT, Some(p), enc);
            return None;
        }
        let p = self.skip_white(Some(p + 1), enc);
        let err_pos = p;
        let (np, name) = self.read_name(p);
        let mut end_tag = b"</".to_vec();
        end_tag.extend_from_slice(&name);
        self.doc.nodes[id as usize].value = name;
        let Some(np) = self.live(np) else {
            self.fail(ERR_FAILED_TO_READ_ELEMENT_NAME, err_pos, enc);
            return None;
        };
        let mut p = Some(np);
        while let Some(q) = self.live(p) {
            let err_pos = Some(q);
            let Some(q) = self.live(self.skip_white(Some(q), enc)) else {
                self.fail(ERR_READING_ATTRIBUTES, err_pos, enc);
                return None;
            };
            match self.at(q) {
                b'/' => {
                    if self.at(q + 1) != b'>' {
                        self.fail(ERR_PARSING_EMPTY, Some(q + 1), enc);
                        return None;
                    }
                    return Some(q + 2);
                }
                b'>' => {
                    let q = self.read_value(id, Some(q + 1), enc);
                    let Some(q) = self.live(q) else {
                        self.fail(ERR_READING_END_TAG, q, enc);
                        return None;
                    };
                    if self.string_equal(Some(q), &end_tag, false) {
                        let q = self.skip_white(Some(q + end_tag.len()), enc);
                        if let Some(q) = q.filter(|&q| self.at(q) == b'>') {
                            return Some(q + 1);
                        }
                        self.fail(ERR_READING_END_TAG, q, enc);
                        return None;
                    }
                    self.fail(ERR_READING_END_TAG, Some(q), enc);
                    return None;
                }
                _ => {
                    let (np, attr) = self.parse_attribute(Some(q), enc, true);
                    if self.live(np).is_none() {
                        self.fail(ERR_PARSING_ELEMENT, Some(q), enc);
                        return None;
                    }
                    if self.doc.nodes[id as usize]
                        .attrs
                        .iter()
                        .any(|a| a.name == attr.name)
                    {
                        self.fail(ERR_PARSING_ELEMENT, Some(q), enc);
                        return None;
                    }
                    self.doc.nodes[id as usize].attrs.push(attr);
                    p = np;
                }
            }
        }
        p
    }

    fn read_value(&mut self, parent: NodeId, p: Pos, enc: Enc) -> Pos {
        let mut p = self.skip_white(p, enc);
        while let Some(q) = self.live(p) {
            if self.at(q) != b'<' {
                let text = self.doc.alloc(Kind::Text { cdata: false }, Vec::new());
                self.doc.nodes[text as usize].parent = Some(parent);
                p = self.parse_text(text, q, enc);
                if self.doc.nodes[text as usize]
                    .value
                    .iter()
                    .any(|&c| !is_white(c))
                {
                    self.doc.link_end_child(parent, text);
                }
                // tinyXML re-reads the same tail forever when text runs into end of input
                if p.is_some_and(|n| n <= q) {
                    return None;
                }
            } else if self.string_equal(Some(q), b"</", false) {
                return Some(q);
            } else {
                let node = self.identify(parent, Some(q), enc)?;
                p = self.parse_node(node, Some(q), enc);
                self.doc.link_end_child(parent, node);
            }
            p = self.skip_white(p, enc);
        }
        if p.is_none() {
            self.fail(ERR_READING_ELEMENT_VALUE, None, enc);
        }
        p
    }

    fn parse_node(&mut self, id: NodeId, p: Pos, enc: Enc) -> Pos {
        match self.doc.nodes[id as usize].kind {
            Kind::Element => {
                if self.depth >= MAX_DEPTH {
                    self.fail(ERR_PARSING_ELEMENT, p, enc);
                    return None;
                }
                self.depth += 1;
                let r = self.parse_element(id, p, enc);
                self.depth -= 1;
                r
            }
            Kind::Comment => self.parse_comment(id, p, enc),
            Kind::Text { .. } => match p {
                Some(p) => self.parse_text(id, p, enc),
                None => None,
            },
            Kind::Declaration { .. } => self.parse_declaration(id, p, enc),
            Kind::Unknown => self.parse_unknown(id, p, enc),
            Kind::Document => None,
        }
    }

    fn identify(&mut self, parent: NodeId, p: Pos, enc: Enc) -> Option<NodeId> {
        let p = self.live(self.skip_white(p, enc))?;
        if self.at(p) != b'<' {
            return None;
        }
        let kind = if self.string_equal(Some(p), b"<?xml", true) {
            Kind::Declaration {
                version: Vec::new(),
                encoding: Vec::new(),
                standalone: Vec::new(),
            }
        } else if self.string_equal(Some(p), b"<!--", false) {
            Kind::Comment
        } else if self.string_equal(Some(p), b"<![CDATA[", false) {
            Kind::Text { cdata: true }
        } else if self.string_equal(Some(p), b"<!", false) {
            Kind::Unknown
        } else if is_alpha(self.at(p + 1)) || self.at(p + 1) == b'_' {
            Kind::Element
        } else {
            Kind::Unknown
        };
        let id = self.doc.alloc(kind, Vec::new());
        self.doc.nodes[id as usize].parent = Some(parent);
        Some(id)
    }
}

impl Doc {
    /// `TiXmlDocument::Parse(const char*)`: `input` is cut at its first NUL.
    pub fn parse(&mut self, input: &[u8]) {
        self.clear_error();
        let s = c_str(input);
        let mut parser = Parser {
            s,
            doc: self,
            stamp: 0,
            row: 0,
            col: 0,
            depth: 0,
        };
        let mut enc = Enc::Unknown;
        if s.is_empty() {
            parser.doc.set_error(ERR_DOCUMENT_EMPTY, None);
            return;
        }
        if s.starts_with(&[UTF_LEAD_0, UTF_LEAD_1, UTF_LEAD_2]) {
            enc = Enc::Utf8;
            parser.doc.use_bom = true;
        }
        let mut p = parser.skip_white(Some(0), enc);
        if p.is_none() {
            parser.doc.set_error(ERR_DOCUMENT_EMPTY, None);
            return;
        }
        while let Some(q) = parser.live(p) {
            let Some(node) = parser.identify(DOC, Some(q), enc) else {
                break;
            };
            p = parser.parse_node(node, Some(q), enc);
            parser.doc.link_end_child(DOC, node);
            if enc == Enc::Unknown
                && let Kind::Declaration { encoding, .. } = &parser.doc.nodes[node as usize].kind
            {
                let e = c_str(encoding);
                enc = if e.is_empty()
                    || e.eq_ignore_ascii_case(b"UTF-8")
                    || e.eq_ignore_ascii_case(b"UTF8")
                {
                    Enc::Utf8
                } else {
                    Enc::Legacy
                };
            }
            p = parser.skip_white(p, enc);
        }
        if parser.doc.nodes[DOC as usize].first.is_none() {
            parser.doc.set_error(ERR_DOCUMENT_EMPTY, None);
        }
    }

    /// `TiXmlDocument::LoadFile(FILE*)` after the caller read the file: `read_ok` false means the
    /// read came up short.
    pub fn load(&mut self, data: &[u8], read_ok: bool) -> bool {
        self.clear();
        if data.is_empty() {
            self.set_error(ERR_DOCUMENT_EMPTY, None);
            return false;
        }
        if !read_ok {
            self.set_error(ERR_OPENING_FILE, None);
            return false;
        }
        let mut buf = Vec::with_capacity(data.len());
        let mut it = data.iter().copied().take_while(|&b| b != 0).peekable();
        while let Some(b) = it.next() {
            if b == b'\r' {
                buf.push(b'\n');
                if it.peek() == Some(&b'\n') {
                    it.next();
                }
            } else {
                buf.push(b);
            }
        }
        self.parse(&buf);
        !self.error
    }
}
