#include "resources/HplMapParse.h"

#include "resources/BinaryBuffer.h"
#include "resources/XmlDocument.h"
#include "resources/ResourcesTypes.h"
#include "system/String.h"
#include "math/Math.h"

#include <cstdarg>
#include <cstdio>
#include <vector>

namespace hpl {

	#define MAP_CACHE_MAGIC		0xF441451F
	#define MAP_CACHE_VERSION	13

	enum { kElementLast = 14, kFormatLast = 3 };


	static OhplStr Str(const tString& asStr)
	{
		OhplStr s = { asStr.data(), asStr.size() };
		return s;
	}

	static OhplF3 F3(const cVector3f& avV)
	{
		OhplF3 v = { avV.x, avV.y, avV.z };
		return v;
	}

	static OhplF4 F4(const cColor& aCol)
	{
		OhplF4 c = { aCol.r, aCol.g, aCol.b, aCol.a };
		return c;
	}

	static OhplMat4 Mat4(const cMatrixf& a_mtx)
	{
		OhplMat4 m;
		for(int i=0; i<16; ++i) m.v[i] = a_mtx.v[i];
		return m;
	}

	static void Emit(void (*apFn)(void*, OhplStr), void *apCtx, const char *asFmt, ...)
	{
		char text[4096];
		va_list ap;
		va_start(ap, asFmt);
		vsnprintf(text, sizeof(text), asFmt, ap);
		va_end(ap);
		tString sText = text;
		apFn(apCtx, Str(sText));
	}

	#define EMIT_LOG(B, ...)		Emit((B)->log, (B)->ctx, __VA_ARGS__)
	#define EMIT_WARNING(B, ...)	Emit((B)->warning, (B)->ctx, __VA_ARGS__)
	#define EMIT_ERROR(B, ...)		Emit((B)->error, (B)->ctx, __VA_ARGS__)

	//-----------------------------------------------------------------------

	static float* gpShortTable;
	static float* gpByteNegPosTable;
	static float* gpBytePosTable;

	static void InitTables()
	{
		if(gpShortTable) return;

		gpShortTable = new float[0xFFFF + 2];
		gpByteNegPosTable = new float[257];
		gpBytePosTable = new float[257];

		for(int i=0; i<=0xFFFF; ++i)
		{
			short lX = *((short*)&i);
			float fX = ((float)lX);
			gpShortTable[i] = fX / 32767.0f;
		}
		for(int i=0; i<=255; ++i)
		{
			char lX = *((char*)&i);
			float fX = ((float)lX);
			gpByteNegPosTable[i] = fX / 127.0f;
		}
		for(int i=0; i<=255; ++i)
		{
			float fX = (float)i;
			gpBytePosTable[i] = fX / 255.0f;
		}
	}

	//-----------------------------------------------------------------------

	static size_t FormatSize(int alFormat)
	{
		return alFormat == 2 ? 1 : 4;
	}

