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
import android.widget.Toast;

public class RecordingActivity extends Activity {

	private final static String TAG = "RecordingActivity";
    private Camera mCamera;
    private CameraPreview mPreview;
    boolean mRecording = false;
    
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
        int audio_frequency = 44100;        
        int audio_channels = AudioFormat.CHANNEL_IN_STEREO;
        int audioEncoding = AudioFormat.ENCODING_PCM_16BIT;
        int bufferSize = AudioRecord.getMinBufferSize(audio_frequency,  
               audio_channels, audioEncoding);  
        mAudioBuffer = new byte[bufferSize];
        mAudioRecord = new AudioRecord(MediaRecorder.AudioSource.MIC, audio_frequency,  
                audio_channels, audioEncoding, bufferSize);  
        
        // video
        // Create an instance of Camera
        mCamera = getCameraInstance();
        // Create our Preview view and set it as the content of our activity.
        mPreview = new CameraPreview(this, mCamera);
        FrameLayout preview = (FrameLayout) findViewById(R.id.camera_preview);
        preview.addView(mPreview);
        
        mPreview.setDisplaySize();
        
        
        // controls
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
							int width = s.width, height = s.height;
							int stride_y = (width % 16 == 0 ? width/16 : width/16 + 1)*16;
							int stride_uv = (width/2 % 16 == 0 ? width/2/16 : width/2/16 + 1)*16;
							//Log.d(TAG, "preview a frame: " + data.length + ", " + (stride_y*height + 2*stride_uv*height/2) + ", " + data[0] + data[1] + data[2] + data[3] + data[4]);
							
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