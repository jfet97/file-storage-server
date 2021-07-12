doubleMe x = x + x

doubleSmallNumbers n = if n > 100
                        then n
                        else n*2

list = [
    {-
        The single element: a triple containing the values
        of the ipothenuse and the two cathets
    -}
    (cat1, cat2, ipo) |

    -- Three ranges for three variables
    ipo <- [1..],
    cat1 <- [1..ipo-1],
    cat2 <-[1..cat1],

    -- Two predicates to satisfy
    ipo^2 == cat1^2 + cat2^2,
    mod (ipo + cat1 + cat2) 2 == 0 ]