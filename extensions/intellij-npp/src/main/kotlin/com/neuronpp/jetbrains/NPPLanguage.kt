package com.neuronpp.jetbrains

import com.intellij.lang.Language

object NPPLanguage : Language("NPP") {
    private fun readResolve(): Any = NPPLanguage
}
