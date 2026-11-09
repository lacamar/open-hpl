/*
 * Copyright © 2009-2020 Frictional Games
 *
 * This file is part of Amnesia: The Dark Descent.
 *
 * Amnesia: The Dark Descent is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.

 * Amnesia: The Dark Descent is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with Amnesia: The Dark Descent.  If not, see <https://www.gnu.org/licenses/>.
 */

#ifndef HPL_CRC_H
#define HPL_CRC_H

#include <cstddef>
#include <cstdint>

extern "C" {
	struct OhplCrc;
	OhplCrc* ohpl_crc_new(uint32_t key);
	void ohpl_crc_free(OhplCrc* crc);
	void ohpl_crc_put_data(OhplCrc* crc, const uint8_t* data, size_t len);
	uint32_t ohpl_crc_done(OhplCrc* crc);
}

namespace hpl {

	typedef unsigned int tCRCKey;

	class cCRC
	{
	public:
		cCRC(tCRCKey key) : mpImpl(ohpl_crc_new(key)) {}
		~cCRC() { ohpl_crc_free(mpImpl); }
		cCRC(const cCRC&) = delete;
		cCRC& operator=(const cCRC&) = delete;

		// Values above 255 indexed past the table in the C++ original (UB); Rust takes the low byte.
		void PutByte(unsigned aByte) { uint8_t b = (uint8_t)aByte; ohpl_crc_put_data(mpImpl, &b, 1); }
		void PutData(char *apData, size_t alSize) { ohpl_crc_put_data(mpImpl, (const uint8_t*)apData, alSize); }
		tCRCKey Done() { return ohpl_crc_done(mpImpl); }

	private:
		OhplCrc* mpImpl;
	};

}

#endif // HPL_CRC_H
