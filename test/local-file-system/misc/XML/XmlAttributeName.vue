<template>
    <span>
        <span class="attribute-name">{{name}}</span>
    </span>
</template>
<script lang="ts">
import {defineComponent} from 'vue'

export default defineComponent({
    props: {
        name: String
    }
})
</script>
<style lang="scss" scoped>
@import "scss/_variables.scss";
.attribute-name {
    color: $attributeNameColor;
}
</style>