#include <stdlib.h>
#include <stdio.h>
#include <stdint.h>
#include <stdbool.h>
#include <string.h>

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

typedef s64 smm;
typedef u64 umm;

#define SMM_MIN S64_MIN
#define SMM_MAX S64_MAX
#define UMM_MAX U64_MAX

#define ARRAY_LEN(A) (sizeof(A)/sizeof(0[A]))

typedef struct String
{
	char* data;
	u64 len;
} String;

#define STRING(S) (String){ .data = (char*)(S), .len = sizeof(S)-1 }

bool
String_Equals(String a, String b)
{
	if (a.len != b.len) return false;

	for (umm i = 0; i < a.len; ++i)
	{
		if (a.data[i] != b.data[i]) return false;
	}

	return true;
}

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
	return (lexer->current_token.kind < Token__PastLastTerminatingToken);
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
	// keep returning Invalid or EOF when hit after the first token has been consumed
	if (lexer->cursor > 0 && lexer->current_token.kind < Token__PastLastTerminatingToken)
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
		String identifier = { .data = input.data + lexer->cursor };

		++lexer->cursor;

		while (lexer->cursor < input.len && IsAlphaNumOrUnderscore(input.data[lexer->cursor]))
		{
			++lexer->cursor;
		}

		identifier.len = (lexer->cursor + input.data) - identifier.data;

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
					//// ERROR
					printf("ERROR: Invalid token, : must be directly followed by =\n");
					lexer->current_token.kind = Token_Invalid;
				}
			} break;

			default:
			{
				//// ERROR
				printf("ERROR: Invalid token \"%c\"\n", c);
				lexer->current_token.kind = Token_Invalid;
			} break;
		}
	}

	return lexer->current_token;
}

typedef enum Term_Kind
{
	Term_Invalid = 0,

	Term_Variable,
	Term_Abstraction,
	Term_Application,
} Term_Kind;

typedef struct Term
{
	Term_Kind kind;

	union
	{
		String variable;

		struct
		{
			String variable;
			struct Term* body;
		} abstraction;

		struct
		{
			struct Term* top; /* wink */
			struct Term* bottom;
		} application;
	};
} Term;

bool
ParseExpression(Lexer* lexer, Term** term, bool precedence)
{
	if (Lexer_EatToken(lexer, Token_Backslash))
	{
		if (!Lexer_IsToken(lexer, Token_Ident))
		{
			//// ERROR
			printf("ERROR: Expected variable name after \\ in abstraction\n");
			return false;
		}

		String variable = Lexer_GetToken(lexer).ident;
		Lexer_NextToken(lexer);

		if (!Lexer_EatToken(lexer, Token_Dot))
		{
			//// ERROR
			printf("ERROR: Missing dot between variable and body in abstraction\n");
			return false;
		}

		Term* body = 0;
		if (!ParseExpression(lexer, &body, true)) return false;

		*term = malloc(sizeof(Term));
		**term = (Term){
			.kind        = Term_Abstraction,
			.abstraction = {
				.variable = variable,
				.body     = body,
			},
		};
	}
	else if (Lexer_EatToken(lexer, Token_OpenParen))
	{
		if (!ParseExpression(lexer, term, false)) return false;

		if (!Lexer_EatToken(lexer, Token_CloseParen))
		{
			//// ERROR
			printf("ERROR: Missing matching closing parenthesis\n");
			return false;
		}
	}
	else if (Lexer_IsToken(lexer, Token_Ident))
	{
		String variable = Lexer_GetToken(lexer).ident;
		Lexer_NextToken(lexer);

		*term = malloc(sizeof(Term));
		**term = (Term){
			.kind     = Term_Variable,
			.variable = variable,
		};
	}
	else
	{
		if (Lexer_IsToken(lexer, Token_Invalid))
		{
			//// ERROR
			printf("ERROR: Lexer error\n");
			return false;
		}
		else
		{
			//// ERROR
			printf("ERROR: Unexpected token\n");
			return false;
		}
	}

	bool is_end = (Lexer_IsTerminatingToken(lexer) || Lexer_IsToken(lexer, Token_CloseParen));
	if (!precedence && !is_end)
	{
		Term* top    = *term;
		Term* bottom = 0;

		if (!ParseExpression(lexer, &bottom, false)) return false;

		*term = malloc(sizeof(Term));
		**term = (Term){
			.kind        = Term_Application,
			.application = {
				.top    = top,
				.bottom = bottom,
			},
		};
	}

	return true;
}

void
PrintIndent(umm level)
{
	for (umm i = 0; i < level; ++i)
	{
		printf("  ");
	}
}

void
PrintTermAsTree(Term* term, umm level)
{
	PrintIndent(level);

	if (term->kind == Term_Variable)
	{
		printf("Term_Variable(%.*s)\n", (int)term->variable.len, term->variable.data);
	}
	else if (term->kind == Term_Abstraction)
	{
		printf("Term_Abstraction(%.*s)\n", (int)term->abstraction.variable.len, term->abstraction.variable.data);

		PrintIndent(level);
		printf("body:\n");
		PrintTermAsTree(term->abstraction.body, level + 1);
	}
	else if (term->kind == Term_Application)
	{
		printf("Term_Application\n");

		PrintIndent(level);
		printf("top:\n");
		PrintTermAsTree(term->application.top, level + 1);

		PrintIndent(level);
		printf("bottom:\n");
		PrintTermAsTree(term->application.bottom, level + 1);
	}
	else
	{
		printf("ERROR\n");
	}
}

void
PrintTerm(Term* term)
{
	if (term->kind == Term_Variable)
	{
		printf("%.*s", (int)term->variable.len, term->variable.data);
	}
	else if (term->kind == Term_Abstraction)
	{
		printf("\\%.*s.", (int)term->abstraction.variable.len, term->abstraction.variable.data);

		bool should_paren = (term->abstraction.body->kind == Term_Application);

		if (should_paren) printf("(");
		PrintTerm(term->abstraction.body);
		if (should_paren) printf(")");
	}
	else if (term->kind == Term_Application)
	{
		Term* part[2] = { term->application.top, term->application.bottom };

		for (umm i = 0; i < ARRAY_LEN(part); ++i)
		{
			bool should_paren = (part[i]->kind != Term_Variable);

			if (should_paren) printf("(");
			PrintTerm(part[i]);
			if (should_paren) printf(")");

			if (i < ARRAY_LEN(part)-1) printf(" ");
		}
	}
}

int
main(int argc, char** argv)
{
	if (argc == 3 && strcmp(argv[1], "eval") == 0)
	{
		String input = {0};
		input.data = argv[2];
		input.len  = strlen(input.data);

		Lexer lexer = Lexer_Init(input);

		Term* term = 0;
		if (!ParseExpression(&lexer, &term, false))
		{
			return 1;
		}

		PrintTerm(term);
		printf("\n");
	}
	else if (argc == 2 && strcmp(argv[1], "repl") == 0)
	{
		while (true)
		{
			printf("\n> ");

			char buffer[1024];

			char* in = fgets(buffer, ARRAY_LEN(buffer), stdin);

			if (in == 0)
			{
				printf("Failed to read input. Exiting...");
				return 1;
			}

			String input = {0};
			input.data = buffer;
			input.len  = strlen(buffer);

			// TODO: read len error
			
			Lexer lexer = Lexer_Init(input);

			Term* term = 0;
			if (!ParseExpression(&lexer, &term, false))
			{
				continue;
			}

			PrintTerm(term);
		}
	}
	else
	{
		printf("Invalid Arguments. Expected: lam [eval | repl]\n");
		return 1;
	}

	return 0;
}
