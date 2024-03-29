#include <cassert>
#include <cstring>

#ifdef WIN32
#include <windows.h>
#else
#include <fcntl.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <unistd.h>
#endif

#include "MumbleLink.h"

namespace spades {
	struct MumbleLinkedMemory {
		uint32_t uiVersion;
		uint32_t uiTick;
		float fAvatarPosition[3];
		float fAvatarFront[3];
		float fAvatarTop[3];
		wchar_t name[256];
		float fCameraPosition[3];
		float fCameraFront[3];
		float fCameraTop[3];
		wchar_t identity[256];
		uint32_t context_len;
		unsigned char context[256];
		wchar_t description[2048];
	};

	struct MumbleLinkPrivate {
#ifdef _WIN32
		HANDLE obj;
#else
		int fd;
#endif
	};

	MumbleLink::MumbleLink()
	    : metrePerBlock(0.63F), mumbleLinkedMemory(nullptr), priv(new MumbleLinkPrivate()) {}

	MumbleLink::~MumbleLink() {
#ifdef WIN32
		UnmapViewOfFile(mumbleLinkedMemory);
		if (priv->obj != nullptr)
			CloseHandle(priv->obj);
#else
		munmap(mumbleLinkedMemory, sizeof(*mumbleLinkedMemory));
		if (priv->fd > 0)
			close(priv->fd);
#endif
	}

	void MumbleLink::SetMumbleVector3(float mumble_vec[3], const spades::Vector3& v) {
		mumble_vec[0] = v.x;
		mumble_vec[1] = v.z;
		mumble_vec[2] = v.y;
	}

	bool MumbleLink::Init() {
		assert(mumbleLinkedMemory == nullptr);
#ifdef WIN32
		priv->obj = OpenFileMappingW(FILE_MAP_ALL_ACCESS, FALSE, L"MumbleLink");
		if (priv->obj == nullptr)
			return false;

		mumbleLinkedMemory = static_cast<MumbleLinkedMemory*>(
		  MapViewOfFile(priv->obj, FILE_MAP_ALL_ACCESS, 0, 0, sizeof(*mumbleLinkedMemory)));

		if (mumbleLinkedMemory == nullptr) {
			CloseHandle(priv->obj);
			priv->obj = nullptr;
			return false;
		}
#else
		std::string name = "/MumbleLink." + std::to_string(getuid());

		priv->fd = shm_open(name.c_str(), O_RDWR, S_IRUSR | S_IWUSR);

		if (priv->fd < 0)
			return false;

		mumbleLinkedMemory = static_cast<MumbleLinkedMemory*>((mmap(
		  nullptr, sizeof(*mumbleLinkedMemory), PROT_READ | PROT_WRITE, MAP_SHARED, priv->fd, 0)));

		if (mumbleLinkedMemory == MAP_FAILED) {
			mumbleLinkedMemory = nullptr;
			return false;
		}
#endif
		return true;
	}

	void MumbleLink::SetContext(const std::string& context) {
		if (mumbleLinkedMemory == nullptr)
			return;
		size_t len(std::min(256, static_cast<int>(context.size())));
		std::memcpy(mumbleLinkedMemory->context, context.c_str(), len);
		mumbleLinkedMemory->context_len = static_cast<std::uint32_t>(len);
	}

	void MumbleLink::SetIdentity(const std::string& identity) {
		if (mumbleLinkedMemory == nullptr)
			return;
		std::wcsncpy(mumbleLinkedMemory->identity,
		             std::wstring(identity.begin(), identity.end()).c_str(), 256);
	}

	void MumbleLink::Update(spades::client::Player* player) {
		if (mumbleLinkedMemory == nullptr || player == nullptr)
			return;

		if (mumbleLinkedMemory->uiVersion != 2) {
			wcsncpy(mumbleLinkedMemory->name, L"OpenSpades", 256);
			wcsncpy(mumbleLinkedMemory->description, L"OpenSpades Link plugin.", 2048);
			mumbleLinkedMemory->uiVersion = 2;
		}
		mumbleLinkedMemory->uiTick++;

		// Left handed coordinate system.
		// X positive towards "right".
		// Y positive towards "up".
		// Z positive towards "front".
		//
		// 1 unit = 1 meter

		// Unit vector pointing out of the avatar's eyes aka "At"-vector.
		const auto& avatarFrontVector = player->GetFront();
		SetMumbleVector3(mumbleLinkedMemory->fAvatarFront, avatarFrontVector);

		// Unit vector pointing out of the top of the avatar's head aka "Up"-vector
		// (here Top points straight up).
		const auto& avatarTopVector = player->GetUp();
		SetMumbleVector3(mumbleLinkedMemory->fAvatarTop, avatarTopVector);

		// Position of the avatar (here standing slightly off the origin)
		const auto& avatarPosition = player->GetPosition() * metrePerBlock;
		SetMumbleVector3(mumbleLinkedMemory->fAvatarPosition, avatarPosition);

		// Same as avatar but for the camera.
		SetMumbleVector3(mumbleLinkedMemory->fCameraPosition, avatarPosition);
		SetMumbleVector3(mumbleLinkedMemory->fCameraFront, avatarFrontVector);
		SetMumbleVector3(mumbleLinkedMemory->fCameraTop, avatarTopVector);
	}
} // namespace spades