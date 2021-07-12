data Expr = Val Int | Add Expr Expr deriving (Show)

folde :: (Int -> a) -> (a -> a -> a) -> Expr -> a
folde f g (Val i) = f i
folde f g (Add e1 e2) = g (folde f g e1) (folde f g e2)