	bool HplMapCacheRead(cBinaryBuffer& aBuff, const OhplMapBuilder *apB, const OhplMapOptions *apOpt)
	{
		InitTables();
		const tString sFile(apOpt->file.p, apOpt->file.n);

		int lMagicNum = aBuff.GetInt32();
		int lVersion = aBuff.GetInt32();

		if(lMagicNum != (int)MAP_CACHE_MAGIC)
		{
			EMIT_ERROR(apB, "File '%s' does not have right MAP_CACHE magic number (%X instead of %X)! Invalid header!\n", sFile.c_str(), lMagicNum, (unsigned)MAP_CACHE_MAGIC);
			return false;
		}
		if(lVersion != MAP_CACHE_VERSION)
		{
			EMIT_ERROR(apB, "File '%s' does not have right MAP_CACHE version! Is %d, newest is %d\n", sFile.c_str(), lVersion, MAP_CACHE_VERSION);
			return false;
		}
		{
			int lExpectedTotalSize = aBuff.GetInt32();
			if((size_t)lExpectedTotalSize != aBuff.GetSize())
			{
				EMIT_ERROR(apB, "File '%s' is truncated or corrupt (expected %d bytes, file is %d)! Rebuilding from source instead.\n",
					sFile.c_str(), lExpectedTotalSize, (int)aBuff.GetSize());
				return false;
			}
		}

		int lStaticMeshBodyNum = aBuff.GetInt32();
		int lStaticShapeBodyNum = aBuff.GetInt32();
		int lStaticMeshEntities = aBuff.GetInt32();
		size_t lLimit = aBuff.GetSize();
		if(lStaticMeshBodyNum < 0 || (size_t)lStaticMeshBodyNum > lLimit ||
			lStaticShapeBodyNum < 0 || (size_t)lStaticShapeBodyNum > lLimit ||
			lStaticMeshEntities < 0 || (size_t)lStaticMeshEntities > lLimit)
		{
			EMIT_ERROR(apB, "Map cache of '%s' has invalid counts (%d, %d, %d)! Rebuilding from source instead.\n",
				sFile.c_str(), lStaticMeshBodyNum, lStaticShapeBodyNum, lStaticMeshEntities);
			return false;
		}
		apB->cache_header(apB->ctx, lStaticMeshBodyNum, lStaticShapeBodyNum, lStaticMeshEntities);

		for(int i=0; i<lStaticMeshBodyNum; ++i)
		{
			tString sName, sMaterial;
			aBuff.GetString(&sName);
			aBuff.GetString(&sMaterial);
			bool bBlocksLight = aBuff.GetBool();
			bool bColliderCharacer = aBuff.GetBool();

			apB->cache_mesh_body(apB->ctx, Str(sName), Str(sMaterial), bBlocksLight, bColliderCharacer);
		}

		for(int i=0; i<lStaticShapeBodyNum; ++i)
		{
			tString sMaterial;
			cMatrixf mtxTransform;

			aBuff.GetString(&sMaterial);
			aBuff.GetMatrixf(&mtxTransform);
			bool bCharCollider = aBuff.GetBool();
			bool bBlocksLight = aBuff.GetBool();

			int lColliderNum = aBuff.GetInt32();
			if(lColliderNum < 0 || (size_t)lColliderNum > lLimit)
			{
				EMIT_ERROR(apB, "Shape body with %d colliders is out of range in map cache!\n", lColliderNum);
				return true;
			}
			std::vector<OhplCacheShape> vShapes(lColliderNum);

			for(size_t j=0; j<vShapes.size(); ++j)
			{
				cVector3f vSize;
				cMatrixf mtxOffset;

				vShapes[j].type = aBuff.GetInt32();
				aBuff.GetVector3f(&vSize);
				aBuff.GetMatrixf(&mtxOffset);
				vShapes[j].size = F3(vSize);
				vShapes[j].offset = Mat4(mtxOffset);
			}

			OhplMat4 mtx = Mat4(mtxTransform);
			apB->cache_shape_body(apB->ctx, Str(sMaterial), &mtx, bCharCollider, bBlocksLight, vShapes.data(), vShapes.size());
		}

		for(int mesh=0; mesh<lStaticMeshEntities; ++mesh)
		{
			tString sName, sMaterial;
			aBuff.GetString(&sName);
			aBuff.GetString(&sMaterial);
			bool bCastShadows = aBuff.GetBool();

			if(apOpt->logCache) EMIT_LOG(apB, "Mesh %d: '%s' '%s'\n", mesh, sName.c_str(), sMaterial.c_str());

			std::vector<OhplCacheArray> vArrays;
			std::vector< std::vector<char> > vData;
			int lVtxNum = aBuff.GetInt32();
			{
				int lVtxTypeNum = aBuff.GetInt32();

				if(apOpt->logCache) EMIT_LOG(apB, " VertexBuffers num: %d typenum: %d\n",lVtxNum, lVtxTypeNum);

				if(lVtxTypeNum < 0 || (size_t)lVtxTypeNum > (aBuff.GetSize() - aBuff.GetPos()) / 20)
				{
					EMIT_ERROR(apB, "Vertex type count %d is out of range in map cache!\n", lVtxTypeNum);
					return true;
				}

				for(int i=0; i< lVtxTypeNum; ++i)
				{
					OhplCacheArray arr;
					arr.type = aBuff.GetShort16();
					arr.format = aBuff.GetShort16();
					arr.progVar = aBuff.GetInt32();
					arr.elemNum = aBuff.GetInt32();
					arr.compression = aBuff.GetInt32();

					if(apOpt->logCache) EMIT_LOG(apB, "   Vtx %d: %d %d %d\n", i, arr.type, arr.progVar, arr.elemNum);

					long long llCount = (long long)lVtxNum * arr.elemNum;
					if(llCount < 0 || llCount > (long long)(aBuff.GetSize() - aBuff.GetPos()))
					{
						EMIT_ERROR(apB, "Vertex array of %d x %d elements is out of range in map cache!\n", lVtxNum, arr.elemNum);
						return true;
					}
					arr.count = (size_t)llCount;

					if(arr.type < 0 || arr.type >= kElementLast || arr.format < 0 || arr.format >= kFormatLast)
					{
						EMIT_ERROR(apB, "Vertex buffer has incorrect format when getting vertexbuffer data during loading of MSH file!\n");
						continue;
					}

					vData.push_back(std::vector<char>());
					std::vector<char>& vBytes = vData.back();

					if(arr.compression ==0)
					{
						vBytes.resize(arr.count * FormatSize(arr.format));
						if(arr.format == 2)	aBuff.GetCharArray(vBytes.data(), arr.count);
						else				aBuff.GetInt32Array((int*)vBytes.data(), arr.count);
					}
					else
					{
						vBytes.resize(arr.count * sizeof(float));
						float *pDestData = (float*)vBytes.data();
						int lElemCount = (int)arr.count;

						if(arr.compression <= 2)
						{
							if(arr.compression == 1)
							{
								while(lElemCount > 0)
								{
									*pDestData = gpBytePosTable[aBuff.GetUnsignedChar()];
									++pDestData; --lElemCount;
								}
							}
							else
							{
								while(lElemCount > 0)
								{
									*pDestData = gpByteNegPosTable[aBuff.GetUnsignedChar()];
									++pDestData; --lElemCount;
								}
							}
						}
						else
						{
							while(lElemCount > 0)
							{
								*pDestData = gpShortTable[aBuff.GetUnsignedShort16()];
								++pDestData; --lElemCount;
							}
						}
					}

					arr.data = vBytes.data();
					vArrays.push_back(arr);
				}
			}

			int lIdxNum = aBuff.GetInt32();
			if(apOpt->logCache) EMIT_LOG(apB, "Indices: %d\n", lIdxNum);
			if(lIdxNum < 0 || (size_t)lIdxNum > (aBuff.GetSize() - aBuff.GetPos()) / 4)
			{
				EMIT_ERROR(apB, "Index count %d is out of range in map cache!\n", lIdxNum);
				return true;
			}
			std::vector<unsigned> vIndices(lIdxNum);
			aBuff.GetInt32Array((int*)vIndices.data(), lIdxNum);

			apB->cache_mesh(apB->ctx, Str(sName), Str(sMaterial), bCastShadows, lVtxNum,
							vArrays.data(), vArrays.size(), vIndices.data(), vIndices.size());
		}

		return true;
	}

