#include "Compiler.h"

#include "Core.h"
#include "Scanner.h"
#include "VirtualMachine.h"
#include "Obj.h"

#include <ranges>

CompilerContext::CompilerContext() = default;

CompilerContext::CompilerContext(::FunType funType)
    : FunType(funType), Fun(ObjRegistry::CreateObj<FunObj>())
{
}

Compiler::Compiler(VirtualMachine* vm)
    : m_VirtualMachine(vm)
{
    Init();
}

void Compiler::Init()
{
    InitParseTable();
    InitContext(FunType::Script, "<script>");
}

void Compiler::InitParseTable()
{
    auto toInt = [](TokenType type) { return static_cast<u32>(type); };
    auto& rules = m_ParseRules;
    rules[toInt(TokenType::LeftParen)]    = { &Compiler::Grouping,   &Compiler::Call,    Precedence::Order::Call };
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
    rules[toInt(TokenType::And)]          = { nullptr,               &Compiler::And,     Precedence::Order::And };
    rules[toInt(TokenType::Class)]        = { nullptr,               nullptr,            Precedence::Order::None };
    rules[toInt(TokenType::Else)]         = { nullptr,               nullptr,            Precedence::Order::None };
    rules[toInt(TokenType::False)]        = { &Compiler::False,      nullptr,            Precedence::Order::None };
    rules[toInt(TokenType::Fun)]          = { nullptr,               nullptr,            Precedence::Order::None };
    rules[toInt(TokenType::For)]          = { nullptr,               nullptr,            Precedence::Order::None };
    rules[toInt(TokenType::If)]           = { nullptr,               nullptr,            Precedence::Order::None };
    rules[toInt(TokenType::Nil)]          = { &Compiler::Nil,        nullptr,            Precedence::Order::None };
    rules[toInt(TokenType::Or)]           = { nullptr,               &Compiler::Or,      Precedence::Order::Or };
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

void Compiler::InitContext(FunType funType, std::string_view funName)
{
    m_CurrentContext = CompilerContext{FunType::Script};
    CurrentChunk().m_Name = funName;
    Token name{.Type = TokenType::Identifier, .Lexeme = "", .Line = 0};
    m_CurrentContext.LocalVars.push_back(LocalVar{.Name = name, .Depth = 0});
}

Chunk& Compiler::CurrentChunk()
{
    return m_CurrentContext.Fun.Get<FunObj>()->Chunk;
}

const Chunk& Compiler::CurrentChunk() const
{
    return m_CurrentContext.Fun.Get<FunObj>()->Chunk;
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
            return CompilerResult{false, ObjHandle::NonHandle()};
        }
    }

    while(!Match(TokenType::Eof))
    {
        Declaration();
    }

    OnCompileEnd();
    
    return CompilerResult{!m_HadError, m_CurrentContext.Fun};
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
    const Token& token = Peek();
    if (!IsAtEnd()) m_CurrentTokenNum++;
    return token;
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
    case TokenType::Var:
        Advance();
        VarDeclaration();
        break;
    case TokenType::Fun:
        Advance();
        FunDeclaration();
        break;
    default:
        Statement();
        break;
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

void Compiler::FunDeclaration()
{
    u32 funName = ParseVariable("Expected fun name");
    // immediately mark as defined, so recursion is possible
    MarkDefined();
    // parse function params and body
    Function(FunType::Function);
    DefineVariable(funName);
}

void Compiler::Statement()
{
    switch (Peek().Type)
    {
    case TokenType::LeftBrace:
        Advance();
        PushScope();
        Block();
        PopScope();
        break;
    case TokenType::If:
        Advance();
        IfStatement();
        break;
    case TokenType::For:
        Advance();
        ForStatement();
        break;
    case TokenType::While:
        Advance();
        WhileStatement();
        break;
    case TokenType::Print:
        Advance();
        PrintStatement();
        break;
    case TokenType::Return:
        Advance();
        ReturnStatement();
        break;
    default:
        ExpressionStatement();
        break;
    }
}

void Compiler::Block()
{
    while (!IsAtEnd() && !Check(TokenType::RightBrace))
    {
        Declaration();
    }
    Consume(TokenType::RightBrace, "Expected '}' at the end of block.");
}

