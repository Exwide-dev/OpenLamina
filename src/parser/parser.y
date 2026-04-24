%defines "parser.tab.hpp"
%define parse.error detailed
%output  "parser.tab.cpp"

// 必须在 %union 之前包含所需的类型声明
%code requires {
    #include <vector>
    #include <string>
    #include "parser/ast.hpp"
}

%code {
    #include <iostream>
    #include <cstring>
    #include "../tools/debug.hpp"
    using namespace lmx;
    bool has_err = false;
    std::string detail_msg;
}

%code {
    extern lmx::ASTNode* result;
}

%code {
    extern int yylineno;
    extern char* yytext;
    std::string error_msg() {
         std::string err_msg;
         err_msg += std::string("Error at line ") + std::to_string(yylineno)
                  + ", near '" + (yytext ? yytext : "") + "'\n" + detail_msg;
         return err_msg;
    }
}

%union {
    int int_val;
    double double_val;
    std::string* string_val;
    lmx::ASTNode* ast_node;
    lmx::ExprNode* expr_node;
    std::vector<lmx::ASTNode*>* ast_list;
}

%code {
    int yylex(void);
    void yyerror(const char* s);
}

%token <string_val> OPER_PLUS
%token <string_val> OPER_MINUS
%token <string_val> OPER_MUL
%token <string_val> OPER_DIV
%token <string_val> OPER_NOT  // 逻辑非运算符
%token <string_val> OPER_EQ   // ==
%token <string_val> OPER_NE   // !=
%token <string_val> OPER_LT   // <
%token <string_val> OPER_GT   // >
%token <string_val> OPER_LE   // <=
%token <string_val> OPER_GE   // >=
%token <string_val> OPER_COMMA  // ,
%token <string_val> ASSIGN
%token <string_val> LPAREN
%token <string_val> RPAREN
%token <string_val> LBRACE
%token <string_val> RBRACE
%token <string_val> NUM_LITERAL
%token <string_val> IDENTIFIER
%token <string_val> KW_LET
%token <string_val> KW_FUNC
%token <string_val> KW_DO
%token <string_val> KW_RETURN
%token <string_val> KW_IF
%token <string_val> KW_ELSE
%token <string_val> KW_LOOP
%token <string_val> KW_BREAK
%token <string_val> KW_CONTINUE

%type <ast_node> program
%type <ast_node> stmt
%type <ast_node> var_decl
%type <ast_node> assign_stmt  // 新增：赋值语句
%type <ast_node> func_decl  // 新增：函数声明
%type <ast_node> return_stmt  // 新增：return 语句
%type <ast_node> if_stmt
%type <ast_node> loop_stmt
%type <ast_node> break_stmt
%type <ast_node> continue_stmt
%type <ast_node> block_stmt
%type <expr_node> expr
%type <expr_node> comparison_expr  // 新增：比较表达式
%type <expr_node> additive_expr
%type <expr_node> multiplicative_expr
%type <expr_node> unary_expr  // 新增：一元表达式层
%type <expr_node> postfix_expr  // 新增：后缀表达式（用于函数调用）
%type <expr_node> factor
%type <expr_node> number
%type <expr_node> identifier
%type <ast_list> stmt_list
%type <ast_list> block_stmt_list
%type <ast_list> param_list  // 新增：参数列表
%type <ast_list> arg_list  // 新增：实参列表

%left OPER_EQ OPER_NE  // 相等性比较优先级最低
%left OPER_LT OPER_GT OPER_LE OPER_GE  // 比较运算符优先级
%left OPER_PLUS OPER_MINUS
%left OPER_MUL OPER_DIV
%right OPER_NOT  // 一元运算符：右结合，高优先级
%precedence UMINUS  // 用于一元负号的虚拟优先级
%precedence CALL  // 新增：函数调用优先级（最高）

%%

program:
    stmt_list {
        LOG("Parsing program");
        result = new ProgramASTNode(*$1);
        $$ = result;
        LOG("Program parsed successfully");
        delete $1;
    }
    ;

