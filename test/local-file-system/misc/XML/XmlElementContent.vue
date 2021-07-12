<template>
    <div>
        <div class="content">
            {{content}}
        </div>
    </div>
</template>
<script lang="ts">
import {defineComponent, computed} from 'vue'

export default defineComponent({
    props: {
        node: Text
    },
    setup(props) {
        const content = computed(() => props.node.textContent)

        return {
            content
        }
    }
})
</script>
<style lang="scss" scoped>
@import "scss/_variables.scss";

.content {
    color: $elementContentColor;
}
</style>