# MINAX Compiler

This is a compiler for converting the MINAX math expression strings to the computable objects.

## String Convention

For convenience, we make the following conventions for the MINAX strings :
* The MINAX string is composed of the following tokens : "Number", "Identifier", "Operator", "LParen", "RParen", "Comma", "End".
  
Among them, identifier contains variables names functions names.Operator currently contains "+", "-", "*", "/", "=", "==", "!=", "<", ">", "<=", ">=".

* Each token is separated by a space, which is more convenient for the parser.The front-end input device will ensure the implementation of this convention.

For example, the MINAX string "1 + 2 * 3" will be converted to the following tokens : "1", "+", "2", "*", "3".the string "1 \*2" is an invalid MINAX string because there is no space between "\*" and "2".

## Step1 : Lexical Analysis

### We design the following structure for tokens :

```c++

struct Token {
    TokenKind kind;
    std::string text;
    std::size_t position;
    double number = 0.0;
    OperatorId op = OperatorId::None;
};

```

* Tokenkind which is mentioned above includes "Number", "Identifier", "Operator", "LParen", "RParen", "Comma", "End".

```c++

enum class TokenKind {
    Number,
    Identifier,
    Operator,
    LParen,
    RParen,
    Comma,
    End
};

```

* Text is the original string of the token.
* Position is the position of the token in the original string, which is used for error reporting.
* Number is the number value of the token, which is only used for number tokens.
* Op is the operator id of the token, which is only used for operator tokens and extensible.It contains all the operators mentioned above.

```c++

enum class OperatorId {
    None,

    Add,
    Subtract,
    Multiply,
    Divide,

    Equal,
    NotEqual,
    Greater,
    Less,
    GreaterEqual,
    LessEqual
};

```

With the above preparations, we can start to implement the lexical analysis.

### Assistant functions

```c++
    const std::unordered_map<std::string, OperatorInfo>& operatorTable();
    bool isSpace(char c);
    void lexicalError(std::size_t position, const std::string& message);
```

* operatorTable() is a function that returns the operator table, which is a hash table that maps operator names to operator ids.This table can easily be extended to support more operators and simplify the implementation of the lexical analysis.
Here is the design of related data structure :
```c++

enum class OperatorCategory {
    Arithmetic,
    Comparison
};

enum class Associativity {
    Left,
    Right,
    NonAssociative
};

struct OperatorInfo {
    OperatorId id;
    OperatorCategory category;
    int precedence;
    Associativity associativity;
};

```

Example of the implementation of the operator table : 
```c++
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
    "*",
    {
        OperatorId::Multiply,
        OperatorCategory::Arithmetic,
        30,
        Associativity::Left
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
}

```

* isSpace() is a function that returns true if the character is a space character.

* lexicalError() is a function that reports a lexical error.It takes the position of the error and the error message as parameters.

### Tokenize

Becase of the simplicity of the MINAX language, we can implement the lexical analysis in a simple way.We just need to split the string into tokens by spaces."(" and ")" are the easiest tokens to deal with. operators are also easy to deal with, because they are all saved in the operator table.Numers and identifiers are more complicated, because they can contain letters and numbers. We can use regular expressions to deal with them, which is a powerful tool for string processing and supported by C++.As for the invalid MINAX strings, we can use the lexicalError() function to report them.Don't forget to add the "End" token at the end of the string.

Here is the implementation of the tokenize() function :
```c++

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

```

### Testing

We can use the following commands to test the lexical analysis(Mingw64) :

```bash

g++ -std=c++17 -Iinclude src/Lexical.cpp test/LexicalTest.cpp -o lexicaltest
./lexicaltest

```

You can append your own test cases to the LexicalTest.cpp file or delete the existing ones.