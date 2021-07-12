<template>
    <span>
        <span v-if="closing" class="bracket">/</span>
        <span @click="onClick" class="element-name">{{name}}</span>
    </span>
</template>
<script lang="ts">
import {defineComponent} from 'vue'

export default defineComponent({
    props: {
        name: String,
        closing: Boolean
    },
    emits: ['click'],
    setup(props, {emit}) {
        const onClick = () => {
            emit('click')
        }

        return {
            onClick
        }
    }
})
</script>
<style lang="scss" scoped>
@import "scss/_variables.scss";

.bracket {
    color: $bracketColor;
}
.element-name {
    color: $elementNameColor;
}
</style>