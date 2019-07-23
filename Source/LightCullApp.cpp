#include "LightCullApp.h"

#include "DiscreteDistribution.h"
#include "EmbeddedAssets.h"
#include "Shader.h"
#include "Utils.h"

#include <Rush/MathTypes.h>
#include <Rush/UtilFile.h>
#include <Rush/UtilLog.h>

#include "ImGuiImpl.h"
#include <imgui.h>

#define STB_PERLIN_IMPLEMENTATION
#include <stb_perlin.h>

#pragma warning(push)
#pragma warning(disable : 4244 4305 4081 4838 4996)
#define PAR_SHAPES_IMPLEMENTATION
#include <par_shapes.h>
#pragma warning(pop)

#include <algorithm>
#include <sstream>
#include <stdlib.h>

static AppConfig g_appConfig;

int main(int argc, char** argv)
{
	g_appConfig.name = "Light Culling";

	g_appConfig.width  = 1920;
	g_appConfig.height = 1080;
	g_appConfig.argc   = argc;
	g_appConfig.argv   = argv;

	g_appConfig.resizable = true;

#ifdef _DEBUG
	g_appConfig.debug = true; // Enable validation layers
	Log::breakOnError = true;
#endif

#if USE_ASSIMP
	if (argc > 4 && !strcmp(argv[1], "convert"))
	{
		const char* inputModel  = argv[2];
		const char* outputModel = argv[3];
		float       modelScale  = 1.0f;
		if (argc > 4)
		{
			modelScale = (float)atof(argv[4]);
		}

		convertModel(inputModel, outputModel, modelScale);
		return 0;
	}
#endif // USE_ASSIMP

	GfxConfig gfxConfig(g_appConfig);
	gfxConfig.preferredCoordinateSystem = GfxConfig::PreferredCoordinateSystem_Direct3D;

	g_appConfig.gfxConfig = &gfxConfig;

	return Platform_Main<LightCullApp>(g_appConfig);
}

template <typename T> T enumNext(T e) { return (T)(((u32)e + 1) % (u32)T::count); }

LightCullApp::LightCullApp() : BaseApplication()
{
	resetCamera();

	m_mainFiber = convertThreadToFiber();

	createGbuffer(m_window->getSize());

	{
		GfxRasterizerDesc rasterizerDesc;
		rasterizerDesc.cullMode = GfxCullMode::CW;
		m_rasterizerState = Gfx_CreateRasterizerState(rasterizerDesc);
	}

	{
		GfxRasterizerDesc rasterizerDesc;
		rasterizerDesc.cullMode = GfxCullMode::None;
		m_rasterizerStateNoCull = Gfx_CreateRasterizerState(rasterizerDesc);
	}

	{
		const u32 defaultAlbedoPixels[] = {0xFFFFFFFF};
		const u32 defaultNormalPixels[] = {0x00FF0000};

		GfxTextureDesc defaultTextureDesc = GfxTextureDesc::make2D(1, 1);
		m_defaultAlbedoTexture = Gfx_CreateTexture(defaultTextureDesc, defaultAlbedoPixels);
		m_defaultNormalTexture = Gfx_CreateTexture(defaultTextureDesc, defaultNormalPixels);

#if 0
		u32 roughnessTextureWidth = 512;
		std::vector<u32> roughnessPixels(roughnessTextureWidth*roughnessTextureWidth);
		for (u32 i = 0; i < roughnessTextureWidth*roughnessTextureWidth; ++i)
		{
			u32 x = i % roughnessTextureWidth;
			u32 y = i / roughnessTextureWidth;

			float tx = 32.0f * x / (roughnessTextureWidth - 1);
			float ty = 32.0f * y / (roughnessTextureWidth - 1);

			float noise = stb_perlin_noise3(tx, ty, 0, 32, 32, 0) * 0.5f + 0.5f;
			noise += ((x/16 + y/16) & 1) ? -0.25f : 0.25f;
			noise = saturate(noise);

			u8 v = u8(32.0f + (255.0f-32.0f)*noise);
			ColorRGBA8 c(v, v, v, 255);
			roughnessPixels[i] = c;
		}
#else
		u32              roughnessTextureWidth = 4;
		std::vector<u32> roughnessPixels(roughnessTextureWidth * roughnessTextureWidth, ColorRGBA8(64, 64, 64, 255));
#endif

		GfxTextureDesc roughnessTextureDesc = GfxTextureDesc::make2D(roughnessTextureWidth, roughnessTextureWidth);
		m_defaultRoughnessTexture = Gfx_CreateTexture(roughnessTextureDesc, roughnessPixels.data());
	}

	{
		GfxSamplerDesc samplerDesc = GfxSamplerDesc::makeLinear();
		samplerDesc.anisotropy     = 8;
		m_samplerAniso8 = Gfx_CreateSamplerState(samplerDesc);
	}

	createShaders();

	m_sceneConstantBuffer = Gfx_CreateConstantBuffer(GfxBufferFlags::Transient, sizeof(SceneConstants));
	m_instanceConstantBuffer = Gfx_CreateConstantBuffer(GfxBufferFlags::Transient, sizeof(InstanceConstants));
	m_lightingConstantBuffer = Gfx_CreateConstantBuffer(GfxBufferFlags::Transient, sizeof(LightingConstants));

	if (g_appConfig.argc > 1 && endsWith(g_appConfig.argv[1], ".json"))
	{
		m_scriptName = g_appConfig.argv[1];
		m_state      = State::RunScript;
	}
	else
	{
		m_modelName = g_appConfig.argc > 1 ? g_appConfig.argv[1] : "";

		if (g_appConfig.argc < 2 || !loadModel(m_modelName.c_str()))
		{
			generateMeshes();
		}

		float minLightIntensity = 0.05f;
		float maxLightIntensity = 0.75f;

		// apply some scene-specific hacks for the demo
		if (strstr(m_modelName.c_str(), "TitanPass"))
		{
			// TODO: load lighting setup from file
			m_lightAnimationBounds.m_min = Vec3(-77.0f, -50.0f, -172.0f);
			m_lightAnimationBounds.m_max = Vec3(144.0f, 60.0f, 340.0f);
			maxLightIntensity            = 2.0f;

			generateLights(MaxLights, minLightIntensity, maxLightIntensity);
		}
		else if (strstr(m_modelName.c_str(), "EpicCitadel"))
		{
			maxLightIntensity = 0.75f;
			generateLightsOnGeometry(MaxLights, minLightIntensity, maxLightIntensity);
		}
		else
		{
			generateLights(MaxLights, minLightIntensity, maxLightIntensity);
		}

		loadCamera();
	}

	generateDebugMeshes();

	m_currentCamera = m_pendingCamera;

	m_cameraMan = CameraManipulator(m_window->getKeyboardState(), m_window->getMouseState());

	m_windowEvents.setOwner(m_window);

#if USE_GPU_BUILDER
	m_tiledLightTreeBuilderGPU = new TiledLightTreeBuilderGPU(MaxLights);
#endif

	m_tiledLightTreeBuilder                            = new TiledLightTreeBuilder(MaxLights);
	m_tiledLightTreeBuilderParams.sliceCount           = 16;
	m_tiledLightTreeBuilderParams.maxSliceDepth        = 60.0f;
	m_tiledLightTreeBuilderParams.useExponentialSlices = false;

	m_clusteredLightBuilder                            = new ClusteredLightBuilder(MaxLights);
	m_clusteredLightBuilderParams.sliceCount           = 16;
	m_clusteredLightBuilderParams.maxSliceDepth        = 500.0f;
	m_clusteredLightBuilderParams.useExponentialSlices = true;

	const bool convertToRGBA8 = true;
	m_falseColorTexture =
	    loadTextureFromMemory(g_viridisTextureData, g_viridisTextureSize, g_viridisTextureFormat, convertToRGBA8);
	m_lightMarkerTexture = loadTextureFromMemory(
	    g_lightMarkerTextureData, g_lightMarkerTextureSize, g_lightMarkerTextureFormat, convertToRGBA8);

	ImGuiImpl_Startup(m_window, m_dev);
}

LightCullApp::~LightCullApp()
{
#if USE_FFMPEG
	if (m_ffmpegPipe)
	{
		_pclose(m_ffmpegPipe);
		m_ffmpegPipe = nullptr;
	}
#endif // USE_FFMPEG

	ImGuiImpl_Shutdown();

	delete m_clusteredLightBuilder;
	delete m_tiledLightTreeBuilder;

	m_windowEvents.setOwner(nullptr);
}

void LightCullApp::updateLights()
{
	if (m_viewSpaceLights.size() != m_lightCount)
	{
		m_viewSpaceLights.resize(m_lightCount);
	}

	// TODO: move this to GPU
	// for (int i = 0; i < m_lightCount; ++i)
	parallelForEach(m_viewSpaceLights.begin(), m_viewSpaceLights.end(), [&](LightSource& viewSpaceLight) {
		u64                        i     = &viewSpaceLight - m_viewSpaceLights.data();
		const AnimatedLightSource& light = m_lights[i];
		viewSpaceLight                   = light;
		viewSpaceLight.position          = transformPoint(m_matView, light.position);
	});

	LightSource dummyLight    = {};
	const void* lightData     = nullptr;
	size_t      lightDataSize = 0;

	if (m_viewSpaceLights.empty())
	{
		lightData     = &dummyLight;
		lightDataSize = sizeof(dummyLight);
	}
	else
	{
		lightData     = m_viewSpaceLights.data();
		lightDataSize = sizeof(LightSource) * m_viewSpaceLights.size();
	}

	if (!m_lightSourceBuffer.valid())
	{
		GfxBufferDesc bufferDesc;
		bufferDesc.count  = MaxLights;
		bufferDesc.stride = (u32)sizeof(LightSource);
		bufferDesc.flags  = GfxBufferFlags::Transient | GfxBufferFlags::Storage;
		bufferDesc.format = GfxFormat_Unknown;
		m_lightSourceBuffer = Gfx_CreateBuffer(bufferDesc);
	}
	Gfx_UpdateBuffer(m_ctx, m_lightSourceBuffer, lightData, u32(lightDataSize));
}

void LightCullApp::update()
{
	switch (m_state)
	{
	case State::Idle: break;
	case State::InitDefault: break;
	case State::RunScript:
		if (m_scriptFiber == nullptr)
		{
			m_scriptFiber = createFiber(scriptFiber, this);
		}
		m_state = State::Idle;
		switchToFiber(m_scriptFiber);
		break;
	case State::Benchmark:
		--m_benchmarkFramesRemaining;
		if (m_benchmarkFramesRemaining == 0)
		{
			finishProfilingExperiment();
			m_state = State::RunScript;
			return;
		}
		break;
	case State::BenchmarkReplay:
		if (m_replayFrame == m_replayCameraFrames.size())
		{
			m_state = State::RunScript;
			switchToFiber(m_scriptFiber);
			return;
		}
		break;
	case State::Quit: m_window->close(); return;
	}

	// m_stats.waitForGpuTime.add(Gfx_Stats().waitForGpuTime); // TODO
	m_stats.gpuTotal.add(Gfx_Stats().lastFrameGpuTime);
	m_stats.gpuGbuffer.add(Gfx_Stats().customTimer[Timestamp_Gbuffer]);
	m_stats.gpuLighting.add(Gfx_Stats().customTimer[Timestamp_Lighting]);

#if USE_GPU_BUILDER
	m_stats.gpuLightingBuild.add(m_useGpuLightTreeBuilder ? Gfx_Stats().customTimer[Timestamp_LightingBuild] : 0);
#endif

	const float dt = (float)m_frameTimer.time();
	m_stats.cpuFrameTime.add(dt);

	m_frameTimer.reset();

	if (!isBenchmarkState(m_state))
	{
		updateGui(dt);
	}

	if (m_replayState == ReplayState::Play)
	{
		if (m_replayFrame < m_replayCameraFrames.size())
		{
			BenchmarkFrame benchmarkFrame       = {};
			benchmarkFrame.frameId              = m_replayFrame;
			benchmarkFrame.gpuLightingTime      = (float)m_stats.gpuLighting.getAverage();
			benchmarkFrame.gpuLightingBuildTime = (float)m_stats.gpuLightingBuild.getAverage();

			if (m_lightingMode == LightingMode::Hybrid || m_lightingMode == LightingMode::Tree)
			{
				benchmarkFrame.treeCellCount = m_tiledLightTreeBuildResult.treeCellCount;
				benchmarkFrame.listCellCount = m_tiledLightTreeBuildResult.listCellCount;

				// there's a race with the GPU, as access to this counter is unsycnronized. This is OK for our purposes
				// though
				memcpy(&benchmarkFrame.treePixelCount, m_tileStatsReducedMappedBuffer.data,
				    m_tileStatsReducedMappedBuffer.size);
			}

			const GfxTextureDesc& outputDesc = Gfx_GetTextureDesc(m_finalFrame);
			benchmarkFrame.totalPixelCount   = outputDesc.width * outputDesc.height;

			benchmarkFrame.cpuLightBuildTime = (float)m_stats.cpuLightBuildTotal.getAverage();
			if (m_lightingMode == LightingMode::Hybrid || m_lightingMode == LightingMode::Tree)
			{
				benchmarkFrame.dataSize = m_tiledLightTreeBuildResult.totalDataSize;
			}
			else
			{
				benchmarkFrame.dataSize = m_clusteredLightBuildResult.totalDataSize;
			}

			benchmarkFrame.visibleLightCount = m_visibleLightCount;

			m_replayBenchmarkFrames.push_back(benchmarkFrame);

			m_pendingCamera.lookAt(
			    m_replayCameraFrames[m_replayFrame].position, m_replayCameraFrames[m_replayFrame].target);
			m_currentCamera = m_pendingCamera;

			m_replayFrame++;
		}
		else
		{
			m_replayState = ReplayState::Idle;
		}
	}

	Gfx_ResetStats();

	if (!isBenchmarkState(m_state))
	{
		m_pendingCamera.setAspect(m_window->getAspect());
		m_cameraMan.setMoveSpeed(20.0f);

		if (!ImGui::GetIO().WantCaptureKeyboard && !ImGui::GetIO().WantCaptureMouse)
		{
			m_cameraMan.update(&m_pendingCamera, dt, m_window->getKeyboardState(), m_window->getMouseState());
		}

		if (m_animateLights)
		{
			animateLights(m_lightAnimationTime, m_lightCount);
			m_lightAnimationTime += dt;
		}

		Tuple2i pendingWindowSize = {-1, -1};
		for (const auto& e : m_windowEvents)
		{
			switch (e.type)
			{
			case WindowEventType_Resize:
				pendingWindowSize.x = e.width;
				pendingWindowSize.y = e.height;
				break;

			case WindowEventType_KeyDown:
				switch (e.code)
				{
				case Key_F1: m_enableGui = !m_enableGui; break;
				case Key_F2: saveCamera(); break;
				case Key_F3: loadCamera(); break;
				case Key_F4: resetCamera(); break;
				case Key_1: m_viewMode = ViewMode::BaseColor; break;
				case Key_2: m_viewMode = ViewMode::Normal; break;
				case Key_3: m_viewMode = ViewMode::Roughness; break;
				case Key_4: m_viewMode = ViewMode::Depth; break;
				case Key_5: m_viewMode = ViewMode::Final; break;
				case Key_6: m_viewMode = ViewMode::VisitedLights; break;
				case Key_7: m_viewMode = ViewMode::LightOverdraw; break;
				case Key_8: m_viewMode = ViewMode::LightTreeVisitedNodes; break;
				case Key_Space: m_animateLights = !m_animateLights; break;
				case Key_M: m_drawLightMarkers = !m_drawLightMarkers; break;
				case Key_N: m_depthTestMarkers = !m_depthTestMarkers; break;
				case Key_B: m_drawDebugSpheres = !m_drawDebugSpheres; break;
				case Key_G: m_drawTileGrid = !m_drawTileGrid; break;
				case Key_H: m_drawTileStats = !m_drawTileStats; break;
				case Key_V: m_viewMode = enumNext(m_viewMode); break;
				case Key_C: m_lightingMode = enumNext(m_lightingMode); break;
				case Key_P: startReplay(); break;
				}
				break;
			}
		}
		m_windowEvents.clear();

		if (pendingWindowSize.x > 0 && pendingWindowSize.y > 0)
		{
			createGbuffer(pendingWindowSize);
		}

		{
			m_pendingCamera.setFov(m_cameraFov);

			float t1 = pow(pow(m_cameraPositionSmoothing, 60.0f), dt);
			float t2 = pow(pow(m_cameraOrientationSmoothing, 60.0f), dt);
			m_currentCamera.blendTo(m_pendingCamera, 1.0f - t1, 1.0f - t2);
		}

		if (m_replayState == ReplayState::Record)
		{
			CameraKeyFrame keyFrame;
			keyFrame.position = m_currentCamera.getPosition();
			keyFrame.target   = keyFrame.position + m_currentCamera.getForward();
			m_replayCameraFrames.push_back(keyFrame);
		}
	}

	const GfxCapability& caps = Gfx_GetCapability();

	m_matView = m_currentCamera.buildViewMatrix();
	m_matProj = m_currentCamera.buildProjMatrix(caps.projectionFlags);

	updateLights();

	draw();
}

