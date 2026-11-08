/*
This file is part of MakuGram,
a custom Telegram Desktop client fork.
*/
#include "makugram/makugram_lang.h"

#include <rpl/rpl.h>

namespace MakuGram::Tr {

bool IsRussian() {
	const auto id = ::Lang::Id();
	return id.startsWith(u"ru"_q, Qt::CaseInsensitive);
}

QString Get(const QString &ru, const QString &en) {
	return IsRussian() ? ru : en;
}

rpl::producer<QString> Value(const QString &ru, const QString &en) {
	return rpl::single(Get(ru, en))
		| rpl::then(::Lang::Updated() | rpl::map([=] { return Get(ru, en); }));
}

} // namespace MakuGram::Tr