	//-----------------------------------------------------------------------

	void HplMapCacheWrite(cBinaryBuffer& aBuff, const OhplMapCacheSource *apSrc)
	{
		aBuff.AddInt32((int)MAP_CACHE_MAGIC);
		aBuff.AddInt32(MAP_CACHE_VERSION);

		size_t lTotalSizePos = aBuff.GetPos();
		aBuff.AddInt32(0);

		int lBodies, lShapes, lMeshes;
		apSrc->counts(apSrc->ctx, &lBodies, &lShapes, &lMeshes);
		aBuff.AddInt32(lBodies);
		aBuff.AddInt32(lShapes);
		aBuff.AddInt32(lMeshes);

		for(int i=0; i<lBodies; ++i)
		{
			OhplCacheBodyOut body;
			apSrc->next_body(apSrc->ctx, &body);

			aBuff.AddString(tString(body.name.p, body.name.n));
			aBuff.AddString(tString(body.material.p, body.material.n));
			aBuff.AddBool(body.blocksLight);
			aBuff.AddBool(body.charCollider);

			apSrc->write_body_shape(apSrc->ctx);
		}

		for(int i=0; i<lShapes; ++i)
		{
			OhplCacheShapeBodyOut sb;
			apSrc->next_shape_body(apSrc->ctx, &sb);

			aBuff.AddString(tString(sb.material.p, sb.material.n));
			cMatrixf mtx;
			for(int j=0; j<16; ++j) mtx.v[j] = sb.transform.v[j];
			aBuff.AddMatrixf(mtx);
			aBuff.AddBool(sb.charCollider);
			aBuff.AddBool(sb.blocksLight);

			aBuff.AddInt32((int)sb.nColliders);
			for(size_t j=0; j<sb.nColliders; ++j)
			{
				const OhplCacheShape& s = sb.colliders[j];
				cMatrixf mtxOffset;
				for(int k=0; k<16; ++k) mtxOffset.v[k] = s.offset.v[k];

				aBuff.AddInt32(s.type);
				aBuff.AddVector3f(cVector3f(s.size.x, s.size.y, s.size.z));
				aBuff.AddMatrixf(mtxOffset);
			}
		}

		for(int i=0; i<lMeshes; ++i)
		{
			OhplCacheMeshOut mesh;
			apSrc->next_mesh(apSrc->ctx, &mesh);

			aBuff.AddString(tString(mesh.name.p, mesh.name.n));
			aBuff.AddString(tString(mesh.material.p, mesh.material.n));
			aBuff.AddBool(mesh.castShadows);

			aBuff.AddInt32(mesh.vertexNum);
			aBuff.AddInt32((int)mesh.nArrays);

			for(size_t a=0; a<mesh.nArrays; ++a)
			{
				const OhplCacheArray& arr = mesh.arrays[a];
				int lVtxNum = mesh.vertexNum;
				int lElementNum = arr.elemNum;

				aBuff.AddShort16(arr.type);
				aBuff.AddShort16(arr.format);
				aBuff.AddInt32(arr.progVar);
				aBuff.AddInt32(lElementNum);

				int lCompressionType =0;
				if(arr.type == 2)
					lCompressionType = 1;
				if(arr.type == 0 || arr.type == 4)
					lCompressionType = 2;

				aBuff.AddInt32(lCompressionType);

				if(lCompressionType ==0)
				{
					if(arr.format == 0 || arr.format == 1)	aBuff.AddInt32Array((const int*)arr.data, (size_t)(lVtxNum * lElementNum));
					else if(arr.format == 2)				aBuff.AddCharArray((const char*)arr.data, (size_t)(lVtxNum * lElementNum));
					else
					{
						EMIT_ERROR(apSrc, "Vertex buffer has incorrect format when getting vertexbuffer data during loading of MSH file!\n");
						EMIT_ERROR(apSrc, "Vertex buffer has incorrect format when adding binary data during loading of MSH file!\n");
					}
				}
				else
				{
					const float* pData = (const float*)arr.data;
					int lElementCount = lElementNum * lVtxNum;

					while(lElementCount>0)
					{
						if(lCompressionType == 1)
						{
							unsigned char lChar = (unsigned char)cMath::FastPositiveFloatToInt(*pData * 255.0f);
							aBuff.AddUnsignedChar(lChar);
						}
						else
						{
							char lChar = (char)cMath::FastPosAndNegFloatToInt(*pData * 127.0f);
							aBuff.AddChar(lChar);
						}

						++pData;
						--lElementCount;
					}
				}
			}

			aBuff.AddInt32((int)mesh.nIndices);
			aBuff.AddInt32Array((const int*)mesh.indices, mesh.nIndices);
		}

		aBuff.SetInt32((int)aBuff.GetPos(), lTotalSizePos);
	}

