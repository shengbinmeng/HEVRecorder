package pku.shengbin.hevrecorder;

import java.io.File;
import java.text.SimpleDateFormat;
import java.util.Date;
import java.util.Locale;

import android.app.Activity;
import android.app.ProgressDialog;
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

/**
 * This RecordingActivity is one which really does the work. 
 * It:
 * 1) controls the start and stop of recording, from a button click or the activity's life cycle.
 * 2) loads native libraries and calls the native recording functions.
 * 3) supplies the audio and video data to native layer, calculates FPS and shows information to the user.
 */
public class RecordingActivity extends Activity {

	private final static String TAG = "RecordingActivity";
	private final static boolean HLS_RECORD = true;

	private Camera mCamera = null;
	private CameraPreview mPreview = null;
	boolean mRecording = false;
	TextView mInfoText = null;
	Button mControlButton = null;
	
	AudioRecord mAudioRecord = null;
	byte[] mAudioBuffer = null;
	Thread mAudioThread = null;
	
	private long mStartTime = 0;
	private long mFrameCount = 0;
	private ProgressDialog mProgressDlg = null;
	
	/**
	 * A new thread will be started for the audio recording.
	 * Because a while loop is needed to continually supply the audio data to native layer.
	 */
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
					int ret = native_recorder_encode_audio(mAudioBuffer);
					if (ret < 0) {
						stopRecording();
						Toast.makeText(RecordingActivity.this, "Error while encoding video, recorder stopped.",
								Toast.LENGTH_SHORT).show();
						break;
					}
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

		// audio parameters are set here
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
		mInfoText.setTextSize(48);
		mControlButton = (Button) findViewById(R.id.button_control);
		
		mControlButton.setOnClickListener(new View.OnClickListener() {
			@Override
			public void onClick(View arg0) {
				if (mRecording) {
					// only show the dialog when user clicks the Stop button;
					// when stop in other case (e.g. Home or Back or interrupt), user won't be able to see it
					mProgressDlg = ProgressDialog.show(RecordingActivity.this, "Please wait", "Stopping......");
					stopRecording();
				} else {
					startRecording();
				}
			}

		});

	}
	
	private String getSdPath() { 
	       File sdDir = null; 
	       boolean sdCardExist = Environment.getExternalStorageState()   
	                           .equals(Environment.MEDIA_MOUNTED);   
	       if (sdCardExist || Environment.isExternalStorageEmulated()) {                               
	    	   sdDir = Environment.getExternalStorageDirectory();
	       }
	       return sdDir.toString(); 
	       
	}
	private void startRecording() {
		// open the recorder
		Camera.Parameters p = mCamera.getParameters();
		Size s = p.getPreviewSize();
		String sdPath = getSdPath();
		File dir = new File(sdPath + "/HEVRecorder");
		if (!dir.exists()) {
			dir.mkdir();
		}
		String timeNow = new SimpleDateFormat("yyyy-MM-dd-HH-mm-ss", Locale.getDefault()).format(new Date());
		String filePath = String.format("%s/record-%s.flv", dir.getPath(), timeNow);
		if (HLS_RECORD) {
			filePath = String.format("%s/", dir.getPath());
		}
		int ret = native_recorder_open(s.width, s.height, filePath);
		if (ret < 0) {
			Toast.makeText(RecordingActivity.this, "Open recorder failed!",
					Toast.LENGTH_SHORT).show();
			return;
		}
		
		mStartTime = System.currentTimeMillis();
		
		// encode every video frame
		mCamera.setPreviewCallback(new PreviewCallback() {
			@Override
			public void onPreviewFrame(byte[] data, Camera cam) {				 
				long beginTime = System.currentTimeMillis();
				int ret = native_recorder_encode_video(data);
				if (ret < 0) {
					stopRecording();
					Toast.makeText(RecordingActivity.this, "Error while encoding video, recorder stopped.",
							Toast.LENGTH_SHORT).show();
					return;
				}
				long currentTime = System.currentTimeMillis();
				Log.d(TAG, "encoding time: " + (currentTime - beginTime) + " ms");
				mFrameCount += 1;
				// update FPS every 1000ms (i.e. 1s)
				if (currentTime - mStartTime > 1000) {
					Camera.Parameters p = mCamera.getParameters();
					Size s = p.getPreviewSize();
					double fps = mFrameCount / ((currentTime - mStartTime)/1000.0);
					mInfoText.setText(String.format("HEVC video recording... size: %dx%d, FPS: %.2f", s.width, s.height, fps));
					mStartTime = currentTime;
					mFrameCount = 0;
				}
			}
		});

		// start audio recording
		mAudioThread = new AudioRecordThread();
		mAudioThread.start();

		mRecording = true;

		mInfoText.setText(String.format("recording... video size: %dx%d, FPS: waiting", s.width, s.height));
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
		// we need to start a new thread to do this, because:
		// during closing, it will take a while to flush the delayed frames,
		// if it's executed in the main UI thread, the progress dialog won't show
		new Thread(){
			public void run(){
				int ret = native_recorder_close();
				if (ret < 0) {
					Toast.makeText(
	 				RecordingActivity.this,
	 				"Close the recorder failed. The recorded file may be wrong.",
	 				Toast.LENGTH_SHORT).show();
				}
				
				if (mProgressDlg != null) {
					runOnUiThread(new Runnable() {
					    @Override
					    public void run() {
							mProgressDlg.dismiss();
							mProgressDlg = null;
					    }
					});
				}
			}
		}.start();
		
		mCamera.setPreviewCallback(null);
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