// This file was developed by Thomas Müller <thomas94@gmx.net>.
// It is published under the GPU GPLv3 license; see LICENSE.txt for details.

#include <twm/logging.h>

#include <format>
#include <iostream>
#include <string>

using namespace std;

namespace twm {

auto min_severity = Severity::Info;

void log(Severity severity, const string& str) {
	if (severity < min_severity) {
		return;
	}

	switch (severity) {
		case Severity::Debug: cout << format("DEBUG: {}\n", str); break;
		case Severity::Info: cout << format("INFO: {}\n", str); break;
		case Severity::Warning: cerr << format("WARNING: {}\n", str); break;
		case Severity::Error: cerr << format("ERROR: {}\n", str); break;
	}
}

void log_debug(const string& str) { log(Severity::Debug, str); }
void log_info(const string& str) { log(Severity::Info, str); }
void log_warning(const string& str) { log(Severity::Warning, str); }
void log_error(const string& str) { log(Severity::Error, str); }

} // namespace twm
