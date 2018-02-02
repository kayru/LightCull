#pragma once

#include <Rush/MathTypes.h>
#include <vector>

struct ModelVertex
{
	Vec3 position;
	Vec3 normal;
	Vec3 tangent;
	Vec3 bitangent;
	Vec2 texcoord;
};

struct ModelSegment
{
	u32 material    = 0;
	u32 indexOffset = 0;
	u32 indexCount  = 0;
};

struct Model
{
	struct OfflineMaterial
	{
		enum
		{
			MaxStringLength = 127,
			StringSize      = MaxStringLength + 1
		};
		char albedoTexture[StringSize]    = {};
		char normalTexture[StringSize]    = {};
		char roughnessTexture[StringSize] = {};
		Vec4 baseColor                    = Vec4(1.0f);
	};

	static const u32 magic;

	Box3                         bounds = Box3(Vec3(0.0f), Vec3(0.0f));
	std::vector<OfflineMaterial> materials;
	std::vector<ModelSegment>    segments;
	std::vector<ModelVertex>     vertices;
	std::vector<u32>             indices;

	bool read(const char* filename);
	void write(const char* filename);
};

#if USE_ASSIMP
bool loadModel(const char* filename, float modelScale, Model& outModel);
void convertModel(const char* input, const char* output, float scale);
#endif
