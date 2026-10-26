package io.github.scratcheverywhere;

import android.content.res.AssetManager;
import android.os.Bundle;
import org.libsdl.app.SDLActivity;

public class MainActivity extends SDLActivity {
	private native void initAssets(AssetManager manager);

	@Override
	protected void onCreate(Bundle savedInstanceState) {
		super.onCreate(savedInstanceState);
		initAssets(getAssets());
	}

	@Override
	protected String[] getLibraries() {
		return new String[] {"SDL2", "scratch"};
	}
}
