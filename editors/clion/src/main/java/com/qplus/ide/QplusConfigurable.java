package com.qplus.ide;

import com.intellij.openapi.fileChooser.FileChooserDescriptor;
import com.intellij.openapi.options.Configurable;
import com.intellij.openapi.project.Project;
import com.intellij.openapi.project.ProjectManager;
import com.intellij.openapi.ui.TextFieldWithBrowseButton;
import com.intellij.platform.lsp.api.LspServerManager;
import com.intellij.util.ui.FormBuilder;
import org.jetbrains.annotations.Nls;
import org.jetbrains.annotations.Nullable;

import javax.swing.JComponent;
import javax.swing.JPanel;

public class QplusConfigurable implements Configurable {
    private TextFieldWithBrowseButton pathField;

    @Override
    public @Nls String getDisplayName() {
        return "Q+";
    }

    @Override
    public @Nullable JComponent createComponent() {
        pathField = new TextFieldWithBrowseButton();
        FileChooserDescriptor descriptor =
                new FileChooserDescriptor(true, false, false, false, false, false)
                        .withTitle("Select qpc")
                        .withDescription("qpc.exe from the Q+ compiler build directory");
        pathField.addBrowseFolderListener(null, descriptor);
        return FormBuilder.createFormBuilder()
                .addLabeledComponent("qpc path:", pathField, 1, false)
                .addComponentFillVertically(new JPanel(), 0)
                .getPanel();
    }

    @Override
    public boolean isModified() {
        return pathField != null && !pathField.getText().equals(QplusSettings.getInstance().getQpcPath());
    }

    @Override
    public void apply() {
        QplusSettings.getInstance().setQpcPath(pathField.getText().trim());
        for (Project project : ProjectManager.getInstance().getOpenProjects()) {
            LspServerManager.getInstance(project).stopAndRestartIfNeeded(QplusLspServerSupportProvider.class);
        }
    }

    @Override
    public void reset() {
        pathField.setText(QplusSettings.getInstance().getQpcPath());
    }
}
