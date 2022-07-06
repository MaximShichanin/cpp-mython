#include "lexer.h"

#include <algorithm>
#include <charconv>
#include <unordered_map>

using namespace std;

namespace parse {

bool operator==(const Token& lhs, const Token& rhs) {
    using namespace token_type;

    if (lhs.index() != rhs.index()) {
        return false;
    }
    if (lhs.Is<Char>()) {
        return lhs.As<Char>().value == rhs.As<Char>().value;
    }
    if (lhs.Is<Number>()) {
        return lhs.As<Number>().value == rhs.As<Number>().value;
    }
    if (lhs.Is<String>()) {
        return lhs.As<String>().value == rhs.As<String>().value;
    }
    if (lhs.Is<Id>()) {
        return lhs.As<Id>().value == rhs.As<Id>().value;
    }
    return true;
}

bool operator!=(const Token& lhs, const Token& rhs) {
    return !(lhs == rhs);
}

std::ostream& operator<<(std::ostream& os, const Token& rhs) {
    using namespace token_type;

#define VALUED_OUTPUT(type) \
    if (auto p = rhs.TryAs<type>()) return os << #type << '{' << p->value << '}';

    VALUED_OUTPUT(Number);
    VALUED_OUTPUT(Id);
    VALUED_OUTPUT(String);
    VALUED_OUTPUT(Char);

#undef VALUED_OUTPUT

#define UNVALUED_OUTPUT(type) \
    if (rhs.Is<type>()) return os << #type;

    UNVALUED_OUTPUT(Class);
    UNVALUED_OUTPUT(Return);
    UNVALUED_OUTPUT(If);
    UNVALUED_OUTPUT(Else);
    UNVALUED_OUTPUT(Def);
    UNVALUED_OUTPUT(Newline);
    UNVALUED_OUTPUT(Print);
    UNVALUED_OUTPUT(Indent);
    UNVALUED_OUTPUT(Dedent);
    UNVALUED_OUTPUT(And);
    UNVALUED_OUTPUT(Or);
    UNVALUED_OUTPUT(Not);
    UNVALUED_OUTPUT(Eq);
    UNVALUED_OUTPUT(NotEq);
    UNVALUED_OUTPUT(LessOrEq);
    UNVALUED_OUTPUT(GreaterOrEq);
    UNVALUED_OUTPUT(None);
    UNVALUED_OUTPUT(True);
    UNVALUED_OUTPUT(False);
    UNVALUED_OUTPUT(Eof);

#undef UNVALUED_OUTPUT

    return os << "Unknown token :("sv;
}

constexpr size_t indent_length = 2u;

const std::set<char> Lexer::op_symbols{',', '.', '\'', '\"', '+', '-', '*', '/', '(', ')', ':', '<', '>', '!', '='};

const std::map<std::string_view, Token> Lexer::reserved_words{{"class"sv, token_type::Class{}},
                                                              {"return"sv, token_type::Return{}},
                                                              {"if"sv, token_type::If{}},
                                                              {"else"sv, token_type::Else{}},
                                                              {"def"sv, token_type::Def{}},
                                                              {"print"sv, token_type::Print{}},
                                                              {"and"sv, token_type::And{}},
                                                              {"or"sv, token_type::Or{}},
                                                              {"not"sv, token_type::Not{}},
                                                              {"None"sv, token_type::None{}},
                                                              {"True"sv, token_type::True{}},
                                                              {"False"sv, token_type::False{}},
                                                              {"<="sv, token_type::LessOrEq{}},
                                                              {">="sv, token_type::GreaterOrEq{}},
                                                              {"!="sv, token_type::NotEq{}},
                                                              {"=="sv, token_type::Eq{}}};

std::string_view GetString(std::string_view& line) {
    size_t head_pos{};
    const char head = line[head_pos];
    std::string_view word;
    while(true) {
        auto tail = line.find_first_of(head, head_pos + 1u);
        if(tail == line.npos) { //unclosed string will be close force
            word = line;
            line.remove_prefix(line.size());
            break;
        }
        else if(line[tail - 1u] == '\\') { //push specsymbol to string
            head_pos = tail;
        }
        else {
            word = line.substr(0, tail + 1u);
            line.remove_prefix(tail + 1u);
            break;
        }
    }
    return word;
}

std::string_view GetOperator(std::string_view& line) {
    std::string_view word{};
    if(line.size() > 1u && line[1] == '=' &&
       (line[0] == '<' || line[0] == '>' || line[0] == '=' || line[0] == '!')) {
        word = line.substr(0, 2u);
        line.remove_prefix(2u);
    }
    else {
        word = line.substr(0, 1u);
        line.remove_prefix(1u);
    }
    return word;
}

std::string_view GetWord(std::string_view& line) {
    auto tail = line.find_first_of(" #,.\'\"+-*/():<>!="sv, 1u);
    std::string_view word = line.substr(0, tail);
    if(tail == line.npos || line[tail] == '#') {
        line.remove_prefix(line.size());
    }
    else {
        line.remove_prefix(word.size());
    }
    return word;
}

std::string_view GetNumber(std::string_view& line) {
    auto tail = line.find_first_not_of("0123456789"sv, 1u);
    std::string_view word = line.substr(0, tail);
    if(tail == line.npos) {
        line.remove_prefix(line.size());
    }
    else {
        line.remove_prefix(word.size());
    }
    return word;
}

std::vector<std::string_view> Lexer::ParseToWords(std::string_view raw_line) const {
    std::vector<std::string_view> result;
    result.reserve(5);
    while(true) {
        
        auto head = raw_line.find_first_not_of(" \\"sv, 0);
        if(head == raw_line.npos || raw_line[head] == '#') {
            break;
        }
        else {
            raw_line.remove_prefix(raw_line[head] == '\\' ? head + 1u : head);
        }
        
        std::string_view word = raw_line[0] == '\"' ? GetString(raw_line) //inside string
                              : raw_line[0] == '\'' ? GetString(raw_line) //also inside string
                              : op_symbols.find(raw_line[0]) != op_symbols.end() ? GetOperator(raw_line) //operator
                              : raw_line[0] >= '0' && raw_line[0] <= '9' ? GetNumber(raw_line) //number
                              : GetWord(raw_line); //general word
        result.push_back(word);
    }
    return result;
}

size_t GetIndent(std::string_view line) {
    size_t tail = line.find_first_not_of(' ', 0);
    if(tail % indent_length != 0) {
        throw std::invalid_argument("wrong indent size"s);
    }
    return tail / indent_length;
}

bool IsForbiddenChar(const char c) {
    return !(std::isprint(c) || c == '\n' || c == '\r' || c == '\t');
}

std::string Lexer::GetLine() const {
    if(!source_) {
        return {};
    }
    bool inside_single_quoted_str = false;
    bool inside_double_quoted_str = false;
    std::string result{};
    result.reserve(100);
    for(char c = source_.get(); source_ && c != source_.eof(); c = source_.get()) {
        if(IsForbiddenChar(c)) {
            continue;
        }
        switch(c) {
            case '\'':
                result += c;
                inside_single_quoted_str = inside_double_quoted_str ? false
                                         : !inside_single_quoted_str;
                break;
            case '\"':
                result += c;
                inside_double_quoted_str = inside_single_quoted_str ? false
                                         : !inside_double_quoted_str;
                break;
            case '\n':
                if(inside_single_quoted_str || inside_double_quoted_str) {
                    result += c;
                }
                else {
                    return result; //do not append endline to result
                }
                break;
            default:
                result += c;
                break;
        }
    }
    if(inside_single_quoted_str) {
        result += '\'';
    }
    else if(inside_double_quoted_str) {
        result += '\"';
    }
    return result;
}

Lexer::Lexer(std::istream& input) 
    : source_(input) {
    ParseNextLine();
}

const Token& Lexer::CurrentToken() const {
    return bufer_.current_token;
}

Token Lexer::NextToken() {
    if(bufer_.current_token == token_type::Eof{}) {
        return bufer_.current_token;
    }
    else if(++bufer_.current_token_pos >= bufer_.tokens_on_line.size()) {
        ParseNextLine();
        return bufer_.current_token;
    }
    return bufer_.current_token = bufer_.tokens_on_line.at(bufer_.current_token_pos);
}

std::string GetCleanedString(std::string_view raw_string) {
    std::string result{raw_string.substr(1u, raw_string.size() - 2u)}; //quots are no need there
    for(auto pos_to_remove = result.find('\\'); pos_to_remove != result.npos; pos_to_remove = result.find('\\')) {
        switch(result[pos_to_remove + 1u]) {
            case 'n':
                result[pos_to_remove + 1u] = '\n';
                break;
            case 't':
                result[pos_to_remove + 1u] = '\t';
                break;
        }
        result.erase(pos_to_remove, 1u);
    }
    return result;
}

void Lexer::ParseNextLine() {
    bufer_.tokens_on_line.clear();
    bufer_.current_token_pos = 0;
    if(!source_) {
        for( ; bufer_.current_indent != 0; --bufer_.current_indent) {
            bufer_.tokens_on_line.push_back(token_type::Dedent{});
        }
        bufer_.tokens_on_line.push_back(token_type::Eof{});
        bufer_.current_token = bufer_.tokens_on_line.front();
        return;
    }
    std::string line = std::move(GetLine());
    auto words = std::move(ParseToWords(line));
    if(line.empty() || words.empty() ||
       std::all_of(line.begin(), line.end(), [] (char c) {return c == ' ';})) { //pass empty string
        ParseNextLine();
        return;
    }
    size_t indent = GetIndent(line);
    while(bufer_.current_indent != indent) {
        if(indent < bufer_.current_indent) {
            bufer_.tokens_on_line.push_back(token_type::Dedent{});
            --bufer_.current_indent;
        }
        else {
            bufer_.tokens_on_line.push_back(token_type::Indent{});
            ++bufer_.current_indent;
        }
    }
    for(const auto word : words) {
        //checking for string token
        if(word.front() == '\'' || word.front() == '\"') {
            bufer_.tokens_on_line.push_back(token_type::String{std::move(GetCleanedString(word))});
            continue;
        }
        //checking for reserved words
        auto iter = reserved_words.find(word);
        if(iter != reserved_words.end()) {
            bufer_.tokens_on_line.push_back(iter->second);
        }
        //cheking for single operators
        else if(word.size() == 1u && (op_symbols.find(word.front()) != op_symbols.end() )) {
            bufer_.tokens_on_line.push_back(token_type::Char{word.front()});
        }
        //cheking for numbers
        else if(std::isdigit(word.front())) {
            bufer_.tokens_on_line.push_back(token_type::Number{std::stoi(std::string{word})});
        }
        //other lexem
        else {
            bufer_.tokens_on_line.push_back(token_type::Id{std::string(word)});
        }
    }
    bufer_.tokens_on_line.push_back(token_type::Newline{});
    bufer_.current_token = bufer_.tokens_on_line.front();
}

}  // namespace parse
