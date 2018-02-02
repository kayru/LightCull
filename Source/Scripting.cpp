#include "Scripting.h"

#include <Rush/UtilFile.h>
#include <Rush/UtilLog.h>

#include <rapidjson/document.h>
#include <rapidjson/rapidjson.h>

#include <vector>

static Vec3 parseVec3(const rapidjson::Value& obj)
{
	RUSH_ASSERT(obj.IsArray());
	return Vec3(obj[0].GetFloat(), obj[1].GetFloat(), obj[2].GetFloat());
}

static Mat4 parseMat4(const rapidjson::Value& obj)
{
	RUSH_ASSERT(obj.IsArray());
	float a[16];
	for (int i = 0; i < 16; ++i)
	{
		a[i] = obj[i].GetFloat();
	}
	return Mat4(a);
}

static CmdLoadStaticScene::PointLight parsePointLight(const rapidjson::Value& obj)
{
	CmdLoadStaticScene::PointLight result = {};
	RUSH_ASSERT(obj.IsObject());

	const auto& objValue = obj.GetObject();

	result.position = parseVec3(objValue["pos"]);
	result.radius   = objValue["radius"].GetFloat();

	return result;
}

static std::vector<CmdLoadStaticScene::PointLight> parsePointLightArray(const rapidjson::Value& obj)
{
	std::vector<CmdLoadStaticScene::PointLight> result;
	RUSH_ASSERT(obj.IsArray());

	for (const auto& it : obj.GetArray())
	{
		CmdLoadStaticScene::PointLight light = parsePointLight(it);
		result.push_back(light);
	}

	return result;
}

