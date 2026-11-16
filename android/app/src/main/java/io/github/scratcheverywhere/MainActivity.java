package io.github.scratcheverywhere;

import android.content.Context;
import android.content.Intent;
import android.content.res.AssetManager;
import android.database.Cursor;
import android.net.ConnectivityManager;
import android.net.NetworkInfo;
import android.net.Uri;
import android.os.Bundle;
import android.os.ParcelFileDescriptor;
import android.provider.OpenableColumns;

import org.libsdl.app.SDLActivity;

public class MainActivity extends SDLActivity {
	private native void initAssets(AssetManager manager);
	private native void loadProject(int fd, String filename);

	@Override
	protected void onCreate(Bundle savedInstanceState) {
		super.onCreate(savedInstanceState);
		initAssets(getAssets());

		// Get file opened with "Open with" of another application at startup
		Intent intent = getIntent();
		if (intent != null && Intent.ACTION_VIEW.equals(intent.getAction())) {
			handleImport(intent.getData());
		}
	}

	@Override
	protected void onNewIntent(Intent intent) {
		super.onNewIntent(intent);

		// Get file opened from "Open with" of another application
		if (Intent.ACTION_VIEW.equals(intent.getAction())) {
			handleImport(intent.getData());
		}
	}

	@Override
	protected String[] getLibraries() {
		return new String[] {"SDL2", "scratch"};
	}

	private void handleImport(Uri uri) {
		if (uri != null) {
			try {
				ParcelFileDescriptor pfd = getContentResolver().openFileDescriptor(uri, "r");
				if (pfd != null) {
					int fd = pfd.detachFd();
					loadProject(fd, getFilename(uri));
					pfd.close();
				}
			} catch (Exception e) {
				e.printStackTrace();
			}
		}
	}

	private String getFilename(Uri uri) {
		String fileName = "project.sb3";
		Cursor cursor = getContentResolver().query(uri, null, null, null, null);
		if (cursor != null) {
			cursor.moveToFirst();

			int nameIndex = cursor.getColumnIndex(OpenableColumns.DISPLAY_NAME);
			fileName = cursor.getString(nameIndex);
			cursor.close();
		}

		return fileName;
	}


	// JNI Methods
	public boolean isOnline() {
		ConnectivityManager cm = (ConnectivityManager)getContext().getSystemService(Context.CONNECTIVITY_SERVICE);
		NetworkInfo info = cm != null ? cm.getActiveNetworkInfo() : null;
		return info != null && info.isConnected();
	}
}
