#pragma once
#include <nlohmann/json.hpp>
#include <filesystem>
#include <fstream>
#include <string>

namespace deckstatus {
// Loaded once before worker threads start; browser UI uses these same dictionaries.
inline nlohmann::json language_strings = nlohmann::json::object();
inline void load_language(const std::filesystem::path& folder, const std::string& code) {
    std::ifstream input(folder / (code + ".json"));
    if (!input) throw std::runtime_error("Language files missing in web/locales.");
    language_strings = nlohmann::json::parse(input);
}
inline std::string tr(const std::string& key) {
    const auto found = language_strings.find(key);
    if (found != language_strings.end() && found->is_string()) return found->get<std::string>();
    // Preserve native error details appended to a translated diagnostic prefix.
    for (const auto& [prefix, value] : language_strings.items())
        if (prefix.size() > 12 && key.starts_with(prefix) && value.is_string())
            return value.get<std::string>() + key.substr(prefix.size());
    return key;
}
}