void runCommandsFromJsonFile(const char* filename, CommandHandler* handler)
{
	using namespace rapidjson;

	std::vector<char> buffer;

	{
		FileIn stream(filename);
		if (stream.valid())
		{
			u32 length = stream.length();
			buffer.resize(length + 1);
			stream.read(buffer.data(), length);
			buffer[length] = 0;
		}
		else
		{
			Log::error("Failed to load script file '%s'.", filename);
			return;
		}
	}

	Document document;

	if (document.ParseInsitu<kParseCommentsFlag | kParseTrailingCommasFlag>(buffer.data()).HasParseError())
	{
		Log::error("Script parse error"); // TODO: get error string from rapidjson
		return;
	}

	RUSH_ASSERT(document.IsArray());

	for (const auto& it : document.GetArray())
	{
		const char* objName  = it.MemberBegin()->name.GetString();
		const auto& objValue = it.MemberBegin()->value.GetObject();

		if (!strcmp(objName, "LoadScene"))
		{
			CmdLoadScene cmd;
			cmd.filename = objValue["filename"].GetString();
			handler->processCommand(cmd);
		}
		else if (!strcmp(objName, "CameraLookAt"))
		{
		}
		else if (!strcmp(objName, "GenerateLights"))
		{
			CmdGenerateLights cmd;
			cmd.minIntensity = objValue["minIntensity"].GetFloat();
			cmd.maxIntensity = objValue["maxIntensity"].GetFloat();
			handler->processCommand(cmd);
		}
		else if (!strcmp(objName, "GenerateLightsOnGeometry"))
		{
			CmdGenerateLightsOnGeometry cmd;
			cmd.minIntensity = objValue["minIntensity"].GetFloat();
			cmd.maxIntensity = objValue["maxIntensity"].GetFloat();
			handler->processCommand(cmd);
		}
		else if (!strcmp(objName, "Profile"))
		{
			CmdBenchmark cmd;
			cmd.experimentName = objValue["experimentName"].GetString();
			handler->processCommand(cmd);
		}
		else if (!strcmp(objName, "SetLightingMode"))
		{
			CmdSetLightingMode cmd;
			cmd.mode             = LightingMode::Tree;
			const char* modeName = objValue["mode"].GetString();

			// backwards compatibility
			if (!strcmp(modeName, "Adaptive"))
				modeName = "Hybrid";
			if (!strcmp(modeName, "Interval"))
				modeName = "Tree";

			for (u32 i = 0; i < (u32)LightingMode::count; ++i)
			{
				cmd.mode = (LightingMode)i;
				if (!strcmp(toString(cmd.mode), modeName))
				{
					break;
				}
			}

			handler->processCommand(cmd);
		}
		else if (!strcmp(objName, "SetLightTreeParams"))
		{
			CmdSetLightTreeParams cmd;
			cmd.sliceCount             = objValue["sliceCount"].GetInt();
			cmd.maxSliceDepth          = objValue["maxSliceDepth"].GetFloat();
			cmd.useExponentialSlices   = objValue["useExponentialSlices"].GetBool();
			cmd.useShallowTree         = objValue["useShallowTree"].GetBool();
			cmd.targetLightsPerLeaf    = objValue["targetLightsPerLeaf"].GetInt();
			cmd.useGpuLightTreeBuilder = objValue["useGpuLightTreeBuilder"].GetBool();
			handler->processCommand(cmd);
		}
		else if (!strcmp(objName, "SetClusteredShadingParams"))
		{
			CmdSetClusteredShadingParams cmd;
			cmd.sliceCount           = objValue["sliceCount"].GetInt();
			cmd.maxSliceDepth        = objValue["maxSliceDepth"].GetFloat();
			cmd.useExponentialSlices = objValue["useExponentialSlices"].GetBool();
			handler->processCommand(cmd);
		}
		else if (!strcmp(objName, "WriteReport"))
		{
			// TODO
		}
		else if (!strcmp(objName, "SetLightCount"))
		{
			CmdSetLightCount cmd;
			cmd.count = objValue["count"].GetInt();
			handler->processCommand(cmd);
		}
		else if (!strcmp(objName, "SetLightAnimationBounds"))
		{
			CmdSetLightAnimationBounds cmd;
			cmd.min = parseVec3(objValue["min"]);
			cmd.max = parseVec3(objValue["max"]);
			handler->processCommand(cmd);
		}
		else if (!strcmp(objName, "Quit"))
		{
			CmdQuit cmd;
			handler->processCommand(cmd);
		}
		else if (!strcmp(objName, "SetWindowSize"))
		{
			CmdSetWindowSize cmd;
			cmd.width  = objValue["width"].GetInt();
			cmd.height = objValue["height"].GetInt();
			handler->processCommand(cmd);
		}
		else if (!strcmp(objName, "LoadCamera"))
		{
			CmdLoadCamera cmd;
			cmd.filename = objValue["filename"].GetString();
			handler->processCommand(cmd);
		}
		else if (!strcmp(objName, "BenchmarkReplay"))
		{
			CmdBenchmarkReplay cmd;
			cmd.filename = objValue["filename"].GetString();
			handler->processCommand(cmd);
		}
		else if (!strcmp(objName, "BenchmarkSave"))
		{
			CmdBenchmarkSave cmd;
			cmd.filename = objValue["filename"].GetString();
			handler->processCommand(cmd);
		}
		else if (!strcmp(objName, "SetTileSize"))
		{
			CmdSetTileSize cmd;
			cmd.size = objValue["size"].GetUint();
			handler->processCommand(cmd);
		}
		else if (!strcmp(objName, "GenerateMeshes"))
		{
			CmdGenerateMeshes cmd;
			handler->processCommand(cmd);
		}
		else if (!strcmp(objName, "CaptureVideo"))
		{
			CmdCaptureVideo cmd;
			cmd.path = objValue["path"].GetString();
			handler->processCommand(cmd);
		}
		else if (!strcmp(objName, "LoadStaticScene"))
		{
			CmdLoadStaticScene cmd;

			cmd.depth            = objValue["depth"].GetString();
			cmd.normals          = objValue["normals"].GetString();
			cmd.albedo           = objValue["albedo"].GetString();
			cmd.specularRM       = objValue["specularRM"].GetString();
			cmd.gbufferFormat    = objValue["gbufferFormat"].GetString();
			cmd.coordinateSystem = objValue["coordinateSystem"].GetString();

			cmd.viewMatrix       = parseMat4(objValue["viewMatrix"]);
			cmd.projectionMatrix = parseMat4(objValue["projectionMatrix"]);

			cmd.pointLights = parsePointLightArray(objValue["pointLights"]);

			handler->processCommand(cmd);
		}
		else
		{
			Log::error("Unknown script object '%s'", objName);
		}
	}
}
