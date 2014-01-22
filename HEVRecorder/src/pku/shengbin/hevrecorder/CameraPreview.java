package pku.shengbin.hevrecorder;

import java.util.List;

import android.graphics.ImageFormat;
import android.hardware.Camera;
import android.app.AlertDialog;
import android.content.Context;
import android.util.Log;
import android.view.Gravity;
import android.view.SurfaceHolder;
import android.view.SurfaceView;
import android.widget.FrameLayout.LayoutParams;

public class CameraPreview extends SurfaceView implements SurfaceHolder.Callback {

	private final static String TAG = "CameraPreview";
	private SurfaceHolder mHolder;
    private Camera mCamera;
    private Context mContext;

    public CameraPreview(Context context) {
        super(context);
    }
    
    public CameraPreview(Context context, Camera camera) {
        super(context);
        mContext = context;
        // Install a SurfaceHolder.Callback so we get notified when the
        // underlying surface is created and destroyed.
        mHolder = getHolder();
        mHolder.addCallback(this);
        
        setCamera(camera);
    }
    
    public void setCamera(Camera c) {
    	mCamera = c;
    	
    	// set image format
    	Camera.Parameters p = mCamera.getParameters();
    	p.setPreviewFormat(ImageFormat.YV12);
    	mCamera.setParameters(p);

    	setPreviewSize();
    }
    
    public void setPreviewSize () {
    	Camera.Parameters p = mCamera.getParameters();
    	int min = p.getSupportedPreviewFpsRange().get(0)[0];
    	int max = p.getSupportedPreviewFpsRange().get(0)[1];
    	p.setPreviewFpsRange(min, max);
    	List<Camera.Size> l = p.getSupportedPreviewSizes();
    	Camera.Size size = l.get(0);
    	for (int i = 0; i < l.size(); i++) {
        	size = l.get(i);
        	if (size.width < 400) {
            	p.setPreviewSize(size.width, size.height);
            	break;
        	}
    	}
    	Log.i("CameraPreview", "preview size:" + size.width + "x" + size.height);
    	mCamera.setParameters(p);
    }
    
    public void setDisplaySize() {
    	
    	Camera.Size size = mCamera.getParameters().getPreviewSize();
    	LayoutParams params = (LayoutParams) this.getLayoutParams(); 
    	params.gravity = Gravity.CENTER;
        params.width = size.width;
        params.height = size.height;
        this.setLayoutParams(params);
        
    }

    public void surfaceCreated(SurfaceHolder holder) {
    	if (mCamera == null) {
    		AlertDialog dialog = new AlertDialog.Builder(this.mContext).setMessage("Camera is null!").setTitle("Sorry")
   			.setCancelable(false)
   			.setPositiveButton(android.R.string.ok, null)
   			.create();
       		dialog.show();
    	}
    }

    public void surfaceDestroyed(SurfaceHolder holder) {
        // empty. Take care of releasing the Camera preview in your activity.
    }

    public void surfaceChanged(SurfaceHolder holder, int format, int w, int h) {
        // If your preview can change or rotate, take care of those events here.
        // Make sure to stop the preview before resizing or reformatting it.

        if (mHolder.getSurface() == null) {
        	// preview surface does not exist
        	return;
        }

        if (mCamera == null) {
       		return;
    	}
        // stop preview before making changes
        try {
            mCamera.stopPreview();
        } catch (Exception e){
        	// tried to stop a non-existent preview
        	e.printStackTrace();
        }

        // set preview size and make any resize, rotate or
        // reformatting changes here
        

        // start preview with new settings
        try {
            mCamera.setPreviewDisplay(mHolder);
            mCamera.startPreview();

        } catch (Exception e){
            Log.d(TAG, "Error starting camera preview: " + e.getMessage());
        }
    }

}
