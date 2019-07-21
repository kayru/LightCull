#pragma once

#include "BaseApplication.h"
#include "ClusteredLightBuilder.h"
#include "Fiber.h"
#include "LightingCommon.h"
#include "Model.h"
#include "Scripting.h"
#include "Shaders/ShaderDefines.h"
#include "TiledLightTreeBuilder.h"
#include "Utils.h"

#include <Rush/GfxBitmapFont.h>
#include <Rush/GfxDevice.h>
#include <Rush/GfxPrimitiveBatch.h>
#include <Rush/MathTypes.h>
#include <Rush/Platform.h>
#include <Rush/UtilCamera.h>
#include <Rush/UtilColor.h>
#include <Rush/UtilRandom.h>
#include <Rush/UtilTimer.h>
#include <Rush/Window.h>

#include <memory>
#include <stdio.h>
#include <string>
#include <unordered_map>

enum class ViewMode
{
	BaseColor,
	Normal,
	Roughness,
	Depth,
	Final,
	LightOverdraw,
	LightOverdrawBruteForce,
	VisitedLights,
	LightTreeVisitedNodes,

	count
};

struct CameraKeyFrame
{
	Vec3 position;
	Vec3 target;
};

struct BenchmarkFrame
{
	u32   frameId;
	float gpuLightingTime;
	float gpuLightingBuildTime;
	u32   listCellCount;
	u32   treeCellCount;
	u32   treePixelCount;
	u32   totalPixelCount;
	float cpuLightBuildTime;
	u32   dataSize;
	u32   visibleLightCount;
};

template <typename T, size_t SIZE> struct StatsAccumulator
{
	inline void reset() { m_cursor = 0; }

	inline void add(T v)
	{
		m_buffer[m_cursor % SIZE] = v;
		m_cursor++;
	}

	inline T getMin() const
	{
		if (m_cursor == 0)
		{
			return T();
		}
		else
		{
			size_t count = min(m_cursor, SIZE);

			T accum = std::numeric_limits<T>::max();
			for (size_t i = 0; i < count; ++i)
			{
				accum = min(accum, m_buffer[i]);
			}
			return accum;
		}
	}

	inline T getAverage() const
	{
		if (m_cursor == 0)
		{
			return T();
		}
		else
		{
			size_t count = min(m_cursor, SIZE);

			T accum = 0;
			for (size_t i = 0; i < count; ++i)
			{
				accum += m_buffer[i];
			}
			return accum / count;
		}
	}

	T      m_buffer[SIZE];
	size_t m_cursor = 0;
};

class LightCullApp : public BaseApplication, public CommandHandler
{
public:
	LightCullApp();
	~LightCullApp();

	void update();
	void updateLights();

private:
	enum Timestamp
	{
		Timestamp_Gbuffer,
		Timestamp_Lighting,
		Timestamp_LightingBuild,
	};

	template <typename T> static void ImGuiEnumCombo(const char* label, T* value);

	void createShaders();

	void createGbuffer(Tuple2i size);

	void updateGui(float dt);

	void draw();
	void drawGbuffer();
	void transcodeGbuffer();

	void buildLighting(GfxContext* ctx);
	void applyLighting();

	void generateTileStats();
	void drawTileStats();

	void generateLights(u32 count, float minIntensity = 0.05f, float maxIntensity = 0.75f);
	void generateLightsOnGeometry(u32 count, float minIntensity = 0.05f, float maxIntensity = 0.75f);
	void animateLights(float elapsedTime, u32 lightCount);

	void generateDebugMeshes();

	void generateMeshes();
	bool loadModel(const char* filename);

	void saveReplayBenchmarkCSV(const char* filename = "replay.csv");
	void saveReplay(const char* filename = "replay.bin");
	void loadReplay(const char* filename = "replay.bin");

	void saveCamera(const char* filename = "camera.bin") const;
	void loadCamera(const char* filename = "camera.bin", bool smooth = true);
	void resetCamera();

	void startReplay();

	GfxRef<GfxTexture> loadTextureFromFile(const std::string& filename);
	GfxRef<GfxTexture> loadTextureFromMemory(const void* data, Tuple2i size, GfxFormat format, bool convertToRGBA8 = false);

