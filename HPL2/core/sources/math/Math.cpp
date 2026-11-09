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

#include "math/Math.h"

#include "math/Frustum.h"
#include "math/RustMath.h"
#include "graphics/VertexBuffer.h"

#include "system/LowLevelSystem.h"

#include <stdlib.h>
#include <math.h>
#include <time.h>
#include <map>

namespace hpl {

	static char mpTempChar[1024];

	int cMath::RandRectl(int alMin, int alMax) { return ohpl_math_rand_rectl(alMin, alMax); }
	float cMath::RandRectf(float afMin, float afMax) { return ohpl_math_rand_rectf(afMin, afMax); }
	float cMath::FastRandomFloat(int alSeed) { return ohpl_math_fast_random_float(alSeed); }

	cVector2f cMath::RandRectVector2f(const cVector2f &avMin,const cVector2f &avMax)
	{
		cVector2f vOut;
		ohpl_math_rand_rect_vector2f(&avMin, &avMax, &vOut);
		return vOut;
	}

	cVector3f cMath::RandRectVector3f(const cVector3f &avMin,const cVector3f &avMax)
	{
		cVector3f vOut;
		ohpl_math_rand_rect_vector3f(&avMin, &avMax, &vOut);
		return vOut;
	}

	cColor cMath::RandRectColor(const cColor &aMin,const cColor &aMax)
	{
		cColor col;
		ohpl_math_rand_rect_color(&aMin, &aMax, &col);
		return col;
	}

	void cMath::Randomize(int alSeed) { ohpl_math_randomize(alSeed); }

	int cMath::GetFlagBit(int alBitNum) { return ohpl_math_get_flag_bit(alBitNum); }

	void cMath::ExpandAABB(cVector3f& avBaseMin,cVector3f& avBaseMax, const cVector3f& avAddMin, const cVector3f& avAddMax)
	{
		ohpl_math_expand_aabb(&avBaseMin, &avBaseMax, &avAddMin, &avAddMax);
	}

			bool cMath::CheckPointInRectIntersection(const cVector2f& avPoint, const cRect2f& aRect) { return ohpl_math_check_point_in_rect(&avPoint, &aRect); }
	bool cMath::CheckRectFit(const cRect2l& aRectSrc, const cRect2l& aRectDest) { return ohpl_math_check_rect_fit_l(&aRectSrc, &aRectDest); }
	bool cMath::CheckRectFit(const cRect2f& aRectSrc, const cRect2f& aRectDest) { return ohpl_math_check_rect_fit_f(&aRectSrc, &aRectDest); }

	cRect2f cMath::GetClipRect(const cRect2f& aRectSrc,const cRect2f& aRectDest)
	{
		cRect2f rect;
		ohpl_math_get_clip_rect(&aRectSrc, &aRectDest, &rect);
		return rect;
	}

	bool cMath::CheckSphereLineIntersection(const cVector3f& avSpherePos, float afSphereRadius, const cVector3f &avLineStart, const cVector3f &avLineEnd, float *apT1, cVector3f* apIntersection1, float *apT2, cVector3f* apIntersection2)
	{
		return ohpl_math_check_sphere_line_intersection(&avSpherePos, afSphereRadius, &avLineStart, &avLineEnd, apT1, apIntersection1, apT2, apIntersection2);
	}


	bool cMath::CheckPointInFlatPolygon( const cVector3f& avRayStart, const cVector3f& avRayEnd,
										 const cVector2f& avPoint, const cVector2f& avViewportSize,
										 int alNumIndices, unsigned int* avIndices, float* avVertexPos, int alStride,
										 const cMatrixf& amtxObjWorldMatrix,
										 const cMatrixf& amtxCamViewMatrix, const cMatrixf& amtxCamProjMatrix,
										 unsigned int* apIntersectedTriIndex, cVector3f* apIntersectionWorldSpace, tVector3fVec* apTriangle)
	{
		////////////////////////////////////////////////////////////////////////////////////////////////
		// HUGE inconvenience with this algo: if one of the vertices of a triangle lies
		// behind the camera, the normal is inverted and the triangle automatically skipped.
		// No improvement if forced to check anyway, as the CheckPointInsideTriangle2D will also fail
		// because of this.
		// Need to find a way around for this.
		bool bInside = false;

		cVector2f vPoint = (avPoint*2)/avViewportSize - 1;

		cMatrixf mtxScreenSpaceTransform = cMath::MatrixMul(amtxCamProjMatrix,
															amtxCamViewMatrix);

		cVector3f vForward = cVector3f(0,0,1);

		float fShortestT = 1000.0f;

		unsigned int lTriIndex;

		cVector3f vClosestIntersection;
		tVector3fVec vTriangleWorldSpace;
		tVector3fVec vTriangleMinDistWorldSpace;
		tVector3fVec vTriangleScreenSpace;
		vTriangleWorldSpace.resize(3);
		vTriangleMinDistWorldSpace.resize(3);
		vTriangleScreenSpace.resize(3);

		cPlanef trianglePlane;

		for(int i=0; i<alNumIndices;i+=3)
		{
			for(int j=0;j<3;++j)
			{
				int lBaseIndex = avIndices[i+j]*alStride;
				vTriangleWorldSpace[j] = cMath::MatrixMul(amtxObjWorldMatrix,
														  cVector3f(
																	avVertexPos[lBaseIndex],
																	avVertexPos[lBaseIndex+1],
																	avVertexPos[lBaseIndex+2])
														 );
				vTriangleScreenSpace[j] = cMath::MatrixMulDivideW(mtxScreenSpaceTransform, vTriangleWorldSpace[j]);
			}

			//////////////////////////////////////////////////////////
			// Check if triangle is facing away and skip if so

			cVector3f vEdge1 = vTriangleScreenSpace[1] - vTriangleScreenSpace[0];
			cVector3f vEdge2 = vTriangleScreenSpace[2] - vTriangleScreenSpace[0];

			cVector3f vTriNormal = cMath::Vector3Cross(vEdge1, vEdge2);
			vTriNormal.Normalize();

			float fDotProduct = cMath::Vector3Dot(vTriNormal, vForward);
			if(fDotProduct >= -kEpsilonf)
				continue;

			float fT;
			cVector3f vIntersection;
			if(cMath::CheckPointInsideTriangle2D(vPoint, vTriangleScreenSpace))
			{
				trianglePlane.FromPoints(vTriangleWorldSpace[0],vTriangleWorldSpace[1],vTriangleWorldSpace[2]);
				if(cMath::CheckPlaneLineIntersection(trianglePlane, avRayStart, avRayEnd, &vIntersection, &fT))
				{
					if(fT<fShortestT)
					{
						fShortestT = fT;
						lTriIndex = i;

						bInside = true;

						vTriangleMinDistWorldSpace[0] = vTriangleWorldSpace[0];
						vTriangleMinDistWorldSpace[1] = vTriangleWorldSpace[1];
						vTriangleMinDistWorldSpace[2] = vTriangleWorldSpace[2];

						vClosestIntersection = vIntersection;
					}
				}
			}
		}

		if(bInside)
		{
			if(apIntersectedTriIndex)
				*apIntersectedTriIndex = lTriIndex;
			if(apIntersectionWorldSpace)
				*apIntersectionWorldSpace = vClosestIntersection;
			if(apTriangle)
			{
				apTriangle->clear();
				*apTriangle = vTriangleMinDistWorldSpace;
			}
		}

		return bInside;
	}

