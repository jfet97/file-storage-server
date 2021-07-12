import Control.Applicative
import Data.Char


newtype Parser a = P (String -> [(a, String)])

parse :: Parser a -> String -> [(a, String)]
parse (P p) s = p s

instance Functor Parser where
    -- fmap :: (a -> b) -> Parser a -> Parser b
    fmap g pa = P (\s -> case parse pa s of
                            [] ->        []
                            [(a, s')] -> [(g a, s')])

instance Applicative Parser where
    -- pure :: a -> Parser a
    pure a = P (\s -> [(a, s)])

    -- <*> :: Parser (a -> b) -> Parser a -> Parser b
    pg <*> pa = P (\s -> case parse pg s of
                            [] ->        []
                            [(g, s')] -> parse (g <$> pa) s')

instance Monad Parser where
    return = pure

    -- (>>=) :: Parser a -> (a -> Parser b) -> Parser b
    pa >>= fapb = P (\s -> case parse pa s of
                            [] ->        []
                            [(a, s')] -> parse (fapb a) s')

instance Alternative Parser where
    -- empty :: Parser a
    empty = P (\s -> [])

    -- (<|>) :: Parser a -> Parser a -> Parser a
    pa <|> qa = P (\s -> case parse pa s of
                            [] ->           parse qa s
                            [(a, s')] ->    [(a, s')])



-- "legge" il primo elemento
item :: Parser Char
item = P (\s -> case s of
            [] ->       []
            (x:xs) ->   [(x, xs)])

-- genera parser che controllano la validità di un carattere tramite un predicato
sat :: (Char -> Bool) -> Parser Char
sat p = do  x <- item
            if p x then return x else empty

digit :: Parser Char
digit = sat isDigit

lower :: Parser Char
lower = sat isLower

upper :: Parser Char
upper = sat isUpper

letter :: Parser Char
letter = sat isAlpha

alphanum :: Parser Char
alphanum = sat isAlphaNum

-- crea un parser che riconosce solo il carattere x
char :: Char -> Parser Char
char x = sat (== x)

-- genera un parser che riconosce esattamente la stringa presa come input
string :: String -> Parser String
string [] = return []
string (x:xs) = do  char x
                    string xs
                    return (x:xs)

ident :: Parser String
ident = do  x <- lower
            xs <- many alphanum
            return (x:xs)

nat :: Parser Int
nat = do    xs <- some digit
            return (read xs)

space :: Parser ()
space = do  many (sat isSpace)
            return ()

int :: Parser Int
int = do    char '-'
            n <- nat
            return (-n)
        <|> nat

token :: Parser a -> Parser a
token p = do    space
                v <- p
                space
                return v

identifier :: Parser String
identifier = token ident

natural :: Parser Int
natural = token nat

integer :: Parser Int
integer = token int

symbol :: String -> Parser String
symbol xs = token (string xs)

nats :: Parser [Int]
nats = do   symbol "["
            n <- natural
            ns <- many (do  symbol ","
                            natural)
            symbol "]"
            return (n:ns)



expr :: Parser Int
expr = term >>= (\t -> (do  symbol "+"
                            e <- expr
                            return (t + e))
                        <|> return t)
term :: Parser Int
term = factor >>= (\f -> (do    symbol "*"
                                t <- term
                                return (f * t))
                        <|> return f)

factor :: Parser Int
factor = do symbol "("
            e <- expr
            symbol ")"
            return e
        <|> natural

-- parse expr input