	void finishProfilingExperiment();

	Timer m_frameTimer;
	Timer m_globalTimer;

	struct Stats
	{
		StatsAccumulator<double, 60> gpuGbuffer;
		StatsAccumulator<double, 60> gpuLighting;
		StatsAccumulator<double, 60> gpuLightingBuild;
		StatsAccumulator<double, 60> gpuTotal;

		StatsAccumulator<double, 60> cpuLightCull;
		StatsAccumulator<double, 60> cpuLightAssign;
		StatsAccumulator<double, 60> cpuLightBuildTree;
		StatsAccumulator<double, 60> cpuLightSort;
		StatsAccumulator<double, 60> cpuLightUpload;
		StatsAccumulator<double, 60> cpuLightBuildTotal;
		StatsAccumulator<double, 60> cpuLightTotal;

		StatsAccumulator<double, 60> cpuFrameTime;
		StatsAccumulator<double, 60> waitForGpuTime;
	} m_stats;

	void resetStats();

	Camera m_pendingCamera;
	Camera m_currentCamera; // this camera is smoothly interpolated towards m_pendingCamera every frame
	float  m_cameraPositionSmoothing    = 0.9f;
	float  m_cameraOrientationSmoothing = 0.875;
	float  m_cameraFov                  = 1.0;

	Mat4 m_matView = Mat4::identity();
	Mat4 m_matProj = Mat4::identity();

	CameraManipulator m_cameraMan;

	GfxOwn<GfxRasterizerState> m_rasterizerState;
	GfxOwn<GfxRasterizerState> m_rasterizerStateNoCull;

	// yuriy_todo: add a helper wrapper for shader and all the bindings,
	// so everything is in one place (resource declaration, setup and actual dispatch)
	GfxOwn<GfxTechnique> m_techniqueGbuffer;
	GfxOwn<GfxTechnique> m_techniqueTiledLightTreeShading[2];
	GfxOwn<GfxTechnique> m_techniqueTiledLightTreeShadingMasked[2];
	GfxOwn<GfxTechnique> m_techniqueHybridTiledLightTreeShading[2];
	GfxOwn<GfxTechnique> m_techniqueClusteredShading[2];
	GfxOwn<GfxTechnique> m_techniqueTileStatsDisplay;
	GfxOwn<GfxTechnique> m_techniqueTileStatsGenerate;
	GfxOwn<GfxTechnique> m_techniqueTileStatsReduce;
	GfxOwn<GfxTechnique> m_techniqueGbufferTranscode;

	Tuple3<u16> m_threadGroupSizeClusteredShading            = {8, 8, 1};
	Tuple3<u16> m_threadGroupSizeTiledLightTreeShading       = {8, 8, 1};
	Tuple3<u16> m_threadGroupSizeTiledLightTreeShadingMasked = {8, 8, 1};
	Tuple3<u16> m_threadGroupSizeHybridTiledLightTreeShading = {8, 8, 1};

	GfxOwn<GfxSampler> m_samplerAniso8;
	GfxOwn<GfxTexture> m_defaultAlbedoTexture;
	GfxOwn<GfxTexture> m_defaultNormalTexture;
	GfxOwn<GfxTexture> m_defaultRoughnessTexture;
	GfxOwn<GfxTexture> m_gbufferBaseColor;
	GfxOwn<GfxTexture> m_gbufferNormal;
	GfxOwn<GfxTexture> m_gbufferRoughness;
	GfxOwn<GfxTexture> m_gbufferDepth;
	GfxOwn<GfxTexture> m_finalFrame;
	GfxRef<GfxTexture> m_falseColorTexture;
	GfxRef<GfxTexture> m_lightMarkerTexture;
	GfxOwn<GfxBuffer>  m_vertexBuffer;
	GfxOwn<GfxBuffer>  m_indexBuffer;
	GfxOwn<GfxBuffer>  m_sceneConstantBuffer;
	GfxOwn<GfxBuffer>  m_instanceConstantBuffer;
	GfxOwn<GfxBuffer>  m_lightingConstantBuffer;
	GfxOwn<GfxBuffer>  m_tileStatsBuffer; // contains count of pixels that used a light tree (as opposed to a light list)
	GfxOwn<GfxBuffer>  m_tileStatsReducedBuffer;
	GfxMappedBuffer m_tileStatsReducedMappedBuffer;

