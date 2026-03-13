package com.Neuron.jetbrains

import com.intellij.openapi.fileTypes.LanguageFileType
import javax.swing.Icon

object NeuronFileType : LanguageFileType(NeuronLanguage) {
    override fun getName(): String = "Neuron"
    override fun getDescription(): String = "Neuron source file"
    override fun getDefaultExtension(): String = "nr"
    override fun getIcon(): Icon? = null // TODO: Add an icon later
}
