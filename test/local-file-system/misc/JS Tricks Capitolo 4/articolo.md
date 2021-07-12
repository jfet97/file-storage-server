# JS Tricks: Capitolo 4

Filtraggio di falsy values, destrutturazione con alias e check di oggetti e array.

&nbsp;

## Filtraggio di falsy values

Per tutte quelle volte dove abbiamo bisogno di filtrare via i falsy values (`0`, `undefined`, `null`, `false`, `NaN`) da un array possiamo semplicemente fare così:

```js
const filteredArray = array.filter(Boolean);
```

Il costruttore `Boolean` infatti trasformerà i falsy values in `false` e la `filter` farà il resto.

&nbsp;

## Destrutturazione e alias

La destrutturazione ci permette di riasumere un codice come il seguente:

```js
const name = person.name;
const address = person.address;
```

In un più comodo:

```js
const { name, address } = person;
```

La cosa interessante sta nel fatto che possiamo rinominare le proprietà estratte per evitare eventuali conflitti con quelle già presenti nello scope:

```js
const { name: personName, address: personAddress } = person;
```

&nbsp;

## È un oggetto?

Per essere sicuri che una qualche variabile contenga un oggetto non è sufficiente il controllo con l'operatore `typeof` perché, per un noto bug, anche `typeof null` è `"object"`. Il modo corretto è il seguente:

```js
function isObject(v) {
    return v !== null && typeof v === "object";
}
```

Un discorso a parte lo meritano le funzioni perché tecnicamente sarebbero oggetti anche esse, ma raramente è necessario considerarle tali. Ad ogni modo, in caso di necessità, possiamo modificare leggermente `isObject` per riconoscere anche le funzioni:

```js
function isObject(v) {
    return v !== null && (typeof v === "object" || typeof v === "function");
    }
```

&nbsp;

## È un array?

Che dire se invece vogliamo essere sicuri che una variabile sia un array? Ecco come fare:

```js
function isArray(v) {
    return Array.isArray(v);
}
```
