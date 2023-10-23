// This file was developed by Thomas Müller <thomas94@gmx.net>.
// It is published under the GPU GPLv3 license; see LICENSE.txt for details.

#include <iostream>

#define NOMINMAX
#include <Windows.h>
#ifdef far
#undef far
#undef near
#endif

int main(int argc, char* argv[]) {
	MessageBox(nullptr, "Why hello there", "twm", MB_OK);
}

