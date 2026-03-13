package com.Neuron.jetbrains

import com.intellij.lang.Language

object NeuronLanguage : Language("Neuron") {
    private fun readResolve(): Any = NeuronLanguage
}