	//-----------------------------------------------------------------------

	namespace {

	class cMapParser
	{
	public:
		cMapParser(const OhplMapBuilder *apB, unsigned alFlags) : mpB(apB), mlFlags(alFlags), mbAbort(false) {}

		int Parse(iXmlNode *apDoc, bool abCacheLoaded);

	private:
		void LoadFileIndexTable(cXmlElement *apContents, const char *asName, tStringVec &avTable);
		void LoadStaticObjects(cXmlElement *apContents);
		void StaticObject(cXmlElement *apElement);
		void Primitive(cXmlElement *apElement);
		void Decal(cXmlElement *apElement);
		void Combo(cXmlElement *apElement);
		void LoadEntities(cXmlElement *apContents);
		void LoadedEntity(cXmlElement *apElement);
		bool TransformValid(const tString &asName, const cVector3f &avPos);

		static std::vector<cXmlElement*> Children(cXmlElement *apElement)
		{
			std::vector<cXmlElement*> vNodes;
			cXmlNodeListIterator it = apElement->GetChildIterator();
			while(it.HasNext()) vNodes.push_back(it.Next()->ToElement());
			return vNodes;
		}

		const OhplMapBuilder *mpB;
		unsigned mlFlags;
		bool mbAbort;

		tStringVec mvFileIndices_StaticObjects;
		tStringVec mvFileIndices_Entities;
		tStringVec mvFileIndices_Decals;
	};

