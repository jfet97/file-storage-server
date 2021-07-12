data Op = Add | Sub | Mul | Div

instance Show Op where
  show Add = "+"
  show Sub = "-"
  show Mul = "*"
  show Div = "/"

ops :: [Op]
ops = [Add, Sub, Mul, Div]

valid :: Op -> Int -> Int -> Bool
valid Add _ _ = True
valid Sub x y = x > y -- 0 not permitted
valid Mul _ _ = True
valid Div x y = x `mod` y == 0

apply :: Op -> Int -> Int -> Int
apply Add x y = x + y
apply Sub x y = x - y
apply Mul x y = x * y
apply Div x y = x `div` y


data Expr = Val Int | App Op Expr Expr

instance Show Expr where
  show (Val x) = show x
  show (App o l r) = brak l ++ show o ++ brak r
                     where
                        brak (Val x) = show x
                        brak e = "(" ++ show e ++ ")"

-- extract the Int values from an expression, packaging them into a list
values :: Expr -> [Int]
values (Val x) = [x]
values (App _ l r) = values l ++ values r

eval :: Expr -> [Int] -- "Maybe" Int where [] == Nothing
eval (Val x) = [x | x > 0]
eval (App o l r) = [apply o x y | x <- eval l, -- if eval l == [] then no list comprehension will happen, [] will be the result
                                  y <- eval r, -- if eval r == [] then no list comprehension will happen, [] will be the result
                                  valid o x y] -- if valid o x y == False then no list comprehension will happen, [] will be the result


set_of_parts :: [a] -> [[a]]
-- the set of parts of the empty set is a set containing only the empty set
set_of_parts [] = [[]]
-- the set of parts of a generic set is formed by all the parts obtained by discarding an element
-- plus these parts where the discarded element is re-added, and that should happen foreach element
set_of_parts (x:xs) = xss ++ map (x:) xss
                    where xss = set_of_parts xs

-- returns all possible ways of inserting a new element into a list
interleave :: a -> [a] -> [[a]]
interleave x [] = [[x]]
interleave x (y:ys) = (x:y:ys) : map (y:) (interleave x ys)

-- returns all permutations of a list
perms :: [a] -> [[a]]
perms [] = [[]]
perms (x:xs) = concat (map (interleave x) pxs)
              where pxs = perms xs

-- returns all permutations of all set of set_of_parts
choices :: [a] -> [[a]]
choices xs = concat . map perms $ set_of_parts xs


-- check a solution to the countdown problem
solution :: Expr -> [Int] -> Int -> Bool
solution e xs x = elem (values e) (choices xs) && eval e == [x]

-- split a list into two non-empty ones that, if appended, give the original one
split :: [a] -> [([a],[a])]
split [] = []
split [_] = []
split (x:xs) = ([x], xs) : [(x:ls, rs) | (ls, rs) <- split xs]


-- combine two expressions using all the operators
combine :: Expr -> Expr -> [Expr]
combine l r = [App o l r | o <- ops]

-- returns all possible expressions whose list of values is precisely a given list
exprs :: [Int] -> [Expr]
exprs [] = []
exprs [x] = [Val x]
exprs xs = [e | (ls, rs) <- split xs, l <- exprs ls, r <- exprs rs, e <- combine l r]

-- returns all the solutions
solutions :: [Int] -> Int -> [Expr]
solutions xs x =
  [e | xs' <- choices xs, e <- exprs xs', eval e == [x]]
