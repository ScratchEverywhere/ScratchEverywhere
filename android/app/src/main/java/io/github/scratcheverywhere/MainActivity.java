package io.github.scratcheverywhere;

import android.os.Bundle;

import org.libsdl.app.SDLActivity;

public class MainActivity extends SDLActivity {
	@Override
	protected void onCreate(Bundle savedInstanceState) {
		super.onCreate(savedInstanceState);
		nativeSetupJNI();
	}

	@Override
	protected String[] getLibraries() {
		return new String[] {
			"scratch"   // Scratch Everywhere!
		};
	}

	public static native int nativeSetupJNI();
}
