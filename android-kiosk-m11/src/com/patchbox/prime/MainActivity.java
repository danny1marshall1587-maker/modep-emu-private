package com.patchbox.prime;

import android.app.Activity;
import android.graphics.Color;
import android.graphics.Rect;
import android.os.Build;
import android.os.Bundle;
import android.view.KeyEvent;
import android.view.View;
import android.view.Window;
import android.view.WindowManager;
import android.webkit.WebChromeClient;
import android.webkit.WebSettings;
import android.webkit.WebView;
import android.webkit.WebViewClient;
import java.util.ArrayList;
import java.util.List;

/**
 * Stage 8: Lenovo Tab M11 (1920x1200 16:10 Widescreen & 1200x1920 10:16 Portrait)
 * Fullscreen Kiosk Console & 2" Tape Machine Activity.
 * - Compiled against official Android SDK android.jar (zero signature mismatch)
 * - requestWindowFeature(FEATURE_NO_TITLE) called before super.onCreate()
 * - True Immersive Sticky Fullscreen (zero status/nav bars)
 * - Hardware WakeLock (FLAG_KEEP_SCREEN_ON)
 * - Bundled Local Assets + Dual-Network Auto-Discovery (192.168.1.148 / 10.42.0.1 / 172.24.1.1)
 */
public class MainActivity extends Activity {
    private WebView webView;

    @Override
    protected void onCreate(Bundle savedInstanceState) {
        try {
            requestWindowFeature(Window.FEATURE_NO_TITLE);
        } catch (Throwable ignored) {}
        super.onCreate(savedInstanceState);

        try {
            Window win = getWindow();
            if (win != null) {
                win.addFlags(WindowManager.LayoutParams.FLAG_KEEP_SCREEN_ON);
                win.addFlags(WindowManager.LayoutParams.FLAG_FULLSCREEN);
            }
        } catch (Throwable ignored) {}

        webView = new WebView(this);
        webView.setBackgroundColor(Color.parseColor("#121417"));
        try {
            webView.setLayerType(View.LAYER_TYPE_HARDWARE, null);
        } catch (Throwable ignored) {}
        webView.setOverScrollMode(View.OVER_SCROLL_NEVER);
        webView.setHorizontalScrollBarEnabled(false);
        webView.setVerticalScrollBarEnabled(false);

        WebSettings settings = webView.getSettings();
        if (settings != null) {
            settings.setJavaScriptEnabled(true);
            settings.setDomStorageEnabled(true);
            settings.setDatabaseEnabled(true);
            settings.setAllowFileAccess(true);
            settings.setAllowContentAccess(true);
            try {
                settings.setAllowFileAccessFromFileURLs(true);
                settings.setAllowUniversalAccessFromFileURLs(true);
            } catch (Throwable ignored) {}
            try {
                settings.setMediaPlaybackRequiresUserGesture(false);
            } catch (Throwable ignored) {}
            settings.setUseWideViewPort(true);
            settings.setLoadWithOverviewMode(true);
            settings.setSupportZoom(false);
            settings.setBuiltInZoomControls(false);
            settings.setDisplayZoomControls(false);
            if (Build.VERSION.SDK_INT >= 21) {
                try {
                    settings.setMixedContentMode(WebSettings.MIXED_CONTENT_ALWAYS_ALLOW);
                } catch (Throwable ignored) {}
            }
            settings.setSupportMultipleWindows(true);
            settings.setJavaScriptCanOpenWindowsAutomatically(true);
            settings.setSupportZoom(true);
            settings.setBuiltInZoomControls(true);
            settings.setDisplayZoomControls(false);
        }

        webView.setWebChromeClient(new WebChromeClient() {
            @Override
            public boolean onCreateWindow(WebView view, boolean isDialog, boolean isUserGesture, android.os.Message resultMsg) {
                WebView newWebView = new WebView(MainActivity.this);
                WebSettings newSettings = newWebView.getSettings();
                if (newSettings != null) {
                    newSettings.setJavaScriptEnabled(true);
                    newSettings.setDomStorageEnabled(true);
                    newSettings.setUseWideViewPort(true);
                    newSettings.setLoadWithOverviewMode(true);
                    newSettings.setSupportZoom(true);
                    newSettings.setBuiltInZoomControls(true);
                    newSettings.setDisplayZoomControls(false);
                }
                newWebView.setWebViewClient(new WebViewClient() {
                    @Override
                    public boolean shouldOverrideUrlLoading(WebView v, String url) {
                        try {
                            android.content.Intent intent = new android.content.Intent(android.content.Intent.ACTION_VIEW, android.net.Uri.parse(url));
                            MainActivity.this.startActivity(intent);
                            return true;
                        } catch (Throwable t) {
                            return false;
                        }
                    }
                });
                android.webkit.WebView.WebViewTransport transport = (android.webkit.WebView.WebViewTransport) resultMsg.obj;
                transport.setWebView(newWebView);
                resultMsg.sendToTarget();
                return true;
            }
        });
        webView.setWebViewClient(new WebViewClient() {
            @Override
            public boolean shouldOverrideUrlLoading(WebView view, String url) {
                if (url != null && (url.startsWith("http:") || url.startsWith("https:")) && !url.contains(":8080/mixer")) {
                    try {
                        android.content.Intent intent = new android.content.Intent(android.content.Intent.ACTION_VIEW, android.net.Uri.parse(url));
                        MainActivity.this.startActivity(intent);
                        return true;
                    } catch (Throwable ignored) {}
                }
                return false;
            }
            @Override
            public void onPageFinished(WebView view, String url) {
                super.onPageFinished(view, url);
                applyImmersiveKioskMode();
                excludeEdgeSwipeGestures();
            }
        });

        setContentView(webView);
        applyImmersiveKioskMode();
        webView.post(new Runnable() {
            @Override
            public void run() {
                excludeEdgeSwipeGestures();
            }
        });

        // Load bundled console UI immediately (instant 0ms offline startup), then auto-pull latest live UI from Pi (:8080/mixer/) when reachable
        webView.loadUrl("file:///android_asset/web/index.html");
        checkAndPullLivePiUi();
    }

