/*
This file is part of MakuGram,
a custom Telegram Desktop client fork.
*/
#include "makugram/makugram_details.h"
#include "makugram/makugram_lang.h"

#include "data/data_peer.h"
#include "data/data_user.h"
#include "base/unixtime.h"
#include <map>
#include <mutex>

namespace MakuGram {
namespace {

std::mutex gJoinDatesMutex;
std::map<std::pair<uint64, uint64>, TimeId> gJoinDates;

} // namespace

QString EstimatedRegistrationDate(uint64 userId) {
	if (userId == 0) {
		return QString();
	}
	const auto ru = Tr::IsRussian();
	const auto y = ru ? QString::fromUtf8(" г.") : QString();
	if (userId < 50'000'000ULL) return "2013-2014" + y;
	if (userId < 150'000'000ULL) return "~2015" + y;
	if (userId < 300'000'000ULL) return "~2016" + y;
	if (userId < 500'000'000ULL) return "~2017" + y;
	if (userId < 800'000'000ULL) return "~2018" + y;
	if (userId < 1'100'000'000ULL) return "~2019" + y;
	if (userId < 1'500'000'000ULL) return "~2020" + y;
	if (userId < 2'000'000'000ULL) return "~2021" + y;
	if (userId < 2'400'000'000ULL) return "~2022" + y;
	if (userId < 2'800'000'000ULL) return "~2023" + y;
	if (userId < 3'300'000'000ULL) return "~2024" + y;
	if (userId < 3'800'000'000ULL) return "~2025" + y;
	return ru
		? QString::fromUtf8("~2026 г. (Свежий аккаунт)")
		: QString::fromUtf8("~2026 (New account)");
}

void RecordChatJoinDate(uint64 peerId, uint64 userId, TimeId date) {
	if (peerId && userId && date > 0) {
		const auto lock = std::lock_guard(gJoinDatesMutex);
		if (gJoinDates.size() > 10000) {
			gJoinDates.clear();
		}
		gJoinDates[{peerId, userId}] = date;
	}
}

bool IsNewbieInChat(const PeerData *peer, const UserData *user) {
	if (!peer || !user) {
		return false;
	}
	const auto peerId = peer->id.value;
	const auto userId = user->id.value;

	const auto joinDate = [&]() -> TimeId {
		const auto lock = std::lock_guard(gJoinDatesMutex);
		const auto it = gJoinDates.find({peerId, userId});
		return (it != gJoinDates.end()) ? it->second : TimeId(0);
	}();

	const auto now = base::unixtime::now();
	constexpr auto kThreeMonths = 90 * 86400;

	if (joinDate > 0) {
		return (now - joinDate) < kThreeMonths;
	}

	const auto rawUserId = userId & 0xFFFFFFFFULL;
	if (rawUserId > 3'900'000'000ULL) {
		return true;
	}

	return false;
}

} // namespace MakuGram