	bool cMath::CheckPointInsideTriangle2D(const cVector2f& avPoint, const tVector3fVec& avTriangle)
	{
		return ohpl_math_check_point_inside_triangle_2d(&avPoint, avTriangle.data());
	}

	bool cMath::CheckPointSameSideOfEdge2D(const cVector3f& avPoint1, const cVector3f& avPoint2, const cVector3f& avPointA, const cVector3f& avPointB)
	{
		return ohpl_math_check_point_same_side_of_edge_2d(&avPoint1, &avPoint2, &avPointA, &avPointB);
	}


	bool cMath::CheckBVIntersection(cBoundingVolume& aBV1,cBoundingVolume& aBV2)
	{
		//////////////////////////////////////////
		//Check Sphere collision.
		//Not needed I think.
		/*float fRadiusSum = aBV1->GetRadius() + aBV2->GetRadius();
		cVector3f vSepAxis = aBV1->GetWorldCenter() - aBV2->GetWorldCenter();
		if(vSepAxis.SqrLength() > (fRadiusSum * fRadiusSum))
		{
		return eBVCollision_Outside;
		}*/

		///////////////////////////////////////////
		//Check box collision
		aBV1.UpdateSize();
		aBV2.UpdateSize();

		const cVector3f &vMin1 = aBV1.mvWorldMin;
		const cVector3f &vMin2 = aBV2.mvWorldMin;

		const cVector3f &vMax1 = aBV1.mvWorldMax;
		const cVector3f &vMax2 = aBV2.mvWorldMax;

		//Log("Min1: %s Max1: %s  vs  Min2: %s Max2: %s\n", vMin1.ToString().c_str(),vMax1.ToString().c_str(),
		//												vMin2.ToString().c_str(), vMax2.ToString().c_str());

		if(vMax1.x < vMin2.x || vMax1.y < vMin2.y || vMax1.z < vMin2.z ||
			vMax2.x < vMin1.x || vMax2.y < vMin1.y || vMax2.z < vMin1.z)
		{
			return false;
		}

		return true;
	}

	bool cMath::CheckAABBIntersection(	const cVector3f& avMin1,const cVector3f& avMax1,
										const cVector3f& avMin2,const cVector3f& avMax2)
	{
		return ohpl_math_check_aabb_intersection(&avMin1, &avMax1, &avMin2, &avMax2);
	}

	bool cMath::CheckAABBInside(const cVector3f& avInsideMin,const cVector3f& avInsideMax,
								const cVector3f& avOutsideMin,const cVector3f& avOutsideMax)
	{
		return ohpl_math_check_aabb_inside(&avInsideMin, &avInsideMax, &avOutsideMin, &avOutsideMax);
	}


	bool cMath::CheckPointInBVIntersection(const cVector3f& avPoint, cBoundingVolume& aBV)
	{
		cVector3f vMax = aBV.GetMax();
		cVector3f vMin = aBV.GetMin();

		if(avPoint.x > vMax.x || avPoint.y > vMax.y || avPoint.z > vMax.z ||
			avPoint.x < vMin.x || avPoint.y < vMin.y || avPoint.z < vMin.z)
		{
			return false;
		}

		return true;
	}

	bool cMath::CheckPointInAABBIntersection(const cVector3f& avPoint, const cVector3f& avMin,const cVector3f& avMax)
	{
		return ohpl_math_check_point_in_aabb(&avPoint, &avMin, &avMax);
	}

	bool cMath::GetNormalizedClipRectFromBV(cVector3f &avDestMin, cVector3f &avDestMax,
											cBoundingVolume &aBV, cFrustum *apFrustum,
											float afTanHalfFov)
	{
		const cVector3f &vMax = aBV.GetMax();
		const cVector3f &vMin = aBV.GetMin();
		return ohpl_math_get_normalized_clip_rect_from_bv(&vMin, &vMax, &apFrustum->GetViewMatrix(), &apFrustum->GetProjectionMatrix(),
			apFrustum->GetProjectionType()==eProjectionType_Perspective, apFrustum->GetFOV(), apFrustum->GetNearPlane(), apFrustum->GetAspect(),
			afTanHalfFov, &avDestMin, &avDestMax);
	}

	cRect2l cMath::GetClipRectFromNormalizedMinMax(const cVector3f &avMin, const cVector3f& avMax, const cVector2l &avScreenSize)
	{
		cRect2l rect;
		ohpl_math_get_clip_rect_from_normalized_min_max(&avMin, &avMax, &avScreenSize, &rect);
		return rect;
	}


	void cMath::GetClipRectFromBV(	cRect2l &aDestRect, cBoundingVolume &aBV,
									cFrustum *apFrustum, const cVector2l &avScreenSize,
									float afTanHalfFov)
	{
		////////////////////////
		//Get the normalized clip coordinates
		cVector3f vMin, vMax;
		bool bVisible = GetNormalizedClipRectFromBV(vMin, vMax, aBV, apFrustum, afTanHalfFov);

		////////////////////////
		// If no point in front, it is not visible and just return full screen rect.
		if(bVisible == false)
		{
			aDestRect.x = 0;
			aDestRect.y = 0;
			aDestRect.w = avScreenSize.x;
			aDestRect.h = avScreenSize.y;
			return;
		}

		aDestRect = GetClipRectFromNormalizedMinMax(vMin, vMax, avScreenSize);
	}

	cRect2l cMath::GetClipRectFromSphere(	const cVector3f& avPosition, float afRadius,
											cFrustum *apFrustum,
											const cVector2l &avScreenSize,
											bool abPositionIsScreenSpace, float afTanHalfFov)
	{
		cRect2l rect;
		ohpl_math_get_clip_rect_from_sphere(&avPosition, afRadius, &apFrustum->GetViewMatrix(), apFrustum->GetFOV(), apFrustum->GetNearPlane(),
			apFrustum->GetAspect(), &avScreenSize, abPositionIsScreenSpace, afTanHalfFov, &rect);
		return rect;
	}

	cVector2f cMath::GetSphericalCoordsFromPoint3D(const cVector3f& avSphCenter, const cVector3f& avPoint)
	{
		cVector2f vOut;
		ohpl_math_get_spherical_coords_from_point_3d(&avSphCenter, &avPoint, &vOut);
		return vOut;
	}

	cVector3f cMath::GetPoint3DFromSphericalCoords(const cVector3f& avSphCenter, float afSphRadius, const cVector2f& avSphCoords)
	{
		cVector3f vOut;
		ohpl_math_get_point_3d_from_spherical_coords(&avSphCenter, afSphRadius, &avSphCoords, &vOut);
		return vOut;
	}

