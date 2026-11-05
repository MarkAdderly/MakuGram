/*
This file is part of MakuGram,
a custom Telegram Desktop client fork.
*/
#pragma once

#include <QtCore/QString>
#include "base/basic_types.h"

class PeerData;
class UserData;

namespace MakuGram {

[[nodiscard]] QString EstimatedRegistrationDate(uint64 userId);
[[nodiscard]] bool IsNewbieInChat(const PeerData *peer, const UserData *user);
void RecordChatJoinDate(uint64 peerId, uint64 userId, TimeId date);

} // namespace MakuGram
