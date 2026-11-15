/*
This file is part of Moligram, a Telegram client built on Telegram Desktop.

For license and copyright information please follow this link:
https://github.com/monolithcraft/moligram/blob/desktop/LEGAL
*/
#pragma once

namespace Moligram {

[[nodiscard]] bool CaptureHidingSupported();
void SetCaptureHidden(bool hidden);
void StartCaptureGuard();

} // namespace Moligram