void LightCullApp::transcodeGbuffer()
{
	const GfxCapability& caps = Gfx_GetCapability();

	GfxPassDesc passDesc;
	passDesc.depth          = m_gbufferDepth.get();
	passDesc.clearDepth     = 1.0f;
	passDesc.clearColors[0] = ColorRGBA::Black();
	passDesc.clearColors[1] = ColorRGBA::Black();
	passDesc.clearColors[2] = ColorRGBA::Black();
	passDesc.color[0]       = m_gbufferBaseColor.get();
	passDesc.color[1]       = m_gbufferNormal.get();
	passDesc.color[2]       = m_gbufferRoughness.get();
	passDesc.flags          = m_gbufferClearFlags;

	// Only clear depth on subsequent frames
	m_gbufferClearFlags = GfxPassFlags::ClearDepthStencil;

	Gfx_BeginPass(m_ctx, passDesc);

	Gfx_SetDepthStencilState(m_ctx, m_depthStencilStates.writeLessEqual);
	Gfx_SetRasterizerState(m_ctx, m_rasterizerState);
	Gfx_SetBlendState(m_ctx, m_blendStates.opaque);

	Gfx_SetTechnique(m_ctx, m_techniqueGbufferTranscode);

	Gfx_SetSampler(m_ctx, GfxStage::Pixel, 0, m_samplerStates.pointClamp);

	Gfx_SetTexture(m_ctx, GfxStage::Pixel, 0, m_staticGbuffer.depth);
	Gfx_SetTexture(m_ctx, GfxStage::Pixel, 1, m_staticGbuffer.albedo);
	Gfx_SetTexture(m_ctx, GfxStage::Pixel, 2, m_staticGbuffer.normals);
	Gfx_SetTexture(m_ctx, GfxStage::Pixel, 3, m_staticGbuffer.specularRM);

	Gfx_Draw(m_ctx, 0, 3);

	Gfx_EndPass(m_ctx);
}

void LightCullApp::drawGbuffer()
{
	const GfxCapability& caps = Gfx_GetCapability();

	GfxMarkerScope markerScope(m_ctx, "gbuffer");

	GfxPassDesc passDesc;
	passDesc.depth          = m_gbufferDepth.get();
	passDesc.clearDepth     = 1.0f;
	passDesc.clearColors[0] = ColorRGBA::Black();
	passDesc.clearColors[1] = ColorRGBA::Black();
	passDesc.clearColors[2] = ColorRGBA::Black();
	passDesc.color[0]       = m_gbufferBaseColor.get();
	passDesc.color[1]       = m_gbufferNormal.get();
	passDesc.color[2]       = m_gbufferRoughness.get();
	passDesc.flags          = m_gbufferClearFlags;

	// Only clear depth on subsequent frames
	m_gbufferClearFlags = GfxPassFlags::ClearDepthStencil;

	Gfx_BeginPass(m_ctx, passDesc);

	SceneConstants sceneConstants;
	sceneConstants.matViewProj = (m_matView * m_matProj).transposed();
	Gfx_UpdateBuffer(m_ctx, m_sceneConstantBuffer, &sceneConstants, sizeof(sceneConstants));

	Gfx_SetDepthStencilState(m_ctx, m_depthStencilStates.writeLessEqual);
	Gfx_SetRasterizerState(m_ctx, m_rasterizerState);

	Gfx_SetBlendState(m_ctx, m_blendStates.opaque);

	Gfx_SetTechnique(m_ctx, m_techniqueGbuffer);
	Gfx_SetVertexStream(m_ctx, 0, m_vertexBuffer);
	Gfx_SetIndexStream(m_ctx, m_indexBuffer);
	Gfx_SetConstantBuffer(m_ctx, 0, m_sceneConstantBuffer);
	Gfx_SetConstantBuffer(m_ctx, 1, m_instanceConstantBuffer);

	u32 currentMaterial = 0xFFFFFFFE;
	for (const ModelSegment& segment : m_segments)
	{
		InstanceConstants instanceConstants;

		if (currentMaterial != segment.material)
		{
			Gfx_SetSampler(m_ctx, GfxStage::Pixel, 0, m_samplerAniso8);

			if (segment.material != 0xFFFFFFFF)
			{
				const Material& material = m_materials[segment.material];

				instanceConstants.baseColor = material.baseColor;

				Gfx_SetTexture(m_ctx, GfxStage::Pixel, 0, material.albedoTexture);
				Gfx_SetTexture(m_ctx, GfxStage::Pixel, 1, material.normalTexture);
				Gfx_SetTexture(m_ctx, GfxStage::Pixel, 2, material.roughnessTexture);

				instanceConstants.useNormalMap = material.normalTexture.get() == m_defaultNormalTexture.get() ? 0 : 1;
			}
			else
			{
				instanceConstants.baseColor = Vec4(1.0f);

				Gfx_SetTexture(m_ctx, GfxStage::Pixel, 0, m_defaultAlbedoTexture);
				Gfx_SetTexture(m_ctx, GfxStage::Pixel, 1, m_defaultNormalTexture);
				Gfx_SetTexture(m_ctx, GfxStage::Pixel, 2, m_defaultRoughnessTexture);

				instanceConstants.useNormalMap = 0;
			}

			Gfx_UpdateBuffer(m_ctx, m_instanceConstantBuffer, &instanceConstants, sizeof(instanceConstants));

			currentMaterial = segment.material;
		}

		Gfx_DrawIndexed(m_ctx, segment.indexCount, segment.indexOffset, 0, m_vertexCount);
	}

	Gfx_EndPass(m_ctx);

	Gfx_AddImageBarrier(m_ctx, m_gbufferBaseColor, GfxResourceState_ShaderRead);
	Gfx_AddImageBarrier(m_ctx, m_gbufferNormal, GfxResourceState_ShaderRead);
	Gfx_AddImageBarrier(m_ctx, m_gbufferRoughness, GfxResourceState_ShaderRead);
	Gfx_AddImageBarrier(m_ctx, m_gbufferDepth, GfxResourceState_ShaderRead);
}

void LightCullApp::buildLighting(GfxContext* ctx)
{
	Timer timer;

	GfxMarkerScope markerScope(ctx, "buildLighting");

	const GfxCapability& caps = Gfx_GetCapability();

	const GfxTextureDesc& outputDesc = Gfx_GetTextureDesc(m_finalFrame);

	const Tuple2u outputResolution = Tuple2u{outputDesc.width, outputDesc.height};

	LightingConstants constants = {};

	if (m_viewMode == ViewMode::LightOverdrawBruteForce)
	{
		constants.debugMode = 1;
	}
	else if (m_viewMode == ViewMode::LightOverdraw)
	{
		constants.debugMode = 2;
	}
	else if (m_viewMode == ViewMode::VisitedLights)
	{
		constants.debugMode = 3;
	}
	else if (m_viewMode == ViewMode::LightTreeVisitedNodes)
	{
		constants.debugMode = 4;
	}

	constants.matView        = m_matView.transposed();
	constants.matProjInv     = m_matProj.inverse().transposed();
	constants.offsetX        = 0;
	constants.offsetY        = 0;
	constants.tileSize       = m_tileSize;
	constants.outputWidth    = outputDesc.width;
	constants.outputHeight   = outputDesc.height;
	constants.ambientLight   = Vec4(m_ambientTint * m_ambientIntensity);
	constants.flipClipSpaceY = (!!(caps.projectionFlags & ProjectionFlags::FlipVertical)) ? 0 : 1;
	constants.constantOne    = 1;
	constants.useShallowTree = m_tiledLightTreeBuilderParams.useShallowTree;

	m_tiledLightTreeBuilderParams.resolution              = outputResolution;
	m_tiledLightTreeBuilderParams.tileSize                = m_tileSize;
	m_tiledLightTreeBuilderParams.calculateTileLightCount = m_drawTileGrid;
	m_tiledLightTreeBuilderParams.useTileFrustumCulling   = !!m_useTileFrustumCulling;
	m_clusteredLightBuilderParams.calculateTileLightCount = m_drawTileGrid;
	m_clusteredLightBuilderParams.useTileFrustumCulling   = !!m_useTileFrustumCulling;

	if (m_lightingMode == LightingMode::Hybrid)
	{
		auto buildParams           = m_tiledLightTreeBuilderParams;
		buildParams.useShallowTree = false;

		m_tiledLightTreeBuildResult =
		    m_tiledLightTreeBuilder->build(ctx, m_currentCamera, m_viewSpaceLights, buildParams);
		const auto& stats = m_tiledLightTreeBuildResult;

		m_stats.cpuLightBuildTotal.add(stats.buildTotalTime);
		m_stats.cpuLightUpload.add(stats.uploadTime);
		m_stats.cpuLightCull.add(stats.lightCullTime);
		m_stats.cpuLightAssign.add(stats.lightAssignTime);
		m_stats.cpuLightSort.add(stats.lightSortTime);
		m_stats.cpuLightBuildTree.add(stats.buildTreeTime);

		m_visibleLightCount = stats.visibleLightCount;

		constants.lightCount = (u32)m_tiledLightTreeBuilder->m_gpuLights.size();
		constants.nodeCount  = (u32)m_tiledLightTreeBuilder->m_gpuLightTree.size();

		constants.lightGridSliceCount = stats.sliceCount;
		constants.lightGridDepthMin   = 0.0f;
		constants.lightGridDepthMax   = stats.hierarchicalCullingDepthThreshold;
		constants.lightGridExtents    = constants.lightGridDepthMax - constants.lightGridDepthMin;

		constants.useExponentialSlices = m_tiledLightTreeBuilderParams.useExponentialSlices;

		Gfx_UpdateBufferT(ctx, m_lightingConstantBuffer, constants);
	}
	else if (m_lightingMode == LightingMode::Tree)
	{
		auto buildParams           = m_tiledLightTreeBuilderParams;
		buildParams.sliceCount     = 1; // sincle depth slice contining light trees
		buildParams.maxSliceDepth  = 0;
		buildParams.useShallowTree = m_tiledLightTreeBuilderParams.useShallowTree;

#if USE_GPU_BUILDER
		if (m_useGpuLightTreeBuilder)
		{
			m_tiledLightTreeBuildResult = m_tiledLightTreeBuilderGPU->build(ctx, m_currentCamera, m_shaderLibrary,
			    m_lightSourceBuffer.get(), u32(m_viewSpaceLights.size()), buildParams);
			constants.lightCount        = 0;
			constants.nodeCount         = 0;
		}
		else
#endif
		{
			m_tiledLightTreeBuildResult =
			    m_tiledLightTreeBuilder->build(ctx, m_currentCamera, m_viewSpaceLights, buildParams);
			constants.lightCount = (u32)m_tiledLightTreeBuilder->m_gpuLights.size();
			constants.nodeCount  = (u32)m_tiledLightTreeBuilder->m_gpuLightTree.size();
		}

		const auto& stats = m_tiledLightTreeBuildResult;

		m_stats.cpuLightBuildTotal.add(stats.buildTotalTime);
		m_stats.cpuLightUpload.add(stats.uploadTime);
		m_stats.cpuLightCull.add(stats.lightCullTime);
		m_stats.cpuLightAssign.add(stats.lightAssignTime);
		m_stats.cpuLightSort.add(stats.lightSortTime);
		m_stats.cpuLightBuildTree.add(stats.buildTreeTime);

		m_visibleLightCount = stats.visibleLightCount;

		constants.lightGridSliceCount = stats.sliceCount;
		constants.lightGridDepthMin   = 0.0f;
		constants.lightGridDepthMax   = stats.hierarchicalCullingDepthThreshold;
		constants.lightGridExtents    = constants.lightGridDepthMax - constants.lightGridDepthMin;

		constants.useExponentialSlices = m_tiledLightTreeBuilderParams.useExponentialSlices;

		Gfx_UpdateBufferT(ctx, m_lightingConstantBuffer, constants);
	}
	else if (m_lightingMode == LightingMode::Clustered)
	{
		ClusteredLightBuilder::BuildParams buildParams = m_clusteredLightBuilderParams;
		buildParams.resolution                         = outputResolution;
		buildParams.tileSize                           = m_tileSize;

		m_clusteredLightBuildResult =
		    m_clusteredLightBuilder->build(ctx, m_currentCamera, m_viewSpaceLights, buildParams);
		const auto& stats = m_clusteredLightBuildResult;

		m_stats.cpuLightBuildTotal.add(stats.buildTotalTime);
		m_stats.cpuLightUpload.add(stats.uploadTime);
		m_stats.cpuLightAssign.add(stats.lightAssignTime);
		m_stats.cpuLightCull.add(stats.lightCullTime);

		m_visibleLightCount = stats.visibleLightCount;

		constants.lightCount          = u32(m_viewSpaceLights.size());
		constants.nodeCount           = 0;
		constants.lightGridSliceCount = m_clusteredLightBuilderParams.sliceCount;
		constants.lightGridDepthMin   = 0.0f;
		constants.lightGridDepthMax   = m_clusteredLightBuilderParams.maxSliceDepth;
		constants.lightGridExtents    = constants.lightGridDepthMax - constants.lightGridDepthMin;

		constants.useExponentialSlices = m_clusteredLightBuilderParams.useExponentialSlices;

		Gfx_UpdateBufferT(ctx, m_lightingConstantBuffer, constants);
	}
	else
	{
		Log::error("Unexpected lighting mode");
	}

	m_stats.cpuLightTotal.add(timer.time());
}

