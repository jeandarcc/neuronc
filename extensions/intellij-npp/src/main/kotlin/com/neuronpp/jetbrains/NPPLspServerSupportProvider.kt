package com.neuronpp.jetbrains

import com.intellij.execution.configurations.GeneralCommandLine
import com.intellij.openapi.project.Project
import com.intellij.openapi.vfs.VirtualFile
import com.intellij.platform.lsp.api.LspServerSupportProvider
import com.intellij.platform.lsp.api.ProjectWideLspServerDescriptor
import java.io.File

class NPPLspServerSupportProvider : LspServerSupportProvider {
    override fun fileOpened(project: Project, file: VirtualFile, serverStarter: LspServerSupportProvider.LspServerStarter) {
        if (file.fileType == NPPFileType) {
            val executable = resolveLspExecutable(project)
            if (executable != null) {
                serverStarter.ensureServerStarted(NPPLspServerDescriptor(project, executable))
            }
        }
    }

    private fun resolveLspExecutable(project: Project): String? {
        // Simple resolution logic: check common build directories or PATH
        val projectRoot = project.basePath ?: return null
        val candidates = listOf(
            File(projectRoot, "build/bin/neuron-lsp.exe"),
            File(projectRoot, "build/bin/neuron-lsp"),
            File(projectRoot, "build-mingw/bin/neuron-lsp.exe"),
            File(projectRoot, "build-mingw/bin/neuron-lsp")
        )
        for (candidate in candidates) {
            if (candidate.exists()) return candidate.absolutePath
        }
        return "neuron-lsp" // Fallback to PATH
    }
}

class NPPLspServerDescriptor(project: Project, private val executablePath: String) : ProjectWideLspServerDescriptor(project, "Neuron++") {
    override fun isSupportedFile(file: VirtualFile) = file.fileType == NPPFileType

    override fun createCommandLine(): GeneralCommandLine {
        return GeneralCommandLine(executablePath)
            .withParentEnvironmentType(GeneralCommandLine.ParentEnvironmentType.CONSOLE)
    }
}
