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

#include "ScriptManager.h"
#include <Client/IImage.h>

namespace spades {
	namespace client {

		class ImageRegistrar : public ScriptObjectRegistrar {
		public:
			ImageRegistrar() : ScriptObjectRegistrar("Image") {}
			virtual void Register(ScriptManager* manager, Phase phase) {
				asIScriptEngine* eng = manager->GetEngine();
				int r;
				eng->SetDefaultNamespace("spades");
				switch (phase) {
					case PhaseObjectType:
						r = eng->RegisterObjectType("Image", 0, asOBJ_REF);
						manager->CheckError(r);
						r = eng->RegisterObjectBehaviour("Image", asBEHAVE_ADDREF, "void f()",
						                                 asMETHOD(IImage, AddRef), asCALL_THISCALL);
						manager->CheckError(r);
						r = eng->RegisterObjectBehaviour("Image", asBEHAVE_RELEASE, "void f()",
						                               asMETHOD(IImage, Release), asCALL_THISCALL);
						manager->CheckError(r);
						break;
					case PhaseObjectMember:
						r = eng->RegisterObjectMethod("Image", "float get_Width()",
						                              asMETHOD(IImage, GetWidth), asCALL_THISCALL);
						manager->CheckError(r);
						r = eng->RegisterObjectMethod("Image", "float get_Height()",
						                              asMETHOD(IImage, GetHeight), asCALL_THISCALL);
						manager->CheckError(r);
						break;
					default: break;
				}
			}
		};

		static ImageRegistrar registrar;
	} // namespace client
} // namespace spades