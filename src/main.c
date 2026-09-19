#include <stdio.h>
#include <stdint.h>
#include <stdbool.h>

typedef int8_t  s8;
typedef int16_t s16;
typedef int32_t s32;
typedef int64_t s64;

#define S8_MIN  ((s8) 0x80)
#define S16_MIN ((s16)0x8000)
#define S32_MIN ((s32)0x80000000)
#define S64_MIN ((s64)0x8000000000000000LL)

#define S8_MAX  ((s8) 0x7F)
#define S16_MAX ((s16)0x7FFF)
#define S32_MAX ((s32)0x7FFFFFFF)
#define S64_MAX ((s64)0x7FFFFFFFFFFFFFFFLL)

typedef uint8_t  u8;
typedef uint16_t u16;
typedef uint32_t u32;
typedef uint64_t u64;

#define U8_MAX  ((u8) 0xFFU)
#define U16_MAX ((u16)0xFFFFU)
#define U32_MAX ((u32)0xFFFFFFFFU)
#define U64_MAX ((u64)0xFFFFFFFFFFFFFFFFULL)

typedef struct String
{
	char* data;
	u64 len;
} String;

#define STRING(S) (String){ .data = (char*)(S), .len = sizeof(S)-1 }

bool
IsWhitespace(char c)
{
	return ((u8)(c - 1) < (u8)0x20);
}

bool
IsDigit(char c)
{
	return ((u8)(c - '0') < (u8)10);
}

bool
IsAlpha(char c)
{
	return ((u8)((c & 0xDF) - 'A') <= (u8)('Z' - 'A'));
}

bool
IsAlphaNumOrUnderscore(char c)
{
	return (IsAlpha(c) || IsDigit(c) || c == '_');
}

typedef enum Token_Kind
{
	Token_Invalid = 0,
	Token_EOF     = 1,
	Token__PastLastTerminatingToken,

	Token_Backslash,
	Token_Dot,
	Token_ColonEq,
	Token_OpenParen,
	Token_CloseParen,

	Token_Ident,

	Token__FirstKeyword,
	Token_Let = Token__FirstKeyword,
	Token__PastLastKeyword,
} Token_Kind;

typedef struct Token
{
	Token_Kind kind;
	String ident;
} Token;

typedef struct Lexer
{
	String input;
	u64 cursor;
	Token current_token;
} Lexer;

Token Lexer_NextToken(Lexer* lexer);

Lexer
Lexer_Init(String input)
{
	Lexer lexer = {
		.input  = input,
		.cursor = 0,
	};

	Lexer_NextToken(&lexer);

	return lexer;
}

Token
Lexer_GetToken(Lexer* lexer)
{
	return lexer->current_token;
}

bool
Lexer_IsToken(Lexer* lexer, Token_Kind kind)
{
	return (lexer->current_token.kind == kind);
}

bool
Lexer_IsTerminatingToken(Lexer* lexer)
{
	return (lexer->curent_token.kind < Token__PastLastTerminatingToken);
}

bool
Lexer_EatToken(Lexer* lexer, Token_Kind kind)
{
	if (lexer->current_token.kind == kind)
	{
		Lexer_NextToken(lexer);
		return true;
	}

	return false;
}

