/*
This file is part of MakuGram,
a custom Telegram Desktop client fork.
*/
#include "makugram/makugram_details.h"

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
	if (userId < 50'000'000ULL) return QString::fromUtf8("2013-2014 г.");
	if (userId < 150'000'000ULL) return QString::fromUtf8("~2015 г.");
	if (userId < 300'000'000ULL) return QString::fromUtf8("~2016 г.");
	if (userId < 500'000'000ULL) return QString::fromUtf8("~2017 г.");
	if (userId < 800'000'000ULL) return QString::fromUtf8("~2018 г.");
	if (userId < 1'100'000'000ULL) return QString::fromUtf8("~2019 г.");
	if (userId < 1'500'000'000ULL) return QString::fromUtf8("~2020 г.");
	if (userId < 2'000'000'000ULL) return QString::fromUtf8("~2021 г.");
	if (userId < 2'400'000'000ULL) return QString::fromUtf8("~2022 г.");
	if (userId < 2'800'000'000ULL) return QString::fromUtf8("~2023 г.");
	if (userId < 3'300'000'000ULL) return QString::fromUtf8("~2024 г.");
	if (userId < 3'800'000'000ULL) return QString::fromUtf8("~2025 г.");
	return QString::fromUtf8("~2026 г. (Свежий аккаунт)");
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
