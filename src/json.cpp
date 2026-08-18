#include "lunify/json.hpp"

#include <cctype>
#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <sstream>

namespace lunify::json {

namespace {

class Parser {
public:
    explicit Parser(const std::string& text) : text_(text) {}

    Value run(std::string* error) {
        skipWs();
        Value v = parseValue(error);
        if (!error->empty()) return Value();
        skipWs();
        if (pos_ < text_.size()) {
            *error = "unexpected trailing characters at offset " + std::to_string(pos_);
        }
        return v;
    }

private:
    const std::string& text_;
    std::size_t pos_ = 0;

    void skipWs() {
        while (pos_ < text_.size() &&
               (text_[pos_] == ' ' || text_[pos_] == '\t' || text_[pos_] == '\n' ||
                text_[pos_] == '\r')) {
            ++pos_;
        }
    }

    bool consume(char c) {
        if (pos_ < text_.size() && text_[pos_] == c) {
            ++pos_;
            return true;
        }
        return false;
    }

    Value parseValue(std::string* error) {
        if (pos_ >= text_.size()) {
            *error = "unexpected end of input";
            return Value();
        }
        char c = text_[pos_];
        if (c == '{') return parseObject(error);
        if (c == '[') return parseArray(error);
        if (c == '"') {
            Value v;
            v.type = Value::Type::String;
            v.string = parseString(error);
            return v;
        }
        if (c == 't' && text_.compare(pos_, 4, "true") == 0) {
            pos_ += 4;
            Value v;
            v.type = Value::Type::Bool;
            v.boolean = true;
            return v;
        }
        if (c == 'f' && text_.compare(pos_, 5, "false") == 0) {
            pos_ += 5;
            Value v;
            v.type = Value::Type::Bool;
            v.boolean = false;
            return v;
        }
        if (c == 'n' && text_.compare(pos_, 4, "null") == 0) {
            pos_ += 4;
            return Value();
        }
        return parseNumber(error);
    }

    Value parseObject(std::string* error) {
        ++pos_;
        Value v;
        v.type = Value::Type::Object;
        skipWs();
        if (consume('}')) return v;
        while (true) {
            skipWs();
            if (pos_ >= text_.size() || text_[pos_] != '"') {
                *error = "expected object key string";
                return v;
            }
            std::string key = parseString(error);
            if (!error->empty()) return v;
            skipWs();
            if (!consume(':')) {
                *error = "expected ':' after object key";
                return v;
            }
            skipWs();
            Value item = parseValue(error);
            if (!error->empty()) return v;
            v.object.emplace_back(std::move(key), std::move(item));
            skipWs();
            if (consume('}')) return v;
            if (!consume(',')) {
                *error = "expected ',' or '}' in object";
                return v;
            }
        }
    }

    Value parseArray(std::string* error) {
        ++pos_;
        Value v;
        v.type = Value::Type::Array;
        skipWs();
        if (consume(']')) return v;
        while (true) {
            skipWs();
            Value item = parseValue(error);
            if (!error->empty()) return v;
            v.array.push_back(std::move(item));
            skipWs();
            if (consume(']')) return v;
            if (!consume(',')) {
                *error = "expected ',' or ']' in array";
                return v;
            }
        }
    }

    std::string parseString(std::string* error) {
        std::string out;
        ++pos_;
        while (pos_ < text_.size()) {
            unsigned char c = static_cast<unsigned char>(text_[pos_++]);
            if (c == '"') return out;
            if (c == '\\') {
                if (pos_ >= text_.size()) break;
                char e = text_[pos_++];
                switch (e) {
                    case '"': out.push_back('"'); break;
                    case '\\': out.push_back('\\'); break;
                    case '/': out.push_back('/'); break;
                    case 'b': out.push_back('\b'); break;
                    case 'f': out.push_back('\f'); break;
                    case 'n': out.push_back('\n'); break;
                    case 'r': out.push_back('\r'); break;
                    case 't': out.push_back('\t'); break;
                    case 'u': {
                        if (pos_ + 4 > text_.size()) {
                            *error = "truncated \\u escape";
                            return out;
                        }
                        unsigned cp = 0;
                        for (int i = 0; i < 4; ++i) {
                            char h = text_[pos_++];
                            cp <<= 4;
                            if (h >= '0' && h <= '9') cp |= static_cast<unsigned>(h - '0');
                            else if (h >= 'a' && h <= 'f') cp |= static_cast<unsigned>(h - 'a' + 10);
                            else if (h >= 'A' && h <= 'F') cp |= static_cast<unsigned>(h - 'A' + 10);
                            else {
                                *error = "invalid \\u escape";
                                return out;
                            }
                        }
                        if (cp <= 0x7F) {
                            out.push_back(static_cast<char>(cp));
                        } else if (cp <= 0x7FF) {
                            out.push_back(static_cast<char>(0xC0 | (cp >> 6)));
                            out.push_back(static_cast<char>(0x80 | (cp & 0x3F)));
                        } else {
                            out.push_back(static_cast<char>(0xE0 | (cp >> 12)));
                            out.push_back(static_cast<char>(0x80 | ((cp >> 6) & 0x3F)));
                            out.push_back(static_cast<char>(0x80 | (cp & 0x3F)));
                        }
                        break;
                    }
                    default:
                        out.push_back(e);
                        break;
                }
            } else {
                out.push_back(static_cast<char>(c));
            }
        }
        *error = "unterminated string";
        return out;
    }

    Value parseNumber(std::string* error) {
        std::size_t start = pos_;
        while (pos_ < text_.size()) {
            char c = text_[pos_];
            if (!(std::isdigit(static_cast<unsigned char>(c)) || c == '-' || c == '+' ||
                  c == '.' || c == 'e' || c == 'E')) {
                break;
            }
            ++pos_;
        }
        if (pos_ == start) {
            *error = "unexpected character at offset " + std::to_string(pos_);
            return Value();
        }
        Value v;
        v.type = Value::Type::Number;
        v.number = std::strtod(text_.substr(start, pos_ - start).c_str(), nullptr);
        return v;
    }
};

}

Value parse(const std::string& text, std::string* error) {
    if (error) error->clear();
    Parser p(text);
    Value v = p.run(error);
    return v;
}

Value parseFile(const std::string& path, std::string* error) {
    std::ifstream in(path, std::ios::binary);
    if (!in) {
        if (error) *error = "cannot open file: " + path;
        return Value();
    }
    std::ostringstream ss;
    ss << in.rdbuf();
    return parse(ss.str(), error);
}

}
