package com.rockstargames.oswrapper;

import android.content.Context;
import android.graphics.PixelFormat;
import android.util.AttributeSet;
import android.util.SparseArray;
import android.view.MotionEvent;
import android.view.SurfaceHolder;
import android.view.SurfaceView;

/**
 * The engine's drawing surface. It passes surface events and touches to
 * the engine, the way Rockstar's GameView and InputHandler do: pointer id
 * and raw view pixels.
 */
public class GameView extends SurfaceView implements SurfaceHolder.Callback {
    private final SparseArray<float[]> touches = new SparseArray<>();
    private volatile boolean focused;

    public GameView(Context context) {
        super(context);
    }

    public GameView(Context context, AttributeSet attrs) {
        super(context, attrs);
    }

    public void setup() {
        getHolder().setFormat(PixelFormat.RGBA_8888);
        getHolder().addCallback(this);
        setKeepScreenOn(true);
        setFocusableInTouchMode(true);
        requestFocus();
    }

    boolean hasGameFocus() {
        return focused;
    }

    public void setGameFocus(boolean focus) {
        focused = focus;
    }

    @Override
    public void surfaceCreated(SurfaceHolder holder) {
        GameThread.post(GameNative::implOnSurfaceCreated);
    }

    @Override
    public void surfaceChanged(SurfaceHolder holder, int format, int width, int height) {
        GameThread.post(() -> GameNative.implOnSurfaceChanged(holder.getSurface(), width, height));
    }

    @Override
    public void surfaceDestroyed(SurfaceHolder holder) {
        // The engine must let go of the surface before this returns.
        final Object done = new Object();
        final boolean[] finished = { false };
        GameThread.post(() -> {
            GameNative.implOnSurfaceDestroyed();
            synchronized (done) {
                finished[0] = true;
                done.notifyAll();
            }
        });
        synchronized (done) {
            long until = System.currentTimeMillis() + 2000;
            while (!finished[0] && System.currentTimeMillis() < until) {
                try {
                    done.wait(100);
                } catch (InterruptedException e) {
                    break;
                }
            }
        }
    }

    @Override
    public boolean onTouchEvent(MotionEvent event) {
        int action = event.getActionMasked();
        switch (action) {
            case MotionEvent.ACTION_DOWN:
            case MotionEvent.ACTION_POINTER_DOWN: {
                int index = event.getActionIndex();
                int id = event.getPointerId(index);
                float x = event.getX(index), y = event.getY(index);
                touches.put(id, new float[] { x, y });
                GameThread.post(() -> GameNative.implOnTouchStart(id, x, y));
                return true;
            }
            case MotionEvent.ACTION_MOVE:
                for (int i = 0; i < event.getPointerCount(); i++) {
                    int id = event.getPointerId(i);
                    float x = event.getX(i), y = event.getY(i);
                    touches.put(id, new float[] { x, y });
                    GameThread.post(() -> GameNative.implOnTouchMove(id, x, y));
                }
                return true;
            case MotionEvent.ACTION_UP:
            case MotionEvent.ACTION_POINTER_UP: {
                int index = event.getActionIndex();
                int id = event.getPointerId(index);
                float x = event.getX(index), y = event.getY(index);
                touches.remove(id);
                GameThread.post(() -> GameNative.implOnTouchEnd(id, x, y));
                return true;
            }
            case MotionEvent.ACTION_CANCEL:
                for (int i = 0; i < touches.size(); i++) {
                    int id = touches.keyAt(i);
                    float[] p = touches.valueAt(i);
                    GameThread.post(() -> GameNative.implOnTouchEnd(id, p[0], p[1]));
                }
                touches.clear();
                return true;
            default:
                return super.onTouchEvent(event);
        }
    }
}
