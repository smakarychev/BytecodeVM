#include "Compiler.h"

#include "Core.h"
#include "Scanner.h"
#include "VirtualMachine.h"
#include "Obj.h"

#include <ranges>

CompilerContext::CompilerContext() = default;

CompilerContext::CompilerContext(::FunType funType)
    : FunType(funType), Fun(ObjRegistry::Create<FunObj>())
{
}

Compiler::Compiler(VirtualMachine* vm)
    : m_VirtualMachine(vm)
{
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
    rules[toInt(TokenType::LeftParen)]    = { &Compiler::Grouping,   &Compiler::Call,      Precedence::Order::Call };
    rules[toInt(TokenType::RightParen)]   = { nullptr,               nullptr,              Precedence::Order::None };
    rules[toInt(TokenType::LeftBrace)]    = { nullptr,               nullptr,              Precedence::Order::None };
    rules[toInt(TokenType::RightBrace)]   = { nullptr,               nullptr,              Precedence::Order::None };
    rules[toInt(TokenType::LeftSquare)]   = { &Compiler::Collection, &Compiler::Subscript, Precedence::Order::Call };
    rules[toInt(TokenType::RightSquare)]  = { nullptr,               nullptr,              Precedence::Order::None };
    rules[toInt(TokenType::Comma)]        = { nullptr,               nullptr,              Precedence::Order::None };
    rules[toInt(TokenType::Dot)]          = { nullptr,               &Compiler::Dot,       Precedence::Order::Call };
    rules[toInt(TokenType::Minus)]        = { &Compiler::Unary,      &Compiler::Binary,    Precedence::Order::Term };
    rules[toInt(TokenType::Plus)]         = { nullptr,               &Compiler::Binary,    Precedence::Order::Term };
    rules[toInt(TokenType::Semicolon)]    = { nullptr,               nullptr,              Precedence::Order::None };
    rules[toInt(TokenType::Slash)]        = { nullptr,               &Compiler::Binary,    Precedence::Order::Factor };
    rules[toInt(TokenType::Star)]         = { nullptr,               &Compiler::Binary,    Precedence::Order::Factor };
    rules[toInt(TokenType::Pipe)]         = { nullptr,               &Compiler::Binary,    Precedence::Order::Factor };
    rules[toInt(TokenType::Bang)]         = { &Compiler::Unary,      nullptr,              Precedence::Order::None };
    rules[toInt(TokenType::BangEqual)]    = { nullptr,               &Compiler::Binary,    Precedence::Order::Equals };
    rules[toInt(TokenType::Equal)]        = { nullptr,               nullptr,              Precedence::Order::None };
    rules[toInt(TokenType::EqualEqual)]   = { nullptr,               &Compiler::Binary,    Precedence::Order::Equals };
    rules[toInt(TokenType::Greater)]      = { nullptr,               &Compiler::Binary,    Precedence::Order::Comparison };
    rules[toInt(TokenType::GreaterEqual)] = { nullptr,               &Compiler::Binary,    Precedence::Order::Comparison };
    rules[toInt(TokenType::Less)]         = { nullptr,               &Compiler::Binary,    Precedence::Order::Comparison };
    rules[toInt(TokenType::LessEqual)]    = { nullptr,               &Compiler::Binary,    Precedence::Order::Comparison };
    rules[toInt(TokenType::Identifier)]   = { &Compiler::Variable,   nullptr,              Precedence::Order::None };
    rules[toInt(TokenType::String)]       = { &Compiler::String,     nullptr,              Precedence::Order::None };
    rules[toInt(TokenType::Number)]       = { &Compiler::Number,     nullptr,              Precedence::Order::None };
    rules[toInt(TokenType::And)]          = { nullptr,               &Compiler::And,       Precedence::Order::And };
    rules[toInt(TokenType::Class)]        = { nullptr,               nullptr,              Precedence::Order::None };
    rules[toInt(TokenType::Else)]         = { nullptr,               nullptr,              Precedence::Order::None };
    rules[toInt(TokenType::False)]        = { &Compiler::False,      nullptr,              Precedence::Order::None };
    rules[toInt(TokenType::Fun)]          = { nullptr,               nullptr,              Precedence::Order::None };
    rules[toInt(TokenType::For)]          = { nullptr,               nullptr,              Precedence::Order::None };
    rules[toInt(TokenType::If)]           = { nullptr,               nullptr,              Precedence::Order::None };
    rules[toInt(TokenType::Nil)]          = { &Compiler::Nil,        nullptr,              Precedence::Order::None };
    rules[toInt(TokenType::Or)]           = { nullptr,               &Compiler::Or,        Precedence::Order::Or };
    rules[toInt(TokenType::Return)]       = { nullptr,               nullptr,              Precedence::Order::None };
    rules[toInt(TokenType::Super)]        = { &Compiler::Super,      nullptr,              Precedence::Order::None };
    rules[toInt(TokenType::This)]         = { &Compiler::This,       nullptr,              Precedence::Order::None };
    rules[toInt(TokenType::True)]         = { &Compiler::True,       nullptr,              Precedence::Order::None };
    rules[toInt(TokenType::Let)]          = { nullptr,               nullptr,              Precedence::Order::None };
    rules[toInt(TokenType::While)]        = { nullptr,               nullptr,              Precedence::Order::None };
    rules[toInt(TokenType::Eof)]          = { nullptr,               nullptr,              Precedence::Order::None };
    rules[toInt(TokenType::Error)]        = { nullptr,               nullptr,              Precedence::Order::None };
}

