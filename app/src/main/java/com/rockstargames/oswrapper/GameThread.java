package com.rockstargames.oswrapper;

import android.os.SystemClock;
import android.util.Log;

import java.util.concurrent.ConcurrentLinkedQueue;
import java.util.concurrent.atomic.AtomicBoolean;

/**
 * The engine's single thread, as in Rockstar's oswrapper: every call into
 * {@link GameNative} is queued here. Each loop drains the queue, then draws
 * a frame while the view has focus and the engine is initialised.
 */
public final class GameThread {
    private static final String TAG = "GameThread";
    private static final ConcurrentLinkedQueue<Runnable> queue = new ConcurrentLinkedQueue<>();
    private static Loop current;

    private GameThread() {}

    /** Runs r on the game thread, after anything already queued. */
    public static void post(Runnable r) {
        queue.add(r);
    }

    public static synchronized void start(GamePlatformServices services) {
        if (current != null) return;
        current = new Loop(services);
        current.start();
    }

    public static synchronized void stop() {
        if (current == null) return;
        current.exit.set(true);
        current = null;
    }

    private static final class Loop extends Thread {
        final AtomicBoolean exit = new AtomicBoolean();
        final GamePlatformServices services;

        Loop(GamePlatformServices services) {
            super("GameThread");
            this.services = services;
        }

        @Override
        public void run() {
            Log.i(TAG, "started");
            long last = SystemClock.elapsedRealtime();
            long frames = 0;
            while (!exit.get()) {
                Runnable r;
                while ((r = queue.poll()) != null) r.run();
                long now = SystemClock.elapsedRealtime();
                float seconds = (now - last) / 1000f;
                last = now;
                if (services.view.hasGameFocus() && GameNative.implIsInitialized()) {
                    if (frames++ % 3600 == 0) Log.i(TAG, "frame " + frames);
                    long start = SystemClock.uptimeMillis();
                    GameNative.implOnDrawFrame(seconds);
                    // The engine paces itself and often returns at once
                    // between its frames; don't spin a core meanwhile.
                    if (SystemClock.uptimeMillis() - start < 1) SystemClock.sleep(1);
                } else {
                    // Nothing to draw: don't spin a core.
                    SystemClock.sleep(5);
                }
            }
            Log.i(TAG, "stopped");
        }
    }
}