	float cMath::PlaneDot(const cPlanef& aPlaneA, const cPlanef& aPlaneB) { return ohpl_math_plane_dot(&aPlaneA, &aPlaneB); }
	bool cMath::PlaneParallel(const cPlanef& aPlaneA, const cPlanef& aPlaneB) { return ohpl_math_plane_parallel(&aPlaneA, &aPlaneB); }

	cPlanef cMath::TransformPlane(const cMatrixf &a_mtxTransform, const cPlanef& aPlane)
	{
		cPlanef plane;
		ohpl_math_transform_plane(&a_mtxTransform, &aPlane, &plane);
		return plane;
	}


	bool cMath::CheckAABBLineIntersection(	const cVector3f& avMin,const cVector3f& avMax,
											const cVector3f &avLineStart, const cVector3f &avLineEnd,
											cVector3f *apIntersectionPos, float *apT)
	{
		return ohpl_math_check_aabb_line_intersection(&avMin, &avMax, &avLineStart, &avLineEnd, apIntersectionPos, apT);
	}

	float cMath::RoundFloatToDecimals(float afVal, int alPrecision) { return ohpl_math_round_float_to_decimals(afVal, alPrecision); }
	float cMath::GetFraction(float afVal) { return ohpl_math_get_fraction(afVal); }
	float cMath::Modulus(float afDividend, float afDivisor) { return ohpl_math_modulus(afDividend, afDivisor); }
	float cMath::Wrap(float afX, float afMin, float afMax) { return ohpl_math_wrap(afX, afMin, afMax); }
	float cMath::Clamp(float afX, float afMin, float afMax) { return ohpl_math_clamp(afX, afMin, afMax); }
	float cMath::GetAngleDistanceRad(float afAngle1, float afAngle2) { return GetAngleDistance(afAngle1, afAngle2,k2Pif); }
	float cMath::GetAngleDistanceDeg(float afAngle1, float afAngle2) { return GetAngleDistance(afAngle1, afAngle2,360.0f); }
	float cMath::GetAngleDistance(float afAngle1, float afAngle2, float afMaxAngle) { return ohpl_math_get_angle_distance(afAngle1, afAngle2, afMaxAngle); }
	float cMath::TurnAngle(float afAngle,float afFinalAngle,float afSpeed,float afMaxAngle) { return ohpl_math_turn_angle(afAngle, afFinalAngle, afSpeed, afMaxAngle); }
	float cMath::TurnAngleRad(float afAngle,float afFinalAngle,float afSpeed) { return TurnAngle(afAngle, afFinalAngle, afSpeed,kPif); }
	float cMath::TurnAngleDeg(float afAngle,float afFinalAngle,float afSpeed) { return TurnAngle(afAngle, afFinalAngle, afSpeed,180.0f); }
	float cMath::GetAngleFromPoints2D(const cVector2f &avStartPos, const cVector2f &avGoalPos) { return ohpl_math_get_angle_from_points_2d(&avStartPos, &avGoalPos); }
	float cMath::ToRad(float afAngle) { return ohpl_math_to_rad(afAngle); }
	float cMath::ToDeg(float afAngle) { return ohpl_math_to_deg(afAngle); }
	int cMath::Log2ToInt(int alX) { return ohpl_math_log2_to_int(alX); }
	int cMath::Pow2(int alX) { return 1 << alX; }
	bool cMath::IsPow2(int alX) { return ohpl_math_is_pow2(alX); }
	float cMath::Interpolate(float afA,float afB,float afT) { return ohpl_math_interpolate(afA, afB, afT); }
	float cMath::IncreaseTo(float afX, float afAdd, float afDest) { return ohpl_math_increase_to(afX, afAdd, afDest); }
	float cMath::SmoothCurve(float afX) { return ohpl_math_smooth_curve(afX); }

	float cMath::Vector2Dist(const cVector2f &avPosA,const cVector2f &avPosB) { return ohpl_math_vector2_dist(&avPosA, &avPosB); }
	float cMath::Vector2DistXY(const cVector3f &avPosA,const cVector3f &avPosB) { return ohpl_math_vector2_dist_xy(&avPosA, &avPosB); }
	float cMath::Vector2DistXZ(const cVector3f &avPosA,const cVector3f &avPosB) { return ohpl_math_vector2_dist_xz(&avPosA, &avPosB); }
	float cMath::Vector2DistSqr(const cVector2f &avPosA,const cVector2f &avPosB) { return ohpl_math_vector2_dist_sqr(&avPosA, &avPosB); }
	float cMath::Vector2DistSqrXY(const cVector3f &avPosA,const cVector3f &avPosB) { return ohpl_math_vector2_dist_sqr_xy(&avPosA, &avPosB); }
	float cMath::Vector2DistSqrXZ(const cVector3f &avPosA,const cVector3f &avPosB) { return ohpl_math_vector2_dist_sqr_xz(&avPosA, &avPosB); }

	cVector2f cMath::Vector2IncreaseTo(const cVector2f& avX, const cVector2f& avAdd, const cVector2f& avDest)
	{
		cVector2f vRet;
		ohpl_math_vector2_increase_to(&avX, &avAdd, &avDest, &vRet);
		return vRet;
	}

	cVector2f cMath::GetVectorFromAngle2D(float afAngle, float afLength)
	{
		cVector2f vVec;
		ohpl_math_get_vector_from_angle_2d(afAngle, afLength, &vVec);
		return vVec;
	}

	void cMath::GetAngleFromVector(const cVector2f &avVec, float *apAngle, float *apLength)
	{
		ohpl_math_get_angle_from_vector(&avVec, apAngle, apLength);
	}

	cVector2f cMath::ProjectVector2D(const cVector2f &avSrcVec, const cVector2f &avDestVec)
	{
		cVector2f vOut;
		ohpl_math_project_vector_2d(&avSrcVec, &avDestVec, &vOut);
		return vOut;
	}

	cVector3f cMath::Vector3MaxLength(const cVector3f &avVec, float afMaxLength)
	{
		cVector3f vOut;
		ohpl_math_vector3_max_length(&avVec, afMaxLength, &vOut);
		return vOut;
	}

	cVector3f cMath::Vector3MinLength(const cVector3f &avVec, float afMinLength)
	{
		cVector3f vOut;
		ohpl_math_vector3_min_length(&avVec, afMinLength, &vOut);
		return vOut;
	}

	cVector3f cMath::Vector3ClampLength(const cVector3f &avVec,float afMinLength, float afMaxLength)
	{
		cVector3f vOut;
		ohpl_math_vector3_clamp_length(&avVec, afMinLength, afMaxLength, &vOut);
		return vOut;
	}

	cVector3f cMath::Vector3AngleDistance(const cVector3f &avAngles1, const cVector3f &avAngles2, float afMaxAngle)
	{
		cVector3f vOut;
		ohpl_math_vector3_angle_distance(&avAngles1, &avAngles2, afMaxAngle, &vOut);
		return vOut;
	}

	cVector3f cMath::Vector3AngleDistanceRad(const cVector3f &avAngles1, const cVector3f &avAngles2) { return Vector3AngleDistance(avAngles1, avAngles2, k2Pif); }
	cVector3f cMath::Vector3AngleDistanceDeg(const cVector3f &avAngles1, const cVector3f &avAngles2) { return Vector3AngleDistance(avAngles1, avAngles2, 360.0f); }

