package pku.shengbin.hevrecorder;

import android.app.Activity;
import android.hardware.Camera;
import android.hardware.Camera.PreviewCallback;
import android.media.AudioFormat;
import android.media.AudioRecord;
import android.media.MediaRecorder;
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
    boolean mRecording = false;
    //---------------------audio record------------
    int audio_sample_format=2;
    int audio_sample_size=2;
    int audio_channels = AudioFormat.CHANNEL_IN_MONO;
    int audio_bit_rate = 64000;
    int audio_frequency = 44100;
    int recBufSize;
    int audioEncoding=AudioFormat.ENCODING_PCM_16BIT;
    AudioRecord audioRecord;  
    class RecordPlayThread extends Thread {  
        public void run() {  
            try {  
                byte[] buffer = new byte[recBufSize];  
                audioRecord.startRecording();
                
                  
                while (mRecording) {  
                    
                    int bufferReadResult = audioRecord.read(buffer, 0,  
                            recBufSize); 
                    if(bufferReadResult==0){
                    	return ;
                    }
                    native_recorder_encode_sound(buffer); 
                }  
                audioRecord.stop();  
            } catch (Throwable t) {  
                
            }  
        }  
    };  
    //---------------------audio record end--------------
    
    // the open function may pass some parameters, e.g. width and height
    /*
    private native int native_encoder_open();
    private native int native_encoder_encode(byte[] data);
    private native int native_encoder_close();
    */
    private native int native_recorder_open();
    private native int native_recorder_encode_sound(byte[] data);
    private native int native_recorder_encode_video(byte[] data);
    private native int native_recorder_close();
    static {
    	System.loadLibrary("lenthevcenc");
    	System.loadLibrary("ffmpeg");
    	System.loadLibrary("native_recorder");
    }
    
   
    @Override
    public void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
		requestWindowFeature(Window.FEATURE_NO_TITLE);
		getWindow().addFlags(WindowManager.LayoutParams.FLAG_KEEP_SCREEN_ON);
    	getWindow().addFlags(WindowManager.LayoutParams.FLAG_FULLSCREEN);
    	
        setContentView(R.layout.activity_recording);
		//-----------audio recorder---------------
        recBufSize = AudioRecord.getMinBufferSize(audio_frequency,  
               audio_channels, audioEncoding);  
        audioRecord=new AudioRecord(MediaRecorder.AudioSource.MIC, audio_frequency,  
                audio_channels, audioEncoding, recBufSize);  
        //-----------audio rd end-----------------*/
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
               // new RecordPlayThread().start();
				if (mRecording) {
					// close the encoder
					native_recorder_close();
					mCamera.setPreviewCallback(null);
					
					mRecording = false;
					((Button)arg0).setText(R.string.start);
				} else {
					// prepare the encoder
					native_recorder_open();
					
					// encode every frame
					mCamera.setPreviewCallback(new PreviewCallback() {
						@Override
						public void onPreviewFrame(byte[] data, Camera cam) {
							long beginTime = System.currentTimeMillis();
							
							native_recorder_encode_video(data);
							long endTime = System.currentTimeMillis();
	                        Log.d(TAG, "time cost:" + (endTime - beginTime));
						}
						
					});
					
					mRecording = true;
					((Button)arg0).setText(R.string.stop);
				}
			}
        	
        });
        
        mPreview.setDisplaySize();
    }
    
	private int plus(int i, int j) {
		// TODO Auto-generated method stub
		return 0;
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