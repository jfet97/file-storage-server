# TypeScript tricks capitolo 2

In questo articolo introdurremo i generic e come utilizzarli efficacemente per tipizzare una semplice versione della funzione `map`.

## I generic

Possiamo vedere i generic come delle variabili, ma al livello dei tipi piuttosto che al livello dei valori. Essi sono molto utili quando abbiamo a che fare con delle collection e/o delle funzioni generiche le quali non sono più di tanto interessate ai tipi delle entità con le quali hanno a che fare. Ad esempio , se dovessimo trovarci nella situazione di dover creare una classe `Array`, non saremmo troppo interessati al tipo degli oggetti che andremo a contenere: tutte le operazioni come `push`, `pull`, `splice`, `concat`, `map`, `filter`, `reduce` funzioneranno indipendentemente dal tipo.\
D'altra parte non sarebbe corretto utilizzare un tipo generico come `unknown` oppure `any` nella costruzione di queste funzioni, perché così facendo perderemmo completamente l'informazione sul tipo attuale quando effettivamente andremo ad utilizzarle.

La sintassi per dichiarare un tipo parametrico, nel caso delle funzioni, è la seguente:

```ts
function identity<T>(arg: T): T {
  return arg;
}
```

Questa semplice funzione che si limita a restituire l'argomento ricevuto dichiara un tipo parametrico `T` che utilizza sia come tipo per il parametro formale `arg` che come tipo di ritorno.\
Ecco che nel seguente snippet di codice TypeScript sarà in grado di inferire correttamente i tipi:

```ts
let str = identity("extelos");
let num = identity(42);
```

Nel primo caso il tipo parametrico `T` prendera come "valore" il tipo `string`, nel secondo caso il tipo `number`. In questo modo `str` sarà correttamente considerata di tipo `string`, mentre `num` sarà `number`.

&nbsp;

## map

La funzione `map` prende un array `a` e una funzione di proiezione `p` e trasforma ogni elemento dell'array `a` dandolo in pasto alla funzione `p`, per poi collezionare il risultato in un nuovo array che restituirà.\
Il modo corretto di tipizzarla è il seguente:

```ts
function map<T, U>(a: Array<T>, p: (el:T) => U): Array<U> {
  const array_of_U = [];

  for(let i = 0; i < a.length; i++) {
      array_of_U.push(p(a[i]));
  }

  return array_of_U;
}
```

Utilizziamo due generic, `T` ed `U`, per i tipi parametrici degli elementi che costituiscono l'array prima e dopo la trasformazione. Ovviamente la funzione di mapping `p` sarà una funzione avente come dominio `T` e come codominio `U`.
