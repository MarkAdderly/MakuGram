/*
This file is part of Telegram Desktop,
the official desktop application for the Telegram messaging service.

For license and copyright information please follow this link:
https://github.com/telegramdesktop/tdesktop/blob/master/LEGAL
*/
#include "api/api_sending.h"

#include "api/api_text_entities.h"
#include "base/random.h"
#include "base/unixtime.h"
#include "data/business/data_shortcut_messages.h"
#include "data/data_document.h"
#include "data/data_photo.h"
#include "data/data_channel.h" // ChannelData::addsSignature.
#include "data/data_user.h" // UserData::name
#include "data/data_chat.h"
#include "data/data_premium_limits.h"
#include "history/history_item_components.h"
#include "data/data_messages.h"
#include "data/data_session.h"
#include "data/data_file_origin.h"
#include "data/data_forum.h"
#include "data/data_histories.h"
#include "data/data_changes.h"
#include "data/components/ephemeral_messages.h"
#include "data/components/welcome_messages.h"
#include "data/stickers/data_stickers.h"
#include "history/history.h"
#include "history/history_item.h"
#include "history/history_item_helpers.h" // NewMessageFlags.
#include "chat_helpers/message_field.h" // ConvertTextTagsToEntities.
#include "chat_helpers/stickers_dice_pack.h" // DicePacks::kDiceString.
#include "ui/text/text_entity.h" // TextWithEntities.
#include "ui/item_text_options.h" // Ui::ItemTextOptions.
#include "ui/chat/attach/attach_prepare.h"
#include "ui/chat/attach/attach_send_files_way.h"
#include "main/main_session.h"
#include "main/main_app_config.h"
#include "storage/localimageloader.h"
#include "storage/file_upload.h"
#include "storage/storage_media_prepare.h"
#include "data/data_photo_media.h"
#include "data/data_document_media.h"
#include "styles/style_boxes.h"
#include "storage/storage_account.h"
#include "base/timer.h"
#include "ui/toast/toast.h"
#include "makugram/makugram_settings.h"
#include <QtCore/QDir>
#include <QtCore/QFileInfo>
#include <QtCore/QBuffer>
#include "mainwidget.h"
#include "apiwrap.h"
#include "base/call_delayed.h"
#include "core/application.h"
#include "window/window_controller.h"