void LightCullApp::applyLighting()
{
	GfxMarkerScope markerScope(m_ctx, "applyLighting");

	const GfxCapability& caps = Gfx_GetCapability();

	const GfxTextureDesc& outputDesc = Gfx_GetTextureDesc(m_finalFrame);

	// submit data to gpu

	Gfx_SetConstantBuffer(m_ctx, 0, m_lightingConstantBuffer);
	Gfx_SetSampler(m_ctx, GfxStage::Compute, 0, m_samplerStates.linearClamp);
	Gfx_SetTexture(m_ctx, GfxStage::Compute, 0, m_gbufferBaseColor);
	Gfx_SetTexture(m_ctx, GfxStage::Compute, 1, m_gbufferNormal);
	Gfx_SetTexture(m_ctx, GfxStage::Compute, 2, m_gbufferRoughness);
	Gfx_SetTexture(m_ctx, GfxStage::Compute, 3, m_gbufferDepth);
	Gfx_SetTexture(m_ctx, GfxStage::Compute, 4, m_falseColorTexture.get());
	Gfx_SetStorageImage(m_ctx, 0, m_finalFrame);

	const u32 debugVisualizationEnabled = m_viewMode == ViewMode::Final ? 0 : 1;

	if (m_lightingMode == LightingMode::Hybrid)
	{
		Gfx_SetTechnique(m_ctx, m_techniqueHybridTiledLightTreeShading[debugVisualizationEnabled]);

		u32 bufferIndex = 0;
		Gfx_SetStorageBuffer(m_ctx, bufferIndex++, m_lightSourceBuffer);
		Gfx_SetStorageBuffer(m_ctx, bufferIndex++, m_tiledLightTreeBuildResult.lightTreeBuffer);
		Gfx_SetStorageBuffer(m_ctx, bufferIndex++, m_tiledLightTreeBuildResult.lightTileInfoBuffer);
		Gfx_SetStorageBuffer(m_ctx, bufferIndex++, m_tiledLightTreeBuildResult.lightIndexBuffer);

		Gfx_SetStorageBuffer(m_ctx, bufferIndex++, m_tileStatsBuffer);

		// TODO: query thread group size from shader or configure it through specialization constants
		u32 dispatchWidth  = divUp(outputDesc.width, m_threadGroupSizeHybridTiledLightTreeShading.x);
		u32 dispatchHeight = divUp(outputDesc.height, m_threadGroupSizeHybridTiledLightTreeShading.y);

		Gfx_Dispatch(m_ctx, dispatchWidth, dispatchHeight, 1);
	}
	else if (m_lightingMode == LightingMode::Tree)
	{
		u32 dispatchWidth;
		u32 dispatchHeight;

		if (m_tiledLightTreeBuilderParams.useShallowTree)
		{
			dispatchWidth  = divUp(outputDesc.width, m_threadGroupSizeTiledLightTreeShadingMasked.x);
			dispatchHeight = divUp(outputDesc.height, m_threadGroupSizeTiledLightTreeShadingMasked.y);

			Gfx_SetTechnique(m_ctx, m_techniqueTiledLightTreeShadingMasked[debugVisualizationEnabled]);
		}
		else
		{
			dispatchWidth  = divUp(outputDesc.width, m_threadGroupSizeTiledLightTreeShading.x);
			dispatchHeight = divUp(outputDesc.height, m_threadGroupSizeTiledLightTreeShading.y);

			Gfx_SetTechnique(m_ctx, m_techniqueTiledLightTreeShading[debugVisualizationEnabled]);
		}

		Gfx_SetStorageBuffer(m_ctx, 0, m_lightSourceBuffer);
		Gfx_SetStorageBuffer(m_ctx, 1, m_tiledLightTreeBuildResult.lightTreeBuffer);
		Gfx_SetStorageBuffer(m_ctx, 2, m_tiledLightTreeBuildResult.lightTileInfoBuffer);
		Gfx_SetStorageBuffer(m_ctx, 3, m_tiledLightTreeBuildResult.lightIndexBuffer);

		Gfx_Dispatch(m_ctx, dispatchWidth, dispatchHeight, 1);
	}
	else if (m_lightingMode == LightingMode::Clustered)
	{
		Gfx_SetTechnique(m_ctx, m_techniqueClusteredShading[debugVisualizationEnabled]);

		Gfx_SetStorageBuffer(m_ctx, 0, m_lightSourceBuffer);
		Gfx_SetStorageBuffer(m_ctx, 1, m_clusteredLightBuildResult.lightGridBuffer);
		Gfx_SetStorageBuffer(m_ctx, 2, m_clusteredLightBuildResult.lightIndexBuffer);

		// TODO: query thread group size from shader or configure it through specialization constants
		u32 dispatchWidth  = divUp(outputDesc.width, m_threadGroupSizeClusteredShading.x);
		u32 dispatchHeight = divUp(outputDesc.height, m_threadGroupSizeClusteredShading.y);

		Gfx_Dispatch(m_ctx, dispatchWidth, dispatchHeight, 1);
	}
	else
	{
		Log::error("Unexpected lighting mode");
	}
}

void LightCullApp::generateTileStats()
{
	const GfxTextureDesc& outputDesc = Gfx_GetTextureDesc(m_finalFrame);

	Gfx_SetTechnique(m_ctx, m_techniqueTileStatsGenerate);

	Gfx_SetConstantBuffer(m_ctx, 0, m_lightingConstantBuffer);

	Gfx_SetSampler(m_ctx, GfxStage::Compute, 0, m_samplerStates.linearClamp);

	Gfx_SetTexture(m_ctx, GfxStage::Compute, 0, m_gbufferBaseColor);
	Gfx_SetTexture(m_ctx, GfxStage::Compute, 1, m_gbufferNormal);
	Gfx_SetTexture(m_ctx, GfxStage::Compute, 2, m_gbufferRoughness);
	Gfx_SetTexture(m_ctx, GfxStage::Compute, 3, m_gbufferDepth);
	Gfx_SetStorageImage(m_ctx, 0, m_finalFrame);

	Gfx_SetStorageBuffer(m_ctx, 0, m_lightSourceBuffer);
	Gfx_SetStorageBuffer(m_ctx, 1, m_tiledLightTreeBuildResult.lightTreeBuffer);
	Gfx_SetStorageBuffer(m_ctx, 2, m_tiledLightTreeBuildResult.lightTileInfoBuffer);
	Gfx_SetStorageBuffer(m_ctx, 3, m_tileStatsBuffer);

	// TODO: query thread group size from shader or configure it through specialization constants
	u32 dispatchWidth  = divUp(outputDesc.width, 8);
	u32 dispatchHeight = divUp(outputDesc.height, 8);

	Gfx_Dispatch(m_ctx, dispatchWidth, dispatchHeight, 1);

	// reduce the stats to a single integer counter

	Gfx_SetTechnique(m_ctx, m_techniqueTileStatsReduce);
	Gfx_SetStorageBuffer(m_ctx, 0, m_tileStatsBuffer);
	Gfx_SetStorageBuffer(m_ctx, 1, m_tileStatsReducedBuffer);

	const u32 inputCount = dispatchWidth * dispatchHeight;
	Gfx_Dispatch(m_ctx, 1, 1, 1, &inputCount, sizeof(inputCount));
}

void LightCullApp::drawTileStats()
{
	const GfxTextureDesc& outputDesc = Gfx_GetTextureDesc(m_finalFrame);

	Gfx_SetTechnique(m_ctx, m_techniqueTileStatsDisplay);

	Gfx_SetStorageImage(m_ctx, 0, m_finalFrame);
	Gfx_SetStorageBuffer(m_ctx, 0, m_tileStatsBuffer);

	u32 dispatchWidth  = divUp(outputDesc.width, 8);
	u32 dispatchHeight = divUp(outputDesc.height, 8);

	struct
	{
		u32 data[2];
	} pushConstants = {m_tileSize, outputDesc.width};

	Gfx_Dispatch(m_ctx, dispatchWidth, dispatchHeight, 1, &pushConstants, sizeof(pushConstants));
}

void LightCullApp::draw()
{
	const GfxCapability& caps = Gfx_GetCapability();

	Gfx_SetPresentInterval(m_enableVsync ? 1 : 0);

	// note: async compute context is kicked off before any other rendering
	GfxContext* lightingBuildContext = m_useAsyncCompute ? Gfx_BeginAsyncCompute(m_ctx) : m_ctx;

	// Draw to off-screen render targets
	Gfx_BeginTimer(m_ctx, Timestamp_Gbuffer);
	if (m_useStaticGbuffer)
	{
		transcodeGbuffer();
	}
	else
	{
		drawGbuffer();
	}
	Gfx_EndTimer(m_ctx, Timestamp_Gbuffer);

	// Lighting using compute shader

	{
		Gfx_BeginTimer(lightingBuildContext, Timestamp_LightingBuild);
		buildLighting(lightingBuildContext);
		Gfx_EndTimer(lightingBuildContext, Timestamp_LightingBuild);

		if (m_useAsyncCompute)
		{
			Gfx_EndAsyncCompute(m_ctx, lightingBuildContext);
		}

		Gfx_BeginTimer(m_ctx, Timestamp_Lighting);
		applyLighting();
		Gfx_EndTimer(m_ctx, Timestamp_Lighting);
	}

	if (isBenchmarkState(m_state) || m_drawTileStats)
	{
		generateTileStats();
	}

	if (m_drawTileStats)
	{
		drawTileStats();
	}

	Gfx_AddImageBarrier(m_ctx, m_finalFrame, GfxResourceState_ShaderRead);

	// Draw to back buffer
	GfxPassDesc passDesc2D;
	passDesc2D.flags    = GfxPassFlags::ClearColor;
	passDesc2D.color[0] = Gfx_GetBackBufferColorTexture();
	Gfx_BeginPass(m_ctx, passDesc2D);

	Gfx_SetDepthStencilState(m_ctx, m_depthStencilStates.disable);
	Gfx_SetBlendState(m_ctx, m_blendStates.lerp);

	Box2 windowRect;
	windowRect.m_min = Vec2(0.0f);
	windowRect.m_max = m_window->getSizeFloat();

	m_prim->begin2D(m_window->getSize());
	switch (m_viewMode)
	{
	case ViewMode::BaseColor: m_prim->setTexture(m_gbufferBaseColor.get()); break;
	case ViewMode::Normal: m_prim->setTexture(m_gbufferNormal.get()); break;
	case ViewMode::Roughness: m_prim->setTexture(m_gbufferRoughness.get()); break;
	case ViewMode::Depth: m_prim->setTexture(m_gbufferDepth.get()); break;
	case ViewMode::Final:
	case ViewMode::LightOverdrawBruteForce:
	case ViewMode::LightOverdraw:
	case ViewMode::VisitedLights:
	case ViewMode::LightTreeVisitedNodes: m_prim->setTexture(m_finalFrame.get()); break;
	}
	m_prim->drawTexturedQuad(windowRect);
	m_prim->end2D();

	Gfx_EndPass(m_ctx);

	GfxPassDesc passDesc3D;
	passDesc3D.color[0] = Gfx_GetBackBufferColorTexture();
	passDesc3D.depth    = m_gbufferDepth.get();
	Gfx_BeginPass(m_ctx, passDesc3D);

	if (!isBenchmarkState(m_state))
	{
		m_prim->begin3D(m_matView * m_matProj);
		// m_prim->drawBox(m_worldBoundingBox);

		if (m_drawDebugSpheres)
		{
			// Draw lights as 3D spheres
			drawDebugSpheres();
		}

		if (m_drawLightMarkers && m_lightCount)
		{
			m_prim->flush();

			if (m_depthTestMarkers)
			{
				Gfx_SetDepthStencilState(m_ctx, m_depthStencilStates.testLessEqual);
			}
			else
			{
				Gfx_SetDepthStencilState(m_ctx, m_depthStencilStates.disable);
			}

			Gfx_SetBlendState(m_ctx, m_blendStates.additive);

			if (m_lightMarkerTexture.valid())
			{
				float      s    = 0.5f;
				const Vec3 dirX = s * m_currentCamera.getRight();
				const Vec3 dirY = s * m_currentCamera.getUp();

				m_prim->setSampler(PrimitiveBatch::SamplerState_Linear);
				m_prim->setTexture(m_lightMarkerTexture.get());

				TexturedQuad3D quad;
				quad.tex[0] = Vec2(0.0f, 0.0f);
				quad.tex[1] = Vec2(1.0f, 0.0f);
				quad.tex[2] = Vec2(1.0f, 1.0f);
				quad.tex[3] = Vec2(0.0f, 1.0f);

				for (int i = 0; i < m_lightCount; ++i)
				{
					const auto& light    = m_lights[i];
					Vec3        lightPos = light.position;

					quad.pos[0] = lightPos - dirX + dirY;
					quad.pos[1] = lightPos + dirX + dirY;
					quad.pos[2] = lightPos + dirX - dirY;
					quad.pos[3] = lightPos - dirX - dirY;

					ColorRGBA color = ColorRGBA(light.intensity * 2.0f);
					m_prim->drawTexturedQuad(&quad, color);
				}
			}
			else
			{
				for (int i = 0; i < m_lightCount; ++i)
				{
					const auto& light = m_lights[i];
					m_prim->drawCross(light.position, 0.1f);
				}
			}

			m_prim->flush();

			Gfx_SetDepthStencilState(m_ctx, m_depthStencilStates.disable);
			Gfx_SetBlendState(m_ctx, m_blendStates.lerp);
		}

		if (m_drawDebugFrustum)
		{
			drawDebugFrustum();
		}

		m_prim->end3D();

		m_prim->begin2D(m_window->getSize());
		if (m_drawTileGrid)
		{
			const GfxTextureDesc& outputDesc = Gfx_GetTextureDesc(m_finalFrame);

			m_prim->drawRect(
			    Box2(Vec2(0.0), Vec2((float)outputDesc.width, (float)outputDesc.height)), ColorRGBA::Black(0.75f));

			u32       w     = divUp(outputDesc.width, m_tileSize);
			u32       h     = divUp(outputDesc.height, m_tileSize);
			ColorRGBA color = ColorRGBA::White(0.25f);
			for (u32 x = 0; x < w; ++x)
			{
				m_prim->drawLine(
				    Vec2((float)x * m_tileSize, 0), Vec2((float)x * m_tileSize, (float)outputDesc.height), color);
			}
			for (u32 y = 0; y < h; ++y)
			{
				m_prim->drawLine(
				    Vec2(0, (float)y * m_tileSize), Vec2((float)outputDesc.width, (float)y * m_tileSize), color);
			}

			m_prim->flush();

			Gfx_SetBlendState(m_ctx, m_blendStates.additive);
			if (m_drawLightMarkers)
			{
				if (m_lightingMode == LightingMode::Hybrid || m_lightingMode == LightingMode::Tree)
				{
					for (u32 lightIndex : m_tiledLightTreeBuilder->m_visibleLightIndices)
					{
						const auto& it = m_tiledLightTreeBuilder->m_lightScreenSpaceExtents[lightIndex];
						m_prim->drawRect(it.screenSpaceBox, ColorRGBA8(8, 4, 2, 255));
					}
				}
				else if (m_lightingMode == LightingMode::Clustered)
				{
					for (u32 lightIndex : m_clusteredLightBuilder->m_visibleLightIndices)
					{
						const auto& it = m_clusteredLightBuilder->m_lightScreenSpaceExtents[lightIndex];
						m_prim->drawRect(it.screenSpaceBox, ColorRGBA8(8, 4, 2, 255));
					}
				}
			}

			m_prim->flush();
			Gfx_SetBlendState(m_ctx, m_blendStates.additive);

			char tempString[1024];
			for (u32 y = 0; y < h; ++y)
			{
				for (u32 x = 0; x < w; ++x)
				{
					Vec2 textPos   = Vec2((float)x * m_tileSize + 2, (float)y * m_tileSize + 2);
					u32  tileIndex = x + y * w;
					if (m_lightingMode == LightingMode::Hybrid ||
					    (m_lightingMode == LightingMode::Tree && !m_useGpuLightTreeBuilder))
					{
						u32 count = m_tiledLightTreeBuilder->m_tileLightCount[tileIndex];
						snprintf(tempString, RUSH_COUNTOF(tempString), "%d", count);
						m_font->draw(m_prim, textPos, tempString, ColorRGBA8::White(), false);
					}
					else if (m_lightingMode == LightingMode::Clustered)
					{
						u32 count = m_clusteredLightBuilder->m_tileLightCount[tileIndex];
						snprintf(tempString, RUSH_COUNTOF(tempString), "%d", count);
						m_font->draw(m_prim, textPos, tempString, ColorRGBA8::White(), false);
					}
				}
			}
		}

		m_prim->end2D();

		ImGui::Render();
	}

	Gfx_EndPass(m_ctx);

#if USE_FFMPEG
	if (!m_captureVideoPath.empty() && m_frameCount != 0 && m_ffmpegPipe)
	{
		Gfx_RequestScreenshot(videoCaptureCallback, this);
	}
#endif // USE_FFMPEG

	++m_frameCount;
}

