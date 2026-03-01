package io.github.scratcheverywhere;

import org.libsdl.app.SDLActivity;

public class MainActivity extends SDLActivity {
	@Override
	protected String[] getLibraries() {
		return new String[] {
			"scratch"   // Scratch Everywhere!
		};
	}
}
