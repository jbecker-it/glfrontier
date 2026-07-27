package org.glfrontier;

import org.libsdl.app.SDLActivity;

/**
 * Entry point for the Android build.
 *
 * There is deliberately almost nothing here. The touch controls are
 * implemented in C, on top of the same abstract input layer a gamepad
 * uses, so there is no Java-side UI to keep in sync with the game.
 */
public class FrontierActivity extends SDLActivity {

    @Override
    protected String[] getLibraries() {
        return new String[] {
            "SDL2",
            "main"
        };
    }

    @Override
    protected String[] getArguments() {
        /* The software renderer is the only one that works here: the GL
         * path needs immediate mode and the GLU tessellator, neither of
         * which exists in OpenGL ES. */
        return new String[] {
            "--old-renderer"
        };
    }
}
