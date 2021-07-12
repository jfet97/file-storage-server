# CSS tricks capitolo 2

In questa seconda puntata vedremo come utilizzare la proprietà `position: sticky;`, come stilizzare elementi di input required e come far scorrere del testo attorno ad una immagine tramite le CSS Shapes

&nbsp;

## position: sticky;

Questa proprietà fa si che un dato elemento html, dopo essere comparso nella view, non scrolli mai fuori da essa. In altre parole "si attacca" in un dato punto della viewport e permane indefinitamente.

```css
#sticky {
  position: sticky;
  width: 100px;
  height: 100px;
}
```

Essi si comportano come componenti `relative` prima di entrare in gioco, per poi trasformarsi in `fixed` quando vengono visualizzati.

&nbsp;

## La pseudo-classe required

Quando abbiamo a che fare con un form molto probabilmente ci sono alcuni campi che dovranno essere obbligatoriamente riempiti dall'utente. Possiamo stilizzarli tramite la pseudo-classe `required`:

```css
:required {
   border: 1px solid pink;
}
```

&nbsp;

## CSS Shapes

Per poter far scorrere del testo attorno ad una immagine in modo tale da far risultare la visualizzazione simile a quella che è possibile trovare in alcuni libri di vecchia data possiamo fare uso delle proprietà `shape-outside` e `shape-margin`.

### shape-outside

La proprietà `shape-outside` aiuta a definire una forma a piacere, attorno alla quale verrà disposto il contenuto adiacente. Ecco alcuni esempi:

```css
#shape {
    shape-outside: circle(50%);
}
```

```css
#shape {
    shape-outside: ellipse(100px 150px at 10% 10%);
}
```

```css
#shape {
    /* la nostra immagine */
    shape-outside: url(/images/rabbit.png);
}
```

```css
#shape {
    shape-outside: polygon(50% 0, 100% 50%, 50% 100%, 0 50%);
}
```

### shape-margin

La proprietà `shape-margin` imposta un margine ad una shape creata tramite `shape-outside`. Nel seguente esempio impostiamo un margine di 20 pixel:

```css
#shape {
    shape-outside: url(/images/rabbit.png);
    shape-margin: 20px;
}
```
