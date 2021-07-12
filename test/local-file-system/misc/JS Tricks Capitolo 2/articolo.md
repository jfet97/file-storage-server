# JS Tricks: Capitolo 2

Aggiunta condizionale veloce di proprietà ad oggetti ed array e conversioni veloci stringa <-> numero

&nbsp;

## Aggiunta opzionale di proprietà ad oggetti

Quante volte vi è capitato di dover aggiungere una proprietà ad un oggetto solo se una certa variabile booleana è `true`? Il modo comune di farlo è il seguente:

```js
const obj = {}

if(shouldBeAdded) {
    obj.prop = value;
}
```

Vi è però un modo molto più veloce da scrivere e facilmente estendibile, che vi illustriamo nel seguente snippet:

```js
const obj = {
    ...shouldBeAdded && { prop: value },
};
```

Nel caso in cui `shouldBeAdded` sia `true`, l'operatore `&&` ha come valore risultante `{ prop: value }` che verrà dato in pasto all'object spread. Altrimenti viene computato lo spread di `false` che non ha alcun effetto collaterale.

&nbsp;

## Aggiunta opzionale di proprietà ad array

Sulla falsariga di quanto detto sopra vi illustriamo il modo più veloce per aggiungere una proprietà numerica ad un array:

```js
const array = [
    ...shouldBeAdded ? [value] : [],
]
```

In questo caso l'array spread agirà su un array contenente il `value` oppure su un array vuoto, senza effetti collaterali, in base al valore di verità di `shouldBeAdded`.

&nbsp;

## Conversioni veloci stringa <-> numero

Come possiamo convertire velocemente una stringa in numero o viceversa senza dover passare dai costruttori `String` e `Number`?

### numero -> stringa

```js
const strFromN = n + "";
```

### stringa -> numero

```js
const nFromStr = +str;
```
