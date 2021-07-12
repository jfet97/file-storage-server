module Main

average: (str: String) -> Double
average str = let ws = words str
                  numWs = wordsCounter ws
                  totalLen = sum (allLenghts ws) in
                  cast totalLen / cast numWs
  where
    wordsCounter : List String -> Nat
    wordsCounter ws = length ws

    allLenghts : List String -> List Nat
    allLenghts ws = map length ws

showAverage : String -> String
showAverage str = "The average word lenght is: " ++
                  show (average str) ++
                  "\n"

main : IO()
main = repl "Enter a string: " showAverage
