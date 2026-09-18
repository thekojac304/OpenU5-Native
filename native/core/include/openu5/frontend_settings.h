#pragma once
#include "frontend.h"
#include "save_json.h"
#include <string>

namespace openu5 {
save::Json settings_document(const FrontendSettings &);
bool settings_from_document(const save::Json &, FrontendSettings &);
bool encode_settings(const FrontendSettings &, std::string &);
bool decode_settings(const std::string &, FrontendSettings &);
}
