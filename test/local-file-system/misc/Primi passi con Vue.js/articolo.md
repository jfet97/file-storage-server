# Primi passi con Vue.js

I framework front-end hanno il merito di aver notevolmente semplificato la vita allo sviluppatore web facendosi carico della gestione del DOM, offrendo un meccanismo per la gestione della reattività out-of-the-box e favorendo una suddivisione delle applicazioni in componenti.

In questo articolo muoveremo i nostri primi passi nello sviluppo tramite il framework Vue.js. Sarà sufficiente un file `.html` nel quale importeremo il runtime nel seguente modo:

```html
<!DOCTYPE html>
<html lang="en">
<head>
    <meta charset="UTF-8">
    <meta name="viewport" content="width=device-width, initial-scale=1.0">
    <title>Primi passi con Vue.js</title>

    <script src="https://cdn.jsdelivr.net/npm/vue@2.6.12"></script>
</head>
<body>

</body>
</html>
```

&nbsp;

## Collegare il framework

Per poter far interagire Vue con la nostra pagina web, è necessario creare un `div` al quale imposteremo come `id` l'id `app`:

```html
<body>
    <div id="app"></div>
</body>
```

Adesso possiamo aggiungere dopo il tag `body` un tag `script` con il seguente contenuto:

```html
</body>

<script>
var app = new Vue({
  el: '#app',
  data: {}
})
</script>
```

Vue verrà istanziato e prenderà il controllo del `div#app` agendo al suo interno, ovvero modificando il DOM in base a ciò che decideremo di fare.

&nbsp;

## Interpolazione

All'interno del campo `data` potremo andare ad inserire ogni tipo di dato valido in JavaScript: array, oggetti, numeri, stringhe, ecc. Potremmo poi farne uso all'interno dell'`html` tramite l'interpolazione.

Ipotizziamo di avere il seguente messaggio:

```html
<script>
var app = new Vue({
  el: '#app',
  data: {
      message: "Primi passi con Vue.js"
  }
})
</script>
```

Allora potremmo farne uso all'interno del `div#app` come contenuto di un heading `h2` nel seguente modo:

```html
<body>
    <div id="app">
        <h2>{{ message }}</h2>
    </div>
</body>
```

Vue è in grado di riconoscere questa particolare sintassi e sostituirà l'intera stringa `{{ message }}` con la stringa `Primi passi con Vue.js`.

&nbsp;

## Reattività ed eventi

La semplice applicazione che andremo a costruire sarà costituita da un `input` nel quale potremo scrivere del testo che sarà utilizzato come contenuto per un blocco di tag `hX`, ovvero gli heading.

Iniziamo scrivendo l'`html` necessario:

```html
<body>
    <div id="app">
        <input type="text"/>
        <br/><br/>

        <h1>{{ message }}</h1>
        <h2>{{ message }}</h2>
        <h3>{{ message }}</h3>
        <h4>{{ message }}</h4>
        <h5>{{ message }}</h5>
        <h6>{{ message }}</h6>
    </div>
</body>
```

Notiamo subito che è possibile far uso della medesima variabile `message` in più punti all'interno dell'`html`.

Adesso vogliamo che anche il contenuto del tag `input` sia esattamente il `message`, ma non solo! Il nostro scopo è quello di permettere al tag `input` di modificare la variabile d'istanza `message`. Il sistema di reattività del framework si occuperà di aggiornare il DOM di conseguenza, aggiornando il contenuto dei tag di heading.

Vediamo in che modo possiamo raggiungere i nostri goal. Per quanto riguarda l'allineamento del contenuto del tag `input` con il `message` abbiamo bisogno di un particolare modificatore proprietario di Vue (in gergo direttiva):

```html
<input type="text" v-bind:value="message"/>
```

Possiamo immaginare questa direttiva come l'equivalente della sintassi `{{ }}` per gli attributi degli elementi `html`.

Purtroppo ciò non è sufficiente per fare in modo che il testo successivamente inserito si rifletta nella variabile `message`. Poiché l'inserimento di testo in un tag `input` scatena l'emissione di un evento `input` del DOM, Vue ci mette a disposizione la direttiva `v-on`per poter reagire a tali eventi:

```html
<input
    type="text"
    v-bind:value="message"

    v-on:input="(event) => message = event.target.value"
/>
```

Abbiamo impostato una semplice callback che verrà automaticamente invocata ogni volta che verrà generato un evento `input` dal tag, nella quale aggiorniamo il contenuto della variabile `message` con ciò che è stato effettivamente scritto all'interno dell'`input`. Ricordiamo che sarà Vue poi ad occuparsi dell'aggiornamento del DOM e quindi del contenuto degli heading come conseguenza del cambiamento di `message`.

&nbsp;

## Rendering condizionale

Nel caso in cui la presenza nel DOM di un certo elemento fosse condizionata dallo stato di verità di una determinata variabile, possiamo fare uso della direttiva `v-if`.

Ipotizziamo di voler far comparire un messaggio se e solo se una checkbox viene selezionata. Avremo quindi bisogno di una ulteriore variabile d'istanza:

```html
<script>
var app = new Vue({
  el: '#app',
  data: {
      message: "Primi passi con Vue.js",
      isChecked: false
  }
})
</script>
```

Invece, l'`html` di partenza sarà il seguente:

```html
<body>
    <div id="app">
        <input type="checkbox"/>
        <br/><br/>

        <h1>{{ message }}</h1>
    </div>
</body>
```

Dobbiamo ora collegare la checkbox al sistema di reattività di Vue in modo simile a quanto abbiamo fatto prima:

```html
<input
    type="checkbox"
    v-bind:checked="isChecked"

    v-on:change="(event) => isChecked = event.target.value"
/>
```

Le differenze consistono nel diverso attributo collegato, `checked` anziché `value`, e nel diverso evento ascoltato, `change` anziché `input`. Ovviamente anche la variabile d'istanza in gioco cambia, essendo essa `isChecked`.

Facciamo ora uso della direttiva `v-if` per nascondere il tag `h1` quando la checkbox non è selezionata e quindi `isChecked` è `false`:

```html
<h1 v-if="isChecked">{{ message }}</h1>
```

Niente di più semplice!

&nbsp;

## Rendering iterativo

Come fare nel caso in cui abbiamo una collezione di dati, di lunghezza variabile, e volessimo generare dinamicamente più volte il medesimo elemento `html`? In casi come questo entra in gioco la direttiva `v-for`.

Ipotizziamo di avere il seguente array di nomi:

```html
<script>
var app = new Vue({
  el: '#app',
  data: {
      names: ["Piero", "Angelo", "Martina"]
  }
})
</script>
```

Vogliamo generare un `p` per ogni nome, `p` che conterrà ovviamente il nome stesso. Ecco come fare:

```html
<body>
    <div id="app">

        <span v-for="(name, i) in names" :key="i">{{ name }}</span>

    </div>
</body>
```

Vue itererà la collezione di `names` dandoci la possibilità di utilizzare come meglio preferiamo ciascun `name`. Come da specifiche, ci limitiamo ad impostarlo come contenuto dell'i-esimo `span`.

L'indice `i` è fornito automaticamente da Vue ed è importante che venga utilizzato, se non si dispone di alternative, come valore `key` univoco per ogni `span`. Senza entrare in dettagli tecnici, questo è un piccolo prezzo da pagare ogni volta che vogliamo utilizzare la direttiva `v-for`.
