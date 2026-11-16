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

import java.io.FileOutputStream;
import java.io.InputStream;
import java.io.OutputStream;

public class MainActivity extends SDLActivity {
	private native void initAssets(AssetManager manager);
	private native void loadProject(String filename);

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
			String path = getExternalFilesDir(null) + "/" + getFilename(uri);
			try {
				InputStream in = getContentResolver().openInputStream(uri);
				FileOutputStream out = new FileOutputStream(path);
				if (in != null) {
					byte[] buffer = new byte[4086];
					int length;
					while ((length = in.read(buffer)) > 0) {
						out.write(buffer, 0, length);
					}
					loadProject(path);
					in.close();
				}
				out.close();
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