GfxRef<GfxTexture> LightCullApp::loadTextureFromMemory(const void* data, Tuple2i size, GfxFormat format, bool convertToRGBA8)
{
	GfxRef<GfxTexture> texture;

	if (convertToRGBA8 || format == GfxFormat_RGBA8_Unorm)
	{
		std::vector<ColorRGBA8> rgbaData(size.x * size.y);

		if (format == GfxFormat_RGB8_Unorm)
		{
			const Tuple3<u8>* rgbData = reinterpret_cast<const Tuple3<u8>*>(data);
			for (size_t i = 0; i < rgbaData.size(); ++i)
			{
				rgbaData[i].r = rgbData[i].x;
				rgbaData[i].g = rgbData[i].y;
				rgbaData[i].b = rgbData[i].z;
				rgbaData[i].a = 255;
			}
		}
		else if (format == GfxFormat_R8_Unorm)
		{
			const u8* rData = reinterpret_cast<const u8*>(data);
			for (size_t i = 0; i < rgbaData.size(); ++i)
			{
				rgbaData[i].r = rData[i];
				rgbaData[i].g = rData[i];
				rgbaData[i].b = rData[i];
				rgbaData[i].a = 255;
			}
		}
		else
		{
			Log::error("Unsupported format");
		}

		GfxTextureData textureData = makeTextureData(rgbaData.data(), 0);

		GfxTextureDesc desc = GfxTextureDesc::make2D(size.x, size.y, GfxFormat_RGBA8_Unorm);
		desc.mips           = 1;

		texture.retain(Gfx_CreateTexture(desc, &textureData, 1));
	}
	else
	{
		GfxTextureData textureData = makeTextureData(data, 0);

		GfxTextureDesc desc = GfxTextureDesc::make2D(size.x, size.y, format);
		desc.mips           = 1;

		texture.retain(Gfx_CreateTexture(desc, &textureData, 1));
	}

	return texture;
}

GfxRef<GfxTexture> LightCullApp::loadTextureFromFile(const std::string& filename)
{
	std::string filenameLower = filename;
	std::transform(filenameLower.begin(), filenameLower.end(), filenameLower.begin(), ::tolower);

	auto it = m_textures.find(filenameLower);
	if (it == m_textures.end())
	{
		Log::message("Loading texture '%s'", filenameLower.c_str());

		GfxRef<GfxTexture> texture;

		if (endsWith(filenameLower, ".dds"))
		{
			texture.retain(loadDDS(filenameLower.c_str()));
		}
		else if (endsWith(filenameLower, ".tga") || endsWith(filenameLower, ".png"))
		{
			texture.retain(loadBitmap(filenameLower.c_str()));
		}

		m_textures.insert(std::make_pair(filenameLower, texture));

		return texture;
	}
	else
	{
		return it->second;
	}
}

void LightCullApp::finishProfilingExperiment()
{
	Log::message("Average GPU lighting time: %.2f ms", m_stats.gpuLighting.getAverage() * 1000.0f);
}

void LightCullApp::resetStats() { m_stats = Stats(); }

#if USE_FFMPEG
void LightCullApp::videoCaptureCallback(const ColorRGBA8* pixels, Tuple2u size, void* userData)
{
	reinterpret_cast<LightCullApp*>(userData)->videoCaptureCallback(pixels, size);
}

void LightCullApp::videoCaptureCallback(const ColorRGBA8* pixels, Tuple2u size)
{
	if (m_ffmpegPipe)
	{
		fwrite(pixels, sizeof(ColorRGBA8) * size.x * size.y, 1, m_ffmpegPipe);
	}
}
#endif // USE_FFMPEG

void LightCullApp::drawDebugSpheres()
{
	Gfx_SetDepthStencilState(m_ctx, m_depthStencilStates.disable);
	Gfx_SetRasterizerState(m_ctx, m_rasterizerStateNoCull);
	Gfx_SetBlendState(m_ctx, m_blendStates.additive);

	Gfx_SetTechnique(m_ctx, m_techniqueGbuffer);
	Gfx_SetVertexStream(m_ctx, 0, m_debugSphereMesh.m_vertexBuffer);
	Gfx_SetIndexStream(m_ctx, m_debugSphereMesh.m_indexBuffer);
	Gfx_SetConstantBuffer(m_ctx, 0, m_sceneConstantBuffer);
	Gfx_SetConstantBuffer(m_ctx, 1, m_instanceConstantBuffer);

	InstanceConstants instanceConstants;
	instanceConstants.baseColor = Vec4(0.1f);

	Gfx_SetSampler(m_ctx, GfxStage::Pixel, 0, m_samplerAniso8);

	Gfx_SetTexture(m_ctx, GfxStage::Pixel, 0, m_defaultAlbedoTexture);
	Gfx_SetTexture(m_ctx, GfxStage::Pixel, 1, m_defaultNormalTexture);
	Gfx_SetTexture(m_ctx, GfxStage::Pixel, 2, m_defaultRoughnessTexture);

	instanceConstants.useNormalMap = 0;

	for (int i = 0; i < m_lightCount; ++i)
	{
		const auto& light          = m_lights[i];
		instanceConstants.matWorld = Mat4::scaleTranslate(Vec3(light.attenuationEnd), light.position).transposed();

		Gfx_UpdateBuffer(m_ctx, m_instanceConstantBuffer, &instanceConstants, sizeof(instanceConstants));
		Gfx_DrawIndexed(m_ctx, m_debugSphereMesh.m_indexCount, 0, 0, m_debugSphereMesh.m_vertexCount);
	}

	Gfx_SetDepthStencilState(m_ctx, m_depthStencilStates.testLessEqual);
	Gfx_SetRasterizerState(m_ctx, m_rasterizerState);
	Gfx_SetBlendState(m_ctx, m_blendStates.lerp);
}

void LightCullApp::captureCameraFrustum()
{
	Mat4    matViewProj = m_matView * m_matProj;
	Frustum frustum(matViewProj);

	frustum.getNearPlanePoints(m_debugFrustumPoints);
	frustum.getFarPlanePoints(m_debugFrustumPoints + 4);
}

void LightCullApp::captureTileFrustum()
{
	const GfxTextureDesc& outputDesc = Gfx_GetTextureDesc(m_finalFrame);
	u32                   tileCountX = divUp(outputDesc.width, m_tileSize);
	u32                   tileCountY = divUp(outputDesc.height, m_tileSize);

	const float yHeight                    = tan(m_currentCamera.getFov() / 2) * 2;
	const float xWidth                     = yHeight * m_currentCamera.getAspect();
	const Vec2  cameraFrustumTopLeftCorner = Vec2(-xWidth / 2, yHeight / 2);
	const float tileFrustumStepSize        = xWidth * float(m_tileSize) / float(outputDesc.width);
	const Vec2  tileStep                   = Vec2(tileFrustumStepSize, -tileFrustumStepSize);

	Vec2 pos = Vec2(float(m_debugTileId.x), float(m_debugTileId.y));

	Vec2 corners[4];
	corners[0] = cameraFrustumTopLeftCorner + (pos + Vec2(0, 0)) * tileStep;
	corners[1] = cameraFrustumTopLeftCorner + (pos + Vec2(1, 0)) * tileStep;
	corners[2] = cameraFrustumTopLeftCorner + (pos + Vec2(0, 1)) * tileStep;
	corners[3] = cameraFrustumTopLeftCorner + (pos + Vec2(1, 1)) * tileStep;

	float nearZ = m_currentCamera.getNearPlane();
	float farZ  = nearZ + 100.0f; // limit to something sensible

	Mat4 viewInverse = m_matView.inverse();

	for (u32 i = 0; i < 4; ++i)
	{
		Vec3 ray                    = normalize(Vec3(corners[i].x, corners[i].y, 1.0f));
		Vec3 n                      = transformPoint(viewInverse, ray * nearZ / ray.z);
		Vec3 f                      = transformPoint(viewInverse, ray * farZ / ray.z);
		m_debugFrustumPoints[i]     = n;
		m_debugFrustumPoints[i + 4] = f;
	}
}

void LightCullApp::drawDebugFrustum()
{
	enum
	{
		NTL,
		NTR,
		NBL,
		NBR,
		FTL,
		FTR,
		FBL,
		FBR
	};

	ColorRGBA8 colors[5] = {
	    ColorRGBA8(255, 255, 255),
	    ColorRGBA8(255, 200, 200),
	    ColorRGBA8(200, 255, 200),
	    ColorRGBA8(200, 200, 255),
	    ColorRGBA8(255, 200, 255),
	};

	m_prim->drawLine(m_debugFrustumPoints[NTL], m_debugFrustumPoints[NTR], colors[0]);
	m_prim->drawLine(m_debugFrustumPoints[NBL], m_debugFrustumPoints[NBR], colors[0]);
	m_prim->drawLine(m_debugFrustumPoints[NTL], m_debugFrustumPoints[NBL], colors[0]);
	m_prim->drawLine(m_debugFrustumPoints[NTR], m_debugFrustumPoints[NBR], colors[0]);

	m_prim->drawLine(m_debugFrustumPoints[NTL], m_debugFrustumPoints[FTL], colors[1]);
	m_prim->drawLine(m_debugFrustumPoints[NTR], m_debugFrustumPoints[FTR], colors[2]);
	m_prim->drawLine(m_debugFrustumPoints[NBL], m_debugFrustumPoints[FBL], colors[3]);
	m_prim->drawLine(m_debugFrustumPoints[NBR], m_debugFrustumPoints[FBR], colors[4]);

	// m_prim->drawLine(m_debugFrustumPoints[FTL], m_debugFrustumPoints[FTR], color);
	// m_prim->drawLine(m_debugFrustumPoints[FBL], m_debugFrustumPoints[FBR], color);
	// m_prim->drawLine(m_debugFrustumPoints[FTL], m_debugFrustumPoints[FBL], color);
	// m_prim->drawLine(m_debugFrustumPoints[FTR], m_debugFrustumPoints[FBR], color);
}

void FIBER_CALL LightCullApp::scriptFiber(void* userPtr)
{
	LightCullApp* app  = reinterpret_cast<LightCullApp*>(userPtr);
	app->m_enableVsync = false;

	runCommandsFromJsonFile(app->m_scriptName.c_str(), app);
	app->m_state = State::Idle;

#if USE_FFMPEG
	if (app->m_ffmpegPipe)
	{
		_pclose(app->m_ffmpegPipe);
		app->m_ffmpegPipe = nullptr;
	}
#endif // USE_FFMPEG

	app->m_enableVsync = true;
	switchToFiber(app->m_mainFiber);
}

void LightCullApp::generateDebugMeshes()
{
	std::vector<ModelVertex> vertices;
	std::vector<u32>         indices;

	par_shapes_mesh* mesh = par_shapes_create_subdivided_sphere(3);
	par_shapes_compute_normals(mesh);

	for (int i = 0; i < mesh->npoints; ++i)
	{
		ModelVertex v = {};
		v.position    = Vec3(&mesh->points[i * 3]);
		if (mesh->normals)
		{
			v.normal = Vec3(&mesh->normals[i * 3]);
		}
		if (mesh->tcoords)
		{
			v.texcoord = Vec2(&mesh->tcoords[i * 2]);
		}
		vertices.push_back(v);
	}

	for (int i = 0; i < mesh->ntriangles; ++i)
	{
		indices.push_back(mesh->triangles[i * 3 + 0]);
		indices.push_back(mesh->triangles[i * 3 + 1]);
		indices.push_back(mesh->triangles[i * 3 + 2]);
	}

	par_shapes_free_mesh(mesh);

	m_debugSphereMesh.m_vertexCount = u32(vertices.size());
	m_debugSphereMesh.m_indexCount  = u32(indices.size());

	GfxBufferDesc vbDesc(
	    GfxBufferFlags::Vertex, m_debugSphereMesh.m_vertexCount, sizeof(ModelVertex));
	m_debugSphereMesh.m_vertexBuffer = Gfx_CreateBuffer(vbDesc, vertices.data());

	GfxBufferDesc ibDesc(
	    GfxBufferFlags::Index, GfxFormat_R32_Uint, m_debugSphereMesh.m_indexCount, 4);
	m_debugSphereMesh.m_indexBuffer = Gfx_CreateBuffer(ibDesc, indices.data());
}

