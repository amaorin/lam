from enum import Enum
from typing import Any

class Token_Kind(Enum):
    Invalid = 0
    EOF     = 1

    Backslash  = 2
    Dot        = 3
    OpenParen  = 4
    CloseParen = 5

    Ident = 6

class Token:
    kind: Token_Kind
    ident: str

    def __init__(self, kind: Token_Kind, ident: str = None):
        self.kind  = kind
        self.ident = ident

    def __str__(self):
        return "Token(" + str(self.kind)  + (", " + self.ident if self.ident != None else "") + ")"

    def IsTerminating(self):
        return (self.kind == Token_Kind.Invalid or self.kind == Token_Kind.EOF)

class Lexer:
    contents: str
    cursor: int
    curren_token: Token

    def __init__(self, contents: str):
        self.contents = contents
        self.cursor = 0
        
        self.NextToken()

    def NextToken(self):
        if self.cursor > 0 and self.current_token.IsTerminating():
            return self.current_token

        self.current_token = Token(Token_Kind.Invalid)

        while self.cursor < len(self.contents) and self.contents[self.cursor] in [" ", "\t", "\n", "\r"]:
            self.cursor += 1

        if self.cursor >= len(self.contents): self.current_token = Token(Token_Kind.EOF)
        else:
            c = self.contents[self.cursor]
            self.cursor += 1

            if   c == "\\": self.current_token = Token(Token_Kind.Backslash)
            elif c ==  ".": self.current_token = Token(Token_Kind.Dot)
            elif c ==  "(": self.current_token = Token(Token_Kind.OpenParen)
            elif c ==  ")": self.current_token = Token(Token_Kind.CloseParen)
            elif c.isalpha():
                start = self.cursor - 1
                
                while self.cursor < len(self.contents) and (self.contents[self.cursor].isalnum() or self.contents[self.cursor] == "_"):
                    self.cursor += 1

                ident = self.contents[start:self.cursor]

                self.current_token = Token(Token_Kind.Ident, ident)

            else:
                self.current_token = Token(Token_Kind.Invalid)

        return self.current_token

    def IsToken(self, kind):
        return (self.current_token.kind == kind)

    def EatToken(self, kind):
        if self.current_token.kind == kind:
            self.NextToken()
            return True

        return False

class Variable:
    ident: str

    def __init__(self, ident: str):
        self.ident = ident

    def __str__(self):
        return self.ident

class Abstraction:
    variable: str
    body: Any

    def __init__(self, variable: str, body: Any):
        self.variable = variable
        self.body     = body

    def __str__(self):
        return f"\\{self.variable}." + (f"({self.body})" if type(self.body) is Application else str(self.body))

class Application:
    top: Any
    bottom: Any

    def __init__(self, top: Any, bottom: Any):
        self.top    = top
        self.bottom = bottom

    def __str__(self):
        a = str(self.top) if type(self.top) is Variable else f"({self.top})"
        b = str(self.bottom) if type(self.bottom) is Variable else f"({self.bottom})"
        return a + " " + b


def ParseInnerExpression(lexer: Lexer) -> tuple[bool, Any]:
    if lexer.EatToken(Token_Kind.Backslash):
        if not lexer.IsToken(Token_Kind.Ident):
            # ERROR
            return False, None

        ident = lexer.current_token.ident
        lexer.NextToken()

        if not lexer.EatToken(Token_Kind.Dot):
            # ERROR
            return False, None

        ok, body = ParseInnerExpression(lexer)
        if not ok:
            # ERROR
            return False, None

        return True, Abstraction(ident, body)

    elif lexer.EatToken(Token_Kind.OpenParen):
        
        ok, expr = ParseOuterExpression(lexer)
        if not ok:
            # ERROR
            return False, None

        if not lexer.EatToken(Token_Kind.CloseParen):
            # ERROR
            return False, None

        return True, expr

    elif lexer.IsToken(Token_Kind.Ident):
        ident = lexer.current_token.ident
        lexer.NextToken()

        return True, Variable(ident)

    else:
        # ERROR
        return False, None

def ParseOuterExpression(lexer: Lexer) -> tuple[bool, Any]:
    ok, expr = ParseInnerExpression(lexer)
    if not ok:
        # ERROR
        return False, None

    if not (lexer.IsToken(Token_Kind.EOF) or lexer.IsToken(Token_Kind.CloseParen)):
        top = expr
        nok, bottom = ParseOuterExpression(lexer)

        if not nok:
            # ERROR
            return False, None

        expr = Application(top, bottom)

    return True, expr

def ParseExpression(lexer: Lexer) -> tuple[bool, Any]:
    ok, expr = ParseOuterExpression(lexer)

    if not ok:
        # ERROR
        return False, None

    if not lexer.IsToken(Token_Kind.EOF):
        # ERROR
        return False, None

    return True, expr

def FreeVars(term: Any) -> set[str]:
    if type(term) is Variable:
        return { term.ident }
    elif type(term) is Application:
        return FreeVars(term.top) | FreeVars(term.bottom)
    else:
        assert type(term) is Abstraction
        return FreeVars(term.body) - { term.variable }

# TODO: consider deep copy
def CaptureAvoidingSubst(term: Any, var: str, subst: Any) -> Any:
    if type(term) is Variable:
        if term.ident == var:
            return subst
        else:
            return term

    elif type(term) is Application:
        term.top    = CaptureAvoidingSubst(term.top, var, subst)
        term.bottom = CaptureAvoidingSubst(term.bottom, var, subst)

        return term

    else:
        assert type(term) is Abstraction
        
        if term.variable == var:
            return term

        elif term.variable not in FreeVars(subst):
            term.body = CaptureAvoidingSubst(term.body, var, subst)
            return term

        else:
            original_var = term.variable
            rename_var = original_var + "'"

            term.variable = rename_var
            term.body = CaptureAvoidingSubst(term.body, original_var, Variable(rename_var))
            term.body = CaptureAvoidingSubst(term.body, var, subst)

            return term

def BetaReduction(term: Any) -> tuple[bool, Any]:
    if (type(term) is Application
        and type(term.top) is Abstraction):

        return True, CaptureAvoidingSubst(term.top.body, term.top.variable, term.bottom)

    else:
        return False, term

def EtaConversion(term: Any) -> tuple[bool, Any]:
    if (type(term) is Abstraction
        and type(term.body) is Application
        and type(term.body.bottom) is Variable
        and term.body.bottom.ident == term.variable
        and term.body.bottom.ident not in FreeVars(term.body.top)):
        
        return True, term.body.top

    else:
        return False, term

def BetaNormalForm(term: Any):
    if type(term) is Variable:
        return term

    elif type(term) is Application:
        term.top    = BetaNormalForm(term.top)
        term.bottom = BetaNormalForm(term.bottom)

        progress, term = BetaReduction(term)

        if progress:
            print("beta", term)
            term = BetaNormalForm(term)

        return term

    else:
        assert type(term) is Abstraction
        
        term.body = BetaNormalForm(term.body)

        progress, term = EtaConversion(term)

        if progress:
            print("eta", term)

        return term

def L(inp: str):
    lexer = Lexer(inp)
    ok, expr = ParseExpression(lexer)
    assert ok

    return expr

#print(BetaNormalForm(L(r"\z.(\x.(x x) y)")))

TRUE  = L(r"\x.\y.x")
FALSE = L(r"\x.\y.y")
AND   = L(r"\p.\q.(p q p)")

t = L(f"{AND} {TRUE} {FALSE}")

print(t)

print(BetaNormalForm(t))
