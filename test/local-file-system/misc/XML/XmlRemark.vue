<template>
    <div>
        <div class="content">
            &lt;!-- <pre class="inline no-margin-padding">{{content}}</pre> --&gt;
        </div>
    </div>
</template>
<script lang="ts">
import {defineComponent, computed} from 'vue'

export default defineComponent({
    props: {
        remark: Comment
    },
    setup(props) {
        const content = computed(() => props.remark.textContent)

        return {
            content
        }
    }
})
</script>
<style lang="scss" scoped>
@import "scss/_variables.scss";

.content {
    color: $remarkContentColor;
}
.inline {
    display: inline;
}
.no-margin-padding {
    margin: 0;
    padding: 0;
}
</style>