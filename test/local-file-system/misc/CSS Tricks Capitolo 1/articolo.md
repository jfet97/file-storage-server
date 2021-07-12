# CSS tricks capitolo 1

In questo primo capitolo ci concentreremo esclusivamente sul box-sizing, questa misteriosa regola CSS che ha un grande impatto nella renderizzazione dei nostri elementi HTML.

&nbsp;

## Il Box Model

Quando un browser deve renderizzare i vari elementi HTML, ogni elemento viene trasformato in un rettangolo, chiamato box, seguendo lo standard del CSS. Questo standard specifica quattro proprietà essenziali: contenuto, padding, bordo e margine.

PIERO METTI L'IMMAGINE DEL BOX MODEL ALLEGATA :)

Il contenuto, come indica il termine, rappresenta ciò che viene inserito all'interno dell'elemento: testo, immagini, video, altri elementi e così via.
Il padding rappresenta quello spazio opzionale tra il contenuto e i bordi di un elemento, i quali possono avere uno spessore. Il margine è lo spazio opzionale tra i bordi e altri elementi esterni a quello che stiamo considerando.

&nbsp;

## La proprietà box-sizing

Di default, nel box model del CSS, l'altezza e la larghezza che assegniamo ad un elemento si applicano solo al suo contenuto. Se l'elemento avesse del bordo o del padding, essi verrebbero aggiunti all'altezza e alla larghezza impostate per produrre la renderizzazione mostrata a video. Questo può risultare scomodo perché dobbiamo tenere conto della possibilità che del margine e del padding vengano aggiunti ai nostri elementi quando ne decidiamo l'altezza e la larghezza.

La proprietà `box-sizing` ci permette di acquistare un maggior controllo sulla situazione:

1. `content-box` è il valore di default che ha come conseguenza quanto appena detto
2. `border-box` impone al browser di considerare eventuali bordi e padding aggiunti come facenti parte dell'altezza e della larghezza degli elementi, rendendoci la vita molto più facile quando vogliamo impostarne le dimensioni.

Risulta quindi molto utile impostare `box-sizing: border-box;` a tutti gli elementi che hanno il compito di modellare un layout. Ciò semplifica notevolmente la gestione delle dimensioni dei sottoelementi, oltre ad eliminare tutta una serie di problematiche minori di non facile risoluzione.

D'altra parte, quando si ha necessità di impostare `position: relative;` o `position: absolute`, la regola `box-sizing: content-box;` consente di impostare dei valori di posizionamento indipendenti dalle modifiche alle dimensioni del bordo e del padding.
