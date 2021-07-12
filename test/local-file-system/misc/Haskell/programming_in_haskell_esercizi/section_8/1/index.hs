data Nat = Zero | Succ Nat deriving (Show)

addNat :: Nat -> Nat -> Nat
addNat Zero n = n
addNat (Succ n) m = Succ (addNat n m)

multNat :: Nat -> Nat -> Nat
multNat Zero _ = Zero
multNat _ Zero = Zero
multNat (Succ n) (Succ m) = prod
  where prod = addNat (addNat (multNat n m) (Succ n)) m
