<template>
    <span>
        <span class="attribute-value">{{value}}</span>
    </span>
</template>
<script lang="ts">
import {defineComponent} from 'vue'

export default defineComponent({
    props: {
        value: String
    }
})
</script>
<style lang="scss" scoped>
@import "scss/_variables.scss";
.attribute-value {
    color: $attributeValueColor;
    padding: 0;
    margin: 0;
}
</style>