void Compiler::InitContext(FunType funType, std::string_view funName)
{
    m_CurrentContext = CompilerContext{funType};
    CurrentChunk().m_Name = funName;
    Token name = SyntheticToken("");
    if (funType == FunType::Method || funType == FunType::Initializer) name.Lexeme = "this";
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
        case TokenType::Return:
        case TokenType::Let:
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
    case TokenType::Let:
        Advance();
        VarDeclaration();
        break;
    case TokenType::Fun:
        Advance();
        FunDeclaration();
        break;
    case TokenType::Class:
        Advance();
        ClassDeclaration();
        break;
    default:
        Statement();
        break;
    }
    if (m_IsInPanic) Synchronize();
}

void Compiler::VarDeclaration()
{
    Consume(TokenType::Identifier, "Expected variable name");
    if (m_CurrentContext.ScopeDepth == 0) GlobalVarDeclaration();
    else LocalVarDeclaration();
}

void Compiler::GlobalVarDeclaration()
{
    u32 varIndex = GlobalIndexByIdentifier(Previous());
    VarRHSDefinition();
    EmitOperation(OpCode::OpDefineGlobal, varIndex);
}

void Compiler::LocalVarDeclaration()
{
    u32 varIndex = LocalIndexByIdentifier(Previous());
    VarRHSDefinition();
    // the newly added var is not "defined" this ensures, that it cannot be used as it's own initializer,
    // after we parsed rhs, we have to mark it as "defined"
    MarkDefined();
}

void Compiler::VarRHSDefinition()
{
    if (Match(TokenType::Equal)) Expression();
    else EmitOperation(OpCode::OpNil);
    Consume(TokenType::Semicolon, "Expected ';' after variable declaration.");
}

void Compiler::FunDeclaration()
{
    Consume(TokenType::Identifier, "Expected fun name");
    if (m_CurrentContext.ScopeDepth == 0) GlobalFunDeclaration();
    else LocalFunDeclaration();
}

void Compiler::GlobalFunDeclaration()
{
    u32 funIndex = GlobalIndexByIdentifier(Previous());
    FunRHSDefinition(FunType::Function);
    EmitOperation(OpCode::OpDefineGlobal, funIndex);
}

void Compiler::LocalFunDeclaration()
{
    u32 funIndex = LocalIndexByIdentifier(Previous());
    // rhs (fun body) may use it's name for recursion, so we need to mark it as defined
    MarkDefined();
    FunRHSDefinition(FunType::Function);
}

