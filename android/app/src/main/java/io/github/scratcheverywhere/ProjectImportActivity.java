package io.github.scratcheverywhere;

import android.annotation.TargetApi;
import android.app.Activity;
import android.app.AlertDialog;
import android.content.Intent;
import android.database.Cursor;
import android.net.Uri;
import android.os.Build;
import android.os.Bundle;
import android.provider.OpenableColumns;
import android.util.Log;
import android.widget.Toast;

import java.io.File;
import java.io.FileNotFoundException;
import java.io.FileOutputStream;
import java.io.IOException;
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
			Uri uri = intent.getParcelableExtra(Intent.EXTRA_STREAM);
			if (uri != null) {
				String fileName = getFileNameFromUri(uri);
				Log.i(TAG, "Received import URI: " + uri);

				// Import URI
				try {
					importProject(uri, fileName);
				} catch (Exception e) {
					Toast.makeText(this, R.string.import_failed, Toast.LENGTH_SHORT)
						.show();
					e.printStackTrace();
				}

				returnToMain();
			}
		} else if (Intent.ACTION_SEND.equals(action)) {
			Uri uri = intent.getParcelableExtra(Intent.EXTRA_STREAM);
			if (uri != null) {
				String fileName = getFileNameFromUri(uri);
				Log.i(TAG, "Received import URI: " + uri);

				// Import URI
				try {
					importProject(uri, fileName);
				} catch (Exception e) {
					Toast.makeText(this, R.string.import_failed, Toast.LENGTH_SHORT)
						.show();

					e.printStackTrace();
				}

				returnToMain();
			}
		} else if (Intent.ACTION_SEND_MULTIPLE.equals(action)) {
			ArrayList<Uri> uriList = intent.getParcelableArrayListExtra(Intent.EXTRA_STREAM);
			if (uriList != null) {
				// Import selected files
				int successful = 0;
				int failed = 0;

				for (Uri uri : uriList) {
					String fileName = getFileNameFromUri(uri);
					Log.i(TAG, "Received import URI: " + uriList);

					try {
						importProject(uri, fileName);
						successful++;
					} catch (Exception e) {
						failed++;
						e.printStackTrace();
					}
				}

				String message = String.format(
					getString(R.string.import_bulk),
					successful,
					failed
				);
				Toast.makeText(this, message, Toast.LENGTH_LONG)
					.show();
				returnToMain();
			}
		}
	}

	private String getFileNameFromUri(Uri uri) {
		Cursor cursor = getContentResolver().query(uri, null, null, null, null);
		cursor.moveToFirst();

		int nameIndex = cursor.getColumnIndex(OpenableColumns.DISPLAY_NAME);
		String fileName = cursor.getString(nameIndex);
		cursor.close();

		return fileName;
	}

	private String getScratchPath() {
		// TODO: support config.json custom paths
		return getExternalFilesDir(null).getAbsolutePath() + "/";
	}

	private void returnToMain() {
		Intent redirectIntent = new Intent(this, MainActivity.class);
		startActivity(redirectIntent);
		finish();
	}

	private void showErrorDialog(int resourceId) {
		AlertDialog.Builder dialog;
		if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.HONEYCOMB) {
			dialog = new AlertDialog.Builder(this, android.R.style.Theme_Holo_Dialog);
		} else {
			dialog = new AlertDialog.Builder(this);
		}

		dialog.setTitle(R.string.error_import_title)
			.setMessage(getString(resourceId))
			.setPositiveButton(android.R.string.ok, (dialogInterface, i) -> {
				dialogInterface.dismiss();
				returnToMain();
			})
			.setCancelable(true);

		dialog.show();
	}

	private void performImport(Uri uri, File file) {
		try {
			InputStream input = getContentResolver().openInputStream(uri);
			OutputStream output = new FileOutputStream(file);

			// Write in a way we can maintain compatibility with older APIs
			byte[] buf = new byte[4096];

			int len;
			while ((len = input.read(buf)) > 0) {
				output.write(buf, 0, len);
			}

			input.close();
			output.close();
		} catch (FileNotFoundException e) {
			Log.e(TAG, "File not found: " + e);
			showErrorDialog(R.string.error_file_not_found);
		} catch (IOException e) {
			Log.e(TAG, "I/O error: " + e);
			showErrorDialog(R.string.error_file_io);
		}
	}

	protected void importProject(Uri uri, String fileName) {
		// Specify our destination file
		File dest = new File(getScratchPath() + fileName);
		if (dest.exists()) {
			// Create confirm view
			AlertDialog.Builder dialog;
			if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.HONEYCOMB) {
				dialog = new AlertDialog.Builder(this, android.R.style.Theme_Holo_Dialog);
			} else {
				dialog = new AlertDialog.Builder(this);
			}

			dialog.setMessage(getString(R.string.import_exists))
				.setNegativeButton(android.R.string.cancel, (dialogInterface, i) -> {
					dialogInterface.cancel();
				})
				.setPositiveButton(android.R.string.ok, (dialogInterface, i) -> {
					dialogInterface.dismiss();
					performImport(uri, dest);
					Log.i(TAG, "Overwriting file " + fileName);
				})
				.setCancelable(true);

			dialog.show();
		} else {
			performImport(uri, dest);
		}
	}
}
