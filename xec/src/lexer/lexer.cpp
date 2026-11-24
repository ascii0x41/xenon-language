#include "lexer/lexer.h"

#include "character_utils.h"

#include <unordered_set>
#include <unordered_map>
#include <string_view>
#include <utility>

namespace xenon::lexer {
namespace {

bool ident_start(char c) { return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || c == '_' || static_cast<unsigned char>(c) >= 0x80; }
bool ident_continue(char c) { return ident_start(c) || (c >= '0' && c <= '9'); }
bool digit(char c) { return c >= '0' && c <= '9'; }
bool hex(char c) { return digit(c) || (c >= 'a' && c <= 'f') || (c >= 'A' && c <= 'F'); }
bool bin(char c) { return c == '0' || c == '1'; }
bool oct(char c) { return c >= '0' && c <= '7'; }

const std::unordered_map<std::string_view, TokenKind> keywords = {
 {"module",TokenKind::Module},{"import",TokenKind::Import},{"type",TokenKind::Type},
 {"impl",TokenKind::Impl},{"func",TokenKind::Func},{"let",TokenKind::Let},
 {"static",TokenKind::Static},{"const",TokenKind::Const},{"mut",TokenKind::Mut},
 {"auto",TokenKind::Auto},{"pub",TokenKind::Pub},{"self",TokenKind::Self},
 {"return",TokenKind::Return},{"if",TokenKind::If},{"else",TokenKind::Else},
 {"while",TokenKind::While},{"do",TokenKind::Do},{"in",TokenKind::In},
 {"break",TokenKind::Break},{"continue",TokenKind::Continue},
 {"true",TokenKind::True},{"false",TokenKind::False},{"nullptr",TokenKind::Nullptr}
};

bool builtin(std::string_view s) {
    static const std::unordered_set<std::string_view> names = {
      "str","bool","rune","char","u8","u16","u32","u64","usize",
      "i8","i16","i32","i64","isize","f32","f64"};
    return names.contains(s);
}
bool int_suffix(std::string_view s) {
    static const std::unordered_set<std::string_view> names = {
      "u8","u16","u32","u64","usize","i8","i16","i32","i64","isize"};
    return names.contains(s);
}
bool float_suffix(std::string_view s) { return s == "f32" || s == "f64"; }

} // namespace

Lexer::Lexer(const source::SourceFile& source) : source_(source), text_(source.text()) {}
bool Lexer::is_at_end() const noexcept { return current_ >= text_.size(); }
char Lexer::peek() const noexcept { return is_at_end() ? '\0' : text_[current_]; }
char Lexer::peek_next() const noexcept { return current_ + 1 >= text_.size() ? '\0' : text_[current_+1]; }
char Lexer::peek_at(std::size_t i) const noexcept { return i >= text_.size() ? '\0' : text_[i]; }
char Lexer::advance() noexcept { return is_at_end() ? '\0' : text_[current_++]; }
bool Lexer::match(char c) noexcept { if (peek()!=c) return false; ++current_; return true; }

void Lexer::emit(TokenStream& out, TokenKind kind, std::size_t begin, std::size_t end) {
    out.push_back(Token{kind, source::Span{source_.id(), begin, end}, {}, NumericBase::Decimal, {}, {}});
}

void Lexer::report(std::vector<diagnostics::Diagnostic>& ds, std::size_t begin, std::size_t end, const char* message) {
    ds.push_back(diagnostics::Diagnostic{diagnostics::Severity::error, message,
        source::Span{source_.id(), begin, end < begin ? begin : end}});
}

void Lexer::scan_identifier(TokenStream& out) {
    ++current_; while (ident_continue(peek())) ++current_;
    std::string_view lexeme=text_.substr(start_, current_-start_);
    if (lexeme == "_") emit(out, TokenKind::Underscore, start_, current_);
    else if (keywords.contains(lexeme)) emit(out, keywords.at(lexeme), start_, current_);
    else if (builtin(lexeme)) emit(out, TokenKind::BuiltinType, start_, current_);
    else emit(out, TokenKind::Identifier, start_, current_);
}

void Lexer::scan_sigiled(TokenStream& out, std::vector<diagnostics::Diagnostic>& ds, bool pct) {
    ++current_;
    if (!ident_start(peek())) {
        report(ds,start_,current_, pct ? "A lone '%' is not valid" : "A lone '$' is not valid"); emit(out,TokenKind::Error,start_,current_); return;
    }
    while (ident_continue(peek())) ++current_;
    std::string_view lexeme=text_.substr(start_+1,current_-start_-1);
    if (lexeme.empty()) return;
    Token t{pct?TokenKind::PctIdent:TokenKind::CtIdent,source::Span{source_.id(),start_,current_},{},NumericBase::Decimal,lexeme,{}};
    if (!pct && lexeme == "if") t.flags.ct_if = true;
    if (!pct && lexeme == "else") t.flags.ct_else = true;
    out.push_back(t);
}

void Lexer::scan_number(TokenStream& out, std::vector<diagnostics::Diagnostic>& ds) {
    const size_t digit_begin=start_; NumericBase base=NumericBase::Decimal;
    if (text_[start_] == '0' && current_+1<text_.size() && (text_[start_+1]=='x'||text_[start_+1]=='X'||text_[start_+1]=='b'||text_[start_+1]=='B'||text_[start_+1]=='o'||text_[start_+1]=='O')) {
        char p=text_[start_+1]; ++current_; ++current_; // prefix bytes
        base=p=='x'||p=='X'?NumericBase::Hex:p=='b'||p=='B'?NumericBase::Binary:NumericBase::Octal;
        auto valid=[&](char c){ return base==NumericBase::Hex?hex(c):base==NumericBase::Binary?bin(c):oct(c); };
        bool got=false;
        while (!is_at_end()) { if(valid(peek())) {got=true;++current_;} else if(peek()=='_' && valid(peek_next())) ++current_; else break; }
        if(!got) { report(ds,start_,current_,"Expected digits after numeric base prefix"); emit(out,TokenKind::Error,start_,current_); return; }
        size_t digits_end=current_; std::string_view digit_text=text_.substr(digit_begin,digits_end-digit_begin);
        size_t suffix_begin=current_; while (ident_continue(peek())) ++current_; std::string_view suffix=text_.substr(suffix_begin,current_-suffix_begin);
        if (!suffix.empty() && !int_suffix(suffix)) { report(ds,suffix_begin,current_,"Unknown integer suffix"); emit(out,TokenKind::Error,start_,current_); return; }
        out.push_back(Token{TokenKind::IntLiteral,source::Span{source_.id(),start_,current_},{suffix.empty()},base,digit_text,suffix});
        return;
    }

    while (digit(peek()) || (peek()=='_' && digit(peek_next()))) ++current_;
    bool is_float=false;
    if (peek()=='.' && digit(peek_next())) { is_float=true; ++current_; while(digit(peek())||(peek()=='_'&&digit(peek_next()))) ++current_; }
    if ((peek()=='e'||peek()=='E') && digit(peek_next())) {
        is_float=true; ++current_; if(peek()== '+'||peek()=='-') ++current_; if(!digit(peek())) report(ds,current_,current_,"Expected exponent digits"); while(digit(peek())||(peek()=='_'&&digit(peek_next()))) ++current_;
    } else if ((peek()=='e'||peek()=='E') && (peek_next()== '+'||peek_next()=='-')) {
        is_float=true; ++current_; ++current_; if(!digit(peek())) report(ds,current_,current_,"Expected exponent digits"); while(digit(peek())||(peek()=='_'&&digit(peek_next()))) ++current_;
    }
    size_t digits_end=current_; std::string_view digit_text=text_.substr(digit_begin,digits_end-digit_begin);
    size_t suffix_begin=current_; while (ident_continue(peek())) ++current_; std::string_view suffix=text_.substr(suffix_begin,current_-suffix_begin);
    if (!suffix.empty()) {
        if(is_float && !float_suffix(suffix)) { report(ds,suffix_begin,current_,"Unknown or invalid float suffix"); emit(out,TokenKind::Error,start_,current_); return; }
        if(!is_float && !int_suffix(suffix)) { report(ds,suffix_begin,current_,"Unknown integer suffix"); emit(out,TokenKind::Error,start_,current_); return; }
    }
    out.push_back(Token{is_float?TokenKind::FloatLiteral:TokenKind::IntLiteral,source::Span{source_.id(),start_,current_},{suffix.empty()},base,digit_text,suffix});
}

void Lexer::scan_string(TokenStream& out, std::vector<diagnostics::Diagnostic>& ds) {
    std::size_t esc_start=0;
    while (!is_at_end()) {
        char c=peek();
        if(c=='"') { ++current_; emit(out,TokenKind::StringLiteral,start_,current_); return; }
        if(c=='\n'||c=='\r') { report(ds,start_,current_,"Unterminated string literal"); emit(out,TokenKind::Error,start_,current_); return; }
        if(c=='\\') {
            esc_start=current_; ++current_; if(is_at_end()) { report(ds,esc_start,current_,"Unterminated escape sequence"); emit(out,TokenKind::Error,start_,current_); return; }
            c=peek(); ++current_;
            if(c!='n'&&c!='r'&&c!='t'&&c!='0'&&c!='\\'&&c!='\''&&c!='"') {
                if(c=='x') {
                    if(!hex(peek())||!hex(peek_next())) { report(ds,esc_start,current_,"Invalid hexadecimal escape"); emit(out,TokenKind::Error,esc_start,current_); return; }
                    ++current_; ++current_;
                } else if(c=='u') { if(!match('{')) report(ds,esc_start,current_,"Expected '{' after '\\u'"); while(hex(peek())) ++current_; if(!match('}')) report(ds,esc_start,current_,"Invalid Unicode escape"); }
                else { report(ds,esc_start,current_,"Invalid escape sequence"); emit(out,TokenKind::Error,esc_start,current_); return; }
            }
        } else ++current_;
    }
    report(ds,start_,current_,"Unterminated string literal"); emit(out,TokenKind::Error,start_,current_);
}

void Lexer::scan_rune(TokenStream& out, std::vector<diagnostics::Diagnostic>& ds) {
     if(is_at_end()) { report(ds,start_,current_,"Unterminated rune literal"); emit(out,TokenKind::Error,start_,current_); return; }
    if(peek()== '\n'||peek()=='\r') { report(ds,start_,current_,"Unterminated rune literal"); emit(out,TokenKind::Error,start_,current_); return; }
    if(peek()=='\\') {
        size_t esc=start_; ++current_; if(is_at_end()) { report(ds,esc,current_,"Unterminated escape sequence"); emit(out,TokenKind::Error,start_,current_); return; }
        char c=peek(); ++current_; if(c!='n'&&c!='r'&&c!='t'&&c!='0'&&c!='\\'&&c!='\''&&c!='"') {
            report(ds,esc,current_,"Invalid escape in rune literal"); emit(out,TokenKind::Error,start_,current_); if(peek()== '\'') ++current_; return;
        }
    } else ++current_;
    if(!match('\'')) { while(!is_at_end()&&peek()!= '\''&&peek()!='\n') ++current_; if(match('\'')) {} else report(ds,start_,current_,"Unterminated rune literal"); emit(out,TokenKind::Error,start_,current_); return; }
    emit(out,TokenKind::RuneLiteral,start_,current_);
}

void Lexer::scan(TokenStream& out, std::vector<diagnostics::Diagnostic>& ds) {
    while (is_at_end()) return;
    char c=advance();
    if(c=='%'||c=='$') { --current_; scan_sigiled(out,ds,c=='%'); return; }
    if(c=='_' && ident_continue(peek())) { --current_; scan_identifier(out); return; }
    if(ident_start(c)) { --current_; scan_identifier(out); return; }
    if(digit(c)) { --current_; scan_number(out,ds); return; }

    switch(c) {
      case '(': emit(out,TokenKind::LParen,start_,current_); break;
      case ')': emit(out,TokenKind::RParen,start_,current_); break;
      case '{': emit(out,TokenKind::LBrace,start_,current_); break;
      case '}': emit(out,TokenKind::RBrace,start_,current_); break;
      case '[': emit(out,TokenKind::LBracket,start_,current_); break;
      case ']': emit(out,TokenKind::RBracket,start_,current_); break;
      case ',': emit(out,TokenKind::Comma,start_,current_); break;
      case ';': emit(out,TokenKind::Semicolon,start_,current_); break;
      case '?': emit(out,TokenKind::Question,start_,current_); break;
      case '~': emit(out,TokenKind::Tilde,start_,current_); break;
      case '.': emit(out,TokenKind::Dot,start_,current_); break;
      case '#': emit(out, TokenKind::HASH, start_, current_); break;
      case ':': if(match(':')) emit(out,TokenKind::ColonColon,start_,current_); else emit(out,TokenKind::Colon,start_,current_); break;
      case '+': if(match('+')) emit(out,TokenKind::PlusPlus,start_,current_); else if(match('=')) emit(out,TokenKind::PlusEq,start_,current_); else emit(out,TokenKind::Plus,start_,current_); break;
      case '-': if(match('>')) emit(out,TokenKind::Arrow,start_,current_); else if(match('-')) emit(out,TokenKind::MinusMinus,start_,current_); else if(match('=')) emit(out,TokenKind::MinusEq,start_,current_); else emit(out,TokenKind::Minus,start_,current_); break;
      case '*': if(match('*')) { if(match('=')) emit(out,TokenKind::DoubleStarEq,start_,current_); else emit(out,TokenKind::DoubleStar,start_,current_); } else if(match('=')) emit(out,TokenKind::StarEq,start_,current_); else emit(out,TokenKind::Star,start_,current_); break;
      // TODO(OPEN-BLOCK-COMMENTS): block comments are undecided in the EBNF; this lexer only skips // lines.
      case '/': if(match('/')) { while(peek()!='\n' && !is_at_end()) ++current_; } else if(match('=')) emit(out,TokenKind::SlashEq,start_,current_); else emit(out,TokenKind::Slash,start_,current_); break;
      case '=': if(match('=')) emit(out,TokenKind::EqEq,start_,current_); else emit(out,TokenKind::Eq,start_,current_); break;
      case '!': if(match('=')) emit(out,TokenKind::BangEq,start_,current_); else emit(out,TokenKind::Bang,start_,current_); break;
      case '<': if(match('<')) { if(match('=')) emit(out,TokenKind::LessLessEq,start_,current_); else emit(out,TokenKind::LessLess,start_,current_); } else if(match('=')) emit(out,TokenKind::LessEq,start_,current_); else emit(out,TokenKind::Less,start_,current_); break;
      case '>': if(match('>')) { if(match('=')) emit(out,TokenKind::GreaterGreaterEq,start_,current_); else emit(out,TokenKind::GreaterGreater,start_,current_); } else if(match('=')) emit(out,TokenKind::GreaterEq,start_,current_); else emit(out,TokenKind::Greater,start_,current_); break;
      case '&': if(match('&')) emit(out,TokenKind::AmpAmp,start_,current_); else if(match('=')) emit(out,TokenKind::AmpEq,start_,current_); else emit(out,TokenKind::Amp,start_,current_); break;
      case '|': if(match('|')) emit(out,TokenKind::PipePipe,start_,current_); else if(match('=')) emit(out,TokenKind::PipeEq,start_,current_); else emit(out,TokenKind::Pipe,start_,current_); break;
      case '^': if(match('=')) emit(out,TokenKind::CaretEq,start_,current_); else emit(out,TokenKind::Caret,start_,current_); break;
      case '"': scan_string(out,ds); break;
      case '\'': scan_rune(out,ds); break;
      case ' ': case '\t': case '\r': case '\n': break;
      default: report(ds,start_,current_,"Unexpected character"); emit(out,TokenKind::Error,start_,current_); break;
    }

}

result::Result<TokenStream> Lexer::lex(const source::SourceFile& source) {
    Lexer lexer(source);
    auto output=lexer.tokenize();
    std::vector<diagnostics::Diagnostic> ds(output.diagnostics().diagnostics());
    return {std::move(output.value()),std::move(ds),output.success()};
}

result::Output<TokenStream> Lexer::lex_file(const source::SourceFile& source) { Lexer lexer(source); return lexer.tokenize(); }

result::Result<Token> Lexer::next() {
    TokenStream out; std::vector<diagnostics::Diagnostic> ds;
    if(is_at_end()) return {Token{TokenKind::Eof,source::Span{source_.id(),current_,current_},{},NumericBase::Decimal,{},{}},{},true};
    start_=current_; scan(out,ds);
    if(out.empty()) return {diagnostics::Diagnostic{diagnostics::Severity::error,"No token",std::nullopt}};
    if(!ds.empty()) return {std::move(out.front()),std::move(ds),false};
    return {std::move(out.front()),{},true};
}

result::Output<TokenStream> Lexer::tokenize() {
    TokenStream out; std::vector<diagnostics::Diagnostic> ds;
    while(!is_at_end()) { start_=current_; scan(out,ds); }
    out.push_back(Token{TokenKind::Eof,source::Span{source_.id(),current_,current_},{},NumericBase::Decimal,{}, {}});
    diagnostics::DiagnosticCollector collector; for(auto& d:ds) collector.report(std::move(d));
    return {std::move(out),std::move(collector),!collector.has_errors()};
}

} // namespace xenon::lexer
