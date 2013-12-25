package pku.shengbin.hevrecorder;

import android.app.Activity;
import android.hardware.Camera;
import android.hardware.Camera.PreviewCallback;
import android.os.Bundle;
import android.util.Log;
import android.view.View;
import android.view.Window;
import android.view.WindowManager;
import android.widget.Button;
import android.widget.FrameLayout;
import android.widget.RelativeLayout;

public class RecordingActivity extends Activity {

	private final static String TAG = "RecordingActivity";
    private Camera mCamera;
    private CameraPreview mPreview;
    private boolean mRecording = false;

    @Override
    public void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
		requestWindowFeature(Window.FEATURE_NO_TITLE);
		getWindow().addFlags(WindowManager.LayoutParams.FLAG_KEEP_SCREEN_ON);
    	getWindow().addFlags(WindowManager.LayoutParams.FLAG_FULLSCREEN);

        setContentView(R.layout.activity_recording);
		
        // Create an instance of Camera
        mCamera = getCameraInstance();

        // Create our Preview view and set it as the content of our activity.
        mPreview = new CameraPreview(this, mCamera);
        FrameLayout preview = (FrameLayout) findViewById(R.id.camera_preview);
        preview.addView(mPreview);
        
        RelativeLayout layoutText = (RelativeLayout) findViewById(R.id.layout_text);
        layoutText.bringToFront();
 
        RelativeLayout layoutButton = (RelativeLayout) findViewById(R.id.layout_button);
        layoutButton.bringToFront();
        
        Button buttonControl = (Button) findViewById(R.id.button_control);
        buttonControl.setOnClickListener(new View.OnClickListener(){

			@Override
			public void onClick(View arg0) {
				// TODO Auto-generated method stub
				if (mRecording) {
					mCamera.setPreviewCallback(null);
					mRecording = false;
					((Button)arg0).setText(R.string.start);
				} else {
					
					mCamera.setPreviewCallback(new PreviewCallback() {
						@Override
						public void onPreviewFrame(byte[] arg0, Camera arg1) {
							long beginTime = System.currentTimeMillis();
							Log.i(TAG, "encode a frame");
							
							long endTime = System.currentTimeMillis();
	                        Log.d(TAG, "time cost:" + (endTime - beginTime));
						}
						
					});
					
					mRecording = true;
					((Button)arg0).setText(R.string.stop);
				}
			}
        	
        });
    }
    
	// attempt to get a Camera instance
    public static Camera getCameraInstance() {
        Camera c = null;
        try {
            c = Camera.open();
        } catch (Exception e) {
            // Camera is not available (in use or does not exist)
        	e.printStackTrace();
        }
        return c;
    }
    
    @Override
    protected void onStart() {
        super.onStart();
        if (mCamera == null) {
        	// Create an instance of Camera
        	mCamera = getCameraInstance();
        	mPreview.setCamera(mCamera);
        }

    }
    
    @Override
    protected void onPause() {
        super.onPause();
        // release the camera immediately on pause event
        releaseCamera();
    }
    
    private void releaseCamera() {
        if (mCamera != null){
            mCamera.release();        // release the camera for other applications
            mCamera = null;
        }
    }
}