	int cMapParser::Parse(iXmlNode *apDoc, bool abCacheLoaded)
	{
		cXmlElement *pXmlMapData = apDoc->GetFirstElement("MapData");
		if(pXmlMapData==NULL) return OHPL_MAP_ABORT;

		mpB->set_fog(mpB->ctx, pXmlMapData->GetAttributeBool("FogActive", false),
						F4(pXmlMapData->GetAttributeColor("FogColor", cColor(1,1))),
						pXmlMapData->GetAttributeFloat("FogFalloffExp", 1.0f),
						pXmlMapData->GetAttributeFloat("FogStart", 0.0f),
						pXmlMapData->GetAttributeFloat("FogEnd", 0.0f),
						pXmlMapData->GetAttributeBool("FogCulling", true));

		{
			bool bActive = pXmlMapData->GetAttributeBool("SkyBoxActive", false);
			cColor color = pXmlMapData->GetAttributeColor("SkyBoxColor", cColor(1,1));
			tString sSkyBoxTexture = pXmlMapData->GetAttributeString("SkyBoxTexture","");
			mpB->set_skybox(mpB->ctx, bActive, F4(color), Str(sSkyBoxTexture));
		}

		cXmlElement *pXmlContents = pXmlMapData->GetFirstElement("MapContents");
		if(pXmlContents==NULL) return OHPL_MAP_NO_CONTENTS;

		LoadFileIndexTable(pXmlContents, "FileIndex_Decals", mvFileIndices_Decals);
		LoadFileIndexTable(pXmlContents, "FileIndex_Entities", mvFileIndices_Entities);
		LoadFileIndexTable(pXmlContents, "FileIndex_StaticObjects", mvFileIndices_StaticObjects);
		if(mbAbort) return OHPL_MAP_ABORT;

		if(abCacheLoaded==false)
		{
			mpB->phase(mpB->ctx, OHPL_MAP_PHASE_STATIC, true);
			LoadStaticObjects(pXmlContents);
			if(mbAbort) return OHPL_MAP_ABORT;
			mpB->phase(mpB->ctx, OHPL_MAP_PHASE_STATIC, false);
		}

		if((mlFlags & eWorldLoadFlag_NoEntities)==0)
		{
			mpB->phase(mpB->ctx, OHPL_MAP_PHASE_ENTITIES, true);
			LoadEntities(pXmlContents);
			if(mbAbort) return OHPL_MAP_ABORT;
			mpB->phase(mpB->ctx, OHPL_MAP_PHASE_ENTITIES, false);
		}

		return OHPL_MAP_OK;
	}

