#include "Model.h"
#include "Utils.h"

#include <Rush/UtilFile.h>
#include <Rush/UtilLog.h>

#if USE_ASSIMP
#include <assimp/Importer.hpp>
#include <assimp/postprocess.h>
#include <assimp/scene.h>
#endif

#ifdef __linux__
#define strcpy_s strcpy
#endif

#if USE_ASSIMP
void convertModel(const char* inputModel, const char* outputModel, float modelScale)
{
	Log::message("Converting model '%s' to '%s' using scale %f", inputModel, outputModel, modelScale);
	Model model;
	if (loadModel(inputModel, modelScale, model))
	{
		model.write(outputModel);
	}
}
#endif // USE_ASSIMP

const u32 Model::magic = 0xfe892a37;

bool Model::read(const char* filename)
{
	FileIn stream(filename);
	if (!stream.valid())
	{
		Log::error("Failed to open file '%s' for reading", filename);
		return false;
	}

	u32 actualMagic = 0;
	stream.readT(actualMagic);
	if (actualMagic != magic)
	{
		Log::error("Model format identifier mismatch. Expected 0x%08x, got 0x%08x.", magic, actualMagic);
		return false;
	}

	stream.readT(bounds);
	readContainer(stream, materials);
	readContainer(stream, segments);
	readContainer(stream, vertices);
	readContainer(stream, indices);

	return true;
}

void Model::write(const char* filename)
{
	FileOut stream(filename);
	if (!stream.valid())
	{
		Log::error("Failed to open file '%s' for writing", filename);
	}

	stream.writeT(magic);
	stream.writeT(bounds);
	writeContainer(stream, materials);
	writeContainer(stream, segments);
	writeContainer(stream, vertices);
	writeContainer(stream, indices);
}

#if USE_ASSIMP

inline const char* getAssimpString(const aiMaterialProperty* prop)
{
	RUSH_ASSERT(prop->mType == aiPTI_String);
	return prop->mData + 4; // assimp stores length in first 4 bytes of data
};

inline const Vec3 getAssimpVec3(const aiMaterialProperty* prop)
{
	RUSH_ASSERT(prop->mType == aiPTI_Float);
	RUSH_ASSERT(prop->mDataLength >= 12);
	return Vec3(reinterpret_cast<const float*>(prop->mData));
};

inline Vec3 convertCoordinateSystem(Vec3 v) { return Vec3(-v.x, v.y, -v.z); }

