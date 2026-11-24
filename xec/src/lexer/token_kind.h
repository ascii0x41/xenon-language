#pragma once

namespace xenon::lexer {

enum class TokenKind {
    Identifier, Underscore, PctIdent, CtIdent,
    Module, Import, Type, Impl, Func, Let, Static, Const, Mut, Auto, Pub,
    Self, Return, If, Else, While, Do, In, Break, Continue,
    True, False, Nullptr, BuiltinType,
    IntLiteral, FloatLiteral, StringLiteral, RuneLiteral,
    ColonColon, Arrow, DoubleStar, DoubleStarEq, LessLess, GreaterGreater,
    LessLessEq, GreaterGreaterEq, EqEq, BangEq, LessEq, GreaterEq,
    AmpAmp, PipePipe, PlusEq, MinusEq, StarEq, SlashEq, AmpEq, PipeEq,
    CaretEq, PlusPlus, MinusMinus,
    Plus, Minus, Star, Slash, Eq, Less, Greater, Bang, Tilde, Amp, Pipe,
    Caret, Colon, Semicolon, Comma, Dot, Question,
    LParen, RParen, LBrace, RBrace, LBracket, RBracket, Eof, Error,

    LET=Let, FUNC=Func, TYPE=Type, IMPL=Impl, STATIC=Static, PUB=Pub,
    MUT=Mut, RAW=Static, SELF=Self, IF=If, ELSE=Else, WHILE=While,
    FOR=Identifier, IN=In, RETURN=Return, BREAK=Break, CONTINUE=Continue,
    IMPORT=Import, MODULE=Module, USE=Identifier, TRUE=True, FALSE=False,
    NULLPTR=Nullptr, IDENTIFIER=Identifier, INT_LITERAL=IntLiteral,
    FLOAT_LITERAL=FloatLiteral, STRING_LITERAL=StringLiteral,
    CHARACTER_LITERAL=RuneLiteral, DOC_COMMENT=Identifier,
    MODULE_DOC_COMMENT=Identifier, EQ=Eq, PLUS_EQ=PlusEq, MINUS_EQ=MinusEq,
    STAR_EQ=StarEq, SLASH_EQ=SlashEq, PERCENT=Identifier, PERCENT_EQ=Identifier,
    AMP_EQ=AmpEq, PIPE_EQ=PipeEq, CARET_EQ=CaretEq, LT_LT_EQ=LessLessEq,
    GT_GT_EQ=GreaterGreaterEq, PLUS=Plus, MINUS=Minus, STAR=Star,
    SLASH=Slash, EQ_EQ=EqEq, BANG_EQ=BangEq, LT=Less, GT=Greater,
    LTE=LessEq, GTE=GreaterEq, BANG=Bang, AND=AmpAmp, OR=PipePipe,
    AMP=Amp, PIPE=Pipe, TILDE=Tilde, CARET=Caret, LT_LT=LessLess,
    GT_GT=GreaterGreater, QUESTION=Question, LPAREN=LParen, RPAREN=RParen,
    LBRACE=LBrace, RBRACE=RBrace, LBRACKET=LBracket, RBRACKET=RBracket,
    COMMA=Comma, SEMICOLON=Semicolon, COLON=Colon, COLON_COLON=ColonColon,
    DOT=Dot, ARROW=Arrow, HASH=Identifier, EOF_TOKEN=Eof
};

} // namespace xenon::lexer
