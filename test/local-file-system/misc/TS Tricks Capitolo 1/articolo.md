# TypeScript tricks capitolo 1

Vediamo oggi alcune utility a livello dei tipi che ci permettono di maneggiarne uno per ottenere i più svariati risultati.
Il preset dell'interpo articolo è il seguente tipo che rappresenta una persona:

```ts
type Person = {
    name: string,
    age: number,
    work: {
        kind: string,
        RAL: number
    }
}
```

## Partial

La prima utility, `Partial`, ci permette di rendere tutti i field al primo livello di un oggetto opzionali:

```ts
type PartialPerson = Partial<Person>

// PartialPerson
{
    name?: string,
    age?: number,
    work?: {
        kind: string,
        RAL: number
    }
}
```

## Required

Questa utility esegue esattamente l'operazione inversa della precedente:

```ts
type Person = Required<PartialPerson>

// Person
{
    name: string,
    age: number,
    work: {
        kind: string,
        RAL: number
    }
}
```

## Readonly

Questa utility ci permette di rendere tutti i field al primo livello di un oggetto readonly:

```ts
type ReadonlyPerson = Readonly<Person>

// ReadonlyPerson
{
    readonly name: string,
    readonly age: number,
    readonly work: {
        kind: string,
        RAL: number
    }
}
```

## Pick

Questa utility ci permette di estrarre un sottoinsieme delle proprietà al primo livello di un oggetto:

```ts
type PersonSubset = Pick<Person, "name" | "work">

// PersonSubset
{
    name: string,
    work: {
        kind: string,
        RAL: number
    }
}
```

## Omit

Questa utility ci permette di eslcudere un sottoinsieme delle proprietà al primo livello di un oggetto:

```ts
type PersonSubset = Omit<Person, "name" | "work">

// PersonSubset
{
    age: number
}
```

## Bonus: keyof

Questo è un operatore particolare messo a disposizione solo da TypeScript perché opera a livello dei tipi, il quale permette di ricavare una union di tutte le proprietà al primo livello di un oggetto:

```ts
type PersonKeyOf = keyof Person

// PersonKeyOf
"name" | "age" | "work"
```
