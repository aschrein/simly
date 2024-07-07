#ifndef __DSL_HPP__
#define __DSL_HPP__

#include <algorithm>
#include <cassert>
#include <cstdio>
#include <iostream>
#include <memory>
#include <sstream>
#include <string.h>
#include <string>
#include <unordered_map>
#include <vector>

namespace dsl {

struct StringRef {
    const char *begin = {};
    const char *end   = {};

    StringRef() = default;
    StringRef(const char *b, const char *e) : begin(b), end(e) {}

    size_t length() const { return end - begin; }
    std::string str() const { return std::string(begin, end); }

    bool operator==(const char *s) const {
        size_t len = strlen(s);
        return (length() == len) && (strncmp(begin, s, len) == 0);
    }
};

enum class TokenType { NUMBER, LITERAL, STRING, OPERATOR, SPECIAL };

struct Token {
    TokenType type  = {};
    StringRef value = {};
    int line        = {};
    int col         = {};

    Token(TokenType t, StringRef v, int l, int c)
        : type(t), value(v), line(l), col(c) {}

    bool is_float() const {
        return type == TokenType::NUMBER &&
               std::find(value.begin, value.end, '.') != value.end;
    }

    float get_float() const { return std::stof(value.str()); }
};

class TokenStream {
  private:
    std::vector<Token> tokens;
    size_t pos;
    std::string string;
    std::vector<std::string> lines;
    std::vector<size_t> lines_scan;

    void tokenize() {
        const char *start = string.c_str();
        const char *end   = start + string.length();
        const char *cur   = start;
        int line          = 0;
        int col           = 0;

        auto add_token = [&](TokenType type, const char *token_start,
                             const char *token_end) {
            tokens.emplace_back(type, StringRef(token_start, token_end), line,
                                col);
            col += token_end - token_start;
        };

        while (cur < end) {
            if (*cur == '\n') {
                lines.push_back(std::string(start, cur));
                lines_scan.push_back(start - string.c_str());
                start = cur + 1;
                line++;
                col = 0;
                cur++;
            } else if (std::isspace(*cur)) {
                cur++;
                col++;
            } else if (*cur == '/' && cur + 1 < end && *(cur + 1) == '/') {
                // Skip comments
                while (cur < end && *cur != '\n')
                    cur++;
            } else if (*cur == '"' || *cur == '\'') {
                const char quote      = *cur;
                const char *str_start = cur;
                cur++;
                while (cur < end && *cur != quote) {
                    if (*cur == '\\' && cur + 1 < end)
                        cur++;
                    cur++;
                }
                if (cur < end)
                    cur++;
                add_token(TokenType::STRING, str_start, cur);
            } else if (std::isdigit(*cur) || (*cur == '.' && cur + 1 < end &&
                                              std::isdigit(*(cur + 1)))) {
                const char *num_start = cur;
                bool has_dot          = false;
                while (cur < end &&
                       (std::isdigit(*cur) || (!has_dot && *cur == '.'))) {
                    if (*cur == '.')
                        has_dot = true;
                    cur++;
                }
                add_token(TokenType::NUMBER, num_start, cur);
            } else if (std::isalpha(*cur) || *cur == '_') {
                const char *id_start = cur;
                while (cur < end && (std::isalnum(*cur) || *cur == '_'))
                    cur++;
                add_token(TokenType::LITERAL, id_start, cur);
            } else {
                // Operators and special characters
                const char *op_start = cur;
                cur++;
                if (cur < end &&
                    (*op_start == '=' || *op_start == '!' || *op_start == '<' ||
                     *op_start == '>' || *op_start == '&' || *op_start == '|' ||
                     *op_start == '+' || *op_start == '-')) {
                    if (*cur == '=' || (*op_start == '&' && *cur == '&') ||
                        (*op_start == '|' && *cur == '|')) {
                        cur++;
                    }
                }
                add_token(TokenType::OPERATOR, op_start, cur);
            }
        }

        if (start < end) {
            lines.push_back(std::string(start, end));
            lines_scan.push_back(start - string.c_str());
        }
    }

  public:
    TokenStream(const std::string &input) : pos(0), string(input) {
        tokenize();
    }

    Token peek() const {
        assert(pos < tokens.size() &&
               "Attempted to peek past end of token stream");
        return tokens[pos];
    }

    Token next() {
        assert(pos < tokens.size() &&
               "Attempted to read past end of token stream");
        return tokens[pos++];
    }

    bool consume(const char *str) {
        if (pos < tokens.size() && tokens[pos].value == str) {
            pos++;
            return true;
        }
        return false;
    }

    std::vector<Token>
    get_list_until(const std::vector<const char *> &end_tokens,
                   bool consume = true) {
        std::vector<Token> result;
        while (pos < tokens.size()) {
            const Token &t = tokens[pos];
            if (std::find_if(end_tokens.begin(), end_tokens.end(),
                             [&t](const char *s) { return t.value == s; }) !=
                end_tokens.end()) {
                if (consume)
                    pos++;
                break;
            }
            result.push_back(next());
        }
        return result;
    }

    std::vector<Token> unwrap_parentheses() {
        assert(consume("(") && "Expected '(' at the beginning of parentheses");
        auto result = get_list_until({")"});
        assert(consume(")") && "Expected ')' at the end of parentheses");
        return result;
    }

    std::vector<std::vector<Token>>
    get_token_groups_in_between(const char *start, const char *end,
                                const char *separator) {
        assert(consume(start) && "Expected start token at the beginning");
        std::vector<std::vector<Token>> groups;
        std::vector<Token> current_group;
        while (pos < tokens.size() && !consume(end)) {
            if (consume(separator)) {
                if (!current_group.empty()) {
                    groups.push_back(std::move(current_group));
                    current_group.clear();
                }
            } else {
                current_group.push_back(next());
            }
        }
        if (!current_group.empty()) {
            groups.push_back(std::move(current_group));
        }
        return groups;
    }

    bool has_more_tokens() const { return pos < tokens.size(); }

    bool eof() const { return pos >= tokens.size(); }

    void move_back() {
        assert(
            pos > 0 &&
            "Attempted to move back before the beginning of the token stream");
        pos--;
    }

    void move_forward() {
        assert(pos < tokens.size() &&
               "Attempted to move forward past the end of the token stream");
        pos++;
    }

    std::string get_line(int line_number) const {
        assert(line_number >= 0 &&
               line_number < static_cast<int>(lines.size()) &&
               "Invalid line number");
        return lines[line_number];
    }

    void print_error_at_current(const std::string &message) const {
        if (pos < tokens.size()) {
            const Token &token = tokens[pos];
            std::cerr << "Error at line " << token.line + 1 << ", col "
                      << token.col << ": " << message << std::endl;
            std::cerr << get_line(token.line) << std::endl;
            std::cerr << std::string(token.col, ' ') << "^" << std::endl;
        } else {
            std::cerr << "Error at end of file: " << message << std::endl;
        }
    }
};

} // namespace dsl

#endif // __DSL_HPP__