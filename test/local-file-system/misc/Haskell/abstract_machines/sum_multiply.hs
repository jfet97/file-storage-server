data Expr = Val Int | Add Expr Expr | Mul Expr Expr deriving (Show)

data Op = EVALA Expr | ADD Int | EVALM Expr | MUL Int
type Cont = [Op]

eval :: Expr -> Cont -> Int
eval (Val n) c = exec c n
eval (Add e f) c = eval e (EVALA f : c)
eval (Mul e f) c = eval e (EVALM f : c)

exec :: Cont -> Int -> Int
exec [] n = n
exec (EVALA f : c) n = eval f (ADD n : c)
exec (EVALM f : c) n = eval f (MUL n : c)
exec (ADD n : c) m = exec c (n+m)
exec (MUL n : c) m = exec c (n*m)

value :: Expr -> Int
value e = eval e []