void Compiler::IfStatement()
{
    Consume(TokenType::LeftParen, "Expected condition after 'if'");
    Expression();
    Consume(TokenType::RightParen, "Expected ')' after condition");

    u32 jumpIfOut = EmitJump(OpCode::OpJumpFalse);
    EmitOperation(OpCode::OpPop);
    Statement(); // if branch
    u32 jumpElseOut = EmitJump(OpCode::OpJump);
    PatchJump(CurrentChunk().CodeLength(), jumpIfOut);
    EmitOperation(OpCode::OpPop);
    if (Match(TokenType::Else)) Statement(); // else branch
    PatchJump(CurrentChunk().CodeLength(), jumpElseOut);
}

void Compiler::WhileStatement()
{
    u32 loopStart = CurrentChunk().CodeLength();
    Consume(TokenType::LeftParen, "Expected condition after 'while'");
    Expression();
    Consume(TokenType::RightParen, "Expected ')' after condition");
    u32 jumpCondOut = EmitJump(OpCode::OpJumpFalse);
    EmitOperation(OpCode::OpPop);
    Statement();
    u32 jumpBodyEnd = EmitJump(OpCode::OpJump);
    PatchJump(loopStart, jumpBodyEnd);
    PatchJump(CurrentChunk().CodeLength(), jumpCondOut);
    EmitOperation(OpCode::OpPop);
}

void Compiler::ForStatement()
{
    PushScope();

    Consume(TokenType::LeftParen, "Expected condition after 'for'");
    // parse initializer
    if (!Match(TokenType::Semicolon))
    {
        if (Match(TokenType::Var)) VarDeclaration();
        else ExpressionStatement();
    }
    // parse condition
    u32 loopStart = CurrentChunk().CodeLength();
    u32 jumpCondOut = std::numeric_limits<u32>::max();
    if (!Match(TokenType::Semicolon))
    {
        Expression();
        Consume(TokenType::Semicolon, "Expected ';' after condition.");
        jumpCondOut = EmitJump(OpCode::OpJumpFalse);
        EmitOperation(OpCode::OpPop);
    }
    // parse increment, but delay emitting
    u32 incrementTokenStart = m_CurrentTokenNum;
    if (Peek().Type != TokenType::RightParen)
    {
        m_NoEmit = true;
        Expression();
        m_NoEmit = false;
    }
    u32 incrementTokenEnd = m_CurrentTokenNum;
    Consume(TokenType::RightParen, "Expected ')' after 'for(...;...;'");

    Statement();
    // push increment at the end of body stmt
    u32 bodyTokenEnd = m_CurrentTokenNum;
    if (incrementTokenStart != incrementTokenEnd)
    {
        m_CurrentTokenNum = incrementTokenStart;
        Expression();
        m_CurrentTokenNum = bodyTokenEnd;
        EmitOperation(OpCode::OpPop);
    }
    u32 jumpBodyEnd = EmitJump(OpCode::OpJump);
    PatchJump(loopStart, jumpBodyEnd);

    if (jumpCondOut != std::numeric_limits<u32>::max())
    {
        PatchJump(CurrentChunk().CodeLength(), jumpCondOut);
        EmitOperation(OpCode::OpPop);
    }
    
    PopScope();
}

void Compiler::PrintStatement()
{
    Expression();
    Consume(TokenType::Semicolon, "Expected ';' after expression.");
    EmitOperation(OpCode::OpPrint);
}

void Compiler::ReturnStatement()
{
    if (m_CurrentContext.FunType == FunType::Script)
    {
        Error("Can return only from functions.");
    }
    if (!Match(TokenType::Semicolon))
    {
        Expression();
        Consume(TokenType::Semicolon, "Expected ';' after expression");
        EmitOperation(OpCode::OpReturn);
    }
    else
    {
        EmitReturn();
    }
}

void Compiler::ExpressionStatement()
{
    Expression();
    Consume(TokenType::Semicolon, "Expected ';' after expression.");
    EmitOperation(OpCode::OpPop);
}