void LightCullApp::generateMeshes()
{
	std::vector<ModelVertex> vertices;
	std::vector<u32>         indices;

	m_worldBoundingBox.expandInit();

	auto makeMaterial = [&](const Vec3 baseColor) {
		Material material;
		material.baseColor        = Vec4(baseColor, 1.0f);
		material.albedoTexture.retain(m_defaultAlbedoTexture);
		material.normalTexture.retain(m_defaultNormalTexture);
		material.roughnessTexture.retain(m_defaultRoughnessTexture);
		return material;
	};

	auto makeRandomizedMaterial = [&]() {
		Vec3 baseColor;
		baseColor.x = m_rng.getFloat(0.1f, 0.9f);
		baseColor.y = m_rng.getFloat(0.1f, 0.9f);
		baseColor.z = m_rng.getFloat(0.1f, 0.9f);
		return makeMaterial(baseColor);
	};

	auto addMesh = [&](par_shapes_mesh* mesh, const Material& material) {
		u32 firstVertex = (u32)vertices.size();

		ModelSegment segment;
		segment.material    = (u32)m_materials.size();
		segment.indexCount  = mesh->ntriangles * 3;
		segment.indexOffset = (u32)indices.size();
		m_segments.push_back(segment);
		m_materials.push_back(material);

		for (int i = 0; i < mesh->npoints; ++i)
		{
			ModelVertex v = {};
			v.position    = Vec3(&mesh->points[i * 3]);
			if (mesh->normals)
			{
				v.normal = Vec3(&mesh->normals[i * 3]);
			}
			if (mesh->tcoords)
			{
				v.texcoord = Vec2(&mesh->tcoords[i * 2]);
			}
			vertices.push_back(v);

			m_worldBoundingBox.expand(v.position);
		}

		for (int i = 0; i < mesh->ntriangles; ++i)
		{
			indices.push_back(firstVertex + mesh->triangles[i * 3 + 0]);
			indices.push_back(firstVertex + mesh->triangles[i * 3 + 1]);
			indices.push_back(firstVertex + mesh->triangles[i * 3 + 2]);
		}
	};

	{
		par_shapes_mesh* mesh = par_shapes_create_subdivided_sphere(2);
		par_shapes_translate(mesh, 0.0f, 1.0f, 0.0f);
		par_shapes_compute_normals(mesh);
		addMesh(mesh, makeRandomizedMaterial());
		par_shapes_free_mesh(mesh);
	}

	for (u32 i = 0; i < 32; ++i)
	{
		par_shapes_mesh* mesh = par_shapes_create_rock(i, 2);
		par_shapes_translate(mesh, m_rng.getFloat(-64.0f, 64.0f), 0.0f, m_rng.getFloat(-64.0f, 64.0f));
		addMesh(mesh, makeRandomizedMaterial());
		par_shapes_free_mesh(mesh);
	}

	{
		par_shapes_mesh* mesh = par_shapes_create_plane(1, 1);
		par_shapes_compute_normals(mesh);
		float axis[3] = {1, 0, 0};
		par_shapes_rotate(mesh, -Pi / 2.0f, axis);
		par_shapes_translate(mesh, -0.5f, 0.0f, 0.5f);
		par_shapes_scale(mesh, 128.0f, 1.0f, 128.0f);
		addMesh(mesh, makeMaterial(Vec3(0.75f)));
		par_shapes_free_mesh(mesh);
	}

	for (u32 i = 0; i < 32; ++i)
	{
		par_shapes_mesh* mesh = par_shapes_create_trefoil_knot(20, 100, 1.0f);
		par_shapes_scale(mesh, 5.0f, 5.0f, 5.0f);
		par_shapes_translate(mesh, m_rng.getFloat(-64.0f, 64.0f), 5.0f, m_rng.getFloat(-64.0f, 64.0f));
		addMesh(mesh, makeRandomizedMaterial());
		par_shapes_free_mesh(mesh);
	}

	for (u32 i = 0; i < 32; ++i)
	{
		par_shapes_mesh* mesh    = par_shapes_create_cylinder(16, 2);
		float            axis[3] = {1, 0, 0};
		par_shapes_rotate(mesh, -Pi / 2.0f, axis);
		par_shapes_scale(mesh, 1.0f, m_rng.getFloat(5.0f, 15.0f), 1.0f);
		par_shapes_translate(mesh, m_rng.getFloat(-64.0f, 64.0f), 0.0f, m_rng.getFloat(-64.0f, 64.0f));
		addMesh(mesh, makeRandomizedMaterial());
		par_shapes_free_mesh(mesh);
	}

	m_vertexCount = (u32)vertices.size();
	m_indexCount  = (u32)indices.size();

	GfxBufferDesc vbDesc(GfxBufferFlags::Vertex, m_vertexCount, sizeof(ModelVertex));
	m_vertexBuffer = Gfx_CreateBuffer(vbDesc, vertices.data());

	GfxBufferDesc ibDesc(GfxBufferFlags::Index, GfxFormat_R32_Uint, m_indexCount, 4);
	m_indexBuffer = Gfx_CreateBuffer(ibDesc, indices.data());

	m_lightAnimationBounds.m_min = Vec3(-64, 5.0f, -64);
	m_lightAnimationBounds.m_max = Vec3(64, 1.0f, 64);
}

const char* toString(ViewMode mode)
{
	switch (mode)
	{
	default: return "Unknown";
	case ViewMode::BaseColor: return "BaseColor";
	case ViewMode::Normal: return "Normal";
	case ViewMode::Roughness: return "Roughness";
	case ViewMode::Depth: return "Depth";
	case ViewMode::Final: return "Final";
	case ViewMode::LightOverdraw: return "LightOverdraw";
	case ViewMode::VisitedLights: return "VisitedLights";
	case ViewMode::LightOverdrawBruteForce: return "LightOverdrawBruteForce";
	case ViewMode::LightTreeVisitedNodes: return "LightTreeVisitedNodes";
	}
}

void LightCullApp::createShaders()
{
	const char* shaderDirectory = Platform_GetExecutableDirectory();

	auto shaderFromFile = [&](const char* filename, GfxShaderType type) {
		std::string fullFilename = std::string(shaderDirectory) + "/" + std::string(filename);
		Log::message("Loading shader '%s'", filename);

		GfxShaderSource source;
		source.type = GfxShaderSourceType_SPV;

		FileIn f(fullFilename.c_str());
		if (f.valid())
		{
			u32 fileSize = f.length();
			source.resize(fileSize);
			f.read(&source[0], fileSize);
		}

		if (source.empty())
		{
			Log::error("Failed to load shader '%s'", filename);
		}

		return source;
	};

	{
		auto vs = Gfx_CreateVertexShader(shaderFromFile("Shaders/Model.vert.spv", GfxShaderType::Vertex));
		auto ps = Gfx_CreatePixelShader(shaderFromFile("Shaders/Model.frag.spv", GfxShaderType::Pixel));

		GfxVertexFormatDesc vfDesc;
		vfDesc.add(0, GfxVertexFormatDesc::DataType::Float3, GfxVertexFormatDesc::Semantic::Position, 0);
		vfDesc.add(0, GfxVertexFormatDesc::DataType::Float3, GfxVertexFormatDesc::Semantic::Normal, 0);
		vfDesc.add(0, GfxVertexFormatDesc::DataType::Float3, GfxVertexFormatDesc::Semantic::Tangent, 0);
		vfDesc.add(0, GfxVertexFormatDesc::DataType::Float3, GfxVertexFormatDesc::Semantic::Bitangent, 0);
		vfDesc.add(0, GfxVertexFormatDesc::DataType::Float2, GfxVertexFormatDesc::Semantic::Texcoord, 0);

		auto vf = Gfx_CreateVertexFormat(vfDesc);

		ShaderBingingsBuilder bindings;
		u32                   bindingIndex = 0;
		bindings.addConstantBuffer("SceneConstants", bindingIndex++);
		bindings.addConstantBuffer("InstanceConstants", bindingIndex++);
		bindings.addSampler("defaultSampler", bindingIndex++);
		bindings.addTexture("s_albedo", bindingIndex++);
		bindings.addTexture("s_normal", bindingIndex++);
		bindings.addTexture("s_roughness", bindingIndex++);

		GfxTechniqueDesc desc(ps, vs, vf, bindings.desc);
		desc.psWaveLimit = 0.25f;

		m_techniqueGbuffer = Gfx_CreateTechnique(desc);
	}

	{
		auto vs = Gfx_CreateVertexShader(shaderFromFile("Shaders/FullScreenTriangle.vert.spv", GfxShaderType::Vertex));
		auto ps = Gfx_CreatePixelShader(shaderFromFile("Shaders/GbufferTranscode.frag.spv", GfxShaderType::Pixel));

		GfxVertexFormatDesc vfDesc;
		auto                vf = Gfx_CreateVertexFormat(vfDesc);

		ShaderBingingsBuilder bindings;
		u32               bindingIndex = 0;
		bindings.addSampler("defaultSampler", bindingIndex++);
		bindings.addTexture("s_depth", bindingIndex++);
		bindings.addTexture("s_albedo", bindingIndex++);
		bindings.addTexture("s_normal", bindingIndex++);
		bindings.addTexture("s_roughness", bindingIndex++);
		m_techniqueGbufferTranscode = Gfx_CreateTechnique(GfxTechniqueDesc(ps, vs, vf, bindings.desc));
	}

	for (u32 debugVisualizationEnabled = 0; debugVisualizationEnabled < 2; ++debugVisualizationEnabled)
	{
		const GfxSpecializationConstant specializationConstants[] = {
		    {0, 0, 4}, // enable debug visualization
		    {1, 4, 4}, // thread group size X
		    {2, 8, 4}, // thread group size Y
		};

		struct SpecializationData
		{
			u32 debugVisualizationEnabled;
			u32 threadGroupSizeX;
			u32 threadGroupSizeY;
		};

		{
			auto cs = Gfx_CreateComputeShader(
			    shaderFromFile("Shaders/TiledLightTreeShadingHybrid.comp.spv", GfxShaderType::Compute));

			ShaderBingingsBuilder bindings;
			u32                   bindingIndex = 0;
			bindings.addConstantBuffer("LightingConstants", bindingIndex++);
			bindings.addSampler("defaultSampler", bindingIndex++);
			bindings.addTexture("gbufferBaseColorImage", bindingIndex++);
			bindings.addTexture("gbufferNormalImage", bindingIndex++);
			bindings.addTexture("gbufferRoughnessImage", bindingIndex++);
			bindings.addTexture("gbufferDepthImage", bindingIndex++);
			bindings.addTexture("falseColorImage", bindingIndex++);
			bindings.addStorageImage("outputImage", bindingIndex++);
			bindings.addStorageBuffer("LightBuffer", bindingIndex++);
			bindings.addStorageBuffer("LightTreeBuffer", bindingIndex++);
			bindings.addStorageBuffer("LightTileInfoBuffer", bindingIndex++);
			bindings.addTypedRWBuffer("LightIndexBuffer", bindingIndex++);

			SpecializationData specializationData;
			specializationData.debugVisualizationEnabled = debugVisualizationEnabled;
			specializationData.threadGroupSizeX          = m_threadGroupSizeHybridTiledLightTreeShading.x;
			specializationData.threadGroupSizeY          = m_threadGroupSizeHybridTiledLightTreeShading.y;

			GfxTechniqueDesc desc(cs, bindings.desc, m_threadGroupSizeHybridTiledLightTreeShading);
			desc.specializationConstants     = specializationConstants;
			desc.specializationConstantCount = RUSH_COUNTOF(specializationConstants);
			desc.specializationData          = &specializationData;
			desc.specializationDataSize      = sizeof(specializationData);

			m_techniqueHybridTiledLightTreeShading[debugVisualizationEnabled] = Gfx_CreateTechnique(desc);
		}

		{
			ShaderBingingsBuilder bindings;
			u32                   bindingIndex = 0;
			bindings.addConstantBuffer("LightingConstants", bindingIndex++);
			bindings.addSampler("defaultSampler", bindingIndex++);
			bindings.addTexture("gbufferBaseColorImage", bindingIndex++);
			bindings.addTexture("gbufferNormalImage", bindingIndex++);
			bindings.addTexture("gbufferRoughnessImage", bindingIndex++);
			bindings.addTexture("gbufferDepthImage", bindingIndex++);
			bindings.addTexture("falseColorImage", bindingIndex++);
			bindings.addStorageImage("outputImage", bindingIndex++);
			bindings.addStorageBuffer("LightBuffer", bindingIndex++);
			bindings.addStorageBuffer("LightTreeBuffer", bindingIndex++);
			bindings.addStorageBuffer("LightTileInfoBuffer", bindingIndex++);
			bindings.addTypedRWBuffer("LightIndexBuffer", bindingIndex++);

			{
				auto cs = Gfx_CreateComputeShader(
				    shaderFromFile("Shaders/TiledLightTreeShading.comp.spv", GfxShaderType::Compute));

				SpecializationData specializationData;
				specializationData.debugVisualizationEnabled = debugVisualizationEnabled;
				specializationData.threadGroupSizeX          = m_threadGroupSizeTiledLightTreeShading.x;
				specializationData.threadGroupSizeY          = m_threadGroupSizeTiledLightTreeShading.y;

				GfxTechniqueDesc desc(cs, bindings.desc, m_threadGroupSizeTiledLightTreeShading);
				desc.specializationConstants     = specializationConstants;
				desc.specializationConstantCount = RUSH_COUNTOF(specializationConstants);
				desc.specializationData          = &specializationData;
				desc.specializationDataSize      = sizeof(specializationData);
				m_techniqueTiledLightTreeShading[debugVisualizationEnabled] = Gfx_CreateTechnique(desc);
			}

			const GfxCapability& caps = Gfx_GetCapability();
			if (caps.shaderWaveIntrinsics && caps.shaderInt64)
			{
				auto cs = Gfx_CreateComputeShader(
				    shaderFromFile("Shaders/TiledLightTreeShadingMasked.comp.spv", GfxShaderType::Compute));

				SpecializationData specializationData;
				specializationData.debugVisualizationEnabled = debugVisualizationEnabled;
				specializationData.threadGroupSizeX          = m_threadGroupSizeTiledLightTreeShadingMasked.x;
				specializationData.threadGroupSizeY          = m_threadGroupSizeTiledLightTreeShadingMasked.y;

				GfxTechniqueDesc desc(cs, bindings.desc, m_threadGroupSizeTiledLightTreeShadingMasked);
				desc.specializationConstants     = specializationConstants;
				desc.specializationConstantCount = RUSH_COUNTOF(specializationConstants);
				desc.specializationData          = &specializationData;
				desc.specializationDataSize      = sizeof(specializationData);
				m_techniqueTiledLightTreeShadingMasked[debugVisualizationEnabled] = Gfx_CreateTechnique(desc);
			}
		}

		{
			auto cs =
			    Gfx_CreateComputeShader(shaderFromFile("Shaders/ClusteredShading.comp.spv", GfxShaderType::Compute));

			ShaderBingingsBuilder bindings;
			u32                   bindingIndex = 0;
			bindings.addConstantBuffer("LightingConstants", bindingIndex++);
			bindings.addSampler("defaultSampler", bindingIndex++);
			bindings.addTexture("gbufferBaseColorImage", bindingIndex++);
			bindings.addTexture("gbufferNormalImage", bindingIndex++);
			bindings.addTexture("gbufferRoughnessImage", bindingIndex++);
			bindings.addTexture("gbufferDepthImage", bindingIndex++);
			bindings.addTexture("falseColorImage", bindingIndex++);
			bindings.addStorageImage("outputImage", bindingIndex++);
			bindings.addStorageBuffer("LightBuffer", bindingIndex++);
			bindings.addStorageBuffer("LightTileInfoBuffer", bindingIndex++);
			bindings.addTypedRWBuffer("LightIndexBuffer", bindingIndex++);

			SpecializationData specializationData;
			specializationData.debugVisualizationEnabled = debugVisualizationEnabled;
			specializationData.threadGroupSizeX          = m_threadGroupSizeClusteredShading.x;
			specializationData.threadGroupSizeY          = m_threadGroupSizeClusteredShading.y;

			GfxTechniqueDesc desc(cs, bindings.desc, m_threadGroupSizeClusteredShading);
			desc.specializationConstants     = specializationConstants;
			desc.specializationConstantCount = RUSH_COUNTOF(specializationConstants);
			desc.specializationData          = &specializationData;
			desc.specializationDataSize      = sizeof(specializationData);
			m_techniqueClusteredShading[debugVisualizationEnabled] = Gfx_CreateTechnique(desc);
		}
	}

	{
		auto cs = Gfx_CreateComputeShader(shaderFromFile("Shaders/TileStatsDisplay.comp.spv", GfxShaderType::Compute));

		ShaderBingingsBuilder bindings;
		u32                   bindingIndex = 0;
		bindings.addPushConstants("PushConstants", GfxStageFlags::Compute, 2 * sizeof(u32));
		bindings.addStorageImage("outputImage", bindingIndex++);
		bindings.addStorageBuffer("TileStatsBuffer", bindingIndex++);
		m_techniqueTileStatsDisplay = Gfx_CreateTechnique(GfxTechniqueDesc(cs, bindings.desc, {8,8,1}));
	}

	{
		auto cs = Gfx_CreateComputeShader(shaderFromFile("Shaders/TileStatsReduce.comp.spv", GfxShaderType::Compute));

		ShaderBingingsBuilder bindings;
		u32                   bindingIndex = 0;
		bindings.addPushConstants("PushConstants", GfxStageFlags::Compute, sizeof(u32));
		bindings.addStorageBuffer("Input", bindingIndex++);
		bindings.addStorageBuffer("Output", bindingIndex++);
		m_techniqueTileStatsReduce = Gfx_CreateTechnique(GfxTechniqueDesc(cs, bindings.desc, {1024,1,1}));
	}

	{
		auto cs = Gfx_CreateComputeShader(shaderFromFile("Shaders/TileStatsGenerate.comp.spv", GfxShaderType::Compute));

		ShaderBingingsBuilder bindings;
		u32                   bindingIndex = 0;
		bindings.addConstantBuffer("LightingConstants", bindingIndex++);
		bindings.addTexture("gbufferBaseColorImage", bindingIndex++);
		bindings.addTexture("gbufferNormalImage", bindingIndex++);
		bindings.addTexture("gbufferRoughnessImage", bindingIndex++);
		bindings.addTexture("gbufferDepthImage", bindingIndex++);
		bindings.addTexture("falseColorRamp", bindingIndex++);
		bindings.addStorageImage("outputImage", bindingIndex++);
		bindings.addStorageBuffer("LightBuffer", bindingIndex++);
		bindings.addStorageBuffer("LightTreeBuffer", bindingIndex++);
		bindings.addStorageBuffer("LightTileInfoBuffer", bindingIndex++);
		bindings.addStorageBuffer("TileStatsBuffer", bindingIndex++);
		m_techniqueTileStatsGenerate = Gfx_CreateTechnique(GfxTechniqueDesc(cs, bindings.desc, {8,8,1}));
	}
}