	cVector3f cMath::Vector3Cross(const cVector3f& avVecA,const cVector3f& avVecB)
	{
		cVector3f vOut;
		ohpl_math_vector3_cross(&avVecA, &avVecB, &vOut);
		return vOut;
	}

	float cMath::Vector3Dot(const cVector3f& avVecA,const cVector3f& avVecB) { return ohpl_math_vector3_dot(&avVecA, &avVecB); }

	cVector3f cMath::Vector3Project(const cVector3f &avSrcVec, const cVector3f &avDestVec)
	{
		cVector3f vOut;
		ohpl_math_vector3_project(&avSrcVec, &avDestVec, &vOut);
		return vOut;
	}

	cVector3f cMath::Vector3Reflect(const cVector3f &avVec, const cVector3f &avNormal)
	{
		cVector3f vOut;
		ohpl_math_vector3_reflect(&avVec, &avNormal, &vOut);
		return vOut;
	}

	float cMath::Vector3Angle(const cVector3f& avVecA,const cVector3f& avVecB) { return ohpl_math_vector3_angle(&avVecA, &avVecB); }

	cVector3f cMath::Vector3UnProject(const cVector3f& avVec,const cRect2f &aScreenRect, cMatrixf a_mtxViewProj)
	{
		cVector3f vOut;
		ohpl_math_vector3_unproject(&avVec, &aScreenRect, &a_mtxViewProj, &vOut);
		return vOut;
	}

	cVector3f cMath::Vector3IncreaseTo(const cVector3f& avX, const cVector3f& avAdd, const cVector3f& avDest)
	{
		cVector3f vRet;
		ohpl_math_vector3_increase_to(&avX, &avAdd, &avDest, &vRet);
		return vRet;
	}

	void cMath::Vector3ClampToLength(cVector3f& avVec, float afMaxLength) { ohpl_math_vector3_clamp_to_length(&avVec, afMaxLength); }

	cVector3f cMath::GetAngleFromPoints3D(const cVector3f &avStartPos, const cVector3f &avGoalPos)
	{
		cVector3f vAngle;
		if(ohpl_math_get_angle_from_points_3d(&avStartPos, &avGoalPos, &vAngle))
			Error("Error in GetAngle 3D code! ARGHHH run in terror\n");
		return vAngle;
	}

	void cMath::Vector3OrthonormalizeBasis(const cVector3f& av1, const cVector3f& av2, const cVector3f& av3,
											cVector3f& avDst1, cVector3f& avDst2, cVector3f& avDst3)
	{
		ohpl_math_vector3_orthonormalize_basis(&av1, &av2, &av3, &avDst1, &avDst2, &avDst3);
	}

	float cMath::PlaneToPointDist(const cPlanef& aPlane, const cVector3f& avVec) { return ohpl_math_plane_to_point_dist(&aPlane, &avVec); }

	void cMath::PlaneIntersectionLine(const cPlanef& aPA, const cPlanef& aPB, cVector3f &avDir, cVector3f &avPoint)
	{
		ohpl_math_plane_intersection_line(&aPA, &aPB, &avDir, &avPoint);
	}

	bool cMath::PlaneIntersectionPoint(const cPlanef& aP1, const cPlanef& aP2, const cPlanef& aP3, cVector3f& avPoint)
	{
		return ohpl_math_plane_intersection_point(&aP1, &aP2, &aP3, &avPoint);
	}

	eCollision cMath::CheckPlaneSphereCollision(const cPlanef& aPlane, const cVector3f &avCenter, float afRadius)
	{
		return (eCollision)ohpl_math_check_plane_sphere_collision(&aPlane, &avCenter, afRadius);
	}


	eCollision cMath::CheckPlaneBVCollision(const cPlanef& aPlane, cBoundingVolume &aBV)
	{
		return CheckPlaneAABBCollision(aPlane, aBV.GetMin(), aBV.GetMax());//, aBV.GetWorldCenter(), aBV.GetRadius());
	}


	eCollision cMath::CheckPlaneAABBCollision(	const cPlanef& aPlane, const cVector3f &avMin, const cVector3f &avMax)
	{
		return (eCollision)ohpl_math_check_plane_aabb_collision(&aPlane, &avMin, &avMax);
	}

	eCollision cMath::CheckPlaneAABBCollision(const cPlanef& aPlane, const cVector3f &avMin, const cVector3f &avMax, const cVector3f &avSphereCenter,
											float afSphereRadius)
	{
		return (eCollision)ohpl_math_check_plane_aabb_sphere_collision(&aPlane, &avMin, &avMax, &avSphereCenter, afSphereRadius);
	}

	bool cMath::CheckPlaneLineIntersection(	const cPlanef& aPlane, const cVector3f &avLineStart, const cVector3f &avLineEnd,
											cVector3f* apIntersectionPos, float *apT)
	{
		return ohpl_math_check_plane_line_intersection(&aPlane, &avLineStart, &avLineEnd, apIntersectionPos, apT);
	}

	eCollision cMath::CheckPointsPlanesCollision(cVector3f *apVertices, int alNumOfVertices, cPlanef *apPlanes, int alNumOfPlanes)
	{
		return (eCollision)ohpl_math_check_points_planes_collision(apVertices, alNumOfVertices, apPlanes, alNumOfPlanes);
	}

	eCollision cMath::CheckPointsAABBPlanesCollision(cVector3f *apVertices, int alNumOfVertices, const cVector3f& avMin, const cVector3f& avMax)
	{
		return (eCollision)ohpl_math_check_points_aabb_planes_collision(apVertices, alNumOfVertices, &avMin, &avMax);
	}

	bool cMath::CheckSeparatingAxisIntersection(cVector3f *apVerticesA, int alNumOfVerticesA, cPlanef *apPlanesA, int alNumOfPlanesA,
												cVector3f *apVerticesB, int alNumOfVerticesB, cPlanef *apPlanesB, int alNumOfPlanesB)
	{
		return ohpl_math_check_separating_axis_intersection(apVerticesA, alNumOfVerticesA, apPlanesA, alNumOfPlanesA,
															apVerticesB, alNumOfVerticesB, apPlanesB, alNumOfPlanesB);
	}


	bool cMath::CheckLineTriMeshIntersection(	const cVector3f& avLineStart, const cVector3f& avLineEnd,
												const cMatrixf& a_mtxInvMeshMtx,
												const unsigned int* apIndexArray,int alIndexNum,
												const float* apVertexArray, int alVtxStride,
												cVector3f *apIntersectionPos, float *apT, int *apTriIndex, bool abSkipBackfacing)
	{
		return ohpl_math_check_line_tri_mesh_intersection(&avLineStart, &avLineEnd, &a_mtxInvMeshMtx, apIndexArray, alIndexNum,
														apVertexArray, alVtxStride, apIntersectionPos, apT, apTriIndex, abSkipBackfacing);
	}



