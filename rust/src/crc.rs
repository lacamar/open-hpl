//! Table-driven MSB-first CRC-32 with a caller-chosen polynomial (`hpl::cCRC`).

pub struct Crc {
    table: [u32; 256],
    register: u32,
}

impl Crc {
    pub fn new(key: u32) -> Self {
        let table = std::array::from_fn(|i| {
            (0..8).fold((i as u32) << 24, |reg, _| {
                if reg & 0x8000_0000 != 0 {
                    (reg << 1) ^ key
                } else {
                    reg << 1
                }
            })
        });
        Self { table, register: 0 }
    }

    #[expect(clippy::indexing_slicing, reason = "256-entry table indexed by a u8")]
    pub fn put_byte(&mut self, byte: u8) {
        let top = (self.register >> 24) as u8 ^ byte;
        self.register = (self.register << 8) ^ self.table[usize::from(top)];
    }

    pub fn put_data(&mut self, data: &[u8]) {
        data.iter().for_each(|&b| self.put_byte(b));
    }

    pub fn done(&mut self) -> u32 {
        std::mem::take(&mut self.register)
    }
}

#[cfg(test)]
mod tests {
    use super::Crc;

    // Values recorded from the C++ cCRC before cut-over.
    #[test]
    fn matches_cpp_characterization() {
        let cases: &[(u32, &[u8], u32)] = &[
            (0x04C1_1DB7, b"123456789", 0x89A1_897F),
            (1337, b"123456789", 0x1AB2_B326),
            (1337, b"", 0),
            (0, b"anything", 0),
        ];
        for &(key, data, want) in cases {
            let mut c = Crc::new(key);
            c.put_data(data);
            assert_eq!(c.done(), want, "key {key:#x} data {data:?}");
            assert_eq!(c.done(), 0);
        }
    }
}
