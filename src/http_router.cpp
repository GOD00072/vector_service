#include "http_router.hpp"

namespace vectordb {

void HTTPRouter::add_route(const std::string& method, const std::string& pattern,
                           std::function<std::string(const std::vector<std::string>&, const std::string&)> handler) {
    Route route;
    route.method = method;
    route.pattern = pattern;
    auto [regex, param_names] = compile_pattern(pattern);
    route.regex = regex;
    route.param_names = param_names;
    route.handler = handler;
    routes_.push_back(std::move(route));
}

void HTTPRouter::get(const std::string& pattern,
                     std::function<std::string(const std::vector<std::string>&, const std::string&)> handler) {
    add_route("GET", pattern, handler);
}

void HTTPRouter::post(const std::string& pattern,
                      std::function<std::string(const std::vector<std::string>&, const std::string&)> handler) {
    add_route("POST", pattern, handler);
}

void HTTPRouter::put(const std::string& pattern,
                     std::function<std::string(const std::vector<std::string>&, const std::string&)> handler) {
    add_route("PUT", pattern, handler);
}

void HTTPRouter::del(const std::string& pattern,
                     std::function<std::string(const std::vector<std::string>&, const std::string&)> handler) {
    add_route("DELETE", pattern, handler);
}

std::pair<std::regex, std::vector<std::string>> HTTPRouter::compile_pattern(const std::string& pattern) {
    std::vector<std::string> param_names;
    std::string regex_str = "^";

    size_t i = 0;
    while (i < pattern.length()) {
        if (pattern[i] == ':') {
            size_t start = i + 1;
            while (i + 1 < pattern.length() && pattern[i + 1] != '/') {
                i++;
            }
            std::string param_name = pattern.substr(start, i - start + 1);
            param_names.push_back(param_name);
            regex_str += "([^/]+)";
        } else {
            if (pattern[i] == '.' || pattern[i] == '*' || pattern[i] == '+' ||
                pattern[i] == '?' || pattern[i] == '(' || pattern[i] == ')' ||
                pattern[i] == '[' || pattern[i] == ']' || pattern[i] == '{' ||
                pattern[i] == '}' || pattern[i] == '|' || pattern[i] == '^' ||
                pattern[i] == '$' || pattern[i] == '\\') {
                regex_str += '\\';
            }
            regex_str += pattern[i];
        }
        i++;
    }

    regex_str += "$";
    return {std::regex(regex_str), param_names};
}

std::string HTTPRouter::route(const std::string& method, const std::string& path, const std::string& body) {
    for (const auto& route : routes_) {
        if (route.method != method) continue;

        std::smatch match;
        if (std::regex_match(path, match, route.regex)) {
            std::vector<std::string> params;
            for (size_t i = 1; i < match.size(); ++i) {
                params.push_back(match[i].str());
            }
            return route.handler(params, body);
        }
    }

    return "";
}

}
