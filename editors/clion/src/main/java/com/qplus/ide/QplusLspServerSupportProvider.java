package com.qplus.ide;

import com.intellij.execution.configurations.GeneralCommandLine;
import com.intellij.execution.configurations.PathEnvironmentVariableUtil;
import com.intellij.icons.AllIcons;
import com.intellij.openapi.project.Project;
import com.intellij.openapi.vfs.VirtualFile;
import com.intellij.platform.lsp.api.LspServer;
import com.intellij.platform.lsp.api.LspServerSupportProvider;
import com.intellij.platform.lsp.api.ProjectWideLspServerDescriptor;
import com.intellij.platform.lsp.api.lsWidget.LspServerWidgetItem;
import org.jetbrains.annotations.NotNull;
import org.jetbrains.annotations.Nullable;

import java.io.File;
import java.io.IOException;
import java.nio.file.Files;
import java.nio.file.Path;
import java.util.ArrayList;
import java.util.List;

public class QplusLspServerSupportProvider implements LspServerSupportProvider {
    @Override
    public void fileOpened(
            @NotNull Project project,
            @NotNull VirtualFile file,
            @NotNull LspServerSupportProvider.LspServerStarter serverStarter) {
        if (isQp(file)) {
            serverStarter.ensureServerStarted(new QplusLspServerDescriptor(project));
        }
    }

    @Override
    public @NotNull LspServerWidgetItem createLspServerWidgetItem(
            @NotNull LspServer lspServer, @Nullable VirtualFile currentFile) {
        return new LspServerWidgetItem(lspServer, currentFile, AllIcons.Json.Object, QplusConfigurable.class);
    }

    static boolean isQp(VirtualFile file) {
        return "qp".equalsIgnoreCase(file.getExtension());
    }

    static String resolveQpc(Project project) {
        String configured = QplusSettings.getInstance().getQpcPath();
        if (configured == null || configured.isBlank()) {
            configured = "qpc";
        }
        configured = configured.trim().replace("\"", "");

        Path configuredPath = Path.of(configured);
        if (configuredPath.isAbsolute() && isFile(configuredPath)) {
            return configuredPath.toAbsolutePath().toString();
        }

        String base = project.getBasePath();
        if (base != null) {
            Path relative = Path.of(base, configured);
            if (isFile(relative)) {
                return relative.toAbsolutePath().toString();
            }
            Path relativeExe = Path.of(base, configured + ".exe");
            if (isFile(relativeExe)) {
                return relativeExe.toAbsolutePath().toString();
            }
            String[] names = {"qpc.exe", "qpc"};
            String[] dirs = {"cmake-build-debug", "cmake-build-release", "build"};
            for (String dir : dirs) {
                for (String name : names) {
                    Path candidate = Path.of(base, dir, name);
                    if (isFile(candidate)) {
                        return candidate.toAbsolutePath().toString();
                    }
                }
            }
        }

        String[] pathNames = configured.equalsIgnoreCase("qpc") || configured.equalsIgnoreCase("qpc.exe")
                ? new String[]{"qpc.exe", "qpc"}
                : new String[]{configured, configured + ".exe"};
        for (String name : pathNames) {
            File found = PathEnvironmentVariableUtil.findInPath(name);
            if (found != null && found.isFile()) {
                return found.getAbsolutePath();
            }
        }
        return configured;
    }

    static boolean isFile(Path path) {
        return Files.isRegularFile(path);
    }

    static List<String> compilerBinDirs(String qpcPath) {
        List<String> dirs = new ArrayList<>();
        Path exe = Path.of(qpcPath);
        if (!exe.isAbsolute()) {
            return dirs;
        }
        Path dir = exe.getParent();
        if (dir == null) {
            return dirs;
        }
        dirs.add(dir.toString());
        Path cache = dir.resolve("CMakeCache.txt");
        if (Files.isRegularFile(cache)) {
            try {
                for (String line : Files.readAllLines(cache)) {
                    if (line.startsWith("CMAKE_CXX_COMPILER:STRING=")
                            || line.startsWith("CMAKE_CXX_COMPILER:FILEPATH=")) {
                        Path compiler = Path.of(line.substring(line.indexOf('=') + 1).trim());
                        Path bin = compiler.getParent();
                        if (bin != null && Files.isDirectory(bin)) {
                            dirs.add(bin.toString());
                        }
                        break;
                    }
                }
            } catch (IOException ignored) {
                // keep exe dir only
            }
        }
        return dirs;
    }

    static void prependCompilerPath(GeneralCommandLine cmd, String qpcPath) {
        List<String> dirs = compilerBinDirs(qpcPath);
        if (dirs.isEmpty()) {
            return;
        }
        String existing = System.getenv("PATH");
        if (existing == null) {
            existing = "";
        }
        cmd.withEnvironment("PATH", String.join(File.pathSeparator, dirs) + File.pathSeparator + existing);
    }
}

class QplusLspServerDescriptor extends ProjectWideLspServerDescriptor {
    QplusLspServerDescriptor(Project project) {
        super(project, "Q+");
    }

    @Override
    public boolean isSupportedFile(@NotNull VirtualFile file) {
        return QplusLspServerSupportProvider.isQp(file);
    }

    @Override
    public @NotNull GeneralCommandLine createCommandLine() {
        String exe = QplusLspServerSupportProvider.resolveQpc(getProject());
        if (!QplusLspServerSupportProvider.isFile(Path.of(exe))) {
            throw new IllegalStateException(
                    "Cannot find qpc ('" + exe + "'). Set Settings → Languages & Frameworks → Q+ → qpc path "
                            + "to qpc.exe (for example cmake-build-debug/qpc.exe).");
        }
        GeneralCommandLine cmd = new GeneralCommandLine(exe, "lsp")
                .withWorkDirectory(getProject().getBasePath())
                .withParentEnvironmentType(GeneralCommandLine.ParentEnvironmentType.CONSOLE);
        QplusLspServerSupportProvider.prependCompilerPath(cmd, exe);
        return cmd;
    }
}
