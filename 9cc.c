#include <ctype.h>
#include <stdarg.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

//トークンの種類(型)
typedef enum {
     TK_RESERVED,   // 記号
     TK_NUM,        // 整数トークン
     TK_EOF,        // 入力の終わりを表すトークン
} TokenKind;

typedef enum {
    ND_ADD, // +
    ND_SUB, // -
    ND_MUL, // *
    ND_DIV, // /
    ND_NUM, // 整数
} NodeKind;

typedef struct Node Node;

// 抽象構文木のノードの型
struct Node {
    NodeKind kind;  // ノードの型
    Node *lhs;      // 左辺
    Node *rhs;      // 右辺
    int val;        // kindがND_NUMの場合のみ使う
};

typedef struct Token Token;

struct Token {
    TokenKind kind; // トークンの型
    Token *next;    // 次の入力トークン
    int val;        // kindがTK_NUMの場合、その数値
    char *str;      // トークン文字列
};

// 現在着目しているトークン
Token *token;

// ユーザーの入力を格納するグローバル変数
char *user_input;

// エラーを報告するための関数
void error_at(char *loc, char *fmt, ...) {
    va_list ap;
    va_start(ap, fmt);

    // loc:エラーが起きている文字列の位置（バイト数） user_input:文字列の先頭の位置（バイト数）
    // user_inputの先頭のバイト数からどれだけ進んだかをloc（バイト数）からuser_input（バイト数）を引いて出している。
    int pos = loc - user_input;

    fprintf(stderr, "%s\n", user_input);
    fprintf(stderr, "%*s", pos, " ");    // pos個の空白を出力
    fprintf(stderr, "^ ");
    vfprintf(stderr, fmt, ap);
    fprintf(stderr, "\n");
    exit(1);
}

// 次のトークンが期待している記号の時には、トークンを1つ読み進めて
// 真を返す。それ以外の場合には偽を返す。
bool consume(char op) {
    // 記号ではない or 指定された文字列(op)ではないとき。
    if (token->kind != TK_RESERVED || token->str[0] != op) {
        return false;
    }
    token = token->next;
    return true;
}

// 次のトークンが期待している記号の時には、トークンを1つ読み進める。
// それ以外の場合にはエラーを報告する。
void expect(char op) {
    if (token->kind != TK_RESERVED || token->str[0] != op) {
        error_at(token->str, "'%c'ではありません", op);
    }
    token = token->next;
}

// 次のトークンが数値の場合、トークンを1つ読み進めてその数値を返す。
// それ以外の場合にはエラーを報告する。
int expect_number() {
    if (token->kind != TK_NUM) {
        error_at(token->str, "数ではありません");
    }
    int val = token->val;
    token = token->next;
    return val;
}

bool at_eof() {
    return token->kind == TK_EOF;
}

// 新しいトークンを作成してcurに繋げる
Token *new_token(TokenKind kind, Token *cur, char *str) {
    Token *tok = calloc(1, sizeof(Token));
    tok->kind = kind;
    tok->str = str;
    cur->next = tok;
    return tok;
}

// 入力文字列pをトークナイズしてそれを返す
Token *tokenize() {
    char *p = user_input;
    Token head;
    head.next = NULL;
    Token *cur = &head;

    while (*p) {
        //空白文字列をスキップ
        if (isspace(*p)) {
            p++;
            continue;
        }

        if (*p == '+' || *p =='-') {
            cur = new_token(TK_RESERVED, cur, p);
            p++;
            continue;
        }

        if (isdigit(*p)) {
            cur = new_token(TK_NUM, cur, p);
            cur->val = strtol(p, &p, 10);
            continue;
        }

        error_at(p, "トークナイズできません");
    }

    new_token(TK_EOF, cur, p);
    return head.next;
}

// 新たにノードを生成する
Node *new_node(NodeKind kind, Node *lhs, Node *rhs) {
    Node *node = calloc(1, sizeof(Node));
    node->kind = kind;
    node->lhs = lhs;
    node->rhs = rhs;
    return node;
}

// ノードの末端である数字のノードを生成する
Node *new_node_num(int val) {
    Node *node = calloc(1, sizeof(Node));
    node->kind = ND_NUM;
    node->val = val;
    return node;
}

// ノードのポインタを返す。（=ノードのアドレスを返している, retunr先の関数がポインタを参照できアドレスに入ってる値を操作できる）
// expr    = mul ("+" mul | "-" mul)*
Node *expr() {
    Node *node = mul();

    for(;;) {
        if (consume('+')) {
            node = new_node(ND_ADD, node, mul());
        } else if (consume('-')) {
            node = new_node(ND_SUB, node, mul());
        } else {
            return node;
        }
    }
}

// mul     = primary ("*" primary | "/" primary)*
Node *mul() {
    Node *node = primary();

    for(;;) {
        if (consume('*')) {
            node = new_node(ND_MUL, node, primary());
        } else if (consume('/')) {
            node = new_node(ND_DIV, node, primary());
        } else {
            return node;
        }
    }
}

// primary = num | "(" expr ")"
Node *primary() {
    // 次のトークンが "(" なら "(" expr ")" のはず
    if (consume('(')) {
        Node *node = expr();
        expect(')');
        return node;
    }

    // そうでなければ数値のはず
    return new_node_num(expect_number());
}

int main(int argc, char **argv) {
    if (argc != 2) {
        fprintf(stderr, "引数の個数が正しくありません。\n");
        return 1;
    }

    //グローバル変数のuser_inputに引数を代入 
    user_input = argv[1];
    // トークナイズする
    token = tokenize(); 

    // アセンブリの前半部分を出力
    printf(".intel_syntax noprefix\n");
    printf(".globl main\n");
    printf("main:\n");

    // 式の最初は数でなければならないので、それをチェックして
    // 最初のmov命令を出力
    printf("    mov rax, %d\n", expect_number());   // 例) rax=2 , token='+' 30 '-' 5

    // `+ <数>`あるいは`- <数>`というトークンの並びを消費しつつ
    // アセンブリを出力
    while (!at_eof()) { //token.kindがTK_EOFじゃない間は繰り返す。
        if (consume('+')) { // 例) rax=2, token=30 '-' 5
            printf("    add rax, %d\n", expect_number()); // 例) rax=32, token='-' 5
            continue;
        }

        expect('-'); // 例) rax=32, token=5
        printf("    sub rax, %d\n", expect_number()); // 例) rax=27 token=
    }

    printf("    ret\n");

    // 以下は実行不可スタックの明示
    printf("\n");
    printf(".section .note.GNU-stack,\"\",@progbits\n");

    return 0;
}