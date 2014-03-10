package pku.shengbin.hevrecorder;

import android.os.Bundle;
import android.app.Activity;
import android.app.AlertDialog;
import android.content.Intent;
import android.view.Menu;
import android.view.MenuItem;
import android.view.View;
import android.view.View.OnClickListener;
import android.widget.Button;

/**
 * The main UI activity. It simply starts RecordingActivity from clicking a button.
 * The About and Settings menu are also handled here.
 */
public class MainActivity extends Activity {

    @Override
    protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        setContentView(R.layout.activity_main);
        // add the button listener
        Button loginButton = (Button) this.findViewById(R.id.button_start);
        loginButton.setOnClickListener(new OnClickListener(){
			@Override
			public void onClick(View v) {
				startRecording();
			}
        });
    }

    public void startRecording() {
    	Intent i = new Intent(this, RecordingActivity.class);
    	startActivity(i);
    }

    @Override
    public boolean onCreateOptionsMenu(Menu menu) {
        // Inflate the menu; this adds items to the action bar if it is present.
        getMenuInflater().inflate(R.menu.main, menu);
        return true;
    }
    
    @Override
   	public boolean onOptionsItemSelected(MenuItem item) {
       	if (item.getItemId() == R.id.action_about) {
       		AlertDialog dialog = new AlertDialog.Builder(this).setMessage(this.getString(R.string.about_message)).setTitle(this.getString(R.string.about_title))
   			.setCancelable(false)
   			.setPositiveButton(android.R.string.ok, null)
   			.create();
       		dialog.show();
       	}
   		return super.onOptionsItemSelected(item);
   	}
    
}