void LightCullApp::createGbuffer(Tuple2i size)
{
	GfxTextureDesc desc;
	desc.type   = TextureType::Tex2D;
	desc.width  = size.x;
	desc.height = size.y;
	desc.depth  = 1;
	desc.mips   = 1;

	desc.format = GfxFormat_RGBA8_Unorm;
	desc.usage  = GfxUsageFlags::RenderTarget | GfxUsageFlags::ShaderResource;
	m_gbufferBaseColor = Gfx_CreateTexture(desc);

	desc.format = GfxFormat_RGBA16_Float;
	desc.usage  = GfxUsageFlags::RenderTarget | GfxUsageFlags::ShaderResource;
	m_gbufferNormal = Gfx_CreateTexture(desc);

	desc.format = GfxFormat_R8_Unorm;
	desc.usage  = GfxUsageFlags::RenderTarget | GfxUsageFlags::ShaderResource;
	m_gbufferRoughness = Gfx_CreateTexture(desc);

	desc.format = GfxFormat_D32_Float;
	desc.usage  = GfxUsageFlags::DepthStencil | GfxUsageFlags::ShaderResource;
	m_gbufferDepth = Gfx_CreateTexture(desc);

	desc.format = GfxFormat_RGBA16_Float;
	desc.usage  = GfxUsageFlags::StorageImage | GfxUsageFlags::ShaderResource;
	m_finalFrame = Gfx_CreateTexture(desc);

	{
		u32 dispatchWidth  = divUp(size.x, 8);
		u32 dispatchHeight = divUp(size.y, 8);

		GfxBufferDesc bufferDesc;
		bufferDesc.flags  = GfxBufferFlags::Storage;
		bufferDesc.format = GfxFormat_R32_Uint;
		bufferDesc.stride = 4;
		bufferDesc.count  = dispatchWidth * dispatchHeight;

		m_tileStatsBuffer = Gfx_CreateBuffer(bufferDesc);
	}

	{
		GfxBufferDesc bufferDesc;
		bufferDesc.flags       = GfxBufferFlags::Storage;
		bufferDesc.format      = GfxFormat_R32_Uint;
		bufferDesc.stride      = 4;
		bufferDesc.count       = 1;
		bufferDesc.hostVisible = true;

		m_tileStatsReducedBuffer = Gfx_CreateBuffer(bufferDesc);

		m_tileStatsReducedMappedBuffer = Gfx_MapBuffer(m_tileStatsReducedBuffer.get());
	}

	// Clear all gbuffer components on next render
	m_gbufferClearFlags = GfxPassFlags::ClearAll;
}

template <typename T> void LightCullApp::ImGuiEnumCombo(const char* label, T* value)
{
	ImGui::Combo(label, reinterpret_cast<int*>(value),
	    [](void*, int idx, const char** outText) {
		    *outText = toString((T)idx);
		    return true;
	    },
	    nullptr, (int)T::count);
}

