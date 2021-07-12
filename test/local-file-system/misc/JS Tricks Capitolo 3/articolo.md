# JS Tricks: Capitolo 3

Check preciso di valori nulli, troncamento veloce di array e come ottenere l'ennesimo elemento di un array partendo dal fondo

&nbsp;

## Check preciso di valori nulli

Nel caso in cui fossimo interessati a controllare se una determinata variabile o una proprietà di un oggetto è `null` oppure `undefined` possiamo fare buon uso della coercizione e limitarci a scrivere:

```js
if(something == null) {
    //...
}
```

oppure:

```js
if(something == undefined) {
    //...
}
```

Questi due snippet sono perfettamente equivalenti e corrispondono al 100% al più verboso doppio check:

```js
if(something === null || something === undefined) {
    //...
}
```

&nbsp;

Potrebbe poi accadere che vogliamo eseguire un controllo simile per poter utilizzare, se fosse necessario, un valore di default. ES2020 ci mette a disposizione l'operatore `??`:

```js
const alwaysDefined = something ?? "unknown";
```

Questo operatore restituisce il valore di default `"unknown"` se e solo se `something` è `null` oppure `undefined`. Questa è una marcata differenza rispetto all'operatore `||` che utilizzerebbe il valore di default anche nel caso in cui `something` fosse una stringa vuota.

&nbsp;

## Troncamento veloce di array

Il modo più rapido per diminuire, o addirittura azzerare, gli elementi contenuti in un array è quello di modificare manualmente la proprietà `length`:

```js
const array = [0, 1, 2, 3, 4, 5, 6, 7, 8, 9];
console.log(array); // print [0, 1, 2, 3, 4, 5, 6, 7, 8, 9]

array.length = 4;
console.log(array); // print [0, 1, 2, 3]

array.length = 0;
console.log(array); // print []
```

&nbsp;

## Ottenere l'ennesimo elemento di un array partendo dal fondo

Per fare ciò è molto comodo il metodo `slice` poiché come indice di partenza può essere utilizzato un numero negativo, al quale corrisponde un dato elemento partendo dalla fine dell'array. Ecco che per ottenere, ad esempio, il terzultimo elemento possiamo scrivere:

```js
const el = array.slice(-3)[0];
```

Nel caso in cui l'array avesse meno di tre elementi, il valore di questa espressione sarebbe `undefined`.