	bool cMath::CheckLineTriVertexBufferIntersection( const cVector3f& avLineStart, const cVector3f& avLineEnd,
														const cMatrixf& a_mtxInvMeshMtx, iVertexBuffer *apVtxBuffer,
														cVector3f *apIntersectionPos, float *apT, int *apTriIndex, bool abSkipBackfacing)
	{
		return CheckLineTriMeshIntersection(avLineStart, avLineEnd,
											a_mtxInvMeshMtx,
											apVtxBuffer->GetIndices(), apVtxBuffer->GetIndexNum(),
											apVtxBuffer->GetFloatArray(eVertexBufferElement_Position),
											apVtxBuffer->GetElementNum(eVertexBufferElement_Position),
											apIntersectionPos, apT, apTriIndex, abSkipBackfacing);
	}


	float cMath::QuaternionDot(const cQuaternion& aqA,const cQuaternion& aqB) { return ohpl_math_quaternion_dot(&aqA, &aqB); }

	cQuaternion cMath::QuaternionMul(const cQuaternion& aqA,const cQuaternion& aqB)
	{
		cQuaternion qOut;
		ohpl_math_quaternion_mul(&aqA, &aqB, &qOut);
		return qOut;
	}

	cQuaternion cMath::QuaternionSlerp(float afT,const cQuaternion& aqA, const cQuaternion& aqB, bool abShortestPath)
	{
		cQuaternion qOut;
		ohpl_math_quaternion_slerp(afT, &aqA, &aqB, abShortestPath, &qOut);
		return qOut;
	}

	cMatrixf cMath::MatrixSlerp(float afT,const cMatrixf& a_mtxA, const cMatrixf& a_mtxB, bool abShortestPath)
	{
		cMatrixf mtx;
		ohpl_math_matrix_slerp(afT, &a_mtxA, &a_mtxB, &mtx);
		return mtx;
	}

	cMatrixf cMath::MatrixMul(const cMatrixf &a_mtxA,const cMatrixf &a_mtxB)
	{
		cMatrixf mtx;
		ohpl_math_matrix_mul(&a_mtxA, &a_mtxB, &mtx);
		return mtx;
	}

	cVector3f cMath::MatrixMul(const cMatrixf &a_mtxA,const cVector3f &avB)
	{
		cVector3f vOut;
		ohpl_math_matrix_mul_vec(&a_mtxA, &avB, &vOut);
		return vOut;
	}

	cVector3f cMath::MatrixMul3x3(const cMatrixf &a_mtxA,const cVector3f &avB)
	{
		cVector3f vOut;
		ohpl_math_matrix_mul_3x3(&a_mtxA, &avB, &vOut);
		return vOut;
	}

	cVector3f cMath::MatrixMulDivideW(const cMatrixf &a_mtxA,const cVector3f &avB)
	{
		cVector3f vOut;
		ohpl_math_matrix_mul_divide_w(&a_mtxA, &avB, &vOut);
		return vOut;
	}

	cMatrixf cMath::MatrixMulScalar(const cMatrixf &a_mtxA, float afB)
	{
		cMatrixf mtx;
		ohpl_math_matrix_mul_scalar(&a_mtxA, afB, &mtx);
		return mtx;
	}

	cMatrixf cMath::MatrixRotate(cVector3f avRot, eEulerRotationOrder aOrder)
	{
		cMatrixf mtx;
		ohpl_math_matrix_rotate(&avRot, (int)aOrder, &mtx);
		return mtx;
	}

	cMatrixf cMath::MatrixRotateX(float afAngle)
	{
		cMatrixf mtx;
		ohpl_math_matrix_rotate_x(afAngle, &mtx);
		return mtx;
	}

	cMatrixf cMath::MatrixRotateY(float afAngle)
	{
		cMatrixf mtx;
		ohpl_math_matrix_rotate_y(afAngle, &mtx);
		return mtx;
	}

	cMatrixf cMath::MatrixRotateZ(float afAngle)
	{
		cMatrixf mtx;
		ohpl_math_matrix_rotate_z(afAngle, &mtx);
		return mtx;
	}

	cMatrixf cMath::MatrixQuaternion(const cQuaternion &aqRot)
	{
		cMatrixf mtx;
		ohpl_math_matrix_quaternion(&aqRot, &mtx);
		return mtx;
	}

	cQuaternion cMath::MatrixToQuaternion(const cMatrixf &a_mtxA)
	{
		cQuaternion qOut;
		ohpl_math_matrix_to_quaternion(&a_mtxA, &qOut);
		return qOut;
	}

	cMatrixf cMath::MatrixScale(cVector3f avScale)
	{
		cMatrixf mtx;
		ohpl_math_matrix_scale(&avScale, &mtx);
		return mtx;
	}

	cMatrixf cMath::MatrixTranslate(cVector3f avTrans)
	{
		cMatrixf mtx;
		ohpl_math_matrix_translate(&avTrans, &mtx);
		return mtx;
	}


	cMatrixf cMath::MatrixPlaneMirror(const cPlanef &aPlane)
	{
		cMatrixf mtx;
		ohpl_math_matrix_plane_mirror(&aPlane, &mtx);
		return mtx;
	}

	cVector3f cMath::MatrixToEulerAngles(const cMatrixf &a_mtxA, eEulerRotationOrder aOrder)
	{
		cVector3f vOut;
		ohpl_math_matrix_to_euler_angles(&a_mtxA, &vOut);
		return vOut;
	}

	cVector3f cMath::MatrixEulerAngleDistance(const cMatrixf &a_mtxA, const cMatrixf &a_mtxB)
	{
		cVector3f vOut;
		ohpl_math_matrix_euler_angle_distance(&a_mtxA, &a_mtxB, &vOut);
		return vOut;
	}

	cMatrixf cMath::MatrixUnitVectors(const cVector3f& avRight, const cVector3f& avUp, const cVector3f& avForward, const cVector3f& avTranslate)
	{
		cMatrixf mtx;
		ohpl_math_matrix_unit_vectors(&avRight, &avUp, &avForward, &avTranslate, &mtx);
		return mtx;
	}

	cMatrixf cMath::ProjectionMatrixObliqueNearClipPlane(const cMatrixf &a_mtxProjMatrix, const cPlanef& aClipPlane)
	{
		cMatrixf mtx;
		ohpl_math_projection_matrix_oblique_near_clip_plane(&a_mtxProjMatrix, &aClipPlane, &mtx);
		return mtx;
	}

	cMatrixf cMath::MatrixPerspectiveProjection(float afNearClipPlane, float afFarClipPlane, float afFOV, float afAspect, bool abInfFarPlane)
	{
		cMatrixf mtx;
		ohpl_math_matrix_perspective_projection(afNearClipPlane, afFarClipPlane, afFOV, afAspect, abInfFarPlane, &mtx);
		return mtx;
	}

	cMatrixf cMath::MatrixOrthographicProjection(float afNearClipPlane, float afFarClipPlane, const cVector2f& avViewSize)
	{
		cMatrixf mtx;
		ohpl_math_matrix_orthographic_projection(afNearClipPlane, afFarClipPlane, &avViewSize, &mtx);
		return mtx;
	}


