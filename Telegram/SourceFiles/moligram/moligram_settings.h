/*
This file is part of Moligram, a Telegram client built on Telegram Desktop.

For license and copyright information please follow this link:
https://github.com/monolithcraft/moligram/blob/desktop/LEGAL
*/
#pragma once

namespace Moligram {

inline constexpr auto kVersion = "1.0.0";

enum class Option {
	SendReadMessages,
	SendReadStories,
	SendOnlineStatus,
	SendTyping,
	SendUploadProgress,
	ReadAfterSending,
	KeepDeletedMessages,
	KeepEditHistory,
	StreamerMode,
	ShowMessageSeconds,
	ShowPeerIds,
	HideStories,
	GhostInMainMenu,
	ConfirmStickerSending,
	SilentSending,
	DeletionAlerts,
	HighlightEdited,
	ClubMember,
	SendListened,
	CleanLinks,
	LockOnMinimize,
	PanicKey,
	ForwardNoAuthor,
	ClubBadges,
	CompactDialogs,
	HoverProfiles,
	DoNotDisturb,
	QuickDnd,
	QuickPanel,
	QuickGhost,
	QuickStreamer,
	QuickHideStories,
	QuickSeconds,
	QuickPeerIds,
	QuickKeepDeleted,
	QuickKeepEdits,
	QuickSilent,

	Count,
};

[[nodiscard]] bool Enabled(Option option);
void Set(Option option, bool enabled);
[[nodiscard]] rpl::producer<bool> Value(Option option);
[[nodiscard]] rpl::producer<> Changes();

enum class PeerList {
	GhostExceptions,
	OnlineAlerts,
};

[[nodiscard]] bool InList(PeerList list, PeerId peer);
void SetInList(PeerList list, PeerId peer, bool included);
[[nodiscard]] std::vector<PeerId> ListPeers(PeerList list);
[[nodiscard]] bool SendsReads(PeerId peer);
[[nodiscard]] bool SendsTyping(PeerId peer);

enum class PeerText {
	Nickname,
	Note,
};

[[nodiscard]] QString PeerTextValue(PeerText which, PeerId peer);
void SetPeerText(PeerText which, PeerId peer, QString text);

[[nodiscard]] QStringList FilterWords();
void SetFilterWords(QStringList words);
[[nodiscard]] bool Filtered(const QString &text);

enum class Highlight {
	Deleted,
	Edited,
};

[[nodiscard]] QColor HighlightColor(Highlight which);
void SetHighlightColor(Highlight which, QColor color);
[[nodiscard]] const std::vector<QColor> &HighlightPalette();

[[nodiscard]] int StickerSize();
void SetStickerSize(int size);

[[nodiscard]] int BubbleRadius();
void SetBubbleRadius(int radius);

[[nodiscard]] int ClubBanner();
void SetClubBanner(int banner);
[[nodiscard]] rpl::producer<int> ClubBannerValue();
[[nodiscard]] std::pair<QColor, QColor> ClubColors();
void SetClubColors(QColor from, QColor to);
[[nodiscard]] MsgId ClubImage();
void SetClubImage(MsgId id);

[[nodiscard]] bool GhostMode();
void SetGhostMode(bool enabled);
[[nodiscard]] rpl::producer<bool> GhostModeValue();

[[nodiscard]] QString FormatMessageTime(QTime time);
[[nodiscard]] QString InterfaceFont(const QString &chosen);

void Load();

} // namespace Moligram