Token
Lexer_NextToken(Lexer* lexer)
{
	// keep returning Invalid or EOF when hit
	if (lexer->current_token.kind < Token__PastLastTerminatingToken)
	{
		return lexer->current_token;
	}

	lexer->current_token = (Token){ .kind = Token_Invalid };

	String input = lexer->input;

	while (lexer->cursor < input.len && IsWhitespace(input.data[lexer->cursor]))
	{
		++lexer->cursor;
	}

	if (lexer->cursor >= input.len)
	{
		lexer->current_token.kind = Token_EOF;
	}
	else if (IsAlpha(input.data[lexer->cursor]))
	{
		String identifier = { .data = input.data + lexer->cursor, .len = 0 };

		++lexer->cursor;

		while (lexer->cursor < input.len && IsAlphaNumOrUnderscore(input.data[lexer->cursor]))
		{
			++lexer->cursor;
		}

		if   (String_Equals(identifier, STRING("let"))) lexer->current_token.kind = Token_Let;
		else                                            lexer->current_token.kind = Token_Ident;

		lexer->current_token.ident = identifier;
	}
	else
	{
		char c = input.data[lexer->cursor];
		lexer->cursor += 1;

		switch (c)
		{
			case '\\': lexer->current_token.kind = Token_Backslash;  break;
			case  '.': lexer->current_token.kind = Token_Dot;        break;
			case  '(': lexer->current_token.kind = Token_OpenParen;  break;
			case  ')': lexer->current_token.kind = Token_CloseParen; break;

			case ':':
			{
				if (lexer->cursor < input.len && input.data[lexer->cursor] == '=')
				{
					lexer->current_token.kind = Token_ColonEq;
					++lexer->cursor;
				}
				else
				{
					//// ERROR: Invalid token, : must be directly followed by =
					lexer->current_token.kind = Token_Invalid;
				}
			} break;

			default:
			{
				//// ERROR: Invalid token
				lexer->current_token.kind = Token_Invalid;
			} break;
		}
	}

	return lexer->current_token;
}

typedef struct Atom
{
	// TODO
	String s;
} Atom;

typedef enum Node_Kind
{
	Node_Invalid = 0,

	Node_Term,
	Node_Application,
	Node_Lambda,
	Node_Let,
} Node_Kind;

typedef struct Node Node;
typedef struct Node
{
	Node_Kind kind;

	union
	{
		Atom term;

		struct
		{
			Node* top; /*wink*/
			Node* bottom;
		} application;

		struct
		{
			Atom arg;
			Node* body;
		} lambda;

		struct
		{
			Atom name;
			Node* body;
		} let;
	};
} Node;

bool
ParseExpression(Lexer* lexer, Node** node)
{
	if (Lexer_IsToken(lexer, Token_CloseParen))
	{
		if (nesting <= 0)
		{
			//// ERROR
			return false;
		}
		else
		{
			Lexer_NextToken(lexer);
			nesting -= 1;
		}
	}
	else if (Lexer_EatToken(lexer, Token_OpenParen))
	{
		nesting += 1;
	}
	else if (Lexer_EatToken(lexer, Token_Backslash))
	{
		if (!Lexer_IsToken(lexer, Token_Ident))
		{
			//// ERROR: Missing lambda argument name
			return false;
		}

		Lexer_NextToken(lexer);

		if (!Lexer_EatToken(lexer, Token_Dot))
		{
			//// ERROR: Missing dot between lambda argument name and body
			return false;
		}

		// TODO
		Node* lambda = malloc(sizeof(Node));
		*lambda = (Node){
			.kind = Node_Lambda,
			.arg  = ,
			.body = 0,
		};

		if (node == 0) *node = lambda;
		else
		{
			Node* applicatioon = malloc(sizeof(Node));
			application->top = *node;
			application->bottom = 
		}

		\x.x y
		\x.(x y)
		(\x.x) y
		\x.(x y)

		node = &lambda->body;
	}
	else if (Lexer_IsToken(lexer, Token_Ident))
	{
		// TODO
		Lexer_NextToken(lexer);

		Node* lambda_term = malloc(sizeof(Node));
		*lambda_term = (Node){
			.kind = Node_Atom,
			.term = ,
		};
	}
	else
	{
		if (Lexer_IsToken(lexer, Token_Error))
		{
			//// ERROR
			return false;
		}
		else
		{
			//// ERROR: Expected a lambda, lambda term or parentheses, not XXX
			return false;
		}
	}
}

int
main(int argc, char** argv)
{
	return 0;
}