	void cMapParser::LoadFileIndexTable(cXmlElement *apContents, const char *asName, tStringVec &avTable)
	{
		cXmlElement *pXmlTable = apContents->GetFirstElement(asName);
		if(pXmlTable==NULL) return;

		int lNum = pXmlTable->GetAttributeInt("NumOfFiles",0);
		if(lNum < 0 || lNum > (1 << 20)) { mbAbort = true; return; }
		avTable.resize(lNum);

		std::vector<cXmlElement*> vChildren = Children(pXmlTable);
		for(size_t i=0; i<vChildren.size(); ++i)
		{
			int lIdx = vChildren[i]->GetAttributeInt("Id", 0);
			tString sPath = vChildren[i]->GetAttributeString("Path", "");
			if(lIdx >= 0 && lIdx < (int)avTable.size()) avTable[lIdx] = sPath;
		}
	}

	bool cMapParser::TransformValid(const tString &asName, const cVector3f &avPos)
	{
		if(cMath::Abs(avPos.x)>10000.0f || cMath::Abs(avPos.y)>10000.0f || cMath::Abs(avPos.z)>10000.0f)
		{
			EMIT_WARNING(mpB, "The object %s has an invalid position: (%s)!\n", asName.c_str(), avPos.ToString().c_str());
			return false;
		}
		return true;
	}

	void cMapParser::LoadStaticObjects(cXmlElement *apContents)
	{
		mpB->static_begin(mpB->ctx);

		cXmlElement *pXmlStaticObjects = apContents->GetFirstElement("StaticObjects");
		if(pXmlStaticObjects==NULL) { mbAbort = true; return; }

		mpB->phase(mpB->ctx, OHPL_MAP_PHASE_STATIC_OBJECTS, true);
		{
			std::vector<cXmlElement*> v = Children(pXmlStaticObjects);
			for(size_t i=0; i<v.size(); ++i) StaticObject(v[i]);
		}
		mpB->phase(mpB->ctx, OHPL_MAP_PHASE_STATIC_OBJECTS, false);

		cXmlElement *pXmlPrimitives = apContents->GetFirstElement("Primitives");
		if(pXmlPrimitives)
		{
			mpB->phase(mpB->ctx, OHPL_MAP_PHASE_PRIMITIVES, true);
			std::vector<cXmlElement*> v = Children(pXmlPrimitives);
			for(size_t i=0; i<v.size(); ++i) Primitive(v[i]);
			mpB->phase(mpB->ctx, OHPL_MAP_PHASE_PRIMITIVES, false);
		}

		cXmlElement *pXmlDecals = apContents->GetFirstElement("Decals");
		if(pXmlDecals && !(mlFlags & eWorldLoadFlag_FastStaticLoad))
		{
			mpB->phase(mpB->ctx, OHPL_MAP_PHASE_DECALS, true);
			std::vector<cXmlElement*> v = Children(pXmlDecals);
			for(size_t i=0; i<v.size(); ++i) Decal(v[i]);
			mpB->phase(mpB->ctx, OHPL_MAP_PHASE_DECALS, false);
		}

		mpB->static_loading_done(mpB->ctx);

		cXmlElement *pXmlObjectCombos = apContents->GetFirstElement("StaticObjectCombos");
		if(pXmlObjectCombos)
		{
			mpB->phase(mpB->ctx, OHPL_MAP_PHASE_COMBOS, true);
			std::vector<cXmlElement*> v = Children(pXmlObjectCombos);
			for(size_t i=0; i<v.size(); ++i) Combo(v[i]);
			mpB->phase(mpB->ctx, OHPL_MAP_PHASE_COMBOS, false);
		}

		mpB->static_compile(mpB->ctx);
	}

