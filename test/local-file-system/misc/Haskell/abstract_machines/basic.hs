data Expr = Val Int | Add Expr Expr

{-
value :: Expr -> Int
value (Val n) = n
value (Add e f) = value e + value f
-}

data Op = EVAL Expr | ADD Int
type Cont = [Op]

eval :: Expr -> Cont -> Int
-- se l'espressione è stata ridotta ad un singolo valore n
-- è completamente valutata e possiamo iniziare ad eseguire il control stack
-- valutando le espressioni sino a qui posticipate
-- sfruttando il valore n se serve
eval (Val n) c = exec c n
-- altrimenti diamo la priorità all'evaluation dell'espressione "sulla SX",
-- memorizzando l'espressione "sulla DX" nel control stack per
-- valutarla successivamente
eval (Add e f) c = eval e (EVAL f : c)

-- non è un caso che l'Op da eseguire venga messa in prima posizione: questo le
-- da maggior priorità rispetto alle precedenti, conseguenza dell'essere andati
-- più a fondo nell'espressione. Oltre a ciò otteniamo la sicurezza che il valore
-- di e venga poi utilizzato coerentemente assieme al futuro valore di y.
-- Eventuali successive espressioni innestate in e da valutare non faranno
-- erroneo uso di f poiché ad ogni espressione valutata corrisponderà
-- una rimandata con precedenza maggiore di f.
-- Eventuali successive espressioni innestate in f da valutare non faranno
-- erroneo uso del valore di e per lo stesso motivo.
-- Quell'ADD n qua sotto viene inserito in parallelo all'evaluation di f:
-- ad ogni EVAL f corrisponde un ADD n che segna il posto col il valore di e
-- ad ogni espressione dentro f valutata corrisponderà una rimandata con
-- precedenza maggiore di ADD n
-- L'ADD n aggiunto non da' problemi alle espressioni da valutare precedenti
-- perché si limita ad usare il valore di e e di f, rimuovendosi dal control stack,
-- prima che le precedenti vengano chiamate in causa
-- L'ADD n aggiunto non viene disturbato dalle espressioni future, poiché ad ogni
-- espressione valutata corrisponderà una rimandata



exec :: Cont -> Int -> Int
-- se il control stack è vuoto non vi è null'altro da valutare
-- il risultato è proprio l'n ricevuto
exec [] n = n
-- se nel control stack è presente un EVAL, significa che avevamo incontrato
-- un Add durante l'evaluation e che avevamo sospeso l'evaluation di f
-- in favore dell'espressione sulla sua SX che ha prodotto n.
-- dobbiamo perciò valutare f impostando però un'operazione di ADD con n
-- da eseguire non appena il valore di f è reso disponibile
exec (EVAL f : c) n = eval f (ADD n : c)
-- se nel control stack è presente un ADD, significa che n era il risultato di e,
-- messo in pausa, mentre m è l'agognato risultato di f appena valutato.
-- possiamo continuare l'esecuzione del control stack
-- fornendo alla precedente espressione sospesa il risultato di
-- quella che era alla sua SX: n+m.
-- LKa corrispondenza tra n e m è garantita dal ragionamento nel messaggione
exec (ADD n : c) m = exec c (n+m)

value :: Expr -> Int
value e = eval e []
