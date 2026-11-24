#include "lexer/token.h"

namespace xenon::lexer {

std::string_view to_string(TokenKind k) noexcept {
#define S(A,B) case TokenKind::A: return B
    switch(k) {
        S(Identifier,"Identifier"); S(Underscore,"Underscore"); S(PctIdent,"PctIdent"); S(CtIdent,"CtIdent");
        S(Module,"Module"); S(Import,"Import"); S(Type,"Type"); S(Impl,"Impl"); S(Func,"Func");
        S(Let,"Let"); S(Static,"Static"); S(Const,"Const"); S(Mut,"Mut"); S(Auto,"Auto");
        S(Pub,"Pub"); S(Self,"Self"); S(Return,"Return"); S(If,"If"); S(Else,"Else");
        S(While,"While"); S(Do,"Do"); S(In,"In"); S(Break,"Break"); S(Continue,"Continue");
        S(True,"True"); S(False,"False"); S(Nullptr,"Nullptr"); S(BuiltinType,"BuiltinType");
        S(IntLiteral,"IntLiteral"); S(FloatLiteral,"FloatLiteral"); S(StringLiteral,"StringLiteral");
        S(RuneLiteral,"RuneLiteral");
        S(ColonColon,"::"); S(Arrow,"->"); S(DoubleStar,"**"); S(DoubleStarEq,"**=");
        S(LessLess,"<<"); S(GreaterGreater,">>"); S(LessLessEq,"<<="); S(GreaterGreaterEq,">>=");
        S(EqEq,"=="); S(BangEq,"!="); S(LessEq,"<="); S(GreaterEq,">=");
        S(AmpAmp,"&&"); S(PipePipe,"||"); S(PlusEq,"+="); S(MinusEq,"-=");
        S(StarEq,"*="); S(SlashEq,"/="); S(AmpEq,"&="); S(PipeEq,"|=");
        S(CaretEq,"^="); S(PlusPlus,"++"); S(MinusMinus,"--");
        S(Plus,"+"); S(Minus,"-"); S(Star,"*"); S(Slash,"/"); S(Eq,"=");
        S(Less,"<"); S(Greater,">"); S(Bang,"!"); S(Tilde,"~"); S(Amp,"&"); S(Pipe,"|");
        S(Caret,"^"); S(Colon,":"); S(Semicolon,";"); S(Comma,","); S(Dot,"."); S(Question,"?");
        S(LParen,"("); S(RParen,")"); S(LBrace,"{"); S(RBrace,"}"); S(LBracket,"["); S(RBracket,"]");
        S(Eof,"Eof"); S(Error,"Error");
    }
#undef S
    return "Unknown";
}

} // namespace xenon::lexer
