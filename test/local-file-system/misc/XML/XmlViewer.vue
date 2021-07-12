<template>
  <div style="background-color: #1e1e1e">
    <XmlElement :node="rootNode" />
  </div>
</template>
<script lang="ts">
import XmlElement from "./XmlElement.vue"

import { defineComponent, ref, watch } from "vue"

export default defineComponent({
  components: {
    XmlElement,
  },
  props: {
    xml: String,
  },
  setup(props) {
    const parser = new DOMParser()
    const rootNode = ref()

    const parseDocument = (xml) => {
      const xmlDoc = parser.parseFromString(xml, "text/xml")
      const rootElement = [...xmlDoc.childNodes].find((c) => c.nodeType === 1)
      rootNode.value = rootElement
    }

    watch(
      () => props.xml,
      (value) => {
        parseDocument(value)
      }
    )

    parseDocument(props.xml)

    return {
      rootNode,
    }
  },
})
</script>