void Compiler::FunRHSDefinition(FunType type)
{
    CompilerContext current = m_CurrentContext;
    InitContext(type, Previous().Lexeme);
    m_CurrentContext.CurrentClass = current.CurrentClass;
    m_CurrentContext.Enclosing = &current;
    PushScope();
    Consume(TokenType::LeftParen, "Expected '(' after function name.");
    FunArgList();
    Consume(TokenType::RightParen, "Expected ')' after function parameters.");

    // parse body
    Consume(TokenType::LeftBrace, "Expected '{' before function body.");
    Block();

    // the newly created function is located in current context
    ObjHandle fun = m_CurrentContext.Fun;
    OnCompileSubFunctionEnd();
    std::vector<UpvalueVar> upvalues = m_CurrentContext.Upvalues;
    m_CurrentContext = current;

    EmitOperation(OpCode::OpConstant, EmitConstant(fun));
    if (fun.As<FunObj>().UpvalueCount == 0 && type == FunType::Function) return;

    // not an ordinary function but a closure
    EmitOperation(OpCode::OpClosure);
    for (auto& upvalue : upvalues)
    {
        EmitByte((u8)upvalue.IsLocal);
        EmitByte(upvalue.Index);
    }
}

void Compiler::ClassDeclaration()
{
    CurrentClass current = CurrentClass{.Enclosing = m_CurrentContext.CurrentClass };
    m_CurrentContext.CurrentClass = &current;
    Consume(TokenType::Identifier, "Expected class name");
    if (m_CurrentContext.ScopeDepth == 0) GlobalClassDeclaration();
    else LocalClassDeclaration();
    if (m_CurrentContext.CurrentClass->HasSuperClass) PopScope();
    m_CurrentContext.CurrentClass = current.Enclosing;
}

void Compiler::GlobalClassDeclaration()
{
    u32 classIndex = GlobalIndexByIdentifier(Previous());
    EmitOperation(OpCode::OpConstant, EmitString(std::string{Previous().Lexeme}));
    EmitOperation(OpCode::OpClass);
    EmitOperation(OpCode::OpDefineGlobal, classIndex);
    if (Match(TokenType::Less)) Inherit(classIndex, false);
    EmitOperation(OpCode::OpReadGlobal, classIndex);
    ClassRHSDefinition();
}

void Compiler::LocalClassDeclaration()
{
    u32 classIndex = LocalIndexByIdentifier(Previous());
    // rhs methods might use class name, so we need to mark it as defined
    MarkDefined();
    EmitOperation(OpCode::OpConstant, EmitString(std::string{Previous().Lexeme}));
    EmitOperation(OpCode::OpClass);
    if (Match(TokenType::Less)) Inherit(classIndex, true);
    EmitOperation(OpCode::OpReadLocal, classIndex);
    ClassRHSDefinition();
}

