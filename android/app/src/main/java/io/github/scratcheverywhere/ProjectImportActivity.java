package io.github.scratcheverywhere;

import android.app.Activity;
import android.app.AlertDialog;
import android.content.Intent;
import android.database.Cursor;
import android.net.Uri;
import android.os.Bundle;
import android.provider.OpenableColumns;
import android.util.Log;
import android.widget.Toast;

import java.io.File;
import java.io.FileOutputStream;
import java.io.InputStream;
import java.io.OutputStream;
import java.lang.Override;
import java.util.ArrayList;
import java.util.concurrent.atomic.AtomicBoolean;

public class ProjectImportActivity extends Activity {
	private static final String TAG = "ProjectImportActivity";

	@Override
	protected void onCreate(Bundle savedInstanceState) {
		super.onCreate(savedInstanceState);
		handleIntent(getIntent());
	}

	@Override
	protected void onNewIntent(Intent intent) {
		super.onNewIntent(intent);
		handleIntent(intent);
	}

	private void handleIntent(Intent intent) {
		String action = intent.getAction();

		if (Intent.ACTION_VIEW.equals(action)) {
			Uri uri = intent.getData();
			if (uri != null) {
				Log.i(TAG, "Received import URI: " + uri);

				// Import URI
				importProject(uri);
			}
		} else if (Intent.ACTION_SEND.equals(action)) {
			Uri uri = intent.getParcelableExtra(Intent.EXTRA_STREAM);
			if (uri != null) {
				Log.i(TAG, "Received import URI: " + uri);

				// Import URI
				try {
					importProject(uri);
				} catch (Exception e) {
					Toast.makeText(this, R.string.import_failed, Toast.LENGTH_SHORT)
						.show();

					e.printStackTrace();
				}
			}
		} else if (Intent.ACTION_SEND_MULTIPLE.equals(action)) {
			ArrayList<Uri> uriList = intent.getParcelableArrayListExtra(Intent.EXTRA_STREAM);
			if (uriList != null) {
				// Import selected files
				for (Uri uri : uriList) {
					Log.i(TAG, "Received import URI: " + uriList);

					try {
						importProject(uri);
					} catch (Exception e) {
						Toast.makeText(this, R.string.import_failed, Toast.LENGTH_SHORT)
							.show();

						e.printStackTrace();
					}
					
					Toast.makeText(this, R.string.import_failed, Toast.LENGTH_SHORT)
						.show();
				}
			}
		}
	}

	private String getScratchPath() {
		return getExternalFilesDir(null).getAbsolutePath() + "/";
	}

	protected void importProject(Uri uri) {
			InputStream input = getContentResolver().openInputStream(uri);

			// Get file name
			Cursor cursor = getContentResolver().query(uri, null, null, null, null);
			cursor.moveToFirst();

			int nameIndex = cursor.getColumnIndex(OpenableColumns.DISPLAY_NAME);
			String fileName = cursor.getString(nameIndex);
			cursor.close();

			// Specify our destination file
			File dest = new File(getScratchPath() + fileName);
			if (dest.exists()) {
				AtomicBoolean overwrite = new AtomicBoolean(false);

				// Create confirm view
				AlertDialog.Builder dialog = new AlertDialog.Builder(this);
				dialog.setMessage(getString(R.string.import_exists));
				dialog.setNegativeButton(android.R.string.cancel, (dialogInterface, i) -> {
					dialogInterface.cancel();
				});
				dialog.setPositiveButton(android.R.string.ok, (dialogInterface, i) -> {
					dialogInterface.dismiss();
					overwrite.set(true);
				});
				dialog.setCancelable(true);
				dialog.show();

				if (!overwrite.get()) return;
				Log.i(TAG, "Overwriting file " + fileName);
			}

			OutputStream output = new FileOutputStream(dest);

			// Write in a way we can maintain compatibility with older APIs
			byte[] buf = new byte[4096];

			int len;
			while ((len = input.read(buf)) > 0) {
				output.write(buf, 0, len);
			}

			input.close();
			output.close();

			String formatted = String.format(getString(R.string.import_success), fileName);
			Toast.makeText(this, formatted, Toast.LENGTH_SHORT)
				.show();

			Intent redirectIntent = new Intent(this, MainActivity.class);
			startActivity(redirectIntent);
			finish();
	}
}
