module Lib
    ( Proposition,
      isTautology
    ) where

import Data.Map.Justified as M
import qualified Data.Set as S

data Proposition = Const Bool
                 | Var Char
                 | Not Proposition
                 | And Proposition Proposition
                 | Or Proposition Proposition
                 | Imply Proposition Proposition
                 | DoubleImply Proposition Proposition

type Environment = M.Map Char Bool

-- case M.member c e of
--     Nothing  ->
--     Just key -> key

-- mi serve un data qlcs del genere che renda la proposition una "TrustedProposition"
-- dove siam sicuri che le variabili sono presenti nell'enviroment, solo in quell'environment
-- la trusted proposition dovrebbe contenere solo Var c con tipo l'M.map (l'enviroment) di turno
-- Altrimenti sarei costretto a restituire Maybe Bool cmq nel caso in cui la prova che la chiave è
-- nell'environment dovesse fallire
-- Infatti potrei impostare un environment prima e una proposition a casissimo dopo
--
-- Voglio fare in modo di non poter chiamare la partial function con una proposition qualsiasi
-- ma con una sicuramente contenente i caratteri nell'environment

data TrustedProposition a = a :| [a]

eval :: Environment -> Proposition -> Bool
eval _ (Const b) = b
eval e (Var c) = M.lookup c e
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

