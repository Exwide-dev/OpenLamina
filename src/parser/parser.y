%defines "parser.tab.hpp"
%output  "parser.tab.cpp"

// 必须在 %union 之前包含所需的类型声明
%code requires {
    #include <vector>
    #include <string>
    #include "parser/ast.hpp"
}

%code {
    #include <iostream>
    using namespace lmx;
    
    // 调试输出宏
    #define DEBUG 1
    #if DEBUG
        #define LOG(msg) std::cout << "[DEBUG] " << msg << std::endl
    #else
        #define LOG(msg) do {} while (0)
    #endif
}

%code {
    extern lmx::ASTNode* result;
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
%token <string_val> ASSIGN
%token <string_val> LPAREN
%token <string_val> RPAREN
%token <string_val> NUM_LITERAL
%token <string_val> IDENTIFIER
%token <string_val> KW_LET

%type <ast_node> program
%type <ast_node> stmt
%type <ast_node> var_decl
%type <expr_node> expr
%type <expr_node> additive_expr
%type <expr_node> multiplicative_expr
%type <expr_node> factor
%type <expr_node> number
%type <expr_node> identifier
%type <ast_list> stmt_list

%left OPER_PLUS OPER_MINUS
%left OPER_MUL OPER_DIV

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
    ;

var_decl:
    KW_LET IDENTIFIER ASSIGN expr {
        LOG("Parsing var_decl: let " + *$2 + " = ...");
        $$ = new VarDeclNode(*$2, $4, false);
        LOG("var_decl parsed successfully");
        delete $2;
    }
    ;

expr:
    additive_expr { 
        LOG("Parsing expr");
        $$ = $1; 
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
    factor {
        LOG("Parsing multiplicative_expr: base case");
        $$ = $1;
    }
    | multiplicative_expr OPER_MUL factor {
        LOG("Parsing multiplicative_expr: *");
        $$ = new BinaryNode($1, $3, "*");
        LOG("multiplicative_expr (*) parsed successfully");
    }
    | multiplicative_expr OPER_DIV factor {
        LOG("Parsing multiplicative_expr: /");
        $$ = new BinaryNode($1, $3, "/");
        LOG("multiplicative_expr (/) parsed successfully");
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
    std::cerr << "Parser error: " << s << std::endl;
}
