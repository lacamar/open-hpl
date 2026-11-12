//! `cSDLFontData::CreateFromBitmapFile`: the BMFont XML read into an owned description. Page
//! bitmaps, textures and glyph objects stay with the C++ shell.

use crate::imageload::fontdata::{self, Header};
use crate::string::token_to_int;
use crate::xml::{DOC, Doc, NodeId};

#[derive(Debug, PartialEq)]
pub enum Error {
    Parse,
    Lacks,
    /// The largest char id the glyph table cannot hold.
    Unreasonable(i32),
}

#[derive(Debug, Clone, Copy, PartialEq, Eq)]
pub enum GlyphStatus {
    Ok,
    InvalidId,
    InvalidPage,
}

#[derive(Debug, Clone, Copy, PartialEq)]
pub struct Glyph {
    pub id: i32,
    pub x: i32,
    pub y: i32,
    pub width: i32,
    pub height: i32,
    pub x_offset: i32,
    pub y_offset: i32,
    pub advance: i32,
    pub page: i32,
    pub status: GlyphStatus,
}

#[derive(Debug)]
pub struct Font {
    pub header: Header,
    pub largest_id: i32,
    pub glyph_count: usize,
    pub pages: Vec<Vec<u8>>,
    pub glyphs: Vec<Glyph>,
}

fn int(doc: &Doc, n: NodeId, name: &[u8]) -> i32 {
    doc.attribute(n, name).map_or(0, token_to_int)
}

fn children<'a>(doc: &'a Doc, parent: NodeId, name: &'a [u8]) -> impl Iterator<Item = NodeId> + 'a {
    std::iter::successors(doc.first_child_element(parent, Some(name)), move |&c| {
        doc.next_sibling_element(c, Some(name))
    })
}

pub fn parse(data: &[u8]) -> Result<Font, Error> {
    let mut doc = Doc::new();
    if !doc.load(data, true) {
        return Err(Error::Parse);
    }
    let root = doc.first_child_element(DOC, None);
    let sub = |name: &[u8]| root.and_then(|r| doc.first_child_element(r, Some(name)));
    let (Some(common), Some(info), Some(chars), Some(pages)) =
        (sub(b"common"), sub(b"info"), sub(b"chars"), sub(b"pages"))
    else {
        return Err(Error::Lacks);
    };

    let outline = int(&doc, info, b"outline");
    let header = fontdata::header(
        outline,
        int(&doc, common, b"lineHeight"),
        int(&doc, common, b"base"),
    );

    let largest_id = children(&doc, chars, b"char")
        .map(|c| int(&doc, c, b"id"))
        .fold(-1, i32::max);
    let glyph_count = fontdata::glyph_count(largest_id).ok_or(Error::Unreasonable(largest_id))?;

    let page_files: Vec<Vec<u8>> = children(&doc, pages, b"page")
        .map(|p| doc.attribute(p, b"file").unwrap_or_default().to_vec())
        .collect();

    let glyphs = children(&doc, chars, b"char")
        .map(|c| {
            let id = int(&doc, c, b"id");
            let page = int(&doc, c, b"page");
            let status = if !fontdata::glyph_id_valid(id, glyph_count) {
                GlyphStatus::InvalidId
            } else if !fontdata::page_valid(page, page_files.len()) {
                GlyphStatus::InvalidPage
            } else {
                GlyphStatus::Ok
            };
            Glyph {
                id,
                x: int(&doc, c, b"x"),
                y: int(&doc, c, b"y"),
                width: int(&doc, c, b"width"),
                height: int(&doc, c, b"height"),
                x_offset: int(&doc, c, b"xoffset"),
                y_offset: int(&doc, c, b"yoffset"),
                advance: fontdata::glyph_advance(int(&doc, c, b"xadvance"), outline),
                page,
                status,
            }
        })
        .collect();

    Ok(Font {
        header,
        largest_id,
        glyph_count,
        pages: page_files,
        glyphs,
    })
}
