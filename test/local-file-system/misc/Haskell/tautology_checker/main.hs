import qualified Data.Map as M
import qualified Data.Set as S

data Proposition = Const Bool
                 | Var Char
                 | Not Proposition
                 | And Proposition Proposition
                 | Or Proposition Proposition
                 | Imply Proposition Proposition
                 | DoubleImply Proposition Proposition

type Environment = M.Map Char Bool

eval :: Environment -> Proposition -> Bool
eval _ (Const b) = b
eval e (Var c) = M.lookup c e // serve un maybe o map con trucco visto oggi tipo non empty map
eval e (Not p) = not (eval e p)
eval e (And p q) = (eval e p) && (eval e q)
eval e (Or p q) = (eval e p) || (eval e q)
eval e (Imply p q) = (eval e p) <= (eval e q)
eval e (DoubleImply p q) = (eval e p) == (eval e q)

vars :: Proposition -> S.Set Char
vars (Const _) = S.empty
vars (Var c) = S.singleton c
vars (Not p) = vars p
vars (And p q) = S.union (vars p) (vars q)
vars (Or p q) = S.union (vars p) (vars q)
vars (Imply p q) = S.union (vars p) (vars q)
vars (DoubleImply p q) = S.union (vars p) (vars q)

sequences :: Int -> [[Bool]]
sequences 0 = [[]]
sequences n = map (False :) bss ++ map (True :) bss
  where bss = sequences (n - 1)

environments :: Proposition -> [Environment]
environments p = map (M.fromList . zip vs) $ sequences (length vs)
  where vs = S.toList $ vars p

isTautology :: Proposition -> Bool
isTautology p = and [eval e p | e <- environments p]
