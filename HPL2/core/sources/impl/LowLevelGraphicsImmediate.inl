// Immediate-mode draw helpers and CPU batch building shared by the GL and Vulkan backends.
// The includer defines IM_CLASS and the IM_* primitives.

	void IM_CLASS::DrawTriangle(tVertexVec& avVtx)
	{
		;

		assert(avVtx.size()==3);

		IM_BEGIN(IM_TRIANGLES);
		{
			for(int i=0;i<3;i++){
				IM_TEXCOORD3(avVtx[i].tex.x,avVtx[i].tex.y,avVtx[i].tex.z);
				IM_COLOR4(avVtx[i].col.r,avVtx[i].col.g,avVtx[i].col.b,avVtx[i].col.a);
				IM_VERTEX3(avVtx[i].pos.x,avVtx[i].pos.y,avVtx[i].pos.z);
			}
		}
		IM_END();
	}

	//-----------------------------------------------------------------------

	void IM_CLASS::DrawQuad(const cVector3f &avPos,const cVector2f &avSize,const cColor& aColor)
	{
		;

		IM_BEGIN(IM_QUADS);
		{
			IM_TEXCOORD2(0.0, 0.0);
			IM_COLOR4(aColor.r,aColor.g,aColor.b,aColor.a);
			IM_VERTEX3(avPos.x, avPos.y,avPos.z);

			IM_TEXCOORD2(1.0, 0.0);
			IM_COLOR4(aColor.r,aColor.g,aColor.b,aColor.a);
			IM_VERTEX3(avPos.x+avSize.x, avPos.y,avPos.z);

			IM_TEXCOORD2(1.0, 1.0);
			IM_COLOR4(aColor.r,aColor.g,aColor.b,aColor.a);
			IM_VERTEX3(avPos.x+avSize.x, avPos.y+avSize.y,avPos.z);

			IM_TEXCOORD2(0.0, 1.0);
			IM_COLOR4(aColor.r,aColor.g,aColor.b,aColor.a);
			IM_VERTEX3(avPos.x, avPos.y+avSize.y,avPos.z);

		}
		IM_END();
	}

	void IM_CLASS::DrawQuad(const cVector3f &avPos,const cVector2f &avSize,
		const cVector2f &avMinTexCoord,const cVector2f &avMaxTexCoord,
		const cColor& aColor)
	{
		;

		IM_BEGIN(IM_QUADS);
		{
			IM_TEXCOORD2(avMinTexCoord.x, avMinTexCoord.y);
			IM_COLOR4(aColor.r,aColor.g,aColor.b,aColor.a);
			IM_VERTEX3(avPos.x, avPos.y,avPos.z);

			IM_TEXCOORD2(avMaxTexCoord.x, avMinTexCoord.y);
			IM_COLOR4(aColor.r,aColor.g,aColor.b,aColor.a);
			IM_VERTEX3(avPos.x+avSize.x, avPos.y,avPos.z);

			IM_TEXCOORD2(avMaxTexCoord.x, avMaxTexCoord.y);
			IM_COLOR4(aColor.r,aColor.g,aColor.b,aColor.a);
			IM_VERTEX3(avPos.x+avSize.x, avPos.y+avSize.y,avPos.z);

			IM_TEXCOORD2(avMinTexCoord.x, avMaxTexCoord.y);
			IM_COLOR4(aColor.r,aColor.g,aColor.b,aColor.a);
			IM_VERTEX3(avPos.x, avPos.y+avSize.y,avPos.z);
		}
		IM_END();
	}

	void IM_CLASS::DrawQuad(const cVector3f &avPos,const cVector2f &avSize,
		const cVector2f &avMinTexCoord0,const cVector2f &avMaxTexCoord0,
		const cVector2f &avMinTexCoord1,const cVector2f &avMaxTexCoord1,
		const cColor& aColor)
	{
		;

		IM_BEGIN(IM_QUADS);
		{
			IM_MULTITEXCOORD2(IM_TEXTURE0,avMinTexCoord0.x, avMinTexCoord0.y);
			IM_MULTITEXCOORD2(IM_TEXTURE1,avMinTexCoord1.x, avMinTexCoord1.y);
			IM_COLOR4(aColor.r,aColor.g,aColor.b,aColor.a);
			IM_VERTEX3(avPos.x, avPos.y,avPos.z);

			IM_MULTITEXCOORD2(IM_TEXTURE0,avMaxTexCoord0.x, avMinTexCoord0.y);
			IM_MULTITEXCOORD2(IM_TEXTURE1,avMaxTexCoord1.x, avMinTexCoord1.y);
			IM_COLOR4(aColor.r,aColor.g,aColor.b,aColor.a);
			IM_VERTEX3(avPos.x+avSize.x, avPos.y,avPos.z);

			IM_MULTITEXCOORD2(IM_TEXTURE0,avMaxTexCoord0.x, avMaxTexCoord0.y);
			IM_MULTITEXCOORD2(IM_TEXTURE1,avMaxTexCoord1.x, avMaxTexCoord1.y);
			IM_COLOR4(aColor.r,aColor.g,aColor.b,aColor.a);
			IM_VERTEX3(avPos.x+avSize.x, avPos.y+avSize.y,avPos.z);

			IM_MULTITEXCOORD2(IM_TEXTURE0,avMinTexCoord0.x, avMaxTexCoord0.y);
			IM_MULTITEXCOORD2(IM_TEXTURE1,avMinTexCoord1.x, avMaxTexCoord1.y);
			IM_COLOR4(aColor.r,aColor.g,aColor.b,aColor.a);
			IM_VERTEX3(avPos.x, avPos.y+avSize.y,avPos.z);
		}
		IM_END();
	}

	//-----------------------------------------------------------------------

	void IM_CLASS::DrawQuad(const tVertexVec &avVtx)
	{
		;

		assert(avVtx.size()==4);

		IM_BEGIN(IM_QUADS);
		{
			for(int i=0;i<4;i++){
				IM_TEXCOORD3(avVtx[i].tex.x,avVtx[i].tex.y,avVtx[i].tex.z);
				IM_COLOR4(avVtx[i].col.r,avVtx[i].col.g,avVtx[i].col.b,avVtx[i].col.a);
				IM_VERTEX3(avVtx[i].pos.x,avVtx[i].pos.y,avVtx[i].pos.z);
			}
		}
		IM_END();
	}

	//-----------------------------------------------------------------------

	void IM_CLASS::DrawQuadMultiTex(const tVertexVec &avVtx,const tVector3fVec &avExtraUvs)
	{
		;

		int lExtraUnits = (int)avExtraUvs.size()/4;
		IM_BEGIN(IM_QUADS);
		{
			for(int i=0;i<4;i++)
			{
				IM_MULTITEXCOORD3(IM_TEXTURE0,avVtx[i].tex.x,avVtx[i].tex.y,avVtx[i].tex.z);

				for(int unit=0; unit<lExtraUnits; ++unit)
				{
					IM_MULTITEXCOORD3(IM_TEXTURE0 + unit + 1,
						avExtraUvs[unit*4 + i].x, avExtraUvs[unit*4 + i].y, avExtraUvs[unit*4 + i].z);
				}

				IM_COLOR4(avVtx[i].col.r,avVtx[i].col.g,avVtx[i].col.b,avVtx[i].col.a);
				IM_VERTEX3(avVtx[i].pos.x,avVtx[i].pos.y,avVtx[i].pos.z);
			}
		}
		IM_END();

	}


	//-----------------------------------------------------------------------

	void IM_CLASS::DrawQuad(const tVertexVec &avVtx, const cColor aCol)
	{
		;

		assert(avVtx.size()==4);

		IM_BEGIN(IM_QUADS);
		{
			//Make all this inline??
			for(int i=0;i<4;i++){
				IM_TEXCOORD3(avVtx[i].tex.x,avVtx[i].tex.y,avVtx[i].tex.z);
				IM_COLOR4(aCol.r,aCol.g,aCol.b,aCol.a);
				IM_VERTEX3(avVtx[i].pos.x,avVtx[i].pos.y,avVtx[i].pos.z);
			}
		}
		IM_END();
	}

	//-----------------------------------------------------------------------

	void IM_CLASS::DrawQuad(const tVertexVec &avVtx,const float afZ)
	{
		;

		assert(avVtx.size()==4);

		IM_BEGIN(IM_QUADS);
		{
			for(int i=0;i<4;i++){
				IM_TEXCOORD3(avVtx[i].tex.x,avVtx[i].tex.y,afZ);
				IM_COLOR4(avVtx[i].col.r,avVtx[i].col.g,avVtx[i].col.b,avVtx[i].col.a);
				IM_VERTEX3(avVtx[i].pos.x,avVtx[i].pos.y,avVtx[i].pos.z);
			}
		}
		IM_END();
	}

	//-----------------------------------------------------------------------

	void IM_CLASS::DrawQuad(const tVertexVec &avVtx,const float afZ,const cColor &aCol)
	{
		;

		assert(avVtx.size()==4);

		IM_BEGIN(IM_QUADS);
		{
			for(int i=0;i<4;i++){
				IM_TEXCOORD3(avVtx[i].tex.x,avVtx[i].tex.y,afZ);
				IM_COLOR4(aCol.r,aCol.g,aCol.b,aCol.a);
				IM_VERTEX3(avVtx[i].pos.x,avVtx[i].pos.y,avVtx[i].pos.z);
			}
		}
		IM_END();
	}

	//-----------------------------------------------------------------------

	void IM_CLASS::DrawLine(const cVector3f& avBegin, const cVector3f& avEnd, cColor aCol)
	{
		;

		IM_COLOR4(aCol.r,aCol.g,aCol.b,aCol.a);
		IM_BEGIN(IM_LINES);
		{
			IM_VERTEX3(avBegin.x,avBegin.y,avBegin.z);
			IM_VERTEX3(avEnd.x,avEnd.y,avEnd.z);
		}
		IM_END();
	}

	void IM_CLASS::DrawLine(const cVector3f& avBegin, const cColor& aBeginCol, const cVector3f& avEnd, const cColor& aEndCol)
	{
		;

		IM_BEGIN(IM_LINES);
		{
			IM_COLOR4(aBeginCol.r,aBeginCol.g,aBeginCol.b,aBeginCol.a);
			IM_VERTEX3(avBegin.x,avBegin.y,avBegin.z);

			IM_COLOR4(aEndCol.r,aEndCol.g,aEndCol.b,aEndCol.a);
			IM_VERTEX3(avEnd.x,avEnd.y,avEnd.z);
		}
		IM_END();
	}

	void IM_CLASS::DrawBoxMinMax(const cVector3f& avMin, const cVector3f& avMax, cColor aCol)
	{
		;

		IM_COLOR4(aCol.r,aCol.g,aCol.b,aCol.a);
		IM_BEGIN(IM_LINES);
		{
			//Pos Z Quad
			IM_VERTEX3(avMax.x,avMax.y,avMax.z);
			IM_VERTEX3(avMin.x,avMax.y,avMax.z);

			IM_VERTEX3(avMax.x,avMax.y,avMax.z);
			IM_VERTEX3(avMax.x,avMin.y,avMax.z);

			IM_VERTEX3(avMin.x,avMax.y,avMax.z);
			IM_VERTEX3(avMin.x,avMin.y,avMax.z);

			IM_VERTEX3(avMin.x,avMin.y,avMax.z);
			IM_VERTEX3(avMax.x,avMin.y,avMax.z);

			//Neg Z Quad
			IM_VERTEX3(avMax.x,avMax.y,avMin.z);
			IM_VERTEX3(avMin.x,avMax.y,avMin.z);

			IM_VERTEX3(avMax.x,avMax.y,avMin.z);
			IM_VERTEX3(avMax.x,avMin.y,avMin.z);

			IM_VERTEX3(avMin.x,avMax.y,avMin.z);
			IM_VERTEX3(avMin.x,avMin.y,avMin.z);

			IM_VERTEX3(avMin.x,avMin.y,avMin.z);
			IM_VERTEX3(avMax.x,avMin.y,avMin.z);

			//Lines between
			IM_VERTEX3(avMax.x,avMax.y,avMax.z);
			IM_VERTEX3(avMax.x,avMax.y,avMin.z);

			IM_VERTEX3(avMin.x,avMax.y,avMax.z);
			IM_VERTEX3(avMin.x,avMax.y,avMin.z);

			IM_VERTEX3(avMin.x,avMin.y,avMax.z);
			IM_VERTEX3(avMin.x,avMin.y,avMin.z);

			IM_VERTEX3(avMax.x,avMin.y,avMax.z);
			IM_VERTEX3(avMax.x,avMin.y,avMin.z);
		}
		IM_END();

	}

	//-----------------------------------------------------------------------

	void IM_CLASS::DrawSphere(const cVector3f& avPos, float afRadius, cColor aCol)
	{
		;

		int alSegments = 32;
		float afAngleStep = k2Pif /(float)alSegments;

		IM_COLOR4(aCol.r,aCol.g,aCol.b,aCol.a);
		IM_BEGIN(IM_LINES);
		{
			//X Circle:
			for(float a=0; a< k2Pif; a+= afAngleStep)
			{
				IM_VERTEX3(avPos.x, avPos.y + sin(a)*afRadius,
					avPos.z + cos(a)*afRadius);

				IM_VERTEX3(avPos.x, avPos.y + sin(a+afAngleStep)*afRadius,
					avPos.z + cos(a+afAngleStep)*afRadius);
			}

			//Y Circle:
			for(float a=0; a< k2Pif; a+= afAngleStep)
			{
				IM_VERTEX3(avPos.x + cos(a)*afRadius, avPos.y,
					avPos.z + sin(a)*afRadius);

				IM_VERTEX3(avPos.x + cos(a+afAngleStep)*afRadius, avPos.y ,
					avPos.z+ sin(a+afAngleStep)*afRadius);
			}

			//Z Circle:
			for(float a=0; a< k2Pif; a+= afAngleStep)
			{
				IM_VERTEX3(avPos.x + cos(a)*afRadius, avPos.y + sin(a)*afRadius, avPos.z);

				IM_VERTEX3(avPos.x + cos(a+afAngleStep)*afRadius,
					avPos.y + sin(a+afAngleStep)*afRadius,
					avPos.z);
			}

		}
		IM_END();
	}

	//-----------------------------------------------------------------------

	void IM_CLASS::DrawSphere(const cVector3f& avPos, float afRadius, cColor aColX, cColor aColY, cColor aColZ)
	{
		;

		int alSegments = 32;
		float afAngleStep = k2Pif /(float)alSegments;

		SetTexture(0,NULL);
		SetBlendActive(false);
		IM_BEGIN(IM_LINES);
		{
			//X Circle:
			IM_COLOR4(aColX.r,aColX.g,aColX.b,aColX.a);
			for(float a=0; a< k2Pif; a+= afAngleStep)
			{
				IM_VERTEX3(avPos.x, avPos.y + sin(a)*afRadius,
					avPos.z + cos(a)*afRadius);

				IM_VERTEX3(avPos.x, avPos.y + sin(a+afAngleStep)*afRadius,
					avPos.z + cos(a+afAngleStep)*afRadius);
			}

			//Y Circle:
			IM_COLOR4(aColY.r,aColY.g,aColY.b,aColY.a);
			for(float a=0; a< k2Pif; a+= afAngleStep)
			{
				IM_VERTEX3(avPos.x + cos(a)*afRadius, avPos.y,
					avPos.z + sin(a)*afRadius);

				IM_VERTEX3(avPos.x + cos(a+afAngleStep)*afRadius, avPos.y ,
					avPos.z+ sin(a+afAngleStep)*afRadius);
			}

			//Z Circle:
			IM_COLOR4(aColZ.r,aColZ.g,aColZ.b,aColZ.a);
			for(float a=0; a< k2Pif; a+= afAngleStep)
			{
				IM_VERTEX3(avPos.x + cos(a)*afRadius, avPos.y + sin(a)*afRadius, avPos.z);

				IM_VERTEX3(avPos.x + cos(a+afAngleStep)*afRadius,
					avPos.y + sin(a+afAngleStep)*afRadius,
					avPos.z);
			}

		}
		IM_END();
	}

	//-----------------------------------------------------------------------

	void IM_CLASS::DrawLineQuad(const cRect2f& aRect, float afZ, cColor aCol)
	{
		;

		IM_COLOR4(aCol.r,aCol.g,aCol.b,aCol.a);
		IM_BEGIN(IM_LINE_STRIP);
		{
			IM_VERTEX3(aRect.x,aRect.y,afZ);
			IM_VERTEX3(aRect.x+aRect.w,aRect.y,afZ);
			IM_VERTEX3(aRect.x+aRect.w,aRect.y+aRect.h,afZ);
			IM_VERTEX3(aRect.x,aRect.y+aRect.h,afZ);
			IM_VERTEX3(aRect.x,aRect.y,afZ);
		}
		IM_END();
	}

	void IM_CLASS::DrawLineQuad(const cVector3f &avPos,const cVector2f &avSize, cColor aCol)
	{
		;

		IM_COLOR4(aCol.r,aCol.g,aCol.b,aCol.a);
		IM_BEGIN(IM_LINE_STRIP);
		{
			IM_VERTEX3(avPos.x,avPos.y,avPos.z);
			IM_VERTEX3(avPos.x+avSize.x,avPos.y,avPos.z);
			IM_VERTEX3(avPos.x+avSize.x,avPos.y+avSize.y,avPos.z);
			IM_VERTEX3(avPos.x,avPos.y+avSize.y,avPos.z);
			IM_VERTEX3(avPos.x,avPos.y,avPos.z);
		}
		IM_END();
	}



	//-----------------------------------------------------------------------

	//////////////////////////////////////////////////////////////////////////
	// VERTEX BATCHING
	//////////////////////////////////////////////////////////////////////////

	//-----------------------------------------------------------------------

	void IM_CLASS::AddVertexToBatch(const cVertex *apVtx)
	{
		;

		//Coord
		mpVertexArray[mlVertexCount + 0] =	apVtx->pos.x;
		mpVertexArray[mlVertexCount + 1] =	apVtx->pos.y;
		mpVertexArray[mlVertexCount + 2] =	apVtx->pos.z;
		//Color
		mpVertexArray[mlVertexCount + 3] =	apVtx->col.r;
		mpVertexArray[mlVertexCount + 4] =	apVtx->col.g;
		mpVertexArray[mlVertexCount + 5] =	apVtx->col.b;
		mpVertexArray[mlVertexCount + 6] =	apVtx->col.a;
		//Texture coord
		mpVertexArray[mlVertexCount + 7] =	apVtx->tex.x;
		mpVertexArray[mlVertexCount + 8] =	apVtx->tex.y;
		mpVertexArray[mlVertexCount + 9] =	apVtx->tex.z;
		//Normal coord
		mpVertexArray[mlVertexCount + 10] =	apVtx->norm.x;
		mpVertexArray[mlVertexCount + 11] =	apVtx->norm.y;
		mpVertexArray[mlVertexCount + 12] =	apVtx->norm.z;

		mlVertexCount = mlVertexCount + mlBatchStride;

		if(mlVertexCount/mlBatchStride >= mlBatchArraySize)
		{
			//Make the array larger.
		}
	}

	//-----------------------------------------------------------------------

	void IM_CLASS::AddVertexToBatch(const cVertex *apVtx, const cVector3f* avTransform)
	{
		;

		//Coord
		mpVertexArray[mlVertexCount + 0] =	apVtx->pos.x+avTransform->x;
		mpVertexArray[mlVertexCount + 1] =	apVtx->pos.y+avTransform->y;
		mpVertexArray[mlVertexCount + 2] =	apVtx->pos.z+avTransform->z;

		/*Log("Trans: %s\n",avTransform->ToString().c_str());
		Log("Adding: %f:%f:%f\n",mpVertexArray[mlVertexCount + 0],
		mpVertexArray[mlVertexCount + 1],
		mpVertexArray[mlVertexCount + 2]);*/
		//Color
		mpVertexArray[mlVertexCount + 3] =	apVtx->col.r;
		mpVertexArray[mlVertexCount + 4] =	apVtx->col.g;
		mpVertexArray[mlVertexCount + 5] =	apVtx->col.b;
		mpVertexArray[mlVertexCount + 6] =	apVtx->col.a;
		//Texture coord
		mpVertexArray[mlVertexCount + 7] =	apVtx->tex.x;
		mpVertexArray[mlVertexCount + 8] =	apVtx->tex.y;
		mpVertexArray[mlVertexCount + 9] =	apVtx->tex.z;

		/*Log("Tex: %f:%f:%f\n",mpVertexArray[mlVertexCount + 7],
		mpVertexArray[mlVertexCount + 8],
		mpVertexArray[mlVertexCount + 9]);*/

		//Normal coord
		mpVertexArray[mlVertexCount + 10] =	apVtx->norm.x;
		mpVertexArray[mlVertexCount + 11] =	apVtx->norm.y;
		mpVertexArray[mlVertexCount + 12] =	apVtx->norm.z;

		mlVertexCount = mlVertexCount + mlBatchStride;

		if(mlVertexCount/mlBatchStride >= mlBatchArraySize)
		{
			//Make the array larger.
		}
	}

	//-----------------------------------------------------------------------

	void IM_CLASS::AddVertexToBatch(const cVertex *apVtx, const cMatrixf* aMtx)
	{
		;

	}

	//-----------------------------------------------------------------------

	void IM_CLASS::AddVertexToBatch_Size2D(const cVertex *apVtx, const cVector3f* avTransform,
		const cColor* apCol,const float& mfW, const float& mfH)
	{
		;

		//Coord
		mpVertexArray[mlVertexCount + 0] =	avTransform->x + mfW;
		mpVertexArray[mlVertexCount + 1] =	avTransform->y + mfH;
		mpVertexArray[mlVertexCount + 2] =	avTransform->z;

		//Color
		mpVertexArray[mlVertexCount + 3] =	apCol->r;
		mpVertexArray[mlVertexCount + 4] =	apCol->g;
		mpVertexArray[mlVertexCount + 5] =	apCol->b;
		mpVertexArray[mlVertexCount + 6] =	apCol->a;

		//Texture coord
		mpVertexArray[mlVertexCount + 7] =	apVtx->tex.x;
		mpVertexArray[mlVertexCount + 8] =	apVtx->tex.y;
		mpVertexArray[mlVertexCount + 9] =	apVtx->tex.z;


		mlVertexCount = mlVertexCount + mlBatchStride;

		if(mlVertexCount/mlBatchStride >= mlBatchArraySize)
		{
			//Make the array larger.
		}
	}

	//-----------------------------------------------------------------------

	void IM_CLASS::AddVertexToBatch_Raw(	const cVector3f& avPos, const cColor &aColor,
		const cVector3f& avTex)
	{
		;

		//Coord
		mpVertexArray[mlVertexCount + 0] =	avPos.x;
		mpVertexArray[mlVertexCount + 1] =	avPos.y;
		mpVertexArray[mlVertexCount + 2] =	avPos.z;

		//Color
		mpVertexArray[mlVertexCount + 3] =	aColor.r;
		mpVertexArray[mlVertexCount + 4] =	aColor.g;
		mpVertexArray[mlVertexCount + 5] =	aColor.b;
		mpVertexArray[mlVertexCount + 6] =	aColor.a;

		//Texture coord
		mpVertexArray[mlVertexCount + 7] =	avTex.x;
		mpVertexArray[mlVertexCount + 8] =	avTex.y;
		mpVertexArray[mlVertexCount + 9] =	avTex.z;


		mlVertexCount = mlVertexCount + mlBatchStride;
	}


	//-----------------------------------------------------------------------

	void IM_CLASS::AddIndexToBatch(int alIndex)
	{
		;

		mpIndexArray[mlIndexCount] = alIndex;
		mlIndexCount++;

		if(mlIndexCount>=mlBatchArraySize)
		{
			//Make the array larger.
		}
	}

	//-----------------------------------------------------------------------

	void IM_CLASS::AddTexCoordToBatch(unsigned int alUnit,const cVector3f *apCoord)
	{
		;

		unsigned int lCount = mlTexCoordArrayCount[alUnit];

		mpTexCoordArray[alUnit][lCount+0] = apCoord->x;
		mpTexCoordArray[alUnit][lCount+1] = apCoord->y;
		mpTexCoordArray[alUnit][lCount+2] = apCoord->z;

		mlTexCoordArrayCount[alUnit]+=3;
	}
