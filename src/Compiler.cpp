#include "Compiler.h"

#include "Core.h"
#include "Scanner.h"

Compiler::Compiler()
{
    Init();
}

void Compiler::Init()
{
    auto toInt = [](TokenType type) { return static_cast<u32>(type); };
    auto& rules = m_ParseRules;
    rules[toInt(TokenType::LeftParen)]    = { &Compiler::Grouping,   nullptr,            Precedence::Order::None };
    rules[toInt(TokenType::RightParen)]   = { nullptr,               nullptr,            Precedence::Order::None };
    rules[toInt(TokenType::LeftBrace)]    = { nullptr,               nullptr,            Precedence::Order::None };
    rules[toInt(TokenType::RightBrace)]   = { nullptr,               nullptr,            Precedence::Order::None };
    rules[toInt(TokenType::Comma)]        = { nullptr,               nullptr,            Precedence::Order::None };
    rules[toInt(TokenType::Dot)]          = { nullptr,               nullptr,            Precedence::Order::None };
    rules[toInt(TokenType::Minus)]        = { &Compiler::Unary,      &Compiler::Binary,  Precedence::Order::Term };
    rules[toInt(TokenType::Plus)]         = { nullptr,               &Compiler::Binary,  Precedence::Order::Term };
    rules[toInt(TokenType::Semicolon)]    = { nullptr,               nullptr,            Precedence::Order::None };
    rules[toInt(TokenType::Slash)]        = { nullptr,               &Compiler::Binary,  Precedence::Order::Factor };
    rules[toInt(TokenType::Star)]         = { nullptr,               &Compiler::Binary,  Precedence::Order::Factor };
    rules[toInt(TokenType::Bang)]         = { &Compiler::Unary,      nullptr,            Precedence::Order::None };
    rules[toInt(TokenType::BangEqual)]    = { nullptr,               &Compiler::Binary,  Precedence::Order::Equals };
    rules[toInt(TokenType::Equal)]        = { nullptr,               nullptr,            Precedence::Order::None };
    rules[toInt(TokenType::EqualEqual)]   = { nullptr,               &Compiler::Binary,  Precedence::Order::Equals };
    rules[toInt(TokenType::Greater)]      = { nullptr,               &Compiler::Binary,  Precedence::Order::Comparison };
    rules[toInt(TokenType::GreaterEqual)] = { nullptr,               &Compiler::Binary,  Precedence::Order::Comparison };
    rules[toInt(TokenType::Less)]         = { nullptr,               &Compiler::Binary,  Precedence::Order::Comparison };
    rules[toInt(TokenType::LessEqual)]    = { nullptr,               &Compiler::Binary,  Precedence::Order::Comparison };
    rules[toInt(TokenType::Identifier)]   = { nullptr,               nullptr,            Precedence::Order::None };
    rules[toInt(TokenType::String)]       = { nullptr,               nullptr,            Precedence::Order::None };
    rules[toInt(TokenType::Number)]       = { &Compiler::Number,     nullptr,            Precedence::Order::None };
    rules[toInt(TokenType::And)]          = { nullptr,               nullptr,            Precedence::Order::None };
    rules[toInt(TokenType::Class)]        = { nullptr,               nullptr,            Precedence::Order::None };
    rules[toInt(TokenType::Else)]         = { nullptr,               nullptr,            Precedence::Order::None };
    rules[toInt(TokenType::False)]        = { &Compiler::False,      nullptr,            Precedence::Order::None };
    rules[toInt(TokenType::Fun)]          = { nullptr,               nullptr,            Precedence::Order::None };
    rules[toInt(TokenType::For)]          = { nullptr,               nullptr,            Precedence::Order::None };
    rules[toInt(TokenType::If)]           = { nullptr,               nullptr,            Precedence::Order::None };
    rules[toInt(TokenType::Nil)]          = { &Compiler::Nil,        nullptr,            Precedence::Order::None };
    rules[toInt(TokenType::Or)]           = { nullptr,               nullptr,            Precedence::Order::None };
    rules[toInt(TokenType::Print)]        = { nullptr,               nullptr,            Precedence::Order::None };
    rules[toInt(TokenType::Return)]       = { nullptr,               nullptr,            Precedence::Order::None };
    rules[toInt(TokenType::Super)]        = { nullptr,               nullptr,            Precedence::Order::None };
    rules[toInt(TokenType::This)]         = { nullptr,               nullptr,            Precedence::Order::None };
    rules[toInt(TokenType::True)]         = { &Compiler::True,       nullptr,            Precedence::Order::None };
    rules[toInt(TokenType::Var)]          = { nullptr,               nullptr,            Precedence::Order::None };
    rules[toInt(TokenType::While)]        = { nullptr,               nullptr,            Precedence::Order::None };
    rules[toInt(TokenType::Eof)]          = { nullptr,               nullptr,            Precedence::Order::None };
    rules[toInt(TokenType::Error)]        = { nullptr,               nullptr,            Precedence::Order::None };
}           

CompilerResult Compiler::Compile(std::string_view source)
{
    {
        // scanning
        Scanner scanner(source);
        m_Tokens = scanner.ScanTokens();
        if (scanner.HadError())
        {
            PrintParseErrors();
            return CompilerResult{true, {}};
        }
    }
    Chunk chunk;
    m_CurrentChunk = &chunk;

    if (m_Tokens.size() > 1)
    {
        Expression();
    }
    Consume(TokenType::Eof, "Expected end of expression.");
    
    OnCompileEnd();
    
    return CompilerResult{!m_HadError, chunk};
}