void Compiler::Function(FunType type)
{
    CompilerContext current = m_CurrentContext;
    InitContext(type, Previous().Lexeme);
    PushScope();
    Consume(TokenType::LeftParen, "Expected '(' after function name.");
    while (!Check(TokenType::RightParen))
    {
        m_CurrentContext.Fun.As<FunObj>().Arity++;
        if (m_CurrentContext.Fun.As<FunObj>().Arity > 255)
        {
            Error("Arguments count more than 255. Stop it, get some help.");
        }
        u32 varName = ParseVariable("Expected parameter name.");
        DefineVariable(varName);
        if (Peek().Type == TokenType::Comma) Advance();
    }
    Consume(TokenType::RightParen, "Expected ')' after function parameters.");

    Consume(TokenType::LeftBrace, "Expected '{' before function body.");
    Block();
    ObjHandle fun = m_CurrentContext.Fun;
    OnCompileSubFunctionEnd();

    m_CurrentContext = current;
    u32 index = EmitConstant(fun);
    EmitOperation(OpCode::OpConstant, index);
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
    u32 index = EmitConstant(val);
    EmitOperation(OpCode::OpConstant, index);
}

void Compiler::String(bool canAssign)
{
    Token stringToken = Previous();
    ObjHandle string = m_VirtualMachine->AddString(
        std::string{stringToken.Lexeme.substr(1, stringToken.Lexeme.length() - 2)});
    u32 index = EmitConstant(string);
    EmitOperation(OpCode::OpConstant, index);
}