	const char* cMath::MatrixToChar(const cMatrixf &a_mtxA)
	{
		sprintf(mpTempChar,"[%.3f, %.3f, %.3f, %.3f] [%.3f, %.3f, %.3f, %.3f] [%.3f, %.3f, %.3f, %.3f] [%.3f, %.3f, %.3f, %.3f]",
							a_mtxA.m[0][0],a_mtxA.m[0][1],a_mtxA.m[0][2],a_mtxA.m[0][3],
							a_mtxA.m[1][0],a_mtxA.m[1][1],a_mtxA.m[1][2],a_mtxA.m[1][3],
							a_mtxA.m[2][0],a_mtxA.m[2][1],a_mtxA.m[2][2],a_mtxA.m[2][3],
							a_mtxA.m[3][0],a_mtxA.m[3][1],a_mtxA.m[3][2],a_mtxA.m[3][3]);

		return mpTempChar;
	}

	bool cMath::CreateTriTangentVectors(float* apDestArray,
										const unsigned int* apIndexArray,int alIndexNum,
										const float* apVertexArray, int alVtxStride,
										const float *apTexArray,
										const float *apNormalArray,
										int alVertexNum)
	{
		ohpl_math_create_tri_tangent_vectors(apDestArray, apIndexArray, alIndexNum, apVertexArray, alVtxStride, apTexArray, apNormalArray, alVertexNum);
		return true;
	}



	static inline cVector3f GetVector3(const float* apVertexArray, int alIdx,int alStride)
	{
		const float* apVec = &apVertexArray[alIdx *alStride];

		return cVector3f(apVec[0],apVec[1],apVec[2]);
	}

	static inline bool Vector3Equal(const float* apArrayA, int alIdxA,const float* apArrayB, int alIdxB,
					int alStride)
	{
		if(apArrayA[alIdxA*alStride + 0] == apArrayB[alIdxB*alStride + 0] &&
			apArrayA[alIdxA*alStride + 1] == apArrayB[alIdxB*alStride + 1] &&
			apArrayA[alIdxA*alStride + 2] == apArrayB[alIdxB*alStride + 2])
		{
			return true;
		}

		return false;
	}


	bool cMath::CreateTriangleData(tTriangleDataVec &avTriangles,
									const unsigned int* apIndexArray,int alIndexNum,
									const float* apVertexArray, int alVtxStride, int alVertexNum)
	{
		int lNumOfTri = alIndexNum / 3;
		if((int)avTriangles.size() < lNumOfTri) avTriangles.resize(lNumOfTri);

		//Log("Creating triangle data:\n");
		for(int tri=0, idx=0; tri < lNumOfTri; tri++,idx+=3)
		{
			//Log("checking: tri %d idx %d\n",tri, idx);
			//Calculate normal
			const float *pVtx0 = &apVertexArray[apIndexArray[idx]*alVtxStride];
			const float *pVtx1 = &apVertexArray[apIndexArray[idx+1]*alVtxStride];
			const float *pVtx2 = &apVertexArray[apIndexArray[idx+2]*alVtxStride];

			cVector3f vEdge1( pVtx1[0] - pVtx0[0], pVtx1[1] - pVtx0[1], pVtx1[2] - pVtx0[2]);
			cVector3f vEdge2( pVtx2[0] - pVtx0[0], pVtx2[1] - pVtx0[1], pVtx2[2] - pVtx0[2]);

			avTriangles[tri].normal = Vector3Cross(vEdge2, vEdge1);
		}

		return true;
	}


	//-----------------------------------------------------------------------

	static bool EdgePointEqual(const float* apVertexArray,
								const cTriEdge &edge1,  const cTriEdge &edge2, int alStride)
	{
		if(Vector3Equal(apVertexArray, edge1.point1,apVertexArray, edge2.point1,alStride) &&
			Vector3Equal(apVertexArray, edge1.point2,apVertexArray, edge2.point2,alStride))
		{
			return true;
		}

		if(Vector3Equal(apVertexArray, edge1.point1,apVertexArray, edge2.point2,alStride) &&
			Vector3Equal(apVertexArray, edge1.point2,apVertexArray, edge2.point1,alStride))
		{
			return true;
		}

		return false;
	}

	/////////////////////////

	static bool EdgeTriEqual(const cTriEdge &edge1,  const cTriEdge &edge2)
	{
		if(edge1.tri1 == edge2.tri1 && edge1.tri2 == edge2.tri2) return true;
		if(edge1.tri1 == edge1.tri1 && edge1.tri2 == edge2.tri1) return true;
		return false;
	}

	/////////////////////////

	static bool EdgeEqual(const float* apVertexArray, const cTriEdge &edge1,  const cTriEdge &edge2, int alStride)
	{
		if(EdgePointEqual(apVertexArray,edge1, edge2,alStride) && EdgeTriEqual(edge1, edge2)) return true;

		return false;
	}

	/////////////////////////

	static const float *gpVertexArray;
	static const unsigned int *gpIndexArray;
	static int glVertexStride;


	//////////////////////////////////////////////////////

	class cVertexIndices
	{
	public:
		cVertexIndices(unsigned int alIdx){
			mlstIndices.push_back(alIdx);
		}
		tUIntList mlstIndices;
	};

	typedef std::map<cVector3f,cVertexIndices> tVtxIdxMap;
	typedef tVtxIdxMap::iterator tVtxIdxMapIt;

	//////////////////////////////////////////////////////

	class cEdgeCompare
	{
	public:
		//The point1 must be > than point2 for this to work!
		bool operator()(const cTriEdge &Edge1, const cTriEdge &Edge2)const
		{
			cVector3f vPoint1_1 = GetVector3(gpVertexArray,Edge1.point1,glVertexStride);
			cVector3f vPoint1_2 = GetVector3(gpVertexArray,Edge1.point2,glVertexStride);
			cVector3f vPoint2_1 = GetVector3(gpVertexArray,Edge2.point1,glVertexStride);
			cVector3f vPoint2_2 = GetVector3(gpVertexArray,Edge2.point2,glVertexStride);

			//1 - 1
			if(vPoint1_1.x != vPoint2_1.x) return vPoint1_1.x > vPoint2_1.x;
			if(vPoint1_1.y != vPoint2_1.y) return vPoint1_1.y > vPoint2_1.y;
			if(vPoint1_1.z != vPoint2_1.z) return vPoint1_1.z > vPoint2_1.z;
			//2 - 2
			if(vPoint1_2.x != vPoint2_2.x) return vPoint1_2.x > vPoint2_2.x;
			if(vPoint1_2.y != vPoint2_2.y) return vPoint1_2.y > vPoint2_2.y;
			if(vPoint1_2.z != vPoint2_2.z) return vPoint1_2.z > vPoint2_2.z;

			return false;
		}
	};

	typedef std::set<cTriEdge, cEdgeCompare> tTriEdgeListMap;
	typedef tTriEdgeListMap::iterator tTriEdgeListMapIt;

	//////////////////////////////////////////////////////

	static void CheckEdgeSwitch(cTriEdge *apEdge)
	{
		cVector3f vPoint1 = GetVector3(gpVertexArray,apEdge->point1,glVertexStride);
		cVector3f vPoint2 = GetVector3(gpVertexArray,apEdge->point2,glVertexStride);
		if(vPoint1 < vPoint2){
			unsigned int lTemp = apEdge->point1;
			apEdge->point1 = apEdge->point2;
			apEdge->point2 = lTemp;
		}
	}