bool loadModel(const char* filename, float modelScale, Model& outModel)
{
	Log::message("Loading model '%s'", filename);

	const bool makeYUp = false;
	const bool flipZ   = false;

	const u32 importFlags = aiProcess_JoinIdenticalVertices | aiProcess_Triangulate | aiProcess_MakeLeftHanded |
	                        aiProcess_FlipWindingOrder | aiProcess_PreTransformVertices | aiProcess_SplitLargeMeshes |
	                        aiProcess_CalcTangentSpace | aiProcess_RemoveRedundantMaterials |
	                        aiProcess_ImproveCacheLocality | aiProcess_FindInvalidData | aiProcess_FlipUVs;

	Assimp::Importer importer;

	const aiScene* scene = importer.ReadFile(filename, importFlags);

	for (u32 materialIt = 0; materialIt < scene->mNumMaterials; ++materialIt)
	{
		const aiMaterial* importedMaterial = scene->mMaterials[materialIt];

		Model::OfflineMaterial material;

		for (u32 propIt = 0; propIt < importedMaterial->mNumProperties; ++propIt)
		{
			const aiMaterialProperty* importedProp = importedMaterial->mProperties[propIt];

			if (!strcmp(importedProp->mKey.C_Str(), "$clr.diffuse"))
			{
				material.baseColor = Vec4(getAssimpVec3(importedProp), 1.0f);
			}
			else if (!strcmp(importedProp->mKey.C_Str(), "$tex.file"))
			{
				switch (importedProp->mSemantic)
				{
				case aiTextureType_DIFFUSE: strcpy_s(material.albedoTexture, getAssimpString(importedProp)); break;
				case aiTextureType_SPECULAR: break;
				case aiTextureType_AMBIENT:
					// TODO: handle as metal mask
					break;
				case aiTextureType_HEIGHT:
				case aiTextureType_NORMALS: strcpy_s(material.normalTexture, getAssimpString(importedProp)); break;
				case aiTextureType_OPACITY:
					// TODO: handle as opacity texture
					break;
				case aiTextureType_SHININESS: strcpy_s(material.roughnessTexture, getAssimpString(importedProp)); break;
				default:
					// Log::warning("Unexpected material texture property");
					break;
				}
			}
		}

		outModel.materials.push_back(material);
	}

	outModel.bounds.expandInit();

	u32 totalVertexCount = 0;
	for (u32 meshIt = 0; meshIt < scene->mNumMeshes; ++meshIt)
	{
		const aiMesh* importedMesh = scene->mMeshes[meshIt];
		totalVertexCount += importedMesh->mNumVertices;
	}

	outModel.vertices.resize(totalVertexCount);

	u32 vertexOffset = 0;
	for (u32 meshIt = 0; meshIt < scene->mNumMeshes; ++meshIt)
	{
		const aiMesh* importedMesh = scene->mMeshes[meshIt];

		if (!importedMesh->HasPositions())
		{
			Log::error("Meshes without positions are not supported");
			continue;
		}

		const bool hasTexcoords = importedMesh->GetNumUVChannels() > 0;
		const bool hasTangent   = importedMesh->HasTangentsAndBitangents();

		for (u32 i = 0; i < importedMesh->mNumVertices; ++i)
		{
			const u32 vertexId = vertexOffset + i;

			const auto& importedVert = importedMesh->mVertices[i];

			Vec3 vertPos;
			vertPos.x = importedVert.x;
			vertPos.y = importedVert.y;
			vertPos.z = importedVert.z;
			vertPos *= modelScale;
			vertPos                              = convertCoordinateSystem(vertPos);
			outModel.vertices[vertexId].position = vertPos;

			outModel.bounds.expand(vertPos);

			if (importedMesh->mNormals)
			{
				Vec3 vertNor;
				vertNor.x                          = importedMesh->mNormals[i].x;
				vertNor.y                          = importedMesh->mNormals[i].y;
				vertNor.z                          = importedMesh->mNormals[i].z;
				vertNor                            = convertCoordinateSystem(vertNor);
				outModel.vertices[vertexId].normal = vertNor;
			}

			if (hasTexcoords)
			{
				Vec2 vertTex;
				vertTex.x                            = importedMesh->mTextureCoords[0][i].x;
				vertTex.y                            = importedMesh->mTextureCoords[0][i].y;
				outModel.vertices[vertexId].texcoord = vertTex;
			}

			if (hasTangent)
			{
				Vec3 vertTanU;
				vertTanU.x = importedMesh->mTangents[i].x;
				vertTanU.y = importedMesh->mTangents[i].y;
				vertTanU.z = importedMesh->mTangents[i].z;
				vertTanU   = convertCoordinateSystem(vertTanU);

				Vec3 vertTanV;
				vertTanV.x = importedMesh->mBitangents[i].x;
				vertTanV.y = importedMesh->mBitangents[i].y;
				vertTanV.z = importedMesh->mBitangents[i].z;
				vertTanV   = convertCoordinateSystem(vertTanV);

				outModel.vertices[vertexId].tangent   = vertTanU;
				outModel.vertices[vertexId].bitangent = vertTanV;
			}
		}

		ModelSegment modelSegment;
		modelSegment.indexOffset = u32(outModel.indices.size());
		modelSegment.indexCount  = importedMesh->mNumFaces * 3;
		modelSegment.material    = importedMesh->mMaterialIndex;

		for (u32 i = 0; i < importedMesh->mNumFaces; ++i)
		{
			const aiFace& face = importedMesh->mFaces[i];
			RUSH_ASSERT(face.mNumIndices == 3);

			u32 idxA = vertexOffset + face.mIndices[0];
			u32 idxB = vertexOffset + face.mIndices[1];
			u32 idxC = vertexOffset + face.mIndices[2];

			outModel.indices.push_back(idxA);
			outModel.indices.push_back(idxB);
			outModel.indices.push_back(idxC);
		}

		outModel.segments.push_back(modelSegment);

		vertexOffset += importedMesh->mNumVertices;
	}

	return true;
}

#endif
