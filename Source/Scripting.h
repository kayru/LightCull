#pragma once

#include "LightingCommon.h"

#include <Rush/MathTypes.h>
#include <string>

struct CmdLoadScene
{
	std::string filename;
};

struct CmdCameraLookAt
{
	Vec3 position;
	Vec3 target;
};

struct CmdGenerateLights
{
	float minIntensity = 0.05f;
	float maxIntensity = 0.75f;
};

struct CmdGenerateLightsOnGeometry
{
	float minIntensity = 0.05f;
	float maxIntensity = 0.75f;
};

struct CmdBenchmark
{
	std::string experimentName;
};

struct CmdSetLightingMode
{
	LightingMode mode;
};

struct CmdSetLightTreeParams
{
	u32   sliceCount             = 16;
	float maxSliceDepth          = 500.0f;
	bool  useExponentialSlices   = false;
	bool  useShallowTree         = false;
	int   targetLightsPerLeaf    = 6;
	bool  useGpuLightTreeBuilder = true;
};

struct CmdSetClusteredShadingParams
{
	u32   sliceCount           = 16;
	float maxSliceDepth        = 500.0f;
	bool  useExponentialSlices = true;
};

struct CmdWriteReport
{
	std::string filename;
};

struct CmdSetLightCount
{
	u32 count = 0;
};

struct CmdSetLightAnimationBounds
{
	Vec3 min;
	Vec3 max;
};

struct CmdQuit
{
};

struct CmdSetWindowSize
{
	int width;
	int height;
};

struct CmdLoadCamera
{
	std::string filename;
};

struct CmdBenchmarkReplay
{
	std::string filename;
};

struct CmdBenchmarkSave
{
	std::string filename;
};

struct CmdSetTileSize
{
	u32 size;
};

struct CmdGenerateMeshes
{
};

struct CmdCaptureVideo
{
	std::string path;
};

struct CmdLoadStaticScene
{
	std::string depth;
	std::string albedo;
	std::string normals;
	std::string specularRM; // roughness & metal mask
	std::string gbufferFormat;
	std::string coordinateSystem;

	Mat4 viewMatrix       = Mat4::identity();
	Mat4 projectionMatrix = Mat4::identity();

	struct PointLight
	{
		Vec3  position;
		float radius;
	};

	std::vector<PointLight> pointLights;
};

class CommandHandler
{
public:
	virtual void processCommand(const CmdLoadScene& cmd)                 = 0;
	virtual void processCommand(const CmdCameraLookAt& cmd)              = 0;
	virtual void processCommand(const CmdGenerateLights& cmd)            = 0;
	virtual void processCommand(const CmdGenerateLightsOnGeometry& cmd)  = 0;
	virtual void processCommand(const CmdBenchmark& cmd)                 = 0;
	virtual void processCommand(const CmdSetLightingMode& cmd)           = 0;
	virtual void processCommand(const CmdSetLightTreeParams& cmd)        = 0;
	virtual void processCommand(const CmdSetClusteredShadingParams& cmd) = 0;
	virtual void processCommand(const CmdWriteReport& cmd)               = 0;
	virtual void processCommand(const CmdSetLightCount& cmd)             = 0;
	virtual void processCommand(const CmdSetLightAnimationBounds& cmd)   = 0;
	virtual void processCommand(const CmdQuit& cmd)                      = 0;
	virtual void processCommand(const CmdSetWindowSize& cmd)             = 0;
	virtual void processCommand(const CmdLoadCamera& cmd)                = 0;
	virtual void processCommand(const CmdBenchmarkReplay& cmd)           = 0;
	virtual void processCommand(const CmdBenchmarkSave& cmd)             = 0;
	virtual void processCommand(const CmdSetTileSize& cmd)               = 0;
	virtual void processCommand(const CmdGenerateMeshes& cmd)            = 0;
	virtual void processCommand(const CmdCaptureVideo& cmd)              = 0;
	virtual void processCommand(const CmdLoadStaticScene& cmd)           = 0;
};

void runCommandsFromJsonFile(const char* filename, CommandHandler* handler);
