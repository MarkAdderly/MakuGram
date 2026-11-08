/*
This file is part of MakuGram,
a custom Telegram Desktop client fork.
*/
#pragma once

#include "lang/lang_keys.h"
#include "rpl/producer.h"
#include <QtCore/QString>

namespace MakuGram::Tr {

[[nodiscard]] bool IsRussian();

[[nodiscard]] QString Get(const QString &ru, const QString &en);

[[nodiscard]] rpl::producer<QString> Value(const QString &ru, const QString &en);

} // namespace MakuGram::Tr