    private void checkAndPullLivePiUi() {
        new Thread(new Runnable() {
            @Override
            public void run() {
                String[] hosts = new String[] { "192.168.1.148", "172.24.1.1", "10.42.0.1" };
                for (final String h : hosts) {
                    java.net.Socket s = null;
                    try {
                        s = new java.net.Socket();
                        s.connect(new java.net.InetSocketAddress(h, 8080), 900);
                        s.close();
                        if (webView != null) {
                            webView.post(new Runnable() {
                                @Override
                                public void run() {
                                    try {
                                        webView.clearCache(true);
                                        webView.loadUrl("http://" + h + ":8080/mixer/?v=" + System.currentTimeMillis());
                                    } catch (Throwable ignored) {}
                                }
                            });
                        }
                        return;
                    } catch (Throwable ignored) {
                        if (s != null) {
                            try { s.close(); } catch (Throwable ignored2) {}
                        }
                    }
                }
            }
        }).start();
    }

    private void applyImmersiveKioskMode() {
        try {
            Window win = getWindow();
            if (win == null) return;
            View decor = win.getDecorView();
            if (decor == null) return;
            int flags = View.SYSTEM_UI_FLAG_LAYOUT_STABLE
                    | View.SYSTEM_UI_FLAG_LAYOUT_HIDE_NAVIGATION
                    | View.SYSTEM_UI_FLAG_LAYOUT_FULLSCREEN
                    | View.SYSTEM_UI_FLAG_HIDE_NAVIGATION
                    | View.SYSTEM_UI_FLAG_FULLSCREEN;
            if (Build.VERSION.SDK_INT >= 19) {
                flags |= View.SYSTEM_UI_FLAG_IMMERSIVE_STICKY;
            }
            decor.setSystemUiVisibility(flags);
        } catch (Throwable ignored) {}
    }

    private void excludeEdgeSwipeGestures() {
        try {
            if (Build.VERSION.SDK_INT >= 29 && webView != null) {
                int w = webView.getWidth() > 0 ? webView.getWidth() : 1920;
                int h = webView.getHeight() > 0 ? webView.getHeight() : 1200;
                // Android 10-14 limits vertical edge gesture exclusion to 200dp max per edge;
                // protect the lower fader bank region where Channel 1 and Master L/R live.
                int bandTop = Math.max(0, h - 360);
                List<Rect> exclusions = new ArrayList<Rect>();
                exclusions.add(new Rect(0, bandTop, 48, h));
                exclusions.add(new Rect(Math.max(0, w - 48), bandTop, w, h));
                webView.setSystemGestureExclusionRects(exclusions);
            }
        } catch (Throwable ignored) {}
    }

    @Override
    public void onWindowFocusChanged(boolean hasFocus) {
        super.onWindowFocusChanged(hasFocus);
        if (hasFocus) {
            applyImmersiveKioskMode();
            excludeEdgeSwipeGestures();
        }
    }

    @Override
    public boolean onKeyDown(int keyCode, KeyEvent event) {
        if (keyCode == KeyEvent.KEYCODE_BACK) {
            if (webView != null) {
                try {
                    webView.evaluateJavascript(
                        "(function(){ " +
                        "if(document.getElementById('dsp-rack-page') && document.getElementById('dsp-rack-page').classList.contains('open')){ closeDspRackPage(); return; } " +
                        "if(typeof closeDspModal === 'function'){ closeDspModal(); } " +
                        "})()", null);
                } catch (Throwable ignored) {}
            }
            return true;
        }
        return super.onKeyDown(keyCode, event);
    }
}
