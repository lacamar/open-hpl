//! tinyXML's two writers: `TiXmlDocument::Print(FILE*)` (4-space indent, newlines) and the
//! `operator<<` stream printer (no whitespace at all).

use super::dom::*;

/// `TiXmlBase::EncodeString`.
pub fn encode_string(s: &[u8], out: &mut Vec<u8>) {
    let len = s.len();
    let mut i = 0;
    while let Some(&c) = s.get(i) {
        if c == b'&' && s.get(i + 1..i + 3) == Some(b"#x") {
            while i + 1 < len {
                out.push(s.get(i).copied().unwrap_or(0));
                i += 1;
                if s.get(i) == Some(&b';') {
                    break;
                }
            }
        } else {
            match c {
                b'&' => out.extend_from_slice(b"&amp;"),
                b'<' => out.extend_from_slice(b"&lt;"),
                b'>' => out.extend_from_slice(b"&gt;"),
                b'"' => out.extend_from_slice(b"&quot;"),
                b'\'' => out.extend_from_slice(b"&apos;"),
                0..32 => out.extend_from_slice(format!("&#x{c:02X};").as_bytes()),
                _ => out.push(c),
            }
            i += 1;
        }
    }
}

fn print_attr(a: &Attr, out: &mut Vec<u8>) {
    let (mut n, mut v) = (Vec::new(), Vec::new());
    encode_string(c_str(&a.name), &mut n);
    encode_string(&a.value, &mut v);
    let q = if a.value.contains(&b'"') { b'\'' } else { b'"' };
    out.extend_from_slice(&n);
    out.extend_from_slice(&[b'=', q]);
    out.extend_from_slice(&v);
    out.push(q);
}

fn print_declaration(version: &[u8], encoding: &[u8], standalone: &[u8], out: &mut Vec<u8>) {
    out.extend_from_slice(b"<?xml ");
    for (key, v) in [
        ("version", version),
        ("encoding", encoding),
        ("standalone", standalone),
    ] {
        if !v.is_empty() {
            out.extend_from_slice(format!("{key}=\"").as_bytes());
            out.extend_from_slice(v);
            out.extend_from_slice(b"\" ");
        }
    }
    out.extend_from_slice(b"?>");
}

fn indent(depth: usize, out: &mut Vec<u8>) {
    for _ in 0..depth {
        out.extend_from_slice(b"    ");
    }
}

impl Doc {
    fn is_text(&self, id: NodeId) -> bool {
        matches!(self.node(id).kind, Kind::Text { .. })
    }

    fn print_node(&self, id: NodeId, depth: usize, out: &mut Vec<u8>) {
        let n = self.node(id);
        match &n.kind {
            Kind::Document => {
                for c in self.children(id) {
                    self.print_node(c, depth, out);
                    out.push(b'\n');
                }
            }
            Kind::Element => {
                indent(depth, out);
                let name = c_str(&n.value);
                out.push(b'<');
                out.extend_from_slice(name);
                for a in self.attrs(id) {
                    out.push(b' ');
                    print_attr(a, out);
                }
                let close = |out: &mut Vec<u8>| {
                    out.extend_from_slice(b"</");
                    out.extend_from_slice(name);
                    out.push(b'>');
                };
                match n.first {
                    None => out.extend_from_slice(b" />"),
                    Some(f) if n.first == n.last && self.is_text(f) => {
                        out.push(b'>');
                        self.print_node(f, depth + 1, out);
                        close(out);
                    }
                    Some(_) => {
                        out.push(b'>');
                        for c in self.children(id) {
                            if !self.is_text(c) {
                                out.push(b'\n');
                            }
                            self.print_node(c, depth + 1, out);
                        }
                        out.push(b'\n');
                        indent(depth, out);
                        close(out);
                    }
                }
            }
            Kind::Comment => {
                indent(depth, out);
                out.extend_from_slice(b"<!--");
                out.extend_from_slice(c_str(&n.value));
                out.extend_from_slice(b"-->");
            }
            Kind::Text { cdata: true } => {
                out.push(b'\n');
                indent(depth, out);
                out.extend_from_slice(b"<![CDATA[");
                out.extend_from_slice(c_str(&n.value));
                out.extend_from_slice(b"]]>\n");
            }
            Kind::Text { cdata: false } => encode_string(&n.value, out),
            Kind::Declaration {
                version,
                encoding,
                standalone,
            } => print_declaration(version, encoding, standalone, out),
            Kind::Unknown => {
                indent(depth, out);
                out.push(b'<');
                out.extend_from_slice(c_str(&n.value));
                out.push(b'>');
            }
        }
    }

    /// `TiXmlDocument::SaveFile` payload, including the BOM it was loaded with.
    pub fn print_file(&self) -> Vec<u8> {
        let mut out = if self.use_bom {
            vec![0xef, 0xbb, 0xbf]
        } else {
            Vec::new()
        };
        self.print_node(DOC, 0, &mut out);
        out
    }

    fn print_stream_node(&self, id: NodeId, out: &mut Vec<u8>) {
        let n = self.node(id);
        match &n.kind {
            Kind::Document => self
                .children(id)
                .for_each(|c| self.print_stream_node(c, out)),
            Kind::Element => {
                let name = c_str(&n.value);
                out.push(b'<');
                out.extend_from_slice(name);
                for a in self.attrs(id) {
                    out.push(b' ');
                    print_attr(a, out);
                }
                if n.first.is_none() {
                    out.extend_from_slice(b" />");
                } else {
                    out.push(b'>');
                    self.children(id)
                        .for_each(|c| self.print_stream_node(c, out));
                    out.extend_from_slice(b"</");
                    out.extend_from_slice(name);
                    out.push(b'>');
                }
            }
            Kind::Comment => {
                out.extend_from_slice(b"<!--");
                out.extend_from_slice(c_str(&n.value));
                out.extend_from_slice(b"-->");
            }
            Kind::Text { cdata: true } => {
                out.extend_from_slice(b"<![CDATA[");
                out.extend_from_slice(c_str(&n.value));
                out.extend_from_slice(b"]]>");
            }
            Kind::Text { cdata: false } => encode_string(&n.value, out),
            Kind::Declaration {
                version,
                encoding,
                standalone,
            } => print_declaration(version, encoding, standalone, out),
            Kind::Unknown => {
                out.push(b'<');
                out.extend_from_slice(c_str(&n.value));
                out.push(b'>');
            }
        }
    }

    /// `operator<<(std::string&, TiXmlNode&)`.
    pub fn print_stream(&self) -> Vec<u8> {
        let mut out = Vec::new();
        self.print_stream_node(DOC, &mut out);
        out
    }
}