	//////////////////////////////////////////////////////

	void AddEdgeToMap(cTriEdge &aEdge, tTriEdgeListMap &aMap)
	{
		tTriEdgeListMapIt it = aMap.find(aEdge);
		//if edge is not added, create a new element.
		if(it == aMap.end())
		{
			aEdge.tri2 = -1;
			aMap.insert(aEdge);
		}
		//if added check if there already exist an edge with the triangles
		else
		{
			if(it->tri2 != -1) return;
			if(it->tri1 == aEdge.tri1) return;

			//Set the other triangle! voila our edge is done!
			it->tri2 = aEdge.tri1;
		}
	}

	//////////////////////////////////////////////////////

	bool cMath::CreateEdges(tTriEdgeVec &avEdges,
							const unsigned int* apIndexArray,int alIndexNum,
							const float* apVertexArray, int alVtxStride,int alVertexNum,
							bool *apIsDoubleSided)
	{
		const bool bLog= false;

		//Initial setup
		*apIsDoubleSided = false;

		tVtxIdxMap mapVtxIndices;
		tTriEdgeListMap mapTriEdgeLists;

		gpIndexArray = apIndexArray;
		gpVertexArray = apVertexArray;
		glVertexStride = alVtxStride;

		////////////////////////////////////////////////
		//Iterate indices and add all that reference the same vertex to
		// the same element in a map
		for(int idx=0; idx < alIndexNum; idx++)
		{
			cVector3f vVtx = GetVector3(apVertexArray,apIndexArray[idx],alVtxStride);
			if(bLog)Log("Checking idx: %d with vec: %s, ",idx,vVtx.ToString().c_str());

			tVtxIdxMapIt it = mapVtxIndices.find(vVtx);
			//If vertex already exist just add the index
			if(it != mapVtxIndices.end())
			{
				if(bLog)Log("Allready added, appending.!\n");
				it->second.mlstIndices.push_back(idx);
			}
			//if vertex is not added create new vertex and add.
			else
			{
				if(bLog)Log("Adding as new!\n");
				mapVtxIndices.insert(tVtxIdxMap::value_type(vVtx, cVertexIndices(idx)));
			}
		}
		if(bLog){
			Log("FINAL VTX MAP:\n");
			tVtxIdxMapIt VtxIt = mapVtxIndices.begin();
			for(; VtxIt != mapVtxIndices.end(); ++VtxIt)
			{
				const cVector3f &vVtx = VtxIt->first;
				const cVertexIndices &Data = VtxIt->second;
				Log("Vtx: %s Idx: ",vVtx.ToString().c_str());
				for(tUIntList::const_iterator it = Data.mlstIndices.begin(); it != Data.mlstIndices.end(); ++it)
				{
					Log("%d, ",*it);
				}
				Log("\n");
			}
		}


		////////////////////////////////////////////////
		//Iterate vertices and check what indices belong to it.
		//create a edges from these.
		//TODO: iterate the amp here instead.
		tVtxIdxMapIt VtxIt = mapVtxIndices.begin();
		for(; VtxIt != mapVtxIndices.end(); ++VtxIt)
		{
			const cVector3f vVtx = VtxIt->first;
			cVertexIndices &Data = VtxIt->second;

            //Iterate the indices and create edges.
			tUIntListIt it = Data.mlstIndices.begin();
			for(; it != Data.mlstIndices.end(); ++it)
			{
				int lTriIdx = ((*it)/3)*3;
				unsigned int lVtx = apIndexArray[*it]; //The num of the vertex this index reference to.

				//Create edges
				cTriEdge edge1,edge2;
				edge1.point1 = lVtx; edge2.point1 = lVtx;
				edge1.tri1 = lTriIdx/3;	edge2.tri1 = lTriIdx/3;

				//Get the index the vertex has in the tri (0 -2)
				int lIdxInTri = (*it) % 3;
				//Log("Idx in tri: %d\n",lIdxInTri);

				//Get the end points of the edge.
				int lPoint1 = lIdxInTri+1;
				if(lPoint1>2) lPoint1 =0;
				int lPoint2 = lIdxInTri-1;
				if(lPoint2<0) lPoint2 =2;

				//Set the end points.
				edge1.point2 = apIndexArray[lTriIdx + lPoint1];
				edge2.point2 = apIndexArray[lTriIdx + lPoint2];

				//Switch points if they 1 < 2
				CheckEdgeSwitch(&edge1);
				CheckEdgeSwitch(&edge2);

				AddEdgeToMap(edge1,mapTriEdgeLists);
				AddEdgeToMap(edge2,mapTriEdgeLists);
			}
		}
		if(bLog){
			Log("FINAL EDGE MAP:\n");
			tTriEdgeListMapIt EdgeIt = mapTriEdgeLists.begin();
			for(;EdgeIt != mapTriEdgeLists.end(); ++EdgeIt)
			{
				cTriEdge &Edge = const_cast<cTriEdge&>(*EdgeIt);

				Log("P1: %d P2: %d Tri1: %d Tri2: %d\n",Edge.point1,Edge.point2,Edge.tri1,Edge.tri2);
			}
		}

		////////////////////////////////////////////////
		//Iterate edge map and check triangles it has, create edges from this.
		avEdges.reserve(mapTriEdgeLists.size());
		tTriEdgeListMapIt EdgeIt = mapTriEdgeLists.begin();
		for(;EdgeIt != mapTriEdgeLists.end(); ++EdgeIt)
		{
			cTriEdge &Edge = const_cast<cTriEdge&>(*EdgeIt);
			const unsigned int *pTri1 = &apIndexArray[Edge.tri1 * 3];
			const unsigned int *pTri2 = NULL;
			if(Edge.tri2 >= 0) pTri2 = &apIndexArray[Edge.tri2 * 3];

			if(Edge.tri2 == -1){
				Edge.invert_tri2 = true;
				*apIsDoubleSided = true;
			}
			else{
				Edge.invert_tri2 = false;
			}

			//Get position of point1 in triangle
			int lPoint1InTri=0;
			for(int i=0; i < 3; i++){
				if(Vector3Equal(apVertexArray,pTri1[i],apVertexArray, Edge.point1,alVtxStride))
				{
					lPoint1InTri = i;
					break;
				}
			}
			//The next position in the triangle.
			int lNextInTri = lPoint1InTri +1;
			if(lNextInTri >=3 ) lNextInTri =0;

			//Log("Point in: %d Next: %d\n",lPoint1InTri,lNextInTri);

			//If next point is NOT point 2, then the edge
			//must be switched.
			if(Vector3Equal(apVertexArray,pTri1[ lNextInTri],apVertexArray,Edge.point2,alVtxStride))
			{
				unsigned int lTemp = Edge.point1;
				Edge.point1 = Edge.point2;
				Edge.point2 = lTemp;
				//if(bLog)Log("Switching points\n");
			}

			//Add the final edge.
			avEdges.push_back(Edge);
		}

		return true;
	}