namespace Api {
namespace {

void InnerFillMessagePostFlags(
		const SendOptions &options,
		not_null<PeerData*> peer,
		MessageFlags &flags) {
	if (ShouldSendSilent(peer, options)) {
		flags |= MessageFlag::Silent;
	}
	if (!peer->amAnonymous()
		|| (!peer->isBroadcast()
			&& options.sendAs
			&& options.sendAs != peer)) {
		flags |= MessageFlag::HasFromId;
	}
	const auto channel = peer->asBroadcast();
	if (!channel) {
		return;
	}
	flags |= MessageFlag::Post;
	// Don't display views and author of a new post when it's scheduled.
	if (options.scheduled) {
		return;
	}
	flags |= MessageFlag::HasViews;
	if (channel->addsSignature()) {
		flags |= MessageFlag::HasPostAuthor;
	}
}

[[nodiscard]] int FillSendMediaOptionFlags(
		SendOptions &options,
		not_null<PeerData*> peer,
		MessageFlags &flags,
		MTPmessages_SendMedia::Flags &sendFlags) {
	const auto starsPaid = std::min(
		peer->starsPerMessageChecked(),
		options.starsApproved);
	if (options.scheduled) {
		flags |= MessageFlag::IsOrWasScheduled;
		sendFlags |= MTPmessages_SendMedia::Flag::f_schedule_date;
		if (options.scheduleRepeatPeriod) {
			sendFlags |= MTPmessages_SendMedia::Flag::f_schedule_repeat_period;
		}
	}
	if (options.shortcutId) {
		flags |= MessageFlag::ShortcutMessage;
		sendFlags |= MTPmessages_SendMedia::Flag::f_quick_reply_shortcut;
	}
	if (options.effectId) {
		sendFlags |= MTPmessages_SendMedia::Flag::f_effect;
	}
	if (options.suggest) {
		sendFlags |= MTPmessages_SendMedia::Flag::f_suggested_post;
	}
	if (options.invertCaption) {
		flags |= MessageFlag::InvertMedia;
		sendFlags |= MTPmessages_SendMedia::Flag::f_invert_media;
	}
	if (starsPaid) {
		options.starsApproved -= starsPaid;
		sendFlags |= MTPmessages_SendMedia::Flag::f_allow_paid_stars;
	}
	return starsPaid;
}

void SendSimpleMedia(SendAction action, MTPInputMedia inputMedia) {
	const auto history = action.history;
	const auto peer = history->peer;
	const auto session = &history->session();
	const auto api = &session->api();

	action.clearDraft = false;
	action.generateLocal = false;
	api->sendAction(action);

	if (!action.options.scheduled
		&& !action.options.shortcutId
		&& session->ephemeralMessages().sendSimpleMedia(
			history,
			action.replyTo,
			inputMedia)) {
		api->finishForwarding(action);
		return;
	}

	if (action.replyTo.messageId
		&& !IsServerMsgId(action.replyTo.messageId.msg)
		&& !session->data().message(action.replyTo.messageId)) {
		action.replyTo = {
			.messageId = (action.replyTo.topicRootId
				? FullMsgId(peer->id, action.replyTo.topicRootId)
				: FullMsgId()),
			.topicRootId = action.replyTo.topicRootId,
			.monoforumPeerId = action.replyTo.monoforumPeerId,
		};
	}

	const auto randomId = base::RandomValue<uint64>();

	auto flags = NewMessageFlags(peer);
	auto sendFlags = MTPmessages_SendMedia::Flags(0);
	if (action.replyTo) {
		flags |= MessageFlag::HasReplyInfo;
		sendFlags |= MTPmessages_SendMedia::Flag::f_reply_to;
	}
	const auto silentPost = ShouldSendSilent(peer, action.options);
	InnerFillMessagePostFlags(action.options, peer, flags);
	if (silentPost) {
		sendFlags |= MTPmessages_SendMedia::Flag::f_silent;
	}
	const auto sendAs = action.options.sendAs;
	if (sendAs) {
		sendFlags |= MTPmessages_SendMedia::Flag::f_send_as;
	}
	const auto messagePostAuthor = peer->isBroadcast()
		? session->user()->name()
		: QString();
	const auto starsPaid = FillSendMediaOptionFlags(
		action.options,
		peer,
		flags,
		sendFlags);

	auto &histories = history->owner().histories();
	histories.sendPreparedMessage(
		history,
		action.replyTo,
		randomId,
		Data::Histories::PrepareMessage<MTPmessages_SendMedia>(
			MTP_flags(sendFlags),
			peer->input(),
			Data::Histories::ReplyToPlaceholder(),
			std::move(inputMedia),
			MTPstring(),
			MTP_long(randomId),
			MTPReplyMarkup(),
			MTPvector<MTPMessageEntity>(),
			MTP_int(action.options.scheduled),
			MTP_int(action.options.scheduleRepeatPeriod),
			(sendAs ? sendAs->input() : MTP_inputPeerEmpty()),
			Data::ShortcutIdToMTP(session, action.options.shortcutId),
			MTP_long(action.options.effectId),
			MTP_long(starsPaid),
			SuggestToMTP(action.options.suggest)
		), [=](const MTPUpdates &result, const MTP::Response &response) {
	}, [=](const MTP::Error &error, const MTP::Response &response) {
		api->sendMessageFail(error, peer, randomId);
	});

	api->finishForwarding(action);
}

template <typename MediaData>
void SendExistingMedia(
		MessageToSend &&message,
		not_null<MediaData*> media,
		Fn<MTPInputMedia()> inputMedia,
		Data::FileOrigin origin,
		std::optional<MsgId> localMessageId) {
	const auto history = message.action.history;
	const auto peer = history->peer;
	const auto session = &history->session();
	const auto api = &session->api();
	const auto welcomeTemplate = message.action.options.welcomeTemplate;

	if (welcomeTemplate
		&& session->welcomeMessages().count(history)
			>= Data::WelcomeMessagesLimit(session)) {
		return;
	}

	message.action.clearDraft = false;
	message.action.generateLocal = true;
	api->sendAction(message.action);

	const auto newId = FullMsgId(
		peer->id,
		localMessageId
			? (*localMessageId)
			: session->data().nextLocalMessageId());
	const auto randomId = welcomeTemplate
		? uint64(0)
		: base::RandomValue<uint64>();
	auto &action = message.action;

	auto flags = NewMessageFlags(peer);
	auto sendFlags = MTPmessages_SendMedia::Flags(0);
	if (welcomeTemplate) {
		flags &= ~MessageFlag::Outgoing;
		flags |= MessageFlag::FakeHistoryItem;
	}
	if (action.replyTo) {
		flags |= MessageFlag::HasReplyInfo;
		sendFlags |= MTPmessages_SendMedia::Flag::f_reply_to;
	}
	if (!welcomeTemplate
		&& !action.options.scheduled
		&& !action.options.shortcutId
		&& session->ephemeralMessages().wouldSendMedia(
			peer,
			action.replyTo,
			message.textWithTags.text)) {
		flags |= MessageFlag::Ephemeral;
	}
	const auto silentPost = ShouldSendSilent(peer, action.options);
	InnerFillMessagePostFlags(action.options, peer, flags);
	if (silentPost) {
		sendFlags |= MTPmessages_SendMedia::Flag::f_silent;
	}
	const auto sendAs = action.options.sendAs;
	if (sendAs) {
		sendFlags |= MTPmessages_SendMedia::Flag::f_send_as;
	}
	auto caption = TextWithEntities{
		message.textWithTags.text,
		TextUtilities::ConvertTextTagsToEntities(message.textWithTags.tags)
	};
	TextUtilities::Trim(caption);
	auto sentEntities = EntitiesToMTP(
		session,
		caption.entities,
		ConvertOption::SkipLocal);
	if (!sentEntities.v.isEmpty()) {
		sendFlags |= MTPmessages_SendMedia::Flag::f_entities;
	}
	const auto captionText = caption.text;
	const auto starsPaid = FillSendMediaOptionFlags(
		action.options,
		peer,
		flags,
		sendFlags);

	const auto item = history->addNewLocalMessage({
		.id = newId.msg,
		.flags = flags,
		.from = NewMessageFromId(action),
		.replyTo = action.replyTo,
		.date = NewMessageDate(action.options),
		.scheduleRepeatPeriod = action.options.scheduleRepeatPeriod,
		.shortcutId = action.options.shortcutId,
		.starsPaid = starsPaid,
		.postAuthor = NewMessagePostAuthor(action),
		.effectId = action.options.effectId,
		.suggest = HistoryMessageSuggestInfo(action.options),
		.mediaSpoiler = action.options.mediaSpoiler,
	}, media, caption);

	if (welcomeTemplate) {
		auto &welcome = session->welcomeMessages();
		welcome.appendSending(item);
		welcome.sendMedia(item, inputMedia(), origin, inputMedia);
		api->finishForwarding(action);
		return;
	}

	session->data().registerMessageRandomId(randomId, newId);

	if (session->ephemeralMessages().sendMedia(
			item,
			inputMedia(),
			origin,
			inputMedia)) {
		api->finishForwarding(action);
		return;
	}

	const auto performRequest = [=](const auto &repeatRequest) -> void {
		auto &histories = history->owner().histories();
		const auto session = &history->session();
		const auto usedFileReference = media->fileReference();
		histories.sendPreparedMessage(
			history,
			action.replyTo,
			randomId,
			Data::Histories::PrepareMessage<MTPmessages_SendMedia>(
				MTP_flags(sendFlags),
				peer->input(),
				Data::Histories::ReplyToPlaceholder(),
				inputMedia(),
				MTP_string(captionText),
				MTP_long(randomId),
				MTPReplyMarkup(),
				sentEntities,
				MTP_int(action.options.scheduled),
				MTP_int(action.options.scheduleRepeatPeriod),
				(sendAs ? sendAs->input() : MTP_inputPeerEmpty()),
				Data::ShortcutIdToMTP(session, action.options.shortcutId),
				MTP_long(action.options.effectId),
				MTP_long(starsPaid),
				SuggestToMTP(action.options.suggest)
			), [=](const MTPUpdates &result, const MTP::Response &response) {
		}, [=](const MTP::Error &error, const MTP::Response &response) {
			if (error.code() == 400
				&& error.type().startsWith(u"FILE_REFERENCE_"_q)) {
				api->refreshFileReference(origin, [=](const auto &result) {
					if (media->fileReference() != usedFileReference) {
						repeatRequest(repeatRequest);
					} else {
						api->sendMessageFail(error, peer, randomId, newId);
					}
				});
			} else {
				api->sendMessageFail(error, peer, randomId, newId);
			}
		});
	};
	performRequest(performRequest);

	api->finishForwarding(action);
}

struct MusicSendRequestItem {
	MusicSelectionItem item;
	not_null<HistoryItem*> localItem;
	FullMsgId localId;
	uint64 randomId = 0;
	TextWithEntities caption;
};

struct MusicRefreshItem {
	not_null<DocumentData*> document;
	Data::FileOrigin origin;
	QByteArray usedFileReference;
};

struct MusicSelectionState {
	SendAction action;
	std::vector<MusicSelectionItem> items;
	TextWithEntities caption;
	int offset = 0;
};

[[nodiscard]] MTPInputMedia PrepareMusicInputMedia(
		const MusicSelectionItem &item,
		bool spoiler) {
	using Flag = MTPDinputMediaDocument::Flag;
	return MTP_inputMediaDocument(
		MTP_flags(spoiler
			? Flag::f_spoiler
			: Flag(0)),
		item.document->mtpInput(),
		MTPInputPhoto(), // video_cover
		MTPint(), // ttl_seconds
		MTPint(), // video_timestamp
		MTPstring()); // query
}

[[nodiscard]] MTPInputSingleMedia PrepareMusicInputSingleMedia(
		not_null<Main::Session*> session,
		const MusicSendRequestItem &item,
		bool spoiler) {
	const auto entities = EntitiesToMTP(
		session,
		item.caption.entities,
		ConvertOption::SkipLocal);
	using Flag = MTPDinputSingleMedia::Flag;
	return MTP_inputSingleMedia(
		MTP_flags(!entities.v.isEmpty()
			? Flag::f_entities
			: Flag(0)),
		PrepareMusicInputMedia(item.item, spoiler),
		MTP_long(item.randomId),
		MTP_string(item.caption.text),
		entities);
}

void SendMusicSelectionBatch(
		SendAction &action,
		std::vector<MusicSelectionItem> items,
		TextWithEntities caption,
		Fn<void()> done) {
	Expects(!items.empty());

	const auto history = action.history;
	const auto peer = history->peer;
	const auto session = &history->session();
	const auto api = &session->api();
	const auto actionPtr = &action;
	const auto multi = (items.size() > 1);
	const auto groupId = multi ? base::RandomValue<uint64>() : uint64(0);

	auto flags = NewMessageFlags(peer);
	if (action.replyTo) {
		flags |= MessageFlag::HasReplyInfo;
	}
	if (!multi
		&& !action.options.scheduled
		&& !action.options.shortcutId
		&& session->ephemeralMessages().isEphemeralBotReply(
			action.replyTo.messageId)) {
		flags |= MessageFlag::Ephemeral;
	}
	InnerFillMessagePostFlags(action.options, peer, flags);
	if (action.options.scheduled) {
		flags |= MessageFlag::IsOrWasScheduled;
	}
	if (action.options.shortcutId) {
		flags |= MessageFlag::ShortcutMessage;
	}
	if (action.options.invertCaption) {
		flags |= MessageFlag::InvertMedia;
	}

	auto batchStarsPaid = 0;
	auto remainingStarsApproved = action.options.starsApproved;
	auto requests = std::vector<MusicSendRequestItem>();
	requests.reserve(items.size());
	for (auto i = 0; i != int(items.size()); ++i) {
		auto itemCaption = (i + 1 == int(items.size()))
			? caption
			: TextWithEntities();
		const auto newId = FullMsgId(
			peer->id,
			session->data().nextLocalMessageId());
		const auto randomId = base::RandomValue<uint64>();
		const auto messageStarsPaid = std::min(
			peer->starsPerMessageChecked(),
			remainingStarsApproved);
		remainingStarsApproved -= messageStarsPaid;
		batchStarsPaid += messageStarsPaid;

		session->data().registerMessageRandomId(randomId, newId);
		const auto localItem = history->addNewLocalMessage({
			.id = newId.msg,
			.flags = flags,
			.from = NewMessageFromId(action),
			.replyTo = action.replyTo,
			.date = NewMessageDate(action.options),
			.scheduleRepeatPeriod = action.options.scheduleRepeatPeriod,
			.shortcutId = action.options.shortcutId,
			.starsPaid = messageStarsPaid,
			.postAuthor = NewMessagePostAuthor(action),
			.groupedId = groupId,
			.effectId = action.options.effectId,
			.suggest = HistoryMessageSuggestInfo(action.options),
			.mediaSpoiler = action.options.mediaSpoiler,
		}, items[i].document, itemCaption);
		requests.push_back({
			.item = std::move(items[i]),
			.localItem = localItem,
			.localId = newId,
			.randomId = randomId,
			.caption = std::move(itemCaption),
		});
	}

	const auto failRequest = [=](const MTP::Error &error) {
		for (const auto &item : requests) {
			api->sendMessageFail(error, peer, item.randomId, item.localId);
		}
		if (done) {
			done();
		}
	};

	const auto refreshItems = [=] {
		auto result = std::vector<MusicRefreshItem>();
		result.reserve(requests.size());
		for (const auto &item : requests) {
			if (!item.item.origin) {
				continue;
			}
			result.push_back({
				.document = item.item.document,
				.origin = item.item.origin,
				.usedFileReference = item.item.document->fileReference(),
			});
		}
		return result;
	}();
	if (!multi) {
		const auto &item = requests.front();
		if (session->ephemeralMessages().sendMedia(
				item.localItem,
				PrepareMusicInputMedia(
					item.item,
					action.options.mediaSpoiler))) {
			if (done) {
				done();
			}
			return;
		}
	}

	const auto performRequest = [=](const auto &repeatRequest, bool refreshed)
			-> void {
		const auto sendAs = action.options.sendAs;
		const auto retryOrFail = [=](
				const MTP::Error &error,
				const MTP::Response &response) {
			if (refreshed
				|| (error.code() != 400)
				|| !error.type().startsWith(u"FILE_REFERENCE_"_q)
				|| refreshItems.empty()) {
				failRequest(error);
				return;
			}
			const auto changed = std::make_shared<bool>(false);
			const auto left = std::make_shared<int>(int(refreshItems.size()));
			for (const auto &refresh : refreshItems) {
				api->refreshFileReference(refresh.origin, [=](const auto &) {
					*changed = *changed
						|| (refresh.document->fileReference()
							!= refresh.usedFileReference);
					if (!--*left) {
						if (*changed) {
							repeatRequest(repeatRequest, true);
						} else {
							failRequest(error);
						}
					}
				});
			}
		};
		if (!multi) {
			const auto &item = requests.front();
			const auto inputMedia = PrepareMusicInputMedia(
				item.item,
				action.options.mediaSpoiler);
			const auto entities = EntitiesToMTP(
				session,
				item.caption.entities,
				ConvertOption::SkipLocal);
			auto sendFlags = MTPmessages_SendMedia::Flags(0);
			if (action.replyTo) {
				sendFlags |= MTPmessages_SendMedia::Flag::f_reply_to;
			}
			if (ShouldSendSilent(peer, action.options)) {
				sendFlags |= MTPmessages_SendMedia::Flag::f_silent;
			}
			if (!entities.v.isEmpty()) {
				sendFlags |= MTPmessages_SendMedia::Flag::f_entities;
			}
			if (action.options.scheduled) {
				sendFlags |= MTPmessages_SendMedia::Flag::f_schedule_date;
				if (action.options.scheduleRepeatPeriod) {
					sendFlags |= MTPmessages_SendMedia::Flag::f_schedule_repeat_period;
				}
			}
			if (action.options.shortcutId) {
				sendFlags |= MTPmessages_SendMedia::Flag::f_quick_reply_shortcut;
			}
			if (sendAs) {
				sendFlags |= MTPmessages_SendMedia::Flag::f_send_as;
			}
			if (action.options.effectId) {
				sendFlags |= MTPmessages_SendMedia::Flag::f_effect;
			}
			if (action.options.suggest) {
				sendFlags |= MTPmessages_SendMedia::Flag::f_suggested_post;
			}
			if (action.options.invertCaption) {
				sendFlags |= MTPmessages_SendMedia::Flag::f_invert_media;
			}
			if (batchStarsPaid) {
				sendFlags |= MTPmessages_SendMedia::Flag::f_allow_paid_stars;
			}

			auto &histories = history->owner().histories();
			histories.sendPreparedMessage(
				history,
				action.replyTo,
				item.randomId,
				Data::Histories::PrepareMessage<MTPmessages_SendMedia>(
					MTP_flags(sendFlags),
					peer->input(),
					Data::Histories::ReplyToPlaceholder(),
					(action.options.price
						? MTPInputMedia(MTP_inputMediaPaidMedia(
							MTP_flags(0),
							MTP_long(action.options.price),
							MTP_vector<MTPInputMedia>(1, inputMedia),
							MTPstring()))
						: inputMedia),
					MTP_string(item.caption.text),
					MTP_long(item.randomId),
					MTPReplyMarkup(),
					entities,
					MTP_int(action.options.scheduled),
					MTP_int(action.options.scheduleRepeatPeriod),
					(sendAs ? sendAs->input() : MTP_inputPeerEmpty()),
					Data::ShortcutIdToMTP(session, action.options.shortcutId),
					MTP_long(action.options.effectId),
					MTP_long(batchStarsPaid),
					SuggestToMTP(action.options.suggest)
				), [=](const MTPUpdates &result, const MTP::Response &response) {
				actionPtr->options.starsApproved -= batchStarsPaid;
				if (done) {
					done();
				}
			}, retryOrFail);
			return;
		}

		using Flag = MTPmessages_SendMultiMedia::Flag;
		const auto sendFlags = Flag(0)
			| (action.replyTo ? Flag::f_reply_to : Flag(0))
			| (ShouldSendSilent(peer, action.options)
				? Flag::f_silent
				: Flag(0))
			| (action.options.scheduled
				? Flag::f_schedule_date
				: Flag(0))
			| (sendAs ? Flag::f_send_as : Flag(0))
			| (action.options.shortcutId
				? Flag::f_quick_reply_shortcut
				: Flag(0))
			| (action.options.effectId ? Flag::f_effect : Flag(0))
			| (action.options.invertCaption
				? Flag::f_invert_media
				: Flag(0))
			| (batchStarsPaid ? Flag::f_allow_paid_stars : Flag(0));

		auto media = QVector<MTPInputSingleMedia>();
		media.reserve(requests.size());
		for (const auto &item : requests) {
			media.push_back(PrepareMusicInputSingleMedia(
				session,
				item,
				action.options.mediaSpoiler));
		}

		auto &histories = history->owner().histories();
		histories.sendPreparedMessage(
			history,
			action.replyTo,
			uint64(0),
			Data::Histories::PrepareMessage<MTPmessages_SendMultiMedia>(
				MTP_flags(sendFlags),
				peer->input(),
				Data::Histories::ReplyToPlaceholder(),
				MTP_vector<MTPInputSingleMedia>(std::move(media)),
				MTP_int(action.options.scheduled),
				(sendAs ? sendAs->input() : MTP_inputPeerEmpty()),
				Data::ShortcutIdToMTP(session, action.options.shortcutId),
				MTP_long(action.options.effectId),
				MTP_long(batchStarsPaid)
			), [=](const MTPUpdates &result, const MTP::Response &response) {
			actionPtr->options.starsApproved -= batchStarsPaid;
			if (done) {
				done();
			}
		}, retryOrFail);
	};
	performRequest(performRequest, false);
}

} // namespace

void SendExistingDocument(
		MessageToSend &&message,
		not_null<DocumentData*> document,
		std::optional<MsgId> localMessageId) {
	SendExistingDocument(
		std::move(message),
		document,
		document->stickerOrGifOrigin(),
		std::move(localMessageId));
}

void SendExistingDocument(
		MessageToSend &&message,
		not_null<DocumentData*> document,
		Data::FileOrigin origin,
		std::optional<MsgId> localMessageId) {
	const auto inputMedia = [=] {
		using Flag = MTPDinputMediaDocument::Flag;
		return MTP_inputMediaDocument(
			MTP_flags(message.action.options.mediaSpoiler
				? Flag::f_spoiler
				: Flag(0)),
			document->mtpInput(),
			MTPInputPhoto(), // video_cover
			MTPint(), // ttl_seconds
			MTPint(), // video_timestamp
			MTPstring()); // query
	};
	SendExistingMedia(
		std::move(message),
		document,
		inputMedia,
		origin ? origin : document->stickerOrGifOrigin(),
		std::move(localMessageId));

	if (document->sticker()) {
		document->owner().stickers().incrementSticker(document);
	}
}

void SendMusicSelection(
		MessageToSend &&message,
		std::vector<MusicSelectionItem> items) {
	if (items.empty()) {
		return;
	}

	auto caption = TextWithEntities{
		std::move(message.textWithTags.text),
		TextUtilities::ConvertTextTagsToEntities(message.textWithTags.tags)
	};
	TextUtilities::Trim(caption);

	const auto api = &message.action.history->session().api();

	message.action.clearDraft = false;
	message.action.generateLocal = true;
	api->sendAction(message.action);

	const auto state = std::make_shared<MusicSelectionState>(MusicSelectionState{
		.action = message.action,
		.items = std::move(items),
		.caption = std::move(caption),
	});
	const auto sendNext = [=](const auto &self) -> void {
		const auto count = int(state->items.size());
		if (state->offset == count) {
			api->finishForwarding(state->action);
			return;
		}
		const auto optionsRequireSingle = state->action.options.price
			|| state->action.options.scheduleRepeatPeriod
			|| state->action.options.suggest;
		const auto batchLimit = optionsRequireSingle
			? 1
			: Ui::MaxAlbumItems();
		const auto till = std::min(state->offset + batchLimit, count);
		auto batch = std::vector<MusicSelectionItem>();
		batch.reserve(till - state->offset);
		for (; state->offset != till; ++state->offset) {
			batch.push_back(std::move(state->items[state->offset]));
		}
		auto batchCaption = TextWithEntities();
		if (state->offset == count) {
			batchCaption = std::move(state->caption);
		}
		SendMusicSelectionBatch(
			state->action,
			std::move(batch),
			std::move(batchCaption),
			[=] { self(self); });
	};
	sendNext(sendNext);
}

void SendExistingPhoto(
		MessageToSend &&message,
		not_null<PhotoData*> photo,
		std::optional<MsgId> localMessageId) {
	SendExistingPhoto(
		std::move(message),
		photo,
		Data::FileOrigin(),
		std::move(localMessageId));
}

void SendExistingPhoto(
		MessageToSend &&message,
		not_null<PhotoData*> photo,
		Data::FileOrigin origin,
		std::optional<MsgId> localMessageId) {
	const auto inputMedia = [=] {
		using Flag = MTPDinputMediaPhoto::Flag;
		return MTP_inputMediaPhoto(
			MTP_flags(message.action.options.mediaSpoiler
				? Flag::f_spoiler
				: Flag(0)),
			photo->mtpInput(),
			MTPint(), // ttl_seconds
			MTPInputDocument()); // video
	};
	SendExistingMedia(
		std::move(message),
		photo,
		inputMedia,
		origin,
		std::move(localMessageId));
}

bool ItemForbidsForwardRaw(not_null<const HistoryItem*> item) {
	if (!MakuGram::Settings::instance().bypassRestrictedContent()) {
		return false;
	}
	if (item->forbidsForwardRaw()) {
		return true;
	}
	const auto peer = item->history()->peer;
	if (const auto channel = peer->asChannel()) {
		if (channel->flags() & ChannelData::Flag::NoForwards) {
			return true;
		}
	} else if (const auto chat = peer->asChat()) {
		if (chat->flags() & ChatData::Flag::NoForwards) {
			return true;
		}
	} else if (const auto user = peer->asUser()) {
		if (!user->allowsForwarding()) {
			return true;
		}
	}
	return false;
}

namespace {

QString ExtractItemAuthorName(not_null<const HistoryItem*> item) {
	QString authorName;
	if (const auto forwarded = item->Get<HistoryMessageForwarded>()) {
		if (forwarded->originalSender) {
			authorName = forwarded->originalSender->name();
			if (!forwarded->originalSender->username().isEmpty()) {
				authorName += u" (@"_q + forwarded->originalSender->username() + u")"_q;
			}
		} else if (forwarded->originalHiddenSenderInfo) {
			authorName = forwarded->originalHiddenSenderInfo->name;
		} else if (!forwarded->originalPostAuthor.isEmpty()) {
			authorName = forwarded->originalPostAuthor;
		}
	}
	if (authorName.isEmpty()) {
		if (const auto sender = item->originalSender()) {
			authorName = sender->name();
			if (!sender->username().isEmpty()) {
				authorName += u" (@"_q + sender->username() + u")"_q;
			}
		} else if (const auto hidden = item->originalHiddenSenderInfo()) {
			authorName = hidden->name;
		}
	}
	if (authorName.isEmpty() && !item->originalPostAuthor().isEmpty()) {
		authorName = item->originalPostAuthor();
	}
	if (authorName.isEmpty()) {
		const auto authorPeer = item->author();
		authorName = authorPeer->name();
		if (!authorPeer->username().isEmpty()) {
			authorName += u" (@"_q + authorPeer->username() + u")"_q;
		}
	}
	if (authorName.isEmpty() && item->from()) {
		authorName = item->from()->name();
		if (!item->from()->username().isEmpty()) {
			authorName += u" (@"_q + item->from()->username() + u")"_q;
		}
	}
	if (authorName.isEmpty() && item->history() && item->history()->peer) {
		authorName = item->history()->peer->name();
		if (!item->history()->peer->username().isEmpty()) {
			authorName += u" (@"_q + item->history()->peer->username() + u")"_q;
		}
	}
	return authorName;
}

} // namespace

void SendCopiedMessages(
		SendAction action,
		const std::vector<not_null<HistoryItem*>> &items,
		Data::ForwardOptions forwardOptions) {
	if (items.empty()) {
		return;
	}
	auto ordered = items;
	ranges::sort(ordered, ranges::less(), &HistoryItem::position);

	const auto session = &action.history->session();
	const auto tempDir = session->local().tempDirectory();
	QDir().mkpath(tempDir);

	struct CopiedMediaItem {
		MsgId id = 0;
		FullMsgId fullId;
		MessageGroupId groupId;
		TextWithEntities originalText;
		QString authorName;
		PhotoData *photo = nullptr;
		std::shared_ptr<Data::PhotoMedia> photoView;
		QString photoPath;
		DocumentData *document = nullptr;
		QString docPath;
		bool isVideo = false;
		bool isAudio = false;
		bool spoiler = false;
		bool invertMedia = false;
	};

	std::vector<CopiedMediaItem> itemsToCopy;
	itemsToCopy.reserve(ordered.size());

	for (const auto &item : ordered) {
		CopiedMediaItem copyItem;
		copyItem.id = item->id;
		copyItem.fullId = item->fullId();
		copyItem.groupId = item->groupId();
		copyItem.originalText = item->originalText();
		copyItem.authorName = ExtractItemAuthorName(item);
		copyItem.invertMedia = item->invertMedia();

		if (const auto media = item->media()) {
			copyItem.spoiler = media->hasSpoiler();
		if (const auto photo = media->photo()) {
				copyItem.photo = photo;
				copyItem.photoView = photo->createMediaView();
				copyItem.photoPath = QDir(tempDir).filePath(
					u"makugram_p_"_q + QString::number(photo->id) + u".jpg"_q);
			} else if (const auto doc = media->document()) {
				copyItem.document = doc;
				copyItem.isVideo = doc->isVideoFile()
					|| doc->isAnimation()
					|| doc->isGifv()
					|| doc->mimeString().contains(u"video"_q);
				copyItem.isAudio = doc->isSong() || doc->isAudioFile();

				auto filename = doc->filename();
				if (filename.isEmpty() || !filename.contains('.')) {
					if (copyItem.isVideo) {
						filename = filename.isEmpty() ? u"video.mp4"_q : (filename + u".mp4"_q);
					} else if (copyItem.isAudio) {
						filename = filename.isEmpty() ? u"audio.mp3"_q : (filename + u".mp3"_q);
					} else if (doc->isVoiceMessage()) {
						filename = filename.isEmpty() ? u"audio.ogg"_q : (filename + u".ogg"_q);
					} else if (doc->isImage()) {
						filename = filename.isEmpty() ? u"image.jpg"_q : (filename + u".jpg"_q);
					} else if (filename.isEmpty()) {
						filename = u"file.dat"_q;
					}
				}
				copyItem.docPath = QDir(tempDir).filePath(
					u"makugram_d_"_q + QString::number(doc->id) + u"_"_q + filename);
			}
		}
		itemsToCopy.push_back(std::move(copyItem));
	}

	for (auto &copyItem : itemsToCopy) {
		if (copyItem.photo && copyItem.photoView) {
			copyItem.photoView->wanted(Data::PhotoSize::Large, copyItem.fullId);
		} else if (copyItem.document) {
			const auto doc = copyItem.document;
			const auto filepath = doc->filepath(true);
			if (!filepath.isEmpty() && QFileInfo::exists(filepath)) {
				if (filepath != copyItem.docPath) {
					if (!QFileInfo::exists(copyItem.docPath) || QFileInfo(copyItem.docPath).size() == 0) {
						QFile::copy(filepath, copyItem.docPath);
					}
				}
			} else {
				if (!QFileInfo::exists(copyItem.docPath) || (doc->size > 0 && QFileInfo(copyItem.docPath).size() < doc->size)) {
					doc->save(Data::FileOrigin(copyItem.fullId), copyItem.docPath);
				}
			}
		}
	}

	struct CopyContext {
		SendAction action;
		Data::ForwardOptions forwardOptions;
		std::vector<CopiedMediaItem> itemsToCopy;
		QString tempDir;
		bool finished = false;
		int lastPercent = -1;
		base::Timer timer;
		rpl::lifetime lifetime;

		CopyContext(
			SendAction action,
			Data::ForwardOptions forwardOptions,
			std::vector<CopiedMediaItem> &&itemsToCopy,
			QString tempDir)
		: action(action)
		, forwardOptions(forwardOptions)
		, itemsToCopy(std::move(itemsToCopy))
		, tempDir(std::move(tempDir)) {
		}
	};

	const auto ctx = std::make_shared<CopyContext>(
		action,
		forwardOptions,
		std::move(itemsToCopy),
		tempDir);

	const auto computeCaption = [ctx](const CopiedMediaItem &item, bool includeAuthor) -> TextWithTags {
		const auto session = &ctx->action.history->session();
		const auto captionLimit = Data::PremiumLimits(session).captionLengthCurrent();

		auto caption = (ctx->forwardOptions != Data::ForwardOptions::NoNamesAndCaptions)
			? item.originalText
			: TextWithEntities();

		const auto dropAuthor = (ctx->forwardOptions == Data::ForwardOptions::NoSenderNames)
			|| (ctx->forwardOptions == Data::ForwardOptions::NoNamesAndCaptions);

		if (includeAuthor && !dropAuthor && !item.authorName.isEmpty()) {
			const auto prefix = caption.text.isEmpty() ? QString() : QString("\n\n");
			const auto authorLine = u"\u0410\u0432\u0442\u043e\u0440: "_q + item.authorName;
			const auto neededLength = prefix.size() + authorLine.size();

			const auto hasMedia = (item.photo != nullptr) || (item.document != nullptr);
			if (hasMedia && (caption.text.size() + neededLength > captionLimit)) {
				const auto maxOrigLength = std::max(0, int(captionLimit - neededLength));
				caption.text.truncate(maxOrigLength);
				for (auto it = caption.entities.begin(); it != caption.entities.end();) {
					if (it->offset() >= maxOrigLength) {
						it = caption.entities.erase(it);
					} else {
						if (it->offset() + it->length() > maxOrigLength) {
							*it = EntityInText(
								it->type(),
								it->offset(),
								maxOrigLength - it->offset(),
								it->data());
						}
						++it;
					}
				}
			}

			const auto actualPrefix = caption.text.isEmpty() ? QString() : prefix;
			const auto start = caption.text.size() + actualPrefix.size();
			caption.text += actualPrefix + authorLine;
			caption.entities.push_back(EntityInText(
				EntityType::Italic,
				start,
				authorLine.size()));
		}

		return TextWithTags{
			caption.text,
			TextUtilities::ConvertEntitiesToTextTags(caption.entities),
		};
	};

	const auto preparePhoto = [ctx](const CopiedMediaItem &item) -> std::optional<Ui::PreparedFile> {
		if (!item.photo) {
			return std::nullopt;
		}
		const auto session = &ctx->action.history->session();
		const auto premium = session->premium();
		if (item.photoView) {
			if (!QFileInfo::exists(item.photoPath) || QFileInfo(item.photoPath).size() == 0) {
				item.photoView->saveToFile(item.photoPath);
			}
		}
		if (QFileInfo::exists(item.photoPath) && QFileInfo(item.photoPath).size() > 0) {
			auto list = Storage::PrepareMediaList(
				QStringList{ item.photoPath },
				st::sendMediaPreviewSize,
				premium);
			if (!list.files.empty()) {
				return std::move(list.files.front());
			}
		}
		if (item.photoView) {
			for (const auto size : { Data::PhotoSize::Large, Data::PhotoSize::Thumbnail, Data::PhotoSize::Small }) {
				const auto bytes = item.photoView->imageBytes(size);
				if (!bytes.isEmpty()) {
					QFile f(item.photoPath);
					if (f.open(QIODevice::WriteOnly) && (f.write(bytes) == bytes.size())) {
						f.close();
						auto list = Storage::PrepareMediaList(
							QStringList{ item.photoPath },
							st::sendMediaPreviewSize,
							premium);
						if (!list.files.empty()) {
							return std::move(list.files.front());
						}
					}
				}
				if (const auto img = item.photoView->image(size)) {
					auto original = img->original();
					if (!original.isNull()) {
						if (original.save(item.photoPath, "JPG", 92)) {
							auto list = Storage::PrepareMediaList(
								QStringList{ item.photoPath },
								st::sendMediaPreviewSize,
								premium);
							if (!list.files.empty()) {
								return std::move(list.files.front());
							}
						}
					}
				}
			}
		}
		return std::nullopt;
	};

	const auto prepareDoc = [ctx](const CopiedMediaItem &item) -> std::optional<Ui::PreparedFile> {
		if (!item.document) {
			return std::nullopt;
		}
		const auto session = &ctx->action.history->session();
		const auto premium = session->premium();
		auto filepath = item.docPath;
		if (!QFileInfo::exists(filepath) || QFileInfo(filepath).size() == 0) {
			const auto cached = item.document->filepath(true);
			if (!cached.isEmpty() && QFileInfo::exists(cached)) {
				QFile::copy(cached, filepath);
				if (!QFileInfo::exists(filepath) || QFileInfo(filepath).size() == 0) {
					filepath = cached;
				}
			}
		}
		if (QFileInfo::exists(filepath) && QFileInfo(filepath).size() > 0) {
			auto list = Storage::PrepareMediaList(
				QStringList{ filepath },
				st::sendMediaPreviewSize,
				premium);
			if (!list.files.empty()) {
				return std::move(list.files.front());
			}
		}
		return std::nullopt;
	};

	const auto isAlbumItem = [](const CopiedMediaItem &item) -> bool {
		if (item.photo) {
			return true;
		}
		if (item.document) {
			return item.isVideo;
		}
		return false;
	};

	const auto doSend = [ctx, computeCaption, preparePhoto, prepareDoc, isAlbumItem] {
		const auto session = &ctx->action.history->session();
		auto &api = session->api();
		const auto count = int(ctx->itemsToCopy.size());

		for (auto i = 0; i < count;) {
			const auto &first = ctx->itemsToCopy[i];
			const auto groupId = first.groupId;
			auto j = i + 1;
			if (!groupId.empty() && isAlbumItem(first)) {
				while (j < count
					&& ctx->itemsToCopy[j].groupId == groupId
					&& isAlbumItem(ctx->itemsToCopy[j])
					&& (j - i < Ui::MaxAlbumItems())) {
					++j;
				}
			}

			if (j - i > 1) {
				auto captionIndex = -1;
				for (auto k = i; k < j; ++k) {
					if (!ctx->itemsToCopy[k].originalText.text.isEmpty()) {
						captionIndex = k;
						break;
					}
				}
				if (captionIndex == -1) {
					captionIndex = j - 1;
				}

				Ui::PreparedList albumList;
				auto allOk = true;
				for (auto k = i; k < j; ++k) {
					const auto &copyItem = ctx->itemsToCopy[k];
					std::optional<Ui::PreparedFile> prepared;
					if (copyItem.photo) {
						prepared = preparePhoto(copyItem);
					} else if (copyItem.document) {
						prepared = prepareDoc(copyItem);
					}
					if (!prepared) {
						allOk = false;
						break;
					}
					auto file = std::move(*prepared);
					file.caption = computeCaption(copyItem, (k == captionIndex));
					file.spoiler = copyItem.spoiler;
					if (copyItem.photo) {
						file.sendLargePhotos = true;
					}
					albumList.files.push_back(std::move(file));
				}

				if (allOk && !albumList.files.empty()) {
					auto itemAction = ctx->action;
					itemAction.clearDraft = false;
					itemAction.generateLocal = true;

					Ui::SendFilesWay way;
					way.setGroupFiles(true);
					way.setSendImagesAsPhotos(true);
					way.setSendLargePhotos(true);

					auto groups = Ui::DivideByGroups(std::move(albumList), way, false);
					for (auto &group : groups) {
						const auto album = (group.type != Ui::AlbumType::None)
							? std::make_shared<SendingAlbum>()
							: nullptr;
						const auto type = (group.type == Ui::AlbumType::Music)
							? SendMediaType::Audio
							: (group.type == Ui::AlbumType::File)
							? SendMediaType::File
							: SendMediaType::Photo;
						api.sendFiles(std::move(group.list), type, album, itemAction);
					}
					i = j;
					continue;
				}
			}

			const auto &copyItem = ctx->itemsToCopy[i];
			auto itemAction = ctx->action;
			itemAction.clearDraft = false;
			itemAction.generateLocal = true;
			itemAction.options.mediaSpoiler = copyItem.spoiler;
			itemAction.options.invertCaption = copyItem.invertMedia;

			const auto captionTags = computeCaption(copyItem, true);

			auto message = MessageToSend(itemAction);
			message.textWithTags = captionTags;

			if (copyItem.photo) {
				if (auto prepared = preparePhoto(copyItem)) {
					Ui::PreparedList singleList;
					auto file = std::move(*prepared);
					file.caption = captionTags;
					file.spoiler = itemAction.options.mediaSpoiler;
					file.sendLargePhotos = true;
					singleList.files.push_back(std::move(file));
					api.sendFiles(
						std::move(singleList),
						SendMediaType::Photo,
						nullptr,
						itemAction);
					++i;
					continue;
				}
				api.sendMessage(std::move(message));
			} else if (copyItem.document) {
				if (auto prepared = prepareDoc(copyItem)) {
					Ui::PreparedList singleList;
					auto file = std::move(*prepared);
					file.caption = captionTags;
					file.spoiler = itemAction.options.mediaSpoiler;
					singleList.files.push_back(std::move(file));
					const auto sendType = copyItem.isVideo
						? SendMediaType::Photo
						: copyItem.isAudio
						? SendMediaType::Audio
						: SendMediaType::File;
					api.sendFiles(
						std::move(singleList),
						sendType,
						nullptr,
						itemAction);
					++i;
					continue;
				}
				api.sendMessage(std::move(message));
			} else {
				api.sendMessage(std::move(message));
			}
			++i;
		}
	};

	const auto isItemReady = [ctx](const CopiedMediaItem &item) {
		if (item.photo) {
			if (QFileInfo::exists(item.photoPath) && QFileInfo(item.photoPath).size() > 0) {
				return true;
			}
			if (item.photoView && item.photoView->loaded()) {
				return true;
			}
			if (item.photo->failed(Data::PhotoSize::Large)) {
				return true;
			}
			return false;
		}
		if (item.document) {
			if (QFileInfo::exists(item.docPath) && (item.document->size == 0 || QFileInfo(item.docPath).size() >= item.document->size)) {
				return true;
			}
			const auto filepath = item.document->filepath(true);
			if (!filepath.isEmpty() && QFileInfo::exists(filepath)) {
				return true;
			}
			if (item.document->status == FileDownloadFailed || item.document->cancelled()) {
				return true;
			}
			return false;
		}
		return true;
	};

	const auto allReady = [ctx, isItemReady] {
		for (const auto &item : ctx->itemsToCopy) {
			if (!isItemReady(item)) {
				return false;
			}
		}
		return true;
	};

	const auto computeProgress = [ctx, isItemReady] {
		auto total = 0.0;
		auto count = 0;
		for (const auto &item : ctx->itemsToCopy) {
			if (item.photo) {
				count++;
				if (isItemReady(item)) {
					total += 1.0;
				} else if (item.photo->loading()) {
					total += std::clamp(item.photo->progress(), 0.0, 1.0);
				}
			} else if (item.document) {
				count++;
				if (isItemReady(item)) {
					total += 1.0;
				} else if (item.document->size > 0 && QFileInfo::exists(item.docPath)) {
					total += std::clamp(double(QFileInfo(item.docPath).size()) / double(item.document->size), 0.0, 1.0);
				} else if (item.document->loading()) {
					total += std::clamp(item.document->progress(), 0.0, 1.0);
				}
			}
		}
		return count > 0 ? (total / count) : 1.0;
	};

	if (allReady()) {
		doSend();
	} else {
		const auto showToast = [ctx](const QString &text) {
			if (const auto window = Core::App().windowFor(ctx->action.history->peer)) {
				window->showToast(text);
			} else if (const auto window = Core::App().windowFor(not_null(&ctx->action.history->session().account()))) {
				window->showToast(text);
			}
		};

		const auto updateToast = [ctx, computeProgress, showToast] {
			const auto progress = computeProgress();
			const auto percent = int(std::clamp(progress * 100.0, 0.0, 100.0));
			if (percent != ctx->lastPercent) {
				ctx->lastPercent = percent;
				showToast(u"\u0417\u0430\u0433\u0440\u0443\u0437\u043a\u0430 \u043c\u0435\u0434\u0438\u0430: %1%"_q.arg(percent));
			}
		};

		const auto finish = [weak = std::weak_ptr(ctx), doSend] {
			const auto strong = weak.lock();
			if (!strong || strong->finished) {
				return;
			}
			strong->finished = true;
			strong->timer.cancel();
			strong->timer.setCallback(nullptr);
			strong->lifetime.destroy();
			doSend();
		};

		ctx->timer.setCallback([allReady, finish, updateToast] {
			if (allReady()) {
				finish();
			} else {
				updateToast();
			}
		});
		ctx->timer.callEach(100);

		session->downloaderTaskFinished(
		) | rpl::on_next([allReady, finish, updateToast] {
			if (allReady()) {
				finish();
			} else {
				updateToast();
			}
		}, ctx->lifetime);

		base::call_delayed(45000, session, [finish] {
			finish();
		});

		updateToast();
	}
}

bool SendDice(MessageToSend &message) {
	const auto full = QStringView(message.textWithTags.text).trimmed();
	auto length = 0;
	if (!Ui::Emoji::Find(full.data(), full.data() + full.size(), &length)
		|| length != full.size()
		|| !message.textWithTags.tags.isEmpty()) {
		return false;
	}
	const auto &config = message.action.history->session().appConfig();
	static const auto hardcoded = std::vector<QString>{
		Stickers::DicePacks::kDiceString,
		Stickers::DicePacks::kDartString,
		Stickers::DicePacks::kSlotString,
		Stickers::DicePacks::kFballString,
		Stickers::DicePacks::kFballString + QChar(0xFE0F),
		Stickers::DicePacks::kBballString,
	};
	const auto list = config.get<std::vector<QString>>(
		"emojies_send_dice",
		hardcoded);
	const auto emoji = full.toString();
	if (!ranges::contains(list, emoji)) {
		return false;
	}
	const auto history = message.action.history;
	const auto peer = history->peer;
	const auto session = &history->session();
	const auto api = &session->api();

	message.textWithTags = TextWithTags();
	message.action.clearDraft = false;
	message.action.generateLocal = true;

	auto &action = message.action;
	api->sendAction(action);

	const auto newId = FullMsgId(
		peer->id,
		session->data().nextLocalMessageId());
	const auto randomId = base::RandomValue<uint64>();

	auto &histories = history->owner().histories();
	auto flags = NewMessageFlags(peer);
	auto sendFlags = MTPmessages_SendMedia::Flags(0);
	if (action.replyTo) {
		flags |= MessageFlag::HasReplyInfo;
		sendFlags |= MTPmessages_SendMedia::Flag::f_reply_to;
	}
	const auto silentPost = ShouldSendSilent(peer, action.options);
	InnerFillMessagePostFlags(action.options, peer, flags);
	if (silentPost) {
		sendFlags |= MTPmessages_SendMedia::Flag::f_silent;
	}
	const auto sendAs = action.options.sendAs;
	if (sendAs) {
		sendFlags |= MTPmessages_SendMedia::Flag::f_send_as;
	}
	if (action.options.scheduled) {
		flags |= MessageFlag::IsOrWasScheduled;
		sendFlags |= MTPmessages_SendMedia::Flag::f_schedule_date;
		if (action.options.scheduleRepeatPeriod) {
			sendFlags |= MTPmessages_SendMedia::Flag::f_schedule_repeat_period;
		}
	}
	if (action.options.shortcutId) {
		flags |= MessageFlag::ShortcutMessage;
		sendFlags |= MTPmessages_SendMedia::Flag::f_quick_reply_shortcut;
	}
	if (action.options.effectId) {
		sendFlags |= MTPmessages_SendMedia::Flag::f_effect;
	}
	if (action.options.suggest) {
		sendFlags |= MTPmessages_SendMedia::Flag::f_suggested_post;
	}
	if (action.options.invertCaption) {
		flags |= MessageFlag::InvertMedia;
		sendFlags |= MTPmessages_SendMedia::Flag::f_invert_media;
	}
	const auto starsPaid = std::min(
		peer->starsPerMessageChecked(),
		action.options.starsApproved);
	if (starsPaid) {
		action.options.starsApproved -= starsPaid;
		sendFlags |= MTPmessages_SendMedia::Flag::f_allow_paid_stars;
	}

	session->data().registerMessageRandomId(randomId, newId);

	auto seed = QByteArray(32, Qt::Uninitialized);
	base::RandomFill(bytes::make_detached_span(seed));
	const auto stake = action.options.stakeSeedHash.isEmpty()
		? 0
		: action.options.stakeNanoTon;
	history->addNewLocalMessage({
		.id = newId.msg,
		.flags = flags,
		.from = NewMessageFromId(action),
		.replyTo = action.replyTo,
		.date = NewMessageDate(action.options),
		.scheduleRepeatPeriod = action.options.scheduleRepeatPeriod,
		.shortcutId = action.options.shortcutId,
		.starsPaid = starsPaid,
		.postAuthor = NewMessagePostAuthor(action),
		.effectId = action.options.effectId,
		.suggest = HistoryMessageSuggestInfo(action.options),
	}, TextWithEntities(), MTP_messageMediaDice(
		MTP_flags(stake
			? MTPDmessageMediaDice::Flag::f_game_outcome
			: MTPDmessageMediaDice::Flag()),
		MTP_int(0),
		MTP_string(emoji),
		MTP_messages_emojiGameOutcome(
			MTP_bytes(seed),
			MTP_long(stake),
			MTP_long(0))));
	histories.sendPreparedMessage(
		history,
		action.replyTo,
		randomId,
		Data::Histories::PrepareMessage<MTPmessages_SendMedia>(
			MTP_flags(sendFlags),
			peer->input(),
			Data::Histories::ReplyToPlaceholder(),
			(stake
				? MTP_inputMediaStakeDice(
					MTP_bytes(action.options.stakeSeedHash),
					MTP_long(stake),
					MTP_bytes(seed))
				: MTP_inputMediaDice(MTP_string(emoji))),
			MTP_string(),
			MTP_long(randomId),
			MTPReplyMarkup(),
			MTP_vector<MTPMessageEntity>(),
			MTP_int(action.options.scheduled),
			MTP_int(action.options.scheduleRepeatPeriod),
			(sendAs ? sendAs->input() : MTP_inputPeerEmpty()),
			Data::ShortcutIdToMTP(session, action.options.shortcutId),
			MTP_long(action.options.effectId),
			MTP_long(starsPaid),
			SuggestToMTP(action.options.suggest)
		), [=](const MTPUpdates &result, const MTP::Response &response) {
	}, [=](const MTP::Error &error, const MTP::Response &response) {
		api->sendMessageFail(error, peer, randomId, newId);
	});
	api->finishForwarding(action);
	return true;
}

void SendLocation(SendAction action, float64 lat, float64 lon) {
	SendSimpleMedia(
		action,
		MTP_inputMediaGeoPoint(
			MTP_inputGeoPoint(
				MTP_flags(0),
				MTP_double(lat),
				MTP_double(lon),
				MTPint()))); // accuracy_radius
}

void FillMessagePostFlags(
		const SendAction &action,
		not_null<PeerData*> peer,
		MessageFlags &flags) {
	InnerFillMessagePostFlags(action.options, peer, flags);
}

MTPmessages_SendMedia::Flags PrepareSendMedia(
		not_null<Main::Session*> session,
		SendAction &action,
		int &starsPaid) {
	StripEphemeralReply(session, action.replyTo);
	session->api().sendAction(action);

	const auto history = action.history;
	const auto peer = history->peer;
	const auto topicRootId = action.replyTo.messageId
		? action.replyTo.topicRootId
		: 0;
	const auto monoforumPeerId = action.replyTo.monoforumPeerId;
	auto sendFlags = MTPmessages_SendMedia::Flags(0);
	if (action.replyTo) {
		sendFlags |= MTPmessages_SendMedia::Flag::f_reply_to;
	}
	const auto clearCloudDraft = action.clearDraft;
	if (clearCloudDraft) {
		sendFlags |= MTPmessages_SendMedia::Flag::f_clear_draft;
		history->clearLocalDraft(topicRootId, monoforumPeerId);
		history->clearCloudDraft(topicRootId, monoforumPeerId);
		history->startSavingCloudDraft(topicRootId, monoforumPeerId);
	}
	const auto silentPost = ShouldSendSilent(peer, action.options);
	starsPaid = std::min(
		peer->starsPerMessageChecked(),
		action.options.starsApproved);
	if (silentPost) {
		sendFlags |= MTPmessages_SendMedia::Flag::f_silent;
	}
	if (action.options.scheduled) {
		sendFlags |= MTPmessages_SendMedia::Flag::f_schedule_date;
		if (action.options.scheduleRepeatPeriod) {
			sendFlags |= MTPmessages_SendMedia::Flag::f_schedule_repeat_period;
		}
	}
	if (action.options.shortcutId) {
		sendFlags |= MTPmessages_SendMedia::Flag::f_quick_reply_shortcut;
	}
	if (action.options.effectId) {
		sendFlags |= MTPmessages_SendMedia::Flag::f_effect;
	}
	if (action.options.suggest) {
		sendFlags |= MTPmessages_SendMedia::Flag::f_suggested_post;
	}
	if (starsPaid) {
		action.options.starsApproved -= starsPaid;
		sendFlags |= MTPmessages_SendMedia::Flag::f_allow_paid_stars;
	}
	const auto sendAs = action.options.sendAs;
	if (sendAs) {
		sendFlags |= MTPmessages_SendMedia::Flag::f_send_as;
	}
	return sendFlags;
}

namespace {

struct ConfirmedLocalFile {
	std::shared_ptr<FilePrepareResult> file;
	not_null<History*> history;
	not_null<PeerData*> peer;
	FullMsgId newId;
	SendAction action;
	TextWithEntities caption;
	MTPMessageMedia media;
	MessageFlags flags = MessageFlags();
	HistoryItem *itemToEdit = nullptr;
	int starsPaid = 0;
};

[[nodiscard]] TextWithEntities PrepareConfirmedFileCaption(
		not_null<History*> history,
		not_null<Main::Session*> session,
		const std::shared_ptr<FilePrepareResult> &file) {
	auto caption = TextWithEntities{
		file->caption.text,
		TextUtilities::ConvertTextTagsToEntities(file->caption.tags)
	};
	const auto prepareFlags = Ui::ItemTextOptions(
		history,
		session->user()).flags;
	TextUtilities::PrepareForSending(caption, prepareFlags);
	TextUtilities::Trim(caption);
	return caption;
}

[[nodiscard]] MessageFlags PrepareConfirmedFileFlags(
		not_null<Main::Session*> session,
		const SendAction &action,
		not_null<PeerData*> peer,
		const std::shared_ptr<FilePrepareResult> &file,
		const QString &caption,
		bool isEditing) {
	const auto welcomeTemplate = file->to.options.welcomeTemplate;
	const auto album = welcomeTemplate ? nullptr : file->album.lock();
	const auto groupId = album ? album->groupId : uint64(0);
	auto flags = isEditing ? MessageFlags() : NewMessageFlags(peer);
	if (welcomeTemplate) {
		flags &= ~MessageFlag::Outgoing;
		flags |= MessageFlag::FakeHistoryItem;
	}
	if (file->to.replyTo) {
		flags |= MessageFlag::HasReplyInfo;
	}
	if (!isEditing
		&& !welcomeTemplate
		&& !groupId
		&& !file->to.options.scheduled
		&& !file->to.options.shortcutId
		&& session->ephemeralMessages().wouldSendMedia(
			peer,
			file->to.replyTo,
			caption)) {
		flags |= MessageFlag::Ephemeral;
	}
	FillMessagePostFlags(action, peer, flags);
	if (file->to.options.scheduled) {
		flags |= MessageFlag::IsOrWasScheduled;

		// Scheduled messages have no 'edited' badge.
		flags |= MessageFlag::HideEdited;
	}
	if (file->to.options.shortcutId) {
		flags |= MessageFlag::ShortcutMessage;

		// Shortcut messages have no 'edited' badge.
		flags |= MessageFlag::HideEdited;
	}
	const auto mediaTtlSeconds = (file->to.options.scheduled
		|| (file->type != SendMediaType::Photo
			&& file->type != SendMediaType::File))
		? crl::time()
		: file->to.options.ttlSeconds;
	if (file->type == SendMediaType::Audio
		|| file->type == SendMediaType::Round
		|| mediaTtlSeconds) {
		if (!peer->isChannel() || peer->isMegagroup()) {
			flags |= MessageFlag::MediaIsUnread;
		}
	}
	if (file->to.options.invertCaption) {
		flags |= MessageFlag::InvertMedia;
	}
	return flags;
}

[[nodiscard]] MTPMessageMedia PrepareConfirmedFileMedia(
		const std::shared_ptr<FilePrepareResult> &file) {
	const auto mediaTtlSeconds = (file->to.options.scheduled
		|| (file->type != SendMediaType::Photo
			&& file->type != SendMediaType::File))
		? crl::time()
		: file->to.options.ttlSeconds;
	return MTPMessageMedia([&] {
		if (file->type == SendMediaType::Photo) {
			using Flag = MTPDmessageMediaPhoto::Flag;
			return MTP_messageMediaPhoto(
				MTP_flags(Flag::f_photo
					| (file->spoiler ? Flag::f_spoiler : Flag())
					| (mediaTtlSeconds ? Flag::f_ttl_seconds : Flag())),
				file->photo,
				MTP_int(mediaTtlSeconds),
				MTPDocument()); // video
		} else if (file->type == SendMediaType::File) {
			using Flag = MTPDmessageMediaDocument::Flag;
			return MTP_messageMediaDocument(
				MTP_flags(Flag::f_document
					| (file->spoiler ? Flag::f_spoiler : Flag())
					| (file->videoCover ? Flag::f_video_cover : Flag())
					| (mediaTtlSeconds ? Flag::f_ttl_seconds : Flag())),
				file->document,
				MTPVector<MTPDocument>(), // alt_documents
				file->videoCover ? file->videoCover->photo : MTPPhoto(),
				MTPint(), // video_timestamp
				MTP_int(mediaTtlSeconds));
		} else if (file->type == SendMediaType::Audio) {
			const auto ttlSeconds = file->to.options.ttlSeconds;
			using Flag = MTPDmessageMediaDocument::Flag;
			return MTP_messageMediaDocument(
				MTP_flags(Flag::f_document
					| Flag::f_voice
					| (ttlSeconds ? Flag::f_ttl_seconds : Flag())
					| (file->videoCover ? Flag::f_video_cover : Flag())),
				file->document,
				MTPVector<MTPDocument>(), // alt_documents
				file->videoCover ? file->videoCover->photo : MTPPhoto(),
				MTPint(), // video_timestamp
				MTP_int(ttlSeconds));
		} else if (file->type == SendMediaType::Round) {
			using Flag = MTPDmessageMediaDocument::Flag;
			const auto ttlSeconds = file->to.options.ttlSeconds;
			return MTP_messageMediaDocument(
				MTP_flags(Flag::f_document
					| Flag::f_round
					| (ttlSeconds ? Flag::f_ttl_seconds : Flag())
					| (file->spoiler ? Flag::f_spoiler : Flag())),
				file->document,
				MTPVector<MTPDocument>(), // alt_documents
				MTPPhoto(), // video_cover
				MTPint(), // video_timestamp
				MTP_int(ttlSeconds));
		}
		Unexpected("Type in sendFilesConfirmed.");
	}());
}

[[nodiscard]] ConfirmedLocalFile PrepareConfirmedLocalFile(
		not_null<Main::Session*> session,
		const std::shared_ptr<FilePrepareResult> &file,
		int starsPaid) {
	const auto isEditing = (file->type != SendMediaType::Audio)
		&& (file->type != SendMediaType::Round)
		&& (file->to.replaceMediaOf != 0);
	const auto newId = FullMsgId(
		file->to.peer,
		(isEditing
			? file->to.replaceMediaOf
			: session->data().nextLocalMessageId()));
	const auto welcomeTemplate = file->to.options.welcomeTemplate;
	if (const auto album = welcomeTemplate ? nullptr : file->album.lock()) {
		const auto it = ranges::find(
			album->items,
			file->taskId,
			&SendingAlbum::Item::taskId);
		Assert(it != album->items.end());

		it->msgId = newId;
	}

	const auto itemToEdit = isEditing
		? session->data().message(newId)
		: nullptr;
	const auto history = session->data().history(file->to.peer);
	const auto peer = history->peer;

	if (!isEditing) {
		auto &histories = session->data().histories();
		file->to.replyTo.messageId = histories.convertTopicReplyToId(
			history,
			file->to.replyTo.messageId);
		file->to.replyTo.topicRootId = histories.convertTopicReplyToId(
			history,
			file->to.replyTo.topicRootId);
	}

	auto action = SendAction(history, file->to.options);
	action.clearDraft = false;
	action.replyTo = file->to.replyTo;
	action.generateLocal = true;
	action.replaceMediaOf = file->to.replaceMediaOf;

	auto caption = PrepareConfirmedFileCaption(history, session, file);
	const auto flags = PrepareConfirmedFileFlags(
		session,
		action,
		peer,
		file,
		caption.text,
		isEditing);

	return ConfirmedLocalFile{
		file,
		history,
		peer,
		newId,
		action,
		std::move(caption),
		PrepareConfirmedFileMedia(file),
		flags,
		itemToEdit,
		starsPaid,
	};
}

void AddConfirmedLocalPlaceholder(const ConfirmedLocalFile &local) {
	if (local.itemToEdit) {
		auto edition = HistoryMessageEdition();
		edition.isEditHide = (local.flags & MessageFlag::HideEdited);
		edition.editDate = 0;
		edition.ttl = 0;
		edition.mtpMedia = &local.media;
		edition.textWithEntities = local.caption;
		edition.invertMedia = local.file->to.options.invertCaption;
		edition.useSameViews = true;
		edition.useSameForwards = true;
		edition.useSameMarkup = true;
		edition.useSameReplies = true;
		edition.useSameReactions = true;
		edition.useSameSuggest = true;
		edition.savePreviousMedia = true;
		local.itemToEdit->applyEdition(std::move(edition));
		return;
	}

	const auto welcomeTemplate = local.file->to.options.welcomeTemplate;
	const auto album = welcomeTemplate
		? nullptr
		: local.file->album.lock();
	const auto item = local.history->addNewLocalMessage({
		.id = local.newId.msg,
		.flags = local.flags,
		.from = NewMessageFromId(local.action),
		.replyTo = local.file->to.replyTo,
		.date = NewMessageDate(local.file->to.options),
		.scheduleRepeatPeriod = local.file->to.options.scheduleRepeatPeriod,
		.shortcutId = local.file->to.options.shortcutId,
		.starsPaid = local.starsPaid,
		.postAuthor = NewMessagePostAuthor(local.action),
		.groupedId = album ? album->groupId : uint64(0),
		.effectId = local.file->to.options.effectId,
		.suggest = HistoryMessageSuggestInfo(local.file->to.options),
	}, local.caption, local.media);
	if (welcomeTemplate) {
		local.history->session().welcomeMessages().appendSending(item);
	}
}

[[nodiscard]] bool TargetsDiscardedTopic(
		not_null<Main::Session*> session,
		const std::shared_ptr<FilePrepareResult> &file) {
	const auto rootId = file->to.replyTo.topicRootId;
	if (!IsClientMsgId(rootId)) {
		return false;
	}
	const auto history = session->data().history(file->to.peer);
	const auto forum = history->asForum();
	const auto &histories = session->data().histories();
	return !(forum && forum->creating(rootId))
		&& IsClientMsgId(histories.convertTopicReplyToId(history, rootId));
}

[[nodiscard]] bool FlushPreparedMusicBatch(
		not_null<Main::Session*> session,
		const std::shared_ptr<FilePrepareResult> &sample) {
	const auto album = sample->album.lock();
	if (!album || !album->preparedMusicBatching()) {
		return false;
	}
	auto files = album->takePreparedMusic();
	if (files.empty()) {
		return false;
	} else if (TargetsDiscardedTopic(session, files.front())) {
		return true;
	}

	const auto peer = session->data().history(files.front()->to.peer)->peer;
	auto remainingStarsApproved = album->options.starsApproved;
	auto locals = std::vector<ConfirmedLocalFile>();
	locals.reserve(files.size());
	for (const auto &file : files) {
		const auto starsPaid = std::min(
			peer->starsPerMessageChecked(),
			remainingStarsApproved);
		remainingStarsApproved -= starsPaid;
		locals.push_back(PrepareConfirmedLocalFile(session, file, starsPaid));
	}

	auto notifyHistory = false;
	for (const auto &local : locals) {
		session->api().sendAction(local.action);
		AddConfirmedLocalPlaceholder(local);
		notifyHistory = notifyHistory
			|| (!local.itemToEdit
				&& !local.file->to.options.welcomeTemplate);
	}
	for (const auto &local : locals) {
		session->uploader().upload(local.newId, local.file);
	}

	if (notifyHistory) {
		session->data().sendHistoryChangeNotifications();
		session->changes().historyUpdated(
			locals.front().history,
			(locals.front().action.options.scheduled
				? Data::HistoryUpdate::Flag::ScheduledSent
				: Data::HistoryUpdate::Flag::MessageSent));
	}
	return true;
}

} // namespace

void SendConfirmedFile(
		not_null<Main::Session*> session,
		const std::shared_ptr<FilePrepareResult> &file) {
	const auto welcomeTemplate = file->to.options.welcomeTemplate;
	if (welcomeTemplate && file->to.replaceMediaOf) {
		return;
	}
	if (welcomeTemplate) {
		const auto history = session->data().history(file->to.peer);
		if (session->welcomeMessages().count(history)
			>= Data::WelcomeMessagesLimit(session)) {
			return;
		}
	}
	if (FlushPreparedMusicBatch(session, file)
		|| TargetsDiscardedTopic(session, file)) {
		return;
	}

	const auto history = session->data().history(file->to.peer);
	const auto local = PrepareConfirmedLocalFile(
		session,
		file,
		std::min(
			history->peer->starsPerMessageChecked(),
			file->to.options.starsApproved));
	session->uploader().upload(local.newId, file);
	session->api().sendAction(local.action);
	AddConfirmedLocalPlaceholder(local);

	if (local.itemToEdit) {
		return;
	}
	if (welcomeTemplate) {
		return;
	}

	session->data().sendHistoryChangeNotifications();
	session->changes().historyUpdated(
		local.history,
		(local.action.options.scheduled
			? Data::HistoryUpdate::Flag::ScheduledSent
			: Data::HistoryUpdate::Flag::MessageSent));
}

} // namespace Api