	void cMapParser::StaticObject(cXmlElement *apElement)
	{
		tString sName = apElement->GetAttributeString("Name");
		tString sFileName;

		int lFileNameIdx = apElement->GetAttributeInt("FileIndex",-1);
		if(lFileNameIdx < 0)
		{
			apElement->GetAttributeString("MeshFilename");
			if(sFileName=="") sFileName = apElement->GetAttributeString("Filename");
		}
		else
		{
			if(lFileNameIdx < (int)mvFileIndices_StaticObjects.size())
			{
				sFileName = mvFileIndices_StaticObjects[lFileNameIdx];
			}
			else
			{
				EMIT_ERROR(mpB, "Could not load static object '%s', File index: %d does is out of bounds!\n", sName.c_str(), lFileNameIdx);
				return;
			}
		}

		cVector3f vPosition = apElement->GetAttributeVector3f("WorldPos",0);
		cVector3f vScale = apElement->GetAttributeVector3f("Scale",1);
		cVector3f vRotation = apElement->GetAttributeVector3f("Rotation",0);

		bool bCollides = apElement->GetAttributeBool("Collides", true);
		bool bCastsShadows = apElement->GetAttributeBool("CastShadows", true);

		int lID = apElement->GetAttributeInt("ID",-1);

		if(TransformValid(sName, vPosition)==false) return;

		mpB->static_object(mpB->ctx, Str(sName), Str(sFileName), F3(vPosition), F3(vRotation), F3(vScale),
							bCollides, bCastsShadows, lID);
	}

	void cMapParser::Primitive(cXmlElement *apElement)
	{
		tString sType = apElement->GetValue();
		tString sName = apElement->GetAttributeString("Name");
		tString sMaterial = apElement->GetAttributeString("Material");
		bool bCastsShadows = apElement->GetAttributeBool("CastShadows", true);
		bool bCollides = apElement->GetAttributeBool("Collides", true);
		int lID = apElement->GetAttributeInt("ID",-1);

		cVector3f vPosition = apElement->GetAttributeVector3f("WorldPos",0);
		cVector3f vScale = apElement->GetAttributeVector3f("Scale",1);
		cVector3f vRotation = apElement->GetAttributeVector3f("Rotation",0);

		if(TransformValid(sName, vPosition)==false) return;

		if(sType == "Plane")
		{
			cVector3f vStartCorner = apElement->GetAttributeVector3f("StartCorner",0);
			cVector3f vEndCorner = apElement->GetAttributeVector3f("EndCorner",0);
			OhplF2 vUV[4];
			for(int i=0;i<4;++i)
			{
				cVector2f vCorner = apElement->GetAttributeVector2f("Corner" + cString::ToString(i+1) + "UV");
				vUV[i].x = vCorner.x;
				vUV[i].y = vCorner.y;
			}

			mpB->static_plane(mpB->ctx, Str(sName), Str(sMaterial), bCastsShadows, bCollides, lID,
								F3(vPosition), F3(vScale), F3(vRotation), F3(vStartCorner), F3(vEndCorner), vUV);
		}
	}

	void cMapParser::Decal(cXmlElement *apElement)
	{
		tString sName = apElement->GetAttributeString("Name");
		cColor decalColor = apElement->GetAttributeColor("Color", cColor(1,1));
		int lID = apElement->GetAttributeInt("ID",-1);

		tString sMaterial="";
		int lFileNameIdx = apElement->GetAttributeInt("MaterialIndex",-1);
		if(lFileNameIdx < 0)
		{
			sMaterial = apElement->GetAttributeString("Material");
		}
		else
		{
			if(lFileNameIdx < (int)mvFileIndices_Decals.size())
			{
				sMaterial = mvFileIndices_Decals[lFileNameIdx];
			}
			else
			{
				EMIT_ERROR(mpB, "Could not load decal '%s'! File index %d is out of bounds!\n", sName.c_str(), lFileNameIdx);
				return;
			}
		}

		cXmlElement *pDecalMeshElem = apElement->GetFirstElement("DecalMesh");
		mpB->static_decal(mpB->ctx, Str(sName), F4(decalColor), lID, Str(sMaterial), pDecalMeshElem);
	}

