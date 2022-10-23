/*
 Copyright (c) 2013 yvt
 based on code of pysnip (c) Mathias Kaerlev 2011-2012.

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

#include "TCGameMode.h"
#include "World.h"
#include <Core/Debug.h>

namespace spades {
	namespace client {
		TCGameMode::TCGameMode(World& w) : IGameMode(m_TC), world(w) { SPADES_MARK_FUNCTION(); }
		TCGameMode::~TCGameMode() { SPADES_MARK_FUNCTION(); }

		TCGameMode::Team& TCGameMode::GetTeam(int t) {
			SPADES_MARK_FUNCTION();
			return teams.at(t);
		}

		void TCGameMode::AddTerritory(const spades::client::TCGameMode::Territory& t) {
			territories.push_back(t);
		}

		float TCGameMode::Territory::GetProgress() {
			float dt = mode.world.GetTime() - progressStartTime;
			float prg = progressBasePos + progressRate * dt;
			if (prg < 0.0F)
				prg = 0.0F;
			if (prg > 1.0F)
				prg = 1.0F;
			return prg;
		}
	} // namespace client
} // namespace spades