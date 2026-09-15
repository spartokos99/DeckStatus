#pragma once

#include "deckstatus_protocol.h"

#include <cstdint>
#include <filesystem>
#include <memory>
#include <string>
#include <utility>

namespace deckstatus {

// Looks up artwork and metadata in the local collection, without modifying it.
// Calls can safely run on different server threads (with distinct DeckData values).
class ArtworkResolver {
public:
    explicit ArtworkResolver(std::filesystem::path rekordbox_exe,
                             std::filesystem::path database_override = {});
    ~ArtworkResolver();
    ArtworkResolver(const ArtworkResolver&) = delete;
    ArtworkResolver& operator=(const ArtworkResolver&) = delete;

    std::pair<std::string, std::string> get(std::uint32_t content_id);
    // Refreshes collection metadata for deck.track_id; preserves live BPM and IDs.
    // Missing rows/errors clear collection metadata and return false.
    bool enrich(DeckData& deck);
    std::string diagnostic() const;

private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};

} // namespace deckstatus
