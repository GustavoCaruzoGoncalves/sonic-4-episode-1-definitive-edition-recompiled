package com.s4ep1recomp.sonic4;

import android.content.pm.ActivityInfo;
import android.os.Bundle;
import android.view.Gravity;
import android.view.MotionEvent;
import android.view.ViewGroup;
import android.widget.Button;
import android.widget.FrameLayout;

import org.libsdl.app.SDLActivity;

public class Sonic4Activity extends SDLActivity {
    private static final int BUTTON_SOUTH = 0;
    private static final int BUTTON_EAST = 1;
    private static final int BUTTON_WEST = 2;
    private static final int BUTTON_NORTH = 3;
    private static final int BUTTON_START = 6;
    private static final int BUTTON_DPAD_UP = 11;
    private static final int BUTTON_DPAD_DOWN = 12;
    private static final int BUTTON_DPAD_LEFT = 13;
    private static final int BUTTON_DPAD_RIGHT = 14;
    private static final int AXIS_LEFTX = 0;
    private static final int AXIS_LEFTY = 1;

    @Override
    protected String[] getLibraries() {
        // SDL's JNI entry points are linked into librexruntime. libmain holds SDL_main.
        return new String[] {"c++_shared", "rexruntime", "main"};
    }

    @Override
    protected void onCreate(Bundle savedInstanceState) {
        setRequestedOrientation(ActivityInfo.SCREEN_ORIENTATION_SENSOR_LANDSCAPE);
        super.onCreate(savedInstanceState);
        addTouchControls();
    }

    @Override
    public void setOrientationBis(int w, int h, boolean resizable, String hint) {
        setRequestedOrientation(ActivityInfo.SCREEN_ORIENTATION_SENSOR_LANDSCAPE);
    }

    private void addTouchControls() {
        if (mLayout == null) {
            return;
        }
        FrameLayout overlay = new FrameLayout(this);
        mLayout.addView(overlay, new ViewGroup.LayoutParams(
                ViewGroup.LayoutParams.MATCH_PARENT,
                ViewGroup.LayoutParams.MATCH_PARENT));

        addPadButton(overlay, "↑", Gravity.START | Gravity.BOTTOM, 48, 168, BUTTON_DPAD_UP, AXIS_LEFTY, -32767);
        addPadButton(overlay, "↓", Gravity.START | Gravity.BOTTOM, 48, 24, BUTTON_DPAD_DOWN, AXIS_LEFTY, 32767);
        addPadButton(overlay, "←", Gravity.START | Gravity.BOTTOM, 8, 96, BUTTON_DPAD_LEFT, AXIS_LEFTX, -32767);
        addPadButton(overlay, "→", Gravity.START | Gravity.BOTTOM, 96, 96, BUTTON_DPAD_RIGHT, AXIS_LEFTX, 32767);

        addButton(overlay, "A", Gravity.END | Gravity.BOTTOM, 36, 48, BUTTON_SOUTH);
        addButton(overlay, "B", Gravity.END | Gravity.BOTTOM, 132, 96, BUTTON_EAST);
        addButton(overlay, "X", Gravity.END | Gravity.BOTTOM, 132, 180, BUTTON_WEST);
        addButton(overlay, "Y", Gravity.END | Gravity.BOTTOM, 228, 132, BUTTON_NORTH);
        addButton(overlay, "II", Gravity.END | Gravity.TOP, 24, 24, BUTTON_START);
    }

    private void addButton(FrameLayout parent, String label, int gravity, int marginX, int marginY,
                            int button) {
        Button view = makeButton(label);
        FrameLayout.LayoutParams params = new FrameLayout.LayoutParams(dp(72), dp(72), gravity);
        if ((gravity & Gravity.START) == Gravity.START) {
            params.leftMargin = dp(marginX);
        } else {
            params.rightMargin = dp(marginX);
        }
        if ((gravity & Gravity.TOP) == Gravity.TOP) {
            params.topMargin = dp(marginY);
        } else {
            params.bottomMargin = dp(marginY);
        }
        view.setOnTouchListener((v, event) -> {
            int action = event.getActionMasked();
            if (action == MotionEvent.ACTION_DOWN) {
                nativeSetButton(button, true);
                return true;
            }
            if (action == MotionEvent.ACTION_UP || action == MotionEvent.ACTION_CANCEL) {
                nativeSetButton(button, false);
                return true;
            }
            return false;
        });
        parent.addView(view, params);
    }

    private void addPadButton(FrameLayout parent, String label, int gravity, int marginX, int marginY,
                              int button, int axis, int axisValue) {
        Button view = makeButton(label);
        FrameLayout.LayoutParams params = new FrameLayout.LayoutParams(dp(80), dp(80), gravity);
        params.leftMargin = dp(marginX);
        params.bottomMargin = dp(marginY);
        view.setOnTouchListener((v, event) -> {
            int action = event.getActionMasked();
            if (action == MotionEvent.ACTION_DOWN) {
                nativeSetButton(button, true);
                nativeSetAxis(axis, axisValue);
                return true;
            }
            if (action == MotionEvent.ACTION_UP || action == MotionEvent.ACTION_CANCEL) {
                nativeSetButton(button, false);
                nativeSetAxis(axis, 0);
                return true;
            }
            return false;
        });
        parent.addView(view, params);
    }

    private Button makeButton(String label) {
        Button view = new Button(this);
        view.setText(label);
        view.setAllCaps(false);
        view.setAlpha(0.45f);
        view.setMinWidth(0);
        view.setMinHeight(0);
        view.setPadding(0, 0, 0, 0);
        return view;
    }

    private int dp(int value) {
        return Math.round(value * getResources().getDisplayMetrics().density);
    }

    private static native void nativeSetButton(int button, boolean down);

    private static native void nativeSetAxis(int axis, int value);
}
