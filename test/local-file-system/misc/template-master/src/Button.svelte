<script>
    import { createEventDispatcher } from 'svelte';

    const dispatch = createEventDispatcher();

    function emitClick() {
        dispatch('click')
    }

    export let text;
</script>

<main>
 <button on:click={emitClick}>{text}</button>
</main>