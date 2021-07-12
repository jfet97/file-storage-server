<script>
  import Button from "./Button.svelte";

  let message = "Hello World!";
  $: capitalizedMessage = message.toUpperCase();
  $: greaterThan5 = message.length > 5;

  function clickHandler() {
    console.log("The button has been clicked");
  }

  let people = [
    { id: "3TauxZZGyEg", name: "Piero Salvini" },
    { id: "wuRNsybk8To", name: "Adriano Celentano" },
    { id: "0OuxhV5Kz8Y", name: "Giovanni Morandi" },
  ];
</script>

<main>
  <h1>{message}</h1>
  <h2>{capitalizedMessage}</h2>
  <input bind:value={message} />

  {#if greaterThan5}
    <Button text={message} on:click={clickHandler} />
  {/if}

  {#each people as person}
    <li>
      <a target="_blank" href="https://www.youtube.com/watch?v={person.id}">
        {person.name}
      </a>
    </li>
  {/each}
</main>