	void cMapParser::Combo(cXmlElement *apElement)
	{
		int lGroupID = apElement->GetAttributeInt("ID", -1);
		tString sObjIds = apElement->GetAttributeString("ObjIds","");
		tIntVec vObjIds;
		cString::GetIntVec(sObjIds, vObjIds);

		mpB->static_combo(mpB->ctx, lGroupID, vObjIds.data(), vObjIds.size());
	}

	void cMapParser::LoadEntities(cXmlElement *apContents)
	{
		mpB->entities_begin(mpB->ctx);

		cXmlElement *pXmlEntities = apContents->GetFirstElement("Entities");
		if(pXmlEntities==NULL) { mbAbort = true; return; }

		std::vector<cXmlElement*> v = Children(pXmlEntities);
		for(size_t i=0; i<v.size(); ++i) LoadedEntity(v[i]);

		mpB->entities_end(mpB->ctx);
	}

	void cMapParser::LoadedEntity(cXmlElement *apElement)
	{
		tString sObjectType = apElement->GetValue();

		if(sObjectType == "Entity" || sObjectType == "Area")
		{
			tString sName = apElement->GetAttributeString("Name");
			int lID = apElement->GetAttributeInt("ID");
			bool bActive = apElement->GetAttributeBool("Active", true);
			cVector3f vPosition = apElement->GetAttributeVector3f("WorldPos",0);
			cVector3f vScale = apElement->GetAttributeVector3f("Scale",1);
			cVector3f vRotation = apElement->GetAttributeVector3f("Rotation",0);

			if(TransformValid(sName, vPosition)==false) return;

			if(mlFlags & eWorldLoadFlag_NoGameEntities) return;

			cXmlElement *pUserVars = apElement->GetFirstElement("UserVariables");

			if(sObjectType == "Entity")
			{
				tString sFilename;
				int lFileNameIdx = apElement->GetAttributeInt("FileIndex",-1);
				if(lFileNameIdx < 0)
				{
					sFilename = apElement->GetAttributeString("Filename");
				}
				else
				{
					if(lFileNameIdx < (int)mvFileIndices_Entities.size())
					{
						sFilename = mvFileIndices_Entities[lFileNameIdx];
					}
					else
					{
						EMIT_ERROR(mpB, "Could not load entity '%s'!\n File index %d is out of bounds!\n", sName.c_str(), lFileNameIdx);
						return;
					}
				}

				mpB->entity(mpB->ctx, Str(sName), lID, bActive, F3(vPosition), F3(vRotation), F3(vScale), Str(sFilename), pUserVars);
			}
			else
			{
				tString sType = apElement->GetAttributeString("AreaType","");
				mpB->area(mpB->ctx, Str(sName), lID, bActive, F3(vPosition), F3(vRotation), F3(vScale), Str(sType), pUserVars);
			}
		}
		else if(sObjectType == "FogArea")			mpB->fog_area(mpB->ctx, apElement);
		else if(sObjectType == "ParticleSystem")	mpB->particle_system(mpB->ctx, apElement);
		else if(sObjectType == "Sound")				mpB->sound(mpB->ctx, apElement);
		else if(sObjectType == "Billboard")			mpB->billboard(mpB->ctx, apElement);
		else if(cString::GetLastStringPos(sObjectType,"Light")>0)
		{
			if(mlFlags & eWorldLoadFlag_NoLights) return;

			mpB->light(mpB->ctx, apElement);
		}
		else
		{
			EMIT_ERROR(mpB, "Unknown entity type '%s'\n", sObjectType.c_str());
		}
	}

	}

	int HplMapParse(iXmlNode *apDoc, const OhplMapBuilder *apB, const OhplMapOptions *apOpt, bool abCacheLoaded)
	{
		cMapParser parser(apB, apOpt->flags);
		return parser.Parse(apDoc, abCacheLoaded);
	}
}
