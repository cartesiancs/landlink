import type { CapacitorConfig } from "@capacitor/cli";

const config: CapacitorConfig = {
  appId: "com.cartesiancs.landlink",
  appName: "Landlink",
  webDir: "dist",
  plugins: {
    Keyboard: {
      // WHY: 'native' resizes the WKWebView frame, but the resize fires out of
      // sync with the iOS keyboard animation, so 100dvh layouts jump up only
      // after the keyboard finishes animating. 'none' keeps the WebView at
      // full height; the JS hook in src/shared/lib/keyboard-inset.ts listens
      // to keyboardWillShow (fires before the iOS animation starts) and
      // updates a CSS var so the chat input animates in lockstep.
      resize: "none",
      style: "default",
      resizeOnFullScreen: true,
    },
  },
};

export default config;
