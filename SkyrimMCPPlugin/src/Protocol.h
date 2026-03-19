#pragma once

#include <nlohmann/json.hpp>
#include <string>

namespace SkyrimMCP {

    using json = nlohmann::json;

    namespace Protocol {
        // Parse a JSON request string and dispatch to the appropriate handler.
        // Returns a JSON response string (newline-terminated).
        std::string HandleRequest(const std::string& requestLine);
    }

}
