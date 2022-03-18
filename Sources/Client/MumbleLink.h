#pragma once

#include <memory>

#include "Player.h"

namespace spades {
	struct MumbleLinkedMemory;
	struct MumbleLinkPrivate;

	class MumbleLink {
		const float metrePerBlock;
		MumbleLinkedMemory* mumbleLinkedMemory;
		std::unique_ptr<MumbleLinkPrivate> priv;

		void SetMumbleVector3(float mumble_vec[3], const spades::Vector3& v);

	public:
		MumbleLink();
		~MumbleLink();

		bool Init();
		void SetContext(const std::string& context);
		void SetIdentity(const std::string& identity);
		void Update(spades::client::Player* player);
	};
} // namespace spades