<template>
    <span>
        <XmlAttributeName :name="name" class="margin-left-sm" />
        <span class="no-margin-padding assignment">=</span>
        <span class="no-margin-padding quote">"</span>
        <XmlAttributeValue :value="value" class="no-margin-padding" />
        <span class="no-margin-padding quote">"</span>
    </span>
</template>
<script lang="ts">
import XmlAttributeName from './XmlAttributeName.vue'
import XmlAttributeValue from './XmlAttributeValue.vue'

import {defineComponent, computed} from 'vue'

export default defineComponent({
    name: 'XmlElement',
    components: {
        XmlAttributeName,
        XmlAttributeValue
    },
    props: {
        attribute: Attr
    },
    setup(props) {
        const name = computed(() => props.attribute.name)
        const value = computed(() => props.attribute.value)

        return {
            name,
            value
        }
    }
})
</script>

<style lang="scss" scoped>
@import "scss/_variables.scss";
.no-margin-padding {
    margin: 0;
    padding: 0;
}
.margin-left-sm {
    margin-left: 1em;
}
.assignment {
    color: $assignmentColor;
}
.quote {
    color: $quoteColor;
}
</style>