stmt_list:
    /* empty */ {
        LOG("Creating empty stmt_list");
        $$ = new std::vector<ASTNode*>();
    }
    | stmt_list stmt {
        LOG("Adding stmt to stmt_list");
        if ($2 != nullptr) {
            $1->push_back($2);
        }
        $$ = $1;
    }
    ;

stmt:
    var_decl { 
        LOG("Parsing stmt: var_decl");
        $$ = $1; 
    }
    | expr { 
        LOG("Parsing stmt: expr");
        $$ = $1; 
    }
    | assign_stmt {  // 新增：赋值语句
        LOG("Parsing stmt: assign_stmt");
        $$ = $1;
    }
    | func_decl {  // 新增：函数声明
        LOG("Parsing stmt: func_decl");
        $$ = $1;
    }
    | return_stmt {  // 新增：return 语句
        LOG("Parsing stmt: return_stmt");
        $$ = $1;
    }
    | if_stmt {
        LOG("Parsing stmt: if_stmt");
        $$ = $1;
    }
    | loop_stmt {
        LOG("Parsing stmt: loop_stmt");
        $$ = $1;
    }
    | break_stmt {
        LOG("Parsing stmt: break_stmt");
        $$ = $1;
    }
    | continue_stmt {
        LOG("Parsing stmt: continue_stmt");
        $$ = $1;
    }
    | block_stmt {
        LOG("Parsing stmt: block_stmt");
        $$ = $1;
    }
    ;

var_decl:
    KW_LET IDENTIFIER ASSIGN expr {
        LOG("Parsing var_decl: let " + *$2 + " = ...");
        $$ = new VarDeclNode(*$2, $4, false);
        LOG("var_decl parsed successfully");
        delete $2;
    }
    ;

// 新增：赋值语句规则
assign_stmt:
    IDENTIFIER ASSIGN expr {
        LOG("Parsing assign_stmt: " + *$1 + " = ...");
        $$ = new AssignNode(*$1, $3);
        LOG("assign_stmt parsed successfully");
        delete $1;
    }
    ;

// 新增：参数列表规则
param_list:
    /* empty */ {
        LOG("Creating empty param_list");
        $$ = new std::vector<ASTNode*>();
    }
    | IDENTIFIER {
        LOG("Creating param_list with one param");
        $$ = new std::vector<ASTNode*>();
        $$->push_back(new VarRefNode(*$1));
        delete $1;
    }
    | param_list OPER_COMMA IDENTIFIER {
        LOG("Adding param to param_list");
        $1->push_back(new VarRefNode(*$3));
        $$ = $1;
        delete $3;
    }
    ;

// 新增：实参列表规则
arg_list:
    /* empty */ {
        LOG("Creating empty arg_list");
        $$ = new std::vector<ASTNode*>();
    }
    | expr {
        LOG("Creating arg_list with one arg");
        $$ = new std::vector<ASTNode*>();
        $$->push_back($1);
    }
    | arg_list OPER_COMMA expr {
        LOG("Adding arg to arg_list");
        $1->push_back($3);
        $$ = $1;
    }
    ;

// 新增：函数声明规则
func_decl:
    KW_FUNC IDENTIFIER LPAREN param_list RPAREN block_stmt {
        LOG("Parsing func_decl: " + *$2);
        std::vector<std::string> params;
        for (auto& node : *$4) {
            auto var_ref = dynamic_cast<VarRefNode*>(node);
            if (var_ref) {
                params.push_back(var_ref->name);
            }
        }
        $$ = new FuncDeclNode(*$2, params, dynamic_cast<BlockStmtNode*>($6));
        LOG("func_decl parsed successfully");
        delete $2;
        delete $4;
    }
    ;

// 新增：return 语句规则
return_stmt:
    KW_RETURN expr {
        LOG("Parsing return_stmt");
        $$ = new ReturnStmtNode($2);
        LOG("return_stmt parsed successfully");
    }
    | KW_RETURN {
        LOG("Parsing return_stmt (empty)");
        $$ = new ReturnStmtNode(nullptr);
        LOG("return_stmt (empty) parsed successfully");
    }
    ;

