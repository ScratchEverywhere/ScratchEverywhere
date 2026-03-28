// Based on: https://github.com/woheller69/FreeDroidWarn
// Modified to remove AndroidX dependency and be compatible with older APIs

package org.woheller69.freeDroidWarn;

import android.app.AlertDialog;
import android.content.Context;
import android.content.DialogInterface;
import android.content.Intent;

import android.content.SharedPreferences;
import android.net.Uri;
import android.os.Build;
import android.preference.PreferenceManager;
import android.widget.Button;

import io.github.scratcheverywhere.R;

public class FreeDroidWarn {

	public static void showWarningOnUpgrade(Context context, int buildVersion){
		SharedPreferences prefManager = PreferenceManager.getDefaultSharedPreferences(context);
		int versionCode = prefManager.getInt("versionCodeWarn",0);
		if (buildVersion > versionCode){
			AlertDialog.Builder alertDialogBuilder = new AlertDialog.Builder(context);
			alertDialogBuilder.setMessage(R.string.dialog_Warning);
			alertDialogBuilder.setNegativeButton(context.getString(R.string.dialog_more_info), (dialog, which) -> context.startActivity(new Intent(Intent.ACTION_VIEW, Uri.parse("https://keepandroidopen.org"))));
			alertDialogBuilder.setPositiveButton(context.getString(android.R.string.ok), (dialog, which) -> {
				SharedPreferences.Editor editor = prefManager.edit();
				editor.putInt("versionCodeWarn", buildVersion);
				editor.apply();
			});
			alertDialogBuilder.setNeutralButton(context.getString(R.string.solution), (dialog, which) -> context.startActivity(new Intent(Intent.ACTION_VIEW, Uri.parse("https://github.com/woheller69/FreeDroidWarn?tab=readme-ov-file#solutions"))));

			AlertDialog alertDialog = alertDialogBuilder.create();
			alertDialog.show();
			Button neutralButton = alertDialog.getButton(DialogInterface.BUTTON_NEUTRAL);
			if (neutralButton != null && Build.VERSION.SDK_INT >= 14) {
				int color;
				if (Build.VERSION.SDK_INT >= 23) {
					color = context.getColor(android.R.color.holo_red_dark);
				} else {
					color = context.getResources().getColor(android.R.color.holo_red_dark);
				}

				neutralButton.setTextColor(color);
			}
		}

	}

}