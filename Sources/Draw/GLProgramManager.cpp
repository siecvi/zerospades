/*
 Copyright (c) 2013 yvt

 This file is part of OpenSpades.

 OpenSpades is free software: you can redistribute it and/or modify
 it under the terms of the GNU General Public License as published by
 the Free Software Foundation, either version 3 of the License, or
 (at your option) any later version.

 OpenSpades is distributed in the hope that it will be useful,
 but WITHOUT ANY WARRANTY; without even the implied warranty of
 MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 GNU General Public License for more details.

 You should have received a copy of the GNU General Public License
 along with OpenSpades.  If not, see <http://www.gnu.org/licenses/>.

 */

#include "GLProgramManager.h"
#include "GLDynamicLightShader.h"
#include "GLProgram.h"
#include "GLSettings.h"
#include "GLShader.h"
#include "GLShadowMapShader.h"
#include "GLShadowShader.h"
#include "IGLShadowMapRenderer.h"
#include <Core/Debug.h>
#include <Core/Exception.h>
#include <Core/FileManager.h>
#include <Core/Math.h>
#include <Core/Stopwatch.h>
#include <Core/TMPUtils.h>

namespace spades {
	namespace draw {
		GLProgramManager::GLProgramManager(IGLDevice& d, GLSettings& settings)
		    : device(d), settings(settings) {
			SPADES_MARK_FUNCTION();
		}

		GLProgramManager::~GLProgramManager() { SPADES_MARK_FUNCTION(); }

		GLProgram* GLProgramManager::RegisterProgram(const std::string& name) {
			SPADES_MARK_FUNCTION();

			auto it = programs.find(name);
			if (it == programs.end()) {
				auto program = CreateProgram(name);
				GLProgram* programPtr = program.get();
				programs[name] = std::move(program);
				return programPtr;
			} else {
				return it->second.get();
			}
		}

		GLShader* GLProgramManager::RegisterShader(const std::string& name) {
			SPADES_MARK_FUNCTION();

			auto it = shaders.find(name);
			if (it == shaders.end()) {
				auto shader = CreateShader(name);
				GLShader* shaderPtr = shader.get();
				shaders[name] = std::move(shader);
				return shaderPtr;
			} else {
				return it->second.get();
			}
		}

		std::unique_ptr<GLProgram> GLProgramManager::CreateProgram(const std::string& name) {
			SPADES_MARK_FUNCTION();

			SPLog("Loading GLSL program '%s'", name.c_str());
			std::string text = FileManager::ReadAllBytes(name.c_str());
			std::vector<std::string> lines = SplitIntoLines(text);

			auto p = stmp::make_unique<GLProgram>(&device, name);

			for (const auto& line : lines) {
				std::string text = TrimSpaces(line);
				if (text.empty())
					break; // TODO: Probably wrong, but unconfirmed

				if (text == "*shadow*") {
					std::vector<GLShader*> shaders =
					  GLShadowShader::RegisterShader(this, settings, false);
					for (const auto& shader : shaders)
						p->Attach(*shader);
				} else if (text == "*shadow-lite*") {
					std::vector<GLShader*> shaders =
					  GLShadowShader::RegisterShader(this, settings, false, true);
					for (const auto& shader : shaders)
						p->Attach(*shader);
				} else if (text == "*shadow-variance*") {
					std::vector<GLShader*> shaders =
					  GLShadowShader::RegisterShader(this, settings, true);
					for (const auto& shader : shaders)
						p->Attach(*shader);
				} else if (text == "*dlight*") {
					std::vector<GLShader*> shaders = GLDynamicLightShader::RegisterShader(this);
					for (const auto& shader : shaders)
						p->Attach(*shader);
				} else if (text == "*shadowmap*") {
					std::vector<GLShader*> shaders = GLShadowMapShader::RegisterShader(this);
					for (const auto& shader : shaders)
						p->Attach(*shader);
				} else if (text[0] == '*') {
					SPRaise("Unknown special shader: %s", text.c_str());
				} else if (text[0] == '#') {
					// Comment line
				} else {
					auto shader = CreateShader(text);
					p->Attach(*shader);
				}
			}

			Stopwatch sw;
			p->Link();
			SPLog("Successfully linked GLSL program '%s' in %.3fms",
				name.c_str(), sw.GetTime() * 1000.0);
			return p;
		}

		std::unique_ptr<GLShader> GLProgramManager::CreateShader(const std::string& name) {
			SPADES_MARK_FUNCTION();

			SPLog("Loading GLSL shader '%s'", name.c_str());
			std::string text = FileManager::ReadAllBytes(name.c_str());

			GLShader::Type type;
			if (name.find(".fs") != std::string::npos)
				type = GLShader::FragmentShader;
			else if (name.find(".vs") != std::string::npos)
				type = GLShader::VertexShader;
			else if (name.find(".gs") != std::string::npos)
				type = GLShader::GeometryShader;
			else
				SPRaise("Failed to determine the type of a shader: %s", name.c_str());

			auto s = stmp::make_unique<GLShader>(device, type);

			std::string finalSource;

			if (settings.r_hdr) {
				finalSource += "#define USE_HDR 1\n";
				finalSource += "#define LINEAR_FRAMEBUFFER 1\n";
			} else {
				finalSource += "#define USE_HDR 0\n";
				finalSource += "#define LINEAR_FRAMEBUFFER 0\n";
			}

			if (settings.r_fogShadow)
				finalSource += "#define USE_VOLUMETRIC_FOG 1\n";
			else
				finalSource += "#define USE_VOLUMETRIC_FOG 0\n";

			if (settings.r_ssao)
				finalSource += "#define USE_SSAO 1\n";
			else
				finalSource += "#define USE_SSAO 0\n";

			if (settings.r_radiosity.operator int() >= 2)
				finalSource += "#define USE_RADIOSITY 2\n";
			else if (settings.r_radiosity.operator int() >= 1)
				finalSource += "#define USE_RADIOSITY 1\n";
			else
				finalSource += "#define USE_RADIOSITY 0\n";

			finalSource += text;

			s->AddSource(finalSource);

			Stopwatch sw;
			s->Compile();
			SPLog("Successfully compiled GLSL shader '%s' in %.3fms",
				name.c_str(), sw.GetTime() * 1000.0);
			return s;
		}
	} // namespace draw
} // namespace spades