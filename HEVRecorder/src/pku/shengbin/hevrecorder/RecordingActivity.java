package pku.shengbin.hevrecorder;

import android.app.Activity;
import android.hardware.Camera;
import android.hardware.Camera.PreviewCallback;
import android.hardware.Camera.Size;
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
import android.widget.TextView;
import android.widget.Toast;
import android.widget.FrameLayout.LayoutParams;

public class RecordingActivity extends Activity {

	private final static String TAG = "RecordingActivity";
    private Camera mCamera;
    private CameraPreview mPreview;
    boolean mRecording = false;
    TextView mInfoText = null;
    
    AudioRecord mAudioRecord; 
    byte[] mAudioBuffer;
    Thread mAudioThread;
    
    class AudioRecordThread extends Thread {  
        public void run() {  
            try {  
                mAudioRecord.startRecording();

                while (mRecording) {  
                    int bytesRead = mAudioRecord.read(mAudioBuffer, 0,  
                    		mAudioBuffer.length); 
                    if (bytesRead < 0) {
                    	break;
                    }
                    native_recorder_encode_audio(mAudioBuffer);
                }
                
                mAudioRecord.stop();
                
            } catch (Exception e) {
                e.printStackTrace();
            }
        }  
    };  

    private native int native_recorder_open(int width, int height);
    private native int native_recorder_encode_audio(byte[] data);
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
        
        // audio
        int sampleRate = 44100;        
        int channels = AudioFormat.CHANNEL_IN_STEREO;
        int format = AudioFormat.ENCODING_PCM_16BIT;
        int bufferSize = AudioRecord.getMinBufferSize(sampleRate, channels, format);  
        mAudioBuffer = new byte[bufferSize];
        mAudioRecord = new AudioRecord(MediaRecorder.AudioSource.DEFAULT, sampleRate,  
                channels, format, bufferSize);  
        
        // video
        // Create an instance of Camera
        mCamera = getCameraInstance();
        // Create our Preview view and set it as the content of our activity.
        mPreview = new CameraPreview(this, mCamera);
        FrameLayout preview = (FrameLayout) findViewById(R.id.camera_preview);
        preview.addView(mPreview);
        
        // set display size to the size of our frame layout, i.e. full screen (better to consider the ratio)
    	LayoutParams params = (LayoutParams) preview.getLayoutParams();
        mPreview.setDisplaySize(params.width, params.height);
        
        
        // controls
        RelativeLayout layoutText = (RelativeLayout) findViewById(R.id.layout_text);
        layoutText.bringToFront();
        RelativeLayout layoutButton = (RelativeLayout) findViewById(R.id.layout_button);
        layoutButton.bringToFront();
        
        mInfoText = (TextView) findViewById(R.id.text_info);
        mInfoText.setText("");
        Button buttonControl = (Button) findViewById(R.id.button_control);
        buttonControl.setOnClickListener(new View.OnClickListener(){
			@Override
			public void onClick(View arg0) {
				// TODO Auto-generated method stub 				
				if (mRecording) {
					mRecording = false;
					
					// wait the audio thread to end before close native recorder
					try {
						mAudioThread.join();
					} catch (InterruptedException e) {
						// TODO Auto-generated catch block
						e.printStackTrace();
					}
					
					// close the recorder
					int ret = native_recorder_close();
					if (ret < 0) {
						Toast.makeText(RecordingActivity.this, "Close the recorder failed. The recorded file may be wrong.", Toast.LENGTH_SHORT).show();
					}
					mCamera.setPreviewCallback(null);
					
					((Button)arg0).setText(R.string.start);
			        mInfoText.setText("");

				} else {
					// open the recorder
			    	Camera.Parameters p = mCamera.getParameters();
			    	final Size s = p.getPreviewSize();
			    	
					int ret = native_recorder_open(s.width, s.height);
					if (ret < 0) {
						Toast.makeText(RecordingActivity.this, "Open recorder failed!", Toast.LENGTH_SHORT).show();
						return;
					}
					
					// encode every video frame
					mCamera.setPreviewCallback(new PreviewCallback() {
						@Override
						public void onPreviewFrame(byte[] data, Camera cam) {
							long beginTime = System.currentTimeMillis();
							native_recorder_encode_video(data);
							long endTime = System.currentTimeMillis();
	                        Log.d(TAG, "encoding time: " + (endTime - beginTime) + " ms");
						}
						
					});
					
					mRecording = true;
					
					// start audio recording
					mAudioThread = new AudioRecordThread();
					mAudioThread.start();
					
					((Button)arg0).setText(R.string.stop);
			        mInfoText.setText("recording... video size: " + s.width + "x" + s.height);
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