void LightCullApp::updateGui(float dt)
{
	ImGuiImpl_Update(dt);

	if (!m_enableGui)
		return;

	const GfxStats& stats = Gfx_Stats();

	ImGui::Begin("Menu [F1]");

	if (ImGui::CollapsingHeader("Stats", nullptr, true, true))
	{
		ImGui::Text("Camera position : %.2f %.2f %.2f",
		    m_pendingCamera.getPosition().x,
		    m_pendingCamera.getPosition().y,
		    m_pendingCamera.getPosition().z);

		ImGui::Text("World size: %.2f %.2f %.2f",
		    m_worldBoundingBox.dimensions().x,
		    m_worldBoundingBox.dimensions().y,
		    m_worldBoundingBox.dimensions().z);

		ImGui::Text("Draw calls: %d", stats.drawCalls);
		ImGui::Text("Vertices: %d", stats.vertices);
		ImGui::Text("Lights: %d", m_visibleLightCount);

		if (m_lightingMode == LightingMode::Hybrid || m_lightingMode == LightingMode::Tree)
		{
			ImGui::Text("Light data size: %.2f KB", m_tiledLightTreeBuildResult.lightDataSize / 1024.0f);
			ImGui::Text("Light tree size: %.2f KB", m_tiledLightTreeBuildResult.treeDataSize / 1024.0f);
			ImGui::Text("Total size: %.2f KB",
			    (m_tiledLightTreeBuildResult.lightDataSize + m_tiledLightTreeBuildResult.treeDataSize) / 1024.0f);
		}
		else if (m_lightingMode == LightingMode::Clustered)
		{
			ImGui::Text("Light data size: %.2f KB", m_clusteredLightBuilder->m_lightDataSize / 1024.0f);
			ImGui::Text("Light grid size: %.2f KB", m_clusteredLightBuilder->m_lightGridSize / 1024.0f);
			ImGui::Text("Total size: %.2f KB",
			    (m_clusteredLightBuilder->m_lightDataSize + m_clusteredLightBuilder->m_lightGridSize) / 1024.0f);
		}

		const ImVec4 highlightColor(1.0f, 1.0f, 0.5f, 1.0f);

		ImGui::Text("GPU gbuffer time: %.2f ms", m_stats.gpuGbuffer.getAverage() * 1000.0f);
		ImGui::TextColored(highlightColor, "GPU lighting time: %.2f ms", m_stats.gpuLighting.getAverage() * 1000.0f);
		ImGui::Text("GPU lighting build time: %.2f ms", m_stats.gpuLightingBuild.getAverage() * 1000.0f);
		ImGui::Text("GPU total frame time: %.2f ms", m_stats.gpuTotal.getAverage() * 1000.0f);

		ImGui::Text("CPU light cull: %.2f ms (%.2f%%)", m_stats.cpuLightCull.getAverage() * 1000.0f,
		    100.0f * m_stats.cpuLightCull.getAverage() / m_stats.cpuLightBuildTotal.getAverage());
		ImGui::SameLine();
		ImGui::Text("Min: %.2f ms", 1000.0 * m_stats.cpuLightCull.getMin());

		ImGui::Text("CPU light assign: %.2f ms (%.2f%%)", m_stats.cpuLightAssign.getAverage() * 1000.0f,
		    100.0f * m_stats.cpuLightAssign.getAverage() / m_stats.cpuLightBuildTotal.getAverage());
		ImGui::SameLine();
		ImGui::Text("Min: %.2f ms", 1000.0 * m_stats.cpuLightAssign.getMin());

		if (m_lightingMode == LightingMode::Hybrid || m_lightingMode == LightingMode::Tree)
		{
			ImGui::Text("CPU light sort: %.2f ms (%.2f%%)", m_stats.cpuLightSort.getAverage() * 1000.0f,
			    100.0f * m_stats.cpuLightSort.getAverage() / m_stats.cpuLightBuildTotal.getAverage());
			ImGui::Text("CPU build tree: %.2f ms (%.2f%%)", m_stats.cpuLightBuildTree.getAverage() * 1000.0f,
			    100.0f * m_stats.cpuLightBuildTree.getAverage() / m_stats.cpuLightBuildTotal.getAverage());
			ImGui::SameLine();
			ImGui::Text("Min: %.2f ms", 1000.0 * m_stats.cpuLightBuildTree.getMin());
		}

		ImGui::Text("CPU light upload: %.2f ms (%.2f%%)", m_stats.cpuLightUpload.getAverage() * 1000.0f,
		    100.0f * m_stats.cpuLightUpload.getAverage() / m_stats.cpuLightBuildTotal.getAverage());
		ImGui::Text("CPU light build total: %.2f ms", m_stats.cpuLightBuildTotal.getAverage() * 1000.0f);
		ImGui::SameLine();
		ImGui::Text("Min: %.2f ms", 1000.0 * m_stats.cpuLightBuildTotal.getMin());

		ImGui::TextColored(highlightColor, "CPU light total: %.2f ms", m_stats.cpuLightTotal.getAverage() * 1000.0f);
		ImGui::Text("CPU frame time: %.2f ms", m_stats.cpuFrameTime.getAverage() * 1000.0f);
		ImGui::Text("Wait for GPU time: %.2f ms", m_stats.waitForGpuTime.getAverage() * 1000.0f);

		if (ImGui::Button("Reset stats"))
		{
			m_stats = Stats();
		}
	}

	if (ImGui::CollapsingHeader("Settings", nullptr, true, true))
	{
		bool settingsChanges = false;

		ImGui::Checkbox("VSync", &m_enableVsync);
		ImGui::Checkbox("Draw tile grid [G]", &m_drawTileGrid);
		ImGui::Checkbox("Draw light markers [M]", &m_drawLightMarkers);
		ImGui::Checkbox("Depth test markers [N]", &m_depthTestMarkers);

		ImGui::ColorEdit3("Ambient tint", m_ambientTint.begin());
		ImGui::SliderFloat("Ambient intensity", &m_ambientIntensity, 0.0f, 1.0f);
		settingsChanges |= ImGui::SliderInt("Light count", &m_lightCount, 0, MaxLights);

		ImGuiEnumCombo("Lighting mode [C]", &m_lightingMode);
		ImGuiEnumCombo("View mode [V]", &m_viewMode);

		const u32   tileSizes[]       = {8, 16, 32, 48, 64, 80, 96, 112, 128, 256, 512};
		const char* tileSizeStrings[] = {"8", "16", "32", "48", "64", "80", "96", "112", "128", "256", "512"};
		static_assert(RUSH_COUNTOF(tileSizes) == RUSH_COUNTOF(tileSizeStrings),
		    "Mismatch between array of sizes and corresponding strings");

		int tileSize = 0;
		for (u32 i = 0; i < RUSH_COUNTOF(tileSizes); ++i)
		{
			if (tileSizes[i] == m_tileSize)
			{
				tileSize = i;
				break;
			}
		}

		settingsChanges |= ImGui::Combo("TileSize", &tileSize, tileSizeStrings, RUSH_COUNTOF(tileSizeStrings));
		if (tileSizes[tileSize] != m_tileSize)
		{
			processCommand(CmdSetTileSize{tileSizes[tileSize]});
		}

		const int maxSliceCount = 256;

		if (m_lightingMode == LightingMode::Hybrid)
		{
			settingsChanges |=
			    ImGui::Checkbox("Exponential slices", &m_tiledLightTreeBuilderParams.useExponentialSlices);
			settingsChanges |=
			    ImGui::SliderInt("Slice count", (int*)&m_tiledLightTreeBuilderParams.sliceCount, 1, maxSliceCount);
			settingsChanges |=
			    ImGui::SliderFloat("Slice max depth", &m_tiledLightTreeBuilderParams.maxSliceDepth, 1, 500);
		}

		if (m_lightingMode == LightingMode::Hybrid || m_lightingMode == LightingMode::Tree)
		{
			settingsChanges |=
			    ImGui::SliderInt("Target lights per leaf", &m_tiledLightTreeBuilderParams.targetLightsPerLeaf, 1, 32);
		}

		if (m_lightingMode == LightingMode::Hybrid)
		{
			settingsChanges |= ImGui::SliderFloat(
			    "Light tree heuristic", &m_tiledLightTreeBuilderParams.lightTreeHeuristic, 0.0f, 4.0f);
			settingsChanges |=
			    ImGui::Checkbox("Clip light extents", &m_tiledLightTreeBuilderParams.useClippedLightExtents);
		}

		if (m_lightingMode == LightingMode::Clustered)
		{
			settingsChanges |=
			    ImGui::Checkbox("Exponential slices", &m_clusteredLightBuilderParams.useExponentialSlices);
			settingsChanges |=
			    ImGui::SliderInt("Slice count", (int*)&m_clusteredLightBuilderParams.sliceCount, 1, maxSliceCount);
			settingsChanges |=
			    ImGui::SliderFloat("Slice max depth", &m_clusteredLightBuilderParams.maxSliceDepth, 1, 500);
		}

		if (m_lightingMode == LightingMode::Tree)
		{
#if USE_GPU_BUILDER
			settingsChanges |= ImGui::Checkbox("GPU light tree builder", &m_useGpuLightTreeBuilder);
			if (Gfx_GetCapability().asyncCompute)
			{
				settingsChanges |= ImGui::Checkbox("GPU async compute", &m_useAsyncCompute);
			}
#endif
			if (m_techniqueTiledLightTreeShadingMasked[0].valid())
			{
				settingsChanges |= ImGui::Checkbox("Use shallow tree", &m_tiledLightTreeBuilderParams.useShallowTree);
			}
		}

		const char* tileFrustumCullingStrings[] = {"None", "Fast, inaccurate", "Accurate"};
		int         tileFrustumCullingMode      = m_useTileFrustumCulling;
		settingsChanges |= ImGui::Combo("Use tile frustum culling", &tileFrustumCullingMode, tileFrustumCullingStrings,
		    RUSH_COUNTOF(tileFrustumCullingStrings));
		m_useTileFrustumCulling = tileFrustumCullingMode;

		if (settingsChanges)
		{
			resetStats();
		}
	}

	if (ImGui::CollapsingHeader("Replay", nullptr, true, false))
	{
		if (m_replayState == ReplayState::Idle)
		{
			if (ImGui::Button("Play"))
			{
				startReplay();
			}

			ImGui::SameLine();

			if (ImGui::Button("Record"))
			{
				m_replayState = ReplayState::Record;
				m_replayCameraFrames.clear();
			}
		}
		else
		{
			if (ImGui::Button("Stop"))
			{
				m_replayState = ReplayState::Idle;
			}
		}

		if (ImGui::Button("Save"))
		{
			saveReplay();
		}

		ImGui::SameLine();

		if (ImGui::Button("Load"))
		{
			loadReplay();
			m_replayFrame = 0;
		}

		if (ImGui::Button("Save benchmark"))
		{
			saveReplayBenchmarkCSV();
		}

		if (m_replayState == ReplayState::Record)
		{
			ImGui::Text("Frame %d", (u32)m_replayCameraFrames.size());
		}
		else
		{
			ImGui::Text("Frame %d / %d", m_replayFrame, (u32)m_replayCameraFrames.size());
		}
	}

	if (ImGui::CollapsingHeader("Graphs", nullptr, true, false))
	{
		static constexpr size_t sampleCount = RUSH_COUNTOF(m_stats.gpuLighting.m_buffer);
		float                   values[sampleCount];
		for (int i = 0; i < sampleCount; ++i)
		{
			values[i] = (float)m_stats.gpuLighting.m_buffer[i] * 1000.0f;
		}

		ImGui::PlotHistogram("GPU Lighting\n0 - 6 ms", values, sampleCount, 0, nullptr, 0.0f, 6.0f, ImVec2(0, 150));
	}

	if (ImGui::CollapsingHeader("Camera", nullptr, true, false))
	{
		ImGui::SliderFloat("Position smoothing", &m_cameraPositionSmoothing, 0.0f, 1.0f);
		ImGui::SliderFloat("Orientation smoothing", &m_cameraOrientationSmoothing, 0.0f, 1.0f);
		ImGui::SliderFloat("FOV", &m_cameraFov, 0.1f, 3.0f);
	}

	if (ImGui::CollapsingHeader("Light generation", nullptr, true, true))
	{
		ImGui::PushID("LightGeneration");

		ImGui::SliderInt("Light count", &m_pendingLightParams.count, 1, MaxLights);

		m_pendingLightParams.minIntensity = min(m_pendingLightParams.maxIntensity, m_pendingLightParams.minIntensity);
		ImGui::SliderFloat("Min intensity", &m_pendingLightParams.minIntensity, 0.0f, 10.0f);

		m_pendingLightParams.maxIntensity = max(m_pendingLightParams.maxIntensity, m_pendingLightParams.minIntensity);
		ImGui::SliderFloat("Max intensity", &m_pendingLightParams.maxIntensity, 0.0f, 10.0f);

		if (m_model && ImGui::Button("Generate on geometry"))
		{
			generateLightsOnGeometry(
			    m_pendingLightParams.count, m_pendingLightParams.minIntensity, m_pendingLightParams.maxIntensity);
		}

		if (ImGui::Button("Generate in air"))
		{
			generateLights(
			    m_pendingLightParams.count, m_pendingLightParams.minIntensity, m_pendingLightParams.maxIntensity);
		}

		ImGui::PopID();
	}

	if (ImGui::CollapsingHeader("Debug tools", nullptr, true, true))
	{
		ImGui::PushID("DebugTools");

		ImGui::Checkbox("Draw debug frustum", &m_drawDebugFrustum);
		if (ImGui::Button("Capture camera frustum"))
		{
			captureCameraFrustum();
			m_drawDebugFrustum = true;
		}

		const GfxTextureDesc& outputDesc = Gfx_GetTextureDesc(m_finalFrame);
		u32                   tileCountX = divUp(outputDesc.width, m_tileSize);
		u32                   tileCountY = divUp(outputDesc.height, m_tileSize);

		if (ImGui::DragInt2("Debug tile", &m_debugTileId.x, 0.1f, 0, max(tileCountX, tileCountY)))
		{
			if (m_drawDebugFrustum)
			{
				captureTileFrustum();
			}
		}
		if (ImGui::Button("Capture tile frustum"))
		{
			captureTileFrustum();
			m_drawDebugFrustum = true;
		}

		ImGui::PopID();
	}

	ImGui::End();
}

inline Vec3 generateDirection(Rand& rng)
{
	float u1 = rng.getFloat(0.0f, 1.0f);
	float u2 = rng.getFloat(0.0f, 1.0f);

	float z   = 1.0f - 2.0f * u1;
	float r   = sqrtf(max(0.0f, 1.0f - z * z));
	float phi = TwoPi * u2;

	float x = r * cosf(phi);
	float y = r * sinf(phi);

	return Vec3(x, y, z);
}

void LightCullApp::generateLights(u32 count, float minIntensity, float maxIntensity)
{
	Rand m_rng = Rand(m_randomSeed);

	m_lights.clear();

	float minLightRadius = FLT_MAX;
	float maxLightRadius = -FLT_MAX;
	float avgLightRadius = 0;

	const u32 lightCount = min<u32>(count, MaxLights);

	for (u32 i = 0; i < lightCount; ++i)
	{
		AnimatedLightSource light = {};

		light.position.x = m_rng.getFloat(m_lightAnimationBounds.m_min.x, m_lightAnimationBounds.m_max.x);
		light.position.y = m_rng.getFloat(m_lightAnimationBounds.m_min.y, m_lightAnimationBounds.m_max.y);
		light.position.z = m_rng.getFloat(m_lightAnimationBounds.m_min.z, m_lightAnimationBounds.m_max.z);

		light.originalPosition = light.position;

		light.movementDirection   = generateDirection(m_rng);
		light.movementDirection.x = abs(light.movementDirection.x);
		light.movementDirection.y = abs(light.movementDirection.y);
		light.movementDirection.z = abs(light.movementDirection.z);

		light.intensity.x = m_rng.getFloat(minIntensity, maxIntensity);
		light.intensity.y = m_rng.getFloat(minIntensity, maxIntensity);
		light.intensity.z = m_rng.getFloat(minIntensity, maxIntensity);

		const float intensity                     = dot(light.intensity, Vec3(1.0f / 3.0f));
		const float attenuationIntensityThreshold = 0.025f;
		light.attenuationEnd                      = intensity / sqrtf(attenuationIntensityThreshold);

		minLightRadius = min(minLightRadius, light.attenuationEnd);
		maxLightRadius = max(maxLightRadius, light.attenuationEnd);
		avgLightRadius += light.attenuationEnd;

		m_lights.push_back(light);
	}

	avgLightRadius /= lightCount;

	Log::message("Light radius range: %f .. %f", minLightRadius, maxLightRadius);
	Log::message("Average light radius: %f", avgLightRadius);

	animateLights(m_lightAnimationTime, 10000); // 10k is to preserve the behavior before bugfix

	m_currentLightParams.count        = count;
	m_currentLightParams.minIntensity = minIntensity;
	m_currentLightParams.maxIntensity = maxIntensity;

	m_pendingLightParams = m_currentLightParams;
}

inline Vec3 computeTriangleNormal(Vec3 a, Vec3 b, Vec3 c) { return normalize(cross(c - a, c - b)); }

inline float computeTriangleArea(Vec3 a, Vec3 b, Vec3 c) { return length(cross(c - a, c - b)) * 0.5f; }

void LightCullApp::generateLightsOnGeometry(u32 count, float minIntensity, float maxIntensity)
{
	Rand m_rng = Rand(m_randomSeed);

	m_lights.clear();

	float minLightRadius = FLT_MAX;
	float maxLightRadius = -FLT_MAX;
	float avgLightRadius = 0;

	std::vector<float> triangleAreas;
	std::vector<Vec3>  triangleNormals;

	u32 triangleCount = (u32)m_model->indices.size() / 3;

	if (triangleCount == 0)
		return;

	triangleAreas.resize(triangleCount);
	triangleNormals.resize(triangleCount);

	struct Triangle
	{
		Vec3 a, b, c;
	};

	auto getTriangle = [&](u32 triangleIndex) {
		Triangle result;

		u32 indices[3] = {m_model->indices[triangleIndex * 3 + 0], m_model->indices[triangleIndex * 3 + 1],
		    m_model->indices[triangleIndex * 3 + 2]};

		result.a = m_model->vertices[indices[0]].position;
		result.b = m_model->vertices[indices[1]].position;
		result.c = m_model->vertices[indices[2]].position;

		return result;
	};

	parallelFor<u32>(0, triangleCount, [&](u32 triangleIndex) {
		Triangle tri                   = getTriangle(triangleIndex);
		float    area                  = computeTriangleArea(tri.a, tri.b, tri.c);
		Vec3     normal                = computeTriangleNormal(tri.a, tri.b, tri.c);
		triangleAreas[triangleIndex]   = area;
		triangleNormals[triangleIndex] = normal;
	});

	float triangleAreaSum = 0.0;
	for (u32 i = 0; i < triangleCount; ++i)
	{
		triangleAreaSum += triangleAreas[i];
	}

	DiscreteDistribution<float> distribution(triangleAreas.data(), triangleCount, triangleAreaSum);

	m_lights.reserve(count);

	const u32 lightCount = min<u32>(count, MaxLights);

	for (u32 i = 0; i < lightCount; ++i)
	{
		u32 triangleIndex = (u32)distribution(m_rng.rand(), m_rng.getFloat(0.0f, 1.0f));

		if (triangleAreas[triangleIndex] == 0.0f)
			continue;

		Triangle tri = getTriangle(triangleIndex);

		AnimatedLightSource light = {};

		light.originalPosition  = light.position;
		light.movementDirection = Vec3(0.0);

		light.intensity.x = m_rng.getFloat(minIntensity, maxIntensity);
		light.intensity.y = m_rng.getFloat(minIntensity, maxIntensity);
		light.intensity.z = m_rng.getFloat(minIntensity, maxIntensity);

		const float intensity                     = dot(light.intensity, Vec3(1.0f / 3.0f));
		const float attenuationIntensityThreshold = 0.025f;
		light.attenuationEnd                      = intensity / sqrtf(attenuationIntensityThreshold);

		float u, v;
		for (;;)
		{
			u = m_rng.getFloat(0.0f, 1.0f);
			v = m_rng.getFloat(0.0f, 1.0f);
			if (u + v <= 1)
				break;
		}
		float w        = 1.0f - u - v;
		light.position = tri.a * u + tri.b * v + tri.c * w;
		light.position += triangleNormals[triangleIndex] * light.attenuationEnd * 0.25f;

		minLightRadius = min(minLightRadius, light.attenuationEnd);
		maxLightRadius = max(maxLightRadius, light.attenuationEnd);
		avgLightRadius += light.attenuationEnd;

		m_lights.push_back(light);
	}

	m_currentLightParams.count        = count;
	m_currentLightParams.minIntensity = minIntensity;
	m_currentLightParams.maxIntensity = maxIntensity;

	m_pendingLightParams = m_currentLightParams;

	avgLightRadius /= lightCount;

	Log::message("Light radius range: %f .. %f", minLightRadius, maxLightRadius);
	Log::message("Average light radius: %f", avgLightRadius);
}

inline float frac(float value) { return value - floorf(value); }

