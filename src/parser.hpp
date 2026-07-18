#ifndef PARSER_HPP
#define PARSER_HPP

#include <string>
#include <vector>
#include <utility>
#include <functional>
#include "types.hpp"

using HeredocReader = std::function<std::string(const std::string& delimiter)>;

namespace Parser {

    std::vector<std::string> tokenize(const std::string& line);

    bool parseJob(std::vector<std::string> tokens, PipelineJob& job,
                  const HeredocReader& heredocReader);

    std::vector<std::pair<PipelineJob, Connector>> parseLine(
        const std::vector<std::string>& tokens, const HeredocReader& heredocReader);
}

#endif
