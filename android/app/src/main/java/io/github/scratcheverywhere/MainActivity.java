package io.github.scratcheverywhere;

import android.app.AlertDialog;
import android.content.Intent;
import android.content.pm.ActivityInfo;
import android.database.Cursor;
import android.net.Uri;
import android.os.Build;
import android.os.Bundle;
import android.provider.OpenableColumns;
import android.util.Log;
import android.view.WindowManager;
import android.widget.Toast;

import java.io.File;
import java.io.FileOutputStream;
import java.io.InputStream;
import java.io.OutputStream;
import java.lang.Override;
import java.util.concurrent.atomic.AtomicBoolean;

import org.libsdl.app.SDLActivity;

public class MainActivity extends SDLActivity {
	private static final String TAG = "MainActivity";

	@Override
	protected void onCreate(Bundle savedInstanceState) {
		// Init SDL
		super.onCreate(savedInstanceState);

		// Handle back pressed on SDK 33+
		// if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.TIRAMISU) {
			// TODO: handler
		// }
	}

	@Override
	protected String[] getLibraries() {
		return new String[] {
			"scratch"   // Scratch Everywhere!
		};
	}

	@Override
	public void onWindowFocusChanged(boolean hasFocus) {
		super.onWindowFocusChanged(hasFocus);
		setRequestedOrientation(ActivityInfo.SCREEN_ORIENTATION_SENSOR_LANDSCAPE);
	}

	private String getScratchPath() {
		return getExternalFilesDir(null).getAbsolutePath();
	}
}
