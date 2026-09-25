from enum import Enum
from typing import Any
import copy

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

def _ParseExpression(lexer: Lexer) -> tuple[bool, Any]:
    result = None
    while True:
        expr = None

        if lexer.EatToken(Token_Kind.Backslash):
            if not lexer.IsToken(Token_Kind.Ident):
                # ERROR
                return False, None

            variable = lexer.current_token.ident
            lexer.NextToken()

            if not lexer.EatToken(Token_Kind.Dot):
                # ERROR
                return False, None

            ok, body = _ParseExpression(lexer)
            if not ok:
                return False, None

            expr = Abstraction(variable, body)

        elif lexer.EatToken(Token_Kind.OpenParen):
            ok, inner_expr = _ParseExpression(lexer)
            if not ok:
                return False, None

            if not lexer.EatToken(Token_Kind.CloseParen):
                # ERROR
                return False, None

            expr = inner_expr

        elif lexer.IsToken(Token_Kind.Ident):
            ident = lexer.current_token.ident
            lexer.NextToken()

            expr = Variable(ident)

        else:
            # ERROR
            return False, None

        if result is None:
            result = expr
        else:
            top    = result
            bottom = expr

            result = Application(top, bottom)

        if lexer.IsToken(Token_Kind.CloseParen) or lexer.IsToken(Token_Kind.EOF):
            return True, result

def ParseExpression(lexer: Lexer) -> tuple[bool, Any]:
    ok, expr = _ParseExpression(lexer)
    if not ok:
        # ERROR
        return False, None

    if not lexer.IsToken(Token_Kind.EOF):
        # ERROR
        return False, None

    return True, expr

def L(inp: str) -> Any:
    ok, expr = ParseExpression(Lexer(inp))
    assert ok

    return expr

def FreeVars(term: Any) -> set[str]:
    if type(term) is Variable:
        return { term.ident }
    elif type(term) is Application:
        return FreeVars(term.top) | FreeVars(term.bottom)
    else:
        assert type(term) is Abstraction
        return FreeVars(term.body) - { term.variable }

def Subst(term: Any, var: str, subst: Any) -> Any:
    if type(term) is Variable:
        if term.ident == var:
            return subst
        else:
            return term

    elif type(term) is Application:
        term = copy.deepcopy(term)
        term.top    = Subst(term.top, var, subst)
        term.bottom = Subst(term.bottom, var, subst)
        return term

    else:
        assert type(term) is Abstraction
        
        if term.variable == var:
            return term

        elif term.variable not in FreeVars(subst):
            term = copy.deepcopy(term)
            term.body = Subst(term.body, var, subst)
            return term

        else:
            term = copy.deepcopy(term)
            old_name = term.variable
            new_name = old_name + "'"

            term.variable = new_name
            term.body     = Subst(term.body, old_name, Variable(new_name))

            term.body = Subst(term.body, var, subst)

            return term

def Reduce(term: Any) -> Any:
    if type(term) is Variable:
        return term

    elif type(term) is Application:
        if type(term.top) is Abstraction:
            term = Subst(term.top.body, term.top.variable, term.bottom)
            return Reduce(term)

        else:
            term = copy.deepcopy(term)
            term.top    = Reduce(term.top)
            term.bottom = Reduce(term.bottom)

            return term

    else:
        assert type(term) is Abstraction
        
        term = copy.deepcopy(term)
        term.body = Reduce(term.body)

        return term
#t = L(r"x (y ((\a.a) b)) ((\c.c) d)")
#print(t, Reduce(t), sep="\n")

omega = L(r"(\x.x x) (\x.x x)")
t = L(r"(\a. \y. y (a (\x.x x) (\x.x x)) ((\x.\y. x y) y) ((\x.\x. x) a) (\z. (\q. q) z)) ((\p. p) (\u.\v. v))")
b = L(r"\y. y (\v. v) (\y1. y y1) (\x. x) (\z. z)")

print(t)
for i in range(4):
    t = Reduce(t)
    print(t)

print(b)

"""
    if type(term) is Variable:
        pass
    elif type(term) is Application:
        pass
    else:
        assert type(term) is Abstraction
        pass
"""
