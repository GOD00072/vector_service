#pragma once

#include <string>
#include <vector>
#include <functional>
#include <unordered_map>
#include <regex>

namespace vectordb {

struct RouteMatch {
    bool matched = false;
    std::vector<std::string> params;
};

struct Route {
    std::string method;
    std::string pattern;
    std::regex regex;
    std::vector<std::string> param_names;
    std::function<std::string(const std::vector<std::string>&, const std::string&)> handler;
};

class HTTPRouter {
public:
    void get(const std::string& pattern,
             std::function<std::string(const std::vector<std::string>&, const std::string&)> handler);

    void post(const std::string& pattern,
              std::function<std::string(const std::vector<std::string>&, const std::string&)> handler);

    void put(const std::string& pattern,
             std::function<std::string(const std::vector<std::string>&, const std::string&)> handler);

    void del(const std::string& pattern,
             std::function<std::string(const std::vector<std::string>&, const std::string&)> handler);

    std::string route(const std::string& method, const std::string& path, const std::string& body);

private:
    std::vector<Route> routes_;

    void add_route(const std::string& method, const std::string& pattern,
                   std::function<std::string(const std::vector<std::string>&, const std::string&)> handler);

    std::pair<std::regex, std::vector<std::string>> compile_pattern(const std::string& pattern);
};

}
