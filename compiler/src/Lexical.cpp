#include "Lexical.h"

const std::unordered_map<std::string, OperatorInfo>& operatorTable() {
    static const std::unordered_map<std::string, OperatorInfo> table = {
        {
            "+",
            {
                OperatorId::Add,
                OperatorCategory::Arithmetic,
                20,
                Associativity::Left
            }
        },
        {
            "-",
            {
                OperatorId::Subtract,
                OperatorCategory::Arithmetic,
                20,
                Associativity::Left
            }
        },
        {
            "*",
            {
                OperatorId::Multiply,
                OperatorCategory::Arithmetic,
                30,
                Associativity::Left
            }
        },
        {
            "/",
            {
                OperatorId::Divide,
                OperatorCategory::Arithmetic,
                30,
                Associativity::Left
            }
        },
        {
            "=",
            {
                OperatorId::Equal,
                OperatorCategory::Comparison,
                10,
                Associativity::NonAssociative
            }
        },
        {
            "!=",
            {
                OperatorId::NotEqual,
                OperatorCategory::Comparison,
                10,
                Associativity::NonAssociative
            }
        },
        {
            ">",
            {
                OperatorId::Greater,
                OperatorCategory::Comparison,
                10,
                Associativity::NonAssociative
            }
        },
        {
            "<",
            {
                OperatorId::Less,
                OperatorCategory::Comparison,
                10,
                Associativity::NonAssociative
            }
        },
        {
            ">=",
            {
                OperatorId::GreaterEqual,
                OperatorCategory::Comparison,
                10,
                Associativity::NonAssociative
            }
        },
        {
            "<=",
            {
                OperatorId::LessEqual,
                OperatorCategory::Comparison,
                10,
                Associativity::NonAssociative
            }
        }
    };

    return table;
}

std::string tokenKindToString(TokenKind kind) {
    switch (kind) {
        case TokenKind::Number:     return "Number";
        case TokenKind::Identifier: return "Identifier";
        case TokenKind::Operator:   return "Operator";
        case TokenKind::LParen:     return "LParen";
        case TokenKind::RParen:     return "RParen";
        case TokenKind::Comma:      return "Comma";
        case TokenKind::End:        return "End";
    }

    return "Unknown";
}

namespace{

bool isSpace(char c) {
    return c == ' '  ||
           c == '\t' ||
           c == '\n' ||
           c == '\r' ||
           c == '\f' ||
           c == '\v';
}

[[noreturn]]
void lexicalError(std::size_t position, const std::string& message) {
    std::stringstream ss;
    ss << "Lexical Error at position " << position << " : " << message;
    throw std::runtime_error(ss.str());
}

}

Lexical::Lexical(std::string words) : words(std::move(words)) {}

std::vector<Token> Lexical::tokenize() const {
    static const std::regex numberPattern(
        R"(([0-9]+(\.[0-9]*)?|\.[0-9]+)([eE][+-]?[0-9]+)?)"
    );

    static const std::regex identifierPattern(
        R"([A-Za-z_][A-Za-z0-9_]*)"
    );

    const auto& operators = operatorTable();

    std::vector<Token> tokens;
    std::size_t position = 0;

    while (position < words.size()) { 
        while (position < words.size() && isSpace(words[position])) {
            position++;
        }
        if (position == words.size()) {
            break;
        }

        std::size_t begin = position;
        while (position < words.size() && !isSpace(words[position])) {
            position++;
        }
        std::string text = words.substr(begin, position - begin);

        if (text == "(") {
            tokens.push_back(Token{
                TokenKind::LParen,
                "(",
                begin
            });
            continue;
        }

        if (text == ")") {
            tokens.push_back(Token{
                TokenKind::RParen,
                ")",
                begin
            });
            continue;
        }

        if (text == ",") {
            tokens.push_back(Token{
                TokenKind::Comma,
                ",",
                begin
            });
            continue;
        }

        const auto opIt = operators.find(text);
        if (opIt != operators.end()) {
            tokens.push_back(Token{
                TokenKind::Operator,
                text,
                begin,
                0.0,
                opIt->second.id,
            });
            continue;
        }

        if (std::regex_match(text, numberPattern)) {
            double value = 0.0;

            std::istringstream input(text);
            input.imbue(std::locale::classic());

            if (!(input >> value) || !std::isfinite(value)) {
                lexicalError(
                    begin,
                    "Number cannot be represented as a finite double : " +
                    text
                );
            }

            tokens.push_back({
                TokenKind::Number,
                text,
                begin,
                value
            });
            continue;
        }

        if (std::regex_match(text, identifierPattern)) {
            tokens.push_back({
                TokenKind::Identifier,
                text,
                begin
            });
            continue;
        }
        
        else {
            lexicalError(begin, "Invalid token : " + text);
        }
    }
    tokens.push_back({
        TokenKind::End,
        "",
        position
    });
    return tokens;
}