// 块语句规则
block_stmt_list:
    /* empty */ {
        LOG("Creating empty block_stmt_list");
        $$ = new std::vector<ASTNode*>();
    }
    | block_stmt_list stmt {
        LOG("Adding stmt to block_stmt_list");
        if ($2 != nullptr) {
            $1->push_back($2);
        }
        $$ = $1;
    }
    ;

block_stmt:
    LBRACE block_stmt_list RBRACE {
        LOG("Parsing block_stmt");
        $$ = new BlockStmtNode(*$2);
        LOG("block_stmt parsed successfully");
        delete $2;
    }
    ;

// if-else 语句规则
if_stmt:
    KW_IF LPAREN expr RPAREN block_stmt {
        LOG("Parsing if_stmt (no else)");
        $$ = new IfStmtNode($3, dynamic_cast<BlockStmtNode*>($5), nullptr);
        LOG("if_stmt (no else) parsed successfully");
    }
    | KW_IF LPAREN expr RPAREN block_stmt KW_ELSE block_stmt {
        LOG("Parsing if_stmt (with else)");
        $$ = new IfStmtNode($3, dynamic_cast<BlockStmtNode*>($5), dynamic_cast<BlockStmtNode*>($7));
        LOG("if_stmt (with else) parsed successfully");
    }
    ;

// loop 语句规则
loop_stmt:
    KW_LOOP block_stmt {
        LOG("Parsing loop_stmt");
        $$ = new LoopNode(nullptr, dynamic_cast<BlockStmtNode*>($2));
        LOG("loop_stmt parsed successfully");
    }
    ;

// break 语句规则
break_stmt:
    KW_BREAK {
        LOG("Parsing break_stmt");
        $$ = new BreakNode();
        LOG("break_stmt parsed successfully");
    }
    ;

// continue 语句规则
continue_stmt:
    KW_CONTINUE {
        LOG("Parsing continue_stmt");
        $$ = new ContinueNode();
        LOG("continue_stmt parsed successfully");
    }
    ;

expr:
    comparison_expr { 
        LOG("Parsing expr");
        $$ = $1; 
    }
    ;

// 新增：比较表达式规则
comparison_expr:
    additive_expr {
        LOG("Parsing comparison_expr: base case");
        $$ = $1;
    }
    | comparison_expr OPER_EQ additive_expr {
        LOG("Parsing comparison_expr: ==");
        $$ = new BinaryNode($1, $3, "==");
        LOG("comparison_expr (==) parsed successfully");
    }
    | comparison_expr OPER_NE additive_expr {
        LOG("Parsing comparison_expr: !=");
        $$ = new BinaryNode($1, $3, "!=");
        LOG("comparison_expr (!=) parsed successfully");
    }
    | comparison_expr OPER_LT additive_expr {
        LOG("Parsing comparison_expr: <");
        $$ = new BinaryNode($1, $3, "<");
        LOG("comparison_expr (<) parsed successfully");
    }
    | comparison_expr OPER_GT additive_expr {
        LOG("Parsing comparison_expr: >");
        $$ = new BinaryNode($1, $3, ">");
        LOG("comparison_expr (>) parsed successfully");
    }
    | comparison_expr OPER_LE additive_expr {
        LOG("Parsing comparison_expr: <=");
        $$ = new BinaryNode($1, $3, "<=");
        LOG("comparison_expr (<=) parsed successfully");
    }
    | comparison_expr OPER_GE additive_expr {
        LOG("Parsing comparison_expr: >=");
        $$ = new BinaryNode($1, $3, ">=");
        LOG("comparison_expr (>=) parsed successfully");
    }
    ;

additive_expr:
    multiplicative_expr {
        LOG("Parsing additive_expr: base case");
        $$ = $1;
    }
    | additive_expr OPER_PLUS multiplicative_expr {
        LOG("Parsing additive_expr: +");
        $$ = new BinaryNode($1, $3, "+");
        LOG("additive_expr (+) parsed successfully");
    }
    | additive_expr OPER_MINUS multiplicative_expr {
        LOG("Parsing additive_expr: -");
        $$ = new BinaryNode($1, $3, "-");
        LOG("additive_expr (-) parsed successfully");
    }
    ;

