package com.qplus.ide;

import com.intellij.openapi.application.ApplicationManager;
import com.intellij.openapi.components.PersistentStateComponent;
import com.intellij.openapi.components.State;
import com.intellij.openapi.components.Storage;
import org.jetbrains.annotations.NotNull;
import org.jetbrains.annotations.Nullable;

@State(name = "QplusSettings", storages = @Storage("qplus.xml"))
public class QplusSettings implements PersistentStateComponent<QplusSettings.State> {
    public static class State {
        public String qpcPath = "qpc";
    }

    private State state = new State();

    public static QplusSettings getInstance() {
        return ApplicationManager.getApplication().getService(QplusSettings.class);
    }

    public String getQpcPath() {
        return state.qpcPath;
    }

    public void setQpcPath(String path) {
        state.qpcPath = path;
    }

    @Override
    public @Nullable State getState() {
        return state;
    }

    @Override
    public void loadState(@NotNull State state) {
        this.state = state;
    }
}
