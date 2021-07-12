# CSS tricks capitolo 3

In questo appuntamento esamineremo gli aspetti principali del layout flexbox.

&nbsp;

## Flexbox

Lo scopo del layout Flexbox (Flexible Box) è quello di rendere il posizionamento degli elementi html più semplice, permettendoci di allinearli e distribuirli all'interno di un container anche quando la loro dimensione è dinamica.
L'idea si basa sul fornire al container la possibilità di ridimensionarsi in base alle necessità, lasciando agli elementi figli la decisione di come suddividere lo spazio a loro disposizione. Questi elementi potranno espandersi o comprimersi.

&nbsp;

## display: flex;

Questa proprietà definisce quale è il container in gioco. Crea un "contesto flex" per i suoi figli diretti:

```css
.container {
  display: flex;
}
```

&nbsp;

## flex-direction

Un concetto molto importante in questo layout è il cosiddetto asse principale. Questa proprietà ci permette di impostarlo:

```css
.container {
  flex-direction: row | row-reverse | column | column-reverse;
}
```

I quattro valori corrispondono a:

1. ``row`` = da sx a dx
2. ``row-reverse`` = da dx a sx
3. ``column`` = dall'alto al basso
4. ``column-reverse`` = dal basso verso l'alto

Gli item si disporranno lungo questo asse.

&nbsp;

## flex-wrap

Di default gli item tenteranno di disporsi tutti lungo una sola linea/colonna, contraendosi mano mano, ma possiamo fare in modo che finito lo spazio se ne crei un'altra:

```css
.container {
  flex-wrap: wrap;
}
```

&nbsp;

## justify-content

Ci permette di impostare l'allineamento degli elementi lungo l'asse principale:

```css
.container {
  justify-content: flex-start | flex-end | center | space-between | space-around | space-evenly;
}
```

Abbiamo che:

1. `flex-start` = gli item saranno posizionati uno accanto all'altro a partire dall'inizio dell'asse principale
2. `flex-end` = gli item saranno posizionati uno accanto all'altro a partire dalla fine dell'asse principale
3. `center` = gli item saranno posizionati uno accanto all'altro a partire dal centro dell'asse principale
4. `space-between` = gli item vengono distribuiti in modo uniforme; il primo all'inizio, il secondo alla fine, il terzo al centro...
5. `space-around` = lo spazio viene suddiviso egualmente per ogni item
6. `space-evenly` = gli item sono disposti in modo tale che lo spazio tra essi e dai due bordi sia costante

&nbsp;

## align items

Ci permette di impostare l'allineamento degli elementi lungo l'asse secondario. L'asse secondario è sempre perpendicolare a quello principale:

```css
.container {
  align-items: stretch | flex-start | flex-end | center | baseline;
}
```

1. `stretch` = gli item si espanderanno per coprire l'intero container
2. `flex-start` = gli item saranno posizionati all'inizio dell'asse secondario
3. `flex-end` = gli item saranno posizionati alla fine dell'asse secondario
4. `center` = gli items saranno posizionati al centro dell'asse secondario
5. `baseline` = gli item vengono allineati in modo che lo siano le loro baseline