void LightCullApp::animateLights(float elapsedTime, u32 lightCount)
{
	Vec3 worldSize   = m_lightAnimationBounds.dimensions();
	Vec3 worldOffset = m_lightAnimationBounds.m_min;

	Vec3 animationSpeed = Vec3(10.0f, 0.01f, 10.0f);

	for (u32 i = 0; i < lightCount; ++i)
	{
		auto& light = m_lights[i];

		if (light.movementDirection == Vec3(0.0f))
			continue;

		Vec3 o = (light.originalPosition - worldOffset) / worldSize;
		Vec3 p = o + (light.movementDirection * (elapsedTime * animationSpeed)) / worldSize;

		int mx = (int)p.x % 2;
		int my = (int)p.y % 2;
		int mz = (int)p.z % 2;

		p.x = mx ? frac(p.x) : 1.0f - frac(p.x);
		p.y = my ? frac(p.y) : 1.0f - frac(p.y);
		p.z = mz ? frac(p.z) : 1.0f - frac(p.z);

		light.position = p * worldSize + worldOffset;
	}
}

bool LightCullApp::loadModel(const char* filename)
{
	Log::message("Loading model '%s'", filename);

	m_modelName = filename;

	std::string directory = directoryFromFilename(filename);

	m_model = std::unique_ptr<Model>(new Model);

	Model& model = *m_model;

	const bool loaded = model.read(filename);
	if (!loaded)
	{
		return false;
	}

	for (auto& offlineMaterial : model.materials)
	{
		Material material;

		if (offlineMaterial.albedoTexture[0])
		{
			material.albedoTexture = loadTextureFromFile(directory + offlineMaterial.albedoTexture);
		}

		if (!material.albedoTexture.valid())
		{
			material.albedoTexture.retain(m_defaultAlbedoTexture);
		}

		if (offlineMaterial.roughnessTexture[0])
		{
			material.roughnessTexture = loadTextureFromFile(directory + offlineMaterial.roughnessTexture);
		}
		if (!material.roughnessTexture.valid())
		{
			material.roughnessTexture.retain(m_defaultRoughnessTexture);
		}

		if (offlineMaterial.normalTexture[0])
		{
			material.normalTexture = loadTextureFromFile(directory + offlineMaterial.normalTexture);
		}
		if (!material.normalTexture.valid())
		{
			material.normalTexture.retain(m_defaultNormalTexture);
		}

		material.baseColor = offlineMaterial.baseColor;

		m_materials.push_back(material);
	}

	m_worldBoundingBox = model.bounds;

	for (const auto& offlineSegment : model.segments)
	{
		m_segments.push_back(offlineSegment);
	}

	std::sort(m_segments.begin(), m_segments.end(),
	    [](const ModelSegment& a, const ModelSegment& b) { return a.material < b.material; });

	m_vertexCount = (u32)model.vertices.size();
	m_indexCount  = (u32)model.indices.size();

	GfxBufferDesc vbDesc(GfxBufferFlags::Vertex, m_vertexCount, sizeof(ModelVertex));
	m_vertexBuffer = Gfx_CreateBuffer(vbDesc, model.vertices.data());

	GfxBufferDesc ibDesc(GfxBufferFlags::Index, GfxFormat_R32_Uint, m_indexCount, 4);
	m_indexBuffer = Gfx_CreateBuffer(ibDesc, model.indices.data());

	m_lightAnimationBounds = m_worldBoundingBox;

	return true;
}

void LightCullApp::saveReplay(const char* filename)
{
	FileOut stream(filename);
	if (stream.valid())
	{
		writeContainer(stream, m_replayCameraFrames);
	}
}

void LightCullApp::loadReplay(const char* filename)
{
	FileIn stream(filename);
	if (stream.valid())
	{
		readContainer(stream, m_replayCameraFrames);
	}
}

void LightCullApp::saveReplayBenchmarkCSV(const char* filename)
{
	Log::message("Writing benchmark report '%s'", filename);

	std::stringstream output;
	// output << "Frame, GPU Time (ms), List cells, Tree cells, Tree pixels, Total pixels, CPU time, Data size, Visible
	// lights\n";

	int       sampleCount        = 0;
	double    avgGpuLightingTime = 0.0;
	double    avgCpuLightingTime = 0.0;
	double    avgDataSize        = 0.0;
	const int discardFrameCount  = 60;

	std::vector<double> gpuLightingTime;
	std::vector<double> cpuLightingTime;
	std::vector<double> dataSize;

	double avgVisibleLightCount = 0;

	for (const auto& frame : m_replayBenchmarkFrames)
	{
		output << frame.frameId << ", " << frame.gpuLightingTime * 1000.0f << ", " << frame.listCellCount << ", "
		       << frame.treeCellCount << ", " << frame.treePixelCount << ", " << frame.totalPixelCount << ", "
		       << frame.cpuLightBuildTime * 1000.0f << ", " << frame.dataSize << ", " << frame.visibleLightCount << ", "
		       << frame.gpuLightingBuildTime * 1000.0f << "\n";

		avgVisibleLightCount += frame.visibleLightCount;

		if (sampleCount > discardFrameCount)
		{
			avgCpuLightingTime += frame.cpuLightBuildTime;
			avgGpuLightingTime += frame.gpuLightingTime;
			avgDataSize += (double)frame.dataSize;

			gpuLightingTime.push_back(frame.gpuLightingTime);
			cpuLightingTime.push_back(frame.cpuLightBuildTime);
			dataSize.push_back(frame.dataSize);
		}

		++sampleCount;
	}

	if (m_replayBenchmarkFrames.size())
	{
		avgVisibleLightCount /= m_replayBenchmarkFrames.size();
	}

	std::sort(gpuLightingTime.begin(), gpuLightingTime.end());
	std::sort(cpuLightingTime.begin(), cpuLightingTime.end());
	std::sort(dataSize.begin(), dataSize.end());

	size_t pctA = size_t(gpuLightingTime.size() * (25.0 / 100.0));
	size_t pctB = size_t(gpuLightingTime.size() * (50.0 / 100.0));
	size_t pctC = size_t(gpuLightingTime.size() * (75.0 / 100.0));

	avgCpuLightingTime /= max<int>(1, sampleCount - discardFrameCount);
	avgGpuLightingTime /= max<int>(1, sampleCount - discardFrameCount);
	avgDataSize /= max<int>(1, sampleCount - discardFrameCount);

	Log::message("Avg. CPU time: %.2f [%.2f %.2f %.2f]", avgCpuLightingTime * 1000.0, cpuLightingTime[pctA] * 1000.0,
	    cpuLightingTime[pctB] * 1000.0, cpuLightingTime[pctC] * 1000.0);
	Log::message("Avg. GPU time: %.2f [%.2f %.2f %.2f]", avgGpuLightingTime * 1000.0, gpuLightingTime[pctA] * 1000.0,
	    gpuLightingTime[pctB] * 1000.0, gpuLightingTime[pctC] * 1000.0);
	Log::message("Avg. data size : %.2f [%.2f %.2f %.2f] KB", avgDataSize / 1024.0, dataSize[pctA] / 1024.0,
	    dataSize[pctB] / 1024.0, dataSize[pctC] / 1024.0);
	Log::message("Avg. visible lights: %.2f", avgVisibleLightCount);

	FileOut stream(filename);
	if (stream.valid())
	{
		std::string outputString = output.str();
		stream.write(outputString.c_str(), (u32)outputString.length());
	}
}

void LightCullApp::saveCamera(const char* filename) const
{
	FileOut stream(filename);
	if (stream.valid())
	{
		stream.writeT(m_pendingCamera);
	}
}

void LightCullApp::loadCamera(const char* filename, bool smooth)
{
	FileIn stream(filename);
	if (stream.valid())
	{
		stream.readT(m_pendingCamera);
	}
	m_pendingCamera.setAspect(m_window->getAspect());
	if (!smooth)
	{
		m_currentCamera = m_pendingCamera;
	}
}

void LightCullApp::resetCamera()
{
	float aspect    = m_window->getAspect();
	float fov       = 1.0f;
	m_cameraFov     = fov;
	m_pendingCamera = Camera(aspect, fov, 0.25f, 10000.0f);
	m_pendingCamera.lookAt(Vec3(-60.0f, 40.0f, -50.0f), Vec3(-20.0f, 0.0f, -20.0f));
	m_pendingCamera.setClip(0.25f, 10000.0f);
}

void LightCullApp::startReplay()
{
	m_replayState = ReplayState::Play;
	m_replayFrame = 0;
	m_replayBenchmarkFrames.clear();

	m_stats = Stats();
}

void LightCullApp::processCommand(const CmdLoadScene& cmd) { loadModel(cmd.filename.c_str()); }

void LightCullApp::processCommand(const CmdCameraLookAt& cmd)
{
	m_pendingCamera.lookAt(cmd.position, cmd.target);
	m_currentCamera = m_pendingCamera;
}

void LightCullApp::processCommand(const CmdGenerateLights& cmd)
{
	generateLights(MaxLights, cmd.minIntensity, cmd.maxIntensity);
}

void LightCullApp::processCommand(const CmdGenerateLightsOnGeometry& cmd)
{
	generateLightsOnGeometry(MaxLights, cmd.minIntensity, cmd.maxIntensity);
}

void LightCullApp::processCommand(const CmdBenchmark& cmd)
{
	RUSH_ASSERT(m_state == State::Idle);
	m_state = State::Benchmark;

	Log::message("Starting benchmark '%s'", cmd.experimentName.c_str());

	// run current scene for N frames and capture performance data
	m_benchmarkFramesRemaining = 120;
	m_benchmarkExperimentName  = cmd.experimentName;

	switchToFiber(m_mainFiber);
}

void LightCullApp::processCommand(const CmdSetLightingMode& cmd) { m_lightingMode = cmd.mode; }

void LightCullApp::processCommand(const CmdSetLightTreeParams& cmd)
{
	m_tiledLightTreeBuilderParams.sliceCount           = cmd.sliceCount;
	m_tiledLightTreeBuilderParams.maxSliceDepth        = cmd.maxSliceDepth;
	m_tiledLightTreeBuilderParams.useExponentialSlices = cmd.useExponentialSlices;
	m_tiledLightTreeBuilderParams.targetLightsPerLeaf  = cmd.targetLightsPerLeaf;
	m_tiledLightTreeBuilderParams.useShallowTree       = cmd.useShallowTree;
#if USE_GPU_BUILDER
	m_useGpuLightTreeBuilder = cmd.useGpuLightTreeBuilder;
#endif
}

void LightCullApp::processCommand(const CmdSetClusteredShadingParams& cmd)
{
	m_clusteredLightBuilderParams.sliceCount           = cmd.sliceCount;
	m_clusteredLightBuilderParams.maxSliceDepth        = cmd.maxSliceDepth;
	m_clusteredLightBuilderParams.useExponentialSlices = cmd.useExponentialSlices;
}

void LightCullApp::processCommand(const CmdWriteReport& cmd)
{
	// TODO
}

void LightCullApp::processCommand(const CmdSetLightCount& cmd) { m_lightCount = cmd.count; }

void LightCullApp::processCommand(const CmdSetLightAnimationBounds& cmd)
{
	m_lightAnimationBounds.m_min = cmd.min;
	m_lightAnimationBounds.m_max = cmd.max;
}

void LightCullApp::processCommand(const CmdQuit& cmd)
{
	m_state = State::Quit;
	switchToFiber(m_mainFiber);
}

void LightCullApp::processCommand(const CmdSetWindowSize& cmd)
{
	m_window->setSize({cmd.width, cmd.height});
	createGbuffer({cmd.width, cmd.height});
}

void LightCullApp::processCommand(const CmdLoadCamera& cmd)
{
	bool smooth = false;
	loadCamera(cmd.filename.c_str(), smooth);
}

void LightCullApp::processCommand(const CmdBenchmarkReplay& cmd)
{
	loadReplay(cmd.filename.c_str());

	m_state = State::BenchmarkReplay;
	startReplay();

	switchToFiber(m_mainFiber);
}

void LightCullApp::processCommand(const CmdBenchmarkSave& cmd) { saveReplayBenchmarkCSV(cmd.filename.c_str()); }

void LightCullApp::processCommand(const CmdSetTileSize& cmd) { m_tileSize = cmd.size; }

void LightCullApp::processCommand(const CmdGenerateMeshes& cmd) { generateMeshes(); }

void LightCullApp::processCommand(const CmdCaptureVideo& cmd)
{
#if USE_FFMPEG
	m_captureVideoPath = cmd.path;
	std::stringstream ffmpegCmd;
	ffmpegCmd << "ffmpeg -r 60 -f rawvideo -pix_fmt rgba "
	          << "-s " << m_window->getWidth() << "x" << m_window->getHeight() << " "
	          << "-i - -preset fast -y -pix_fmt yuv420p -crf 21 " << m_captureVideoPath;

	m_ffmpegPipe = _popen(ffmpegCmd.str().c_str(), "wb");
	if (m_ffmpegPipe)
	{
		Log::message("Opened ffmpeg pipe");
	}
	else
	{
		Log::error("Failed to open ffmpeg pipe");
	}
#else // USE_FFMPEG
	Log::error("Video capture using ffmpeg is not implemented");
#endif // USE_FFMPEG
}

void LightCullApp::processCommand(const CmdLoadStaticScene& cmd)
{
	m_lights.clear();

	for (const auto& it : cmd.pointLights)
	{
		AnimatedLightSource light = {};

		light.position = light.originalPosition = it.position;
		light.attenuationEnd                    = it.radius;

		light.intensity = Vec3(10.0); // todo: derive intensity from radius using some kind of heuristic

		m_lights.push_back(light);
	}

	m_lightCount = (int)m_lights.size();

	// Compute camera parameters based on provided view & projection matrices
	{

		Mat4 viewInverse = cmd.viewMatrix.inverse();
		Vec3 camPos      = viewInverse.rows[3].xyz();
		Vec3 camForward  = viewInverse.rows[2].xyz();
		Vec3 camUp       = viewInverse.rows[1].xyz();

		m_pendingCamera.lookAt(camPos, camPos + camForward, camUp);

		float sy = cmd.projectionMatrix.rows[1].y;

		float a = cmd.projectionMatrix.rows[2].z;
		float b = cmd.projectionMatrix.rows[3].z;

		float clipNear = -b / a;
		float clipFar  = -b / (a - 1.0f);

		float fov = float(atan(1.0 / sy) * 2.0);

		m_pendingCamera.setClip(clipNear, clipFar);
		m_pendingCamera.setFov(fov);
		m_cameraFov = fov;

		m_currentCamera = m_pendingCamera;
	}

	// Load static gbuffer textures
	{
		m_staticGbuffer            = StaticGbuffer();
		m_staticGbuffer.depth      = loadDDS(cmd.depth.c_str());
		m_staticGbuffer.normals    = loadDDS(cmd.normals.c_str());
		m_staticGbuffer.albedo     = loadDDS(cmd.albedo.c_str());
		m_staticGbuffer.specularRM = loadDDS(cmd.specularRM.c_str());

		m_useStaticGbuffer = true;
	}
}
