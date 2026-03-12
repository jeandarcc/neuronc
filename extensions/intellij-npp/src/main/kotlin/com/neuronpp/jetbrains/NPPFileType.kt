package com.neuronpp.jetbrains

import com.intellij.openapi.fileTypes.LanguageFileType
import javax.swing.Icon

object NPPFileType : LanguageFileType(NPPLanguage) {
    override fun getName(): String = "Neuron++"
    override fun getDescription(): String = "Neuron++ source file"
    override fun getDefaultExtension(): String = "npp"
    override fun getIcon(): Icon? = null // TODO: Add an icon later
}
