aand:: [Bool] -> Bool
aand [] = True
aand (x:xs) = x && aand xs

bao n = n + 1
fiu n = n * 2
faz = bao . fiu

faz 3 = 3*2 + 1



filterM (\x -> [True, False]) [1,2,3]
-- [[1,2,3],[1,2],[1,3],[2,3],[1],[2],[3],[]]
