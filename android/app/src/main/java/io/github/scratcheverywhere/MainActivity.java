package io.github.scratcheverywhere;

import android.content.ContentResolver;
import android.content.Context;
import android.content.res.AssetManager;
import android.database.Cursor;
import android.net.ConnectivityManager;
import android.net.NetworkInfo;
import android.net.Uri;
import android.os.Bundle;
import android.provider.OpenableColumns;

import java.io.FileOutputStream;
import java.io.InputStream;
import java.io.OutputStream;

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

	// JNI Methods
	public boolean isOnline() {
		ConnectivityManager cm = (ConnectivityManager)getContext().getSystemService(Context.CONNECTIVITY_SERVICE);
		NetworkInfo info = cm != null ? cm.getActiveNetworkInfo() : null;
		return info != null && info.isConnected();
	}

	public String getFilename(String input) {
		if (input == null || !input.startsWith("content://")) {
			if (input != null && input.contains("/")) {
				return input.substring(input.lastIndexOf("/") + 1);
			}
			return "project.sb3";
		}

		String result = "project.sb3";
		try {
			Uri uri = Uri.parse(input);
			ContentResolver resolver = mSingleton.getContentResolver();

			Cursor cursor = resolver.query(uri, null, null, null, null);
			if (cursor != null) {
				if (cursor.moveToFirst()) {
					int index = cursor.getColumnIndex(OpenableColumns.DISPLAY_NAME);
					if (index >= 0)
						return cursor.getString(index);
				}
				cursor.close();
			}
		} catch (Exception e) {
			e.printStackTrace();
		}

		return result;
	}

	public boolean importProject(String input, String output) {
		Uri uri = Uri.parse(input);
		if (uri != null) {
			try {
				InputStream inputStream = getContentResolver().openInputStream(uri);
				OutputStream outputStream = new FileOutputStream(output);
				if (inputStream != null) {
					int length;
					byte[] buffer = new byte[4086];
					while ((length = inputStream.read(buffer)) > 0)
						outputStream.write(buffer, 0, length);

					inputStream.close();
					outputStream.close();
					return true;
				}
			} catch (Exception e) {
				e.printStackTrace();
			}
			return false;
		}
		return false;
	}
}