	GfxPassFlags m_gbufferClearFlags = GfxPassFlags::ClearAll;

	struct StaticGbuffer
	{
		GfxOwn<GfxTexture> depth;
		GfxOwn<GfxTexture> albedo;
		GfxOwn<GfxTexture> normals;
		GfxOwn<GfxTexture> specularRM;
	};

	StaticGbuffer m_staticGbuffer;
	bool          m_useStaticGbuffer = false;

	u32 m_indexCount  = 0;
	u32 m_vertexCount = 0;

	struct SceneConstants
	{
		Mat4 matViewProj = Mat4::identity();
	};

	struct InstanceConstants
	{
		Mat4 matWorld     = Mat4::identity();
		Vec4 baseColor    = Vec4(1.0f);
		int  useNormalMap = 1;
	};

	struct LightingConstants
	{
		Mat4 matView;
		Mat4 matProjInv;

		u32 lightCount;
		u32 outputWidth;
		u32 outputHeight;
		u32 debugMode;

		u32 nodeCount;
		u32 offsetX;
		u32 offsetY;
		u32 tileSize;

		float lightGridDepthMin;
		float lightGridDepthMax;
		float lightGridExtents;
		u32   lightGridSliceCount;

		Vec4 ambientLight;

		u32 useExponentialSlices;
		u32 flipClipSpaceY;
		u32 constantOne;
		u32 useShallowTree;
	};

	LightingConstants m_lightingConstants = {};

	std::unordered_map<std::string, GfxRef<GfxTexture>> m_textures;

	struct Material
	{
		GfxRef<GfxTexture> albedoTexture;
		GfxRef<GfxTexture> normalTexture;
		GfxRef<GfxTexture> roughnessTexture;
		Vec4               baseColor = Vec4(1.0f);
	};

	std::vector<Material>     m_materials;
	std::vector<ModelSegment> m_segments;

	WindowEventListener m_windowEvents;

	ViewMode m_viewMode   = ViewMode::Final;
	u32      m_frameCount = 0;

	enum
	{
		MaxLights = 65536
	};
	int m_lightCount = 10000;

	std::vector<LightSource> m_viewSpaceLights;
	GfxOwn<GfxBuffer>        m_lightSourceBuffer; // contains data from m_viewSpaceLights

	std::vector<AnimatedLightSource> m_lights;

	static constexpr u32 m_randomSeed = 2;
	Rand                 m_rng        = Rand(m_randomSeed);

	std::unique_ptr<Model> m_model;

	Box3 m_worldBoundingBox;
	Box3 m_lightAnimationBounds;

	Vec3  m_ambientTint      = Vec3(1.0f);
	float m_ambientIntensity = 0.125f;

	bool  m_drawLightMarkers   = false;
	bool  m_depthTestMarkers   = true;
	bool  m_animateLights      = false;
	float m_lightAnimationTime = 0.0f;
	u32   m_visibleLightCount  = 0;

	bool m_enableVsync   = true;
	bool m_drawTileStats = false;
	bool m_drawTileGrid  = false;
	u32  m_tileSize      = 48;
	bool m_enableGui     = true;

	enum class ReplayState
	{
		Idle,
		Record,
		Play
	};
	ReplayState                 m_replayState = ReplayState::Idle;
	u32                         m_replayFrame = 0;
	std::vector<CameraKeyFrame> m_replayCameraFrames;
	std::vector<BenchmarkFrame> m_replayBenchmarkFrames;

	LightingMode m_lightingMode          = LightingMode::Tree;
	bool         m_useAsyncCompute       = false;
	u32          m_useTileFrustumCulling = 2;

	TiledLightTreeBuilder* m_tiledLightTreeBuilder = nullptr;

#if USE_GPU_BUILDER
	TiledLightTreeBuilderGPU* m_tiledLightTreeBuilderGPU = nullptr;
	bool                      m_useGpuLightTreeBuilder   = true;
#else
	static const bool m_useGpuLightTreeBuilder = false;
#endif

