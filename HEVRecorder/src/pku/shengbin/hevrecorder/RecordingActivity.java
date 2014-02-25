package pku.shengbin.hevrecorder;

import java.io.File;

import android.app.Activity;
import android.hardware.Camera;
import android.hardware.Camera.PreviewCallback;
import android.hardware.Camera.Size;
import android.media.AudioFormat;
import android.media.AudioRecord;
import android.media.MediaRecorder;
import android.os.Bundle;
import android.os.Environment;
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
	Button mControlButton = null;
	long encodingTime=0;
	long encodingFrame=0;
	
	AudioRecord mAudioRecord;
	byte[] mAudioBuffer;
	Thread mAudioThread;
	
	String SDdir;
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

	private native int native_recorder_open(int width, int height,String SDdir);

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
		int bufferSize = AudioRecord.getMinBufferSize(sampleRate, channels,
				format);
		mAudioBuffer = new byte[bufferSize];
		mAudioRecord = new AudioRecord(MediaRecorder.AudioSource.DEFAULT,
				sampleRate, channels, format, bufferSize);

		// video
		// Create an instance of Camera
		mCamera = getCameraInstance();
		// Create our Preview view and set it as the content of our activity.
		mPreview = new CameraPreview(this, mCamera);
		FrameLayout preview = (FrameLayout) findViewById(R.id.camera_preview);
		preview.addView(mPreview);

		// set display size to the size of our frame layout, i.e. full screen
		// (better to consider the ratio)
		LayoutParams params = (LayoutParams) preview.getLayoutParams();
		mPreview.setDisplaySize(params.width, params.height);

		// controls
		RelativeLayout layoutText = (RelativeLayout) findViewById(R.id.layout_text);
		layoutText.bringToFront();
		RelativeLayout layoutButton = (RelativeLayout) findViewById(R.id.layout_button);
		layoutButton.bringToFront();
		mInfoText = (TextView) findViewById(R.id.text_info);
		mInfoText.setText("");
		mControlButton = (Button) findViewById(R.id.button_control);
		mControlButton.setOnClickListener(new View.OnClickListener() {
			@Override
			public void onClick(View arg0) {
				if (mRecording) {
					stopRecording();
				} else {
					startRecording();
				}
			}

		});

	}
	
	private String getSDPath(){ 
	       File sdDir = null; 
	       boolean sdCardExist = Environment.getExternalStorageState()   
	                           .equals(Environment.MEDIA_MOUNTED);   
	       if   (sdCardExist||Environment.isExternalStorageEmulated())   
	       {                               
	         sdDir = Environment.getExternalStorageDirectory();
	         Log.d(TAG,"test in get sd card"+sdDir.toString());
	       } 
	       return sdDir.toString(); 
	       
	}
	private void startRecording() {
		// open the recorder
		Camera.Parameters p = mCamera.getParameters();
		Size s = p.getPreviewSize();
        SDdir=getSDPath();
        Log.d(TAG,"test in get sd card"+SDdir);
        
		int ret = native_recorder_open(s.width, s.height,SDdir);
		if (ret < 0) {
			Toast.makeText(RecordingActivity.this, "Open recorder failed!",
					Toast.LENGTH_SHORT).show();
			return;
		}

		// encode every video frame
		mCamera.setPreviewCallback(new PreviewCallback() {
			@Override
			public void onPreviewFrame(byte[] data, Camera cam) {
				Camera.Parameters p = mCamera.getParameters();
				Size s = p.getPreviewSize();
				 
				long beginTime = System.currentTimeMillis();
				native_recorder_encode_video(data);
				long endTime = System.currentTimeMillis();
				Log.d(TAG, "encoding time: " + (endTime - beginTime) + " ms");
				encodingTime+=endTime - beginTime;
				encodingFrame+=1;
				if (encodingTime>1000)
				{
					double fps=encodingFrame*1000/(double)encodingTime;
					mInfoText.setText("recording... video size: " + s.width + "x" + s.height+"    "+"FPS:"+String.format("%.2f",fps) );
					encodingTime=0;
					encodingFrame=0;
				}
			}

		});

		// start audio recording
		mAudioThread = new AudioRecordThread();
		mAudioThread.start();

		mRecording = true;

		mInfoText.setText("recording... video size: " + s.width + "x" + s.height+"    "+"FPS: waiting");
		mControlButton.setText(R.string.stop);

	}

	private void stopRecording() {
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
			Toast.makeText(
					RecordingActivity.this,
					"Close the recorder failed. The recorded file may be wrong.",
					Toast.LENGTH_SHORT).show();
		}
		mCamera.setPreviewCallback(null);
		Log.d(TAG, "Total encoding time: " + (encodingTime) + " ms");
		Log.d(TAG, "Total encoding frame: " + (encodingFrame));
		Log.d(TAG, "FPS: " + (encodingFrame*1000/encodingTime));
		mInfoText.setText("");
		mControlButton.setText(R.string.start);
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
		if (mRecording) {
			stopRecording();
		}
		// release the camera immediately on pause event
		releaseCamera();
	}

	private void releaseCamera() {
		if (mCamera != null) {
			mCamera.release(); // release the camera for other applications
			mCamera = null;
		}
	}
}