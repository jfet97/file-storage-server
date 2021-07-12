type State = Int

newtype ST a = S (State -> (a, State))

app (S st) x = st x

instance Monad ST where
    st >>= f = S (\s -> let (x,s') = app st s in app (f x) s')
    return x = S (\s -> (x, s'))



data Tree a = Leaf a | Node (Tree a) (Tree a)
            deriving Show

fresh :: ST Int
fresh = S (\n -> (n, n+1))

rlabel :: Tree a -> ST (Tree Int)
rlabel (Leaf _)     n = (Leaf n, n+1)
rlabel (Node l r)   n = (Node l' r', n'')
                        where
                            (l', n') = rlabel l n
                            (r', n'') = rlabel r n'

