# TS Tricks Capitolo 3

In questo episodio vedremo la differenza tra i tipi unknown ed any, come aggiungere metodi statici a delle classi già esistenti e come implementare il nominal typing.

## Unknown vs Any

Può succedere di non avere a disposizione delle informazioni corrette sui tipi in gioco, e spesso l'alternativa scelta è quella di utilizzare il tipo `any`. Ad ogni modo il tipo `any` nasconde delle insidie non da poco, poiché ogni operazione è concessa su una variabile di tipo `any` e quindi si rischia di ignorare errori potenzialmente fatali a runtime.\
Il tipo `unknown` è una new entry in TypeScript, descritta come l'alternativa safe ad `any`. Esattamente come per il suo fratello malvagio, un qualsiasi valore può essere assegnato ad una variabile di tipo `unknown`. La grossa differenza è che nessuna operazione ci è concessa, ovvero non potremo fare in alcun modo uso di tale variabile:

```ts
const u: unknown = { whatever : 42, log() { console.log("log") } };
u.whatever; // Error: Object is of type 'unknown'
u.log(); // Error: Object is of type 'unknown'

const o: { whatever : number, log: () => void } = u
// Error: Type 'unknown' is not assignable to type '{ whatever: number; log: () => void; }'
```

Dove sta il vantaggio di avere delle entità non direttamente utilizzabili? Il punto è che siamo costretti ad eseguire dei controlli prima di poterne fare uso:

```ts
type MyType = { whatever : number, log: () => void }

function isMyType(e: unknown): e is MyType {
    const cast_e = e as MyType

    if(typeof cast_e === "object" &&
       cast_e &&
       typeof cast_e.whatever === "number" &&
       typeof cast_e.log === "function") {
        return true
    } else {
        return false
    }
}

const u: unknown = { whatever : 42, log() { console.log("log") } };

if(isMyType(u)) {
    u.log()
}
```

Controlli che verranno eseguiti anche a runtime, garantendo una sicurezza maggiore.

&nbsp;

## Aggiungere metodi statici a classi esistenti

Che dire se volessimo aggiungere un metodo statico ad una classe già esistente, tipo la classe `Array`? Oltre ad aggiungere un nuovo metodo ad essa, dobbiamo anche avvisare TypeScript delle nostre intenzioni.

In sostanza dobbiamo dichiarare una "nuova" interfaccia per il costruttore che vogliamo estendere, in modo da poter aggiungere il tipo del nostro nuovo metodo:

```ts
declare interface ArrayConstructor {
    last<T>(a: Array<T>): T | undefined;
}

Array.last = (a) => a[a.length - 1];
```

TypeScript unirà automaticamente questa interfaccia con quella già in suo possesso, in modo tale che possiamo usare in sicurezza la versione tipizzata del metodo `last` appena aggiunto.

&nbsp;

## Nominal Typing

Ipotizziamo di avere a che fare con due tipi di identificativi diversi, uno per il personale di un'azienda e l'altro per i prodotti della stessa azienza. TypeScript ci permette di tipizzare entrambi come stringhe, nulla di più. Non ci impedisce quindi di invocare una funzione che richiede l'id di un dipendente con l'id di un prodotto:

```ts
type EmployeeID = string
type ProductID = string

function fire(id: EmployeeID) {
    // licenzia un dipendente
}

const product = "123456" // "123456" è una qualsiasi stringa :(

fire(product) // <-- :(
```

Questo perché il type system di TS è interessato alla struttura delle entità, non ai nomi che utilizziamo per indicare i tipi. In altre parole, il tipo `EmployeeID` è esattamente equivalente al tipo `ProductID` che è esattamente equivalente al tipo `string`.

Per implementare il nominal typing in TS possiamo fare così:

```ts
type EmployeeID = string & { readonly brand : unique symbol }
type ProductID = string & { readonly brand : unique symbol }
```

TypeScript non considererà più `EmployeeID` equivalente a `ProductID`. Ovviamente sarà invalidata anche l'equivalenza di default con il tipo `string`.\
Entità di tipo `EmployeeID` o `ProductID` non potranno più essere create direttamente: avremo bisogno di funzioni specifiche che eseguono un cast.
