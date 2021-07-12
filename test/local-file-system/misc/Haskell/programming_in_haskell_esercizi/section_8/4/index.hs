module Albero (Tree (..)) where

data Tree a = Leaf a | Node (Tree a) (Tree a) deriving (Show)

balanced :: Tree a -> (Int, Bool)
balanced (Leaf a) = (1, True)
balanced (Node l r) =
  let (ln, lb) = balanced l
      (rn, rb) = balanced r
      in
        if (not (lb && rb)) then (-1, False)
        else ((ln+rn),  abs (ln - rn) < 2)

balance :: [a] -> Tree a
balance [] = error "Cannot extract elements from an empty list"
balance (a:[]) = Leaf a
balance l = Node (balance fh) (balance sh)
  where cx = (length l) `div` 2
        fh = take cx l
        sh = drop cx l
