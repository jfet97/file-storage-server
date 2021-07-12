<template>
    <div>
        <div class="content">
            <span>&lt;![CDATA[</span><pre class="inline no-margin-padding">{{content}}</pre><span>]]&gt;</span>
        </div>
    </div>
</template>
<script lang="ts">
import {defineComponent, computed} from 'vue'

export default defineComponent({
    props: {
        cdataSection: Text
    },
    setup(props) {
        const content = computed(() => props.cdataSection.textContent)

        return {
            content
        }
    }
})
</script>
<style lang="scss" scoped>
@import "scss/_variables.scss";
.content {
    color: $cdataContentColor;
}
.inline {
    display: inline;
}
.no-margin-padding {
    margin: 0;
    padding: 0;
}
</style>