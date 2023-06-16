#include "Compiler.h"

#include "Core.h"
#include "Scanner.h"
#include "VirtualMachine.h"

Compiler::Compiler(VirtualMachine* vm)
    : m_VirtualMachine(vm)
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
    rules[toInt(TokenType::Identifier)]   = { &Compiler::Variable,   nullptr,            Precedence::Order::None };
    rules[toInt(TokenType::String)]       = { &Compiler::String,     nullptr,            Precedence::Order::None };
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
            return CompilerResult{false, {}};
        }
    }
    Chunk chunk;
    m_CurrentChunk = &chunk;

    while(!Match(TokenType::Eof))
    {
        Declaration();
    }
    
    OnCompileEnd();
    
    return CompilerResult{!m_HadError, chunk};
}

bool Compiler::IsAtEnd() const
{
    return m_Tokens[m_CurrentTokenNum].Type == TokenType::Eof;
}

bool Compiler::Check(TokenType type) const
{
    return Peek().Type == type;
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

void Compiler::Synchronize()
{
    m_IsInPanic = false;
    while (!IsAtEnd())
    {
        if (Previous().Type == TokenType::Semicolon) return;
        switch (Peek().Type)
        { 
        case TokenType::Class:
        case TokenType::Else: 
        case TokenType::Fun: 
        case TokenType::For: 
        case TokenType::If: 
        case TokenType::Print: 
        case TokenType::Return:
        case TokenType::Var:
        case TokenType::While:
            return;
        default: break;
        }
        Advance();
    }
}

void Compiler::Declaration()
{
    switch (Peek().Type)
    { 
    case TokenType::Var: Advance(); VarDeclaration(); break;
    default: Statement(); break;
    }
    if (m_IsInPanic) Synchronize();
}

void Compiler::VarDeclaration()
{
    u32 varName = ParseVariable("Expected variable name");
    if (Match(TokenType::Equal)) Expression();
    else EmitOperation(OpCode::OpNil);
    Consume(TokenType::Semicolon, "Expected ';' after expression.");
    DefineVariable(varName);
}

void Compiler::Statement()
{
    if (Match(TokenType::Print))
    {
        PrintStatement();
        return;
    }
    ExpressionStatement();
}

void Compiler::PrintStatement()
{
    Expression();
    Consume(TokenType::Semicolon, "Expected ';' after expression.");
    EmitOperation(OpCode::OpPrint);
}

void Compiler::ExpressionStatement()
{
    Expression();
    Consume(TokenType::Semicolon, "Expected ';' after expression.");
    EmitOperation(OpCode::OpPop);
}

void Compiler::Expression()
{
    ParsePrecedence(Precedence::Assignment);
}

void Compiler::Grouping(bool canAssign)
{
    Expression();
    Consume(TokenType::RightParen, "Expected ')' after expression.");
}

void Compiler::Binary(bool canAssign)
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

void Compiler::Unary(bool canAssign)
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

void Compiler::Number(bool canAssign)
{
    Value val;
    if (Previous().Lexeme.find('.') != std::string_view::npos)
    {
        val = std::strtod(Previous().Lexeme.data(), nullptr);
    }
    else
    {
        val = std::strtoull(Previous().Lexeme.data(), nullptr, 10);
    }
    EmitConstantCode(val);
}

void Compiler::String(bool canAssign)
{
    Token stringToken = Previous();
    ObjHandle string = m_VirtualMachine->AddString(
        std::string{stringToken.Lexeme.substr(1, stringToken.Lexeme.length() - 2)});
    EmitConstantCode(string);
}

void Compiler::Variable(bool canAssign)
{
    u32 varName = NamedVariable(Previous());
    if (canAssign && Match(TokenType::Equal))
    {
        Expression();
        EmitConstantCode((u64)varName);
        EmitOperation(OpCode::OpSetGlobal);
    }
    else
    {
        EmitConstantCode((u64)varName);
        EmitOperation(OpCode::OpReadGlobal);
    }
}

void Compiler::Nil(bool canAssign)
{
    EmitOperation(OpCode::OpNil);
}

void Compiler::False(bool canAssign)
{
    EmitOperation(OpCode::OpFalse);
}

void Compiler::True(bool canAssign)
{
    EmitOperation(OpCode::OpTrue);
}

void Compiler::ParsePrecedence(Precedence::Order precedence)
{
    Advance();
    const ParseRule& rule = GetRule(Previous().Type);
    bool canAssign = precedence <= Precedence::Assignment;
    if (rule.Prefix == nullptr)
    {
        Error("Expected expression.");
        return;
    }
    (this->*rule.Prefix)(canAssign);

    while (GetRule(m_Tokens[m_CurrentTokenNum].Type).InfixPrecedence >= precedence)
    {
        Advance();
        const ParseRule& infix = GetRule(Previous().Type);
        (this->*infix.Infix)(canAssign);
    }
    if (canAssign && Match(TokenType::Equal))
    {
        Error("Invalid assignment target.");
    }
}

u32 Compiler::ParseVariable(std::string_view message)
{
    Consume(TokenType::Identifier, message);
    Token indentifier = Previous();
    ObjHandle varname = m_VirtualMachine->AddString(std::string{indentifier.Lexeme});
    return AddOrGetGlobalIndex(varname);
}

void Compiler::DefineVariable(u32 variableName)
{
    EmitConstantCode((u64)variableName);
    EmitOperation(OpCode::OpDefineGlobal);
}

u32 Compiler::NamedVariable(const Token& name)
{
    ObjHandle varname = m_VirtualMachine->AddString(std::string{name.Lexeme});
    return AddOrGetGlobalIndex(varname);
}

u32 Compiler::AddOrGetGlobalIndex(ObjHandle variableName)
{
    auto compare = [variableName](const auto& v) {
        if constexpr(std::is_same_v<ObjHandle, decltype(v)>) return v == variableName;
        else return false;
    };
    auto it = std::find_if(m_CurrentChunk->GetValues().begin(), m_CurrentChunk->GetValues().end(), compare);
    if (it == m_CurrentChunk->GetValues().end()) return EmitConstantVal(variableName);
    return static_cast<u32>(it - m_CurrentChunk->GetValues().begin());
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

void Compiler::EmitByte(u8 byte)
{
    m_CurrentChunk->AddByte(byte, Previous().Line);
}

void Compiler::EmitOperation(OpCode opCode)
{
    m_CurrentChunk->AddOperation(opCode, Previous().Line);
}

u32 Compiler::EmitConstantCode(Value val)
{
    return m_CurrentChunk->AddConstantCode(val, Previous().Line);
}

u32 Compiler::EmitConstantVal(Value val)
{
    return m_CurrentChunk->AddConstantVal(val);
}

void Compiler::EmitReturn()
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
