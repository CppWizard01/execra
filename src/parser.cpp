#include "parser.hpp"
#include <stdexcept>

using namespace std;

namespace Parser {

vector<string> tokenize(const string& line) {
    vector<string> tokens;
    string cur;
    bool inWord = false;
    size_t i = 0, n = line.size();

    auto flush = [&]() {
        if (inWord) { tokens.push_back(cur); cur.clear(); inWord = false; }
    };

    auto isOperatorChar = [](char c) {
        return c == '|' || c == '>' || c == '<' || c == ';' || c == '&';
    };

    while (i < n) {
        char c = line[i];

        if (c == ' ' || c == '\t') { flush(); i++; continue; }

        if (c == '\'' || c == '"') {
            char q = c;
            i++;
            inWord = true;
            while (i < n && line[i] != q) {
                if (q == '"' && line[i] == '\\' && i + 1 < n &&
                    (line[i + 1] == '"' || line[i + 1] == '\\')) {
                    cur += line[i + 1];
                    i += 2;
                    continue;
                }
                cur += line[i];
                i++;
            }
            if (i < n) { i++; }
            else throw runtime_error("unmatched quote");
            continue;
        }

        if (isOperatorChar(c)) {
            flush();
            if (c == '&' && i + 1 < n && line[i + 1] == '&') { tokens.push_back("&&"); i += 2; continue; }
            if (c == '|' && i + 1 < n && line[i + 1] == '|') { tokens.push_back("||"); i += 2; continue; }
            if (c == '>' && i + 1 < n && line[i + 1] == '>') { tokens.push_back(">>"); i += 2; continue; }
            if (c == '<' && i + 1 < n && line[i + 1] == '<') { tokens.push_back("<<"); i += 2; continue; }
            tokens.push_back(string(1, c));
            i++;
            continue;
        }

        if (c == '\\' && i + 1 < n) { cur += line[i + 1]; inWord = true; i += 2; continue; }

        cur += c;
        inWord = true;
        i++;
    }
    flush();
    return tokens;
}

bool parseJob(vector<string> tokens, PipelineJob& job, const HeredocReader& heredocReader) {
    if (tokens.empty()) return false;

    vector<vector<string>> stages;
    vector<string> cur;
    for (auto& t : tokens) {
        if (t == "|") {
            if (cur.empty()) throw runtime_error("syntax error near unexpected token '|'");
            stages.push_back(cur);
            cur.clear();
        } else {
            cur.push_back(t);
        }
    }
    if (cur.empty()) throw runtime_error("syntax error near unexpected token '|'");
    stages.push_back(cur);

    job.pipeline.clear();
    for (auto& stageTokens : stages) {
        Command cmd;
        for (size_t i = 0; i < stageTokens.size(); i++) {
            string& t = stageTokens[i];
            if (t == "<" || t == ">" || t == ">>") {
                if (i + 1 >= stageTokens.size())
                    throw runtime_error("expected filename after '" + t + "'");
                string file = stageTokens[++i];
                if (t == "<") cmd.inFile = file;
                else if (t == ">") cmd.outFile = file;
                else { cmd.outFile = file; cmd.appendOut = true; }
            } else if (t == "<<") {
                if (i + 1 >= stageTokens.size())
                    throw runtime_error("expected delimiter after '<<'");
                string delimiter = stageTokens[++i];
                if (!heredocReader)
                    throw runtime_error("heredoc not supported here");
                cmd.heredocContent = heredocReader(delimiter);
                cmd.hasHeredoc = true;
            } else {
                cmd.args.push_back(t);
            }
        }
        if (cmd.args.empty()) throw runtime_error("syntax error: empty command in pipeline");
        job.pipeline.push_back(cmd);
    }
    return true;
}

vector<pair<PipelineJob, Connector>> parseLine(const vector<string>& tokens,
                                                const HeredocReader& heredocReader) {
    vector<pair<PipelineJob, Connector>> result;
    vector<string> seg;

    auto flushSeg = [&](Connector conn, bool background) {
        if (seg.empty()) {
            if (conn == Connector::NONE && !background) return;
            throw runtime_error("syntax error: unexpected token");
        }
        PipelineJob job;
        parseJob(seg, job, heredocReader);
        job.background = background;
        for (auto& t : seg) { if (!job.text.empty()) job.text += " "; job.text += t; }
        result.push_back({job, conn});
        seg.clear();
    };

    for (auto& t : tokens) {
        if (t == ";") { flushSeg(Connector::SEQ, false); }
        else if (t == "&&") { flushSeg(Connector::AND, false); }
        else if (t == "||") { flushSeg(Connector::OR, false); }
        else if (t == "&") { flushSeg(Connector::SEQ, true); }
        else seg.push_back(t);
    }
    if (!seg.empty()) flushSeg(Connector::NONE, false);

    return result;
}

}