multiplicative_expr:
    postfix_expr {
        LOG("Parsing multiplicative_expr: base case");
        $$ = $1;
    }
    | multiplicative_expr OPER_MUL postfix_expr {
        LOG("Parsing multiplicative_expr: *");
        $$ = new BinaryNode($1, $3, "*");
        LOG("multiplicative_expr (*) parsed successfully");
    }
    | multiplicative_expr OPER_DIV postfix_expr {
        LOG("Parsing multiplicative_expr: /");
        $$ = new BinaryNode($1, $3, "/");
        LOG("multiplicative_expr (/) parsed successfully");
    }
    ;

// 新增：后缀表达式（用于函数调用）
postfix_expr:
    unary_expr {
        LOG("Parsing postfix_expr: base case");
        $$ = $1;
    }
    | postfix_expr LPAREN arg_list RPAREN %prec CALL {
        LOG("Parsing postfix_expr: function call");
        // 注意：我们需要处理函数名为表达式的情况，这里暂时用 VarRefNode 来表示函数名
        // 实际上，FuncCallExprNode 接受的是 std::string 作为函数名，
        // 我们需要创建一个特殊的节点或者修改 AST 结构
        // 这里先简化处理，假设函数名就是标识符
        std::string func_name = "anonymous";
        auto var_ref = dynamic_cast<VarRefNode*>($1);
        if (var_ref) {
            func_name = var_ref->name;
        }
        $$ = new FuncCallExprNode(func_name, *$3);
        LOG("postfix_expr (function call) parsed successfully");
        delete $3;
    }
    | KW_DO LPAREN param_list RPAREN block_stmt {
        LOG("Parsing postfix_expr: anonymous function");
        std::vector<std::string> params;
        for (auto& node : *$3) {
            auto var_ref = dynamic_cast<VarRefNode*>(node);
            if (var_ref) {
                params.push_back(var_ref->name);
            }
        }
        // 注意：匿名函数需要特殊处理，这里我们暂时用 FuncCallExprNode 或其他方式表示
        // 为了简化，我们先创建一个特殊的标识符表示匿名函数
        // 实际上，我们可能需要新的 AST 节点
        // 这里我们先用一个简单的占位符
        $$ = new VarRefNode("(anonymous function)");
        LOG("postfix_expr (anonymous function) parsed successfully");
        delete $3;
    }
    ;

// 新增：一元表达式规则
unary_expr:
    factor {
        LOG("Parsing unary_expr: base case");
        $$ = $1;
    }
    | OPER_NOT unary_expr {
        LOG("Parsing unary_expr: !");
        $$ = new UnaryNode("!", $2);
        LOG("unary_expr (!) parsed successfully");
    }
    | OPER_MINUS unary_expr %prec UMINUS {
        LOG("Parsing unary_expr: - (unary)");
        $$ = new UnaryNode("-", $2);
        LOG("unary_expr (-) parsed successfully");
    }
    ;

factor:
    number { 
        LOG("Parsing factor: number");
        $$ = $1; 
    }
    | identifier { 
        LOG("Parsing factor: identifier");
        $$ = $1; 
    }
    | LPAREN expr RPAREN { 
        LOG("Parsing factor: (expr)");
        $$ = $2; 
    }
    ;

number:
    NUM_LITERAL {
        LOG("Parsing number: " + *$1);
        $$ = new NumberNode(*$1);
        LOG("number parsed successfully");
        delete $1;
    }
    ;

identifier:
    IDENTIFIER {
        LOG("Parsing identifier: " + *$1);
        $$ = new VarRefNode(*$1);
        LOG("identifier parsed successfully");
        delete $1;
    }
    ;

%%

void yyerror(const char* s) {
    has_err = true;
    detail_msg = std::string("Details: ") + s;
}
