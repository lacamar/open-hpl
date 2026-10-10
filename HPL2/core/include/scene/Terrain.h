#ifndef HPL_TERRAIN_H
#define HPL_TERRAIN_H

#include <vector>
#include "math/Math.h"

namespace hpl {

	class cTerrain
	{
	public:
		cTerrain(int alSize, float afUnit, std::vector<float> avHeight) : mlSize(alSize), mfUnit(afUnit), mvHeight(std::move(avHeight)) {}

		bool GetWorldPosHeightAndNormal(const cVector3f& avPosition, float& afHeight, cVector3f& avNormal)
		{
			const float fOffset = mlSize * mfUnit * 0.5f;
			const float x = (avPosition.x + fOffset) / mfUnit, z = (avPosition.z + fOffset) / mfUnit;
			if (!(x > 0 && x < mlSize && z > 0 && z < mlSize)) return false;
			const int x0 = (int)x, z0 = (int)z;
			const float fx = x - x0, fz = z - z0;
			afHeight = (H(x0, z0) * (1 - fx) + H(x0 + 1, z0) * fx) * (1 - fz) + (H(x0, z0 + 1) * (1 - fx) + H(x0 + 1, z0 + 1) * fx) * fz;
			avNormal = cMath::Vector3Normalize(cVector3f(H(x0 - 1, z0) - H(x0 + 1, z0), 2 * mfUnit, H(x0, z0 - 1) - H(x0, z0 + 1)));
			return true;
		}

		void SetCheapMaterial(const tString&, float) {}

	private:
		float H(int x, int z) { return mvHeight[(size_t)cMath::Clamp(z, 0, mlSize - 1) * mlSize + cMath::Clamp(x, 0, mlSize - 1)]; }

		int mlSize;
		float mfUnit;
		std::vector<float> mvHeight;
	};

}

#endif