void Compiler::ClassRHSDefinition()
{
    Consume(TokenType::LeftBrace, "Expected '{' before class body.");
    while (!IsAtEnd() && !Check(TokenType::RightBrace))
    {
        Method();
    }
    Consume(TokenType::RightBrace, "Expected '}' after class body.");
    EmitOperation(OpCode::OpPop);
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

    Consume(TokenType::LeftParen, "Expected '(' after 'for'");
    u32 counterIndexStart = (u32)m_CurrentContext.LocalVars.size();
    u32 counterIndexEnd = (u32)m_CurrentContext.LocalVars.size();
    // parse initializer
    if (!Match(TokenType::Semicolon))
    {
        if (Match(TokenType::Let)) VarDeclaration();
        else ExpressionStatement();
        counterIndexEnd = (u32)m_CurrentContext.LocalVars.size();
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
    // parse body
    Statement();

    // close all captured upvalues of counters
    for (u32 i = counterIndexStart; i < counterIndexEnd; i++)
    {
        if (m_CurrentContext.LocalVars[i].IsCaptured)
        {
            EmitOperation(OpCode::OpCloseUpvalue);
            m_CurrentContext.LocalVars[i].IsCaptured = false;
        }
    }
    
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

void Compiler::ReturnStatement()
{
    if (m_CurrentContext.FunType == FunType::Script)
    {
        Error("Can return only from functions.");
    }
    if (m_CurrentContext.FunType == FunType::Initializer)
    {
        Error("Cannot return from an initializer.");
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

void Compiler::FunArgList()
{
    while (!Check(TokenType::RightParen))
    {
        m_CurrentContext.Fun.As<FunObj>().Arity++;
        if (m_CurrentContext.Fun.As<FunObj>().Arity > 255)
        {
            Error("Arguments count more than 255. Stop it, get some help.");
        }
        Consume(TokenType::Identifier, "Expected parameter name.");
        u32 varIndex = LocalIndexByIdentifier(Previous());
        // arguments cannot have initializers, so we mark them immediately
        MarkDefined();
        if (Peek().Type == TokenType::Comma) Advance();
    }
}

void Compiler::Method()
{
    // after we're done, we will have class at stack top - 2
    Consume(TokenType::Identifier, "Expected method name.");
    const Token& identifier = Previous();
    FunType type = FunType::Method;
    if (identifier.Lexeme == "init") type = FunType::Initializer;
    // emit body, will end up on stack top - 1
    FunRHSDefinition(type);
    // emit name, will end up on stack top
    EmitOperation(OpCode::OpConstant, EmitString(std::string{identifier.Lexeme}));
    EmitOperation(OpCode::OpMethod);
}

void Compiler::Inherit(u32 subclassIndex, bool isLocal)
{
    m_CurrentContext.CurrentClass->HasSuperClass = true;
    PushScope();
    AddLocal(SyntheticToken("super"));
    MarkDefined();
    
    Consume(TokenType::Identifier, "Expected superclass name.");
    Variable(false);
    if (isLocal)
    {
        EmitOperation(OpCode::OpReadLocal, subclassIndex);
    }
    else
    {
        EmitOperation(OpCode::OpReadGlobal, subclassIndex);
    }
    EmitOperation(OpCode::OpInherit);
}

u32 Compiler::CallArgList()
{
    u32 argc = 0;
    while (!Check(TokenType::RightParen))
    {
        argc++;
        Expression();
        if (Peek().Type == TokenType::Comma) Advance();
    }
    Consume(TokenType::RightParen, "Expected ')' after function parameters.");
    return argc;
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
    case TokenType::Minus:          EmitOperation(OpCode::OpSubtract);      break;
    case TokenType::Plus:           EmitOperation(OpCode::OpAdd);           break;
    case TokenType::Slash:          EmitOperation(OpCode::OpDivide);        break;
    case TokenType::Star:           EmitOperation(OpCode::OpMultiply);      break;
    case TokenType::Pipe:           EmitOperation(OpCode::OpColMultiply);   break;
    case TokenType::EqualEqual:     EmitOperation(OpCode::OpEqual);         break;
    case TokenType::BangEqual:      EmitOperation(OpCode::OpEqual);         EmitOperation(OpCode::OpNot); break;
    case TokenType::Less:           EmitOperation(OpCode::OpLess);          break;
    case TokenType::LessEqual:      EmitOperation(OpCode::OpLequal);        break;
    case TokenType::Greater:        EmitOperation(OpCode::OpLequal);        EmitOperation(OpCode::OpNot); break;
    case TokenType::GreaterEqual:   EmitOperation(OpCode::OpLess);          EmitOperation(OpCode::OpNot); break;
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
    val = std::strtod(Previous().Lexeme.data(), nullptr);
    u32 index = EmitConstant(val);
    EmitOperation(OpCode::OpConstant, index);
}

void Compiler::String(bool canAssign)
{
    Token stringToken = Previous();
    std::string internal = ProcessEscapeSeq(stringToken.Lexeme.substr(1, stringToken.Lexeme.length() - 2));
    ObjHandle string = m_VirtualMachine->AddString(internal);
    u32 index = EmitConstant(string);
    EmitOperation(OpCode::OpConstant, index);
}

void Compiler::Variable(bool canAssign)
{
    Variable(Previous(), canAssign);
}

void Compiler::Variable(const Token& identifier, bool canAssign)
{
    u32 varName = ResolveLocalVar(identifier);
    OpCode readOp;
    OpCode setOp;
    if (varName != LocalVar::INVALID_INDEX)
    {
        readOp = OpCode::OpReadLocal;
        setOp = OpCode::OpSetLocal;
    }
    else
    {
        varName = ResolveUpvalue(identifier);
        if (varName != UpvalueVar::INVALID_INDEX)
        {
            readOp = OpCode::OpReadUpvalue;
            setOp = OpCode::OpSetUpvalue;
        }
        else
        {
            varName = ResolveGlobal(identifier);
            readOp = OpCode::OpReadGlobal;
            setOp = OpCode::OpSetGlobal;
        }
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
    u32 argc = CallArgList();
    EmitOperation(OpCode::OpCall);
    EmitByte((u8)argc);
}

void Compiler::Dot(bool canAssign)
{
    Consume(TokenType::Identifier, "Expected identifier after '.'");
    const Token& identifier = Previous();
    if (Match(TokenType::Equal) && canAssign)
    {
        Expression();
        EmitOperation(OpCode::OpSetProperty, EmitString(std::string{identifier.Lexeme}));
    }
    else if(Match(TokenType::LeftParen))
    {
        u32 argc = CallArgList();
        EmitOperation(OpCode::OpConstant, EmitString(std::string{identifier.Lexeme}));
        EmitOperation(OpCode::OpInvoke);
        EmitByte((u8)argc);
    }
    else
    {
        EmitOperation(OpCode::OpReadProperty, EmitString(std::string{identifier.Lexeme}));
    }
}

void Compiler::This(bool canAssign)
{
    if (m_CurrentContext.CurrentClass == nullptr)
    {
        Error("Cannot use 'this' outside of a method.");
        return;
    }
    Variable(false);
}

void Compiler::Super(bool canAssign)
{
    if (m_CurrentContext.CurrentClass == nullptr)
    {
        Error("Cannot use 'super' outside of class.");
        return;
    }
    if (!m_CurrentContext.CurrentClass->HasSuperClass)
    {
        Error("Class does not inherit from other.");
        return;
    }
    Consume(TokenType::Dot, "Expected '.' after 'super'");
    Consume(TokenType::Identifier, "Expected method name after super");
    const Token& method = Previous();
    Variable(SyntheticToken("this"), false);
    if (Match(TokenType::LeftParen))
    {
        u32 argc = CallArgList();
        Variable(SyntheticToken("super"), false);
        EmitOperation(OpCode::OpConstant, EmitString(std::string{method.Lexeme}));
        EmitOperation(OpCode::OpInvokeSuper);
        EmitByte((u8)argc);
    }
    else
    {
        Variable(SyntheticToken("super"), false);
        EmitOperation(OpCode::OpConstant, EmitString(std::string{method.Lexeme}));
        EmitOperation(OpCode::OpReadSuper);    
    }
}

void Compiler::Collection(bool canAssign)
{
    u32 count = 0;
    while (!Check(TokenType::RightSquare))
    {
        Expression();
        if (Peek().Type == TokenType::Comma) Advance();
        count++;
    }
    Consume(TokenType::RightSquare, "Expected ']' after collection.");
    EmitOperation(OpCode::OpConstant, EmitConstant((f64)count));
    EmitOperation(OpCode::OpCollection);
}

void Compiler::Subscript(bool canAssign)
{
    Expression();
    Consume(TokenType::RightSquare, "Expected ']' after subscript.");
    if (Match(TokenType::Equal) && canAssign)
    {
        Expression();
        EmitOperation(OpCode::OpSetSubscript);
    }
    else
    {
        EmitOperation(OpCode::OpReadSubscript);
    }
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

u32 Compiler::ResolveLocalVar(const Token& name)
{
    auto it = std::find_if(m_CurrentContext.LocalVars.rbegin(), m_CurrentContext.LocalVars.rend(), [&name](const auto& local) { return local.Name.Lexeme == name.Lexeme; });
    if (it != m_CurrentContext.LocalVars.rend())
    {
        if (it->Depth == LocalVar::DEPTH_UNDEFINED) Error("Cannot read local variable in its own initializer.");
        return static_cast<u32>(std::distance(it, m_CurrentContext.LocalVars.rend()) - 1);
    }
    return LocalVar::INVALID_INDEX;
}

u8 Compiler::ResolveUpvalue(const Token& name)
{
    if (m_CurrentContext.Enclosing == nullptr) return UpvalueVar::INVALID_INDEX;
    CompilerContext current = m_CurrentContext;

    m_CurrentContext = *m_CurrentContext.Enclosing;
    u32 local = ResolveLocalVar(name);
    m_CurrentContext = current;
    if (local != LocalVar::INVALID_INDEX) return AddUpvalue((u8)local, true);

    m_CurrentContext = *m_CurrentContext.Enclosing;
    u8 upvalue = ResolveUpvalue(name);
    m_CurrentContext = current;
    if (upvalue != UpvalueVar::INVALID_INDEX) return AddUpvalue(upvalue, false);
    
    return UpvalueVar::INVALID_INDEX;
}

u32 Compiler::ResolveGlobal(const Token& name)
{
    ObjHandle varname = m_VirtualMachine->AddString(std::string{name.Lexeme});
    return AddOrGetGlobalIndex(varname);
}

u32 Compiler::LocalIndexByIdentifier(const Token& identifier)
{
    TryAddLocal(identifier);
    return (u32)m_CurrentContext.LocalVars.size() - 1;
}

u32 Compiler::GlobalIndexByIdentifier(const Token& identifier)
{
    ObjHandle name = m_VirtualMachine->AddString(std::string{identifier.Lexeme});
    return AddOrGetGlobalIndex(name);
}

u32 Compiler::AddOrGetGlobalIndex(ObjHandle variableName)
{
    auto compare = [variableName](const auto& v) {
        if (v.template HasType<ObjHandle>())
        {
            return v.template As<ObjHandle>() == variableName;
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
        if (m_CurrentContext.LocalVars.back().IsCaptured)
        {
            PopLocals(popCount);
            EmitOperation(OpCode::OpCloseUpvalue);
            EmitOperation(OpCode::OpPop);
            popCount = 0;
        }
        else
        {
            popCount++;
        }
        m_CurrentContext.LocalVars.pop_back();
    }
    PopLocals(popCount);
}

void Compiler::PopLocals(u32 count)
{
    if (count == 0) return;
    if (count == 1) EmitOperation(OpCode::OpPop);
    else
    {
        u32 index = EmitConstant((f64)count);
        EmitOperation(OpCode::OpConstant, index);
        EmitOperation(OpCode::OpPopN);
    }
}

void Compiler::TryAddLocal(const Token& name)
{
    // first, check that a variable with the same name has not already been defined (declared)
    for (auto& localVar : std::ranges::reverse_view(m_CurrentContext.LocalVars))
    {
        if (localVar.Depth != LocalVar::DEPTH_UNDEFINED && localVar.Depth < m_CurrentContext.ScopeDepth) break;
        if (localVar.Name.Lexeme == name.Lexeme) Error(std::format("Variable with a name {} is already declared", name.Lexeme));
    }
    AddLocal(name);
}

void Compiler::AddLocal(const Token& name)
{
    m_CurrentContext.LocalVars.push_back(LocalVar{.Name = name, .Depth = LocalVar::DEPTH_UNDEFINED});
}

u8 Compiler::AddUpvalue(u8 index, bool isLocal)
{
    // 255 as a limit is only because I'm feeling lazy
    if (m_CurrentContext.Fun.As<FunObj>().UpvalueCount == 255) Error("Cannot have more than 255 upvalues.");
    // mark local var of outers scope as captures, so it will be migrated to the heap later
    if (isLocal) m_CurrentContext.Enclosing->LocalVars[index].IsCaptured = true;
    
    UpvalueVar newUpvalue = UpvalueVar{.Index = index, .IsLocal = isLocal};
    auto it = std::ranges::find(m_CurrentContext.Upvalues, newUpvalue);
    if (it != m_CurrentContext.Upvalues.end()) return (u8)std::distance(m_CurrentContext.Upvalues.begin(), it);
    m_CurrentContext.Upvalues.push_back(newUpvalue);
    u8 upvalueIndex = m_CurrentContext.Fun.As<FunObj>().UpvalueCount;
    m_CurrentContext.Fun.As<FunObj>().UpvalueCount++;
    return upvalueIndex;
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
    m_LastEmittedOpcode = opCode;
    CurrentChunk().AddOperation(opCode, Previous().Line);
}

void Compiler::EmitOperation(OpCode opCode, u32 operandIndex)
{
    if (m_NoEmit) return;
    m_LastEmittedOpcode = opCode;
    CurrentChunk().AddOperation(opCode, operandIndex, Previous().Line);
}

u32 Compiler::EmitString(const std::string& val)
{
    if (m_NoEmit) return std::numeric_limits<u32>::max();
    ObjHandle string = m_VirtualMachine->AddString(val);
    return EmitConstant(string);
}

u32 Compiler::EmitConstant(Value val)
{
    if (m_NoEmit) return std::numeric_limits<u32>::max();
    return CurrentChunk().AddConstant(val);
}

void Compiler::EmitReturn()
{
    if (m_NoEmit) return;
    if (m_LastEmittedOpcode == OpCode::OpReturn) return;
    if (m_CurrentContext.FunType == FunType::Initializer)
    {
        EmitOperation(OpCode::OpReadLocal, 0);    
    }
    else
    {
        CurrentChunk().AddOperation(OpCode::OpNil, Peek().Line);
    }
    CurrentChunk().AddOperation(OpCode::OpReturn, Peek().Line);
    m_LastEmittedOpcode = OpCode::OpReturn;
}

u32 Compiler::EmitJump(OpCode jumpCode)
{
    if (m_NoEmit) return std::numeric_limits<u32>::max();
    m_LastEmittedOpcode = jumpCode;
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

std::string Compiler::ProcessEscapeSeq(std::string_view lexeme) const
{
    std::string result;
    for (usize i = 0; i < lexeme.size();)
    {
        if (lexeme[i] == '\\')
        {
            if (i < lexeme.size() - 1)
            {
                switch (lexeme[i + 1])
                {
                case 'a':   result.push_back('\a'); i+=2; break;
                case 'b':   result.push_back('\b'); i+=2; break;
                case 'f':   result.push_back('\f'); i+=2; break;
                case 'n':   result.push_back('\n'); i+=2; break;
                case 'r':   result.push_back('\r'); i+=2; break;
                case 't':   result.push_back('\t'); i+=2; break;
                case 'v':   result.push_back('\v'); i+=2; break;
                case '\'':  result.push_back('\''); i+=2; break;
                case '"':   result.push_back('\"'); i+=2; break;
                case '\\':  result.push_back('\\'); i+=2; break;
                case '?':   result.push_back('\?'); i+=2; break;
                default:    result.push_back('\\'); i+=1; break;
                }
            }
        }
        else
        {
            result.push_back(lexeme[i]);
            i+=1;
        }
    }
    return result;
}

Token Compiler::SyntheticToken(std::string_view lexeme) const
{
    Token token{.Type = TokenType::Identifier, .Lexeme = lexeme, .Line = 0};
    return token;
}