void Compiler::Variable(bool canAssign)
{
    u32 varName = NamedLocalVar(Previous());
    OpCode readOp;
    OpCode setOp;
    if (varName != LocalVar::INVALID_INDEX)
    {
        readOp = OpCode::OpReadLocal;
        setOp = OpCode::OpSetLocal;
    }
    else
    {
        varName = NamedGlobalVar(Previous());
        readOp = OpCode::OpReadGlobal;
        setOp = OpCode::OpSetGlobal;
    }
    if (canAssign && Match(TokenType::Equal))
    {
        Expression();
        EmitOperation(setOp, varName);
    }
    else
    {
        EmitOperation(readOp, varName);
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

void Compiler::And(bool canAssign)
{
    u32 jump = EmitJump(OpCode::OpJumpFalse);
    EmitOperation(OpCode::OpPop); // pop only if it's true, if it is false, it will be popped by condition expr
    ParsePrecedence(Precedence::And);
    PatchJump(CurrentChunk().CodeLength(), jump);
}

void Compiler::Or(bool canAssign)
{
    u32 jump = EmitJump(OpCode::OpJumpTrue);
    EmitOperation(OpCode::OpPop); // pop only if it's false, if it is true, it will be popped by condition expr
    ParsePrecedence(Precedence::Or);
    PatchJump(CurrentChunk().CodeLength(), jump);
}

void Compiler::Call(bool canAssign)
{
    u32 argCount = 0;
    while (!Check(TokenType::RightParen))
    {
        argCount++;
        Expression();
        if (Peek().Type == TokenType::Comma) Advance();
    }
    Consume(TokenType::RightParen, "Expected ')' after function parameters.");
    EmitOperation(OpCode::OpCall);
    EmitByte((u8)argCount);
}

void Compiler::ParsePrecedence(Precedence::Order precedence)
{
    const ParseRule& rule = GetRule(Advance().Type);
    bool canAssign = precedence <= Precedence::Assignment;
    if (rule.Prefix == nullptr)
    {
        Error("Expected expression.");
        return;
    }
    (this->*rule.Prefix)(canAssign);

    while (GetRule(m_Tokens[m_CurrentTokenNum].Type).InfixPrecedence >= precedence)
    {
        const ParseRule& infix = GetRule(Advance().Type);
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

    DeclareVariable();
    if (m_CurrentContext.ScopeDepth > 0) return 0;
    
    return AddOrGetGlobalIndex(varname);
}

void Compiler::DefineVariable(u32 variableName)
{
    if (m_CurrentContext.ScopeDepth > 0)
    {
        MarkDefined();
        return;
    }
    EmitOperation(OpCode::OpDefineGlobal, variableName);
}

u32 Compiler::NamedLocalVar(const Token& name)
{
    auto it = std::find_if(m_CurrentContext.LocalVars.rbegin(), m_CurrentContext.LocalVars.rend(), [&name](const auto& local) { return local.Name.Lexeme == name.Lexeme; });
    if (it != m_CurrentContext.LocalVars.rend())
    {
        if (it->Depth == LocalVar::DEPTH_UNDEFINED) Error("Cannot read local variable in its own initializer.");
        return static_cast<u32>(std::distance(it, m_CurrentContext.LocalVars.rend()) - 1);
    }
    return LocalVar::INVALID_INDEX;
}

u32 Compiler::NamedGlobalVar(const Token& name)
{
    ObjHandle varname = m_VirtualMachine->AddString(std::string{name.Lexeme});
    return AddOrGetGlobalIndex(varname);
}

u32 Compiler::AddOrGetGlobalIndex(ObjHandle variableName)
{
    auto compare = [variableName](const auto& v) {
        if (std::holds_alternative<ObjHandle>(v))
        {
            return std::get<ObjHandle>(v) == variableName;
        }
        return false;
    };
    auto it = std::find_if(CurrentChunk().GetValues().begin(), CurrentChunk().GetValues().end(), compare);
    if (it == CurrentChunk().GetValues().end()) return EmitConstant(variableName);
    return static_cast<u32>(it - CurrentChunk().GetValues().begin());
}

void Compiler::MarkDefined()
{
    if (m_CurrentContext.ScopeDepth == 0) return;
    m_CurrentContext.LocalVars.back().Depth = m_CurrentContext.ScopeDepth;
}

void Compiler::PushScope()
{
    m_CurrentContext.ScopeDepth++;
}

void Compiler::PopScope()
{
    m_CurrentContext.ScopeDepth--;
    u32 popCount = 0;
    while (!m_CurrentContext.LocalVars.empty() && m_CurrentContext.LocalVars.back().Depth > m_CurrentContext.ScopeDepth)
    {
        popCount++;
        m_CurrentContext.LocalVars.pop_back();
    }
    if (popCount == 0) return;
    if (popCount == 1) EmitOperation(OpCode::OpPop);
    else
    {
        u32 index = EmitConstant((u64)popCount);
        EmitOperation(OpCode::OpConstant, index);
        EmitOperation(OpCode::OpPopN);
    }
}

void Compiler::DeclareVariable()
{
    if (m_CurrentContext.ScopeDepth == 0) return;
    const Token& var = Previous();
    for (auto& localVar : std::ranges::reverse_view(m_CurrentContext.LocalVars))
    {
        if (localVar.Depth != LocalVar::DEPTH_UNDEFINED && localVar.Depth < m_CurrentContext.ScopeDepth) break;
        if (localVar.Name.Lexeme == var.Lexeme) Error(std::format("Variable with a name {} is already declared", var.Lexeme));
    }
    AddLocal(var);
}

void Compiler::AddLocal(const Token& name)
{
    m_CurrentContext.LocalVars.push_back(LocalVar{.Name = name, .Depth = LocalVar::DEPTH_UNDEFINED});
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
    if (m_NoEmit) return;
    CurrentChunk().AddByte(byte, Previous().Line);
}

void Compiler::EmitOperation(OpCode opCode)
{
    if (m_NoEmit) return;
    CurrentChunk().AddOperation(opCode, Previous().Line);
}

void Compiler::EmitOperation(OpCode opCode, u32 operandIndex)
{
    if (m_NoEmit) return;
    CurrentChunk().AddOperation(opCode, operandIndex, Previous().Line);
}

u32 Compiler::EmitConstant(Value val)
{
    if (m_NoEmit) return std::numeric_limits<u32>::max();
    return CurrentChunk().AddConstant(val);
}

void Compiler::EmitReturn()
{
    if (m_NoEmit) return;
    CurrentChunk().AddOperation(OpCode::OpNil, Previous().Line);
    CurrentChunk().AddOperation(OpCode::OpReturn, Previous().Line);
}

u32 Compiler::EmitJump(OpCode jumpCode)
{
    if (m_NoEmit) return std::numeric_limits<u32>::max();
    CurrentChunk().AddOperation(jumpCode, Previous().Line);
    CurrentChunk().AddInt(0, Previous().Line);
    return (u32)CurrentChunk().m_Code.size() - 4;
}

void Compiler::PatchJump(u32 jumpTo, u32 jumpFrom)
{
    i32 codeLen = (i32)(jumpTo - jumpFrom - 4);
    *reinterpret_cast<i32*>(&CurrentChunk().m_Code[jumpFrom]) = codeLen;
}

void Compiler::OnCompileEnd()
{
    EmitReturn();
#ifdef DEBUG_TRACE
    if (!m_HadError) Disassembler::Disassemble(CurrentChunk());
#endif
}

void Compiler::OnCompileSubFunctionEnd()
{
    OnCompileEnd();
}

const ParseRule& Compiler::GetRule(TokenType tokenType) const
{
    return m_ParseRules[static_cast<u32>(tokenType)];
}
