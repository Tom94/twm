// This file was developed by Thomas Müller <thomas94@gmx.net>.
// It is published under the GPU GPLv3 license; see LICENSE.txt for details.

#pragma once

#include <twm/common.h>

namespace twm {

class TrayPresence {
public:
	TrayPresence(HINSTANCE instance);
	~TrayPresence();

private:
	static const uint32_t WM_TRAYICON_MSG = WM_APP + 1;

	static LRESULT CALLBACK tray_window_proc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam);

	HWND m_invisible_window = nullptr;
	uint32_t m_uid;
};

} // namespace twm