bool Compiler::IsAtEnd() const
{
    return m_Tokens[m_CurrentTokenNum].Type == TokenType::Eof;
}

const Token& Compiler::Advance()
{
    m_CurrentTokenNum++;
    return Previous();
}

const Token& Compiler::Peek() const
{
    return m_Tokens[m_CurrentTokenNum];
}

const Token& Compiler::Previous() const
{
    BCVM_ASSERT(m_CurrentTokenNum != 0, "No previous token exist.")
    return m_Tokens[m_CurrentTokenNum - 1];
}

void Compiler::Consume(TokenType type, std::string_view message)
{
    if (Peek().Type == type)
    {
        Advance();
        return;
    }
    ErrorAtCurrent(message);
}

void Compiler::Expression()
{
    ParsePrecedence(Precedence::Assignment);
}

void Compiler::Grouping()
{
    Expression();
    Consume(TokenType::RightParen, "Expected ')' after expression.");
}

void Compiler::Binary()
{
    const Token& op = Previous();
    const ParseRule& rule = GetRule(op.Type);
    ParsePrecedence(static_cast<Precedence::Order>(rule.InfixPrecedence + 1));
    switch (op.Type)
    { 
    case TokenType::Minus:          EmitOperation(OpCode::OpSubtract); break;
    case TokenType::Plus:           EmitOperation(OpCode::OpAdd); break;
    case TokenType::Slash:          EmitOperation(OpCode::OpDivide); break;
    case TokenType::Star:           EmitOperation(OpCode::OpMultiply); break;
    case TokenType::EqualEqual:     EmitOperation(OpCode::OpEqual); break;
    case TokenType::BangEqual:      EmitOperation(OpCode::OpEqual); EmitOperation(OpCode::OpNot); break;
    case TokenType::Less:           EmitOperation(OpCode::OpLess); break;
    case TokenType::LessEqual:      EmitOperation(OpCode::OpLequal); break;
    case TokenType::Greater:        EmitOperation(OpCode::OpLequal); EmitOperation(OpCode::OpNot); break;
    case TokenType::GreaterEqual:   EmitOperation(OpCode::OpLess); EmitOperation(OpCode::OpNot); break;
    default: return;
    }
}

void Compiler::Unary()
{
    const Token& op = Previous();
    ParsePrecedence(Precedence::Unary);
    switch (op.Type)
    {
    case TokenType::Bang:   EmitOperation(OpCode::OpNot); break;
    case TokenType::Minus:  EmitOperation(OpCode::OpNegate); break;
    default: return;
    }
}

void Compiler::Number()
{
    f64 val = std::strtod(Previous().Lexeme.data(), nullptr);
    EmitConstant(val);
}

void Compiler::Nil()
{
    EmitOperation(OpCode::OpNil);
}

void Compiler::False()
{
    EmitOperation(OpCode::OpFalse);
}

void Compiler::True()
{
    EmitOperation(OpCode::OpTrue);
}

void Compiler::ParsePrecedence(Precedence::Order precedence)
{
    Advance();
    const ParseRule& rule = GetRule(Previous().Type);
    if (rule.Prefix == nullptr)
    {
        Error("Expected expression.");
        return;
    }
    (this->*rule.Prefix)();

    while (GetRule(m_Tokens[m_CurrentTokenNum].Type).InfixPrecedence >= precedence)
    {
        Advance();
        const ParseRule& infix = GetRule(Previous().Type);
        (this->*infix.Infix)();
    }
}

void Compiler::PrintParseErrors()
{
    for (const auto& token : m_Tokens)
    {
        if (token.Type == TokenType::Error) ErrorAt(token, token.Lexeme);
    }
}

void Compiler::ErrorAt(const Token& token, std::string_view message)
{
    if (m_IsInPanic) return;
    m_HadError = true;
    m_IsInPanic = true;
    std::string errorMessage;
    if (token.Type == TokenType::Error) errorMessage = std::format("[line {}] : {}", token.Line, message);
    else if (token.Type == TokenType::Eof) errorMessage = std::format("[line {}] at end : {}", token.Line, message);
    else errorMessage = std::format("[line {}] at {} : {}", token.Line, std::string{token.Lexeme}, message);
    LOG_ERROR("Compiler: {}", errorMessage);
}

void Compiler::ErrorAtCurrent(std::string_view message)
{
    ErrorAt(Peek(), message);
}

void Compiler::Error(std::string_view message)
{
    ErrorAt(Previous(), message);
}

void Compiler::EmitByte(u8 byte) const
{
    m_CurrentChunk->AddByte(byte, Previous().Line);
}

void Compiler::EmitOperation(OpCode opCode) const
{
    m_CurrentChunk->AddOperation(opCode, Previous().Line);
}

void Compiler::EmitConstant(Value val) const
{
    m_CurrentChunk->AddConstant(val, Previous().Line);
}

void Compiler::EmitReturn() const
{
    m_CurrentChunk->AddOperation(OpCode::OpReturn, Previous().Line);
}

void Compiler::OnCompileEnd()
{
    EmitReturn();
#ifdef DEBUG_TRACE
    if (!m_HadError) Disassembler::Disassemble(*m_CurrentChunk);
#endif
}

const ParseRule& Compiler::GetRule(TokenType tokenType) const
{
    return m_ParseRules[static_cast<u32>(tokenType)];
}
