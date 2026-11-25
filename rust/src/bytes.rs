use std::ops::Range;

pub fn sl(d: &[u8], r: Range<usize>) -> &[u8] {
    d.get(r).unwrap_or_default()
}

pub fn tail(d: &[u8], from: usize) -> &[u8] {
    d.get(from..).unwrap_or_default()
}

pub fn has_at(d: &[u8], at: usize, pat: &[u8]) -> bool {
    d.get(at..).is_some_and(|t| t.starts_with(pat))
}

pub fn find_from(hay: &[u8], needle: &[u8], from: usize) -> Option<usize> {
    if from > hay.len() || hay.len() - from < needle.len() {
        return None;
    }
    (from..=hay.len() - needle.len()).find(|&i| sl(hay, i..i + needle.len()) == needle)
}

pub fn find(hay: &[u8], needle: &[u8]) -> Option<usize> {
    find_from(hay, needle, 0)
}

pub fn rfind_upto(hay: &[u8], needle: &[u8], max_start: usize) -> Option<usize> {
    if hay.len() < needle.len() {
        return None;
    }
    let top = max_start.min(hay.len() - needle.len());
    (0..=top)
        .rev()
        .find(|&i| sl(hay, i..i + needle.len()) == needle)
}

pub fn rfind(hay: &[u8], needle: &[u8]) -> Option<usize> {
    rfind_upto(hay, needle, usize::MAX)
}

pub fn is_space(c: u8) -> bool {
    matches!(c, b' ' | b'\t' | b'\n' | 0x0b | 0x0c | b'\r')
}

pub fn is_word(c: u8) -> bool {
    c.is_ascii_alphanumeric() || c == b'_'
}

pub fn trim(s: &[u8]) -> &[u8] {
    let keep = |c: &u8| !matches!(c, b' ' | b'\t' | b'\r' | b'\n');
    let Some(b) = s.iter().position(keep) else {
        return &[];
    };
    let e = s.iter().rposition(keep).unwrap_or(b);
    s.get(b..=e).unwrap_or_default()
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn searches() {
        assert_eq!(find(b"abcabc", b"ca"), Some(2));
        assert_eq!(find(b"abc", b""), Some(0));
        assert_eq!(find_from(b"abcabc", b"abc", 1), Some(3));
        assert_eq!(find_from(b"abc", b"", 3), Some(3));
        assert_eq!(find_from(b"abc", b"", 4), None);
        assert_eq!(rfind(b"abcabc", b"abc"), Some(3));
        assert_eq!(rfind_upto(b"abcabc", b"abc", 2), Some(0));
        assert_eq!(rfind(b"ab", b"abc"), None);
    }

    #[test]
    fn classes_and_trim() {
        assert!(is_space(0x0b) && is_space(b' ') && !is_space(b'a'));
        assert!(is_word(b'_') && is_word(b'Z') && !is_word(b'-'));
        assert_eq!(trim(b" \t a b\r\n"), b"a b");
        assert_eq!(trim(b" \n"), b"");
        assert_eq!(sl(b"abc", 1..9), b"");
        assert_eq!(tail(b"abc", 1), b"bc");
        assert!(has_at(b"abc", 1, b"bc") && !has_at(b"abc", 5, b""));
    }
}