	TiledLightTreeBuildParams m_tiledLightTreeBuilderParams;
	TiledLightTreeBuildResult m_tiledLightTreeBuildResult;

	ClusteredLightBuilder*             m_clusteredLightBuilder = nullptr;
	ClusteredLightBuilder::BuildParams m_clusteredLightBuilderParams;
	ClusteredLightBuildResult          m_clusteredLightBuildResult;

	enum class State
	{
		Idle,
		InitDefault,
		RunScript,
		Benchmark,
		BenchmarkReplay,
		Quit
	};

	static inline bool isBenchmarkState(State state)
	{
		switch (state)
		{
		case State::Benchmark:
		case State::BenchmarkReplay: return true;
		default: return false;
		}
	}

	struct LightGenerationParams
	{
		int   count        = 0;
		float minIntensity = 0;
		float maxIntensity = 0;
	};

	LightGenerationParams m_pendingLightParams;
	LightGenerationParams m_currentLightParams;

	State       m_state                    = State::Idle;
	u32         m_benchmarkFramesRemaining = 0;
	std::string m_benchmarkExperimentName;

	std::string m_modelName;
	std::string m_scriptName;

	void* m_scriptFiber = nullptr;
	void* m_mainFiber   = nullptr;

	std::string m_captureVideoPath;
	FILE*       m_ffmpegPipe = nullptr;

	static void videoCaptureCallback(const ColorRGBA8* pixels, Tuple2u size, void* userData);
	void        videoCaptureCallback(const ColorRGBA8* pixels, Tuple2u size);

	static constexpr bool m_useLightIndexBuffer = ENABLE_LIGHT_INDEX_BUFFER;
	static_assert(m_useLightIndexBuffer, "Non-indexed lighting is not implemented");

	struct DebugMesh
	{
		GfxOwn<GfxBuffer> m_vertexBuffer;
		GfxOwn<GfxBuffer> m_indexBuffer;
		u32               m_indexCount  = 0;
		u32               m_vertexCount = 0;
	};

	DebugMesh m_debugSphereMesh;
	bool      m_drawDebugSpheres = false;
	void      drawDebugSpheres();

	// Near TL, TR, BL, BR, Near TL, TR, BL, BR
	Vec3    m_debugFrustumPoints[8] = {};
	bool    m_drawDebugFrustum      = false;
	Tuple2i m_debugTileId           = {0, 0};

	void captureCameraFrustum();
	void captureTileFrustum();
	void drawDebugFrustum();

	static void FIBER_CALL scriptFiber(void*);

	// scripting interface
	virtual void processCommand(const CmdLoadScene& cmd) override;
	virtual void processCommand(const CmdCameraLookAt& cmd) override;
	virtual void processCommand(const CmdGenerateLights& cmd) override;
	virtual void processCommand(const CmdGenerateLightsOnGeometry& cmd) override;
	virtual void processCommand(const CmdBenchmark& cmd) override;
	virtual void processCommand(const CmdSetLightingMode& cmd) override;
	virtual void processCommand(const CmdSetLightTreeParams& cmd) override;
	virtual void processCommand(const CmdSetClusteredShadingParams& cmd) override;
	virtual void processCommand(const CmdWriteReport& cmd) override;
	virtual void processCommand(const CmdSetLightCount& cmd) override;
	virtual void processCommand(const CmdSetLightAnimationBounds& cmd) override;
	virtual void processCommand(const CmdQuit& cmd) override;
	virtual void processCommand(const CmdSetWindowSize& cmd) override;
	virtual void processCommand(const CmdLoadCamera& cmd) override;
	virtual void processCommand(const CmdBenchmarkReplay& cmd) override;
	virtual void processCommand(const CmdBenchmarkSave& cmd) override;
	virtual void processCommand(const CmdSetTileSize& cmd) override;
	virtual void processCommand(const CmdGenerateMeshes& cmd) override;
	virtual void processCommand(const CmdCaptureVideo& cmd) override;
	virtual void processCommand(const CmdLoadStaticScene& cmd) override;
};