	bool cMath::CheckRectIntersection(cRect2l aRect1, cRect2l aRect2)
	{
		if(aRect1.w<0)
		{
			aRect1.w*=-1;
			aRect1.x-=aRect1.w;
		}

		if(aRect1.h<0)
		{
			aRect1.h*=-1;
			aRect1.y-=aRect1.h;
		}

		if(aRect2.w<0)
		{
			aRect2.w*=-1;
			aRect2.x-=aRect2.w;
		}

		if(aRect2.h<0)
		{
			aRect2.h*=-1;
			aRect2.y-=aRect2.h;
		}

		return (aRect1.x>aRect2.x+(aRect2.w-1) || aRect2.x>aRect1.x+(aRect1.w-1) ||
			aRect1.y>aRect2.y+(aRect2.h-1) || aRect2.y>aRect1.y+(aRect1.h-1))==false;
	}

	//-----------------------------------------------------------------------

	bool cMath::CheckRectIntersection(cRect2f aRect1, cRect2f aRect2)
	{
		if(aRect1.w<0)
		{
			aRect1.w*=-1;
			aRect1.x-=aRect1.w;
		}

		if(aRect1.h<0)
		{
			aRect1.h*=-1;
			aRect1.y-=aRect1.h;
		}

		if(aRect2.w<0)
		{
			aRect2.w*=-1;
			aRect2.x-=aRect2.w;
		}

		if(aRect2.h<0)
		{
			aRect2.h*=-1;
			aRect2.y-=aRect2.h;
		}
		return (aRect1.x>aRect2.x+(aRect2.w) || aRect2.x>aRect1.x+(aRect1.w) ||
			aRect1.y>aRect2.y+(aRect2.h) || aRect2.y>aRect1.y+(aRect1.h))==false;
	}
	bool cMath::CheckSphereInPlanes(const cVector3f &avCenter, float afRadius, const cPlanef* apPlanes, int alPlaneCount)
	{
		for(int i=0; i<alPlaneCount; i++)
		{
			float fDist = cMath::PlaneToPointDist(apPlanes[i],avCenter);

			if(fDist < -afRadius){
				return false;
			}
		}

		return true;
	}
	bool cMath::CheckLineTriangleIntersection(	const cVector3f& avLineStart, const cVector3f& avLineEnd,
												const cVector3f& avP0, const cVector3f& avP1, const cVector3f& avP2,
												float *apT, bool abSkipBackfacing)
	{
		cVector3f vLineDelta = avLineEnd - avLineStart;

		cVector3f vEdge1 = avP1 - avP0;
		cVector3f vEdge2 = avP2 - avP0;

		if(abSkipBackfacing)
		{
			cVector3f vNormal = Vector3Cross(vEdge1, vEdge2);

			float fDDotN = Vector3Dot(vNormal, vLineDelta);
			if(fDDotN<0)
				return false;
		}


		cVector3f vDCrossE = Vector3Cross(vLineDelta, vEdge2);

        float fEDotN = Vector3Dot(vEdge1, vDCrossE);
		if(fabs(fEDotN) < 0.00001f) return false; //Line is parallel

		float fF = 1.0f / fEDotN;

		cVector3f vTriToStart = avLineStart - avP0;

		//Get U
		float fU = fF * Vector3Dot(vTriToStart, vDCrossE);
		if(fU < 0.0f || fU > 1.0f) return false;

		//Get V
		cVector3f vQ = Vector3Cross(vTriToStart,vEdge1);
		float fV = fF * Vector3Dot(vLineDelta, vQ);
		if (fV < 0.0 || fU + fV > 1.0) return false;

        //Get T
		float fT = fF * Vector3Dot(vEdge2, vQ);
        if(fT <0 || fT >1.0f) return false;

		if(apT) *apT = fT;
		return true;
	}
	float cMath::MatrixMinor(const cMatrixf &a_mtxA,
		const size_t r0, const size_t r1, const size_t r2,
		const size_t c0, const size_t c1, const size_t c2)
	{
		return a_mtxA.m[r0][c0] * (a_mtxA.m[r1][c1] * a_mtxA.m[r2][c2] - a_mtxA.m[r2][c1] * a_mtxA.m[r1][c2]) -
				a_mtxA.m[r0][c1] * (a_mtxA.m[r1][c0] * a_mtxA.m[r2][c2] - a_mtxA.m[r2][c0] * a_mtxA.m[r1][c2]) +
				a_mtxA.m[r0][c2] * (a_mtxA.m[r1][c0] * a_mtxA.m[r2][c1] - a_mtxA.m[r2][c0] * a_mtxA.m[r1][c1]);
	}

	//-----------------------------------------------------------------------

	cMatrixf cMath::MatrixAdjoint(const cMatrixf &a_mtxA)
	{
		return cMatrixf( MatrixMinor(a_mtxA, 1, 2, 3, 1, 2, 3),
						-MatrixMinor(a_mtxA, 0, 2, 3, 1, 2, 3),
						MatrixMinor(a_mtxA, 0, 1, 3, 1, 2, 3),
						-MatrixMinor(a_mtxA, 0, 1, 2, 1, 2, 3),

						-MatrixMinor(a_mtxA, 1, 2, 3, 0, 2, 3),
						MatrixMinor(a_mtxA, 0, 2, 3, 0, 2, 3),
						-MatrixMinor(a_mtxA, 0, 1, 3, 0, 2, 3),
						MatrixMinor(a_mtxA, 0, 1, 2, 0, 2, 3),

						MatrixMinor(a_mtxA, 1, 2, 3, 0, 1, 3),
						-MatrixMinor(a_mtxA, 0, 2, 3, 0, 1, 3),
						MatrixMinor(a_mtxA, 0, 1, 3, 0, 1, 3),
						-MatrixMinor(a_mtxA, 0, 1, 2, 0, 1, 3),

						-MatrixMinor(a_mtxA, 1, 2, 3, 0, 1, 2),
						MatrixMinor(a_mtxA, 0, 2, 3, 0, 1, 2),
						-MatrixMinor(a_mtxA, 0, 1, 3, 0, 1, 2),
						MatrixMinor(a_mtxA, 0, 1, 2, 0, 1, 2));
	}

	//-----------------------------------------------------------------------

	float cMath::MatrixDeterminant(const cMatrixf &a_mtxA)
	{
		return	a_mtxA.m[0][0] * MatrixMinor(a_mtxA, 1, 2, 3, 1, 2, 3) -
				a_mtxA.m[0][1] * MatrixMinor(a_mtxA, 1, 2, 3, 0, 2, 3) +
				a_mtxA.m[0][2] * MatrixMinor(a_mtxA, 1, 2, 3, 0, 1, 3) -
				a_mtxA.m[0][3] * MatrixMinor(a_mtxA, 1, 2, 3, 0, 1, 2);
	}

	//-----------------------------------------------------------------------

	cMatrixf cMath::MatrixInverse(const cMatrixf &a_mtxA)
	{
		return MatrixMulScalar(MatrixAdjoint(a_mtxA), (1.0f / MatrixDeterminant(a_mtxA)) );
	}

}
